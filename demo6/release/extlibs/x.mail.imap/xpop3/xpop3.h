#ifndef XRT_EXTLIB_XPOP3_H
#define XRT_EXTLIB_XPOP3_H

#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <errno.h>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "../xmail_mime/xmail_mime.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if defined(_WIN32) || defined(_WIN64)
#define XPOP3_SOCKET_INVALID INVALID_SOCKET
#define XPOP3_CLOSESOCK closesocket
#else
#define XPOP3_SOCKET_INVALID (-1)
#define XPOP3_CLOSESOCK close
#endif

#define XPOP3_SECURE_AUTO      0
#define XPOP3_SECURE_NONE      1
#define XPOP3_SECURE_SSL       2
#define XPOP3_SECURE_STARTTLS  3

#define XPOP3_STAGE_NONE       0
#define XPOP3_STAGE_CONNECT    1
#define XPOP3_STAGE_TLS        2
#define XPOP3_STAGE_GREETING   3
#define XPOP3_STAGE_CAPA       4
#define XPOP3_STAGE_AUTH       5
#define XPOP3_STAGE_COMMAND    6
#define XPOP3_STAGE_RETR       7
#define XPOP3_STAGE_QUIT       8

#define XPOP3_CAP_STLS         0x0001u
#define XPOP3_CAP_UIDL         0x0002u
#define XPOP3_CAP_TOP          0x0004u
#define XPOP3_CAP_USER         0x0008u
#define XPOP3_CAP_SASL         0x0010u

#define XPOP3_AUTH_AUTO        0
#define XPOP3_AUTH_USERPASS    1
#define XPOP3_AUTH_PLAIN       2

#define XPOP3_TLS_VERSION_DEFAULT 0
#define XPOP3_TLS_VERSION_1_2     0x0303u
#define XPOP3_TLS_VERSION_1_3     0x0304u

typedef struct xpop3client_struct xpop3client;

typedef struct {
	const char* sHost;
	uint16 iPort;
	uint32 iTimeoutMs;
	int iSecureMode;
	bool bVerifyPeer;
	uint16 iTlsMaxVersion;
	const char* sCaFile;
	int iAuthMode;
	const char* sUser;
	const char* sPass;
} xpop3config;

typedef struct {
	uint32 iTimeoutMs;
	const char* sDebugName;
	bool bDeleteAfterRetr;
} xpop3asyncopts;

typedef struct {
	bool bSuccess;
	bool bUsedTLS;
	bool bUsedStartTLS;
	int iStage;
	int iServerCode;
	uint32 iCapabilities;
	char sError[256];
	char sLastReply[1024];
} xpop3result;

typedef struct xpop3summary_struct xpop3summary;

typedef struct {
	xpop3result tResult;
	str sRaw;
} xpop3fetchrawresult;

typedef struct {
	xpop3result tResult;
	xmailparsedmessage tMessage;
} xpop3fetchmimeresult;

typedef struct {
	xpop3result tResult;
	xpop3summary* arrItems;
	size_t iCount;
} xpop3summaryresult;

typedef struct {
	uint32 iCount;
	uint64 iSize;
} xpop3stat;

typedef struct {
	uint32 iNumber;
	uint64 iSize;
} xpop3listitem;

typedef struct {
	uint32 iNumber;
	char sUid[256];
} xpop3uidlitem;

typedef struct xpop3summary_struct {
	uint32 iNumber;
	uint64 iSize;
	char sUid[256];
	char sSubject[512];
	char sFrom[512];
	char sDate[128];
} xpop3summary;

typedef struct {
	char* sData;
	size_t iLen;
	size_t iCap;
} xpop3buf;

struct xpop3client_struct {
	xsocket hSock;
	xtlssession* pTls;
	xnetengine* pEngine;
	xnetstream* pStream;
	xmutex pLock;
	xcond pCond;
	bool bOpen;
	bool bClosed;
	bool bRecvOverflow;
	int iCloseReason;
	int iLastSysErr;
	uint32 iTimeoutMs;
	xpop3buf tRecv;
};

typedef struct {
	xpop3config tCfg;
	xpop3asyncopts tOpts;
	uint32 iNumber;
	uint32 iTopLineCount;
	bool bParseMime;
	bool bUseTop;
	char* sHost;
	char* sUser;
	char* sPass;
	char* sCaFile;
	char* sDebugName;
} xpop3fetchtaskctx;

static bool xpop3_buf_reserve(xpop3buf* pBuf, size_t iNeed);
static bool xpop3_buf_append_n(xpop3buf* pBuf, const char* sText, size_t iLen);
static bool xpop3_buf_append(xpop3buf* pBuf, const char* sText);
static void xpop3_buf_free(xpop3buf* pBuf);

static const char* xpop3_str_or_empty(const char* sText)
{
	return sText ? sText : "";
}

static void xrtPop3ConfigInit(xpop3config* pCfg)
{
	if ( pCfg == NULL ) {
		return;
	}
	memset(pCfg, 0, sizeof(*pCfg));
	pCfg->iPort = 995;
	pCfg->iTimeoutMs = 15000u;
	pCfg->iSecureMode = XPOP3_SECURE_AUTO;
	pCfg->bVerifyPeer = TRUE;
	pCfg->iTlsMaxVersion = XPOP3_TLS_VERSION_DEFAULT;
	pCfg->iAuthMode = XPOP3_AUTH_AUTO;
}

static void xrtPop3ResultInit(xpop3result* pRet)
{
	if ( pRet == NULL ) {
		return;
	}
	memset(pRet, 0, sizeof(*pRet));
}

static void xrtPop3AsyncOptsInit(xpop3asyncopts* pOpts)
{
	if ( pOpts == NULL ) {
		return;
	}
	memset(pOpts, 0, sizeof(*pOpts));
}

static void xrtPop3ResultFree(xpop3result* pRet)
{
	if ( pRet != NULL ) {
		xrtFree(pRet);
	}
}

static void xrtPop3FetchRawResultFree(xpop3fetchrawresult* pResult)
{
	if ( pResult == NULL ) {
		return;
	}
	if ( pResult->sRaw != NULL ) {
		xrtFree(pResult->sRaw);
	}
	xrtFree(pResult);
}

static void xrtPop3FetchMimeResultFree(xpop3fetchmimeresult* pResult)
{
	if ( pResult == NULL ) {
		return;
	}
	xmailMimeParsedMessageFree(&pResult->tMessage);
	xrtFree(pResult);
}

static void xrtPop3SummaryResultFree(xpop3summaryresult* pResult)
{
	if ( pResult == NULL ) {
		return;
	}
	if ( pResult->arrItems != NULL ) {
		xrtFree(pResult->arrItems);
	}
	xrtFree(pResult);
}

static char* xpop3_dup_text(const char* sText)
{
	size_t iLen;
	char* sCopy;

	if ( sText == NULL ) {
		return NULL;
	}
	iLen = strlen(sText);
	sCopy = (char*)xrtMalloc(iLen + 1u);
	if ( sCopy == NULL ) {
		return NULL;
	}
	memcpy(sCopy, sText, iLen + 1u);
	return sCopy;
}

static void xpop3_result_stage(xpop3result* pRet, int iStage)
{
	if ( pRet != NULL ) {
		pRet->iStage = iStage;
	}
}

static void xpop3_result_error(xpop3result* pRet, const char* sError)
{
	if ( pRet == NULL ) {
		return;
	}
	pRet->bSuccess = FALSE;
	snprintf(pRet->sError, sizeof(pRet->sError), "%s", xpop3_str_or_empty(sError));
}

static int xpop3_socket_last_error(void)
{
#if defined(_WIN32) || defined(_WIN64)
	return WSAGetLastError();
#else
	return errno;
#endif
}

static void xpop3_result_sys_error(xpop3result* pRet, const char* sError)
{
	if ( pRet == NULL ) {
		return;
	}
	pRet->bSuccess = FALSE;
	snprintf(pRet->sError, sizeof(pRet->sError), "%s (sys=%d)", xpop3_str_or_empty(sError), xpop3_socket_last_error());
}

static void xpop3_result_tls_error(xpop3result* pRet, const char* sError, xnet_result iResult)
{
	if ( pRet == NULL ) {
		return;
	}
	pRet->bSuccess = FALSE;
	snprintf(pRet->sError, sizeof(pRet->sError), "%s (tls=%d)", xpop3_str_or_empty(sError), (int)iResult);
}

static void xpop3_result_errorf(xpop3result* pRet, const char* sFormat, ...)
{
	va_list ap;
	char sBuf[256];

	if ( pRet == NULL || sFormat == NULL ) {
		return;
	}
	va_start(ap, sFormat);
	vsnprintf(sBuf, sizeof(sBuf), sFormat, ap);
	va_end(ap);
	xpop3_result_error(pRet, sBuf);
}

static bool xpop3_buf_reserve(xpop3buf* pBuf, size_t iNeed)
{
	size_t iCap;
	char* sNew;

	if ( pBuf == NULL ) {
		return FALSE;
	}
	if ( pBuf->iLen + iNeed + 1u <= pBuf->iCap ) {
		return TRUE;
	}
	iCap = pBuf->iCap ? pBuf->iCap : 256u;
	while ( iCap < pBuf->iLen + iNeed + 1u ) {
		iCap *= 2u;
	}
	sNew = (char*)xrtRealloc(pBuf->sData, iCap);
	if ( sNew == NULL ) {
		return FALSE;
	}
	pBuf->sData = sNew;
	pBuf->iCap = iCap;
	return TRUE;
}

static bool xpop3_buf_append_n(xpop3buf* pBuf, const char* sText, size_t iLen)
{
	if ( pBuf == NULL || sText == NULL ) {
		return FALSE;
	}
	if ( !xpop3_buf_reserve(pBuf, iLen) ) {
		return FALSE;
	}
	memcpy(pBuf->sData + pBuf->iLen, sText, iLen);
	pBuf->iLen += iLen;
	pBuf->sData[pBuf->iLen] = '\0';
	return TRUE;
}

static bool xpop3_buf_append(xpop3buf* pBuf, const char* sText)
{
	return xpop3_buf_append_n(pBuf, xpop3_str_or_empty(sText), strlen(xpop3_str_or_empty(sText)));
}

static void xpop3_buf_free(xpop3buf* pBuf)
{
	if ( pBuf != NULL && pBuf->sData != NULL ) {
		xrtFree(pBuf->sData);
		pBuf->sData = NULL;
		pBuf->iLen = 0u;
		pBuf->iCap = 0u;
	}
}

