#ifndef __RTMP_H__
#define __RTMP_H__
/*
 *      Copyright (C) 2005-2008 Team XBMC
 *      http://www.xbmc.org
 *      Copyright (C) 2008-2009 Andrej Stepanchuk
 *      Copyright (C) 2009-2010 Howard Chu
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

#if !defined(NO_CRYPTO) && !defined(CRYPTO)
#define CRYPTO
#else
#include <sys/types.h> //for off_t
#endif

#include <errno.h>
#include <stdint.h>

#ifdef _WIN32
#ifdef _MSC_VER
#pragma warning(disable:4996) //depricated warnings
#pragma warning(disable:4244) //64bit defensive mechanism, fixed the ones that mattered
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#define SOCKET int
#endif

#include "amf.h"

#ifdef CRYPTO
#if defined(USE_MBEDTLS)
#include <mbedtls/version.h>

#if MBEDTLS_VERSION_NUMBER >= 0x02040000
#include <mbedtls/net_sockets.h>
#else
#include <mbedtls/net.h>
#endif

#include <mbedtls/ssl.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>

#define my_dhm_P \
    "E4004C1F94182000103D883A448B3F80" \
    "2CE4B44A83301270002C20D0321CFD00" \
    "11CCEF784C26A400F43DFB901BCA7538" \
    "F2C6B176001CF5A0FD16D2C48B1D0C1C" \
    "F6AC8E1DA6BCC3B4E1F96B0564965300" \
    "FFA1D0B601EB2800F489AA512C4B248C" \
    "01F76949A60BB7F00A40B1EAB64BDD48" \
    "E8A700D60B7F1200FA8E77B0A979DABF"

#define my_dhm_G "4"

#define	SSL_SET_SESSION(S,resume,timeout,ctx)	mbedtls_ssl_set_session(S,ctx)

typedef struct tls_ctx
{
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_config conf;
    mbedtls_ssl_session ssn;
    mbedtls_x509_crt *cacert;
    mbedtls_net_context net;
} tls_ctx;

typedef tls_ctx *TLS_CTX;

#define TLS_client(ctx,s)	\
  s = malloc(sizeof(mbedtls_ssl_context));\
  mbedtls_ssl_init(s);\
  mbedtls_ssl_setup(s, &ctx->conf);\
	mbedtls_ssl_config_defaults(&ctx->conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);\
  mbedtls_ssl_conf_authmode(&ctx->conf, MBEDTLS_SSL_VERIFY_REQUIRED);\
	mbedtls_ssl_conf_rng(&ctx->conf, mbedtls_ctr_drbg_random, &ctx->ctr_drbg)

#define TLS_setfd(s,fd)	mbedtls_ssl_set_bio(s, fd, mbedtls_net_send, mbedtls_net_recv, NULL)
#define TLS_connect(s)	mbedtls_ssl_handshake(s)
#define TLS_accept(s)	mbedtls_ssl_handshake(s)
#define TLS_read(s,b,l)	mbedtls_ssl_read(s,(unsigned char *)b,l)
#define TLS_write(s,b,l)	mbedtls_ssl_write(s,(unsigned char *)b,l)
#define TLS_shutdown(s)	mbedtls_ssl_close_notify(s)
#define TLS_close(s)	mbedtls_ssl_free(s); free(s)

#elif defined(USE_POLARSSL)
#include <polarssl/version.h>
#include <polarssl/net.h>
#include <polarssl/ssl.h>
#include <polarssl/havege.h>
#if POLARSSL_VERSION_NUMBER < 0x01010000
#define havege_random	havege_rand
#endif
#if POLARSSL_VERSION_NUMBER >= 0x01020000
#define	SSL_SET_SESSION(S,resume,timeout,ctx)	ssl_set_session(S,ctx)
#else
#define	SSL_SET_SESSION(S,resume,timeout,ctx)	ssl_set_session(S,resume,timeout,ctx)
#endif
typedef struct tls_ctx
{
    havege_state hs;
    ssl_session ssn;
} tls_ctx;

#define TLS_CTX tls_ctx *
#define TLS_client(ctx,s)	s = malloc(sizeof(ssl_context)); ssl_init(s);\
	ssl_set_endpoint(s, SSL_IS_CLIENT); ssl_set_authmode(s, SSL_VERIFY_NONE);\
	ssl_set_rng(s, havege_random, &ctx->hs);\
	ssl_set_ciphersuites(s, ssl_default_ciphersuites);\
	SSL_SET_SESSION(s, 1, 600, &ctx->ssn)
#define TLS_setfd(s,fd)	ssl_set_bio(s, net_recv, &fd, net_send, &fd)
#define TLS_connect(s)	ssl_handshake(s)
#define TLS_accept(s)	ssl_handshake(s)
#define TLS_read(s,b,l)	ssl_read(s,(unsigned char *)b,l)
#define TLS_write(s,b,l)	ssl_write(s,(unsigned char *)b,l)
#define TLS_shutdown(s)	ssl_close_notify(s)
#define TLS_close(s)	ssl_free(s); free(s)


#elif defined(USE_GNUTLS)
#include <gnutls/gnutls.h>
typedef struct tls_ctx
{
    gnutls_certificate_credentials_t cred;
    gnutls_priority_t prios;
} tls_ctx;
#define TLS_CTX	tls_ctx *
#define TLS_client(ctx,s)	gnutls_init((gnutls_session_t *)(&s), GNUTLS_CLIENT); gnutls_priority_set(s, ctx->prios); gnutls_credentials_set(s, GNUTLS_CRD_CERTIFICATE, ctx->cred)
#define TLS_setfd(s,fd)	gnutls_transport_set_ptr(s, (gnutls_transport_ptr_t)(long)fd)
#define TLS_connect(s)	gnutls_handshake(s)
#define TLS_accept(s)	gnutls_handshake(s)
#define TLS_read(s,b,l)	gnutls_record_recv(s,b,l)
#define TLS_write(s,b,l)	gnutls_record_send(s,b,l)
#define TLS_shutdown(s)	gnutls_bye(s, GNUTLS_SHUT_RDWR)
#define TLS_close(s)	gnutls_deinit(s)

#else	/* USE_OPENSSL */
#define TLS_CTX	SSL_CTX *
#define TLS_client(ctx,s)	s = SSL_new(ctx)
#define TLS_setfd(s,fd)	SSL_set_fd(s,fd)
#define TLS_connect(s)	SSL_connect(s)
#define TLS_accept(s)	SSL_accept(s)
#define TLS_read(s,b,l)	SSL_read(s,b,l)
#define TLS_write(s,b,l)	SSL_write(s,b,l)
#define TLS_shutdown(s)	SSL_shutdown(s)
#define TLS_close(s)	SSL_free(s)

