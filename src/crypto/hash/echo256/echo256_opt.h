/*
 * file        : hash_api.h
 * version     : 1.0.208
 * date        : 14.12.2010
 * 
 * ECHO vperm implementation Hash API
 *
 * Cagdas Calik
 * ccalik@metu.edu.tr
 * Institute of Applied Mathematics, Middle East Technical University, Turkey.
 *
 */
// File contains modifications by: The Gulden developers
// All modifications:
// Copyright (c) 2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

#ifndef ECHO256_OPT_H
#define ECHO256_OPT_H
enum HashReturn
{
    SUCCESS = 0,
    FAIL = 1,
    BAD_HASHBITLEN = 2
};
enum Echo256OptSelection
{
    OPT_NONE=0,
    OPT_SSE3,
    OPT_SSE3_AES,
    OPT_SSE4,
    OPT_SSE4_AES,
    OPT_AVX,
    OPT_AVX_AES,
    OPT_AVX2,
    OPT_AVX2_AES,
    OPT_AVX512F,
    OPT_AVX512F_AES,
};
extern Echo256OptSelection echo256_opt_selected;
#endif

#ifndef ECHO256_OPT_IMPL
#include "opt/echo256_opt_sse3.h"
#include "opt/echo256_opt_sse3_aes.h"
#include "opt/echo256_opt_sse4.h"
#include "opt/echo256_opt_sse4_aes.h"
#include "opt/echo256_opt_avx.h"
#include "opt/echo256_opt_avx_aes.h"
#include "opt/echo256_opt_avx2.h"
#include "opt/echo256_opt_avx2_aes.h"
#include "opt/echo256_opt_avx512f.h"
#include "opt/echo256_opt_avx512f_aes.h"
#else
#include "compat.h"
#include <compat/arch.h>
#include <compat/sse.h>

typedef struct
{
    __m128i        state[4][4];
    unsigned char  buffer[192];
    __m128i        k;
    __m128i        hashsize;
    __m128i        const1536;
    unsigned int   uRounds;
    unsigned int   uHashSize;
    unsigned int   uBlockLength;
    unsigned int   uBufferBytes;
    uint64_t       processed_bits;
} echo256_opt_hashState __attribute__ ((aligned (64)));

HashReturn echo256_opt_Init(echo256_opt_hashState* state);
HashReturn echo256_opt_Update(echo256_opt_hashState* state, const unsigned char* data, uint64_t databitlen);
HashReturn echo256_opt_Final(echo256_opt_hashState* state, unsigned char* hashval);
HashReturn echo256_opt_UpdateFinal(echo256_opt_hashState* state, unsigned char* hashval, const unsigned char* data, uint64_t databitlen);
#endif
