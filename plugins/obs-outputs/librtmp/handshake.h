/*
 *  Copyright (C) 2008-2009 Andrej Stepanchuk
 *  Copyright (C) 2009-2010 Howard Chu
 *  Copyright (C) 2010 2a665470ced7adb7156fcef47f8199a6371c117b8a79e399a2771e0b36384090
 *
 *  This file is part of librtmp.
 *
 *  librtmp is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2.1,
 *  or (at your option) any later version.
 *
 *  librtmp is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with librtmp see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 *  http://www.gnu.org/copyleft/lgpl.html
 */

/* This file is #included in rtmp.c, it is not meant to be compiled alone */

#if defined(USE_MBEDTLS)
#include <mbedtls/md.h>
#include <mbedtls/arc4.h>
#ifndef SHA256_DIGEST_LENGTH
#define SHA256_DIGEST_LENGTH	32
#endif
typedef mbedtls_md_context_t *HMAC_CTX;
#define HMAC_setup(ctx, key, len)	ctx = malloc(sizeof(mbedtls_md_context_t)); mbedtls_md_init(ctx); \
  mbedtls_md_setup(ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1); \
  mbedtls_md_hmac_starts(ctx, (const unsigned char *)key, len)
#define HMAC_crunch(ctx, buf, len)	mbedtls_md_hmac_update(ctx, buf, len)
#define HMAC_finish(ctx, dig)		mbedtls_md_hmac_finish(ctx, dig)
#define HMAC_close(ctx)			mbedtls_md_free(ctx); free(ctx); ctx = NULL

typedef mbedtls_arc4_context*	RC4_handle;
#define RC4_alloc(h)	*h = malloc(sizeof(mbedtls_arc4_context)); mbedtls_arc4_init(*h)
#define RC4_setkey(h,l,k)	mbedtls_arc4_setup(h,k,l)
#define RC4_encrypt(h,l,d)	mbedtls_arc4_crypt(h,l,(unsigned char *)d,(unsigned char *)d)
#define RC4_encrypt2(h,l,s,d)	mbedtls_arc4_crypt(h,l,(unsigned char *)s,(unsigned char *)d)
#define RC4_free(h)	mbedtls_arc4_free(h); free(h); h = NULL

#elif defined(USE_POLARSSL)
#include <polarssl/sha2.h>
#include <polarssl/arc4.h>
#ifndef SHA256_DIGEST_LENGTH
#define SHA256_DIGEST_LENGTH	32
#endif
#define HMAC_CTX	sha2_context
#define HMAC_setup(ctx, key, len)	sha2_hmac_starts(&ctx, (unsigned char *)key, len, 0)
#define HMAC_crunch(ctx, buf, len)	sha2_hmac_update(&ctx, buf, len)
#define HMAC_finish(ctx, dig)		sha2_hmac_finish(&ctx, dig)

typedef arc4_context *	RC4_handle;
#define RC4_alloc(h)	*h = malloc(sizeof(arc4_context))
#define RC4_setkey(h,l,k)	arc4_setup(h,k,l)
#define RC4_encrypt(h,l,d)	arc4_crypt(h,l,(unsigned char *)d,(unsigned char *)d)
#define RC4_encrypt2(h,l,s,d)	arc4_crypt(h,l,(unsigned char *)s,(unsigned char *)d)
#define RC4_free(h)	free(h)

#elif defined(USE_GNUTLS)
#include <nettle/hmac.h>
#include <nettle/arcfour.h>
#ifndef SHA256_DIGEST_LENGTH
#define SHA256_DIGEST_LENGTH	32
#endif
#undef HMAC_CTX
#define HMAC_CTX	struct hmac_sha256_ctx
#define HMAC_setup(ctx, key, len)	hmac_sha256_set_key(&ctx, len, key)
#define HMAC_crunch(ctx, buf, len)	hmac_sha256_update(&ctx, len, buf)
#define HMAC_finish(ctx, dig)		hmac_sha256_digest(&ctx, SHA256_DIGEST_LENGTH, dig)
#define HMAC_close(ctx)

typedef struct arcfour_ctx*	RC4_handle;
#define RC4_alloc(h)	*h = malloc(sizeof(struct arcfour_ctx))
#define RC4_setkey(h,l,k)	arcfour_set_key(h, l, k)
#define RC4_encrypt(h,l,d)	arcfour_crypt(h,l,(uint8_t *)d,(uint8_t *)d)
#define RC4_encrypt2(h,l,s,d)	arcfour_crypt(h,l,(uint8_t *)d,(uint8_t *)s)
#define RC4_free(h)	free(h)

#else	/* USE_OPENSSL */
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/rc4.h>
#if OPENSSL_VERSION_NUMBER < 0x0090800 || !defined(SHA256_DIGEST_LENGTH)
#error Your OpenSSL is too old, need 0.9.8 or newer with SHA256
#endif
#define HMAC_setup(ctx, key, len)	HMAC_CTX_init(&ctx); HMAC_Init_ex(&ctx, key, len, EVP_sha256(), 0)
#define HMAC_crunch(ctx, buf, len)	HMAC_Update(&ctx, buf, len)
#define HMAC_finish(ctx, dig, len)	HMAC_Final(&ctx, dig, &len); HMAC_CTX_cleanup(&ctx)

typedef RC4_KEY *	RC4_handle;
#define RC4_alloc(h)	*h = malloc(sizeof(RC4_KEY))
#define RC4_setkey(h,l,k)	RC4_set_key(h,l,k)
#define RC4_encrypt(h,l,d)	RC4(h,l,(uint8_t *)d,(uint8_t *)d)
#define RC4_encrypt2(h,l,s,d)	RC4(h,l,(uint8_t *)s,(uint8_t *)d)
#define RC4_free(h)	free(h)
#endif

#define FP10

#include "dh.h"

static const uint8_t GenuineFMSKey[] =
{
    0x47, 0x65, 0x6e, 0x75, 0x69, 0x6e, 0x65, 0x20, 0x41, 0x64, 0x6f, 0x62,
    0x65, 0x20, 0x46, 0x6c,
    0x61, 0x73, 0x68, 0x20, 0x4d, 0x65, 0x64, 0x69, 0x61, 0x20, 0x53, 0x65,
    0x72, 0x76, 0x65, 0x72,
    0x20, 0x30, 0x30, 0x31,	/* Genuine Adobe Flash Media Server 001 */

    0xf0, 0xee, 0xc2, 0x4a, 0x80, 0x68, 0xbe, 0xe8, 0x2e, 0x00, 0xd0, 0xd1,
    0x02, 0x9e, 0x7e, 0x57, 0x6e, 0xec, 0x5d, 0x2d, 0x29, 0x80, 0x6f, 0xab,
    0x93, 0xb8, 0xe6, 0x36,
    0xcf, 0xeb, 0x31, 0xae
};				/* 68 */

static const uint8_t GenuineFPKey[] =
{
    0x47, 0x65, 0x6E, 0x75, 0x69, 0x6E, 0x65, 0x20, 0x41, 0x64, 0x6F, 0x62,
    0x65, 0x20, 0x46, 0x6C,
    0x61, 0x73, 0x68, 0x20, 0x50, 0x6C, 0x61, 0x79, 0x65, 0x72, 0x20, 0x30,
    0x30, 0x31,			/* Genuine Adobe Flash Player 001 */
    0xF0, 0xEE,
    0xC2, 0x4A, 0x80, 0x68, 0xBE, 0xE8, 0x2E, 0x00, 0xD0, 0xD1, 0x02, 0x9E,
    0x7E, 0x57, 0x6E, 0xEC,
    0x5D, 0x2D, 0x29, 0x80, 0x6F, 0xAB, 0x93, 0xB8, 0xE6, 0x36, 0xCF, 0xEB,
    0x31, 0xAE
};				/* 62 */