#endif
#elif defined(USE_ONLY_MD5)
#include "md5.h"
#include "cencode.h"
#define MD5_DIGEST_LENGTH 16
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#define RTMP_LIB_VERSION	0x020300	/* 2.3 */

#define RTMP_FEATURE_HTTP	0x01
#define RTMP_FEATURE_ENC	0x02
#define RTMP_FEATURE_SSL	0x04
#define RTMP_FEATURE_MFP	0x08	/* not yet supported */
#define RTMP_FEATURE_WRITE	0x10	/* publish, not play */
#define RTMP_FEATURE_HTTP2	0x20	/* server-side rtmpt */

#define RTMP_PROTOCOL_UNDEFINED	-1
#define RTMP_PROTOCOL_RTMP      0
#define RTMP_PROTOCOL_RTMPE     RTMP_FEATURE_ENC
#define RTMP_PROTOCOL_RTMPT     RTMP_FEATURE_HTTP
#define RTMP_PROTOCOL_RTMPS     RTMP_FEATURE_SSL
#define RTMP_PROTOCOL_RTMPTE    (RTMP_FEATURE_HTTP|RTMP_FEATURE_ENC)
#define RTMP_PROTOCOL_RTMPTS    (RTMP_FEATURE_HTTP|RTMP_FEATURE_SSL)
#define RTMP_PROTOCOL_RTMFP     RTMP_FEATURE_MFP

#define RTMP_DEFAULT_CHUNKSIZE	128

    /* needs to fit largest number of bytes recv() may return */
#define RTMP_BUFFER_CACHE_SIZE (16*1024)

#define	RTMP_CHANNELS	65600

    extern const char RTMPProtocolStringsLower[][7];
    extern const AVal RTMP_DefaultFlashVer;
    extern int RTMP_ctrlC;

    uint32_t RTMP_GetTime(void);

    /*      RTMP_PACKET_TYPE_...                0x00 */
#define RTMP_PACKET_TYPE_CHUNK_SIZE         0x01
    /*      RTMP_PACKET_TYPE_...                0x02 */
