// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2016-2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#include "walletmodel.h"

#include "addresstablemodel.h"
#include "consensus/validation.h"
#include "_Gulden/accounttablemodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "paymentserver.h"
#include "recentrequeststablemodel.h"
#include "sendcoinsdialog.h"
#include "transactiontablemodel.h"

#include "base58.h"
#include "chain.h"
#include "keystore.h"
#include "validation/validation.h"
#include "net.h" // for g_connman
#include "policy/rbf.h"
#include "sync.h"
#include "ui_interface.h"
#include "util.h" // for GetBoolArg
#include "wallet/feebumper.h"
#include "wallet/walletdb.h" // for BackupWallet

#include <stdint.h>

#include <QDebug>
#include <QMessageBox>
#include <QSet>
#include <QTimer>


#include "askpassphrasedialog.h"

#include "Gulden/util.h"
#include "validation/validation.h"

WalletModel::WalletModel(const QStyle *platformStyle, CWallet *_wallet, OptionsModel *_optionsModel, QObject *parent)
: QObject(parent)
, wallet(_wallet)
, optionsModel(_optionsModel)
, addressTableModel(nullptr)
, accountTableModel(nullptr)
, transactionTableModel(nullptr)
, recentRequestsTableModel(nullptr)
, cachedEncryptionStatus(Unencrypted)
, patternMatcherIBAN("^[a-zA-Z]{2,2}[0-9]{2,2}(?:[a-zA-Z0-9]{1,30})$")
{
    fHaveWatchOnly = wallet->HaveWatchOnly();
    fForceCheckBalanceChanged = false;

    addressTableModel = new AddressTableModel(wallet, this);
    accountTableModel = new AccountTableModel(wallet, this);
    transactionTableModel = new TransactionTableModel(platformStyle, wallet, this);
    recentRequestsTableModel = new RecentRequestsTableModel(wallet, this);

    //fixme: (2.1) - Get rid of this timer - core signals should handle this.
    // This timer will be fired repeatedly to update the balance
    pollTimer = new QTimer(this);
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(pollBalanceChanged()));
    pollTimer->start(MODEL_UPDATE_DELAY);

    subscribeToCoreSignals();
}

WalletModel::~WalletModel()
{
    LogPrintf("WalletModel::~WalletModel\n");
    transactionTableModel = nullptr;
    wallet = nullptr;
}

CAmount WalletModel::getBalance(CAccount* forAccount, const CCoinControl *coinControl) const
{
    //fixme: (Post-2.1) - coin control
    /*if (coinControl)
    {
        return wallet->GetAvailableBalance(coinControl);
    }*/
    if (cachedBalances.availableExcludingLocked != -1)
        return cachedBalances.availableExcludingLocked;
    else if (wallet)
        return wallet->GetBalance(forAccount, true, true, false);
    else
        return 0;
}

CAmount WalletModel::getUnconfirmedBalance(CAccount* forAccount) const
{
    if (cachedBalances.unconfirmedExcludingLocked != -1)
        return cachedBalances.unconfirmedExcludingLocked;
    else if (wallet)
        return wallet->GetUnconfirmedBalance(forAccount);
    else
        return 0;
}

CAmount WalletModel::getImmatureBalance() const
{
    if (cachedBalances.immatureExcludingLocked != -1)
        return cachedBalances.immatureExcludingLocked;
    else if (wallet)
        return wallet->GetImmatureBalance();
    else
        return 0;
}

bool WalletModel::haveWatchOnly() const
{
    return fHaveWatchOnly;
}

CAmount WalletModel::getWatchBalance() const
{
    if (wallet)
        return wallet->GetWatchOnlyBalance();
    else
        return 0;
}

CAmount WalletModel::getWatchUnconfirmedBalance() const
{
    if (cachedWatchUnconfBalance != -1)
        return cachedWatchUnconfBalance;
    else if (wallet)
        return wallet->GetUnconfirmedWatchOnlyBalance();
    else
        return 0;
}

CAmount WalletModel::getWatchImmatureBalance() const
{
    if (cachedWatchImmatureBalance != -1)
        return cachedWatchImmatureBalance;
    else if (wallet)
        return wallet->GetImmatureWatchOnlyBalance();
    else
        return 0;
}

WalletBalances WalletModel::getBalances() const
{
    if (wallet && cachedBalances.totalLocked == -1)
        wallet->GetBalances(cachedBalances);

    return cachedBalances;
}