static void InitRC4Encryption
(uint8_t * secretKey,
 uint8_t * pubKeyIn,
 uint8_t * pubKeyOut, RC4_handle *rc4keyIn, RC4_handle *rc4keyOut)
{
    uint8_t digest[SHA256_DIGEST_LENGTH];
#if !(defined(USE_MBEDTLS) || defined(USE_POLARSSL) || defined(USE_GNUTLS))
    unsigned int digestLen = 0;
#endif
    HMAC_CTX ctx;

    RC4_alloc(rc4keyIn);
    RC4_alloc(rc4keyOut);

    HMAC_setup(ctx, secretKey, 128);
    HMAC_crunch(ctx, pubKeyIn, 128);
#if defined(USE_MBEDTLS) || defined(USE_POLARSSL) || defined(USE_GNUTLS)
    HMAC_finish(ctx, digest);
#else
    HMAC_finish(ctx, digest, digestLen);
#endif

    RTMP_Log(RTMP_LOGDEBUG, "RC4 Out Key: ");
    RTMP_LogHex(RTMP_LOGDEBUG, digest, 16);

    RC4_setkey(*rc4keyOut, 16, digest);

    HMAC_setup(ctx, secretKey, 128);
    HMAC_crunch(ctx, pubKeyOut, 128);
#if defined(USE_MBEDTLS) || defined(USE_POLARSSL) || defined(USE_GNUTLS)
    HMAC_finish(ctx, digest);
#else
    HMAC_finish(ctx, digest, digestLen);
#endif

    RTMP_Log(RTMP_LOGDEBUG, "RC4 In Key: ");
    RTMP_LogHex(RTMP_LOGDEBUG, digest, 16);

    RC4_setkey(*rc4keyIn, 16, digest);
}

typedef unsigned int (getoff)(uint8_t *buf, unsigned int len);

static unsigned int
GetDHOffset2(uint8_t *handshake, unsigned int len)
{
    (void) len;

    unsigned int offset = 0;
    uint8_t *ptr = handshake + 768;
    unsigned int res;

    assert(RTMP_SIG_SIZE <= len);

    offset += (*ptr);
    ptr++;
    offset += (*ptr);
    ptr++;
    offset += (*ptr);
    ptr++;
    offset += (*ptr);

    res = (offset % 632) + 8;

    if (res + 128 > 767)
    {
        RTMP_Log(RTMP_LOGERROR,
                 "%s: Couldn't calculate correct DH offset (got %d), exiting!",
                 __FUNCTION__, res);
        exit(1);
    }
    return res;
}

static unsigned int
GetDigestOffset2(uint8_t *handshake, unsigned int len)
{
    (void) len;

    unsigned int offset = 0;
    uint8_t *ptr = handshake + 772;
    unsigned int res;

    offset += (*ptr);
    ptr++;
    offset += (*ptr);
    ptr++;
    offset += (*ptr);
    ptr++;
    offset += (*ptr);

    res = (offset % 728) + 776;

    if (res + 32 > 1535)
    {
        RTMP_Log(RTMP_LOGERROR,
                 "%s: Couldn't calculate correct digest offset (got %d), exiting",
                 __FUNCTION__, res);
        exit(1);
    }

    (void)len;
    return res;
}

static unsigned int
GetDHOffset1(uint8_t *handshake, unsigned int len)
{
    (void) len;

    unsigned int offset = 0;
    uint8_t *ptr = handshake + 1532;
    unsigned int res;

    assert(RTMP_SIG_SIZE <= len);

    offset += (*ptr);
    ptr++;
    offset += (*ptr);
    ptr++;
    offset += (*ptr);
    ptr++;
    offset += (*ptr);

    res = (offset % 632) + 772;

    if (res + 128 > 1531)
    {
        RTMP_Log(RTMP_LOGERROR, "%s: Couldn't calculate DH offset (got %d), exiting!",
                 __FUNCTION__, res);
        exit(1);
    }

    return res;
}

static unsigned int
GetDigestOffset1(uint8_t *handshake, unsigned int len)
{
    (void) len;

    unsigned int offset = 0;
    uint8_t *ptr = handshake + 8;
    unsigned int res;

    assert(12 <= len);

    offset += (*ptr);
    ptr++;
    offset += (*ptr);
    ptr++;
    offset += (*ptr);
    ptr++;
    offset += (*ptr);

    res = (offset % 728) + 12;

    if (res + 32 > 771)
    {
        RTMP_Log(RTMP_LOGERROR,
                 "%s: Couldn't calculate digest offset (got %d), exiting!",
                 __FUNCTION__, res);
        exit(1);
    }

    return res;
}

static getoff *digoff[] = {GetDigestOffset1, GetDigestOffset2};
static getoff *dhoff[] = {GetDHOffset1, GetDHOffset2};

static void
HMACsha256(const uint8_t *message, size_t messageLen, const uint8_t *key,
           size_t keylen, uint8_t *digest)
{
    unsigned int digestLen;
    HMAC_CTX ctx;

    HMAC_setup(ctx, key, keylen);
    HMAC_crunch(ctx, message, messageLen);

#if defined(USE_MBEDTLS) || defined(USE_POLARSSL) || defined(USE_GNUTLS)
    digestLen = SHA256_DIGEST_LENGTH;
    HMAC_finish(ctx, digest);
#else
    HMAC_finish(ctx, digest, digestLen);
#endif

    assert(digestLen == 32);
    UNUSED_PARAMETER(digestLen); // Make GCC happy digestLen is used in release.
}

static void
CalculateDigest(unsigned int digestPos, uint8_t *handshakeMessage,
                const uint8_t *key, size_t keyLen, uint8_t *digest)
{
    const int messageLen = RTMP_SIG_SIZE - SHA256_DIGEST_LENGTH;
    uint8_t message[RTMP_SIG_SIZE - SHA256_DIGEST_LENGTH];

    memcpy(message, handshakeMessage, digestPos);
    memcpy(message + digestPos,
           &handshakeMessage[digestPos + SHA256_DIGEST_LENGTH],
           messageLen - digestPos);

    HMACsha256(message, messageLen, key, keyLen, digest);
}

static int
VerifyDigest(unsigned int digestPos, uint8_t *handshakeMessage, const uint8_t *key,
             size_t keyLen)
{
    uint8_t calcDigest[SHA256_DIGEST_LENGTH];

    CalculateDigest(digestPos, handshakeMessage, key, keyLen, calcDigest);

    return memcmp(&handshakeMessage[digestPos], calcDigest,
                  SHA256_DIGEST_LENGTH) == 0;
}

/* handshake
 *
 * Type		= [1 bytes] plain: 0x03, encrypted: 0x06, 0x08, 0x09
 * -------------------------------------------------------------------- [1536 bytes]
 * Uptime	= [4 bytes] big endian unsigned number, uptime
 * Version 	= [4 bytes] each byte represents a version number, e.g. 9.0.124.0
 * ...
 *
 */

static const uint32_t rtmpe8_keys[16][4] =
{
    {0xbff034b2, 0x11d9081f, 0xccdfb795, 0x748de732},
    {0x086a5eb6, 0x1743090e, 0x6ef05ab8, 0xfe5a39e2},
    {0x7b10956f, 0x76ce0521, 0x2388a73a, 0x440149a1},
    {0xa943f317, 0xebf11bb2, 0xa691a5ee, 0x17f36339},
    {0x7a30e00a, 0xb529e22c, 0xa087aea5, 0xc0cb79ac},
    {0xbdce0c23, 0x2febdeff, 0x1cfaae16, 0x1123239d},
    {0x55dd3f7b, 0x77e7e62e, 0x9bb8c499, 0xc9481ee4},
    {0x407bb6b4, 0x71e89136, 0xa7aebf55, 0xca33b839},
    {0xfcf6bdc3, 0xb63c3697, 0x7ce4f825, 0x04d959b2},
    {0x28e091fd, 0x41954c4c, 0x7fb7db00, 0xe3a066f8},
    {0x57845b76, 0x4f251b03, 0x46d45bcd, 0xa2c30d29},
    {0x0acceef8, 0xda55b546, 0x03473452, 0x5863713b},
    {0xb82075dc, 0xa75f1fee, 0xd84268e8, 0xa72a44cc},
    {0x07cf6e9e, 0xa16d7b25, 0x9fa7ae6c, 0xd92f5629},
    {0xfeb1eae4, 0x8c8c3ce1, 0x4e0064a7, 0x6a387c2a},
    {0x893a9427, 0xcc3013a2, 0xf106385b, 0xa829f927}
};

/* RTMPE type 8 uses XTEA on the regular signature
 * http://en.wikipedia.org/wiki/XTEA
 */