static bool xpop3_response_is_ok(const char* sLine)
{
	return sLine != NULL
		&& sLine[0] == '+'
		&& tolower((unsigned char)sLine[1]) == 'o'
		&& tolower((unsigned char)sLine[2]) == 'k'
		&& (sLine[3] == '\0' || isspace((unsigned char)sLine[3]));
}

static bool xpop3_response_is_err(const char* sLine)
{
	return sLine != NULL
		&& sLine[0] == '-'
		&& tolower((unsigned char)sLine[1]) == 'e'
		&& tolower((unsigned char)sLine[2]) == 'r'
		&& tolower((unsigned char)sLine[3]) == 'r'
		&& (sLine[4] == '\0' || isspace((unsigned char)sLine[4]));
}

static const char* xpop3_response_text(const char* sLine)
{
	const char* p;

	if ( xpop3_response_is_ok(sLine) ) {
		p = sLine + 3;
	} else if ( xpop3_response_is_err(sLine) ) {
		p = sLine + 4;
	} else {
		return "";
	}
	while ( *p && isspace((unsigned char)*p) ) {
		p++;
	}
	return p;
}

static bool xpop3_parse_uint64_token(const char** ppText, uint64* pValue)
{
	const char* p;
	uint64 iValue = 0u;
	bool bAny = FALSE;

	if ( ppText == NULL || *ppText == NULL || pValue == NULL ) {
		return FALSE;
	}
	p = *ppText;
	while ( *p && isspace((unsigned char)*p) ) {
		p++;
	}
	while ( *p >= '0' && *p <= '9' ) {
		iValue = (iValue * 10u) + (uint64)(*p - '0');
		p++;
		bAny = TRUE;
	}
	if ( !bAny ) {
		return FALSE;
	}
	*ppText = p;
	*pValue = iValue;
	return TRUE;
}

static bool xpop3_parse_stat_line(const char* sText, xpop3stat* pStat)
{
	const char* p = sText;
	uint64 iCount;
	uint64 iSize;

	if ( pStat == NULL || !xpop3_response_is_ok(sText) ) {
		return FALSE;
	}
	p = xpop3_response_text(sText);
	if ( !xpop3_parse_uint64_token(&p, &iCount) ) {
		return FALSE;
	}
	while ( *p && isspace((unsigned char)*p) ) {
		p++;
	}
	if ( !xpop3_parse_uint64_token(&p, &iSize) ) {
		return FALSE;
	}
	pStat->iCount = (uint32)iCount;
	pStat->iSize = (uint64)iSize;
	return TRUE;
}

static bool xpop3_parse_list_line(const char* sText, xpop3listitem* pItem)
{
	const char* p = sText;
	uint64 iNumber;
	uint64 iSize;

	if ( pItem == NULL || sText == NULL ) {
		return FALSE;
	}
	while ( *p && isspace((unsigned char)*p) ) {
		p++;
	}
	if ( !xpop3_parse_uint64_token(&p, &iNumber) ) {
		return FALSE;
	}
	while ( *p && isspace((unsigned char)*p) ) {
		p++;
	}
	if ( !xpop3_parse_uint64_token(&p, &iSize) ) {
		return FALSE;
	}
	pItem->iNumber = (uint32)iNumber;
	pItem->iSize = (uint64)iSize;
	return TRUE;
}

static bool xpop3_parse_uidl_line(const char* sText, xpop3uidlitem* pItem)
{
	const char* p = sText;
	uint64 iNumber;
	size_t iLen;

	if ( pItem == NULL || sText == NULL ) {
		return FALSE;
	}
	while ( *p && isspace((unsigned char)*p) ) {
		p++;
	}
	if ( !xpop3_parse_uint64_token(&p, &iNumber) ) {
		return FALSE;
	}
	while ( *p && isspace((unsigned char)*p) ) {
		p++;
	}
	if ( *p == '\0' ) {
		return FALSE;
	}
	iLen = strlen(p);
	if ( iLen >= sizeof(pItem->sUid) ) {
		iLen = sizeof(pItem->sUid) - 1u;
	}
	pItem->iNumber = (uint32)iNumber;
	memcpy(pItem->sUid, p, iLen);
	pItem->sUid[iLen] = '\0';
	return TRUE;
}

static size_t xpop3_count_lines(const char* sText)
{
	const char* p = xpop3_str_or_empty(sText);
	size_t iCount = 0u;

	while ( *p ) {
		const char* sLineEnd = strstr(p, "\r\n");
		size_t iLineLen = sLineEnd ? (size_t)(sLineEnd - p) : strlen(p);
		if ( iLineLen > 0u ) {
			iCount++;
		}
		if ( sLineEnd == NULL ) {
			break;
		}
		p = sLineEnd + 2;
	}
	return iCount;
}

static bool xpop3_parse_list_block(const char* sText, xpop3listitem** ppItems, size_t* pCount)
{
	xpop3listitem* arrItems;
	const char* p;
	size_t iCap;
	size_t iCount = 0u;

	if ( ppItems == NULL || pCount == NULL ) {
		return FALSE;
	}
	*ppItems = NULL;
	*pCount = 0u;
	iCap = xpop3_count_lines(sText);
	if ( iCap == 0u ) {
		return TRUE;
	}
	arrItems = (xpop3listitem*)xrtMalloc(sizeof(xpop3listitem) * iCap);
	if ( arrItems == NULL ) {
		return FALSE;
	}
	p = xpop3_str_or_empty(sText);
	while ( *p ) {
		char sLine[128];
		const char* sLineEnd = strstr(p, "\r\n");
		size_t iLineLen = sLineEnd ? (size_t)(sLineEnd - p) : strlen(p);
		if ( iLineLen >= sizeof(sLine) ) {
			iLineLen = sizeof(sLine) - 1u;
		}
		memcpy(sLine, p, iLineLen);
		sLine[iLineLen] = '\0';
		if ( sLine[0] != '\0' && !xpop3_parse_list_line(sLine, &arrItems[iCount++]) ) {
			xrtFree(arrItems);
			return FALSE;
		}
		if ( sLineEnd == NULL ) {
			break;
		}
		p = sLineEnd + 2;
	}
	*ppItems = arrItems;
	*pCount = iCount;
	return TRUE;
}

static bool xpop3_parse_uidl_block(const char* sText, xpop3uidlitem** ppItems, size_t* pCount)
{
	xpop3uidlitem* arrItems;
	const char* p;
	size_t iCap;
	size_t iCount = 0u;

	if ( ppItems == NULL || pCount == NULL ) {
		return FALSE;
	}
	*ppItems = NULL;
	*pCount = 0u;
	iCap = xpop3_count_lines(sText);
	if ( iCap == 0u ) {
		return TRUE;
	}
	arrItems = (xpop3uidlitem*)xrtMalloc(sizeof(xpop3uidlitem) * iCap);
	if ( arrItems == NULL ) {
		return FALSE;
	}
	p = xpop3_str_or_empty(sText);
	while ( *p ) {
		char sLine[512];
		const char* sLineEnd = strstr(p, "\r\n");
		size_t iLineLen = sLineEnd ? (size_t)(sLineEnd - p) : strlen(p);
		if ( iLineLen >= sizeof(sLine) ) {
			iLineLen = sizeof(sLine) - 1u;
		}
		memcpy(sLine, p, iLineLen);
		sLine[iLineLen] = '\0';
		if ( sLine[0] != '\0' && !xpop3_parse_uidl_line(sLine, &arrItems[iCount++]) ) {
			xrtFree(arrItems);
			return FALSE;
		}
		if ( sLineEnd == NULL ) {
			break;
		}
		p = sLineEnd + 2;
	}
	*ppItems = arrItems;
	*pCount = iCount;
	return TRUE;
}

static bool xpop3_starts_with_i(const char* sText, const char* sPrefix)
{
	size_t i;

	if ( sText == NULL || sPrefix == NULL ) {
		return FALSE;
	}
	for ( i = 0u; sPrefix[i] != '\0'; i++ ) {
		if ( sText[i] == '\0' || tolower((unsigned char)sText[i]) != tolower((unsigned char)sPrefix[i]) ) {
			return FALSE;
		}
	}
	return TRUE;
}

static void xpop3_parse_capa_line(const char* sLine, uint32* pCaps)
{
	if ( sLine == NULL || pCaps == NULL ) {
		return;
	}
	if ( xpop3_starts_with_i(sLine, "STLS") ) {
		*pCaps |= XPOP3_CAP_STLS;
	} else if ( xpop3_starts_with_i(sLine, "UIDL") ) {
		*pCaps |= XPOP3_CAP_UIDL;
	} else if ( xpop3_starts_with_i(sLine, "TOP") ) {
		*pCaps |= XPOP3_CAP_TOP;
	} else if ( xpop3_starts_with_i(sLine, "USER") ) {
		*pCaps |= XPOP3_CAP_USER;
	} else if ( xpop3_starts_with_i(sLine, "SASL") ) {
		*pCaps |= XPOP3_CAP_SASL;
	}
}

static uint32 xpop3_parse_capa_block(const char* sText)
{
	const char* p = xpop3_str_or_empty(sText);
	uint32 iCaps = 0u;

	while ( *p ) {
		char sLine[512];
		const char* sLineEnd = strstr(p, "\r\n");
		size_t iLineLen = sLineEnd ? (size_t)(sLineEnd - p) : strlen(p);
		if ( iLineLen >= sizeof(sLine) ) {
			iLineLen = sizeof(sLine) - 1u;
		}
		memcpy(sLine, p, iLineLen);
		sLine[iLineLen] = '\0';
		xpop3_parse_capa_line(sLine, &iCaps);
		if ( sLineEnd == NULL ) {
			break;
		}
		p = sLineEnd + 2;
	}
	return iCaps;
}

static str xpop3_unstuff_multiline(const char* sText)
{
	xpop3buf tBuf;
	const char* p;

	memset(&tBuf, 0, sizeof(tBuf));
	p = xpop3_str_or_empty(sText);
	while ( *p ) {
		const char* sLineEnd = strstr(p, "\r\n");
		size_t iLineLen = sLineEnd ? (size_t)(sLineEnd - p) : strlen(p);
		if ( iLineLen == 1u && p[0] == '.' ) {
			break;
		}
		if ( iLineLen > 0u && p[0] == '.' ) {
			p++;
			iLineLen--;
		}
		if ( !xpop3_buf_append_n(&tBuf, p, iLineLen) || !xpop3_buf_append(&tBuf, "\r\n") ) {
			xpop3_buf_free(&tBuf);
			return NULL;
		}
		if ( sLineEnd == NULL ) {
			break;
		}
		p = sLineEnd + 2;
	}
	if ( tBuf.sData == NULL ) {
		return xrtCopyStr((str)"", 0);
	}
	return (str)tBuf.sData;
}