void WalletModel::updateStatus()
{
    EncryptionStatus newEncryptionStatus = getEncryptionStatus();

    if(cachedEncryptionStatus != newEncryptionStatus)
        Q_EMIT encryptionStatusChanged(newEncryptionStatus);
}

void WalletModel::pollBalanceChanged()
{
    if (!wallet)
        return;

    // Get required locks upfront. This avoids the GUI from getting stuck on
    // periodical polls if the core is holding the locks for a longer time -
    // for example, during a wallet rescan.
    TRY_LOCK(cs_main, lockMain);
    if(!lockMain)
        return;
    TRY_LOCK(wallet->cs_wallet, lockWallet);
    if(!lockWallet)
        return;

    if(fForceCheckBalanceChanged || chainActive.Height() != cachedNumBlocks)
    {
        fForceCheckBalanceChanged = false;

        // Balance and number of transactions might have changed
        cachedNumBlocks = chainActive.Height();

        checkBalanceChanged();
        if(transactionTableModel)
            transactionTableModel->updateConfirmations();
    }
}

void WalletModel::checkBalanceChanged()
{
    WalletBalances balances;
    wallet->GetBalances(balances, nullptr, true);

    CAmount newWatchOnlyBalance = 0;
    CAmount newWatchUnconfBalance = 0;
    CAmount newWatchImmatureBalance = 0;
    if (haveWatchOnly())
    {
        newWatchOnlyBalance = getWatchBalance();
        newWatchUnconfBalance = getWatchUnconfirmedBalance();
        newWatchImmatureBalance = getWatchImmatureBalance();
    }

    if (cachedBalances != balances || cachedWatchOnlyBalance != newWatchOnlyBalance || cachedWatchUnconfBalance != newWatchUnconfBalance || cachedWatchImmatureBalance != newWatchImmatureBalance)
    {
        cachedBalances = balances;
        cachedWatchOnlyBalance = newWatchOnlyBalance;
        cachedWatchUnconfBalance = newWatchUnconfBalance;
        cachedWatchImmatureBalance = newWatchImmatureBalance;
        Q_EMIT balanceChanged(cachedBalances, cachedWatchOnlyBalance, cachedWatchUnconfBalance, cachedWatchImmatureBalance);
    }
}

void WalletModel::updateTransaction()
{
    // Balance and number of transactions might have changed
    fForceCheckBalanceChanged = true;
}

void WalletModel::updateAddressBook(const QString &address, const QString &label,
        bool isMine, const QString &purpose, int status)
{
    if(addressTableModel)
        addressTableModel->updateEntry(address, label, isMine, purpose, status);
}

void WalletModel::updateWatchOnlyFlag(bool fHaveWatchonly)
{
    fHaveWatchOnly = fHaveWatchonly;
    Q_EMIT notifyWatchonlyChanged(fHaveWatchonly);
}

bool WalletModel::validateAddress(const QString &address)
{
    if (address.isEmpty())
        return false;
    CGuldenAddress addressParsed(address.toStdString());
    return addressParsed.IsValid();
}

bool WalletModel::validateAddressBitcoin(const QString &address)
{
    if (address.isEmpty())
        return false;
    CGuldenAddress addressParsed(address.toStdString());
    return addressParsed.IsValidBitcoin();
}

bool WalletModel::validateAddressIBAN(const QString &address)
{
    if (address.isEmpty())
        return false;
    if (patternMatcherIBAN.match(address).hasMatch())
        return true;
    return false;
}