static void rtmpe8_sig(uint8_t *in, uint8_t *out, int keyid)
{
    unsigned int i, num_rounds = 32;
    uint32_t v0, v1, sum=0, delta=0x9E3779B9;
    uint32_t const *k;

    v0 = in[0] | (in[1] << 8) | (in[2] << 16) | (in[3] << 24);
    v1 = in[4] | (in[5] << 8) | (in[6] << 16) | (in[7] << 24);
    k = rtmpe8_keys[keyid];

    for (i=0; i < num_rounds; i++)
    {
        v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + k[sum & 3]);
        sum += delta;
        v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + k[(sum>>11) & 3]);
    }

    out[0] = v0;
    v0 >>= 8;
    out[1] = v0;
    v0 >>= 8;
    out[2] = v0;
    v0 >>= 8;
    out[3] = v0;

    out[4] = v1;
    v1 >>= 8;
    out[5] = v1;
    v1 >>= 8;
    out[6] = v1;
    v1 >>= 8;
    out[7] = v1;
}

/* RTMPE type 9 uses Blowfish on the regular signature
 * http://en.wikipedia.org/wiki/Blowfish_(cipher)
 */
#define	BF_ROUNDS	16
typedef struct bf_key
{
    uint32_t s[4][256];
    uint32_t p[BF_ROUNDS+2];
} bf_key;