static uint64 xpop3_now_ms(void)
{
	double fNow = xrtTimer();
	if ( fNow <= 0.0 ) {
		return 0;
	}
	return (uint64)(fNow * 1000.0);
}

static int xpop3_secure_mode_auto(const xpop3config* pCfg)
{
	if ( pCfg == NULL ) {
		return XPOP3_SECURE_SSL;
	}
	if ( pCfg->iSecureMode != XPOP3_SECURE_AUTO ) {
		return pCfg->iSecureMode;
	}
	if ( pCfg->iPort == 995 ) {
		return XPOP3_SECURE_SSL;
	}
	if ( pCfg->iPort == 110 ) {
		return XPOP3_SECURE_NONE;
	}
	return XPOP3_SECURE_NONE;
}

static bool xpop3_wait_recv_state(xpop3client* pClient, uint64 iDeadlineMs)
{
	for (;;) {
		uint32 iRemain;
		int iWaitRet;

		if ( pClient->tRecv.iLen > 0u || pClient->bClosed || pClient->iLastSysErr != 0 || pClient->bRecvOverflow ) {
			return TRUE;
		}
		if ( xpop3_now_ms() >= iDeadlineMs ) {
			return FALSE;
		}
		iRemain = (uint32)(iDeadlineMs - xpop3_now_ms());
		if ( iRemain == 0u ) {
			return FALSE;
		}
		iWaitRet = xrtCondWaitTimeout(pClient->pCond, pClient->pLock, iRemain);
		if ( iWaitRet == XRT_WAIT_TIMEOUT || iWaitRet == XRT_WAIT_ERROR ) {
			return FALSE;
		}
	}
}

static bool xpop3_socket_send_all(xpop3client* pClient, const void* pData, size_t iLen, xpop3result* pRet)
{
	const char* p = (const char*)pData;
	size_t iLeft = iLen;

	if ( pClient == NULL || pClient->hSock == XPOP3_SOCKET_INVALID || pClient->hSock == 0 ) {
		xpop3_result_error(pRet, "POP3 client is not connected");
		return FALSE;
	}
	while ( iLeft > 0u ) {
		int iSent = send(pClient->hSock, p, (int)iLeft, 0);
		if ( iSent <= 0 ) {
			xpop3_result_error(pRet, "POP3 send failed");
			return FALSE;
		}
		p += iSent;
		iLeft -= (size_t)iSent;
	}
	return TRUE;
}

static bool xpop3_tls_flush(xpop3client* pClient, xpop3result* pRet)
{
	char aBuf[4096];
	size_t iRead = 0u;

	if ( pClient == NULL || pClient->pTls == NULL ) {
		return TRUE;
	}
	while ( xrtNetTlsSessionPendingCipher(pClient->pTls) > 0u ) {
		if ( xrtNetTlsSessionPeekCipher(pClient->pTls, aBuf, sizeof(aBuf), &iRead) != XRT_NET_OK || iRead == 0u ) {
			xpop3_result_error(pRet, "POP3 TLS cipher peek failed");
			return FALSE;
		}
		if ( !xpop3_socket_send_all(pClient, aBuf, iRead, pRet) ) {
			return FALSE;
		}
		xrtNetTlsSessionConsumeCipher(pClient->pTls, iRead);
	}
	return TRUE;
}

static bool xpop3_tls_handshake(xpop3client* pClient, const xpop3config* pCfg, xpop3result* pRet)
{
	xtlsconfig tTlsCfg;
	char aBuf[4096];

	if ( pClient == NULL || pCfg == NULL ) {
		xpop3_result_error(pRet, "POP3 TLS config invalid");
		return FALSE;
	}
	memset(&tTlsCfg, 0, sizeof(tTlsCfg));
	tTlsCfg.sHostName = pCfg->sHost;
	tTlsCfg.bVerifyPeer = pCfg->bVerifyPeer;
	tTlsCfg.iMaxVersion = pCfg->iTlsMaxVersion;
	tTlsCfg.sCaFile = pCfg->sCaFile;
	pClient->pTls = xrtNetTlsSessionCreate(&tTlsCfg, FALSE);
	if ( pClient->pTls == NULL ) {
		xpop3_result_error(pRet, "POP3 TLS session create failed");
		return FALSE;
	}
	while ( !xrtNetTlsSessionIsReady(pClient->pTls) ) {
		int iRet;
		xnet_result iRes = xrtNetTlsSessionDriveHandshake(pClient->pTls);
		if ( iRes != XRT_NET_OK && iRes != XRT_NET_AGAIN ) {
			xpop3_result_tls_error(pRet, "POP3 TLS handshake failed", iRes);
			return FALSE;
		}
		if ( !xpop3_tls_flush(pClient, pRet) ) {
			return FALSE;
		}
		if ( xrtNetTlsSessionIsReady(pClient->pTls) ) {
			break;
		}
		if ( xrtNetTlsSessionPendingRecv(pClient->pTls) > 0u ) {
			continue;
		}
		iRet = recv(pClient->hSock, aBuf, (int)sizeof(aBuf), 0);
		if ( iRet <= 0 ) {
			xpop3_result_sys_error(pRet, "POP3 TLS handshake recv failed");
			return FALSE;
		}
		iRes = xrtNetTlsSessionFeedCipher(pClient->pTls, aBuf, (size_t)iRet);
		if ( iRes != XRT_NET_OK ) {
			xpop3_result_tls_error(pRet, "POP3 TLS handshake feed failed", iRes);
			return FALSE;
		}
	}
	if ( pRet != NULL ) {
		pRet->bUsedTLS = TRUE;
	}
	return TRUE;
}

static bool xpop3_recv_bytes(xpop3client* pClient, void* pBuf, size_t iCap, size_t* pRead, xpop3result* pRet)
{
	if ( pRead != NULL ) {
		*pRead = 0u;
	}
	if ( pClient == NULL || pBuf == NULL || iCap == 0u || pClient->hSock == XPOP3_SOCKET_INVALID || pClient->hSock == 0 ) {
		xpop3_result_error(pRet, "POP3 client is not connected");
		return FALSE;
	}
	if ( pClient->pTls == NULL ) {
		int iRet = recv(pClient->hSock, (char*)pBuf, (int)iCap, 0);
		if ( iRet <= 0 ) {
			xpop3_result_sys_error(pRet, "POP3 recv failed");
			return FALSE;
		}
		if ( pRead != NULL ) {
			*pRead = (size_t)iRet;
		}
		return TRUE;
	}
	for (;;) {
		size_t iPlain = 0u;
		xnet_result iRes = xrtNetTlsSessionReadPlain(pClient->pTls, pBuf, iCap, &iPlain);
		if ( iRes == XRT_NET_OK && iPlain > 0u ) {
			if ( pRead != NULL ) {
				*pRead = iPlain;
			}
			return TRUE;
		}
		if ( iRes == XRT_NET_AGAIN ) {
			char aCipher[4096];
			int iRet = recv(pClient->hSock, aCipher, (int)sizeof(aCipher), 0);
			xnet_result iFeed;
			if ( iRet <= 0 ) {
				xpop3_result_sys_error(pRet, "POP3 TLS recv failed");
				return FALSE;
			}
			iFeed = xrtNetTlsSessionFeedCipher(pClient->pTls, aCipher, (size_t)iRet);
			if ( iFeed != XRT_NET_OK ) {
				xpop3_result_tls_error(pRet, "POP3 TLS feed failed", iFeed);
				return FALSE;
			}
			if ( !xpop3_tls_flush(pClient, pRet) ) {
				return FALSE;
			}
			continue;
		}
		if ( iRes != XRT_NET_OK || iPlain == 0u ) {
			xpop3_result_tls_error(pRet, "POP3 TLS read failed", iRes);
			return FALSE;
		}
	}
}

static bool xpop3_client_connect(xpop3client* pClient, const xpop3config* pCfg, xpop3result* pRet)
{
	struct addrinfo tHints;
	struct addrinfo* pRes = NULL;
	struct addrinfo* pIt;
	char sPort[16];

	if ( pClient == NULL || pCfg == NULL || pCfg->sHost == NULL || pCfg->sHost[0] == '\0' ) {
		xpop3_result_error(pRet, "POP3 host is empty");
		return FALSE;
	}
	(void)xrtInit();
	pClient->iTimeoutMs = pCfg->iTimeoutMs ? pCfg->iTimeoutMs : 15000u;
	pClient->hSock = XPOP3_SOCKET_INVALID;

	memset(&tHints, 0, sizeof(tHints));
	tHints.ai_family = AF_UNSPEC;
	tHints.ai_socktype = SOCK_STREAM;
	snprintf(sPort, sizeof(sPort), "%u", (unsigned)pCfg->iPort);
	if ( getaddrinfo(pCfg->sHost, sPort, &tHints, &pRes) != 0 || pRes == NULL ) {
		xpop3_result_error(pRet, "POP3 host resolve failed");
		return FALSE;
	}
	{
		int iPass;
		for ( iPass = 0; iPass < 2 && pClient->hSock == XPOP3_SOCKET_INVALID; ++iPass ) {
			int iFamilyWant = (iPass == 0) ? AF_INET : AF_INET6;
			for ( pIt = pRes; pIt != NULL; pIt = pIt->ai_next ) {
				xsocket hSock;
				if ( pIt->ai_family != iFamilyWant ) {
					continue;
				}
				hSock = socket(pIt->ai_family, pIt->ai_socktype, pIt->ai_protocol);
				if ( hSock == XPOP3_SOCKET_INVALID ) {
					continue;
				}
				if ( connect(hSock, pIt->ai_addr, (int)pIt->ai_addrlen) == 0 ) {
					pClient->hSock = hSock;
					break;
				}
				XPOP3_CLOSESOCK(hSock);
			}
		}
	}
	freeaddrinfo(pRes);
	if ( pClient->hSock == XPOP3_SOCKET_INVALID ) {
		xpop3_result_error(pRet, "POP3 stream connect failed");
		return FALSE;
	}
#if defined(_WIN32) || defined(_WIN64)
	{
		DWORD iSockTimeout = (DWORD)pClient->iTimeoutMs;
		setsockopt(pClient->hSock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&iSockTimeout, sizeof(iSockTimeout));
		setsockopt(pClient->hSock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&iSockTimeout, sizeof(iSockTimeout));
	}
#else
	{
		struct timeval tSockTimeout;
		tSockTimeout.tv_sec = (time_t)(pClient->iTimeoutMs / 1000u);
		tSockTimeout.tv_usec = (suseconds_t)((pClient->iTimeoutMs % 1000u) * 1000u);
		setsockopt(pClient->hSock, SOL_SOCKET, SO_RCVTIMEO, &tSockTimeout, sizeof(tSockTimeout));
		setsockopt(pClient->hSock, SOL_SOCKET, SO_SNDTIMEO, &tSockTimeout, sizeof(tSockTimeout));
	}
#endif
	if ( xpop3_secure_mode_auto(pCfg) == XPOP3_SECURE_SSL ) {
		xpop3_result_stage(pRet, XPOP3_STAGE_TLS);
		if ( !xpop3_tls_handshake(pClient, pCfg, pRet) ) {
			return FALSE;
		}
		if ( pRet != NULL ) {
			pRet->bUsedTLS = TRUE;
		}
	}
	return TRUE;
}