WalletModel::SendCoinsReturn WalletModel::prepareTransaction(CAccount* forAccount, WalletModelTransaction &transaction, const CCoinControl *coinControl)
{
    CAmount total = 0;
    bool fSubtractFeeFromAmount = false;
    QList<SendCoinsRecipient> recipients = transaction.getRecipients();
    std::vector<CRecipient> vecSend;

    if(recipients.empty())
    {
        return OK;
    }

    QSet<QString> setAddress; // Used to detect duplicates
    int nAddresses = 0;

    int nTipPrevPoW2Phase = GetPoW2Phase(chainActive.Tip()->pprev, Params(), chainActive);
    // Pre-check input data for validity
    for(const SendCoinsRecipient &rcp : recipients)
    {
        if (rcp.fSubtractFeeFromAmount)
            fSubtractFeeFromAmount = true;

        if (rcp.paymentRequest.IsInitialized())
        {   // PaymentRequest...
            CAmount subtotal = 0;
            const payments::PaymentDetails& details = rcp.paymentRequest.getDetails();
            for (int i = 0; i < details.outputs_size(); i++)
            {
                const payments::Output& out = details.outputs(i);
                if (out.amount() <= 0) continue;
                subtotal += out.amount();
                //fixme: (2.1) (SEGSIG)
                const unsigned char* scriptStr = (const unsigned char*)out.script().data();
                CScript scriptPubKey(scriptStr, scriptStr+out.script().size());
                CAmount nAmount = out.amount();
                CRecipient recipient = CRecipient(scriptPubKey, nAmount, rcp.fSubtractFeeFromAmount);
                vecSend.push_back(recipient);
            }
            if (subtotal <= 0)
            {
                return InvalidAmount;
            }
            total += subtotal;
        }
        else
        {   // User-entered Gulden address / amount:
            if(!validateAddress(rcp.address))
            {
                return InvalidAddress;
            }
            if(rcp.amount <= 0)
            {
                return InvalidAmount;
            }
            setAddress.insert(rcp.address);
                ++nAddresses;
            if(rcp.destinationPoW2Witness.lockUntilBlock != 0)
            {
                assert(rcp.destinationPoW2Witness.lockFromBlock == 0);

                if (IsSegSigEnabled(chainActive.TipPrev()))
                {
                    CRecipient recipient = CRecipient(GetPoW2WitnessOutputFromWitnessDestination(rcp.destinationPoW2Witness), rcp.amount, rcp.fSubtractFeeFromAmount);

                    //NB! Setting this is -super- important, if we don't then encrypted wallets may fail to witness.
                    recipient.witnessForAccount = rcp.witnessForAccount;
                    assert(recipient.witnessDetails.witnessKeyID != recipient.witnessDetails.spendingKeyID);

                    vecSend.push_back(recipient);
                }
                else if (nTipPrevPoW2Phase >= 2)
                {
                    CScript scriptPubKey = GetScriptForDestination(rcp.destinationPoW2Witness);
                    CRecipient recipient = CRecipient(scriptPubKey, rcp.amount, rcp.fSubtractFeeFromAmount);

                    // We have to copy this anyway even though we are using a CSCript as later code depends on it to grab the witness key id.
                    recipient.witnessDetails.witnessKeyID = rcp.destinationPoW2Witness.witnessKey;
                    recipient.witnessDetails.spendingKeyID = rcp.destinationPoW2Witness.spendingKey;
                    assert(recipient.witnessDetails.witnessKeyID != recipient.witnessDetails.spendingKeyID);

                    //NB! Setting this is -super- important, if we don't then encrypted wallets may fail to witness.
                    recipient.witnessForAccount = rcp.witnessForAccount;

                    vecSend.push_back(recipient);
                }
                else
                {
                    return WalletModel::PoW2NotActive;
                }
            }
            else
            {
                if (IsSegSigEnabled(chainActive.TipPrev()))
                {
                    CKeyID key;
                    if (!CGuldenAddress(rcp.address.toStdString()).GetKeyID(key))
                        return InvalidAddress;

                    CRecipient recipient = CRecipient(CTxOutStandardKeyHash(key), rcp.amount, rcp.fSubtractFeeFromAmount);
                    vecSend.push_back(recipient);
                }
                else
                {
                    CScript scriptPubKey = GetScriptForDestination(CGuldenAddress(rcp.address.toStdString()).Get());
                    CRecipient recipient = CRecipient(scriptPubKey, rcp.amount, rcp.fSubtractFeeFromAmount);
                    vecSend.push_back(recipient);
                }
            }
            total += rcp.amount;
        }
    }
    if(setAddress.size() != nAddresses)
    {
        return DuplicateAddress;
    }

    CAmount nBalance = getBalance(forAccount, coinControl);

    if (total > nBalance)
    {
        return AmountExceedsBalance;
    }

    {
        LOCK2(cs_main, wallet->cs_wallet);

        transaction.newPossibleKeyChange(forAccount, wallet);

        CAmount nFeeRequired = 0;
        int nChangePosRet = -1;
        std::string strFailReason;

        CWalletTx *newTx = transaction.getTransaction();
        CReserveKeyOrScript *keyChange = transaction.getPossibleKeyChange();
        bool fCreated = wallet->CreateTransaction(forAccount, vecSend, *newTx, *keyChange, nFeeRequired, nChangePosRet, strFailReason, coinControl);
        transaction.setTransactionFee(nFeeRequired);
        if (fSubtractFeeFromAmount && fCreated)
            transaction.reassignAmounts(nChangePosRet);

        if(!fCreated)
        {
            if(!fSubtractFeeFromAmount && (total + nFeeRequired) > nBalance)
            {
                return SendCoinsReturn(AmountWithFeeExceedsBalance);
            }
            //NB! do not warn of error here we can legitimately fail in some cases.
            //Caller is responsible for warning.
            return TransactionCreationFailed;
        }

        // reject absurdly high fee. (This can never happen because the
        // wallet caps the fee at maxTxFee. This merely serves as a
        // belt-and-suspenders check)
        if (nFeeRequired > 500 * COIN)
            return AbsurdFee;
    }

    return SendCoinsReturn(OK);
}