static const uint32_t bf_sinit[][256] =
{

    /* S-Box 0 */
    {
        0xd1310ba6, 0x98dfb5ac, 0x2ffd72db, 0xd01adfb7, 0xb8e1afed, 0x6a267e96,
        0xba7c9045, 0xf12c7f99, 0x24a19947, 0xb3916cf7, 0x0801f2e2, 0x858efc16,
        0x636920d8, 0x71574e69, 0xa458fea3, 0xf4933d7e, 0x0d95748f, 0x728eb658,
        0x718bcd58, 0x82154aee, 0x7b54a41d, 0xc25a59b5, 0x9c30d539, 0x2af26013,
        0xc5d1b023, 0x286085f0, 0xca417918, 0xb8db38ef, 0x8e79dcb0, 0x603a180e,
        0x6c9e0e8b, 0xb01e8a3e, 0xd71577c1, 0xbd314b27, 0x78af2fda, 0x55605c60,
        0xe65525f3, 0xaa55ab94, 0x57489862, 0x63e81440, 0x55ca396a, 0x2aab10b6,
        0xb4cc5c34, 0x1141e8ce, 0xa15486af, 0x7c72e993, 0xb3ee1411, 0x636fbc2a,
        0x2ba9c55d, 0x741831f6, 0xce5c3e16, 0x9b87931e, 0xafd6ba33, 0x6c24cf5c,
        0x7a325381, 0x28958677, 0x3b8f4898, 0x6b4bb9af, 0xc4bfe81b, 0x66282193,
        0x61d809cc, 0xfb21a991, 0x487cac60, 0x5dec8032, 0xef845d5d, 0xe98575b1,
        0xdc262302, 0xeb651b88, 0x23893e81, 0xd396acc5, 0x0f6d6ff3, 0x83f44239,
        0x2e0b4482, 0xa4842004, 0x69c8f04a, 0x9e1f9b5e, 0x21c66842, 0xf6e96c9a,
        0x670c9c61, 0xabd388f0, 0x6a51a0d2, 0xd8542f68, 0x960fa728, 0xab5133a3,
        0x6eef0b6c, 0x137a3be4, 0xba3bf050, 0x7efb2a98, 0xa1f1651d, 0x39af0176,
        0x66ca593e, 0x82430e88, 0x8cee8619, 0x456f9fb4, 0x7d84a5c3, 0x3b8b5ebe,
        0xe06f75d8, 0x85c12073, 0x401a449f, 0x56c16aa6, 0x4ed3aa62, 0x363f7706,
        0x1bfedf72, 0x429b023d, 0x37d0d724, 0xd00a1248, 0xdb0fead3, 0x49f1c09b,
        0x075372c9, 0x80991b7b, 0x25d479d8, 0xf6e8def7, 0xe3fe501a, 0xb6794c3b,
        0x976ce0bd, 0x04c006ba, 0xc1a94fb6, 0x409f60c4, 0x5e5c9ec2, 0x196a2463,
        0x68fb6faf, 0x3e6c53b5, 0x1339b2eb, 0x3b52ec6f, 0x6dfc511f, 0x9b30952c,
        0xcc814544, 0xaf5ebd09, 0xbee3d004, 0xde334afd, 0x660f2807, 0x192e4bb3,
        0xc0cba857, 0x45c8740f, 0xd20b5f39, 0xb9d3fbdb, 0x5579c0bd, 0x1a60320a,
        0xd6a100c6, 0x402c7279, 0x679f25fe, 0xfb1fa3cc, 0x8ea5e9f8, 0xdb3222f8,
        0x3c7516df, 0xfd616b15, 0x2f501ec8, 0xad0552ab, 0x323db5fa, 0xfd238760,
        0x53317b48, 0x3e00df82, 0x9e5c57bb, 0xca6f8ca0, 0x1a87562e, 0xdf1769db,
        0xd542a8f6, 0x287effc3, 0xac6732c6, 0x8c4f5573, 0x695b27b0, 0xbbca58c8,
        0xe1ffa35d, 0xb8f011a0, 0x10fa3d98, 0xfd2183b8, 0x4afcb56c, 0x2dd1d35b,
        0x9a53e479, 0xb6f84565, 0xd28e49bc, 0x4bfb9790, 0xe1ddf2da, 0xa4cb7e33,
        0x62fb1341, 0xcee4c6e8, 0xef20cada, 0x36774c01, 0xd07e9efe, 0x2bf11fb4,
        0x95dbda4d, 0xae909198, 0xeaad8e71, 0x6b93d5a0, 0xd08ed1d0, 0xafc725e0,
        0x8e3c5b2f, 0x8e7594b7, 0x8ff6e2fb, 0xf2122b64, 0x8888b812, 0x900df01c,
        0x4fad5ea0, 0x688fc31c, 0xd1cff191, 0xb3a8c1ad, 0x2f2f2218, 0xbe0e1777,
        0xea752dfe, 0x8b021fa1, 0xe5a0cc0f, 0xb56f74e8, 0x18acf3d6, 0xce89e299,
        0xb4a84fe0, 0xfd13e0b7, 0x7cc43b81, 0xd2ada8d9, 0x165fa266, 0x80957705,
        0x93cc7314, 0x211a1477, 0xe6ad2065, 0x77b5fa86, 0xc75442f5, 0xfb9d35cf,
        0xebcdaf0c, 0x7b3e89a0, 0xd6411bd3, 0xae1e7e49, 0x00250e2d, 0x2071b35e,
        0x226800bb, 0x57b8e0af, 0x2464369b, 0xf009b91e, 0x5563911d, 0x59dfa6aa,
        0x78c14389, 0xd95a537f, 0x207d5ba2, 0x02e5b9c5, 0x83260376, 0x6295cfa9,
        0x11c81968, 0x4e734a41, 0xb3472dca, 0x7b14a94a, 0x1b510052, 0x9a532915,
        0xd60f573f, 0xbc9bc6e4, 0x2b60a476, 0x81e67400, 0x08ba6fb5, 0x571be91f,
        0xf296ec6b, 0x2a0dd915, 0xb6636521, 0xe7b9f9b6, 0xff34052e, 0xc5855664,
        0x53b02d5d, 0xa99f8fa1, 0x08ba4799, 0x6e85076a,
    },

    /* S-Box 1 */
    {
        0x4b7a70e9, 0xb5b32944, 0xdb75092e, 0xc4192623, 0xad6ea6b0, 0x49a7df7d,
        0x9cee60b8, 0x8fedb266, 0xecaa8c71, 0x699a17ff, 0x5664526c, 0xc2b19ee1,
        0x193602a5, 0x75094c29, 0xa0591340, 0xe4183a3e, 0x3f54989a, 0x5b429d65,
        0x6b8fe4d6, 0x99f73fd6, 0xa1d29c07, 0xefe830f5, 0x4d2d38e6, 0xf0255dc1,
        0x4cdd2086, 0x8470eb26, 0x6382e9c6, 0x021ecc5e, 0x09686b3f, 0x3ebaefc9,
        0x3c971814, 0x6b6a70a1, 0x687f3584, 0x52a0e286, 0xb79c5305, 0xaa500737,
        0x3e07841c, 0x7fdeae5c, 0x8e7d44ec, 0x5716f2b8, 0xb03ada37, 0xf0500c0d,
        0xf01c1f04, 0x0200b3ff, 0xae0cf51a, 0x3cb574b2, 0x25837a58, 0xdc0921bd,
        0xd19113f9, 0x7ca92ff6, 0x94324773, 0x22f54701, 0x3ae5e581, 0x37c2dadc,
        0xc8b57634, 0x9af3dda7, 0xa9446146, 0x0fd0030e, 0xecc8c73e, 0xa4751e41,
        0xe238cd99, 0x3bea0e2f, 0x3280bba1, 0x183eb331, 0x4e548b38, 0x4f6db908,
        0x6f420d03, 0xf60a04bf, 0x2cb81290, 0x24977c79, 0x5679b072, 0xbcaf89af,
        0xde9a771f, 0xd9930810, 0xb38bae12, 0xdccf3f2e, 0x5512721f, 0x2e6b7124,
        0x501adde6, 0x9f84cd87, 0x7a584718, 0x7408da17, 0xbc9f9abc, 0xe94b7d8c,
        0xec7aec3a, 0xdb851dfa, 0x63094366, 0xc464c3d2, 0xef1c1847, 0x3215d908,
        0xdd433b37, 0x24c2ba16, 0x12a14d43, 0x2a65c451, 0x50940002, 0x133ae4dd,
        0x71dff89e, 0x10314e55, 0x81ac77d6, 0x5f11199b, 0x043556f1, 0xd7a3c76b,
        0x3c11183b, 0x5924a509, 0xf28fe6ed, 0x97f1fbfa, 0x9ebabf2c, 0x1e153c6e,
        0x86e34570, 0xeae96fb1, 0x860e5e0a, 0x5a3e2ab3, 0x771fe71c, 0x4e3d06fa,
        0x2965dcb9, 0x99e71d0f, 0x803e89d6, 0x5266c825, 0x2e4cc978, 0x9c10b36a,
        0xc6150eba, 0x94e2ea78, 0xa5fc3c53, 0x1e0a2df4, 0xf2f74ea7, 0x361d2b3d,
        0x1939260f, 0x19c27960, 0x5223a708, 0xf71312b6, 0xebadfe6e, 0xeac31f66,
        0xe3bc4595, 0xa67bc883, 0xb17f37d1, 0x018cff28, 0xc332ddef, 0xbe6c5aa5,
        0x65582185, 0x68ab9802, 0xeecea50f, 0xdb2f953b, 0x2aef7dad, 0x5b6e2f84,
        0x1521b628, 0x29076170, 0xecdd4775, 0x619f1510, 0x13cca830, 0xeb61bd96,
        0x0334fe1e, 0xaa0363cf, 0xb5735c90, 0x4c70a239, 0xd59e9e0b, 0xcbaade14,
        0xeecc86bc, 0x60622ca7, 0x9cab5cab, 0xb2f3846e, 0x648b1eaf, 0x19bdf0ca,
        0xa02369b9, 0x655abb50, 0x40685a32, 0x3c2ab4b3, 0x319ee9d5, 0xc021b8f7,
        0x9b540b19, 0x875fa099, 0x95f7997e, 0x623d7da8, 0xf837889a, 0x97e32d77,
        0x11ed935f, 0x16681281, 0x0e358829, 0xc7e61fd6, 0x96dedfa1, 0x7858ba99,
        0x57f584a5, 0x1b227263, 0x9b83c3ff, 0x1ac24696, 0xcdb30aeb, 0x532e3054,
        0x8fd948e4, 0x6dbc3128, 0x58ebf2ef, 0x34c6ffea, 0xfe28ed61, 0xee7c3c73,
        0x5d4a14d9, 0xe864b7e3, 0x42105d14, 0x203e13e0, 0x45eee2b6, 0xa3aaabea,
        0xdb6c4f15, 0xfacb4fd0, 0xc742f442, 0xef6abbb5, 0x654f3b1d, 0x41cd2105,
        0xd81e799e, 0x86854dc7, 0xe44b476a, 0x3d816250, 0xcf62a1f2, 0x5b8d2646,
        0xfc8883a0, 0xc1c7b6a3, 0x7f1524c3, 0x69cb7492, 0x47848a0b, 0x5692b285,
        0x095bbf00, 0xad19489d, 0x1462b174, 0x23820e00, 0x58428d2a, 0x0c55f5ea,
        0x1dadf43e, 0x233f7061, 0x3372f092, 0x8d937e41, 0xd65fecf1, 0x6c223bdb,
        0x7cde3759, 0xcbee7460, 0x4085f2a7, 0xce77326e, 0xa6078084, 0x19f8509e,
        0xe8efd855, 0x61d99735, 0xa969a7aa, 0xc50c06c2, 0x5a04abfc, 0x800bcadc,
        0x9e447a2e, 0xc3453484, 0xfdd56705, 0x0e1e9ec9, 0xdb73dbd3, 0x105588cd,
        0x675fda79, 0xe3674340, 0xc5c43465, 0x713e38d8, 0x3d28f89e, 0xf16dff20,
        0x153e21e7, 0x8fb03d4a, 0xe6e39f2b, 0xdb83adf7,
    },

    /* S-Box 2 */
    {
        0xe93d5a68, 0x948140f7, 0xf64c261c, 0x94692934, 0x411520f7, 0x7602d4f7,
        0xbcf46b2e, 0xd4a20068, 0xd4082471, 0x3320f46a, 0x43b7d4b7, 0x500061af,
        0x1e39f62e, 0x97244546, 0x14214f74, 0xbf8b8840, 0x4d95fc1d, 0x96b591af,
        0x70f4ddd3, 0x66a02f45, 0xbfbc09ec, 0x03bd9785, 0x7fac6dd0, 0x31cb8504,
        0x96eb27b3, 0x55fd3941, 0xda2547e6, 0xabca0a9a, 0x28507825, 0x530429f4,
        0x0a2c86da, 0xe9b66dfb, 0x68dc1462, 0xd7486900, 0x680ec0a4, 0x27a18dee,
        0x4f3ffea2, 0xe887ad8c, 0xb58ce006, 0x7af4d6b6, 0xaace1e7c, 0xd3375fec,
        0xce78a399, 0x406b2a42, 0x20fe9e35, 0xd9f385b9, 0xee39d7ab, 0x3b124e8b,
        0x1dc9faf7, 0x4b6d1856, 0x26a36631, 0xeae397b2, 0x3a6efa74, 0xdd5b4332,
        0x6841e7f7, 0xca7820fb, 0xfb0af54e, 0xd8feb397, 0x454056ac, 0xba489527,
        0x55533a3a, 0x20838d87, 0xfe6ba9b7, 0xd096954b, 0x55a867bc, 0xa1159a58,
        0xcca92963, 0x99e1db33, 0xa62a4a56, 0x3f3125f9, 0x5ef47e1c, 0x9029317c,
        0xfdf8e802, 0x04272f70, 0x80bb155c, 0x05282ce3, 0x95c11548, 0xe4c66d22,
        0x48c1133f, 0xc70f86dc, 0x07f9c9ee, 0x41041f0f, 0x404779a4, 0x5d886e17,
        0x325f51eb, 0xd59bc0d1, 0xf2bcc18f, 0x41113564, 0x257b7834, 0x602a9c60,
        0xdff8e8a3, 0x1f636c1b, 0x0e12b4c2, 0x02e1329e, 0xaf664fd1, 0xcad18115,
        0x6b2395e0, 0x333e92e1, 0x3b240b62, 0xeebeb922, 0x85b2a20e, 0xe6ba0d99,
        0xde720c8c, 0x2da2f728, 0xd0127845, 0x95b794fd, 0x647d0862, 0xe7ccf5f0,
        0x5449a36f, 0x877d48fa, 0xc39dfd27, 0xf33e8d1e, 0x0a476341, 0x992eff74,
        0x3a6f6eab, 0xf4f8fd37, 0xa812dc60, 0xa1ebddf8, 0x991be14c, 0xdb6e6b0d,
        0xc67b5510, 0x6d672c37, 0x2765d43b, 0xdcd0e804, 0xf1290dc7, 0xcc00ffa3,
        0xb5390f92, 0x690fed0b, 0x667b9ffb, 0xcedb7d9c, 0xa091cf0b, 0xd9155ea3,
        0xbb132f88, 0x515bad24, 0x7b9479bf, 0x763bd6eb, 0x37392eb3, 0xcc115979,
        0x8026e297, 0xf42e312d, 0x6842ada7, 0xc66a2b3b, 0x12754ccc, 0x782ef11c,
        0x6a124237, 0xb79251e7, 0x06a1bbe6, 0x4bfb6350, 0x1a6b1018, 0x11caedfa,
        0x3d25bdd8, 0xe2e1c3c9, 0x44421659, 0x0a121386, 0xd90cec6e, 0xd5abea2a,
        0x64af674e, 0xda86a85f, 0xbebfe988, 0x64e4c3fe, 0x9dbc8057, 0xf0f7c086,
        0x60787bf8, 0x6003604d, 0xd1fd8346, 0xf6381fb0, 0x7745ae04, 0xd736fccc,
        0x83426b33, 0xf01eab71, 0xb0804187, 0x3c005e5f, 0x77a057be, 0xbde8ae24,
        0x55464299, 0xbf582e61, 0x4e58f48f, 0xf2ddfda2, 0xf474ef38, 0x8789bdc2,
        0x5366f9c3, 0xc8b38e74, 0xb475f255, 0x46fcd9b9, 0x7aeb2661, 0x8b1ddf84,
        0x846a0e79, 0x915f95e2, 0x466e598e, 0x20b45770, 0x8cd55591, 0xc902de4c,
        0xb90bace1, 0xbb8205d0, 0x11a86248, 0x7574a99e, 0xb77f19b6, 0xe0a9dc09,
        0x662d09a1, 0xc4324633, 0xe85a1f02, 0x09f0be8c, 0x4a99a025, 0x1d6efe10,
        0x1ab93d1d, 0x0ba5a4df, 0xa186f20f, 0x2868f169, 0xdcb7da83, 0x573906fe,
        0xa1e2ce9b, 0x4fcd7f52, 0x50115e01, 0xa70683fa, 0xa002b5c4, 0x0de6d027,
        0x9af88c27, 0x773f8641, 0xc3604c06, 0x61a806b5, 0xf0177a28, 0xc0f586e0,
        0x006058aa, 0x30dc7d62, 0x11e69ed7, 0x2338ea63, 0x53c2dd94, 0xc2c21634,
        0xbbcbee56, 0x90bcb6de, 0xebfc7da1, 0xce591d76, 0x6f05e409, 0x4b7c0188,
        0x39720a3d, 0x7c927c24, 0x86e3725f, 0x724d9db9, 0x1ac15bb4, 0xd39eb8fc,
        0xed545578, 0x08fca5b5, 0xd83d7cd3, 0x4dad0fc4, 0x1e50ef5e, 0xb161e6f8,
        0xa28514d9, 0x6c51133c, 0x6fd5c7e7, 0x56e14ec4, 0x362abfce, 0xddc6c837,
        0xd79a3234, 0x92638212, 0x670efa8e, 0x406000e0,
    },

    /* S-Box 3 */
    {
        0x3a39ce37, 0xd3faf5cf, 0xabc27737, 0x5ac52d1b, 0x5cb0679e, 0x4fa33742,
        0xd3822740, 0x99bc9bbe, 0xd5118e9d, 0xbf0f7315, 0xd62d1c7e, 0xc700c47b,
        0xb78c1b6b, 0x21a19045, 0xb26eb1be, 0x6a366eb4, 0x5748ab2f, 0xbc946e79,
        0xc6a376d2, 0x6549c2c8, 0x530ff8ee, 0x468dde7d, 0xd5730a1d, 0x4cd04dc6,
        0x2939bbdb, 0xa9ba4650, 0xac9526e8, 0xbe5ee304, 0xa1fad5f0, 0x6a2d519a,
        0x63ef8ce2, 0x9a86ee22, 0xc089c2b8, 0x43242ef6, 0xa51e03aa, 0x9cf2d0a4,
        0x83c061ba, 0x9be96a4d, 0x8fe51550, 0xba645bd6, 0x2826a2f9, 0xa73a3ae1,
        0x4ba99586, 0xef5562e9, 0xc72fefd3, 0xf752f7da, 0x3f046f69, 0x77fa0a59,
        0x80e4a915, 0x87b08601, 0x9b09e6ad, 0x3b3ee593, 0xe990fd5a, 0x9e34d797,
        0x2cf0b7d9, 0x022b8b51, 0x96d5ac3a, 0x017da67d, 0xd1cf3ed6, 0x7c7d2d28,
        0x1f9f25cf, 0xadf2b89b, 0x5ad6b472, 0x5a88f54c, 0xe029ac71, 0xe019a5e6,
        0x47b0acfd, 0xed93fa9b, 0xe8d3c48d, 0x283b57cc, 0xf8d56629, 0x79132e28,
        0x785f0191, 0xed756055, 0xf7960e44, 0xe3d35e8c, 0x15056dd4, 0x88f46dba,
        0x03a16125, 0x0564f0bd, 0xc3eb9e15, 0x3c9057a2, 0x97271aec, 0xa93a072a,
        0x1b3f6d9b, 0x1e6321f5, 0xf59c66fb, 0x26dcf319, 0x7533d928, 0xb155fdf5,
        0x03563482, 0x8aba3cbb, 0x28517711, 0xc20ad9f8, 0xabcc5167, 0xccad925f,
        0x4de81751, 0x3830dc8e, 0x379d5862, 0x9320f991, 0xea7a90c2, 0xfb3e7bce,
        0x5121ce64, 0x774fbe32, 0xa8b6e37e, 0xc3293d46, 0x48de5369, 0x6413e680,
        0xa2ae0810, 0xdd6db224, 0x69852dfd, 0x09072166, 0xb39a460a, 0x6445c0dd,
        0x586cdecf, 0x1c20c8ae, 0x5bbef7dd, 0x1b588d40, 0xccd2017f, 0x6bb4e3bb,
        0xdda26a7e, 0x3a59ff45, 0x3e350a44, 0xbcb4cdd5, 0x72eacea8, 0xfa6484bb,
        0x8d6612ae, 0xbf3c6f47, 0xd29be463, 0x542f5d9e, 0xaec2771b, 0xf64e6370,
        0x740e0d8d, 0xe75b1357, 0xf8721671, 0xaf537d5d, 0x4040cb08, 0x4eb4e2cc,
        0x34d2466a, 0x0115af84, 0xe1b00428, 0x95983a1d, 0x06b89fb4, 0xce6ea048,
        0x6f3f3b82, 0x3520ab82, 0x011a1d4b, 0x277227f8, 0x611560b1, 0xe7933fdc,
        0xbb3a792b, 0x344525bd, 0xa08839e1, 0x51ce794b, 0x2f32c9b7, 0xa01fbac9,
        0xe01cc87e, 0xbcc7d1f6, 0xcf0111c3, 0xa1e8aac7, 0x1a908749, 0xd44fbd9a,
        0xd0dadecb, 0xd50ada38, 0x0339c32a, 0xc6913667, 0x8df9317c, 0xe0b12b4f,
        0xf79e59b7, 0x43f5bb3a, 0xf2d519ff, 0x27d9459c, 0xbf97222c, 0x15e6fc2a,
        0x0f91fc71, 0x9b941525, 0xfae59361, 0xceb69ceb, 0xc2a86459, 0x12baa8d1,
        0xb6c1075e, 0xe3056a0c, 0x10d25065, 0xcb03a442, 0xe0ec6e0e, 0x1698db3b,
        0x4c98a0be, 0x3278e964, 0x9f1f9532, 0xe0d392df, 0xd3a0342b, 0x8971f21e,
        0x1b0a7441, 0x4ba3348c, 0xc5be7120, 0xc37632d8, 0xdf359f8d, 0x9b992f2e,
        0xe60b6f47, 0x0fe3f11d, 0xe54cda54, 0x1edad891, 0xce6279cf, 0xcd3e7e6f,
        0x1618b166, 0xfd2c1d05, 0x848fd2c5, 0xf6fb2299, 0xf523f357, 0xa6327623,
        0x93a83531, 0x56cccd02, 0xacf08162, 0x5a75ebb5, 0x6e163697, 0x88d273cc,
        0xde966292, 0x81b949d0, 0x4c50901b, 0x71c65614, 0xe6c6c7bd, 0x327a140a,
        0x45e1d006, 0xc3f27b9a, 0xc9aa53fd, 0x62a80f00, 0xbb25bfe2, 0x35bdd2f6,
        0x71126905, 0xb2040222, 0xb6cbcf7c, 0xcd769c2b, 0x53113ec0, 0x1640e3d3,
        0x38abbd60, 0x2547adf0, 0xba38209c, 0xf746ce76, 0x77afa1c5, 0x20756060,
        0x85cbfe4e, 0x8ae88dd8, 0x7aaaf9b0, 0x4cf9aa7e, 0x1948c25c, 0x02fb8a8c,
        0x01c36ae4, 0xd6ebe1f9, 0x90d4f869, 0xa65cdea0, 0x3f09252d, 0xc208e69f,
        0xb74e6132, 0xce77e25b, 0x578fdfe3, 0x3ac372e6,
    },
};