#define RTMP_PACKET_TYPE_BYTES_READ_REPORT  0x03
#define RTMP_PACKET_TYPE_CONTROL            0x04
#define RTMP_PACKET_TYPE_SERVER_BW          0x05
#define RTMP_PACKET_TYPE_CLIENT_BW          0x06
    /*      RTMP_PACKET_TYPE_...                0x07 */
#define RTMP_PACKET_TYPE_AUDIO              0x08
#define RTMP_PACKET_TYPE_VIDEO              0x09
    /*      RTMP_PACKET_TYPE_...                0x0A */
    /*      RTMP_PACKET_TYPE_...                0x0B */
    /*      RTMP_PACKET_TYPE_...                0x0C */
    /*      RTMP_PACKET_TYPE_...                0x0D */
    /*      RTMP_PACKET_TYPE_...                0x0E */
#define RTMP_PACKET_TYPE_FLEX_STREAM_SEND   0x0F
#define RTMP_PACKET_TYPE_FLEX_SHARED_OBJECT 0x10
#define RTMP_PACKET_TYPE_FLEX_MESSAGE       0x11
#define RTMP_PACKET_TYPE_INFO               0x12
#define RTMP_PACKET_TYPE_SHARED_OBJECT      0x13
#define RTMP_PACKET_TYPE_INVOKE             0x14
    /*      RTMP_PACKET_TYPE_...                0x15 */
#define RTMP_PACKET_TYPE_FLASH_VIDEO        0x16

#define RTMP_MAX_HEADER_SIZE 18

#define RTMP_PACKET_SIZE_LARGE    0
#define RTMP_PACKET_SIZE_MEDIUM   1
#define RTMP_PACKET_SIZE_SMALL    2
#define RTMP_PACKET_SIZE_MINIMUM  3

    typedef struct RTMPChunk
    {
        int c_headerSize;
        int c_chunkSize;
        char *c_chunk;
        char c_header[RTMP_MAX_HEADER_SIZE];
    } RTMPChunk;

    typedef struct RTMPPacket
    {
        uint8_t m_headerType;
        uint8_t m_packetType;
        uint8_t m_hasAbsTimestamp;	/* timestamp absolute or relative? */
        int m_nChannel;
        uint32_t m_nTimeStamp;	/* timestamp */
        int32_t m_nInfoField2;	/* last 4 bytes in a long header */
        uint32_t m_nBodySize;
        uint32_t m_nBytesRead;
        RTMPChunk *m_chunk;
        char *m_body;
    } RTMPPacket;

    typedef struct RTMPSockBuf
    {
        SOCKET sb_socket;
        int sb_size;		/* number of unprocessed bytes in buffer */
        char *sb_start;		/* pointer into sb_pBuffer of next byte to process */
        char sb_buf[RTMP_BUFFER_CACHE_SIZE];	/* data read from socket */
        int sb_timedout;
        void *sb_ssl;
    } RTMPSockBuf;

    void RTMPPacket_Reset(RTMPPacket *p);
    void RTMPPacket_Dump(RTMPPacket *p);
    int RTMPPacket_Alloc(RTMPPacket *p, uint32_t nSize);
    void RTMPPacket_Free(RTMPPacket *p);