WalletModel::SendCoinsReturn WalletModel::sendCoins(WalletModelTransaction &transaction)
{
    QByteArray transaction_array; /* store serialized transaction */

    {
        LOCK2(cs_main, wallet->cs_wallet);
        CWalletTx *newTx = transaction.getTransaction();

        for(const SendCoinsRecipient &rcp : transaction.getRecipients())
        {
            if (rcp.paymentRequest.IsInitialized())
            {
                // Make sure any payment requests involved are still valid.
                if (PaymentServer::verifyExpired(rcp.paymentRequest.getDetails())) {
                    return PaymentRequestExpired;
                }

                // Store PaymentRequests in wtx.vOrderForm in wallet.
                std::string key("PaymentRequest");
                std::string value;
                rcp.paymentRequest.SerializeToString(&value);
                newTx->vOrderForm.push_back(std::pair(key, value));
            }
            else if (!rcp.message.isEmpty()) // Message from normal Gulden:URI (Gulden:123...?message=example)
                newTx->vOrderForm.push_back(std::pair("Message", rcp.message.toStdString()));
        }

        CReserveKeyOrScript *keyChange = transaction.getPossibleKeyChange();
        CValidationState state;
        if(!wallet->CommitTransaction(*newTx, *keyChange, g_connman.get(), state))
            return SendCoinsReturn(TransactionCommitFailed, QString::fromStdString(state.GetRejectReason()));

        CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
        ssTx << *newTx->tx;
        transaction_array.append(&(ssTx[0]), ssTx.size());
    }

    // Add addresses / update labels that we've sent to to the address book,
    // and emit coinsSent signal for each recipient
    for(const SendCoinsRecipient &rcp : transaction.getRecipients())
    {
        // Don't touch the address book when we have a payment request
        if (!rcp.paymentRequest.IsInitialized() && rcp.addToAddressBook)
        {
            std::string strAddress = rcp.forexAddress.isEmpty() ? rcp.address.toStdString() : rcp.forexAddress.toStdString();
            std::string strLabel = rcp.label.toStdString();
            {
                LOCK(wallet->cs_wallet);

                std::map<std::string, CAddressBookData>::iterator mi = wallet->mapAddressBook.find(strAddress);

                // Check if we have a new address or an updated label
                if (mi == wallet->mapAddressBook.end())
                {
                    wallet->SetAddressBook(strAddress, strLabel, "send");
                }
                else if (mi->second.name != strLabel)
                {
                    wallet->SetAddressBook(strAddress, strLabel, ""); // "" means don't change purpose
                }
            }
        }
        Q_EMIT coinsSent(wallet, rcp, transaction_array);
    }
    checkBalanceChanged(); // update balance immediately, otherwise there could be a short noticeable delay until pollBalanceChanged hits

    return SendCoinsReturn(OK);
}

OptionsModel *WalletModel::getOptionsModel()
{
    return optionsModel;
}

AddressTableModel *WalletModel::getAddressTableModel()
{
    return addressTableModel;
}

AccountTableModel *WalletModel::getAccountTableModel()
{
    return accountTableModel;
}

TransactionTableModel *WalletModel::getTransactionTableModel()
{
    return transactionTableModel;
}

RecentRequestsTableModel *WalletModel::getRecentRequestsTableModel()
{
    return recentRequestsTableModel;
}

WalletModel::EncryptionStatus WalletModel::getEncryptionStatus() const
{
    if (!wallet)
        return Locked;

    if(!wallet->IsCrypted())
    {
        return Unencrypted;
    }
    else if(wallet->IsLocked())
    {
        return Locked;
    }
    else
    {
        return Unlocked;
    }
}

bool WalletModel::setWalletEncrypted(bool encrypted, const SecureString &passphrase)
{
    if (!wallet)
        return false;

    if(encrypted)
    {
        // Encrypt
        return wallet->EncryptWallet(passphrase);
    }
    else
    {
        // Decrypt -- TODO; not supported yet
        return false;
    }
}