static const uint32_t bf_pinit[] =
{
    /* P-Box */
    0x243f6a88, 0x85a308d3, 0x13198a2e, 0x03707344, 0xa4093822, 0x299f31d0,
    0x082efa98, 0xec4e6c89, 0x452821e6, 0x38d01377, 0xbe5466cf, 0x34e90c6c,
    0xc0ac29b7, 0xc97c50dd, 0x3f84d5b5, 0xb5470917, 0x9216d5d9, 0x8979fb1b,
};

#define KEYBYTES	24

static const unsigned char rtmpe9_keys[16][KEYBYTES] =
{
    {
        0x79, 0x34, 0x77, 0x4c, 0x67, 0xd1, 0x38, 0x3a, 0xdf, 0xb3, 0x56, 0xbe,
        0x8b, 0x7b, 0xd0, 0x24, 0x38, 0xe0, 0x73, 0x58, 0x41, 0x5d, 0x69, 0x67,
    },
    {
        0x46, 0xf6, 0xb4, 0xcc, 0x01, 0x93, 0xe3, 0xa1, 0x9e, 0x7d, 0x3c, 0x65,
        0x55, 0x86, 0xfd, 0x09, 0x8f, 0xf7, 0xb3, 0xc4, 0x6f, 0x41, 0xca, 0x5c,
    },
    {
        0x1a, 0xe7, 0xe2, 0xf3, 0xf9, 0x14, 0x79, 0x94, 0xc0, 0xd3, 0x97, 0x43,
        0x08, 0x7b, 0xb3, 0x84, 0x43, 0x2f, 0x9d, 0x84, 0x3f, 0x21, 0x01, 0x9b,
    },
    {
        0xd3, 0xe3, 0x54, 0xb0, 0xf7, 0x1d, 0xf6, 0x2b, 0x5a, 0x43, 0x4d, 0x04,
        0x83, 0x64, 0x3e, 0x0d, 0x59, 0x2f, 0x61, 0xcb, 0xb1, 0x6a, 0x59, 0x0d,
    },
    {
        0xc8, 0xc1, 0xe9, 0xb8, 0x16, 0x56, 0x99, 0x21, 0x7b, 0x5b, 0x36, 0xb7,
        0xb5, 0x9b, 0xdf, 0x06, 0x49, 0x2c, 0x97, 0xf5, 0x95, 0x48, 0x85, 0x7e,
    },
    {
        0xeb, 0xe5, 0xe6, 0x2e, 0xa4, 0xba, 0xd4, 0x2c, 0xf2, 0x16, 0xe0, 0x8f,
        0x66, 0x23, 0xa9, 0x43, 0x41, 0xce, 0x38, 0x14, 0x84, 0x95, 0x00, 0x53,
    },
    {
        0x66, 0xdb, 0x90, 0xf0, 0x3b, 0x4f, 0xf5, 0x6f, 0xe4, 0x9c, 0x20, 0x89,
        0x35, 0x5e, 0xd2, 0xb2, 0xc3, 0x9e, 0x9f, 0x7f, 0x63, 0xb2, 0x28, 0x81,
    },
    {
        0xbb, 0x20, 0xac, 0xed, 0x2a, 0x04, 0x6a, 0x19, 0x94, 0x98, 0x9b, 0xc8,
        0xff, 0xcd, 0x93, 0xef, 0xc6, 0x0d, 0x56, 0xa7, 0xeb, 0x13, 0xd9, 0x30,
    },
    {
        0xbc, 0xf2, 0x43, 0x82, 0x09, 0x40, 0x8a, 0x87, 0x25, 0x43, 0x6d, 0xe6,
        0xbb, 0xa4, 0xb9, 0x44, 0x58, 0x3f, 0x21, 0x7c, 0x99, 0xbb, 0x3f, 0x24,
    },
    {
        0xec, 0x1a, 0xaa, 0xcd, 0xce, 0xbd, 0x53, 0x11, 0xd2, 0xfb, 0x83, 0xb6,
        0xc3, 0xba, 0xab, 0x4f, 0x62, 0x79, 0xe8, 0x65, 0xa9, 0x92, 0x28, 0x76,
    },
    {
        0xc6, 0x0c, 0x30, 0x03, 0x91, 0x18, 0x2d, 0x7b, 0x79, 0xda, 0xe1, 0xd5,
        0x64, 0x77, 0x9a, 0x12, 0xc5, 0xb1, 0xd7, 0x91, 0x4f, 0x96, 0x4c, 0xa3,
    },
    {
        0xd7, 0x7c, 0x2a, 0xbf, 0xa6, 0xe7, 0x85, 0x7c, 0x45, 0xad, 0xff, 0x12,
        0x94, 0xd8, 0xde, 0xa4, 0x5c, 0x3d, 0x79, 0xa4, 0x44, 0x02, 0x5d, 0x22,
    },
    {
        0x16, 0x19, 0x0d, 0x81, 0x6a, 0x4c, 0xc7, 0xf8, 0xb8, 0xf9, 0x4e, 0xcd,
        0x2c, 0x9e, 0x90, 0x84, 0xb2, 0x08, 0x25, 0x60, 0xe1, 0x1e, 0xae, 0x18,
    },
    {
        0xe9, 0x7c, 0x58, 0x26, 0x1b, 0x51, 0x9e, 0x49, 0x82, 0x60, 0x61, 0xfc,
        0xa0, 0xa0, 0x1b, 0xcd, 0xf5, 0x05, 0xd6, 0xa6, 0x6d, 0x07, 0x88, 0xa3,
    },
    {
        0x2b, 0x97, 0x11, 0x8b, 0xd9, 0x4e, 0xd9, 0xdf, 0x20, 0xe3, 0x9c, 0x10,
        0xe6, 0xa1, 0x35, 0x21, 0x11, 0xf9, 0x13, 0x0d, 0x0b, 0x24, 0x65, 0xb2,
    },
    {
        0x53, 0x6a, 0x4c, 0x54, 0xac, 0x8b, 0x9b, 0xb8, 0x97, 0x29, 0xfc, 0x60,
        0x2c, 0x5b, 0x3a, 0x85, 0x68, 0xb5, 0xaa, 0x6a, 0x44, 0xcd, 0x3f, 0xa7,
    },
};