static void xrtPop3Close(xpop3client* pClient)
{
	if ( pClient == NULL ) {
		return;
	}
	if ( pClient->pTls != NULL ) {
		xrtNetTlsSessionDestroy(pClient->pTls);
		pClient->pTls = NULL;
	}
	if ( pClient->hSock != XPOP3_SOCKET_INVALID && pClient->hSock != 0 ) {
		XPOP3_CLOSESOCK(pClient->hSock);
		pClient->hSock = XPOP3_SOCKET_INVALID;
	}
	if ( pClient->pStream != NULL ) {
		xrtNetStreamClose(pClient->pStream, 0);
		(void)xrtNetStreamWaitTimeoutEx(pClient->pStream, XNET_STREAM_WAIT_CLOSE, pClient->iTimeoutMs ? pClient->iTimeoutMs : 1000u);
		if ( xrtGetError() ) {
			xrtClearError();
		}
		xrtNetStreamDestroy(pClient->pStream);
		if ( xrtGetError() ) {
			xrtClearError();
		}
		pClient->pStream = NULL;
	}
	if ( pClient->pEngine != NULL ) {
		xrtNetEngineDestroy(pClient->pEngine);
		pClient->pEngine = NULL;
	}
	xpop3_buf_free(&pClient->tRecv);
	if ( pClient->pCond != NULL ) {
		xrtCondDestroy(pClient->pCond);
		pClient->pCond = NULL;
	}
	if ( pClient->pLock != NULL ) {
		xrtMutexDestroy(pClient->pLock);
		pClient->pLock = NULL;
	}
	xrtFree(pClient);
}

static bool xpop3_send_line(xpop3client* pClient, const char* sLine, xpop3result* pRet)
{
	xpop3buf tBuf;
	xnet_result iRet;

	memset(&tBuf, 0, sizeof(tBuf));
	if ( pClient == NULL || (pClient->pStream == NULL && (pClient->hSock == XPOP3_SOCKET_INVALID || pClient->hSock == 0)) ) {
		xpop3_result_error(pRet, "POP3 client is not connected");
		return FALSE;
	}
	if ( !xpop3_buf_append(&tBuf, xpop3_str_or_empty(sLine)) || !xpop3_buf_append(&tBuf, "\r\n") ) {
		xpop3_result_error(pRet, "POP3 send buffer alloc failed");
		return FALSE;
	}
	if ( pClient->hSock != XPOP3_SOCKET_INVALID && pClient->hSock != 0 ) {
		if ( pClient->pTls != NULL ) {
			const char* p = tBuf.sData;
			size_t iLeft = tBuf.iLen;
			while ( iLeft > 0u ) {
				size_t iWritten = 0u;
				xnet_result iRes = xrtNetTlsSessionWritePlain(pClient->pTls, p, iLeft, &iWritten);
				if ( iRes != XRT_NET_OK && iRes != XRT_NET_AGAIN ) {
					xpop3_buf_free(&tBuf);
					xpop3_result_error(pRet, "POP3 TLS write failed");
					return FALSE;
				}
				if ( !xpop3_tls_flush(pClient, pRet) ) {
					xpop3_buf_free(&tBuf);
					return FALSE;
				}
				if ( iWritten == 0u && iLeft > 0u ) {
					xpop3_buf_free(&tBuf);
					xpop3_result_error(pRet, "POP3 TLS write stalled");
					return FALSE;
				}
				p += iWritten;
				iLeft -= iWritten;
			}
		} else if ( !xpop3_socket_send_all(pClient, tBuf.sData, tBuf.iLen, pRet) ) {
			xpop3_buf_free(&tBuf);
			return FALSE;
		}
		xpop3_buf_free(&tBuf);
		return TRUE;
	}
	iRet = xrtNetStreamSend(pClient->pStream, tBuf.sData, tBuf.iLen);
	xpop3_buf_free(&tBuf);
	if ( iRet != XRT_NET_OK ) {
		xpop3_result_error(pRet, "POP3 send failed");
		return FALSE;
	}
	return TRUE;
}

static bool xpop3_read_line(xpop3client* pClient, char* sOut, size_t iOutCap, xpop3result* pRet)
{
	uint64 iDeadlineMs;
	size_t i;

	if ( sOut == NULL || iOutCap == 0u ) {
		xpop3_result_error(pRet, "POP3 read line output invalid");
		return FALSE;
	}
	sOut[0] = '\0';
	if ( pClient == NULL ) {
		xpop3_result_error(pRet, "POP3 client is not connected");
		return FALSE;
	}
	if ( pClient->hSock != XPOP3_SOCKET_INVALID && pClient->hSock != 0 ) {
		size_t iLen = 0u;
		for (;;) {
			char ch;
			size_t iRead = 0u;
			if ( !xpop3_recv_bytes(pClient, &ch, 1u, &iRead, pRet) ) {
				return FALSE;
			}
			if ( ch == '\n' ) {
				if ( iLen > 0u && sOut[iLen - 1u] == '\r' ) {
					iLen--;
				}
				sOut[iLen] = '\0';
				return TRUE;
			}
			if ( iLen + 1u < iOutCap ) {
				sOut[iLen++] = ch;
			}
		}
	}
	if ( pClient->pLock == NULL ) {
		xpop3_result_error(pRet, "POP3 client is not connected");
		return FALSE;
	}
	iDeadlineMs = xpop3_now_ms() + pClient->iTimeoutMs;
	for (;;) {
		xrtMutexLock(pClient->pLock);
		for ( i = 0u; i + 1u < pClient->tRecv.iLen; i++ ) {
			if ( pClient->tRecv.sData[i] == '\r' && pClient->tRecv.sData[i + 1u] == '\n' ) {
				size_t iCopy = i >= iOutCap ? iOutCap - 1u : i;
				memcpy(sOut, pClient->tRecv.sData, iCopy);
				sOut[iCopy] = '\0';
				memmove(pClient->tRecv.sData, pClient->tRecv.sData + i + 2u, pClient->tRecv.iLen - (i + 2u));
				pClient->tRecv.iLen -= (i + 2u);
				pClient->tRecv.sData[pClient->tRecv.iLen] = '\0';
				xrtMutexUnlock(pClient->pLock);
				return TRUE;
			}
		}
		if ( pClient->bRecvOverflow ) {
			xrtMutexUnlock(pClient->pLock);
			xpop3_result_error(pRet, "POP3 recv buffer overflow");
			return FALSE;
		}
		if ( pClient->iLastSysErr != 0 ) {
			int iSysErr = pClient->iLastSysErr;
			xrtMutexUnlock(pClient->pLock);
			xpop3_result_errorf(pRet, "POP3 recv failed (sys=%d)", iSysErr);
			return FALSE;
		}
		if ( pClient->bClosed ) {
			int iReason = pClient->iCloseReason;
			xrtMutexUnlock(pClient->pLock);
			xpop3_result_errorf(pRet, "POP3 stream closed (reason=%d)", iReason);
			return FALSE;
		}
		if ( !xpop3_wait_recv_state(pClient, iDeadlineMs) ) {
			xrtMutexUnlock(pClient->pLock);
			xpop3_result_errorf(pRet, "POP3 recv timeout (%u ms)", pClient->iTimeoutMs);
			return FALSE;
		}
		xrtMutexUnlock(pClient->pLock);
	}
}

static bool xpop3_read_reply(xpop3client* pClient, xpop3result* pRet, char* sOut, size_t iOutCap)
{
	char sLine[1024];

	if ( !xpop3_read_line(pClient, sLine, sizeof(sLine), pRet) ) {
		return FALSE;
	}
	if ( pRet != NULL ) {
		snprintf(pRet->sLastReply, sizeof(pRet->sLastReply), "%s", sLine);
		pRet->iServerCode = xpop3_response_is_ok(sLine) ? 0 : -1;
	}
	if ( sOut != NULL && iOutCap > 0u ) {
		snprintf(sOut, iOutCap, "%s", sLine);
	}
	if ( !xpop3_response_is_ok(sLine) ) {
		xpop3_result_error(pRet, xpop3_response_is_err(sLine) ? xpop3_response_text(sLine) : "POP3 invalid reply");
		return FALSE;
	}
	return TRUE;
}

static bool xpop3_read_multiline(xpop3client* pClient, str* psOut, xpop3result* pRet)
{
	xpop3buf tBuf;
	char sLine[4096];

	if ( psOut == NULL ) {
		xpop3_result_error(pRet, "POP3 multiline output invalid");
		return FALSE;
	}
	*psOut = NULL;
	memset(&tBuf, 0, sizeof(tBuf));
	for (;;) {
		const char* sUseLine = sLine;
		if ( !xpop3_read_line(pClient, sLine, sizeof(sLine), pRet) ) {
			xpop3_buf_free(&tBuf);
			return FALSE;
		}
		if ( strcmp(sLine, ".") == 0 ) {
			break;
		}
		if ( sLine[0] == '.' ) {
			sUseLine = sLine + 1;
		}
		if ( !xpop3_buf_append(&tBuf, sUseLine) || !xpop3_buf_append(&tBuf, "\r\n") ) {
			xpop3_buf_free(&tBuf);
			xpop3_result_error(pRet, "POP3 multiline buffer alloc failed");
			return FALSE;
		}
	}
	if ( tBuf.sData == NULL ) {
		tBuf.sData = (char*)xrtCopyStr((str)"", 0);
		if ( tBuf.sData == NULL ) {
			return FALSE;
		}
	}
	*psOut = (str)tBuf.sData;
	return TRUE;
}