bool WalletModel::setWalletLocked(bool locked, const SecureString &passPhrase)
{
    if (!wallet)
        return false;

    if(locked)
    {
        // Lock
        return wallet->Lock();
    }
    else
    {
        // Unlock
        return wallet->Unlock(passPhrase);
    }
}

bool WalletModel::changePassphrase(const SecureString &oldPass, const SecureString &newPass)
{
    if (!wallet)
        return false;

    bool retval;
    {
        LOCK(wallet->cs_wallet);
        wallet->Lock(); // Make sure wallet is locked before attempting pass change
        retval = wallet->ChangeWalletPassphrase(oldPass, newPass);
    }
    return retval;
}

bool WalletModel::backupWallet(const QString &filename)
{
    if (!wallet)
        return false;

    return wallet->BackupWallet(filename.toLocal8Bit().data());
}

// Handlers for core signals
static void NotifyKeyStoreStatusChanged(WalletModel *walletmodel,  [[maybe_unused]] CCryptoKeyStore *wallet)
{
    qDebug() << "NotifyKeyStoreStatusChanged";
    QMetaObject::invokeMethod(walletmodel, "updateStatus", Qt::QueuedConnection);
}

static void NotifyAddressBookChanged(WalletModel *walletmodel, [[maybe_unused]] CWallet *wallet,
        const std::string &address, const std::string &label, bool isMine,
        const std::string &purpose, ChangeType status)
{
    QString strLabel = QString::fromStdString(label);
    QString strPurpose = QString::fromStdString(purpose);

    qDebug() << "NotifyAddressBookChanged: " + QString::fromStdString(address) + " " + strLabel + " isMine=" + QString::number(isMine) + " purpose=" + strPurpose + " status=" + QString::number(status);
    QMetaObject::invokeMethod(walletmodel, "updateAddressBook", Qt::QueuedConnection,
                              Q_ARG(QString, QString::fromStdString(address)),
                              Q_ARG(QString, strLabel),
                              Q_ARG(bool, isMine),
                              Q_ARG(QString, strPurpose),
                              Q_ARG(int, status));
}

static void NotifyTransactionChanged(WalletModel *walletmodel, CWallet *wallet, const uint256 &hash, ChangeType status)
{
    (unused)wallet;
    (unused)hash;
    (unused)status;
    QMetaObject::invokeMethod(walletmodel, "updateTransaction", Qt::QueuedConnection);
}

static void NotifyAccountNameChanged(WalletModel *walletmodel, CWallet *wallet, CAccount* account)
{
    (unused)wallet;
    if (account == walletmodel->getActiveAccount())
    {
        LogPrintf("NotifyAccountNameChanged\n");
        QMetaObject::invokeMethod(walletmodel, "accountNameChanged",  Qt::QueuedConnection, Q_ARG(CAccount*, account));
    }
}

static void NotifyAccountWarningChanged(WalletModel *walletmodel, CWallet *wallet, CAccount* account)
{
    (unused)wallet;
    LogPrintf("NotifyAccountWarningChanged\n");
    QMetaObject::invokeMethod(walletmodel, "accountWarningChanged",  Qt::QueuedConnection, Q_ARG(CAccount*, account));
}


static void NotifyActiveAccountChanged(WalletModel *walletmodel, CWallet *wallet, CAccount* account)
{
    (unused)wallet;
    LogPrintf("walletmodel::setActiveAccount\n");
    QMetaObject::invokeMethod(walletmodel, "activeAccountChanged",  Qt::QueuedConnection, Q_ARG(CAccount*, account));
}

static void NotifyAccountAdded(WalletModel *walletmodel, CWallet *wallet, CAccount* account)
{
    (unused)wallet;
    LogPrintf("NotifyAccountAdded\n");
    QMetaObject::invokeMethod(walletmodel, "accountAdded",  Qt::QueuedConnection, Q_ARG(CAccount*, account));
}

static void NotifyAccountDeleted(WalletModel *walletmodel, CWallet *wallet, CAccount* account)
{
    LogPrintf("NotifyAccountDeleted\n");
    if (wallet)
    {
        QMetaObject::invokeMethod(walletmodel, "accountDeleted",  Qt::QueuedConnection, Q_ARG(CAccount*, account));
        if (account == walletmodel->getActiveAccount())
        {
            if (wallet->activeAccount && wallet->activeAccount->m_State != AccountState::Deleted && account != wallet->activeAccount)
            {
                walletmodel->setActiveAccount(wallet->activeAccount);
                return;
            }
            for ( const auto& iter : wallet->mapAccounts)
            {
                if (iter.second->m_State == AccountState::Normal)
                {
                    walletmodel->setActiveAccount(iter.second);
                    return;
                }
            }
        }
    }
}