#define	BF_ENC(X,S) \
	(((S[0][X>>24] + S[1][X>>16 & 0xff]) ^ S[2][(X>>8) & 0xff]) + S[3][X & 0xff])

static void bf_enc(uint32_t *x, bf_key *key)
{
    uint32_t  Xl;
    uint32_t  Xr;
    uint32_t  temp;
    int	i;

    Xl = x[0];
    Xr = x[1];

    for (i = 0; i < BF_ROUNDS; ++i)
    {
        Xl ^= key->p[i];
        Xr ^= BF_ENC(Xl,key->s);

        temp = Xl;
        Xl = Xr;
        Xr = temp;
    }

    Xl ^= key->p[BF_ROUNDS];
    Xr ^= key->p[BF_ROUNDS + 1];

    x[0] = Xr;
    x[1] = Xl;
}

static void bf_setkey(const unsigned char *kp, int keybytes, bf_key *key)
{
    int          i;
    int          j;
    int          k;
    uint32_t  data;
    uint32_t  d[2];

    memcpy(key->p, bf_pinit, sizeof(key->p));
    memcpy(key->s, bf_sinit, sizeof(key->s));

    j = 0;
    for (i = 0; i < BF_ROUNDS + 2; ++i)
    {
        data = 0x00000000;
        for (k = 0; k < 4; ++k)
        {
            data = (data << 8) | kp[j];
            j = j + 1;
            if (j >= keybytes)
            {
                j = 0;
            }
        }
        key->p[i] ^= data;
    }

    d[0] = 0x00000000;
    d[1] = 0x00000000;

    for (i = 0; i < BF_ROUNDS + 2; i += 2)
    {
        bf_enc(d, key);

        key->p[i] = d[0];
        key->p[i + 1] = d[1];
    }

    for (i = 0; i < 4; ++i)
    {
        for (j = 0; j < 256; j += 2)
        {

            bf_enc(d, key);

            key->s[i][j] = d[0];
            key->s[i][j + 1] = d[1];
        }
    }
}

