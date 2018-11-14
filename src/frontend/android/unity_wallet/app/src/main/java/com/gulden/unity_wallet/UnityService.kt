// Copyright (c) 2018 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@webmail.co.za)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package com.gulden.unity_wallet

import android.content.Intent
import android.os.Binder
import android.os.IBinder
import kotlin.concurrent.thread
import android.app.*
import android.support.v4.app.NotificationCompat
import android.app.NotificationManager
import android.content.Context
import android.support.v4.app.NotificationCompat.VISIBILITY_PUBLIC
import com.gulden.jniunifiedbackend.*
import android.app.NotificationChannel
import android.app.NotificationManager.IMPORTANCE_LOW
import android.arch.lifecycle.Lifecycle
import android.arch.lifecycle.LifecycleObserver
import android.arch.lifecycle.OnLifecycleEvent
import android.arch.lifecycle.ProcessLifecycleOwner
import android.graphics.Color
import android.os.Build


var NOTIFICATION_ID_FOREGROUND_SERVICE = 2
var NOTIFICATION_ID_INCOMING_TRANSACTION = 3

class UnityService : Service(), LifecycleObserver
{

    // All signal are broadcast to main program via this handler.
    interface UnityServiceSignalHandler
    {
        fun syncProgressChanged(percent : Float) : Boolean
        fun walletBalanceChanged(balance : Long) : Boolean
        fun coreUIInit() : Boolean
        fun createNewWallet() : Boolean
        fun haveExistingWallet() : Boolean
    }

    // Store state of some signals so we can re-trigger them if they fired before the handler was actually set.
    var coreReady = false;
    var signalHandler: UnityServiceSignalHandler? = null
        set(value)
        {
            field = value
            if (field != null)
            {
                if (coreReady)
                {
                    value?.coreUIInit()
                }
            }
        }

    // Handle signals from core library and convert them to service signals where necessary.
    private val coreLibrarySignalHandler = object : GuldenUnifiedFrontend() {
        override fun logPrint(str: String?) {
            // logging already done at C++ level
        }

        override fun notifyUnifiedProgress(progress: Float) {
            signalHandler?.syncProgressChanged( 100 * progress)
        }

        override fun notifyBalanceChange(newBalance: BalanceRecord): Boolean
        {
            signalHandler?.walletBalanceChanged(newBalance.availableIncludingLocked + newBalance.immatureIncludingLocked + newBalance.unconfirmedIncludingLocked)
            return true
        }

        override fun notifyNewTransaction(newTransaction: TransactionRecord): Boolean
        {
            notifyIncomingTransaction(newTransaction)
            return true
        }

        override fun notifyShutdown(): Boolean
        {
            ProcessLifecycleOwner.get().lifecycle.removeObserver(this@UnityService)
            stopSelf()
            return true
        }

        override fun notifyCoreReady(): Boolean
        {
            coreReady = true
            signalHandler?.coreUIInit()

            ProcessLifecycleOwner.get().lifecycle.addObserver(this@UnityService)

            return true
        }

        override fun notifyInitWithExistingWallet()
        {
            signalHandler?.haveExistingWallet()
            return
        }

        override fun notifyInitWithoutExistingWallet()
        {
            signalHandler?.createNewWallet()
            return
        }
    }


    fun notifyIncomingTransaction(transactionRecord : TransactionRecord)
    {
        val notificationIntent = Intent(this, WalletActivity::class.java)
        val pendingIntent = PendingIntent.getActivity(this, 0, notificationIntent, 0)

        var prefix = "+"
        if (transactionRecord.type == TransactionType.SEND)
            prefix = "-"

        val notification = NotificationCompat.Builder(this)
                .setContentTitle("Incoming transaction")
                .setTicker("Incoming transaction")
                .setContentText((" "+prefix+"%.2f").format(transactionRecord.amount.toDouble() / 100000000))
                .setSmallIcon(R.drawable.ic_g_logo)
                //.setLargeIcon(Bitmap.createScaledBitmap(R.drawable.ic_g_logo, 128, 128, false))
                .setContentIntent(pendingIntent)
                .setOngoing(false)
                .setAutoCancel(true)
                .setVisibility(VISIBILITY_PUBLIC)
                //.setPublicVersion()
                //.setTimeoutAfter()
                .setDefaults(Notification.DEFAULT_ALL)
                .build()



        val notificationManager = applicationContext.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        notificationManager.notify(1, notification)
    }

    // Binder given to clients
    private val binder = LocalBinder()


    // Class used for the client Binder.
    inner class LocalBinder : Binder()
    {
        internal// Return this instance of MyService so clients can call public methods
        val service: UnityService get() = this@UnityService
    }

    fun loadLibrary()
    {
        thread(true)
        {
            System.loadLibrary("gulden_unity_jni")
            libraryLoaded = true
            GuldenUnifiedBackend.InitUnityLib(applicationContext.applicationInfo.dataDir, Constants.TEST, coreLibrarySignalHandler)
        }
    }

    fun unloadLibrary()
    {
        if (libraryLoaded)
        {
            GuldenUnifiedBackend.TerminateUnityLib()
            libraryLoaded = false
        }
    }

    val channelID = "com.gulden.unity_wallet.service.channel"
    fun startInForeground()
    {
        loadLibrary()

        val notificationIntent = Intent(this, WalletActivity::class.java)
        val pendingIntent = PendingIntent.getActivity(this, 0, notificationIntent, 0)

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
        {
            val notificationChannel = NotificationChannel(channelID, "Gulden service", IMPORTANCE_LOW)
            (getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager).createNotificationChannel(notificationChannel)
            notificationChannel.enableLights(false)
            notificationChannel.setShowBadge(false)
        }


        val notification = NotificationCompat.Builder(this, channelID)
                .setContentTitle("Gulden")
                .setTicker("Gulden")
                .setContentText("Gulden")
                .setSmallIcon(R.drawable.ic_g_logo)
                .setContentIntent(pendingIntent)
                .setOngoing(true)
                .build()
        startForeground(NOTIFICATION_ID_FOREGROUND_SERVICE, notification)
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int
    {
        if (intent?.action.equals(START_FOREGROUND_ACTION))
        {
            startInForeground()
        }

        return super.onStartCommand(intent, flags, startId)
    }

    override fun onDestroy()
    {
        unloadLibrary()
        super.onDestroy()
    }

    override fun onTaskRemoved(rootIntent: Intent)
    {
        if (closeOnAppExit)
        {
            unloadLibrary()
        }
    }

    override fun onBind(intent: Intent?): IBinder?
    {
        return binder
    }

    var libraryLoaded = false
    var closeOnAppExit = true

    companion object
    {
        var IS_SERVICE_RUNNING = false
        var START_FOREGROUND_ACTION = "com.gulden.UnityService.startforegroundaction"
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_STOP)
    fun allActivitiesStopped()
    {
        GuldenUnifiedBackend.PersistAndPruneForSPV();
    }
}