static void ShowProgress(WalletModel *walletmodel, const std::string &title, int nProgress)
{
    // emits signal "showProgress"
    QMetaObject::invokeMethod(walletmodel, "showProgress", Qt::QueuedConnection,
                              Q_ARG(QString, QString::fromStdString(title)),
                              Q_ARG(int, nProgress));
}

static void NotifyWatchonlyChanged(WalletModel *walletmodel, bool fHaveWatchonly)
{
    QMetaObject::invokeMethod(walletmodel, "updateWatchOnlyFlag", Qt::QueuedConnection,
                              Q_ARG(bool, fHaveWatchonly));
}

void WalletModel::subscribeToCoreSignals()
{
    if (!wallet)
        return;

    // Connect signals to wallet
    //fixme: (2.1) - Find a better way to do this instead of connecting to a specific account
    if (wallet->mapAccounts.size() > 0)
    {
        wallet->activeAccount->externalKeyStore.NotifyStatusChanged.connect(boost::bind(&NotifyKeyStoreStatusChanged, this, _1));
        wallet->activeAccount->internalKeyStore.NotifyStatusChanged.connect(boost::bind(&NotifyKeyStoreStatusChanged, this, _1));
    }
    wallet->NotifyAddressBookChanged.connect(boost::bind(NotifyAddressBookChanged, this, _1, _2, _3, _4, _5, _6));
    wallet->NotifyTransactionChanged.connect(boost::bind(NotifyTransactionChanged, this, _1, _2, _3));
    wallet->NotifyAccountNameChanged.connect(boost::bind(NotifyAccountNameChanged, this, _1, _2));
    wallet->NotifyAccountWarningChanged.connect(boost::bind(NotifyAccountWarningChanged, this, _1, _2));
    wallet->NotifyActiveAccountChanged.connect(boost::bind(NotifyActiveAccountChanged, this, _1, _2));
    wallet->NotifyAccountAdded.connect(boost::bind(NotifyAccountAdded, this, _1, _2));
    wallet->NotifyAccountDeleted.connect(boost::bind(NotifyAccountDeleted, this, _1, _2));
    wallet->ShowProgress.connect(boost::bind(ShowProgress, this, _1, _2));
    wallet->NotifyWatchonlyChanged.connect(boost::bind(NotifyWatchonlyChanged, this, _1));
}

void WalletModel::unsubscribeFromCoreSignals()
{
    LogPrintf("WalletModel::~unsubscribeFromCoreSignals\n");
    if (pollTimer)
    {
        disconnect(pollTimer, SIGNAL(timeout()), this, SLOT(pollBalanceChanged()));
    }
    if (transactionTableModel)
        transactionTableModel->unsubscribeFromCoreSignals();
    if (wallet)
    {
        LogPrintf("WalletModel::~unsubscribeFromCoreSignals - disconnect account signals\n");
        //fixme: (2.1) - Find a better way to do this instead of connecting to a specific account
        if (wallet->activeAccount)
        {
            wallet->activeAccount->externalKeyStore.NotifyStatusChanged.disconnect(boost::bind(&NotifyKeyStoreStatusChanged, this, _1));
            wallet->activeAccount->internalKeyStore.NotifyStatusChanged.disconnect(boost::bind(&NotifyKeyStoreStatusChanged, this, _1));
        }

        LogPrintf("WalletModel::~unsubscribeFromCoreSignals - disconnect wallet signals\n");
        wallet->NotifyAddressBookChanged.disconnect(boost::bind(NotifyAddressBookChanged, this, _1, _2, _3, _4, _5, _6));
        wallet->NotifyTransactionChanged.disconnect(boost::bind(NotifyTransactionChanged, this, _1, _2, _3));
        wallet->NotifyAccountNameChanged.disconnect(boost::bind(NotifyAccountNameChanged, this, _1, _2));
        wallet->NotifyAccountWarningChanged.disconnect(boost::bind(NotifyAccountWarningChanged, this, _1, _2));
        wallet->NotifyActiveAccountChanged.disconnect(boost::bind(NotifyActiveAccountChanged, this, _1, _2));
        wallet->NotifyAccountAdded.disconnect(boost::bind(NotifyAccountAdded, this, _1, _2));
        wallet->NotifyAccountDeleted.disconnect(boost::bind(NotifyAccountDeleted, this, _1, _2));
        wallet->ShowProgress.disconnect(boost::bind(ShowProgress, this, _1, _2));
        wallet->NotifyWatchonlyChanged.disconnect(boost::bind(NotifyWatchonlyChanged, this, _1));
        LogPrintf("WalletModel::~unsubscribeFromCoreSignals - done disconnecting signals\n");
    }
}