static void rtmpe9_sig(uint8_t *in, uint8_t *out, int keyid)
{
    uint32_t d[2];
    bf_key key;

    bf_setkey(rtmpe9_keys[keyid], KEYBYTES, &key);

    /* input is little-endian */
    d[0] = in[0] | (in[1] << 8) | (in[2] << 16) | (in[3] << 24);
    d[1] = in[4] | (in[5] << 8) | (in[6] << 16) | (in[7] << 24);
    bf_enc(d, &key);
    out[0] = d[0] & 0xff;
    out[1] = (d[0] >> 8) & 0xff;
    out[2] = (d[0] >> 16) & 0xff;
    out[3] = (d[0] >> 24) & 0xff;
    out[4] = d[1] & 0xff;
    out[5] = (d[1] >> 8) & 0xff;
    out[6] = (d[1] >> 16) & 0xff;
    out[7] = (d[1] >> 24) & 0xff;
}

static int
HandShake(RTMP * r, int FP9HandShake)
{
    int i, offalg = 0;
    int dhposClient = 0;
    int digestPosClient = 0;
    int encrypted = r->Link.protocol & RTMP_FEATURE_ENC;

    RC4_handle keyIn = 0;
    RC4_handle keyOut = 0;

#ifndef _DEBUG
    int32_t *ip;
#endif
    uint32_t uptime;

    uint8_t clientbuf[RTMP_SIG_SIZE + 4], *clientsig=clientbuf+4;
    uint8_t serversig[RTMP_SIG_SIZE], client2[RTMP_SIG_SIZE], *reply;
    uint8_t type;
    getoff *getdh = NULL, *getdig = NULL;

    if (encrypted || r->Link.SWFSize)
        FP9HandShake = TRUE;
    else
        FP9HandShake = FALSE;

    r->Link.rc4keyIn = r->Link.rc4keyOut = 0;

    if (encrypted)
    {
        clientsig[-1] = 0x06;	/* 0x08 is RTMPE as well */
        offalg = 1;
    }
    else
        clientsig[-1] = 0x03;

    uptime = htonl(RTMP_GetTime());
    memcpy(clientsig, &uptime, 4);

    if (FP9HandShake)
    {
        /* set version to at least 9.0.115.0 */
        if (encrypted)
        {
            clientsig[4] = 128;
            clientsig[6] = 3;
        }
        else
        {
            clientsig[4] = 10;
            clientsig[6] = 45;
        }
        clientsig[5] = 0;
        clientsig[7] = 2;

        RTMP_Log(RTMP_LOGDEBUG, "%s: Client type: %02X", __FUNCTION__, clientsig[-1]);
        getdig = digoff[offalg];
        getdh  = dhoff[offalg];
    }
    else
    {
        memset(&clientsig[4], 0, 4);
    }

    /* generate random data */
#ifdef _DEBUG
    memset(clientsig+8, 0, RTMP_SIG_SIZE-8);
#else
    ip = (int32_t *)(clientsig+8);
    for (i = 2; i < RTMP_SIG_SIZE/4; i++)
        *ip++ = rand();
#endif

    /* set handshake digest */
    if (FP9HandShake)
    {
        if (encrypted)
        {
            /* generate Diffie-Hellmann parameters */
            r->Link.dh = DHInit(1024);
            if (!r->Link.dh)
            {
                RTMP_Log(RTMP_LOGERROR, "%s: Couldn't initialize Diffie-Hellmann!",
                         __FUNCTION__);
                return FALSE;
            }

            dhposClient = getdh(clientsig, RTMP_SIG_SIZE);
            RTMP_Log(RTMP_LOGDEBUG, "%s: DH pubkey position: %d", __FUNCTION__, dhposClient);

            if (!DHGenerateKey(r))
            {
                RTMP_Log(RTMP_LOGERROR, "%s: Couldn't generate Diffie-Hellmann public key!",
                         __FUNCTION__);
                return FALSE;
            }

            if (!DHGetPublicKey(r->Link.dh, &clientsig[dhposClient], 128))
            {
                RTMP_Log(RTMP_LOGERROR, "%s: Couldn't write public key!", __FUNCTION__);
                return FALSE;
            }
        }

        digestPosClient = getdig(clientsig, RTMP_SIG_SIZE);	/* reuse this value in verification */
        RTMP_Log(RTMP_LOGDEBUG, "%s: Client digest offset: %d", __FUNCTION__,
                 digestPosClient);

        CalculateDigest(digestPosClient, clientsig, GenuineFPKey, 30,
                        &clientsig[digestPosClient]);

        RTMP_Log(RTMP_LOGDEBUG, "%s: Initial client digest: ", __FUNCTION__);
        RTMP_LogHex(RTMP_LOGDEBUG, clientsig + digestPosClient,
                    SHA256_DIGEST_LENGTH);
    }

#ifdef _DEBUG
    RTMP_Log(RTMP_LOGDEBUG, "Clientsig: ");
    RTMP_LogHex(RTMP_LOGDEBUG, clientsig, RTMP_SIG_SIZE);
#endif

    if (!WriteN(r, (char *)clientsig-1, RTMP_SIG_SIZE + 1))
        return FALSE;

    if (ReadN(r, (char *)&type, 1) != 1)	/* 0x03 or 0x06 */
        return FALSE;

    RTMP_Log(RTMP_LOGDEBUG, "%s: Type Answer   : %02X", __FUNCTION__, type);

    if (type != clientsig[-1])
        RTMP_Log(RTMP_LOGWARNING, "%s: Type mismatch: client sent %d, server answered %d",
                 __FUNCTION__, clientsig[-1], type);

    if (ReadN(r, (char *)serversig, RTMP_SIG_SIZE) != RTMP_SIG_SIZE)
        return FALSE;

    /* decode server response */
    memcpy(&uptime, serversig, 4);
    uptime = ntohl(uptime);

    RTMP_Log(RTMP_LOGDEBUG, "%s: Server Uptime : %d", __FUNCTION__, uptime);
    RTMP_Log(RTMP_LOGDEBUG, "%s: FMS Version   : %d.%d.%d.%d", __FUNCTION__, serversig[4],
             serversig[5], serversig[6], serversig[7]);

    if (FP9HandShake && type == 3 && !serversig[4])
        FP9HandShake = FALSE;

#ifdef _DEBUG
    RTMP_Log(RTMP_LOGDEBUG, "Server signature:");
    RTMP_LogHex(RTMP_LOGDEBUG, serversig, RTMP_SIG_SIZE);
#endif

    if (FP9HandShake)
    {
        uint8_t digestResp[SHA256_DIGEST_LENGTH];
        uint8_t *signatureResp = NULL;

        /* we have to use this signature now to find the correct algorithms for getting the digest and DH positions */
        int digestPosServer = getdig(serversig, RTMP_SIG_SIZE);

        if (!VerifyDigest(digestPosServer, serversig, GenuineFMSKey, 36))
        {
            RTMP_Log(RTMP_LOGWARNING, "Trying different position for server digest!");
            offalg ^= 1;
            getdig = digoff[offalg];
            getdh  = dhoff[offalg];
            digestPosServer = getdig(serversig, RTMP_SIG_SIZE);

            if (!VerifyDigest(digestPosServer, serversig, GenuineFMSKey, 36))
            {
                RTMP_Log(RTMP_LOGERROR, "Couldn't verify the server digest");	/* continuing anyway will probably fail */
                return FALSE;
            }
        }

        /* generate SWFVerification token (SHA256 HMAC hash of decompressed SWF, key are the last 32 bytes of the server handshake) */
        if (r->Link.SWFSize)
        {
            const char swfVerify[] = { 0x01, 0x01 };
            char *vend = r->Link.SWFVerificationResponse+sizeof(r->Link.SWFVerificationResponse);

            memcpy(r->Link.SWFVerificationResponse, swfVerify, 2);
            AMF_EncodeInt32(&r->Link.SWFVerificationResponse[2], vend, r->Link.SWFSize);
            AMF_EncodeInt32(&r->Link.SWFVerificationResponse[6], vend, r->Link.SWFSize);
            HMACsha256(r->Link.SWFHash, SHA256_DIGEST_LENGTH,
                       &serversig[RTMP_SIG_SIZE - SHA256_DIGEST_LENGTH],
                       SHA256_DIGEST_LENGTH,
                       (uint8_t *)&r->Link.SWFVerificationResponse[10]);
        }

        /* do Diffie-Hellmann Key exchange for encrypted RTMP */
        if (encrypted)
        {
            /* compute secret key */
            uint8_t secretKey[128] = { 0 };
            int len, dhposServer;

            dhposServer = getdh(serversig, RTMP_SIG_SIZE);
            RTMP_Log(RTMP_LOGDEBUG, "%s: Server DH public key offset: %d", __FUNCTION__,
                     dhposServer);
            len = DHComputeSharedSecretKey(r->Link.dh, &serversig[dhposServer],
                                           128, secretKey);
            if (len < 0)
            {
                RTMP_Log(RTMP_LOGDEBUG, "%s: Wrong secret key position!", __FUNCTION__);
                return FALSE;
            }

            RTMP_Log(RTMP_LOGDEBUG, "%s: Secret key: ", __FUNCTION__);
            RTMP_LogHex(RTMP_LOGDEBUG, secretKey, 128);

            InitRC4Encryption(secretKey,
                              (uint8_t *) & serversig[dhposServer],
                              (uint8_t *) & clientsig[dhposClient],
                              &keyIn, &keyOut);
        }


        reply = client2;
#ifdef _DEBUG
        memset(reply, 0xff, RTMP_SIG_SIZE);
#else
        ip = (int32_t *)reply;
        for (i = 0; i < RTMP_SIG_SIZE/4; i++)
            *ip++ = rand();
#endif
        /* calculate response now */
        signatureResp = reply+RTMP_SIG_SIZE-SHA256_DIGEST_LENGTH;

        HMACsha256(&serversig[digestPosServer], SHA256_DIGEST_LENGTH,
                   GenuineFPKey, sizeof(GenuineFPKey), digestResp);
        HMACsha256(reply, RTMP_SIG_SIZE - SHA256_DIGEST_LENGTH, digestResp,
                   SHA256_DIGEST_LENGTH, signatureResp);

        /* some info output */
        RTMP_Log(RTMP_LOGDEBUG,
                 "%s: Calculated digest key from secure key and server digest: ",
                 __FUNCTION__);
        RTMP_LogHex(RTMP_LOGDEBUG, digestResp, SHA256_DIGEST_LENGTH);

#ifdef FP10
        if (type == 8 )
        {
            uint8_t *dptr = digestResp;
            uint8_t *sig = signatureResp;
            /* encrypt signatureResp */
            for (i=0; i<SHA256_DIGEST_LENGTH; i+=8)
                rtmpe8_sig(sig+i, sig+i, dptr[i] % 15);
        }
        else if (type == 9)
        {
            uint8_t *dptr = digestResp;
            uint8_t *sig = signatureResp;
            /* encrypt signatureResp */
            for (i=0; i<SHA256_DIGEST_LENGTH; i+=8)
                rtmpe9_sig(sig+i, sig+i, dptr[i] % 15);
        }
#endif
        RTMP_Log(RTMP_LOGDEBUG, "%s: Client signature calculated:", __FUNCTION__);
        RTMP_LogHex(RTMP_LOGDEBUG, signatureResp, SHA256_DIGEST_LENGTH);
    }
    else
    {
        reply = serversig;
#if 0
        uptime = htonl(RTMP_GetTime());
        memcpy(reply+4, &uptime, 4);
#endif
    }

#ifdef _DEBUG
    RTMP_Log(RTMP_LOGDEBUG, "%s: Sending handshake response: ",
             __FUNCTION__);
    RTMP_LogHex(RTMP_LOGDEBUG, reply, RTMP_SIG_SIZE);
#endif
    if (!WriteN(r, (char *)reply, RTMP_SIG_SIZE))
        return FALSE;

    /* 2nd part of handshake */
    if (ReadN(r, (char *)serversig, RTMP_SIG_SIZE) != RTMP_SIG_SIZE)
        return FALSE;

#ifdef _DEBUG
    RTMP_Log(RTMP_LOGDEBUG, "%s: 2nd handshake: ", __FUNCTION__);
    RTMP_LogHex(RTMP_LOGDEBUG, serversig, RTMP_SIG_SIZE);
#endif

    if (FP9HandShake)
    {
        uint8_t signature[SHA256_DIGEST_LENGTH];
        uint8_t digest[SHA256_DIGEST_LENGTH];

        if (serversig[4] == 0 && serversig[5] == 0 && serversig[6] == 0
                && serversig[7] == 0)
        {
            RTMP_Log(RTMP_LOGDEBUG,
                     "%s: Wait, did the server just refuse signed authentication?",
                     __FUNCTION__);
        }
        RTMP_Log(RTMP_LOGDEBUG, "%s: Server sent signature:", __FUNCTION__);
        RTMP_LogHex(RTMP_LOGDEBUG, &serversig[RTMP_SIG_SIZE - SHA256_DIGEST_LENGTH],
                    SHA256_DIGEST_LENGTH);

        /* verify server response */
        HMACsha256(&clientsig[digestPosClient], SHA256_DIGEST_LENGTH,
                   GenuineFMSKey, sizeof(GenuineFMSKey), digest);
        HMACsha256(serversig, RTMP_SIG_SIZE - SHA256_DIGEST_LENGTH, digest,
                   SHA256_DIGEST_LENGTH, signature);

        /* show some information */
        RTMP_Log(RTMP_LOGDEBUG, "%s: Digest key: ", __FUNCTION__);
        RTMP_LogHex(RTMP_LOGDEBUG, digest, SHA256_DIGEST_LENGTH);

#ifdef FP10
        if (type == 8 )
        {
            uint8_t *dptr = digest;
            uint8_t *sig = signature;
            /* encrypt signature */
            for (i=0; i<SHA256_DIGEST_LENGTH; i+=8)
                rtmpe8_sig(sig+i, sig+i, dptr[i] % 15);
        }
        else if (type == 9)
        {
            uint8_t *dptr = digest;
            uint8_t *sig = signature;
            /* encrypt signatureResp */
            for (i=0; i<SHA256_DIGEST_LENGTH; i+=8)
                rtmpe9_sig(sig+i, sig+i, dptr[i] % 15);
        }
#endif
        RTMP_Log(RTMP_LOGDEBUG, "%s: Signature calculated:", __FUNCTION__);
        RTMP_LogHex(RTMP_LOGDEBUG, signature, SHA256_DIGEST_LENGTH);
        if (memcmp
                (signature, &serversig[RTMP_SIG_SIZE - SHA256_DIGEST_LENGTH],
                 SHA256_DIGEST_LENGTH) != 0)
        {
            RTMP_Log(RTMP_LOGWARNING, "%s: Server not genuine Adobe!", __FUNCTION__);
            return FALSE;
        }
        else
        {
            RTMP_Log(RTMP_LOGDEBUG, "%s: Genuine Adobe Flash Media Server", __FUNCTION__);
        }

        if (encrypted)
        {
            char buff[RTMP_SIG_SIZE];
            /* set keys for encryption from now on */
            r->Link.rc4keyIn = keyIn;
            r->Link.rc4keyOut = keyOut;


            /* update the keystreams */
            if (r->Link.rc4keyIn)
            {
                RC4_encrypt(r->Link.rc4keyIn, RTMP_SIG_SIZE, (uint8_t *) buff);
            }

            if (r->Link.rc4keyOut)
            {
                RC4_encrypt(r->Link.rc4keyOut, RTMP_SIG_SIZE, (uint8_t *) buff);
            }
        }
    }
    else
    {
        if (memcmp(serversig, clientsig, RTMP_SIG_SIZE) != 0)
        {
            RTMP_Log(RTMP_LOGWARNING, "%s: client signature does not match!",
                     __FUNCTION__);
        }
    }
    // TODO(mgoulet): Should this have a HMAC_finish here?

    RTMP_Log(RTMP_LOGDEBUG, "%s: Handshaking finished....", __FUNCTION__);
    return TRUE;
}