static bool xpop3_cmd_simple(xpop3client* pClient, const char* sLine, int iStage, xpop3result* pRet)
{
	xpop3_result_stage(pRet, iStage);
	return xpop3_send_line(pClient, sLine, pRet) && xpop3_read_reply(pClient, pRet, NULL, 0u);
}

static int xpop3_auth_mode_auto(const xpop3config* pCfg)
{
	if ( pCfg == NULL ) {
		return XPOP3_AUTH_USERPASS;
	}
	if ( pCfg->iAuthMode == XPOP3_AUTH_PLAIN || pCfg->iAuthMode == XPOP3_AUTH_USERPASS ) {
		return pCfg->iAuthMode;
	}
	return XPOP3_AUTH_USERPASS;
}

static bool xpop3_auth_plain(xpop3client* pClient, const xpop3config* pCfg, xpop3result* pRet)
{
	xpop3buf tPlain;
	xpop3buf tCmd;
	str sBase64;
	bool bOK;

	memset(&tPlain, 0, sizeof(tPlain));
	memset(&tCmd, 0, sizeof(tCmd));
	if ( !xpop3_buf_append_n(&tPlain, "\0", 1u)
		|| !xpop3_buf_append(&tPlain, xpop3_str_or_empty(pCfg->sUser))
		|| !xpop3_buf_append_n(&tPlain, "\0", 1u)
		|| !xpop3_buf_append(&tPlain, xpop3_str_or_empty(pCfg->sPass)) ) {
		xpop3_buf_free(&tPlain);
		xpop3_result_error(pRet, "POP3 AUTH PLAIN buffer alloc failed");
		return FALSE;
	}
	sBase64 = xrtBase64Encode((ptr)tPlain.sData, tPlain.iLen, NULL);
	xpop3_buf_free(&tPlain);
	if ( sBase64 == NULL ) {
		xpop3_result_error(pRet, "POP3 AUTH PLAIN base64 failed");
		return FALSE;
	}
	if ( !xpop3_buf_append(&tCmd, "AUTH PLAIN ") || !xpop3_buf_append(&tCmd, (const char*)sBase64) ) {
		xrtFree(sBase64);
		xpop3_buf_free(&tCmd);
		xpop3_result_error(pRet, "POP3 AUTH PLAIN alloc failed");
		return FALSE;
	}
	xrtFree(sBase64);
	bOK = xpop3_send_line(pClient, tCmd.sData, pRet) && xpop3_read_reply(pClient, pRet, NULL, 0u);
	xpop3_buf_free(&tCmd);
	return bOK;
}

static bool xpop3_auth_userpass(xpop3client* pClient, const xpop3config* pCfg, xpop3result* pRet)
{
	xpop3buf tCmd;

	memset(&tCmd, 0, sizeof(tCmd));
	if ( !xpop3_buf_append(&tCmd, "USER ") || !xpop3_buf_append(&tCmd, pCfg->sUser) ) {
		xpop3_buf_free(&tCmd);
		xpop3_result_error(pRet, "POP3 USER alloc failed");
		return FALSE;
	}
	if ( !xpop3_send_line(pClient, tCmd.sData, pRet) || !xpop3_read_reply(pClient, pRet, NULL, 0u) ) {
		xpop3_buf_free(&tCmd);
		return FALSE;
	}
	xpop3_buf_free(&tCmd);
	memset(&tCmd, 0, sizeof(tCmd));
	if ( !xpop3_buf_append(&tCmd, "PASS ") || !xpop3_buf_append(&tCmd, xpop3_str_or_empty(pCfg->sPass)) ) {
		xpop3_buf_free(&tCmd);
		xpop3_result_error(pRet, "POP3 PASS alloc failed");
		return FALSE;
	}
	if ( !xpop3_send_line(pClient, tCmd.sData, pRet) || !xpop3_read_reply(pClient, pRet, NULL, 0u) ) {
		xpop3_buf_free(&tCmd);
		return FALSE;
	}
	xpop3_buf_free(&tCmd);
	return TRUE;
}

static bool xrtPop3Open(const xpop3config* pCfg, xpop3client** ppClient, xpop3result* pRet)
{
	xpop3client* pClient;
	char sReply[1024];

	if ( pRet != NULL ) {
		xrtPop3ResultInit(pRet);
	}
	if ( ppClient == NULL ) {
		xpop3_result_error(pRet, "POP3 client output missing");
		return FALSE;
	}
	*ppClient = NULL;
	if ( pCfg == NULL ) {
		xpop3_result_error(pRet, "POP3 config missing");
		return FALSE;
	}
	pClient = (xpop3client*)xrtCalloc(1, sizeof(*pClient));
	if ( pClient == NULL ) {
		xpop3_result_error(pRet, "POP3 client alloc failed");
		return FALSE;
	}
	xpop3_result_stage(pRet, XPOP3_STAGE_CONNECT);
	if ( !xpop3_client_connect(pClient, pCfg, pRet) ) {
		xrtPop3Close(pClient);
		return FALSE;
	}
	xpop3_result_stage(pRet, XPOP3_STAGE_GREETING);
	if ( !xpop3_read_reply(pClient, pRet, sReply, sizeof(sReply)) ) {
		xrtPop3Close(pClient);
		return FALSE;
	}
	if ( xpop3_secure_mode_auto(pCfg) == XPOP3_SECURE_STARTTLS ) {
		xpop3_result_stage(pRet, XPOP3_STAGE_TLS);
		if ( !xpop3_send_line(pClient, "STLS", pRet) || !xpop3_read_reply(pClient, pRet, NULL, 0u) ) {
			xrtPop3Close(pClient);
			return FALSE;
		}
		if ( !xpop3_tls_handshake(pClient, pCfg, pRet) ) {
			xrtPop3Close(pClient);
			return FALSE;
		}
		if ( pRet != NULL ) {
			pRet->bUsedStartTLS = TRUE;
		}
	}
	if ( pCfg->sUser != NULL && pCfg->sUser[0] != '\0' ) {
		int iAuthMode = xpop3_auth_mode_auto(pCfg);
		xpop3_result_stage(pRet, XPOP3_STAGE_AUTH);
		if ( iAuthMode == XPOP3_AUTH_PLAIN ) {
			if ( !xpop3_auth_plain(pClient, pCfg, pRet) ) {
				xrtPop3Close(pClient);
				return FALSE;
			}
		} else if ( !xpop3_auth_userpass(pClient, pCfg, pRet) ) {
			xrtPop3Close(pClient);
			return FALSE;
		}
	}
	*ppClient = pClient;
	if ( pRet != NULL ) {
		pRet->bSuccess = TRUE;
	}
	return TRUE;
}

static bool xrtPop3Stat(xpop3client* pClient, xpop3stat* pStat, xpop3result* pRet)
{
	char sReply[1024];

	if ( pStat == NULL ) {
		xpop3_result_error(pRet, "POP3 STAT output missing");
		return FALSE;
	}
	memset(pStat, 0, sizeof(*pStat));
	xpop3_result_stage(pRet, XPOP3_STAGE_COMMAND);
	if ( !xpop3_send_line(pClient, "STAT", pRet) || !xpop3_read_reply(pClient, pRet, sReply, sizeof(sReply)) ) {
		return FALSE;
	}
	if ( !xpop3_parse_stat_line(sReply, pStat) ) {
		xpop3_result_error(pRet, "POP3 STAT parse failed");
		return FALSE;
	}
	return TRUE;
}

static bool xrtPop3CapaRaw(xpop3client* pClient, str* psOut, xpop3result* pRet)
{
	xpop3_result_stage(pRet, XPOP3_STAGE_CAPA);
	return xpop3_send_line(pClient, "CAPA", pRet)
		&& xpop3_read_reply(pClient, pRet, NULL, 0u)
		&& xpop3_read_multiline(pClient, psOut, pRet);
}

static bool xrtPop3Capa(xpop3client* pClient, uint32* pCaps, xpop3result* pRet)
{
	str sRaw = NULL;

	if ( pCaps == NULL ) {
		xpop3_result_error(pRet, "POP3 CAPA output missing");
		return FALSE;
	}
	*pCaps = 0u;
	if ( !xrtPop3CapaRaw(pClient, &sRaw, pRet) ) {
		return FALSE;
	}
	*pCaps = xpop3_parse_capa_block((const char*)sRaw);
	if ( pRet != NULL ) {
		pRet->iCapabilities = *pCaps;
	}
	xrtFree(sRaw);
	return TRUE;
}

static bool xrtPop3ListRaw(xpop3client* pClient, str* psOut, xpop3result* pRet)
{
	xpop3_result_stage(pRet, XPOP3_STAGE_COMMAND);
	return xpop3_send_line(pClient, "LIST", pRet)
		&& xpop3_read_reply(pClient, pRet, NULL, 0u)
		&& xpop3_read_multiline(pClient, psOut, pRet);
}

static bool xrtPop3List(xpop3client* pClient, xpop3listitem** ppItems, size_t* pCount, xpop3result* pRet)
{
	str sRaw = NULL;
	bool bOK;

	if ( ppItems == NULL || pCount == NULL ) {
		xpop3_result_error(pRet, "POP3 LIST output missing");
		return FALSE;
	}
	*ppItems = NULL;
	*pCount = 0u;
	if ( !xrtPop3ListRaw(pClient, &sRaw, pRet) ) {
		return FALSE;
	}
	bOK = xpop3_parse_list_block((const char*)sRaw, ppItems, pCount);
	xrtFree(sRaw);
	if ( !bOK ) {
		xpop3_result_error(pRet, "POP3 LIST parse failed");
	}
	return bOK;
}

static bool xrtPop3UidlRaw(xpop3client* pClient, str* psOut, xpop3result* pRet)
{
	xpop3_result_stage(pRet, XPOP3_STAGE_COMMAND);
	return xpop3_send_line(pClient, "UIDL", pRet)
		&& xpop3_read_reply(pClient, pRet, NULL, 0u)
		&& xpop3_read_multiline(pClient, psOut, pRet);
}

static bool xrtPop3Uidl(xpop3client* pClient, xpop3uidlitem** ppItems, size_t* pCount, xpop3result* pRet)
{
	str sRaw = NULL;
	bool bOK;

	if ( ppItems == NULL || pCount == NULL ) {
		xpop3_result_error(pRet, "POP3 UIDL output missing");
		return FALSE;
	}
	*ppItems = NULL;
	*pCount = 0u;
	if ( !xrtPop3UidlRaw(pClient, &sRaw, pRet) ) {
		return FALSE;
	}
	bOK = xpop3_parse_uidl_block((const char*)sRaw, ppItems, pCount);
	xrtFree(sRaw);
	if ( !bOK ) {
		xpop3_result_error(pRet, "POP3 UIDL parse failed");
	}
	return bOK;
}