// WalletModel::UnlockContext implementation
WalletModel::UnlockContext WalletModel::requestUnlock()
{
    bool was_locked = getEncryptionStatus() == Locked;
    if(was_locked)
    {
        // Request UI to unlock wallet
        Q_EMIT requireUnlock();
    }
    // If wallet is still locked, unlock was failed or cancelled, mark context as invalid
    bool valid = getEncryptionStatus() != Locked;

    return UnlockContext(this, valid, was_locked);
}

WalletModel::UnlockContext::UnlockContext(WalletModel *_wallet, bool _valid, bool _relock)
: wallet(_wallet)
, valid(_valid)
, relock(_relock)
{
}

WalletModel::UnlockContext::~UnlockContext()
{
    if(valid && relock)
    {
        wallet->setWalletLocked(true);
    }
}

void WalletModel::UnlockContext::CopyFrom(const UnlockContext& rhs)
{
    // Transfer context; old object no longer relocks wallet
    *this = rhs;
    rhs.relock = false;
}

bool WalletModel::getPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const
{
    return wallet->GetPubKey(address, vchPubKeyOut);
}

bool WalletModel::havePrivKey(const CKeyID &address) const
{
    return wallet->HaveKey(address);
}

bool WalletModel::getPrivKey(const CKeyID &address, CKey& vchPrivKeyOut) const
{
    return wallet->GetKey(address, vchPrivKeyOut);
}

// returns a list of COutputs from COutPoints
void WalletModel::getOutputs(const std::vector<COutPoint>& vOutpoints, std::vector<COutput>& vOutputs)
{
    LOCK2(cs_main, wallet->cs_wallet);
    for(const COutPoint& outpoint : vOutpoints)
    {
        if (!wallet->mapWallet.count(outpoint.getHash())) continue;
        int nDepth = wallet->mapWallet[outpoint.getHash()].GetDepthInMainChain();
        if (nDepth < 0) continue;
        COutput out(&wallet->mapWallet[outpoint.getHash()], outpoint.n, nDepth, true /* spendable */, true /* solvable */, true /* safe */);
        vOutputs.push_back(out);
    }
}

bool WalletModel::isSpent(const COutPoint& outpoint) const
{
    LOCK2(cs_main, wallet->cs_wallet);
    return wallet->IsSpent(outpoint.getHash(), outpoint.n);
}

QString WalletModel::getAccountLabel(const boost::uuids::uuid& uuid)
{
    LOCK(wallet->cs_wallet);
    if (wallet->mapAccountLabels.count(uuid) > 0)
    {
        return QString::fromStdString(wallet->mapAccountLabels[uuid]);
    }
    return "";
}

void WalletModel::setActiveAccount( CAccount* account )
{
    wallet->setActiveAccount(account);
}

CAccount* WalletModel::getActiveAccount()
{
    return wallet->getActiveAccount();
}


// AvailableCoins + LockedCoins grouped by wallet address (put change in one group with wallet address)
void WalletModel::listCoins(CAccount* forAccount, std::map<QString, std::vector<COutput> >& mapCoins) const
{
    for (auto& group : wallet->ListCoins(forAccount)) {
        auto& resultGroup = mapCoins[QString::fromStdString(CGuldenAddress(group.first).ToString())];
        for (auto& coin : group.second) {
            resultGroup.emplace_back(std::move(coin));
        }
    }
}

bool WalletModel::isLockedCoin(uint256 hash, unsigned int n) const
{
    LOCK2(cs_main, wallet->cs_wallet);
    return wallet->IsLockedCoin(hash, n);
}

void WalletModel::lockCoin(COutPoint& output)
{
    LOCK2(cs_main, wallet->cs_wallet);
    wallet->LockCoin(output);
}

void WalletModel::unlockCoin(COutPoint& output)
{
    LOCK2(cs_main, wallet->cs_wallet);
    wallet->UnlockCoin(output);
}

void WalletModel::listLockedCoins(std::vector<COutPoint>& vOutpts)
{
    LOCK2(cs_main, wallet->cs_wallet);
    wallet->ListLockedCoins(vOutpts);
}

void WalletModel::loadReceiveRequests(std::vector<std::string>& vReceiveRequests)
{
    vReceiveRequests = wallet->GetDestValues("rr"); // receive request
}