#define RTMPPacket_IsReady(a)	((a)->m_nBytesRead == (a)->m_nBodySize)

    typedef struct RTMP_Stream {
        int id;
        AVal playpath;
    } RTMP_Stream;

    typedef struct RTMP_LNK
    {
#define RTMP_MAX_STREAMS 8
        RTMP_Stream streams[RTMP_MAX_STREAMS];
        int nStreams;
        int curStreamIdx;
        int playingStreams;

        AVal hostname;
        AVal sockshost;

        AVal tcUrl;
        AVal swfUrl;
        AVal pageUrl;
        AVal app;
        AVal auth;
        AVal flashVer;
        AVal subscribepath;
        AVal usherToken;
        AVal token;
        AVal pubUser;
        AVal pubPasswd;
        AMFObject extras;
        int edepth;

        int seekTime;
        int stopTime;

#define RTMP_LF_AUTH	0x0001	/* using auth param */
#define RTMP_LF_LIVE	0x0002	/* stream is live */
#define RTMP_LF_SWFV	0x0004	/* do SWF verification */
#define RTMP_LF_PLST	0x0008	/* send playlist before play */
#define RTMP_LF_BUFX	0x0010	/* toggle stream on BufferEmpty msg */
#define RTMP_LF_FTCU	0x0020	/* free tcUrl on close */
        int lFlags;

        int swfAge;

        int protocol;
        int timeout;		/* connection timeout in seconds */

#define RTMP_PUB_NAME   0x0001  /* send login to server */
#define RTMP_PUB_RESP   0x0002  /* send salted password hash */
#define RTMP_PUB_ALLOC  0x0004  /* allocated data for new tcUrl & app */
#define RTMP_PUB_CLEAN  0x0008  /* need to free allocated data for newer tcUrl & app at exit */
#define RTMP_PUB_CLATE  0x0010  /* late clean tcUrl & app at exit */
        int pFlags;

        unsigned short socksport;
        unsigned short port;

#ifdef CRYPTO
#define RTMP_SWF_HASHLEN	32
        void *dh;			/* for encryption */
        void *rc4keyIn;
        void *rc4keyOut;

        uint32_t SWFSize;
        uint8_t SWFHash[RTMP_SWF_HASHLEN];
        char SWFVerificationResponse[RTMP_SWF_HASHLEN+10];
#endif
    } RTMP_LNK;

    /* state for read() wrapper */
    typedef struct RTMP_READ
    {
        char *buf;
        char *bufpos;
        unsigned int buflen;
        uint32_t timestamp;
        uint8_t dataType;
        uint8_t flags;
#define RTMP_READ_HEADER	0x01
#define RTMP_READ_RESUME	0x02
#define RTMP_READ_NO_IGNORE	0x04
#define RTMP_READ_GOTKF		0x08
#define RTMP_READ_GOTFLVK	0x10
#define RTMP_READ_SEEKING	0x20
        int8_t status;
#define RTMP_READ_COMPLETE	-3
#define RTMP_READ_ERROR	-2
#define RTMP_READ_EOF	-1
#define RTMP_READ_IGNORE	0

        /* if bResume == TRUE */
        uint8_t initialFrameType;
        uint32_t nResumeTS;
        char *metaHeader;
        char *initialFrame;
        uint32_t nMetaHeaderSize;
        uint32_t nInitialFrameSize;
        uint32_t nIgnoredFrameCounter;
        uint32_t nIgnoredFlvFrameCounter;
    } RTMP_READ;

    typedef struct RTMP_METHOD
    {
        AVal name;
        int num;
    } RTMP_METHOD;

    typedef struct RTMP_BINDINFO
    {
        struct sockaddr_storage addr;
        int addrLen;
    } RTMP_BINDINFO;

    typedef int (*CUSTOMSEND)(RTMPSockBuf*, const char *, int, void*);

    typedef struct RTMP
    {
        int m_inChunkSize;
        int m_outChunkSize;
        int m_nBWCheckCounter;
        int m_nBytesIn;
        int m_nBytesInSent;
        int m_nBufferMS;
        int m_stream_id;		/* returned in _result from createStream */
        int m_mediaChannel;
        uint32_t m_mediaStamp;
        uint32_t m_pauseStamp;
        int m_pausing;
        int m_nServerBW;
        int m_nClientBW;
        uint8_t m_nClientBW2;
        uint8_t m_bPlaying;
        uint8_t m_bSendEncoding;
        uint8_t m_bSendCounter;

        uint8_t m_bUseNagle;
        uint8_t m_bCustomSend;
        void*   m_customSendParam;
        CUSTOMSEND m_customSendFunc;

        RTMP_BINDINFO m_bindIP;

        uint8_t m_bSendChunkSizeInfo;

        int m_numInvokes;
        int m_numCalls;
        RTMP_METHOD *m_methodCalls;	/* remote method calls queue */

        int m_channelsAllocatedIn;
        int m_channelsAllocatedOut;
        RTMPPacket **m_vecChannelsIn;
        RTMPPacket **m_vecChannelsOut;
        int *m_channelTimestamp;	/* abs timestamp of last packet */

        double m_fAudioCodecs;	/* audioCodecs for the connect packet */
        double m_fVideoCodecs;	/* videoCodecs for the connect packet */
        double m_fEncoding;		/* AMF0 or AMF3 */

        double m_fDuration;		/* duration of stream in seconds */

        int m_msgCounter;		/* RTMPT stuff */
        int m_polling;
        int m_resplen;
        int m_unackd;
        AVal m_clientID;

        RTMP_READ m_read;
        RTMPPacket m_write;
        RTMPSockBuf m_sb;
        RTMP_LNK Link;
        int connect_time_ms;
        int last_error_code;

#ifdef CRYPTO
        TLS_CTX RTMP_TLS_ctx;
#endif
    } RTMP;

    int RTMP_ParseURL(const char *url, int *protocol, AVal *host,
                      unsigned int *port, AVal *app);

    void RTMP_ParsePlaypath(AVal *in, AVal *out);
    void RTMP_SetBufferMS(RTMP *r, int size);
    void RTMP_UpdateBufferMS(RTMP *r);

    int RTMP_SetOpt(RTMP *r, const AVal *opt, AVal *arg);
    int RTMP_SetupURL(RTMP *r, char *url);
    int RTMP_AddStream(RTMP *r, const char *playpath);
    void RTMP_SetupStream(RTMP *r, int protocol,
                          AVal *hostname,
                          unsigned int port,
                          AVal *sockshost,
                          AVal *playpath,
                          AVal *tcUrl,
                          AVal *swfUrl,
                          AVal *pageUrl,
                          AVal *app,
                          AVal *auth,
                          AVal *swfSHA256Hash,
                          uint32_t swfSize,
                          AVal *flashVer,
                          AVal *subscribepath,
                          AVal *usherToken,
                          int dStart,
                          int dStop, int bLiveStream, long int timeout);

    int RTMP_Connect(RTMP *r, RTMPPacket *cp);
    struct sockaddr;
    int RTMP_Connect0(RTMP *r, struct sockaddr *svc, socklen_t addrlen);
    int RTMP_Connect1(RTMP *r, RTMPPacket *cp);

    int RTMP_ReadPacket(RTMP *r, RTMPPacket *packet);
    int RTMP_SendPacket(RTMP *r, RTMPPacket *packet, int queue);
    int RTMP_SendChunk(RTMP *r, RTMPChunk *chunk);
    int RTMP_IsConnected(RTMP *r);
    SOCKET RTMP_Socket(RTMP *r);
    int RTMP_IsTimedout(RTMP *r);
    double RTMP_GetDuration(RTMP *r);
    int RTMP_ToggleStream(RTMP *r);

    int RTMP_ConnectStream(RTMP *r, int seekTime);
    int RTMP_ReconnectStream(RTMP *r, int seekTime, int streamIdx);
    void RTMP_DeleteStream(RTMP *r, int streamIdx);
    int RTMP_GetNextMediaPacket(RTMP *r, RTMPPacket *packet);
    int RTMP_ClientPacket(RTMP *r, RTMPPacket *packet);

    void RTMP_Init(RTMP *r);
    void RTMP_Reset(RTMP *r);
    void RTMP_Close(RTMP *r);
    RTMP *RTMP_Alloc(void);
    void RTMP_TLS_Free(RTMP *r);
    void RTMP_Free(RTMP *r);
    void RTMP_EnableWrite(RTMP *r);

    int RTMP_LibVersion(void);
    void RTMP_UserInterrupt(void);	/* user typed Ctrl-C */

    int RTMP_SendCtrl(RTMP *r, short nType, unsigned int nObject,
                      unsigned int nTime);

    /* caller probably doesn't know current timestamp, should
     * just use RTMP_Pause instead
     */
    int RTMP_SendPause(RTMP *r, int DoPause, int dTime);
    int RTMP_Pause(RTMP *r, int DoPause);

    int RTMP_FindFirstMatchingProperty(AMFObject *obj, const AVal *name,
                                       AMFObjectProperty * p);

    int RTMPSockBuf_Fill(RTMPSockBuf *sb);
    int RTMPSockBuf_Send(RTMPSockBuf *sb, const char *buf, int len);
    int RTMPSockBuf_Close(RTMPSockBuf *sb);

    int RTMP_SendCreateStream(RTMP *r);
    int RTMP_SendSeek(RTMP *r, int dTime);
    int RTMP_SendServerBW(RTMP *r);
    int RTMP_SendClientBW(RTMP *r);
    void RTMP_DropRequest(RTMP *r, int i, int freeit);
    int RTMP_Read(RTMP *r, char *buf, int size);
    int RTMP_Write(RTMP *r, const char *buf, int size, int streamIdx);

#ifdef USE_HASHSWF
    /* hashswf.c */
    int RTMP_HashSWF(const char *url, unsigned int *size, unsigned char *hash,
                     int age);
#endif
#ifdef __cplusplus
};
#endif

#endif