static void xrtPop3ListFree(xpop3listitem* arrItems)
{
	if ( arrItems != NULL ) {
		xrtFree(arrItems);
	}
}

static void xrtPop3UidlFree(xpop3uidlitem* arrItems)
{
	if ( arrItems != NULL ) {
		xrtFree(arrItems);
	}
}

static void xrtPop3SummariesFree(xpop3summary* arrItems)
{
	if ( arrItems != NULL ) {
		xrtFree(arrItems);
	}
}

static bool xrtPop3RetrRaw(xpop3client* pClient, uint32 iNumber, str* psOut, xpop3result* pRet)
{
	char sCmd[64];
	xpop3_result_stage(pRet, XPOP3_STAGE_RETR);
	snprintf(sCmd, sizeof(sCmd), "RETR %u", (unsigned)iNumber);
	return xpop3_send_line(pClient, sCmd, pRet)
		&& xpop3_read_reply(pClient, pRet, NULL, 0u)
		&& xpop3_read_multiline(pClient, psOut, pRet);
}

static bool xrtPop3RetrMime(xpop3client* pClient, uint32 iNumber, xmailparsedmessage* pMsg, xpop3result* pRet)
{
	str sRaw = NULL;

	if ( pMsg == NULL ) {
		xpop3_result_error(pRet, "POP3 RETR MIME output missing");
		return FALSE;
	}
	memset(pMsg, 0, sizeof(*pMsg));
	if ( !xrtPop3RetrRaw(pClient, iNumber, &sRaw, pRet) ) {
		return FALSE;
	}
	if ( !xmailMimeParseMessage((const char*)sRaw, pMsg) ) {
		xrtFree(sRaw);
		xpop3_result_error(pRet, "POP3 RETR MIME parse failed");
		return FALSE;
	}
	xrtFree(sRaw);
	return TRUE;
}

static bool xrtPop3TopRaw(xpop3client* pClient, uint32 iNumber, uint32 iLineCount, str* psOut, xpop3result* pRet)
{
	char sCmd[64];

	xpop3_result_stage(pRet, XPOP3_STAGE_COMMAND);
	snprintf(sCmd, sizeof(sCmd), "TOP %u %u", (unsigned)iNumber, (unsigned)iLineCount);
	return xpop3_send_line(pClient, sCmd, pRet)
		&& xpop3_read_reply(pClient, pRet, NULL, 0u)
		&& xpop3_read_multiline(pClient, psOut, pRet);
}

static bool xrtPop3TopMime(xpop3client* pClient, uint32 iNumber, uint32 iLineCount, xmailparsedmessage* pMsg, xpop3result* pRet)
{
	str sRaw = NULL;

	if ( pMsg == NULL ) {
		xpop3_result_error(pRet, "POP3 TOP MIME output missing");
		return FALSE;
	}
	memset(pMsg, 0, sizeof(*pMsg));
	if ( !xrtPop3TopRaw(pClient, iNumber, iLineCount, &sRaw, pRet) ) {
		return FALSE;
	}
	if ( !xmailMimeParseMessage((const char*)sRaw, pMsg) ) {
		xrtFree(sRaw);
		xpop3_result_error(pRet, "POP3 TOP MIME parse failed");
		return FALSE;
	}
	xrtFree(sRaw);
	return TRUE;
}

static const char* xpop3_uidl_find(const xpop3uidlitem* arrUidl, size_t iUidlCount, uint32 iNumber)
{
	size_t i;

	if ( arrUidl == NULL ) {
		return "";
	}
	for ( i = 0u; i < iUidlCount; i++ ) {
		if ( arrUidl[i].iNumber == iNumber ) {
			return arrUidl[i].sUid;
		}
	}
	return "";
}

static void xpop3_summary_set_text(char* sOut, size_t iOutCap, const char* sValue)
{
	if ( sOut == NULL || iOutCap == 0u ) {
		return;
	}
	snprintf(sOut, iOutCap, "%s", sValue ? sValue : "");
}

static bool xrtPop3ListSummaries(xpop3client* pClient, xpop3summary** ppItems, size_t* pCount, xpop3result* pRet)
{
	xpop3listitem* arrList = NULL;
	xpop3uidlitem* arrUidl = NULL;
	xpop3summary* arrOut = NULL;
	size_t iListCount = 0u;
	size_t iUidlCount = 0u;
	size_t i;
	bool bOK = FALSE;

	if ( ppItems == NULL || pCount == NULL ) {
		xpop3_result_error(pRet, "POP3 summaries output missing");
		return FALSE;
	}
	*ppItems = NULL;
	*pCount = 0u;
	if ( !xrtPop3List(pClient, &arrList, &iListCount, pRet) ) {
		goto cleanup;
	}
	(void)xrtPop3Uidl(pClient, &arrUidl, &iUidlCount, pRet);
	if ( iListCount == 0u ) {
		bOK = TRUE;
		goto cleanup;
	}
	arrOut = (xpop3summary*)xrtCalloc(iListCount, sizeof(*arrOut));
	if ( arrOut == NULL ) {
		xpop3_result_error(pRet, "POP3 summaries alloc failed");
		goto cleanup;
	}
	for ( i = 0u; i < iListCount; i++ ) {
		xmailparsedmessage tMsg;
		const char* sHeader;

		memset(&tMsg, 0, sizeof(tMsg));
		arrOut[i].iNumber = arrList[i].iNumber;
		arrOut[i].iSize = arrList[i].iSize;
		xpop3_summary_set_text(arrOut[i].sUid, sizeof(arrOut[i].sUid), xpop3_uidl_find(arrUidl, iUidlCount, arrList[i].iNumber));
		if ( !xrtPop3TopMime(pClient, arrList[i].iNumber, 0u, &tMsg, pRet) ) {
			xmailMimeParsedMessageFree(&tMsg);
			goto cleanup;
		}
		sHeader = xmailMimeParsedGetHeader(&tMsg, "Subject");
		xpop3_summary_set_text(arrOut[i].sSubject, sizeof(arrOut[i].sSubject), sHeader);
		sHeader = xmailMimeParsedGetHeader(&tMsg, "From");
		xpop3_summary_set_text(arrOut[i].sFrom, sizeof(arrOut[i].sFrom), sHeader);
		sHeader = xmailMimeParsedGetHeader(&tMsg, "Date");
		xpop3_summary_set_text(arrOut[i].sDate, sizeof(arrOut[i].sDate), sHeader);
		xmailMimeParsedMessageFree(&tMsg);
	}
	*ppItems = arrOut;
	*pCount = iListCount;
	arrOut = NULL;
	bOK = TRUE;

cleanup:
	if ( arrOut != NULL ) {
		xrtFree(arrOut);
	}
	xrtPop3ListFree(arrList);
	xrtPop3UidlFree(arrUidl);
	return bOK;
}

static bool xrtPop3Delete(xpop3client* pClient, uint32 iNumber, xpop3result* pRet)
{
	char sCmd[64];
	snprintf(sCmd, sizeof(sCmd), "DELE %u", (unsigned)iNumber);
	return xpop3_cmd_simple(pClient, sCmd, XPOP3_STAGE_COMMAND, pRet);
}

static bool xrtPop3Noop(xpop3client* pClient, xpop3result* pRet)
{
	return xpop3_cmd_simple(pClient, "NOOP", XPOP3_STAGE_COMMAND, pRet);
}

static bool xrtPop3Rset(xpop3client* pClient, xpop3result* pRet)
{
	return xpop3_cmd_simple(pClient, "RSET", XPOP3_STAGE_COMMAND, pRet);
}

static bool xrtPop3Quit(xpop3client* pClient, xpop3result* pRet)
{
	bool bOK;
	xpop3_result_stage(pRet, XPOP3_STAGE_QUIT);
	bOK = xpop3_send_line(pClient, "QUIT", pRet) && xpop3_read_reply(pClient, pRet, NULL, 0u);
	xrtPop3Close(pClient);
	return bOK;
}

static void xpop3_fetch_task_ctx_free(xpop3fetchtaskctx* pCtx)
{
	if ( pCtx == NULL ) {
		return;
	}
	if ( pCtx->sHost != NULL ) xrtFree(pCtx->sHost);
	if ( pCtx->sUser != NULL ) xrtFree(pCtx->sUser);
	if ( pCtx->sPass != NULL ) xrtFree(pCtx->sPass);
	if ( pCtx->sCaFile != NULL ) xrtFree(pCtx->sCaFile);
	if ( pCtx->sDebugName != NULL ) xrtFree(pCtx->sDebugName);
	xrtFree(pCtx);
}