bool WalletModel::saveReceiveRequest(const std::string &sAddress, const int64_t nId, const std::string &sRequest)
{
    CTxDestination dest = CGuldenAddress(sAddress).Get();

    std::stringstream ss;
    ss << nId;
    std::string key = "rr" + ss.str(); // "rr" prefix = "receive request" in destdata

    LOCK(wallet->cs_wallet);
    if (sRequest.empty())
        return wallet->EraseDestData(dest, key);
    else
        return wallet->AddDestData(dest, key, sRequest);
}

bool WalletModel::transactionCanBeAbandoned(uint256 hash) const
{
    return wallet->TransactionCanBeAbandoned(hash);
}

bool WalletModel::abandonTransaction(uint256 hash) const
{
    LOCK2(cs_main, wallet->cs_wallet);
    return wallet->AbandonTransaction(hash);
}

bool WalletModel::transactionCanBeBumped(uint256 hash) const
{
    LOCK2(cs_main, wallet->cs_wallet);
    const CWalletTx *wtx = wallet->GetWalletTx(hash);
    return wtx && SignalsOptInRBF(*wtx) && !wtx->mapValue.count("replaced_by_txid");
}

bool WalletModel::bumpFee(uint256 hash)
{
    std::unique_ptr<CFeeBumper> feeBump;
    {
        LOCK2(cs_main, wallet->cs_wallet);
        feeBump.reset(new CFeeBumper(wallet, hash, nTxConfirmTarget, false, 0, true));
    }
    if (feeBump->getResult() != BumpFeeResult::OK)
    {
        QMessageBox::critical(0, tr("Fee bump error"), tr("Increasing transaction fee failed") + "<br />(" +
            (feeBump->getErrors().size() ? QString::fromStdString(feeBump->getErrors()[0]) : "") +")");
         return false;
    }

    // allow a user based fee verification
    QString questionString = tr("Do you want to increase the fee?");
    questionString.append("<br />");
    CAmount oldFee = feeBump->getOldFee();
    CAmount newFee = feeBump->getNewFee();
    questionString.append("<table style=\"text-align: left;\">");
    questionString.append("<tr><td>");
    questionString.append(tr("Current fee:"));
    questionString.append("</td><td>");
    questionString.append(GuldenUnits::formatHtmlWithUnit(getOptionsModel()->getDisplayUnit(), oldFee));
    questionString.append("</td></tr><tr><td>");
    questionString.append(tr("Increase:"));
    questionString.append("</td><td>");
    questionString.append(GuldenUnits::formatHtmlWithUnit(getOptionsModel()->getDisplayUnit(), newFee - oldFee));
    questionString.append("</td></tr><tr><td>");
    questionString.append(tr("New fee:"));
    questionString.append("</td><td>");
    questionString.append(GuldenUnits::formatHtmlWithUnit(getOptionsModel()->getDisplayUnit(), newFee));
    questionString.append("</td></tr></table>");
    SendConfirmationDialog confirmationDialog(tr("Confirm fee bump"), questionString);
    confirmationDialog.exec();
    QMessageBox::StandardButton retval = (QMessageBox::StandardButton)confirmationDialog.result();

    // cancel sign&broadcast if users doesn't want to bump the fee
    if (retval != QMessageBox::Yes) {
        return false;
    }

    WalletModel::UnlockContext ctx(requestUnlock());
    if(!ctx.isValid())
    {
        return false;
    }

    // sign bumped transaction
    bool res = false;
    {
        LOCK2(cs_main, wallet->cs_wallet);
        res = feeBump->signTransaction(wallet);
    }
    if (!res) {
        QMessageBox::critical(0, tr("Fee bump error"), tr("Can't sign transaction."));
        return false;
    }
    // commit the bumped transaction
    {
        LOCK2(cs_main, wallet->cs_wallet);
        res = feeBump->commit(wallet);
    }
    if(!res) {
        QMessageBox::critical(0, tr("Fee bump error"), tr("Could not commit transaction") + "<br />(" +
            QString::fromStdString(feeBump->getErrors()[0])+")");
         return false;
    }
    return true;
}

bool WalletModel::isWalletEnabled()
{
   return !GetBoolArg("-disablewallet", DEFAULT_DISABLE_WALLET);
}

/*bool WalletModel::hdEnabled() const
{
    return wallet->IsHDEnabled();
}*/

int WalletModel::getDefaultConfirmTarget() const
{
    return nTxConfirmTarget;
}

bool WalletModel::getDefaultWalletRbf() const
{
    return fWalletRbf;
}