static xpop3fetchtaskctx* xpop3_fetch_task_ctx_create(const xpop3config* pCfg, uint32 iNumber, uint32 iTopLineCount, const xpop3asyncopts* pOpts, bool bParseMime, bool bUseTop)
{
	xpop3fetchtaskctx* pCtx;

	if ( pCfg == NULL || iNumber == 0u ) {
		return NULL;
	}
	pCtx = (xpop3fetchtaskctx*)xrtCalloc(1, sizeof(*pCtx));
	if ( pCtx == NULL ) {
		return NULL;
	}
	pCtx->tCfg = *pCfg;
	if ( pOpts != NULL ) {
		pCtx->tOpts = *pOpts;
	}
	pCtx->iNumber = iNumber;
	pCtx->iTopLineCount = iTopLineCount;
	pCtx->bParseMime = bParseMime;
	pCtx->bUseTop = bUseTop;
	pCtx->sHost = xpop3_dup_text(pCfg->sHost);
	pCtx->sUser = xpop3_dup_text(pCfg->sUser);
	pCtx->sPass = xpop3_dup_text(pCfg->sPass);
	pCtx->sCaFile = xpop3_dup_text(pCfg->sCaFile);
	pCtx->sDebugName = xpop3_dup_text(pCtx->tOpts.sDebugName);
	if ( (pCfg->sHost != NULL && pCtx->sHost == NULL)
		|| (pCfg->sUser != NULL && pCtx->sUser == NULL)
		|| (pCfg->sPass != NULL && pCtx->sPass == NULL)
		|| (pCfg->sCaFile != NULL && pCtx->sCaFile == NULL)
		|| (pCtx->tOpts.sDebugName != NULL && pCtx->sDebugName == NULL) ) {
		xpop3_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	pCtx->tCfg.sHost = pCtx->sHost;
	pCtx->tCfg.sUser = pCtx->sUser;
	pCtx->tCfg.sPass = pCtx->sPass;
	pCtx->tCfg.sCaFile = pCtx->sCaFile;
	pCtx->tOpts.sDebugName = pCtx->sDebugName;
	if ( pCtx->tOpts.iTimeoutMs > 0u ) {
		pCtx->tCfg.iTimeoutMs = pCtx->tOpts.iTimeoutMs;
	}
	return pCtx;
}

static xpop3fetchtaskctx* xpop3_summary_task_ctx_create(const xpop3config* pCfg, const xpop3asyncopts* pOpts)
{
	xpop3fetchtaskctx* pCtx;

	if ( pCfg == NULL ) {
		return NULL;
	}
	pCtx = (xpop3fetchtaskctx*)xrtCalloc(1, sizeof(*pCtx));
	if ( pCtx == NULL ) {
		return NULL;
	}
	pCtx->tCfg = *pCfg;
	if ( pOpts != NULL ) {
		pCtx->tOpts = *pOpts;
	}
	pCtx->sHost = xpop3_dup_text(pCfg->sHost);
	pCtx->sUser = xpop3_dup_text(pCfg->sUser);
	pCtx->sPass = xpop3_dup_text(pCfg->sPass);
	pCtx->sCaFile = xpop3_dup_text(pCfg->sCaFile);
	pCtx->sDebugName = xpop3_dup_text(pCtx->tOpts.sDebugName);
	if ( (pCfg->sHost != NULL && pCtx->sHost == NULL)
		|| (pCfg->sUser != NULL && pCtx->sUser == NULL)
		|| (pCfg->sPass != NULL && pCtx->sPass == NULL)
		|| (pCfg->sCaFile != NULL && pCtx->sCaFile == NULL)
		|| (pCtx->tOpts.sDebugName != NULL && pCtx->sDebugName == NULL) ) {
		xpop3_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	pCtx->tCfg.sHost = pCtx->sHost;
	pCtx->tCfg.sUser = pCtx->sUser;
	pCtx->tCfg.sPass = pCtx->sPass;
	pCtx->tCfg.sCaFile = pCtx->sCaFile;
	pCtx->tOpts.sDebugName = pCtx->sDebugName;
	if ( pCtx->tOpts.iTimeoutMs > 0u ) {
		pCtx->tCfg.iTimeoutMs = pCtx->tOpts.iTimeoutMs;
	}
	return pCtx;
}

static bool xpop3_fetch_run_common(xpop3fetchtaskctx* pCtx, xpop3result* pRet, str* psRaw, xmailparsedmessage* pMsg)
{
	xpop3client* pClient = NULL;
	bool bOK = FALSE;

	if ( pRet != NULL ) {
		xrtPop3ResultInit(pRet);
	}
	if ( pCtx == NULL ) {
		xpop3_result_error(pRet, "POP3 async task context missing");
		return FALSE;
	}
	if ( psRaw != NULL ) {
		*psRaw = NULL;
	}
	if ( pMsg != NULL ) {
		memset(pMsg, 0, sizeof(*pMsg));
	}
	if ( !xrtPop3Open(&pCtx->tCfg, &pClient, pRet) ) {
		goto cleanup;
	}
	if ( pMsg != NULL ) {
		bOK = pCtx->bUseTop
			? xrtPop3TopMime(pClient, pCtx->iNumber, pCtx->iTopLineCount, pMsg, pRet)
			: xrtPop3RetrMime(pClient, pCtx->iNumber, pMsg, pRet);
	} else {
		bOK = pCtx->bUseTop
			? xrtPop3TopRaw(pClient, pCtx->iNumber, pCtx->iTopLineCount, psRaw, pRet)
			: xrtPop3RetrRaw(pClient, pCtx->iNumber, psRaw, pRet);
	}
	if ( !bOK ) {
		goto cleanup;
	}
	if ( !pCtx->bUseTop && pCtx->tOpts.bDeleteAfterRetr && !xrtPop3Delete(pClient, pCtx->iNumber, pRet) ) {
		bOK = FALSE;
		goto cleanup;
	}
	if ( !xrtPop3Quit(pClient, pRet) ) {
		pClient = NULL;
		bOK = FALSE;
		goto cleanup;
	}
	pClient = NULL;
	bOK = TRUE;
	if ( pRet != NULL ) {
		pRet->bSuccess = TRUE;
	}

cleanup:
	if ( pClient != NULL ) {
		xrtPop3Close(pClient);
	}
	if ( !bOK && psRaw != NULL && *psRaw != NULL ) {
		xrtFree(*psRaw);
		*psRaw = NULL;
	}
	return bOK;
}

static bool xpop3_summary_run_common(xpop3fetchtaskctx* pCtx, xpop3result* pRet, xpop3summary** ppItems, size_t* pCount)
{
	xpop3client* pClient = NULL;
	bool bOK = FALSE;

	if ( pRet != NULL ) {
		xrtPop3ResultInit(pRet);
	}
	if ( ppItems != NULL ) {
		*ppItems = NULL;
	}
	if ( pCount != NULL ) {
		*pCount = 0u;
	}
	if ( pCtx == NULL ) {
		xpop3_result_error(pRet, "POP3 async summary task context missing");
		return FALSE;
	}
	if ( ppItems == NULL || pCount == NULL ) {
		xpop3_result_error(pRet, "POP3 async summary output missing");
		return FALSE;
	}
	if ( !xrtPop3Open(&pCtx->tCfg, &pClient, pRet) ) {
		goto cleanup;
	}
	if ( !xrtPop3ListSummaries(pClient, ppItems, pCount, pRet) ) {
		goto cleanup;
	}
	if ( !xrtPop3Quit(pClient, pRet) ) {
		pClient = NULL;
		goto cleanup;
	}
	pClient = NULL;
	bOK = TRUE;
	if ( pRet != NULL ) {
		pRet->bSuccess = TRUE;
	}

cleanup:
	if ( pClient != NULL ) {
		xrtPop3Close(pClient);
	}
	if ( !bOK && ppItems != NULL && *ppItems != NULL ) {
		xrtPop3SummariesFree(*ppItems);
		*ppItems = NULL;
	}
	if ( !bOK && pCount != NULL ) {
		*pCount = 0u;
	}
	return bOK;
}

static int32 xpop3_fetch_raw_task(ptr pArg, xfuture_result* pOut)
{
	xpop3fetchtaskctx* pCtx = (xpop3fetchtaskctx*)pArg;
	xpop3fetchrawresult* pResult;

	if ( pOut == NULL ) {
		xpop3_fetch_task_ctx_free(pCtx);
		return XRT_NET_ERROR;
	}
	memset(pOut, 0, sizeof(*pOut));
	pResult = (xpop3fetchrawresult*)xrtCalloc(1, sizeof(*pResult));
	if ( pResult == NULL ) {
		pOut->iStatus = XRT_NET_ERROR;
		pOut->sError = (str)"POP3 async raw result alloc failed";
		xpop3_fetch_task_ctx_free(pCtx);
		return pOut->iStatus;
	}
	(void)xpop3_fetch_run_common(pCtx, &pResult->tResult, &pResult->sRaw, NULL);
	pOut->iStatus = XRT_NET_OK;
	pOut->pValue = pResult;
	pOut->iFlags = XFUTURE_RESULT_F_OWN_VALUE;
	xpop3_fetch_task_ctx_free(pCtx);
	return pOut->iStatus;
}

static int32 xpop3_fetch_mime_task(ptr pArg, xfuture_result* pOut)
{
	xpop3fetchtaskctx* pCtx = (xpop3fetchtaskctx*)pArg;
	xpop3fetchmimeresult* pResult;

	if ( pOut == NULL ) {
		xpop3_fetch_task_ctx_free(pCtx);
		return XRT_NET_ERROR;
	}
	memset(pOut, 0, sizeof(*pOut));
	pResult = (xpop3fetchmimeresult*)xrtCalloc(1, sizeof(*pResult));
	if ( pResult == NULL ) {
		pOut->iStatus = XRT_NET_ERROR;
		pOut->sError = (str)"POP3 async MIME result alloc failed";
		xpop3_fetch_task_ctx_free(pCtx);
		return pOut->iStatus;
	}
	(void)xpop3_fetch_run_common(pCtx, &pResult->tResult, NULL, &pResult->tMessage);
	pOut->iStatus = XRT_NET_OK;
	pOut->pValue = pResult;
	pOut->iFlags = XFUTURE_RESULT_F_OWN_VALUE;
	xpop3_fetch_task_ctx_free(pCtx);
	return pOut->iStatus;
}

static int32 xpop3_summary_task(ptr pArg, xfuture_result* pOut)
{
	xpop3fetchtaskctx* pCtx = (xpop3fetchtaskctx*)pArg;
	xpop3summaryresult* pResult;

	if ( pOut == NULL ) {
		xpop3_fetch_task_ctx_free(pCtx);
		return XRT_NET_ERROR;
	}
	memset(pOut, 0, sizeof(*pOut));
	pResult = (xpop3summaryresult*)xrtCalloc(1, sizeof(*pResult));
	if ( pResult == NULL ) {
		pOut->iStatus = XRT_NET_ERROR;
		pOut->sError = (str)"POP3 async summary result alloc failed";
		xpop3_fetch_task_ctx_free(pCtx);
		return pOut->iStatus;
	}
	(void)xpop3_summary_run_common(pCtx, &pResult->tResult, &pResult->arrItems, &pResult->iCount);
	pOut->iStatus = XRT_NET_OK;
	pOut->pValue = pResult;
	pOut->iFlags = XFUTURE_RESULT_F_OWN_VALUE;
	xpop3_fetch_task_ctx_free(pCtx);
	return pOut->iStatus;
}

static xfuture* xrtPop3FetchRawFuture(const xpop3config* pCfg, uint32 iNumber, const xpop3asyncopts* pOpts)
{
	xpop3fetchtaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = xpop3_fetch_task_ctx_create(pCfg, iNumber, 0u, pOpts, FALSE, FALSE);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pFuture = xTaskRunThread(xpop3_fetch_raw_task, pCtx, 0);
	if ( pFuture == NULL ) {
		xpop3_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static xfuture* xrtPop3TopRawFuture(const xpop3config* pCfg, uint32 iNumber, uint32 iLineCount, const xpop3asyncopts* pOpts)
{
	xpop3fetchtaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = xpop3_fetch_task_ctx_create(pCfg, iNumber, iLineCount, pOpts, FALSE, TRUE);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pFuture = xTaskRunThread(xpop3_fetch_raw_task, pCtx, 0);
	if ( pFuture == NULL ) {
		xpop3_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static xfuture* xrtPop3FetchMimeFuture(const xpop3config* pCfg, uint32 iNumber, const xpop3asyncopts* pOpts)
{
	xpop3fetchtaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = xpop3_fetch_task_ctx_create(pCfg, iNumber, 0u, pOpts, TRUE, FALSE);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pFuture = xTaskRunThread(xpop3_fetch_mime_task, pCtx, 0);
	if ( pFuture == NULL ) {
		xpop3_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static xfuture* xrtPop3TopMimeFuture(const xpop3config* pCfg, uint32 iNumber, uint32 iLineCount, const xpop3asyncopts* pOpts)
{
	xpop3fetchtaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = xpop3_fetch_task_ctx_create(pCfg, iNumber, iLineCount, pOpts, TRUE, TRUE);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pFuture = xTaskRunThread(xpop3_fetch_mime_task, pCtx, 0);
	if ( pFuture == NULL ) {
		xpop3_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static xfuture* xrtPop3ListSummariesFuture(const xpop3config* pCfg, const xpop3asyncopts* pOpts)
{
	xpop3fetchtaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = xpop3_summary_task_ctx_create(pCfg, pOpts);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pFuture = xTaskRunThread(xpop3_summary_task, pCtx, 0);
	if ( pFuture == NULL ) {
		xpop3_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static bool xrtPop3FetchRawAsyncWait(const xpop3config* pCfg, uint32 iNumber, const xpop3asyncopts* pOpts, str* psRaw, xpop3result* pOut)
{
	xfuture* pFuture;
	xpop3fetchrawresult* pResult;

	if ( psRaw != NULL ) {
		*psRaw = NULL;
	}
	if ( pOut != NULL ) {
		xrtPop3ResultInit(pOut);
	}
	if ( psRaw == NULL ) {
		xpop3_result_error(pOut, "POP3 async raw output missing");
		return FALSE;
	}
	pFuture = xrtPop3FetchRawFuture(pCfg, iNumber, pOpts);
	if ( pFuture == NULL ) {
		xpop3_result_error(pOut, "POP3 async raw future create failed");
		return FALSE;
	}
	pResult = (xpop3fetchrawresult*)xFutureWaitValue(pFuture);
	if ( pResult == NULL ) {
		xfuture_result tResult;
		memset(&tResult, 0, sizeof(tResult));
		if ( xFutureGetResult(pFuture, &tResult) && tResult.pValue != NULL ) {
			pResult = (xpop3fetchrawresult*)tResult.pValue;
		}
	}
	if ( pResult == NULL ) {
		str sError = xFutureError(pFuture);
		xpop3_result_error(pOut, sError ? (const char*)sError : "POP3 async raw wait failed");
		xFutureRelease(pFuture);
		return FALSE;
	}
	if ( pOut != NULL ) {
		*pOut = pResult->tResult;
	}
	*psRaw = pResult->sRaw;
	pResult->sRaw = NULL;
	xrtPop3FetchRawResultFree(pResult);
	xFutureRelease(pFuture);
	return pOut ? pOut->bSuccess : TRUE;
}

static bool xrtPop3ListSummariesAsyncWait(const xpop3config* pCfg, const xpop3asyncopts* pOpts, xpop3summary** ppItems, size_t* pCount, xpop3result* pOut)
{
	xfuture* pFuture;
	xpop3summaryresult* pResult;

	if ( ppItems != NULL ) {
		*ppItems = NULL;
	}
	if ( pCount != NULL ) {
		*pCount = 0u;
	}
	if ( pOut != NULL ) {
		xrtPop3ResultInit(pOut);
	}
	if ( ppItems == NULL || pCount == NULL ) {
		xpop3_result_error(pOut, "POP3 async summary output missing");
		return FALSE;
	}
	pFuture = xrtPop3ListSummariesFuture(pCfg, pOpts);
	if ( pFuture == NULL ) {
		xpop3_result_error(pOut, "POP3 async summary future create failed");
		return FALSE;
	}
	pResult = (xpop3summaryresult*)xFutureWaitValue(pFuture);
	if ( pResult == NULL ) {
		xfuture_result tResult;
		memset(&tResult, 0, sizeof(tResult));
		if ( xFutureGetResult(pFuture, &tResult) && tResult.pValue != NULL ) {
			pResult = (xpop3summaryresult*)tResult.pValue;
		}
	}
	if ( pResult == NULL ) {
		str sError = xFutureError(pFuture);
		xpop3_result_error(pOut, sError ? (const char*)sError : "POP3 async summary wait failed");
		xFutureRelease(pFuture);
		return FALSE;
	}
	if ( pOut != NULL ) {
		*pOut = pResult->tResult;
	}
	*ppItems = pResult->arrItems;
	*pCount = pResult->iCount;
	pResult->arrItems = NULL;
	pResult->iCount = 0u;
	xrtPop3SummaryResultFree(pResult);
	xFutureRelease(pFuture);
	return pOut ? pOut->bSuccess : TRUE;
}

static bool xrtPop3TopRawAsyncWait(const xpop3config* pCfg, uint32 iNumber, uint32 iLineCount, const xpop3asyncopts* pOpts, str* psRaw, xpop3result* pOut)
{
	xfuture* pFuture;
	xpop3fetchrawresult* pResult;

	if ( psRaw != NULL ) {
		*psRaw = NULL;
	}
	if ( pOut != NULL ) {
		xrtPop3ResultInit(pOut);
	}
	if ( psRaw == NULL ) {
		xpop3_result_error(pOut, "POP3 async TOP raw output missing");
		return FALSE;
	}
	pFuture = xrtPop3TopRawFuture(pCfg, iNumber, iLineCount, pOpts);
	if ( pFuture == NULL ) {
		xpop3_result_error(pOut, "POP3 async TOP raw future create failed");
		return FALSE;
	}
	pResult = (xpop3fetchrawresult*)xFutureWaitValue(pFuture);
	if ( pResult == NULL ) {
		xfuture_result tResult;
		memset(&tResult, 0, sizeof(tResult));
		if ( xFutureGetResult(pFuture, &tResult) && tResult.pValue != NULL ) {
			pResult = (xpop3fetchrawresult*)tResult.pValue;
		}
	}
	if ( pResult == NULL ) {
		str sError = xFutureError(pFuture);
		xpop3_result_error(pOut, sError ? (const char*)sError : "POP3 async TOP raw wait failed");
		xFutureRelease(pFuture);
		return FALSE;
	}
	if ( pOut != NULL ) {
		*pOut = pResult->tResult;
	}
	*psRaw = pResult->sRaw;
	pResult->sRaw = NULL;
	xrtPop3FetchRawResultFree(pResult);
	xFutureRelease(pFuture);
	return pOut ? pOut->bSuccess : TRUE;
}

static bool xrtPop3FetchMimeAsyncWait(const xpop3config* pCfg, uint32 iNumber, const xpop3asyncopts* pOpts, xmailparsedmessage* pMsg, xpop3result* pOut)
{
	xfuture* pFuture;
	xpop3fetchmimeresult* pResult;

	if ( pMsg != NULL ) {
		memset(pMsg, 0, sizeof(*pMsg));
	}
	if ( pOut != NULL ) {
		xrtPop3ResultInit(pOut);
	}
	if ( pMsg == NULL ) {
		xpop3_result_error(pOut, "POP3 async MIME output missing");
		return FALSE;
	}
	pFuture = xrtPop3FetchMimeFuture(pCfg, iNumber, pOpts);
	if ( pFuture == NULL ) {
		xpop3_result_error(pOut, "POP3 async MIME future create failed");
		return FALSE;
	}
	pResult = (xpop3fetchmimeresult*)xFutureWaitValue(pFuture);
	if ( pResult == NULL ) {
		xfuture_result tResult;
		memset(&tResult, 0, sizeof(tResult));
		if ( xFutureGetResult(pFuture, &tResult) && tResult.pValue != NULL ) {
			pResult = (xpop3fetchmimeresult*)tResult.pValue;
		}
	}
	if ( pResult == NULL ) {
		str sError = xFutureError(pFuture);
		xpop3_result_error(pOut, sError ? (const char*)sError : "POP3 async MIME wait failed");
		xFutureRelease(pFuture);
		return FALSE;
	}
	if ( pOut != NULL ) {
		*pOut = pResult->tResult;
	}
	*pMsg = pResult->tMessage;
	memset(&pResult->tMessage, 0, sizeof(pResult->tMessage));
	xrtPop3FetchMimeResultFree(pResult);
	xFutureRelease(pFuture);
	return pOut ? pOut->bSuccess : TRUE;
}

static bool xrtPop3TopMimeAsyncWait(const xpop3config* pCfg, uint32 iNumber, uint32 iLineCount, const xpop3asyncopts* pOpts, xmailparsedmessage* pMsg, xpop3result* pOut)
{
	xfuture* pFuture;
	xpop3fetchmimeresult* pResult;

	if ( pMsg != NULL ) {
		memset(pMsg, 0, sizeof(*pMsg));
	}
	if ( pOut != NULL ) {
		xrtPop3ResultInit(pOut);
	}
	if ( pMsg == NULL ) {
		xpop3_result_error(pOut, "POP3 async TOP MIME output missing");
		return FALSE;
	}
	pFuture = xrtPop3TopMimeFuture(pCfg, iNumber, iLineCount, pOpts);
	if ( pFuture == NULL ) {
		xpop3_result_error(pOut, "POP3 async TOP MIME future create failed");
		return FALSE;
	}
	pResult = (xpop3fetchmimeresult*)xFutureWaitValue(pFuture);
	if ( pResult == NULL ) {
		xfuture_result tResult;
		memset(&tResult, 0, sizeof(tResult));
		if ( xFutureGetResult(pFuture, &tResult) && tResult.pValue != NULL ) {
			pResult = (xpop3fetchmimeresult*)tResult.pValue;
		}
	}
	if ( pResult == NULL ) {
		str sError = xFutureError(pFuture);
		xpop3_result_error(pOut, sError ? (const char*)sError : "POP3 async TOP MIME wait failed");
		xFutureRelease(pFuture);
		return FALSE;
	}
	if ( pOut != NULL ) {
		*pOut = pResult->tResult;
	}
	*pMsg = pResult->tMessage;
	memset(&pResult->tMessage, 0, sizeof(pResult->tMessage));
	xrtPop3FetchMimeResultFree(pResult);
	xFutureRelease(pFuture);
	return pOut ? pOut->bSuccess : TRUE;
}

#endif
