#ifndef XRT_EXTLIB_XIMAP_H
#define XRT_EXTLIB_XIMAP_H

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
#define XIMAP_SOCKET_INVALID INVALID_SOCKET
#define XIMAP_CLOSESOCK closesocket
#else
#define XIMAP_SOCKET_INVALID (-1)
#define XIMAP_CLOSESOCK close
#endif

#define XIMAP_SECURE_AUTO      0
#define XIMAP_SECURE_NONE      1
#define XIMAP_SECURE_SSL       2
#define XIMAP_SECURE_STARTTLS  3

#define XIMAP_STAGE_NONE       0
#define XIMAP_STAGE_CONNECT    1
#define XIMAP_STAGE_TLS        2
#define XIMAP_STAGE_GREETING   3
#define XIMAP_STAGE_CAPABILITY 4
#define XIMAP_STAGE_AUTH       5
#define XIMAP_STAGE_SELECT     6
#define XIMAP_STAGE_COMMAND    7
#define XIMAP_STAGE_FETCH      8
#define XIMAP_STAGE_LOGOUT     9
#define XIMAP_STAGE_IDLE       10

#define XIMAP_STATUS_UNKNOWN   0
#define XIMAP_STATUS_OK        1
#define XIMAP_STATUS_NO        2
#define XIMAP_STATUS_BAD       3
#define XIMAP_STATUS_BYE       4
#define XIMAP_STATUS_PREAUTH   5

#define XIMAP_CAP_IMAP4REV1    0x0001u
#define XIMAP_CAP_STARTTLS     0x0002u
#define XIMAP_CAP_IDLE         0x0004u
#define XIMAP_CAP_UIDPLUS      0x0008u
#define XIMAP_CAP_XOAUTH2      0x0010u

#define XIMAP_TLS_VERSION_DEFAULT 0
#define XIMAP_TLS_VERSION_1_2     0x0303u
#define XIMAP_TLS_VERSION_1_3     0x0304u

#define XIMAP_IDLE_LOOP_EVENT_START       "start"
#define XIMAP_IDLE_LOOP_EVENT_CYCLE_START "cycle_start"
#define XIMAP_IDLE_LOOP_EVENT_CYCLE_OK    "cycle_ok"
#define XIMAP_IDLE_LOOP_EVENT_CYCLE_ERROR "cycle_error"
#define XIMAP_IDLE_LOOP_EVENT_SLEEP       "sleep"
#define XIMAP_IDLE_LOOP_EVENT_STOP        "stop"
#define XIMAP_IDLE_LOOP_EVENT_CANCEL      "cancel"
#define XIMAP_IDLE_LOOP_EVENT_HEARTBEAT   "heartbeat"
#define XIMAP_IDLE_LOOP_EVENT_DONE        "done"

typedef struct ximapclient_struct ximapclient;
typedef struct ximapresult_struct ximapresult;
typedef bool (*ximapidlecallback)(const char* sEvent, void* pUser);
typedef bool (*ximapidleloopstopcallback)(void* pUser);
typedef void (*ximapidlelooptracecallback)(const char* sEvent, size_t iCycle, size_t iEvents, uint32 iDelayMs, const ximapresult* pRet, void* pUser);
typedef bool (*ximapidleloopheartbeatcallback)(size_t iCycle, size_t iEvents, const ximapresult* pRet, void* pUser);

typedef struct {
	const char* sHost;
	uint16 iPort;
	uint32 iTimeoutMs;
	int iSecureMode;
	bool bVerifyPeer;
	uint16 iTlsMaxVersion;
	const char* sCaFile;
	const char* sUser;
	const char* sPass;
	const char* sOAuth2Token;
} ximapconfig;

typedef struct {
	uint32 iTimeoutMs;
	const char* sDebugName;
	const char* sMailbox;
} ximapasyncopts;

typedef struct {
	const char* sMailbox;
	size_t iMaxEventsPerCycle;
	uint32 iMaxCycles;
	uint32 iReconnectDelayMs;
	uint32 iMaxReconnectDelayMs;
	uint32 iHeartbeatEveryCycles;
	ximapidleloopstopcallback pShouldStop;
	void* pStopUser;
	ximapidlelooptracecallback pTrace;
	void* pTraceUser;
	ximapidleloopheartbeatcallback pHeartbeat;
	void* pHeartbeatUser;
} ximapidleloopopts;

typedef struct ximapresult_struct {
	bool bSuccess;
	bool bUsedTLS;
	bool bUsedStartTLS;
	int iStage;
	int iStatus;
	uint32 iCapabilities;
	char sError[256];
	char sLastReply[1024];
} ximapresult;

typedef struct {
	size_t iCycles;
	size_t iEvents;
	size_t iHeartbeats;
	bool bCancelled;
	bool bStopped;
	bool bHeartbeatStopped;
	bool bLastCycleFailed;
} ximapidleloopsummary;

typedef struct {
	ximapresult tResult;
	str sRaw;
} ximapfetchrawresult;

typedef struct {
	ximapresult tResult;
	xmailparsedmessage tMessage;
} ximapfetchmimeresult;

typedef struct {
	char sTag[32];
	int iStatus;
	const char* sText;
} ximaptaggedline;

typedef struct {
	const char* sAtom;
	const char* sRest;
} ximapuntaggedline;

typedef struct {
	size_t iLiteralLen;
	bool bHasLiteral;
} ximapliteralmarker;

typedef struct {
	bool bSeen;
	bool bAnswered;
	bool bFlagged;
	bool bDeleted;
	bool bDraft;
	bool bRecent;
	char sRaw[256];
} ximapflags;

typedef struct {
	ximapresult tResult;
	ximapflags tFlags;
} ximapflagsresult;

typedef struct {
	uint64* arrIds;
	size_t iCount;
} ximapsearchids;

typedef struct {
	char sFlags[256];
	char sDelimiter[16];
	char sMailbox[512];
	bool bNoSelect;
	bool bHasNoChildren;
	bool bHasChildren;
	bool bMarked;
	bool bUnmarked;
} ximaplistitem;

typedef struct {
	ximaplistitem* arrItems;
	size_t iCount;
} ximaplist;

typedef struct {
	uint32 iExists;
	uint32 iRecent;
	uint32 iUnseen;
	uint64 iUidValidity;
	uint64 iUidNext;
	bool bReadOnly;
	bool bReadWrite;
} ximapmailboxstatus;

typedef struct {
	ximapresult tResult;
	ximapmailboxstatus tStatus;
} ximapstatusresult;

typedef struct {
	ximapresult tResult;
	ximaplist tList;
} ximaplistresult;

typedef struct {
	ximapresult tResult;
	ximapsearchids tIds;
} ximapsearchresult;

typedef struct {
	uint32* arrSequences;
	size_t iCount;
} ximapexpungeresult;

typedef struct {
	ximapresult tResult;
	ximapexpungeresult tExpunge;
} ximapexpungeasyncresult;

typedef struct {
	ximapresult tResult;
	str* arrEvents;
	size_t iCount;
} ximapidleresult;

typedef struct {
	ximapresult tResult;
	size_t iCount;
} ximapidlewatchresult;

typedef struct {
	char sName[64];
	char sValue[256];
} ximapparamelem;

typedef struct ximapbodystructure_struct {
	bool bMultipart;
	uint32 iPartCount;
	uint32 iParamCount;
	uint32 iDispositionParamCount;
	uint64 iOctets;
	uint32 iLines;
	char sType[64];
	char sSubType[64];
	char sId[128];
	char sDescription[256];
	char sEncoding[64];
	char sMd5[128];
	char sDisposition[64];
	char sLanguage[128];
	char sLocation[256];
	str sRaw;
	str sEnvelope;
	uint32 iMessageLines;
	ximapparamelem* arrParams;
	ximapparamelem* arrDispositionParams;
	struct ximapbodystructure_struct* pMessageBody;
	struct ximapbodystructure_struct* arrChildren;
} ximapbodystructure;

typedef struct {
	ximapresult tResult;
	ximapbodystructure tBody;
} ximapbodystructureresult;

typedef struct {
	char* sData;
	size_t iLen;
	size_t iCap;
} ximapbuf;

typedef struct ximappendingresponse_struct {
	char sTag[32];
	int iStatus;
	str sRaw;
	struct ximappendingresponse_struct* pNext;
} ximappendingresponse;

struct ximapclient_struct {
	xsocket hSock;
	xtlssession* pTls;
	uint32 iTimeoutMs;
	uint32 iTagCounter;
	ximappendingresponse* pPending;
};

typedef struct {
	ximapconfig tCfg;
	ximapasyncopts tOpts;
	char* sUid;
	char* sHost;
	char* sUser;
	char* sPass;
	char* sOAuth2Token;
	char* sCaFile;
	char* sDebugName;
	char* sMailbox;
	char* sReference;
	char* sPattern;
	char* sCriteria;
	char* sOperation;
	char* sFlags;
	char* sSection;
	char** arrFields;
	size_t iFieldCount;
	size_t iMaxEvents;
	ximapidlecallback pIdleCallback;
	void* pIdleUser;
	bool bParseMime;
	bool bExtractLiteral;
	bool bPeek;
	bool bUidSearch;
	bool bFetchFlags;
	bool bStoreFlags;
	bool bUidStore;
	bool bUidExpunge;
	bool bBodySection;
	bool bBodyStructure;
	bool bHeaderFields;
	bool bIdleWatch;
} ximapfetchtaskctx;

static bool ximap_buf_reserve(ximapbuf* pBuf, size_t iNeed);
static bool ximap_buf_append_n(ximapbuf* pBuf, const char* sText, size_t iLen);
static bool ximap_buf_append(ximapbuf* pBuf, const char* sText);
static void ximap_buf_free(ximapbuf* pBuf);
static bool ximap_uid_set_is_safe(const char* sUidSet);
static bool xrtImapLogout(ximapclient* pClient, ximapresult* pRet);

static const char* ximap_str_or_empty(const char* sText)
{
	return sText ? sText : "";
}

static void xrtImapConfigInit(ximapconfig* pCfg)
{
	if ( pCfg == NULL ) {
		return;
	}
	memset(pCfg, 0, sizeof(*pCfg));
	pCfg->iPort = 993;
	pCfg->iTimeoutMs = 15000u;
	pCfg->iSecureMode = XIMAP_SECURE_AUTO;
	pCfg->bVerifyPeer = TRUE;
	pCfg->iTlsMaxVersion = XIMAP_TLS_VERSION_DEFAULT;
}

static void xrtImapResultInit(ximapresult* pRet)
{
	if ( pRet == NULL ) {
		return;
	}
	memset(pRet, 0, sizeof(*pRet));
}

static void xrtImapAsyncOptsInit(ximapasyncopts* pOpts)
{
	if ( pOpts == NULL ) {
		return;
	}
	memset(pOpts, 0, sizeof(*pOpts));
}

static void xrtImapIdleLoopOptsInit(ximapidleloopopts* pOpts)
{
	if ( pOpts == NULL ) {
		return;
	}
	memset(pOpts, 0, sizeof(*pOpts));
	pOpts->iMaxEventsPerCycle = 0u;
	pOpts->iMaxCycles = 0u;
	pOpts->iReconnectDelayMs = 30000u;
	pOpts->iMaxReconnectDelayMs = 300000u;
}

static void xrtImapResultFree(ximapresult* pRet)
{
	if ( pRet != NULL ) {
		xrtFree(pRet);
	}
}

static void xrtImapFetchRawResultFree(ximapfetchrawresult* pResult)
{
	if ( pResult == NULL ) {
		return;
	}
	if ( pResult->sRaw != NULL ) {
		xrtFree(pResult->sRaw);
	}
	xrtFree(pResult);
}

static void xrtImapFetchMimeResultFree(ximapfetchmimeresult* pResult)
{
	if ( pResult == NULL ) {
		return;
	}
	xmailMimeParsedMessageFree(&pResult->tMessage);
	xrtFree(pResult);
}

static void xrtImapFlagsResultFree(ximapflagsresult* pResult)
{
	if ( pResult == NULL ) {
		return;
	}
	xrtFree(pResult);
}

static void xrtImapStatusResultFree(ximapstatusresult* pResult)
{
	if ( pResult == NULL ) {
		return;
	}
	xrtFree(pResult);
}

static void xrtImapListResultFree(ximaplistresult* pResult)
{
	if ( pResult == NULL ) {
		return;
	}
	if ( pResult->tList.arrItems != NULL ) {
		xrtFree(pResult->tList.arrItems);
	}
	xrtFree(pResult);
}

static void xrtImapSearchResultFree(ximapsearchresult* pResult)
{
	if ( pResult == NULL ) {
		return;
	}
	if ( pResult->tIds.arrIds != NULL ) {
		xrtFree(pResult->tIds.arrIds);
	}
	xrtFree(pResult);
}

static void xrtImapSearchIdsFree(ximapsearchids* pIds)
{
	if ( pIds == NULL ) {
		return;
	}
	if ( pIds->arrIds != NULL ) {
		xrtFree(pIds->arrIds);
	}
	memset(pIds, 0, sizeof(*pIds));
}

static void xrtImapListFree(ximaplist* pList)
{
	if ( pList == NULL ) {
		return;
	}
	if ( pList->arrItems != NULL ) {
		xrtFree(pList->arrItems);
	}
	memset(pList, 0, sizeof(*pList));
}

static void xrtImapExpungeResultFree(ximapexpungeresult* pResult)
{
	if ( pResult == NULL ) {
		return;
	}
	if ( pResult->arrSequences != NULL ) {
		xrtFree(pResult->arrSequences);
	}
	memset(pResult, 0, sizeof(*pResult));
}

static void xrtImapExpungeAsyncResultFree(ximapexpungeasyncresult* pResult)
{
	if ( pResult == NULL ) {
		return;
	}
	xrtImapExpungeResultFree(&pResult->tExpunge);
	xrtFree(pResult);
}

static void xrtImapIdleEventsFree(str* arrEvents, size_t iCount)
{
	size_t i;

	if ( arrEvents == NULL ) {
		return;
	}
	for ( i = 0u; i < iCount; i++ ) {
		if ( arrEvents[i] != NULL ) {
			xrtFree(arrEvents[i]);
		}
	}
	xrtFree(arrEvents);
}

static void xrtImapIdleResultFree(ximapidleresult* pResult)
{
	if ( pResult == NULL ) {
		return;
	}
	xrtImapIdleEventsFree(pResult->arrEvents, pResult->iCount);
	xrtFree(pResult);
}

static void xrtImapIdleWatchResultFree(ximapidlewatchresult* pResult)
{
	if ( pResult == NULL ) {
		return;
	}
	xrtFree(pResult);
}

static char* ximap_dup_text(const char* sText)
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

static void ximap_result_stage(ximapresult* pRet, int iStage)
{
	if ( pRet != NULL ) {
		pRet->iStage = iStage;
	}
}

static void ximap_result_error(ximapresult* pRet, const char* sError)
{
	if ( pRet == NULL ) {
		return;
	}
	pRet->bSuccess = FALSE;
	snprintf(pRet->sError, sizeof(pRet->sError), "%s", ximap_str_or_empty(sError));
}

static int ximap_socket_last_error(void)
{
#if defined(_WIN32) || defined(_WIN64)
	return WSAGetLastError();
#else
	return errno;
#endif
}

static void ximap_result_sys_error(ximapresult* pRet, const char* sError)
{
	if ( pRet == NULL ) {
		return;
	}
	pRet->bSuccess = FALSE;
	snprintf(pRet->sError, sizeof(pRet->sError), "%s (sys=%d)", ximap_str_or_empty(sError), ximap_socket_last_error());
}

static void ximap_result_tls_error(ximapresult* pRet, const char* sError, xnet_result iResult)
{
	if ( pRet == NULL ) {
		return;
	}
	pRet->bSuccess = FALSE;
	snprintf(pRet->sError, sizeof(pRet->sError), "%s (tls=%d)", ximap_str_or_empty(sError), (int)iResult);
}

static void ximap_result_errorf(ximapresult* pRet, const char* sFmt, ...)
{
	va_list ap;

	if ( pRet == NULL ) {
		return;
	}
	pRet->bSuccess = FALSE;
	va_start(ap, sFmt);
	vsnprintf(pRet->sError, sizeof(pRet->sError), ximap_str_or_empty(sFmt), ap);
	va_end(ap);
}

static bool ximap_buf_reserve(ximapbuf* pBuf, size_t iNeed)
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

static bool ximap_buf_append_n(ximapbuf* pBuf, const char* sText, size_t iLen)
{
	if ( pBuf == NULL || (sText == NULL && iLen != 0u) ) {
		return FALSE;
	}
	if ( !ximap_buf_reserve(pBuf, iLen) ) {
		return FALSE;
	}
	if ( iLen != 0u ) {
		memcpy(pBuf->sData + pBuf->iLen, sText, iLen);
	}
	pBuf->iLen += iLen;
	pBuf->sData[pBuf->iLen] = '\0';
	return TRUE;
}

static bool ximap_buf_append(ximapbuf* pBuf, const char* sText)
{
	return ximap_buf_append_n(pBuf, ximap_str_or_empty(sText), strlen(ximap_str_or_empty(sText)));
}

static void ximap_buf_free(ximapbuf* pBuf)
{
	if ( pBuf != NULL && pBuf->sData != NULL ) {
		xrtFree(pBuf->sData);
		pBuf->sData = NULL;
		pBuf->iLen = 0u;
		pBuf->iCap = 0u;
	}
}

static bool ximap_ascii_eq_i(const char* a, const char* b)
{
	size_t i;

	if ( a == NULL || b == NULL ) {
		return FALSE;
	}
	for ( i = 0u; a[i] != '\0' && b[i] != '\0'; i++ ) {
		if ( tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]) ) {
			return FALSE;
		}
	}
	return a[i] == '\0' && b[i] == '\0';
}

static bool ximap_starts_with_i(const char* sText, const char* sPrefix)
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

static void ximap_pending_free_all(ximapclient* pClient)
{
	ximappendingresponse* pCur;
	if ( pClient == NULL ) {
		return;
	}
	pCur = pClient->pPending;
	while ( pCur != NULL ) {
		ximappendingresponse* pNext = pCur->pNext;
		if ( pCur->sRaw != NULL ) {
			xrtFree(pCur->sRaw);
		}
		xrtFree(pCur);
		pCur = pNext;
	}
	pClient->pPending = NULL;
}

static bool ximap_pending_take(ximapclient* pClient, const char* sTag, str* psOut, ximapresult* pRet)
{
	ximappendingresponse* pPrev = NULL;
	ximappendingresponse* pCur;
	if ( pClient == NULL || sTag == NULL ) {
		return FALSE;
	}
	pCur = pClient->pPending;
	while ( pCur != NULL ) {
		if ( ximap_ascii_eq_i(pCur->sTag, sTag) ) {
			if ( pPrev != NULL ) {
				pPrev->pNext = pCur->pNext;
			} else {
				pClient->pPending = pCur->pNext;
			}
			if ( pRet != NULL ) {
				pRet->iStatus = pCur->iStatus;
				snprintf(pRet->sLastReply, sizeof(pRet->sLastReply), "%s", pCur->sRaw ? (const char*)pCur->sRaw : "");
			}
			if ( psOut != NULL ) {
				*psOut = pCur->sRaw;
			} else if ( pCur->sRaw != NULL ) {
				xrtFree(pCur->sRaw);
			}
			pCur->sRaw = NULL;
			xrtFree(pCur);
			return TRUE;
		}
		pPrev = pCur;
		pCur = pCur->pNext;
	}
	return FALSE;
}

static bool ximap_pending_store(ximapclient* pClient, const char* sTag, int iStatus, str sRaw, ximapresult* pRet)
{
	ximappendingresponse* pNode;
	size_t iLen;
	if ( pClient == NULL || sTag == NULL || sRaw == NULL ) {
		if ( sRaw != NULL ) {
			xrtFree(sRaw);
		}
		return FALSE;
	}
	pNode = (ximappendingresponse*)xrtCalloc(1, sizeof(*pNode));
	if ( pNode == NULL ) {
		xrtFree(sRaw);
		ximap_result_error(pRet, "IMAP pending response alloc failed");
		return FALSE;
	}
	iLen = strlen(sTag);
	if ( iLen >= sizeof(pNode->sTag) ) {
		iLen = sizeof(pNode->sTag) - 1u;
	}
	memcpy(pNode->sTag, sTag, iLen);
	pNode->sTag[iLen] = '\0';
	pNode->iStatus = iStatus;
	pNode->sRaw = sRaw;
	pNode->pNext = pClient->pPending;
	pClient->pPending = pNode;
	return TRUE;
}

static int ximap_parse_status_atom(const char* sText)
{
	if ( ximap_ascii_eq_i(sText, "OK") ) return XIMAP_STATUS_OK;
	if ( ximap_ascii_eq_i(sText, "NO") ) return XIMAP_STATUS_NO;
	if ( ximap_ascii_eq_i(sText, "BAD") ) return XIMAP_STATUS_BAD;
	if ( ximap_ascii_eq_i(sText, "BYE") ) return XIMAP_STATUS_BYE;
	if ( ximap_ascii_eq_i(sText, "PREAUTH") ) return XIMAP_STATUS_PREAUTH;
	return XIMAP_STATUS_UNKNOWN;
}

static str ximap_next_tag(uint32* pCounter)
{
	uint32 iValue;
	char sBuf[32];

	if ( pCounter == NULL ) {
		return NULL;
	}
	(*pCounter)++;
	iValue = *pCounter;
	snprintf(sBuf, sizeof(sBuf), "A%04u", (unsigned)iValue);
	return xrtCopyStr((str)sBuf, 0);
}

static str ximap_build_tagged_command(uint32* pCounter, const char* sCommand)
{
	str sTag;
	str sOut;

	sTag = ximap_next_tag(pCounter);
	if ( sTag == NULL ) {
		return NULL;
	}
	sOut = xrtFormat("%s %s", (const char*)sTag, ximap_str_or_empty(sCommand));
	xrtFree(sTag);
	return sOut;
}

static bool ximap_get_tag_from_command(const char* sCommand, char* sTag, size_t iTagCap)
{
	const char* sEnd;
	size_t iLen;

	if ( sCommand == NULL || sTag == NULL || iTagCap == 0u ) {
		return FALSE;
	}
	sEnd = strchr(sCommand, ' ');
	if ( sEnd == NULL || sEnd == sCommand ) {
		return FALSE;
	}
	iLen = (size_t)(sEnd - sCommand);
	if ( iLen >= iTagCap ) {
		iLen = iTagCap - 1u;
	}
	memcpy(sTag, sCommand, iLen);
	sTag[iLen] = '\0';
	return TRUE;
}

static bool ximap_parse_tagged_line(const char* sLine, ximaptaggedline* pOut)
{
	const char* p;
	const char* sTagEnd;
	const char* sStatusEnd;
	char sStatus[16];
	size_t iTagLen;
	size_t iStatusLen;

	if ( sLine == NULL || pOut == NULL || sLine[0] == '*' || sLine[0] == '+' ) {
		return FALSE;
	}
	memset(pOut, 0, sizeof(*pOut));
	sTagEnd = strchr(sLine, ' ');
	if ( sTagEnd == NULL || sTagEnd == sLine ) {
		return FALSE;
	}
	iTagLen = (size_t)(sTagEnd - sLine);
	if ( iTagLen >= sizeof(pOut->sTag) ) {
		iTagLen = sizeof(pOut->sTag) - 1u;
	}
	memcpy(pOut->sTag, sLine, iTagLen);
	pOut->sTag[iTagLen] = '\0';
	p = sTagEnd + 1;
	while ( *p && isspace((unsigned char)*p) ) {
		p++;
	}
	sStatusEnd = p;
	while ( *sStatusEnd && !isspace((unsigned char)*sStatusEnd) ) {
		sStatusEnd++;
	}
	iStatusLen = (size_t)(sStatusEnd - p);
	if ( iStatusLen == 0u || iStatusLen >= sizeof(sStatus) ) {
		return FALSE;
	}
	memcpy(sStatus, p, iStatusLen);
	sStatus[iStatusLen] = '\0';
	pOut->iStatus = ximap_parse_status_atom(sStatus);
	while ( *sStatusEnd && isspace((unsigned char)*sStatusEnd) ) {
		sStatusEnd++;
	}
	pOut->sText = sStatusEnd;
	return pOut->iStatus != XIMAP_STATUS_UNKNOWN;
}

static bool ximap_parse_untagged_line(const char* sLine, ximapuntaggedline* pOut)
{
	const char* p;
	const char* sAtomStart;
	const char* sAtomEnd;

	if ( sLine == NULL || pOut == NULL || sLine[0] != '*' ) {
		return FALSE;
	}
	memset(pOut, 0, sizeof(*pOut));
	p = sLine + 1;
	while ( *p && isspace((unsigned char)*p) ) {
		p++;
	}
	while ( *p && isdigit((unsigned char)*p) ) {
		p++;
	}
	while ( *p && isspace((unsigned char)*p) ) {
		p++;
	}
	sAtomStart = p;
	while ( *p && !isspace((unsigned char)*p) ) {
		p++;
	}
	sAtomEnd = p;
	if ( sAtomEnd == sAtomStart ) {
		return FALSE;
	}
	pOut->sAtom = sAtomStart;
	while ( *p && isspace((unsigned char)*p) ) {
		p++;
	}
	pOut->sRest = p;
	return TRUE;
}

static bool ximap_line_is_continuation(const char* sLine)
{
	return sLine != NULL && sLine[0] == '+';
}

static bool ximap_parse_size_token(const char* sStart, const char* sEnd, size_t* pValue)
{
	const char* p;
	size_t iValue = 0u;
	bool bAny = FALSE;

	if ( sStart == NULL || sEnd == NULL || sStart > sEnd || pValue == NULL ) {
		return FALSE;
	}
	for ( p = sStart; p < sEnd; p++ ) {
		if ( *p < '0' || *p > '9' ) {
			return FALSE;
		}
		iValue = (iValue * 10u) + (size_t)(*p - '0');
		bAny = TRUE;
	}
	if ( !bAny ) {
		return FALSE;
	}
	*pValue = iValue;
	return TRUE;
}

static bool ximap_parse_literal_marker(const char* sLine, ximapliteralmarker* pOut)
{
	const char* pEnd;
	const char* pStart;
	size_t iLen;

	if ( pOut == NULL ) {
		return FALSE;
	}
	memset(pOut, 0, sizeof(*pOut));
	if ( sLine == NULL ) {
		return FALSE;
	}
	pEnd = strrchr(sLine, '}');
	if ( pEnd == NULL || pEnd == sLine ) {
		return FALSE;
	}
	pStart = pEnd;
	while ( pStart > sLine && pStart[-1] != '{' ) {
		pStart--;
	}
	if ( pStart == sLine || pStart[-1] != '{' ) {
		return FALSE;
	}
	if ( !ximap_parse_size_token(pStart, pEnd, &iLen) ) {
		return FALSE;
	}
	pOut->iLiteralLen = iLen;
	pOut->bHasLiteral = TRUE;
	return TRUE;
}

static void ximap_parse_capability_line(const char* sLine, uint32* pCaps)
{
	const char* p;

	if ( sLine == NULL || pCaps == NULL ) {
		return;
	}
	p = sLine;
	if ( ximap_starts_with_i(p, "* CAPABILITY") ) {
		p += strlen("* CAPABILITY");
	}
	while ( *p ) {
		while ( *p && isspace((unsigned char)*p) ) {
			p++;
		}
		if ( ximap_starts_with_i(p, "IMAP4rev1") ) *pCaps |= XIMAP_CAP_IMAP4REV1;
		else if ( ximap_starts_with_i(p, "STARTTLS") ) *pCaps |= XIMAP_CAP_STARTTLS;
		else if ( ximap_starts_with_i(p, "IDLE") ) *pCaps |= XIMAP_CAP_IDLE;
		else if ( ximap_starts_with_i(p, "UIDPLUS") ) *pCaps |= XIMAP_CAP_UIDPLUS;
		else if ( ximap_starts_with_i(p, "AUTH=XOAUTH2") ) *pCaps |= XIMAP_CAP_XOAUTH2;
		while ( *p && !isspace((unsigned char)*p) ) {
			p++;
		}
	}
}

static bool ximap_flag_token_eq(const char* sToken, size_t iLen, const char* sFlag)
{
	size_t i;

	if ( sToken == NULL || sFlag == NULL || strlen(sFlag) != iLen ) {
		return FALSE;
	}
	for ( i = 0u; i < iLen; i++ ) {
		if ( tolower((unsigned char)sToken[i]) != tolower((unsigned char)sFlag[i]) ) {
			return FALSE;
		}
	}
	return TRUE;
}

static bool ximap_parse_flags_list(const char* sText, ximapflags* pFlags)
{
	const char* pStart;
	const char* pEnd;
	const char* p;
	size_t iRawLen;

	if ( sText == NULL || pFlags == NULL ) {
		return FALSE;
	}
	memset(pFlags, 0, sizeof(*pFlags));
	pStart = strstr(sText, "FLAGS (");
	if ( pStart == NULL ) {
		pStart = strstr(sText, "flags (");
	}
	if ( pStart == NULL ) {
		return FALSE;
	}
	pStart += strlen("FLAGS (");
	pEnd = strchr(pStart, ')');
	if ( pEnd == NULL || pEnd < pStart ) {
		return FALSE;
	}
	iRawLen = (size_t)(pEnd - pStart);
	if ( iRawLen >= sizeof(pFlags->sRaw) ) {
		iRawLen = sizeof(pFlags->sRaw) - 1u;
	}
	memcpy(pFlags->sRaw, pStart, iRawLen);
	pFlags->sRaw[iRawLen] = '\0';
	p = pStart;
	while ( p < pEnd ) {
		const char* sTokenStart;
		size_t iTokenLen;
		while ( p < pEnd && isspace((unsigned char)*p) ) {
			p++;
		}
		sTokenStart = p;
		while ( p < pEnd && !isspace((unsigned char)*p) ) {
			p++;
		}
		iTokenLen = (size_t)(p - sTokenStart);
		if ( iTokenLen != 0u ) {
			if ( ximap_flag_token_eq(sTokenStart, iTokenLen, "\\Seen") ) pFlags->bSeen = TRUE;
			else if ( ximap_flag_token_eq(sTokenStart, iTokenLen, "\\Answered") ) pFlags->bAnswered = TRUE;
			else if ( ximap_flag_token_eq(sTokenStart, iTokenLen, "\\Flagged") ) pFlags->bFlagged = TRUE;
			else if ( ximap_flag_token_eq(sTokenStart, iTokenLen, "\\Deleted") ) pFlags->bDeleted = TRUE;
			else if ( ximap_flag_token_eq(sTokenStart, iTokenLen, "\\Draft") ) pFlags->bDraft = TRUE;
			else if ( ximap_flag_token_eq(sTokenStart, iTokenLen, "\\Recent") ) pFlags->bRecent = TRUE;
		}
	}
	return TRUE;
}

static bool ximap_parse_fetch_flags_response(const char* sRaw, ximapflags* pFlags)
{
	const char* p;
	const char* sLineEnd;

	if ( sRaw == NULL || pFlags == NULL ) {
		return FALSE;
	}
	p = sRaw;
	while ( *p ) {
		sLineEnd = strstr(p, "\r\n");
		if ( sLineEnd == NULL ) {
			return ximap_parse_flags_list(p, pFlags);
		}
		{
			size_t iLineLen = (size_t)(sLineEnd - p);
			char* sLine = (char*)xrtMalloc(iLineLen + 1u);
			bool bParsed;
			if ( sLine == NULL ) {
				return FALSE;
			}
			memcpy(sLine, p, iLineLen);
			sLine[iLineLen] = '\0';
			bParsed = strstr(sLine, " FETCH ") != NULL && ximap_parse_flags_list(sLine, pFlags);
			xrtFree(sLine);
			if ( bParsed ) {
				return TRUE;
			}
		}
		p = sLineEnd + 2;
	}
	return FALSE;
}

static bool ximap_search_ids_append(ximapsearchids* pIds, uint64 iValue)
{
	uint64* arrNew;
	size_t iNewCount;

	if ( pIds == NULL ) {
		return FALSE;
	}
	iNewCount = pIds->iCount + 1u;
	arrNew = (uint64*)xrtRealloc(pIds->arrIds, sizeof(uint64) * iNewCount);
	if ( arrNew == NULL ) {
		return FALSE;
	}
	pIds->arrIds = arrNew;
	pIds->arrIds[pIds->iCount] = iValue;
	pIds->iCount = iNewCount;
	return TRUE;
}

static bool ximap_parse_u64_token(const char* sStart, const char* sEnd, uint64* pValue)
{
	const char* p;
	uint64 iValue = 0u;
	bool bAny = FALSE;

	if ( sStart == NULL || sEnd == NULL || sStart >= sEnd || pValue == NULL ) {
		return FALSE;
	}
	for ( p = sStart; p < sEnd; p++ ) {
		if ( *p < '0' || *p > '9' ) {
			return FALSE;
		}
		iValue = (iValue * 10u) + (uint64)(*p - '0');
		bAny = TRUE;
	}
	if ( !bAny ) {
		return FALSE;
	}
	*pValue = iValue;
	return TRUE;
}

static bool ximap_parse_search_line(const char* sLine, ximapsearchids* pIds)
{
	const char* p;

	if ( sLine == NULL || pIds == NULL || !ximap_starts_with_i(sLine, "* SEARCH") ) {
		return FALSE;
	}
	p = sLine + strlen("* SEARCH");
	while ( *p != '\0' ) {
		const char* sTokenStart;
		uint64 iValue;

		while ( *p != '\0' && isspace((unsigned char)*p) ) {
			p++;
		}
		if ( *p == '\0' ) {
			break;
		}
		sTokenStart = p;
		while ( *p != '\0' && !isspace((unsigned char)*p) ) {
			p++;
		}
		if ( !ximap_parse_u64_token(sTokenStart, p, &iValue) || !ximap_search_ids_append(pIds, iValue) ) {
			return FALSE;
		}
	}
	return TRUE;
}

static bool ximap_parse_search_response(const char* sRaw, ximapsearchids* pIds)
{
	const char* p;
	bool bSawSearch = FALSE;

	if ( sRaw == NULL || pIds == NULL ) {
		return FALSE;
	}
	memset(pIds, 0, sizeof(*pIds));
	p = sRaw;
	while ( *p != '\0' ) {
		const char* sLineEnd = strstr(p, "\r\n");
		size_t iLineLen = sLineEnd ? (size_t)(sLineEnd - p) : strlen(p);
		char sLine[1024];

		if ( iLineLen >= sizeof(sLine) ) {
			xrtImapSearchIdsFree(pIds);
			return FALSE;
		}
		memcpy(sLine, p, iLineLen);
		sLine[iLineLen] = '\0';
		if ( ximap_starts_with_i(sLine, "* SEARCH") ) {
			bSawSearch = TRUE;
			if ( !ximap_parse_search_line(sLine, pIds) ) {
				xrtImapSearchIdsFree(pIds);
				return FALSE;
			}
		}
		if ( sLineEnd == NULL ) {
			break;
		}
		p = sLineEnd + 2;
	}
	return bSawSearch;
}

static void ximap_list_parse_flags(ximaplistitem* pItem)
{
	if ( pItem == NULL ) {
		return;
	}
	pItem->bNoSelect = strstr(pItem->sFlags, "\\Noselect") != NULL || strstr(pItem->sFlags, "\\NoSelect") != NULL;
	pItem->bHasNoChildren = strstr(pItem->sFlags, "\\HasNoChildren") != NULL;
	pItem->bHasChildren = strstr(pItem->sFlags, "\\HasChildren") != NULL;
	pItem->bMarked = strstr(pItem->sFlags, "\\Marked") != NULL;
	pItem->bUnmarked = strstr(pItem->sFlags, "\\Unmarked") != NULL;
}

static bool ximap_list_append(ximaplist* pList, const ximaplistitem* pItem)
{
	ximaplistitem* arrNew;
	size_t iNewCount;

	if ( pList == NULL || pItem == NULL ) {
		return FALSE;
	}
	iNewCount = pList->iCount + 1u;
	arrNew = (ximaplistitem*)xrtRealloc(pList->arrItems, sizeof(ximaplistitem) * iNewCount);
	if ( arrNew == NULL ) {
		return FALSE;
	}
	pList->arrItems = arrNew;
	pList->arrItems[pList->iCount] = *pItem;
	pList->iCount = iNewCount;
	return TRUE;
}

static const char* ximap_list_parse_quoted_or_nil(const char* p, char* sOut, size_t iOutCap)
{
	size_t iLen = 0u;

	if ( p == NULL || sOut == NULL || iOutCap == 0u ) {
		return NULL;
	}
	sOut[0] = '\0';
	while ( *p != '\0' && isspace((unsigned char)*p) ) {
		p++;
	}
	if ( ximap_starts_with_i(p, "NIL") && (p[3] == '\0' || isspace((unsigned char)p[3])) ) {
		return p + 3;
	}
	if ( *p != '"' ) {
		return NULL;
	}
	p++;
	while ( *p != '\0' && *p != '"' ) {
		char ch = *p++;
		if ( ch == '\\' && *p != '\0' ) {
			ch = *p++;
		}
		if ( iLen + 1u >= iOutCap ) {
			return NULL;
		}
		sOut[iLen++] = ch;
	}
	if ( *p != '"' ) {
		return NULL;
	}
	sOut[iLen] = '\0';
	return p + 1;
}

static bool ximap_parse_list_line(const char* sLine, ximaplistitem* pItem)
{
	const char* p;
	const char* sFlagsStart;
	const char* sFlagsEnd;
	size_t iFlagsLen;

	if ( sLine == NULL || pItem == NULL || !ximap_starts_with_i(sLine, "* LIST") ) {
		return FALSE;
	}
	memset(pItem, 0, sizeof(*pItem));
	p = sLine + strlen("* LIST");
	while ( *p != '\0' && isspace((unsigned char)*p) ) {
		p++;
	}
	if ( *p != '(' ) {
		return FALSE;
	}
	sFlagsStart = p + 1;
	sFlagsEnd = strchr(sFlagsStart, ')');
	if ( sFlagsEnd == NULL ) {
		return FALSE;
	}
	iFlagsLen = (size_t)(sFlagsEnd - sFlagsStart);
	if ( iFlagsLen >= sizeof(pItem->sFlags) ) {
		return FALSE;
	}
	memcpy(pItem->sFlags, sFlagsStart, iFlagsLen);
	pItem->sFlags[iFlagsLen] = '\0';
	p = ximap_list_parse_quoted_or_nil(sFlagsEnd + 1, pItem->sDelimiter, sizeof(pItem->sDelimiter));
	if ( p == NULL ) {
		return FALSE;
	}
	p = ximap_list_parse_quoted_or_nil(p, pItem->sMailbox, sizeof(pItem->sMailbox));
	if ( p == NULL || pItem->sMailbox[0] == '\0' ) {
		return FALSE;
	}
	ximap_list_parse_flags(pItem);
	return TRUE;
}

static bool ximap_parse_list_response(const char* sRaw, ximaplist* pList)
{
	const char* p;
	bool bSawList = FALSE;

	if ( sRaw == NULL || pList == NULL ) {
		return FALSE;
	}
	memset(pList, 0, sizeof(*pList));
	p = sRaw;
	while ( *p != '\0' ) {
		const char* sLineEnd = strstr(p, "\r\n");
		size_t iLineLen = sLineEnd ? (size_t)(sLineEnd - p) : strlen(p);
		char sLine[1024];

		if ( iLineLen >= sizeof(sLine) ) {
			xrtImapListFree(pList);
			return FALSE;
		}
		memcpy(sLine, p, iLineLen);
		sLine[iLineLen] = '\0';
		if ( ximap_starts_with_i(sLine, "* LIST") ) {
			ximaplistitem tItem;
			bSawList = TRUE;
			if ( !ximap_parse_list_line(sLine, &tItem) || !ximap_list_append(pList, &tItem) ) {
				xrtImapListFree(pList);
				return FALSE;
			}
		}
		if ( sLineEnd == NULL ) {
			break;
		}
		p = sLineEnd + 2;
	}
	return bSawList;
}

static bool ximap_parse_mailbox_count_line(const char* sLine, const char* sName, uint32* pOut)
{
	const char* p;
	const char* sNumStart;
	uint64 iValue;

	if ( sLine == NULL || sName == NULL || pOut == NULL || sLine[0] != '*' || !isspace((unsigned char)sLine[1]) ) {
		return FALSE;
	}
	p = sLine + 2;
	sNumStart = p;
	while ( *p != '\0' && !isspace((unsigned char)*p) ) {
		p++;
	}
	if ( !ximap_parse_u64_token(sNumStart, p, &iValue) || iValue > 0xffffffffu ) {
		return FALSE;
	}
	while ( *p != '\0' && isspace((unsigned char)*p) ) {
		p++;
	}
	if ( !ximap_starts_with_i(p, sName) ) {
		return FALSE;
	}
	*pOut = (uint32)iValue;
	return TRUE;
}

static bool ximap_parse_ok_response_code_u64(const char* sLine, const char* sCode, uint64* pOut)
{
	const char* p;
	const char* sValueStart;

	if ( sLine == NULL || sCode == NULL || pOut == NULL ) {
		return FALSE;
	}
	p = strchr(sLine, '[');
	while ( p != NULL ) {
		p++;
		if ( ximap_starts_with_i(p, sCode) && isspace((unsigned char)p[strlen(sCode)]) ) {
			p += strlen(sCode);
			while ( *p != '\0' && isspace((unsigned char)*p) ) {
				p++;
			}
			sValueStart = p;
			while ( *p != '\0' && *p != ']' && !isspace((unsigned char)*p) ) {
				p++;
			}
			return ximap_parse_u64_token(sValueStart, p, pOut);
		}
		p = strchr(p, '[');
	}
	return FALSE;
}

static bool ximap_parse_mailbox_status_response(const char* sRaw, ximapmailboxstatus* pStatus)
{
	const char* p;
	bool bSawTaggedStatus = FALSE;

	if ( sRaw == NULL || pStatus == NULL ) {
		return FALSE;
	}
	memset(pStatus, 0, sizeof(*pStatus));
	p = sRaw;
	while ( *p != '\0' ) {
		const char* sLineEnd = strstr(p, "\r\n");
		size_t iLineLen = sLineEnd ? (size_t)(sLineEnd - p) : strlen(p);
		char sLine[1024];
		uint64 iValue;

		if ( iLineLen >= sizeof(sLine) ) {
			return FALSE;
		}
		memcpy(sLine, p, iLineLen);
		sLine[iLineLen] = '\0';
		(void)ximap_parse_mailbox_count_line(sLine, "EXISTS", &pStatus->iExists);
		(void)ximap_parse_mailbox_count_line(sLine, "RECENT", &pStatus->iRecent);
		if ( ximap_parse_ok_response_code_u64(sLine, "UIDVALIDITY", &iValue) ) {
			pStatus->iUidValidity = iValue;
		}
		if ( ximap_parse_ok_response_code_u64(sLine, "UIDNEXT", &iValue) ) {
			pStatus->iUidNext = iValue;
		}
		if ( strstr(sLine, "[READ-ONLY]") != NULL || strstr(sLine, "[read-only]") != NULL ) {
			pStatus->bReadOnly = TRUE;
			bSawTaggedStatus = TRUE;
		}
		if ( strstr(sLine, "[READ-WRITE]") != NULL || strstr(sLine, "[read-write]") != NULL ) {
			pStatus->bReadWrite = TRUE;
			bSawTaggedStatus = TRUE;
		}
		if ( sLineEnd == NULL ) {
			break;
		}
		p = sLineEnd + 2;
	}
	return bSawTaggedStatus || pStatus->iExists != 0u || pStatus->iRecent != 0u
		|| pStatus->iUidValidity != 0u || pStatus->iUidNext != 0u;
}

static bool ximap_status_token_eq(const char* sToken, size_t iLen, const char* sName)
{
	size_t i;

	if ( sToken == NULL || sName == NULL || strlen(sName) != iLen ) {
		return FALSE;
	}
	for ( i = 0u; i < iLen; i++ ) {
		if ( tolower((unsigned char)sToken[i]) != tolower((unsigned char)sName[i]) ) {
			return FALSE;
		}
	}
	return TRUE;
}

static bool ximap_parse_status_list(const char* sStart, const char* sEnd, ximapmailboxstatus* pStatus)
{
	const char* p;

	if ( sStart == NULL || sEnd == NULL || sStart > sEnd || pStatus == NULL ) {
		return FALSE;
	}
	p = sStart;
	while ( p < sEnd ) {
		const char* sNameStart;
		const char* sValueStart;
		size_t iNameLen;
		uint64 iValue;

		while ( p < sEnd && isspace((unsigned char)*p) ) {
			p++;
		}
		if ( p >= sEnd ) {
			break;
		}
		sNameStart = p;
		while ( p < sEnd && !isspace((unsigned char)*p) ) {
			p++;
		}
		iNameLen = (size_t)(p - sNameStart);
		while ( p < sEnd && isspace((unsigned char)*p) ) {
			p++;
		}
		sValueStart = p;
		while ( p < sEnd && !isspace((unsigned char)*p) ) {
			p++;
		}
		if ( !ximap_parse_u64_token(sValueStart, p, &iValue) ) {
			return FALSE;
		}
		if ( ximap_status_token_eq(sNameStart, iNameLen, "MESSAGES") ) {
			if ( iValue > 0xffffffffu ) return FALSE;
			pStatus->iExists = (uint32)iValue;
		} else if ( ximap_status_token_eq(sNameStart, iNameLen, "RECENT") ) {
			if ( iValue > 0xffffffffu ) return FALSE;
			pStatus->iRecent = (uint32)iValue;
		} else if ( ximap_status_token_eq(sNameStart, iNameLen, "UNSEEN") ) {
			if ( iValue > 0xffffffffu ) return FALSE;
			pStatus->iUnseen = (uint32)iValue;
		} else if ( ximap_status_token_eq(sNameStart, iNameLen, "UIDVALIDITY") ) {
			pStatus->iUidValidity = iValue;
		} else if ( ximap_status_token_eq(sNameStart, iNameLen, "UIDNEXT") ) {
			pStatus->iUidNext = iValue;
		}
	}
	return TRUE;
}

static bool ximap_parse_status_response(const char* sRaw, ximapmailboxstatus* pStatus)
{
	const char* p;
	bool bSawStatus = FALSE;

	if ( sRaw == NULL || pStatus == NULL ) {
		return FALSE;
	}
	memset(pStatus, 0, sizeof(*pStatus));
	p = sRaw;
	while ( *p != '\0' ) {
		const char* sLineEnd = strstr(p, "\r\n");
		size_t iLineLen = sLineEnd ? (size_t)(sLineEnd - p) : strlen(p);
		char sLine[1024];

		if ( iLineLen >= sizeof(sLine) ) {
			return FALSE;
		}
		memcpy(sLine, p, iLineLen);
		sLine[iLineLen] = '\0';
		if ( ximap_starts_with_i(sLine, "* STATUS") ) {
			const char* sOpen = strchr(sLine, '(');
			const char* sClose = strrchr(sLine, ')');
			bSawStatus = TRUE;
			if ( sOpen == NULL || sClose == NULL || sClose < sOpen || !ximap_parse_status_list(sOpen + 1, sClose, pStatus) ) {
				return FALSE;
			}
		}
		if ( sLineEnd == NULL ) {
			break;
		}
		p = sLineEnd + 2;
	}
	return bSawStatus;
}

static bool ximap_expunge_result_append(ximapexpungeresult* pResult, uint32 iSequence)
{
	uint32* arrNew;
	size_t iNewCount;

	if ( pResult == NULL ) {
		return FALSE;
	}
	iNewCount = pResult->iCount + 1u;
	arrNew = (uint32*)xrtRealloc(pResult->arrSequences, sizeof(uint32) * iNewCount);
	if ( arrNew == NULL ) {
		return FALSE;
	}
	pResult->arrSequences = arrNew;
	pResult->arrSequences[pResult->iCount] = iSequence;
	pResult->iCount = iNewCount;
	return TRUE;
}

static bool ximap_parse_expunge_line(const char* sLine, uint32* pSequence)
{
	const char* p;
	const char* sNumStart;
	uint64 iValue;

	if ( sLine == NULL || pSequence == NULL || sLine[0] != '*' || !isspace((unsigned char)sLine[1]) ) {
		return FALSE;
	}
	p = sLine + 2;
	sNumStart = p;
	while ( *p != '\0' && !isspace((unsigned char)*p) ) {
		p++;
	}
	if ( !ximap_parse_u64_token(sNumStart, p, &iValue) || iValue == 0u || iValue > 0xffffffffu ) {
		return FALSE;
	}
	while ( *p != '\0' && isspace((unsigned char)*p) ) {
		p++;
	}
	if ( !ximap_starts_with_i(p, "EXPUNGE") ) {
		return FALSE;
	}
	*pSequence = (uint32)iValue;
	return TRUE;
}

static bool ximap_parse_expunge_response(const char* sRaw, ximapexpungeresult* pResult)
{
	const char* p;
	bool bSawExpunge = FALSE;

	if ( sRaw == NULL || pResult == NULL ) {
		return FALSE;
	}
	memset(pResult, 0, sizeof(*pResult));
	p = sRaw;
	while ( *p != '\0' ) {
		const char* sLineEnd = strstr(p, "\r\n");
		size_t iLineLen = sLineEnd ? (size_t)(sLineEnd - p) : strlen(p);
		char sLine[1024];
		uint32 iSequence;

		if ( iLineLen >= sizeof(sLine) ) {
			xrtImapExpungeResultFree(pResult);
			return FALSE;
		}
		memcpy(sLine, p, iLineLen);
		sLine[iLineLen] = '\0';
		if ( ximap_parse_expunge_line(sLine, &iSequence) ) {
			bSawExpunge = TRUE;
			if ( !ximap_expunge_result_append(pResult, iSequence) ) {
				xrtImapExpungeResultFree(pResult);
				return FALSE;
			}
		}
		if ( sLineEnd == NULL ) {
			break;
		}
		p = sLineEnd + 2;
	}
	return bSawExpunge;
}

static void xrtImapBodyStructureFree(ximapbodystructure* pBody)
{
	uint32 i;

	if ( pBody == NULL ) {
		return;
	}
	if ( pBody->sRaw != NULL ) {
		xrtFree(pBody->sRaw);
		pBody->sRaw = NULL;
	}
	if ( pBody->sEnvelope != NULL ) {
		xrtFree(pBody->sEnvelope);
		pBody->sEnvelope = NULL;
	}
	if ( pBody->arrParams != NULL ) {
		xrtFree(pBody->arrParams);
		pBody->arrParams = NULL;
	}
	if ( pBody->arrDispositionParams != NULL ) {
		xrtFree(pBody->arrDispositionParams);
		pBody->arrDispositionParams = NULL;
	}
	if ( pBody->pMessageBody != NULL ) {
		xrtImapBodyStructureFree(pBody->pMessageBody);
		xrtFree(pBody->pMessageBody);
		pBody->pMessageBody = NULL;
	}
	if ( pBody->arrChildren != NULL ) {
		for ( i = 0u; i < pBody->iPartCount; i++ ) {
			xrtImapBodyStructureFree(&pBody->arrChildren[i]);
		}
		xrtFree(pBody->arrChildren);
		pBody->arrChildren = NULL;
	}
	pBody->iPartCount = 0u;
	pBody->iParamCount = 0u;
	pBody->iDispositionParamCount = 0u;
	pBody->iMessageLines = 0u;
}

static void xrtImapBodyStructureResultFree(ximapbodystructureresult* pResult)
{
	if ( pResult == NULL ) {
		return;
	}
	xrtImapBodyStructureFree(&pResult->tBody);
	xrtFree(pResult);
}

static bool ximap_read_quoted_token(const char** pp, char* sOut, size_t iOutCap)
{
	const char* p;
	size_t iLen = 0u;

	if ( pp == NULL || *pp == NULL || sOut == NULL || iOutCap == 0u ) {
		return FALSE;
	}
	sOut[0] = '\0';
	p = *pp;
	while ( *p && isspace((unsigned char)*p) ) {
		p++;
	}
	if ( *p != '"' ) {
		return FALSE;
	}
	p++;
	while ( *p && *p != '"' ) {
		if ( *p == '\\' && p[1] != '\0' ) {
			p++;
		}
		if ( iLen + 1u < iOutCap ) {
			sOut[iLen++] = *p;
		}
		p++;
	}
	if ( *p != '"' ) {
		return FALSE;
	}
	sOut[iLen] = '\0';
	*pp = p + 1;
	return TRUE;
}

static const char* ximap_find_bodystructure_value(const char* sRaw)
{
	const char* p;

	if ( sRaw == NULL ) {
		return NULL;
	}
	p = strstr(sRaw, "BODYSTRUCTURE");
	if ( p == NULL ) {
		p = strstr(sRaw, "bodystructure");
	}
	if ( p == NULL ) {
		return NULL;
	}
	p += strlen("BODYSTRUCTURE");
	while ( *p && isspace((unsigned char)*p) ) {
		p++;
	}
	return *p ? p : NULL;
}

static void ximap_skip_ws(const char** pp)
{
	if ( pp == NULL || *pp == NULL ) {
		return;
	}
	while ( **pp && isspace((unsigned char)**pp) ) {
		(*pp)++;
	}
}

static bool ximap_find_list_end(const char* sList, const char** ppEnd)
{
	const char* p;
	int iDepth = 0;
	bool bQuoted = FALSE;
	bool bEsc = FALSE;

	if ( sList == NULL || sList[0] != '(' ) {
		return FALSE;
	}
	for ( p = sList; *p; p++ ) {
		if ( bQuoted ) {
			if ( bEsc ) bEsc = FALSE;
			else if ( *p == '\\' ) bEsc = TRUE;
			else if ( *p == '"' ) bQuoted = FALSE;
			continue;
		}
		if ( *p == '"' ) {
			bQuoted = TRUE;
		} else if ( *p == '(' ) {
			iDepth++;
		} else if ( *p == ')' ) {
			iDepth--;
			if ( iDepth == 0 ) {
				if ( ppEnd != NULL ) {
					*ppEnd = p + 1;
				}
				return TRUE;
			}
			if ( iDepth < 0 ) {
				return FALSE;
			}
		}
	}
	return FALSE;
}

static bool ximap_read_uint64_token(const char** pp, uint64* pOut)
{
	const char* p;
	uint64 iValue = 0u;
	bool bAny = FALSE;

	if ( pp == NULL || *pp == NULL || pOut == NULL ) {
		return FALSE;
	}
	p = *pp;
	ximap_skip_ws(&p);
	while ( *p >= '0' && *p <= '9' ) {
		bAny = TRUE;
		iValue = (iValue * 10u) + (uint64)(*p - '0');
		p++;
	}
	if ( !bAny ) {
		return FALSE;
	}
	*pOut = iValue;
	*pp = p;
	return TRUE;
}

static bool ximap_param_array_add(ximapparamelem** ppParams, uint32* pCount, const char* sName, const char* sValue)
{
	ximapparamelem* arrNew;

	if ( ppParams == NULL || pCount == NULL || sName == NULL || sValue == NULL ) {
		return FALSE;
	}
	arrNew = *ppParams
		? (ximapparamelem*)xrtRealloc(*ppParams, sizeof(ximapparamelem) * (*pCount + 1u))
		: (ximapparamelem*)xrtMalloc(sizeof(ximapparamelem));
	if ( arrNew == NULL ) {
		return FALSE;
	}
	*ppParams = arrNew;
	memset(&(*ppParams)[*pCount], 0, sizeof(ximapparamelem));
	snprintf((*ppParams)[*pCount].sName, sizeof((*ppParams)[*pCount].sName), "%s", sName);
	snprintf((*ppParams)[*pCount].sValue, sizeof((*ppParams)[*pCount].sValue), "%s", sValue);
	(*pCount)++;
	return TRUE;
}

static bool ximap_bodystructure_add_param(ximapbodystructure* pBody, const char* sName, const char* sValue)
{
	if ( pBody == NULL ) {
		return FALSE;
	}
	return ximap_param_array_add(&pBody->arrParams, &pBody->iParamCount, sName, sValue);
}

static bool ximap_parse_bodystructure_params(const char** pp, ximapbodystructure* pBody)
{
	const char* p;

	if ( pp == NULL || *pp == NULL || pBody == NULL ) {
		return FALSE;
	}
	p = *pp;
	ximap_skip_ws(&p);
	if ( ximap_starts_with_i(p, "NIL") ) {
		*pp = p + 3;
		return TRUE;
	}
	if ( *p != '(' ) {
		return FALSE;
	}
	p++;
	for (;;) {
		char sName[64];
		char sValue[256];

		ximap_skip_ws(&p);
		if ( *p == ')' ) {
			*pp = p + 1;
			return TRUE;
		}
		if ( !ximap_read_quoted_token(&p, sName, sizeof(sName)) ) {
			return FALSE;
		}
		if ( !ximap_read_quoted_token(&p, sValue, sizeof(sValue)) ) {
			return FALSE;
		}
		if ( !ximap_bodystructure_add_param(pBody, sName, sValue) ) {
			return FALSE;
		}
	}
}

static bool ximap_parse_bodystructure_param_list_to(const char** pp, ximapparamelem** ppParams, uint32* pCount)
{
	const char* p;

	if ( pp == NULL || *pp == NULL || ppParams == NULL || pCount == NULL ) {
		return FALSE;
	}
	p = *pp;
	ximap_skip_ws(&p);
	if ( ximap_starts_with_i(p, "NIL") ) {
		*pp = p + 3;
		return TRUE;
	}
	if ( *p != '(' ) {
		return FALSE;
	}
	p++;
	for (;;) {
		char sName[64];
		char sValue[256];

		ximap_skip_ws(&p);
		if ( *p == ')' ) {
			*pp = p + 1;
			return TRUE;
		}
		if ( !ximap_read_quoted_token(&p, sName, sizeof(sName)) ) {
			return FALSE;
		}
		if ( !ximap_read_quoted_token(&p, sValue, sizeof(sValue)) ) {
			return FALSE;
		}
		if ( !ximap_param_array_add(ppParams, pCount, sName, sValue) ) {
			return FALSE;
		}
	}
}

static bool ximap_read_nstring_token(const char** pp, char* sOut, size_t iOutCap)
{
	const char* p;

	if ( pp == NULL || *pp == NULL || sOut == NULL || iOutCap == 0u ) {
		return FALSE;
	}
	sOut[0] = '\0';
	p = *pp;
	ximap_skip_ws(&p);
	if ( ximap_starts_with_i(p, "NIL") ) {
		*pp = p + 3;
		return TRUE;
	}
	if ( *p == '"' ) {
		if ( !ximap_read_quoted_token(&p, sOut, iOutCap) ) {
			return FALSE;
		}
		*pp = p;
		return TRUE;
	}
	return FALSE;
}

static bool ximap_parse_bodystructure_disposition(const char** pp, ximapbodystructure* pBody)
{
	const char* p;

	if ( pp == NULL || *pp == NULL || pBody == NULL ) {
		return FALSE;
	}
	p = *pp;
	ximap_skip_ws(&p);
	if ( ximap_starts_with_i(p, "NIL") ) {
		*pp = p + 3;
		return TRUE;
	}
	if ( *p != '(' ) {
		return FALSE;
	}
	p++;
	if ( !ximap_read_quoted_token(&p, pBody->sDisposition, sizeof(pBody->sDisposition)) ) {
		return FALSE;
	}
	if ( !ximap_parse_bodystructure_param_list_to(&p, &pBody->arrDispositionParams, &pBody->iDispositionParamCount) ) {
		return FALSE;
	}
	ximap_skip_ws(&p);
	if ( *p != ')' ) {
		return FALSE;
	}
	*pp = p + 1;
	return TRUE;
}

static bool ximap_parse_bodystructure_extensions(const char** pp, ximapbodystructure* pBody)
{
	const char* p;

	if ( pp == NULL || *pp == NULL || pBody == NULL ) {
		return FALSE;
	}
	p = *pp;
	ximap_skip_ws(&p);
	if ( *p == ')' || *p == '\0' ) {
		*pp = p;
		return TRUE;
	}
	if ( !ximap_read_nstring_token(&p, pBody->sMd5, sizeof(pBody->sMd5)) ) {
		return FALSE;
	}
	ximap_skip_ws(&p);
	if ( *p == ')' || *p == '\0' ) {
		*pp = p;
		return TRUE;
	}
	if ( !ximap_parse_bodystructure_disposition(&p, pBody) ) {
		return FALSE;
	}
	ximap_skip_ws(&p);
	if ( *p == ')' || *p == '\0' ) {
		*pp = p;
		return TRUE;
	}
	if ( !ximap_read_nstring_token(&p, pBody->sLanguage, sizeof(pBody->sLanguage)) ) {
		return FALSE;
	}
	ximap_skip_ws(&p);
	if ( *p == ')' || *p == '\0' ) {
		*pp = p;
		return TRUE;
	}
	if ( !ximap_read_nstring_token(&p, pBody->sLocation, sizeof(pBody->sLocation)) ) {
		return FALSE;
	}
	*pp = p;
	return TRUE;
}

static bool ximap_parse_bodystructure_multipart_extensions(const char** pp, ximapbodystructure* pBody)
{
	const char* p;

	if ( pp == NULL || *pp == NULL || pBody == NULL ) {
		return FALSE;
	}
	p = *pp;
	ximap_skip_ws(&p);
	if ( *p == ')' || *p == '\0' ) {
		*pp = p;
		return TRUE;
	}
	if ( !ximap_parse_bodystructure_params(&p, pBody) ) {
		return FALSE;
	}
	ximap_skip_ws(&p);
	if ( *p == ')' || *p == '\0' ) {
		*pp = p;
		return TRUE;
	}
	if ( !ximap_parse_bodystructure_disposition(&p, pBody) ) {
		return FALSE;
	}
	ximap_skip_ws(&p);
	if ( *p == ')' || *p == '\0' ) {
		*pp = p;
		return TRUE;
	}
	if ( !ximap_read_nstring_token(&p, pBody->sLanguage, sizeof(pBody->sLanguage)) ) {
		return FALSE;
	}
	ximap_skip_ws(&p);
	if ( *p == ')' || *p == '\0' ) {
		*pp = p;
		return TRUE;
	}
	if ( !ximap_read_nstring_token(&p, pBody->sLocation, sizeof(pBody->sLocation)) ) {
		return FALSE;
	}
	*pp = p;
	return TRUE;
}

static bool ximap_parse_bodystructure_node(const char* sValue, const char** ppAfter, ximapbodystructure* pBody);

static bool ximap_read_parenthesized_raw_token(const char** pp, str* psOut)
{
	const char* p;
	const char* pEnd;
	size_t iLen;

	if ( pp == NULL || *pp == NULL || psOut == NULL ) {
		return FALSE;
	}
	*psOut = NULL;
	p = *pp;
	ximap_skip_ws(&p);
	if ( *p != '(' ) {
		return FALSE;
	}
	if ( !ximap_find_list_end(p, &pEnd) ) {
		return FALSE;
	}
	iLen = (size_t)(pEnd - p);
	*psOut = (str)xrtMalloc(iLen + 1u);
	if ( *psOut == NULL ) {
		return FALSE;
	}
	memcpy(*psOut, p, iLen);
	(*psOut)[iLen] = '\0';
	*pp = pEnd;
	return TRUE;
}

static bool ximap_parse_bodystructure_leaf_fields(const char** pp, ximapbodystructure* pBody)
{
	uint64 iLines = 0u;

	if ( pp == NULL || *pp == NULL || pBody == NULL ) {
		return FALSE;
	}
	if ( !ximap_parse_bodystructure_params(pp, pBody) ) {
		return FALSE;
	}
	if ( !ximap_read_nstring_token(pp, pBody->sId, sizeof(pBody->sId)) ) {
		return FALSE;
	}
	if ( !ximap_read_nstring_token(pp, pBody->sDescription, sizeof(pBody->sDescription)) ) {
		return FALSE;
	}
	if ( !ximap_read_quoted_token(pp, pBody->sEncoding, sizeof(pBody->sEncoding)) ) {
		return FALSE;
	}
	if ( !ximap_read_uint64_token(pp, &pBody->iOctets) ) {
		return FALSE;
	}
	if ( ximap_starts_with_i(pBody->sType, "TEXT") ) {
		if ( !ximap_read_uint64_token(pp, &iLines) ) {
			return FALSE;
		}
		pBody->iLines = (uint32)iLines;
	} else if ( ximap_starts_with_i(pBody->sType, "MESSAGE") && ximap_starts_with_i(pBody->sSubType, "RFC822") ) {
		if ( !ximap_read_parenthesized_raw_token(pp, &pBody->sEnvelope) ) {
			return FALSE;
		}
		pBody->pMessageBody = (ximapbodystructure*)xrtCalloc(1, sizeof(*pBody->pMessageBody));
		if ( pBody->pMessageBody == NULL ) {
			return FALSE;
		}
		if ( !ximap_parse_bodystructure_node(*pp, pp, pBody->pMessageBody) ) {
			return FALSE;
		}
		if ( !ximap_read_uint64_token(pp, &iLines) ) {
			return FALSE;
		}
		pBody->iMessageLines = (uint32)iLines;
	}
	if ( !ximap_parse_bodystructure_extensions(pp, pBody) ) {
		return FALSE;
	}
	return TRUE;
}

static bool ximap_bodystructure_add_child(ximapbodystructure* pBody, const ximapbodystructure* pChild)
{
	ximapbodystructure* arrNew;

	if ( pBody == NULL || pChild == NULL ) {
		return FALSE;
	}
	arrNew = pBody->arrChildren
		? (ximapbodystructure*)xrtRealloc(pBody->arrChildren, sizeof(ximapbodystructure) * (pBody->iPartCount + 1u))
		: (ximapbodystructure*)xrtMalloc(sizeof(ximapbodystructure));
	if ( arrNew == NULL ) {
		return FALSE;
	}
	pBody->arrChildren = arrNew;
	pBody->arrChildren[pBody->iPartCount] = *pChild;
	pBody->iPartCount++;
	return TRUE;
}

static bool ximap_parse_bodystructure_node(const char* sValue, const char** ppAfter, ximapbodystructure* pBody)
{
	const char* p;
	const char* pNodeStart;

	if ( sValue == NULL || pBody == NULL ) {
		return FALSE;
	}
	memset(pBody, 0, sizeof(*pBody));
	p = sValue;
	ximap_skip_ws(&p);
	if ( *p != '(' ) {
		return FALSE;
	}
	pNodeStart = p;
	p++;
	ximap_skip_ws(&p);
	if ( *p == '(' ) {
		pBody->bMultipart = TRUE;
		snprintf(pBody->sType, sizeof(pBody->sType), "%s", "MULTIPART");
		while ( *p == '(' ) {
			ximapbodystructure tChild;
			const char* pAfterChild = NULL;
			memset(&tChild, 0, sizeof(tChild));
			if ( !ximap_parse_bodystructure_node(p, &pAfterChild, &tChild) ) {
				xrtImapBodyStructureFree(pBody);
				return FALSE;
			}
			if ( !ximap_bodystructure_add_child(pBody, &tChild) ) {
				xrtImapBodyStructureFree(&tChild);
				xrtImapBodyStructureFree(pBody);
				return FALSE;
			}
			p = pAfterChild;
			ximap_skip_ws(&p);
		}
		if ( !ximap_read_quoted_token(&p, pBody->sSubType, sizeof(pBody->sSubType)) ) {
			xrtImapBodyStructureFree(pBody);
			return FALSE;
		}
		if ( !ximap_parse_bodystructure_multipart_extensions(&p, pBody) ) {
			xrtImapBodyStructureFree(pBody);
			return FALSE;
		}
	} else {
		if ( !ximap_read_quoted_token(&p, pBody->sType, sizeof(pBody->sType)) ) {
			return FALSE;
		}
		if ( !ximap_read_quoted_token(&p, pBody->sSubType, sizeof(pBody->sSubType)) ) {
			xrtImapBodyStructureFree(pBody);
			return FALSE;
		}
		if ( !ximap_parse_bodystructure_leaf_fields(&p, pBody) ) {
			xrtImapBodyStructureFree(pBody);
			return FALSE;
		}
		pBody->iPartCount = 1u;
	}
	if ( !ximap_find_list_end(pNodeStart, &p) ) {
		xrtImapBodyStructureFree(pBody);
		return FALSE;
	}
	if ( ppAfter != NULL ) {
		*ppAfter = p;
	}
	return TRUE;
}

static bool ximap_parse_bodystructure_response(const char* sRaw, ximapbodystructure* pBody)
{
	const char* sValue;
	bool bOK;

	if ( sRaw == NULL || pBody == NULL ) {
		return FALSE;
	}
	memset(pBody, 0, sizeof(*pBody));
	sValue = ximap_find_bodystructure_value(sRaw);
	if ( sValue == NULL || sValue[0] != '(' ) {
		return FALSE;
	}
	bOK = ximap_parse_bodystructure_node(sValue, NULL, pBody);
	if ( !bOK ) {
		xrtImapBodyStructureFree(pBody);
		return FALSE;
	}
	pBody->sRaw = xrtCopyStr((str)sValue, 0);
	if ( pBody->sRaw == NULL ) {
		xrtImapBodyStructureFree(pBody);
		return FALSE;
	}
	return TRUE;
}

static int ximap_secure_mode_auto(const ximapconfig* pCfg)
{
	if ( pCfg == NULL ) {
		return XIMAP_SECURE_NONE;
	}
	if ( pCfg->iSecureMode != XIMAP_SECURE_AUTO ) {
		return pCfg->iSecureMode;
	}
	if ( pCfg->iPort == 993u ) {
		return XIMAP_SECURE_SSL;
	}
	return XIMAP_SECURE_NONE;
}

static bool ximap_quote_astring(ximapbuf* pBuf, const char* sText)
{
	const unsigned char* p;

	if ( pBuf == NULL ) {
		return FALSE;
	}
	if ( !ximap_buf_append(pBuf, "\"") ) {
		return FALSE;
	}
	for ( p = (const unsigned char*)ximap_str_or_empty(sText); *p; p++ ) {
		char sEsc[2];
		if ( *p == '\\' || *p == '"' ) {
			if ( !ximap_buf_append_n(pBuf, "\\", 1u) ) {
				return FALSE;
			}
		}
		sEsc[0] = (char)*p;
		sEsc[1] = '\0';
		if ( !ximap_buf_append_n(pBuf, sEsc, 1u) ) {
			return FALSE;
		}
	}
	return ximap_buf_append(pBuf, "\"");
}

static bool ximap_socket_send_all(ximapclient* pClient, const void* pData, size_t iLen, ximapresult* pRet)
{
	const char* p = (const char*)pData;
	size_t iLeft = iLen;

	if ( pClient == NULL || pClient->hSock == XIMAP_SOCKET_INVALID || pClient->hSock == 0 ) {
		ximap_result_error(pRet, "IMAP client is not connected");
		return FALSE;
	}
	while ( iLeft > 0u ) {
		int iSent = send(pClient->hSock, p, (int)iLeft, 0);
		if ( iSent <= 0 ) {
			ximap_result_error(pRet, "IMAP send failed");
			return FALSE;
		}
		p += iSent;
		iLeft -= (size_t)iSent;
	}
	return TRUE;
}

static bool ximap_tls_flush(ximapclient* pClient, ximapresult* pRet)
{
	char aBuf[4096];
	size_t iRead = 0u;

	if ( pClient == NULL || pClient->pTls == NULL ) {
		return TRUE;
	}
	while ( xrtNetTlsSessionPendingCipher(pClient->pTls) > 0u ) {
		if ( xrtNetTlsSessionPeekCipher(pClient->pTls, aBuf, sizeof(aBuf), &iRead) != XRT_NET_OK || iRead == 0u ) {
			ximap_result_error(pRet, "IMAP TLS cipher peek failed");
			return FALSE;
		}
		if ( !ximap_socket_send_all(pClient, aBuf, iRead, pRet) ) {
			return FALSE;
		}
		xrtNetTlsSessionConsumeCipher(pClient->pTls, iRead);
	}
	return TRUE;
}

static bool ximap_tls_handshake(ximapclient* pClient, const ximapconfig* pCfg, ximapresult* pRet)
{
	xtlsconfig tTlsCfg;
	char aBuf[4096];

	if ( pClient == NULL || pCfg == NULL ) {
		ximap_result_error(pRet, "IMAP TLS config invalid");
		return FALSE;
	}
	memset(&tTlsCfg, 0, sizeof(tTlsCfg));
	tTlsCfg.sHostName = pCfg->sHost;
	tTlsCfg.bVerifyPeer = pCfg->bVerifyPeer;
	tTlsCfg.iMaxVersion = pCfg->iTlsMaxVersion;
	tTlsCfg.sCaFile = pCfg->sCaFile;
	pClient->pTls = xrtNetTlsSessionCreate(&tTlsCfg, FALSE);
	if ( pClient->pTls == NULL ) {
		ximap_result_error(pRet, "IMAP TLS session create failed");
		return FALSE;
	}
	while ( !xrtNetTlsSessionIsReady(pClient->pTls) ) {
		size_t iRead = 0u;
		xnet_result iRes = xrtNetTlsSessionDriveHandshake(pClient->pTls);
		if ( iRes != XRT_NET_OK && iRes != XRT_NET_AGAIN ) {
			ximap_result_tls_error(pRet, "IMAP TLS handshake failed", iRes);
			return FALSE;
		}
		if ( !ximap_tls_flush(pClient, pRet) ) {
			return FALSE;
		}
		if ( xrtNetTlsSessionIsReady(pClient->pTls) ) {
			break;
		}
		if ( xrtNetTlsSessionPendingRecv(pClient->pTls) > 0u ) {
			continue;
		}
		iRead = (size_t)recv(pClient->hSock, aBuf, (int)sizeof(aBuf), 0);
		if ( iRead == 0u || iRead == (size_t)-1 ) {
			ximap_result_sys_error(pRet, "IMAP TLS handshake recv failed");
			return FALSE;
		}
		iRes = xrtNetTlsSessionFeedCipher(pClient->pTls, aBuf, iRead);
		if ( iRes != XRT_NET_OK ) {
			ximap_result_tls_error(pRet, "IMAP TLS handshake feed failed", iRes);
			return FALSE;
		}
	}
	if ( pRet != NULL ) {
		pRet->bUsedTLS = TRUE;
	}
	return TRUE;
}

static bool ximap_recv_bytes(ximapclient* pClient, void* pBuf, size_t iCap, size_t* pRead, ximapresult* pRet)
{
	if ( pRead != NULL ) {
		*pRead = 0u;
	}
	if ( pClient == NULL || pBuf == NULL || iCap == 0u || pClient->hSock == XIMAP_SOCKET_INVALID || pClient->hSock == 0 ) {
		ximap_result_error(pRet, "IMAP client is not connected");
		return FALSE;
	}
	if ( pClient->pTls == NULL ) {
		int iRet = recv(pClient->hSock, (char*)pBuf, (int)iCap, 0);
		if ( iRet <= 0 ) {
			ximap_result_sys_error(pRet, "IMAP recv failed");
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
				ximap_result_sys_error(pRet, "IMAP TLS recv failed");
				return FALSE;
			}
			iFeed = xrtNetTlsSessionFeedCipher(pClient->pTls, aCipher, (size_t)iRet);
			if ( iFeed != XRT_NET_OK ) {
				ximap_result_tls_error(pRet, "IMAP TLS feed failed", iFeed);
				return FALSE;
			}
			if ( !ximap_tls_flush(pClient, pRet) ) {
				return FALSE;
			}
			continue;
		}
		if ( iRes != XRT_NET_OK || iPlain == 0u ) {
			ximap_result_tls_error(pRet, "IMAP TLS read failed", iRes);
			return FALSE;
		}
	}
}

static bool ximap_client_connect(ximapclient* pClient, const ximapconfig* pCfg, ximapresult* pRet)
{
	struct addrinfo tHints;
	struct addrinfo* pRes = NULL;
	struct addrinfo* pIt;
	char sPort[16];
	uint32 iTimeoutMs;

	if ( pClient == NULL || pCfg == NULL || pCfg->sHost == NULL || pCfg->sHost[0] == '\0' ) {
		ximap_result_error(pRet, "IMAP host is empty");
		return FALSE;
	}
	(void)xrtInit();
	pClient->iTimeoutMs = pCfg->iTimeoutMs ? pCfg->iTimeoutMs : 15000u;
	iTimeoutMs = pClient->iTimeoutMs;
	pClient->hSock = XIMAP_SOCKET_INVALID;

	memset(&tHints, 0, sizeof(tHints));
	tHints.ai_family = AF_UNSPEC;
	tHints.ai_socktype = SOCK_STREAM;
	snprintf(sPort, sizeof(sPort), "%u", (unsigned)pCfg->iPort);
	if ( getaddrinfo(pCfg->sHost, sPort, &tHints, &pRes) != 0 || pRes == NULL ) {
		ximap_result_error(pRet, "IMAP host resolve failed");
		return FALSE;
	}
	{
		int iPass;
		for ( iPass = 0; iPass < 2 && pClient->hSock == XIMAP_SOCKET_INVALID; ++iPass ) {
			int iFamilyWant = (iPass == 0) ? AF_INET : AF_INET6;
			for ( pIt = pRes; pIt != NULL; pIt = pIt->ai_next ) {
				xsocket hSock;
				if ( pIt->ai_family != iFamilyWant ) {
					continue;
				}
				hSock = socket(pIt->ai_family, pIt->ai_socktype, pIt->ai_protocol);
				if ( hSock == XIMAP_SOCKET_INVALID ) {
					continue;
				}
				if ( connect(hSock, pIt->ai_addr, (int)pIt->ai_addrlen) == 0 ) {
					pClient->hSock = hSock;
					break;
				}
				XIMAP_CLOSESOCK(hSock);
			}
		}
	}
	freeaddrinfo(pRes);
	if ( pClient->hSock == XIMAP_SOCKET_INVALID ) {
		ximap_result_error(pRet, "IMAP connect failed");
		return FALSE;
	}
#if defined(_WIN32) || defined(_WIN64)
	{
		DWORD iSockTimeout = (DWORD)iTimeoutMs;
		setsockopt(pClient->hSock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&iSockTimeout, sizeof(iSockTimeout));
		setsockopt(pClient->hSock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&iSockTimeout, sizeof(iSockTimeout));
	}
#else
	{
		struct timeval tSockTimeout;
		tSockTimeout.tv_sec = (time_t)(iTimeoutMs / 1000u);
		tSockTimeout.tv_usec = (suseconds_t)((iTimeoutMs % 1000u) * 1000u);
		setsockopt(pClient->hSock, SOL_SOCKET, SO_RCVTIMEO, &tSockTimeout, sizeof(tSockTimeout));
		setsockopt(pClient->hSock, SOL_SOCKET, SO_SNDTIMEO, &tSockTimeout, sizeof(tSockTimeout));
	}
#endif
	if ( ximap_secure_mode_auto(pCfg) == XIMAP_SECURE_SSL ) {
		ximap_result_stage(pRet, XIMAP_STAGE_TLS);
		if ( !ximap_tls_handshake(pClient, pCfg, pRet) ) {
			return FALSE;
		}
	}
	return TRUE;
}

static void xrtImapClose(ximapclient* pClient)
{
	if ( pClient == NULL ) {
		return;
	}
	ximap_pending_free_all(pClient);
	if ( pClient->pTls != NULL ) {
		xrtNetTlsSessionDestroy(pClient->pTls);
		pClient->pTls = NULL;
	}
	if ( pClient->hSock != XIMAP_SOCKET_INVALID && pClient->hSock != 0 ) {
		XIMAP_CLOSESOCK(pClient->hSock);
		pClient->hSock = XIMAP_SOCKET_INVALID;
	}
	xrtFree(pClient);
}

static bool ximap_send_line(ximapclient* pClient, const char* sLine, ximapresult* pRet)
{
	ximapbuf tBuf;

	memset(&tBuf, 0, sizeof(tBuf));
	if ( pClient == NULL || pClient->hSock == XIMAP_SOCKET_INVALID || pClient->hSock == 0 ) {
		ximap_result_error(pRet, "IMAP client is not connected");
		return FALSE;
	}
	if ( !ximap_buf_append(&tBuf, ximap_str_or_empty(sLine)) || !ximap_buf_append(&tBuf, "\r\n") ) {
		ximap_result_error(pRet, "IMAP send buffer alloc failed");
		return FALSE;
	}
	if ( pClient->pTls != NULL ) {
		const char* p = tBuf.sData;
		size_t iLeft = tBuf.iLen;
		while ( iLeft > 0u ) {
			size_t iWritten = 0u;
			xnet_result iRes = xrtNetTlsSessionWritePlain(pClient->pTls, p, iLeft, &iWritten);
			if ( iRes != XRT_NET_OK && iRes != XRT_NET_AGAIN ) {
				ximap_buf_free(&tBuf);
				ximap_result_error(pRet, "IMAP TLS write failed");
				return FALSE;
			}
			if ( !ximap_tls_flush(pClient, pRet) ) {
				ximap_buf_free(&tBuf);
				return FALSE;
			}
			if ( iWritten == 0u && iLeft > 0u ) {
				ximap_buf_free(&tBuf);
				ximap_result_error(pRet, "IMAP TLS write stalled");
				return FALSE;
			}
			p += iWritten;
			iLeft -= iWritten;
		}
	} else if ( !ximap_socket_send_all(pClient, tBuf.sData, tBuf.iLen, pRet) ) {
		ximap_buf_free(&tBuf);
		return FALSE;
	}
	ximap_buf_free(&tBuf);
	return TRUE;
}

static bool ximap_read_line(ximapclient* pClient, char* sOut, size_t iOutCap, ximapresult* pRet)
{
	size_t iLen;

	if ( sOut == NULL || iOutCap == 0u ) {
		ximap_result_error(pRet, "IMAP read line output invalid");
		return FALSE;
	}
	sOut[0] = '\0';
	if ( pClient == NULL || pClient->hSock == XIMAP_SOCKET_INVALID || pClient->hSock == 0 ) {
		ximap_result_error(pRet, "IMAP client is not connected");
		return FALSE;
	}
	iLen = 0u;
	for (;;) {
		char ch;
		size_t iRead = 0u;
		if ( !ximap_recv_bytes(pClient, &ch, 1u, &iRead, pRet) ) {
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

static bool ximap_read_exact(ximapclient* pClient, ximapbuf* pBuf, size_t iLen, ximapresult* pRet)
{
	char sTmp[1024];
	size_t iLeft = iLen;

	if ( pClient == NULL || pClient->hSock == XIMAP_SOCKET_INVALID || pClient->hSock == 0 || pBuf == NULL ) {
		ximap_result_error(pRet, "IMAP client is not connected");
		return FALSE;
	}
	while ( iLeft > 0u ) {
		size_t iWant = iLeft < sizeof(sTmp) ? iLeft : sizeof(sTmp);
		size_t iRead = 0u;
		if ( !ximap_recv_bytes(pClient, sTmp, iWant, &iRead, pRet) ) {
			return FALSE;
		}
		if ( !ximap_buf_append_n(pBuf, sTmp, iRead) ) {
			ximap_result_error(pRet, "IMAP literal buffer alloc failed");
			return FALSE;
		}
		iLeft -= iRead;
	}
	return TRUE;
}

static bool ximap_read_tagged_response(ximapclient* pClient, const char* sExpectedTag, str* psOut, ximapresult* pRet)
{
	ximapbuf tSegment;
	char sLine[4096];
	ximaptaggedline tTagged;
	ximapliteralmarker tLiteral;
	bool bDone = FALSE;
	bool bOK = FALSE;

	if ( psOut != NULL ) {
		*psOut = NULL;
	}
	if ( ximap_pending_take(pClient, sExpectedTag, psOut, pRet) ) {
		bOK = pRet == NULL || pRet->iStatus == XIMAP_STATUS_OK || pRet->iStatus == XIMAP_STATUS_PREAUTH;
		if ( !bOK ) {
			ximap_result_error(pRet, "IMAP command failed");
		}
		return bOK;
	}
	memset(&tSegment, 0, sizeof(tSegment));
	while ( !bDone ) {
		if ( !ximap_read_line(pClient, sLine, sizeof(sLine), pRet) ) {
			ximap_buf_free(&tSegment);
			return FALSE;
		}
		if ( !ximap_buf_append(&tSegment, sLine) || !ximap_buf_append(&tSegment, "\r\n") ) {
			ximap_buf_free(&tSegment);
			ximap_result_error(pRet, "IMAP response buffer alloc failed");
			return FALSE;
		}
		if ( pRet != NULL ) {
			snprintf(pRet->sLastReply, sizeof(pRet->sLastReply), "%s", sLine);
		}
		ximap_parse_capability_line(sLine, pRet ? &pRet->iCapabilities : NULL);
		if ( ximap_parse_literal_marker(sLine, &tLiteral) && tLiteral.bHasLiteral ) {
			if ( !ximap_read_exact(pClient, &tSegment, tLiteral.iLiteralLen, pRet) ) {
				ximap_buf_free(&tSegment);
				return FALSE;
			}
		}
		if ( ximap_parse_tagged_line(sLine, &tTagged) ) {
			if ( pRet != NULL ) {
				pRet->iStatus = tTagged.iStatus;
			}
			if ( ximap_ascii_eq_i(tTagged.sTag, sExpectedTag) ) {
				bDone = TRUE;
				bOK = tTagged.iStatus == XIMAP_STATUS_OK || tTagged.iStatus == XIMAP_STATUS_PREAUTH;
				if ( !bOK ) {
					ximap_result_error(pRet, tTagged.sText[0] ? tTagged.sText : "IMAP command failed");
				}
			} else {
				str sPendingRaw = (str)tSegment.sData;
				tSegment.sData = NULL;
				tSegment.iLen = 0u;
				tSegment.iCap = 0u;
				if ( !ximap_pending_store(pClient, tTagged.sTag, tTagged.iStatus, sPendingRaw, pRet) ) {
					ximap_buf_free(&tSegment);
					return FALSE;
				}
			}
		}
	}
	if ( psOut != NULL ) {
		*psOut = (str)tSegment.sData;
	} else {
		ximap_buf_free(&tSegment);
	}
	return bOK;
}

static bool ximap_extract_first_literal(const char* sRaw, str* psLiteral)
{
	const char* p;
	const char* sLineEnd;
	ximapliteralmarker tLiteral;

	if ( psLiteral == NULL ) {
		return FALSE;
	}
	*psLiteral = NULL;
	if ( sRaw == NULL ) {
		return FALSE;
	}
	p = sRaw;
	while ( *p ) {
		sLineEnd = strstr(p, "\r\n");
		if ( sLineEnd == NULL ) {
			return FALSE;
		}
		{
			size_t iLineLen = (size_t)(sLineEnd - p);
			char* sLine = (char*)xrtMalloc(iLineLen + 1u);
			if ( sLine == NULL ) {
				return FALSE;
			}
			memcpy(sLine, p, iLineLen);
			sLine[iLineLen] = '\0';
			if ( ximap_parse_literal_marker(sLine, &tLiteral) && tLiteral.bHasLiteral ) {
				const char* sLiteral = sLineEnd + 2;
				xrtFree(sLine);
				if ( strlen(sLiteral) < tLiteral.iLiteralLen ) {
					return FALSE;
				}
				*psLiteral = xrtCopyStr((str)sLiteral, tLiteral.iLiteralLen);
				return *psLiteral != NULL;
			}
			xrtFree(sLine);
		}
		p = sLineEnd + 2;
	}
	return FALSE;
}

static bool ximap_command_raw(ximapclient* pClient, const char* sCommand, int iStage, str* psOut, ximapresult* pRet)
{
	str sTagged;
	char sTag[32];

	if ( psOut != NULL ) {
		*psOut = NULL;
	}
	if ( pClient == NULL ) {
		ximap_result_error(pRet, "IMAP client is not connected");
		return FALSE;
	}
	ximap_result_stage(pRet, iStage);
	sTagged = ximap_build_tagged_command(&pClient->iTagCounter, sCommand);
	if ( sTagged == NULL ) {
		ximap_result_error(pRet, "IMAP command alloc failed");
		return FALSE;
	}
	if ( !ximap_get_tag_from_command((const char*)sTagged, sTag, sizeof(sTag)) ) {
		xrtFree(sTagged);
		ximap_result_error(pRet, "IMAP tag parse failed");
		return FALSE;
	}
	if ( !ximap_send_line(pClient, (const char*)sTagged, pRet) ) {
		xrtFree(sTagged);
		return FALSE;
	}
	xrtFree(sTagged);
	return ximap_read_tagged_response(pClient, sTag, psOut, pRet);
}

static bool xrtImapCommandBegin(ximapclient* pClient, const char* sCommand, int iStage, char* sTagOut, size_t iTagOutCap, ximapresult* pRet)
{
	str sTagged;
	char sTag[32];

	if ( sTagOut != NULL && iTagOutCap > 0u ) {
		sTagOut[0] = '\0';
	}
	if ( pClient == NULL ) {
		ximap_result_error(pRet, "IMAP client is not connected");
		return FALSE;
	}
	if ( sTagOut == NULL || iTagOutCap == 0u ) {
		ximap_result_error(pRet, "IMAP command tag output missing");
		return FALSE;
	}
	ximap_result_stage(pRet, iStage);
	sTagged = ximap_build_tagged_command(&pClient->iTagCounter, sCommand);
	if ( sTagged == NULL ) {
		ximap_result_error(pRet, "IMAP command alloc failed");
		return FALSE;
	}
	if ( !ximap_get_tag_from_command((const char*)sTagged, sTag, sizeof(sTag)) ) {
		xrtFree(sTagged);
		ximap_result_error(pRet, "IMAP tag parse failed");
		return FALSE;
	}
	if ( strlen(sTag) >= iTagOutCap ) {
		xrtFree(sTagged);
		ximap_result_error(pRet, "IMAP command tag output too small");
		return FALSE;
	}
	if ( !ximap_send_line(pClient, (const char*)sTagged, pRet) ) {
		xrtFree(sTagged);
		return FALSE;
	}
	memcpy(sTagOut, sTag, strlen(sTag) + 1u);
	xrtFree(sTagged);
	return TRUE;
}

static bool xrtImapCommandFinish(ximapclient* pClient, const char* sTag, str* psOut, ximapresult* pRet)
{
	if ( psOut != NULL ) {
		*psOut = NULL;
	}
	if ( pClient == NULL ) {
		ximap_result_error(pRet, "IMAP client is not connected");
		return FALSE;
	}
	if ( sTag == NULL || sTag[0] == '\0' ) {
		ximap_result_error(pRet, "IMAP command tag missing");
		return FALSE;
	}
	ximap_result_stage(pRet, XIMAP_STAGE_COMMAND);
	return ximap_read_tagged_response(pClient, sTag, psOut, pRet);
}

static bool xrtImapLogin(ximapclient* pClient, const char* sUser, const char* sPass, ximapresult* pRet)
{
	ximapbuf tCmd;
	bool bOK;

	memset(&tCmd, 0, sizeof(tCmd));
	if ( !ximap_buf_append(&tCmd, "LOGIN ") || !ximap_quote_astring(&tCmd, sUser)
		|| !ximap_buf_append(&tCmd, " ") || !ximap_quote_astring(&tCmd, sPass) ) {
		ximap_buf_free(&tCmd);
		ximap_result_error(pRet, "IMAP LOGIN alloc failed");
		return FALSE;
	}
	bOK = ximap_command_raw(pClient, tCmd.sData, XIMAP_STAGE_AUTH, NULL, pRet);
	ximap_buf_free(&tCmd);
	return bOK;
}

static bool xrtImapAuthenticateXOAuth2(ximapclient* pClient, const char* sUser, const char* sOAuth2Token, ximapresult* pRet)
{
	ximapbuf tPayload;
	str sBase64 = NULL;
	str sTagged = NULL;
	char sTag[32];
	char sLine[1024];
	bool bOK = FALSE;

	memset(&tPayload, 0, sizeof(tPayload));
	if ( pClient == NULL ) {
		ximap_result_error(pRet, "IMAP client is not connected");
		return FALSE;
	}
	if ( sUser == NULL || sUser[0] == '\0' || sOAuth2Token == NULL || sOAuth2Token[0] == '\0' ) {
		ximap_result_error(pRet, "IMAP XOAUTH2 user or token missing");
		return FALSE;
	}
	if ( !ximap_buf_append(&tPayload, "user=") || !ximap_buf_append(&tPayload, sUser)
		|| !ximap_buf_append_n(&tPayload, "\001auth=Bearer ", 13u)
		|| !ximap_buf_append(&tPayload, sOAuth2Token)
		|| !ximap_buf_append_n(&tPayload, "\001\001", 2u) ) {
		ximap_buf_free(&tPayload);
		ximap_result_error(pRet, "IMAP XOAUTH2 payload alloc failed");
		return FALSE;
	}
	sBase64 = xrtBase64Encode(tPayload.sData, tPayload.iLen, NULL);
	ximap_buf_free(&tPayload);
	if ( sBase64 == NULL || sBase64[0] == '\0' ) {
		xrtFree(sBase64);
		ximap_result_error(pRet, "IMAP XOAUTH2 base64 failed");
		return FALSE;
	}
	ximap_result_stage(pRet, XIMAP_STAGE_AUTH);
	sTagged = ximap_build_tagged_command(&pClient->iTagCounter, "AUTHENTICATE XOAUTH2");
	if ( sTagged == NULL || !ximap_get_tag_from_command((const char*)sTagged, sTag, sizeof(sTag)) ) {
		xrtFree(sTagged);
		xrtFree(sBase64);
		ximap_result_error(pRet, "IMAP XOAUTH2 command alloc failed");
		return FALSE;
	}
	if ( !ximap_send_line(pClient, (const char*)sTagged, pRet) ) {
		xrtFree(sTagged);
		xrtFree(sBase64);
		return FALSE;
	}
	xrtFree(sTagged);
	if ( !ximap_read_line(pClient, sLine, sizeof(sLine), pRet) ) {
		xrtFree(sBase64);
		return FALSE;
	}
	if ( !ximap_line_is_continuation(sLine) ) {
		xrtFree(sBase64);
		ximap_result_error(pRet, "IMAP XOAUTH2 continuation missing");
		return FALSE;
	}
	if ( !ximap_send_line(pClient, (const char*)sBase64, pRet) ) {
		xrtFree(sBase64);
		return FALSE;
	}
	xrtFree(sBase64);
	bOK = ximap_read_tagged_response(pClient, sTag, NULL, pRet);
	return bOK;
}

static bool xrtImapOpen(const ximapconfig* pCfg, ximapclient** ppClient, ximapresult* pRet)
{
	ximapclient* pClient;
	char sGreeting[1024];
	ximapuntaggedline tGreeting;

	if ( pRet != NULL ) {
		xrtImapResultInit(pRet);
	}
	if ( ppClient == NULL ) {
		ximap_result_error(pRet, "IMAP client output missing");
		return FALSE;
	}
	*ppClient = NULL;
	if ( pCfg == NULL ) {
		ximap_result_error(pRet, "IMAP config missing");
		return FALSE;
	}
	pClient = (ximapclient*)xrtCalloc(1, sizeof(*pClient));
	if ( pClient == NULL ) {
		ximap_result_error(pRet, "IMAP client alloc failed");
		return FALSE;
	}
	pClient->hSock = XIMAP_SOCKET_INVALID;
	ximap_result_stage(pRet, XIMAP_STAGE_CONNECT);
	if ( !ximap_client_connect(pClient, pCfg, pRet) ) {
		xrtImapClose(pClient);
		return FALSE;
	}
	ximap_result_stage(pRet, XIMAP_STAGE_GREETING);
	if ( !ximap_read_line(pClient, sGreeting, sizeof(sGreeting), pRet) ) {
		xrtImapClose(pClient);
		return FALSE;
	}
	if ( pRet != NULL ) {
		snprintf(pRet->sLastReply, sizeof(pRet->sLastReply), "%s", sGreeting);
	}
	if ( !ximap_parse_untagged_line(sGreeting, &tGreeting)
		|| (!ximap_starts_with_i(tGreeting.sAtom, "OK") && !ximap_starts_with_i(tGreeting.sAtom, "PREAUTH")) ) {
		ximap_result_error(pRet, "IMAP invalid greeting");
		xrtImapClose(pClient);
		return FALSE;
	}
	if ( ximap_starts_with_i(tGreeting.sAtom, "PREAUTH") && pRet != NULL ) {
		pRet->iStatus = XIMAP_STATUS_PREAUTH;
	} else if ( pRet != NULL ) {
		pRet->iStatus = XIMAP_STATUS_OK;
	}
	if ( ximap_secure_mode_auto(pCfg) == XIMAP_SECURE_STARTTLS ) {
		if ( !ximap_command_raw(pClient, "STARTTLS", XIMAP_STAGE_TLS, NULL, pRet) ) {
			xrtImapClose(pClient);
			return FALSE;
		}
		if ( !ximap_tls_handshake(pClient, pCfg, pRet) ) {
			xrtImapClose(pClient);
			return FALSE;
		}
		if ( pRet != NULL ) {
			pRet->bUsedStartTLS = TRUE;
		}
	}
	if ( pCfg->sUser != NULL && pCfg->sUser[0] != '\0' ) {
		if ( pCfg->sOAuth2Token != NULL && pCfg->sOAuth2Token[0] != '\0' ) {
			if ( !xrtImapAuthenticateXOAuth2(pClient, pCfg->sUser, pCfg->sOAuth2Token, pRet) ) {
				xrtImapClose(pClient);
				return FALSE;
			}
		} else if ( !xrtImapLogin(pClient, pCfg->sUser, ximap_str_or_empty(pCfg->sPass), pRet) ) {
			xrtImapClose(pClient);
			return FALSE;
		}
	}
	*ppClient = pClient;
	if ( pRet != NULL ) {
		pRet->bSuccess = TRUE;
	}
	return TRUE;
}

static bool xrtImapCapability(ximapclient* pClient, uint32* pCaps, ximapresult* pRet)
{
	str sRaw = NULL;
	bool bOK;

	if ( pCaps == NULL ) {
		ximap_result_error(pRet, "IMAP CAPABILITY output missing");
		return FALSE;
	}
	*pCaps = 0u;
	bOK = ximap_command_raw(pClient, "CAPABILITY", XIMAP_STAGE_CAPABILITY, &sRaw, pRet);
	if ( bOK ) {
		ximap_parse_capability_line((const char*)sRaw, pCaps);
		if ( pRet != NULL ) {
			pRet->iCapabilities = *pCaps;
		}
	}
	xrtFree(sRaw);
	return bOK;
}

static bool xrtImapStatusRaw(ximapclient* pClient, const char* sMailbox, str* psOut, ximapresult* pRet)
{
	ximapbuf tCmd;
	bool bOK;

	memset(&tCmd, 0, sizeof(tCmd));
	if ( !ximap_buf_append(&tCmd, "STATUS ") || !ximap_quote_astring(&tCmd, sMailbox)
		|| !ximap_buf_append(&tCmd, " (MESSAGES RECENT UIDNEXT UIDVALIDITY UNSEEN)") ) {
		ximap_buf_free(&tCmd);
		ximap_result_error(pRet, "IMAP STATUS alloc failed");
		return FALSE;
	}
	bOK = ximap_command_raw(pClient, tCmd.sData, XIMAP_STAGE_COMMAND, psOut, pRet);
	ximap_buf_free(&tCmd);
	return bOK;
}

static bool xrtImapStatus(ximapclient* pClient, const char* sMailbox, ximapmailboxstatus* pStatus, ximapresult* pRet)
{
	str sRaw = NULL;
	bool bOK;

	if ( pStatus == NULL ) {
		ximap_result_error(pRet, "IMAP STATUS output missing");
		return FALSE;
	}
	memset(pStatus, 0, sizeof(*pStatus));
	if ( !xrtImapStatusRaw(pClient, sMailbox, &sRaw, pRet) ) {
		return FALSE;
	}
	bOK = ximap_parse_status_response((const char*)sRaw, pStatus);
	xrtFree(sRaw);
	if ( !bOK ) {
		ximap_result_error(pRet, "IMAP STATUS parse failed");
	}
	return bOK;
}

static bool xrtImapSelect(ximapclient* pClient, const char* sMailbox, str* psOut, ximapresult* pRet)
{
	ximapbuf tCmd;
	bool bOK;

	memset(&tCmd, 0, sizeof(tCmd));
	if ( !ximap_buf_append(&tCmd, "SELECT ") || !ximap_quote_astring(&tCmd, sMailbox) ) {
		ximap_buf_free(&tCmd);
		ximap_result_error(pRet, "IMAP SELECT alloc failed");
		return FALSE;
	}
	bOK = ximap_command_raw(pClient, tCmd.sData, XIMAP_STAGE_SELECT, psOut, pRet);
	ximap_buf_free(&tCmd);
	return bOK;
}

static bool xrtImapSelectStatus(ximapclient* pClient, const char* sMailbox, ximapmailboxstatus* pStatus, ximapresult* pRet)
{
	str sRaw = NULL;
	bool bOK;

	if ( pStatus == NULL ) {
		ximap_result_error(pRet, "IMAP SELECT status output missing");
		return FALSE;
	}
	memset(pStatus, 0, sizeof(*pStatus));
	if ( !xrtImapSelect(pClient, sMailbox, &sRaw, pRet) ) {
		return FALSE;
	}
	bOK = ximap_parse_mailbox_status_response((const char*)sRaw, pStatus);
	xrtFree(sRaw);
	if ( !bOK ) {
		ximap_result_error(pRet, "IMAP SELECT status parse failed");
	}
	return bOK;
}

static bool xrtImapExamine(ximapclient* pClient, const char* sMailbox, str* psOut, ximapresult* pRet)
{
	ximapbuf tCmd;
	bool bOK;

	memset(&tCmd, 0, sizeof(tCmd));
	if ( !ximap_buf_append(&tCmd, "EXAMINE ") || !ximap_quote_astring(&tCmd, sMailbox) ) {
		ximap_buf_free(&tCmd);
		ximap_result_error(pRet, "IMAP EXAMINE alloc failed");
		return FALSE;
	}
	bOK = ximap_command_raw(pClient, tCmd.sData, XIMAP_STAGE_SELECT, psOut, pRet);
	ximap_buf_free(&tCmd);
	return bOK;
}

static bool xrtImapExamineStatus(ximapclient* pClient, const char* sMailbox, ximapmailboxstatus* pStatus, ximapresult* pRet)
{
	str sRaw = NULL;
	bool bOK;

	if ( pStatus == NULL ) {
		ximap_result_error(pRet, "IMAP EXAMINE status output missing");
		return FALSE;
	}
	memset(pStatus, 0, sizeof(*pStatus));
	if ( !xrtImapExamine(pClient, sMailbox, &sRaw, pRet) ) {
		return FALSE;
	}
	bOK = ximap_parse_mailbox_status_response((const char*)sRaw, pStatus);
	xrtFree(sRaw);
	if ( !bOK ) {
		ximap_result_error(pRet, "IMAP EXAMINE status parse failed");
	}
	return bOK;
}

static bool xrtImapListRaw(ximapclient* pClient, const char* sReference, const char* sPattern, str* psOut, ximapresult* pRet)
{
	ximapbuf tCmd;
	bool bOK;

	memset(&tCmd, 0, sizeof(tCmd));
	if ( !ximap_buf_append(&tCmd, "LIST ") || !ximap_quote_astring(&tCmd, sReference)
		|| !ximap_buf_append(&tCmd, " ") || !ximap_quote_astring(&tCmd, sPattern) ) {
		ximap_buf_free(&tCmd);
		ximap_result_error(pRet, "IMAP LIST alloc failed");
		return FALSE;
	}
	bOK = ximap_command_raw(pClient, tCmd.sData, XIMAP_STAGE_COMMAND, psOut, pRet);
	ximap_buf_free(&tCmd);
	return bOK;
}

static bool xrtImapList(ximapclient* pClient, const char* sReference, const char* sPattern, ximaplist* pList, ximapresult* pRet)
{
	str sRaw = NULL;
	bool bOK;

	if ( pList == NULL ) {
		ximap_result_error(pRet, "IMAP LIST output missing");
		return FALSE;
	}
	memset(pList, 0, sizeof(*pList));
	if ( !xrtImapListRaw(pClient, sReference, sPattern, &sRaw, pRet) ) {
		return FALSE;
	}
	bOK = ximap_parse_list_response((const char*)sRaw, pList);
	xrtFree(sRaw);
	if ( !bOK ) {
		ximap_result_error(pRet, "IMAP LIST parse failed");
	}
	return bOK;
}

static bool xrtImapSearchRaw(ximapclient* pClient, const char* sCriteria, str* psOut, ximapresult* pRet)
{
	ximapbuf tCmd;
	bool bOK;

	memset(&tCmd, 0, sizeof(tCmd));
	if ( !ximap_buf_append(&tCmd, "SEARCH ") || !ximap_buf_append(&tCmd, ximap_str_or_empty(sCriteria)) ) {
		ximap_buf_free(&tCmd);
		ximap_result_error(pRet, "IMAP SEARCH alloc failed");
		return FALSE;
	}
	bOK = ximap_command_raw(pClient, tCmd.sData, XIMAP_STAGE_COMMAND, psOut, pRet);
	ximap_buf_free(&tCmd);
	return bOK;
}

static bool xrtImapSearch(ximapclient* pClient, const char* sCriteria, ximapsearchids* pIds, ximapresult* pRet)
{
	str sRaw = NULL;
	bool bOK;

	if ( pIds == NULL ) {
		ximap_result_error(pRet, "IMAP SEARCH output missing");
		return FALSE;
	}
	memset(pIds, 0, sizeof(*pIds));
	if ( !xrtImapSearchRaw(pClient, sCriteria, &sRaw, pRet) ) {
		return FALSE;
	}
	bOK = ximap_parse_search_response((const char*)sRaw, pIds);
	xrtFree(sRaw);
	if ( !bOK ) {
		ximap_result_error(pRet, "IMAP SEARCH parse failed");
	}
	return bOK;
}

static bool xrtImapUidSearchRaw(ximapclient* pClient, const char* sCriteria, str* psOut, ximapresult* pRet)
{
	ximapbuf tCmd;
	bool bOK;

	memset(&tCmd, 0, sizeof(tCmd));
	if ( !ximap_buf_append(&tCmd, "UID SEARCH ") || !ximap_buf_append(&tCmd, ximap_str_or_empty(sCriteria)) ) {
		ximap_buf_free(&tCmd);
		ximap_result_error(pRet, "IMAP UID SEARCH alloc failed");
		return FALSE;
	}
	bOK = ximap_command_raw(pClient, tCmd.sData, XIMAP_STAGE_COMMAND, psOut, pRet);
	ximap_buf_free(&tCmd);
	return bOK;
}

static bool xrtImapUidSearch(ximapclient* pClient, const char* sCriteria, ximapsearchids* pIds, ximapresult* pRet)
{
	str sRaw = NULL;
	bool bOK;

	if ( pIds == NULL ) {
		ximap_result_error(pRet, "IMAP UID SEARCH output missing");
		return FALSE;
	}
	memset(pIds, 0, sizeof(*pIds));
	if ( !xrtImapUidSearchRaw(pClient, sCriteria, &sRaw, pRet) ) {
		return FALSE;
	}
	bOK = ximap_parse_search_response((const char*)sRaw, pIds);
	xrtFree(sRaw);
	if ( !bOK ) {
		ximap_result_error(pRet, "IMAP UID SEARCH parse failed");
	}
	return bOK;
}

static bool xrtImapFetchRaw(ximapclient* pClient, const char* sSequence, const char* sItems, str* psOut, ximapresult* pRet)
{
	ximapbuf tCmd;
	bool bOK;

	memset(&tCmd, 0, sizeof(tCmd));
	if ( !ximap_buf_append(&tCmd, "FETCH ") || !ximap_buf_append(&tCmd, ximap_str_or_empty(sSequence))
		|| !ximap_buf_append(&tCmd, " ") || !ximap_buf_append(&tCmd, ximap_str_or_empty(sItems)) ) {
		ximap_buf_free(&tCmd);
		ximap_result_error(pRet, "IMAP FETCH alloc failed");
		return FALSE;
	}
	bOK = ximap_command_raw(pClient, tCmd.sData, XIMAP_STAGE_FETCH, psOut, pRet);
	ximap_buf_free(&tCmd);
	return bOK;
}

static bool xrtImapFetchFlags(ximapclient* pClient, const char* sSequence, ximapflags* pFlags, ximapresult* pRet)
{
	str sRaw = NULL;
	bool bOK;

	if ( pFlags == NULL ) {
		ximap_result_error(pRet, "IMAP FETCH FLAGS output missing");
		return FALSE;
	}
	memset(pFlags, 0, sizeof(*pFlags));
	if ( !xrtImapFetchRaw(pClient, sSequence, "FLAGS", &sRaw, pRet) ) {
		return FALSE;
	}
	bOK = ximap_parse_fetch_flags_response((const char*)sRaw, pFlags);
	xrtFree(sRaw);
	if ( !bOK ) {
		ximap_result_error(pRet, "IMAP FETCH FLAGS parse failed");
	}
	return bOK;
}

static bool xrtImapFetchBodyStructureRaw(ximapclient* pClient, const char* sSequence, str* psOut, ximapresult* pRet)
{
	return xrtImapFetchRaw(pClient, sSequence, "BODYSTRUCTURE", psOut, pRet);
}

static bool xrtImapFetchBodyStructure(ximapclient* pClient, const char* sSequence, ximapbodystructure* pBody, ximapresult* pRet)
{
	str sRaw = NULL;
	bool bOK;

	if ( pBody == NULL ) {
		ximap_result_error(pRet, "IMAP BODYSTRUCTURE output missing");
		return FALSE;
	}
	memset(pBody, 0, sizeof(*pBody));
	if ( !xrtImapFetchBodyStructureRaw(pClient, sSequence, &sRaw, pRet) ) {
		return FALSE;
	}
	bOK = ximap_parse_bodystructure_response((const char*)sRaw, pBody);
	xrtFree(sRaw);
	if ( !bOK ) {
		ximap_result_error(pRet, "IMAP BODYSTRUCTURE parse failed");
	}
	return bOK;
}

static bool ximap_uid_fetch_body_raw_ex(ximapclient* pClient, const char* sUid, bool bPeek, str* psOut, ximapresult* pRet)
{
	ximapbuf tCmd;
	bool bOK;

	memset(&tCmd, 0, sizeof(tCmd));
	if ( !ximap_buf_append(&tCmd, "UID FETCH ") || !ximap_buf_append(&tCmd, ximap_str_or_empty(sUid))
		|| !ximap_buf_append(&tCmd, bPeek ? " BODY.PEEK[]" : " BODY[]") ) {
		ximap_buf_free(&tCmd);
		ximap_result_error(pRet, "IMAP UID FETCH alloc failed");
		return FALSE;
	}
	bOK = ximap_command_raw(pClient, tCmd.sData, XIMAP_STAGE_FETCH, psOut, pRet);
	ximap_buf_free(&tCmd);
	return bOK;
}

static bool xrtImapUidFetchBodyRaw(ximapclient* pClient, const char* sUid, str* psOut, ximapresult* pRet)
{
	return ximap_uid_fetch_body_raw_ex(pClient, sUid, FALSE, psOut, pRet);
}

static bool xrtImapUidPeekBodyRaw(ximapclient* pClient, const char* sUid, str* psOut, ximapresult* pRet)
{
	return ximap_uid_fetch_body_raw_ex(pClient, sUid, TRUE, psOut, pRet);
}

static bool ximap_uid_fetch_body_literal_ex(ximapclient* pClient, const char* sUid, bool bPeek, str* psLiteral, ximapresult* pRet)
{
	str sRaw = NULL;
	bool bOK;

	if ( psLiteral == NULL ) {
		ximap_result_error(pRet, "IMAP UID FETCH BODY literal output missing");
		return FALSE;
	}
	*psLiteral = NULL;
	if ( !ximap_uid_fetch_body_raw_ex(pClient, sUid, bPeek, &sRaw, pRet) ) {
		return FALSE;
	}
	bOK = ximap_extract_first_literal((const char*)sRaw, psLiteral);
	xrtFree(sRaw);
	if ( !bOK ) {
		ximap_result_error(pRet, "IMAP UID FETCH BODY literal parse failed");
	}
	return bOK;
}

static bool xrtImapUidFetchBodyLiteral(ximapclient* pClient, const char* sUid, str* psLiteral, ximapresult* pRet)
{
	return ximap_uid_fetch_body_literal_ex(pClient, sUid, FALSE, psLiteral, pRet);
}

static bool xrtImapUidPeekBodyLiteral(ximapclient* pClient, const char* sUid, str* psLiteral, ximapresult* pRet)
{
	return ximap_uid_fetch_body_literal_ex(pClient, sUid, TRUE, psLiteral, pRet);
}

static bool ximap_header_field_is_safe(const char* sField);

static bool ximap_body_section_field_list_is_safe(const char* sList, size_t iLen)
{
	char sField[64];
	size_t i = 0u;
	size_t iFieldLen = 0u;
	bool bSawField = FALSE;

	if ( sList == NULL || iLen == 0u ) {
		return FALSE;
	}
	while ( i < iLen ) {
		while ( i < iLen && sList[i] == ' ' ) {
			i++;
		}
		if ( i >= iLen ) {
			break;
		}
		iFieldLen = 0u;
		while ( i < iLen && sList[i] != ' ' ) {
			if ( iFieldLen + 1u >= sizeof(sField) ) {
				return FALSE;
			}
			sField[iFieldLen++] = sList[i++];
		}
		sField[iFieldLen] = '\0';
		if ( !ximap_header_field_is_safe(sField) ) {
			return FALSE;
		}
		bSawField = TRUE;
	}
	return bSawField;
}

static bool ximap_body_section_token_eq_i(const char* sText, size_t iLen, const char* sExpect)
{
	size_t i;

	if ( sText == NULL || sExpect == NULL || strlen(sExpect) != iLen ) {
		return FALSE;
	}
	for ( i = 0u; i < iLen; i++ ) {
		if ( tolower((unsigned char)sText[i]) != tolower((unsigned char)sExpect[i]) ) {
			return FALSE;
		}
	}
	return TRUE;
}

static bool ximap_body_section_is_safe(const char* sSection)
{
	const unsigned char* p;
	const char* sOpen = NULL;
	const char* sClose = NULL;
	const char* sPrefixEnd;
	size_t iPrefixLen;
	bool bHasSpace = FALSE;

	if ( sSection == NULL || sSection[0] == '\0' ) {
		return FALSE;
	}
	for ( p = (const unsigned char*)sSection; *p != '\0'; p++ ) {
		if ( *p == ' ' ) {
			bHasSpace = TRUE;
			continue;
		}
		if ( *p == '(' ) {
			if ( sOpen != NULL ) {
				return FALSE;
			}
			sOpen = (const char*)p;
			continue;
		}
		if ( *p == ')' ) {
			if ( sOpen == NULL || sClose != NULL || p[1] != '\0' ) {
				return FALSE;
			}
			sClose = (const char*)p;
			continue;
		}
		if ( !((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9')
			|| *p == '.' || *p == '-' || *p == '_') ) {
			return FALSE;
		}
	}
	if ( sOpen == NULL || sClose == NULL ) {
		return !bHasSpace;
	}
	if ( sOpen == sSection || sOpen[1] == ')' || sOpen[-1] != ' ' ) {
		return FALSE;
	}
	sPrefixEnd = sOpen - 1;
	while ( sPrefixEnd > sSection && sPrefixEnd[-1] == ' ' ) {
		sPrefixEnd--;
	}
	iPrefixLen = (size_t)(sPrefixEnd - sSection);
	if ( iPrefixLen == strlen("HEADER.FIELDS") ) {
		if ( !ximap_body_section_token_eq_i(sSection, iPrefixLen, "HEADER.FIELDS") ) {
			return FALSE;
		}
	} else if ( iPrefixLen > strlen(".HEADER.FIELDS")
		&& ximap_body_section_token_eq_i(sPrefixEnd - strlen(".HEADER.FIELDS"), strlen(".HEADER.FIELDS"), ".HEADER.FIELDS") ) {
		/* Section prefix such as 1.2.HEADER.FIELDS. */
	} else {
		return FALSE;
	}
	return ximap_body_section_field_list_is_safe(sOpen + 1, (size_t)(sClose - sOpen - 1));
}

static bool ximap_header_field_is_safe(const char* sField)
{
	const unsigned char* p;

	if ( sField == NULL || sField[0] == '\0' ) {
		return FALSE;
	}
	if ( !((sField[0] >= 'A' && sField[0] <= 'Z') || (sField[0] >= 'a' && sField[0] <= 'z') || (sField[0] >= '0' && sField[0] <= '9')) ) {
		return FALSE;
	}
	for ( p = (const unsigned char*)sField; *p != '\0'; p++ ) {
		if ( !((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-') ) {
			return FALSE;
		}
	}
	return TRUE;
}

static bool ximap_uid_fetch_body_section_raw_ex(ximapclient* pClient, const char* sUid, const char* sSection, bool bPeek, str* psOut, ximapresult* pRet)
{
	ximapbuf tCmd;
	bool bOK;

	if ( !ximap_body_section_is_safe(sSection) ) {
		ximap_result_error(pRet, "IMAP UID FETCH BODY section invalid");
		return FALSE;
	}
	memset(&tCmd, 0, sizeof(tCmd));
	if ( !ximap_buf_append(&tCmd, "UID FETCH ") || !ximap_buf_append(&tCmd, ximap_str_or_empty(sUid))
		|| !ximap_buf_append(&tCmd, bPeek ? " BODY.PEEK[" : " BODY[") || !ximap_buf_append(&tCmd, sSection)
		|| !ximap_buf_append(&tCmd, "]") ) {
		ximap_buf_free(&tCmd);
		ximap_result_error(pRet, "IMAP UID FETCH BODY section alloc failed");
		return FALSE;
	}
	bOK = ximap_command_raw(pClient, tCmd.sData, XIMAP_STAGE_FETCH, psOut, pRet);
	ximap_buf_free(&tCmd);
	return bOK;
}

static bool xrtImapUidFetchBodySectionRaw(ximapclient* pClient, const char* sUid, const char* sSection, str* psOut, ximapresult* pRet)
{
	return ximap_uid_fetch_body_section_raw_ex(pClient, sUid, sSection, FALSE, psOut, pRet);
}

static bool xrtImapUidPeekBodySectionRaw(ximapclient* pClient, const char* sUid, const char* sSection, str* psOut, ximapresult* pRet)
{
	return ximap_uid_fetch_body_section_raw_ex(pClient, sUid, sSection, TRUE, psOut, pRet);
}

static bool ximap_uid_fetch_header_fields_raw_ex(ximapclient* pClient, const char* sUid, const char* const* arrFields, size_t iFieldCount, bool bPeek, str* psOut, ximapresult* pRet)
{
	ximapbuf tCmd;
	size_t i;
	bool bOK;

	if ( arrFields == NULL || iFieldCount == 0u ) {
		ximap_result_error(pRet, "IMAP HEADER.FIELDS field list missing");
		return FALSE;
	}
	for ( i = 0u; i < iFieldCount; i++ ) {
		if ( !ximap_header_field_is_safe(arrFields[i]) ) {
			ximap_result_error(pRet, "IMAP HEADER.FIELDS field invalid");
			return FALSE;
		}
	}
	memset(&tCmd, 0, sizeof(tCmd));
	if ( !ximap_buf_append(&tCmd, "UID FETCH ") || !ximap_buf_append(&tCmd, ximap_str_or_empty(sUid))
		|| !ximap_buf_append(&tCmd, bPeek ? " BODY.PEEK[HEADER.FIELDS (" : " BODY[HEADER.FIELDS (") ) {
		ximap_buf_free(&tCmd);
		ximap_result_error(pRet, "IMAP HEADER.FIELDS alloc failed");
		return FALSE;
	}
	for ( i = 0u; i < iFieldCount; i++ ) {
		if ( i > 0u && !ximap_buf_append(&tCmd, " ") ) {
			ximap_buf_free(&tCmd);
			ximap_result_error(pRet, "IMAP HEADER.FIELDS alloc failed");
			return FALSE;
		}
		if ( !ximap_buf_append(&tCmd, arrFields[i]) ) {
			ximap_buf_free(&tCmd);
			ximap_result_error(pRet, "IMAP HEADER.FIELDS alloc failed");
			return FALSE;
		}
	}
	if ( !ximap_buf_append(&tCmd, ")]") ) {
		ximap_buf_free(&tCmd);
		ximap_result_error(pRet, "IMAP HEADER.FIELDS alloc failed");
		return FALSE;
	}
	bOK = ximap_command_raw(pClient, tCmd.sData, XIMAP_STAGE_FETCH, psOut, pRet);
	ximap_buf_free(&tCmd);
	return bOK;
}

static bool xrtImapUidFetchHeaderFieldsRaw(ximapclient* pClient, const char* sUid, const char* const* arrFields, size_t iFieldCount, str* psOut, ximapresult* pRet)
{
	return ximap_uid_fetch_header_fields_raw_ex(pClient, sUid, arrFields, iFieldCount, FALSE, psOut, pRet);
}

static bool xrtImapUidPeekHeaderFieldsRaw(ximapclient* pClient, const char* sUid, const char* const* arrFields, size_t iFieldCount, str* psOut, ximapresult* pRet)
{
	return ximap_uid_fetch_header_fields_raw_ex(pClient, sUid, arrFields, iFieldCount, TRUE, psOut, pRet);
}

static bool ximap_uid_fetch_header_fields_ex(ximapclient* pClient, const char* sUid, const char* const* arrFields, size_t iFieldCount, bool bPeek, str* psHeader, ximapresult* pRet)
{
	str sRaw = NULL;
	bool bOK;

	if ( psHeader == NULL ) {
		ximap_result_error(pRet, "IMAP HEADER.FIELDS output missing");
		return FALSE;
	}
	*psHeader = NULL;
	if ( !ximap_uid_fetch_header_fields_raw_ex(pClient, sUid, arrFields, iFieldCount, bPeek, &sRaw, pRet) ) {
		return FALSE;
	}
	bOK = ximap_extract_first_literal((const char*)sRaw, psHeader);
	xrtFree(sRaw);
	if ( !bOK ) {
		ximap_result_error(pRet, "IMAP HEADER.FIELDS literal parse failed");
	}
	return bOK;
}

static bool xrtImapUidFetchHeaderFields(ximapclient* pClient, const char* sUid, const char* const* arrFields, size_t iFieldCount, str* psHeader, ximapresult* pRet)
{
	return ximap_uid_fetch_header_fields_ex(pClient, sUid, arrFields, iFieldCount, FALSE, psHeader, pRet);
}

static bool xrtImapUidPeekHeaderFields(ximapclient* pClient, const char* sUid, const char* const* arrFields, size_t iFieldCount, str* psHeader, ximapresult* pRet)
{
	return ximap_uid_fetch_header_fields_ex(pClient, sUid, arrFields, iFieldCount, TRUE, psHeader, pRet);
}

static bool xrtImapUidFetchMime(ximapclient* pClient, const char* sUid, xmailparsedmessage* pMsg, ximapresult* pRet)
{
	str sLiteral = NULL;

	if ( pMsg == NULL ) {
		ximap_result_error(pRet, "IMAP UID FETCH MIME output missing");
		return FALSE;
	}
	memset(pMsg, 0, sizeof(*pMsg));
	if ( !xrtImapUidFetchBodyLiteral(pClient, sUid, &sLiteral, pRet) ) {
		return FALSE;
	}
	if ( !xmailMimeParseMessage((const char*)sLiteral, pMsg) ) {
		xrtFree(sLiteral);
		ximap_result_error(pRet, "IMAP UID FETCH MIME parse failed");
		return FALSE;
	}
	xrtFree(sLiteral);
	return TRUE;
}

static bool xrtImapUidPeekMime(ximapclient* pClient, const char* sUid, xmailparsedmessage* pMsg, ximapresult* pRet)
{
	str sLiteral = NULL;

	if ( pMsg == NULL ) {
		ximap_result_error(pRet, "IMAP UID PEEK MIME output missing");
		return FALSE;
	}
	memset(pMsg, 0, sizeof(*pMsg));
	if ( !xrtImapUidPeekBodyLiteral(pClient, sUid, &sLiteral, pRet) ) {
		return FALSE;
	}
	if ( !xmailMimeParseMessage((const char*)sLiteral, pMsg) ) {
		xrtFree(sLiteral);
		ximap_result_error(pRet, "IMAP UID PEEK MIME parse failed");
		return FALSE;
	}
	xrtFree(sLiteral);
	return TRUE;
}

static bool xrtImapStoreFlags(ximapclient* pClient, const char* sSequence, const char* sOperation, const char* sFlags, str* psOut, ximapresult* pRet)
{
	ximapbuf tCmd;
	bool bOK;

	memset(&tCmd, 0, sizeof(tCmd));
	if ( !ximap_buf_append(&tCmd, "STORE ") || !ximap_buf_append(&tCmd, ximap_str_or_empty(sSequence))
		|| !ximap_buf_append(&tCmd, " ") || !ximap_buf_append(&tCmd, ximap_str_or_empty(sOperation))
		|| !ximap_buf_append(&tCmd, " ") || !ximap_buf_append(&tCmd, ximap_str_or_empty(sFlags)) ) {
		ximap_buf_free(&tCmd);
		ximap_result_error(pRet, "IMAP STORE alloc failed");
		return FALSE;
	}
	bOK = ximap_command_raw(pClient, tCmd.sData, XIMAP_STAGE_COMMAND, psOut, pRet);
	ximap_buf_free(&tCmd);
	return bOK;
}

static bool xrtImapStoreFlagsParsed(ximapclient* pClient, const char* sSequence, const char* sOperation, const char* sFlags, ximapflags* pOutFlags, ximapresult* pRet)
{
	str sRaw = NULL;
	bool bOK;

	if ( pOutFlags == NULL ) {
		ximap_result_error(pRet, "IMAP STORE FLAGS output missing");
		return FALSE;
	}
	memset(pOutFlags, 0, sizeof(*pOutFlags));
	if ( !xrtImapStoreFlags(pClient, sSequence, sOperation, sFlags, &sRaw, pRet) ) {
		return FALSE;
	}
	bOK = ximap_parse_fetch_flags_response((const char*)sRaw, pOutFlags);
	xrtFree(sRaw);
	if ( !bOK ) {
		ximap_result_error(pRet, "IMAP STORE FLAGS parse failed");
	}
	return bOK;
}

static bool xrtImapUidStoreFlags(ximapclient* pClient, const char* sUid, const char* sOperation, const char* sFlags, str* psOut, ximapresult* pRet)
{
	ximapbuf tCmd;
	bool bOK;

	memset(&tCmd, 0, sizeof(tCmd));
	if ( !ximap_buf_append(&tCmd, "UID STORE ") || !ximap_buf_append(&tCmd, ximap_str_or_empty(sUid))
		|| !ximap_buf_append(&tCmd, " ") || !ximap_buf_append(&tCmd, ximap_str_or_empty(sOperation))
		|| !ximap_buf_append(&tCmd, " ") || !ximap_buf_append(&tCmd, ximap_str_or_empty(sFlags)) ) {
		ximap_buf_free(&tCmd);
		ximap_result_error(pRet, "IMAP UID STORE alloc failed");
		return FALSE;
	}
	bOK = ximap_command_raw(pClient, tCmd.sData, XIMAP_STAGE_COMMAND, psOut, pRet);
	ximap_buf_free(&tCmd);
	return bOK;
}

static bool xrtImapUidStoreFlagsParsed(ximapclient* pClient, const char* sUid, const char* sOperation, const char* sFlags, ximapflags* pOutFlags, ximapresult* pRet)
{
	str sRaw = NULL;
	bool bOK;

	if ( pOutFlags == NULL ) {
		ximap_result_error(pRet, "IMAP UID STORE FLAGS output missing");
		return FALSE;
	}
	memset(pOutFlags, 0, sizeof(*pOutFlags));
	if ( !xrtImapUidStoreFlags(pClient, sUid, sOperation, sFlags, &sRaw, pRet) ) {
		return FALSE;
	}
	bOK = ximap_parse_fetch_flags_response((const char*)sRaw, pOutFlags);
	xrtFree(sRaw);
	if ( !bOK ) {
		ximap_result_error(pRet, "IMAP UID STORE FLAGS parse failed");
	}
	return bOK;
}

static bool ximap_uid_set_is_safe(const char* sUidSet)
{
	const unsigned char* p;

	if ( sUidSet == NULL || sUidSet[0] == '\0' ) {
		return FALSE;
	}
	for ( p = (const unsigned char*)sUidSet; *p != '\0'; p++ ) {
		if ( !((*p >= '0' && *p <= '9') || *p == ',' || *p == ':' || *p == '*') ) {
			return FALSE;
		}
	}
	return TRUE;
}

static bool xrtImapExpungeRaw(ximapclient* pClient, str* psOut, ximapresult* pRet)
{
	return ximap_command_raw(pClient, "EXPUNGE", XIMAP_STAGE_COMMAND, psOut, pRet);
}

static bool xrtImapExpunge(ximapclient* pClient, ximapexpungeresult* pResult, ximapresult* pRet)
{
	str sRaw = NULL;
	bool bOK;

	if ( pResult == NULL ) {
		ximap_result_error(pRet, "IMAP EXPUNGE output missing");
		return FALSE;
	}
	memset(pResult, 0, sizeof(*pResult));
	if ( !xrtImapExpungeRaw(pClient, &sRaw, pRet) ) {
		return FALSE;
	}
	bOK = ximap_parse_expunge_response((const char*)sRaw, pResult);
	xrtFree(sRaw);
	if ( !bOK ) {
		ximap_result_error(pRet, "IMAP EXPUNGE parse failed");
	}
	return bOK;
}

static bool xrtImapUidExpungeRaw(ximapclient* pClient, const char* sUidSet, str* psOut, ximapresult* pRet)
{
	ximapbuf tCmd;
	bool bOK;

	if ( !ximap_uid_set_is_safe(sUidSet) ) {
		ximap_result_error(pRet, "IMAP UID EXPUNGE uid set invalid");
		return FALSE;
	}
	memset(&tCmd, 0, sizeof(tCmd));
	if ( !ximap_buf_append(&tCmd, "UID EXPUNGE ") || !ximap_buf_append(&tCmd, sUidSet) ) {
		ximap_buf_free(&tCmd);
		ximap_result_error(pRet, "IMAP UID EXPUNGE alloc failed");
		return FALSE;
	}
	bOK = ximap_command_raw(pClient, tCmd.sData, XIMAP_STAGE_COMMAND, psOut, pRet);
	ximap_buf_free(&tCmd);
	return bOK;
}

static bool xrtImapUidExpunge(ximapclient* pClient, const char* sUidSet, ximapexpungeresult* pResult, ximapresult* pRet)
{
	str sRaw = NULL;
	bool bOK;

	if ( pResult == NULL ) {
		ximap_result_error(pRet, "IMAP UID EXPUNGE output missing");
		return FALSE;
	}
	memset(pResult, 0, sizeof(*pResult));
	if ( !xrtImapUidExpungeRaw(pClient, sUidSet, &sRaw, pRet) ) {
		return FALSE;
	}
	bOK = ximap_parse_expunge_response((const char*)sRaw, pResult);
	xrtFree(sRaw);
	if ( !bOK ) {
		ximap_result_error(pRet, "IMAP UID EXPUNGE parse failed");
	}
	return bOK;
}

static bool xrtImapIdleOnce(ximapclient* pClient, str* psEvent, ximapresult* pRet)
{
	str sTagged = NULL;
	char sTag[32];
	char sLine[4096];
	bool bOK;

	if ( psEvent != NULL ) {
		*psEvent = NULL;
	}
	if ( pClient == NULL ) {
		ximap_result_error(pRet, "IMAP client is not connected");
		return FALSE;
	}
	ximap_result_stage(pRet, XIMAP_STAGE_IDLE);
	sTagged = ximap_build_tagged_command(&pClient->iTagCounter, "IDLE");
	if ( sTagged == NULL || !ximap_get_tag_from_command((const char*)sTagged, sTag, sizeof(sTag)) ) {
		xrtFree(sTagged);
		ximap_result_error(pRet, "IMAP IDLE command alloc failed");
		return FALSE;
	}
	if ( !ximap_send_line(pClient, (const char*)sTagged, pRet) ) {
		xrtFree(sTagged);
		return FALSE;
	}
	xrtFree(sTagged);
	if ( !ximap_read_line(pClient, sLine, sizeof(sLine), pRet) ) {
		return FALSE;
	}
	if ( !ximap_line_is_continuation(sLine) ) {
		ximap_result_error(pRet, "IMAP IDLE continuation missing");
		return FALSE;
	}
	if ( !ximap_read_line(pClient, sLine, sizeof(sLine), pRet) ) {
		return FALSE;
	}
	if ( psEvent != NULL ) {
		*psEvent = xrtCopyStr((str)sLine, 0);
		if ( *psEvent == NULL ) {
			ximap_result_error(pRet, "IMAP IDLE event alloc failed");
			return FALSE;
		}
	}
	if ( pRet != NULL ) {
		snprintf(pRet->sLastReply, sizeof(pRet->sLastReply), "%s", sLine);
	}
	if ( !ximap_send_line(pClient, "DONE", pRet) ) {
		return FALSE;
	}
	bOK = ximap_read_tagged_response(pClient, sTag, NULL, pRet);
	return bOK;
}

static bool xrtImapIdleProbe(ximapclient* pClient, ximapresult* pRet)
{
	str sTagged = NULL;
	char sTag[32];
	char sLine[4096];
	bool bOK;

	if ( pClient == NULL ) {
		ximap_result_error(pRet, "IMAP client is not connected");
		return FALSE;
	}
	ximap_result_stage(pRet, XIMAP_STAGE_IDLE);
	sTagged = ximap_build_tagged_command(&pClient->iTagCounter, "IDLE");
	if ( sTagged == NULL || !ximap_get_tag_from_command((const char*)sTagged, sTag, sizeof(sTag)) ) {
		xrtFree(sTagged);
		ximap_result_error(pRet, "IMAP IDLE command alloc failed");
		return FALSE;
	}
	if ( !ximap_send_line(pClient, (const char*)sTagged, pRet) ) {
		xrtFree(sTagged);
		return FALSE;
	}
	xrtFree(sTagged);
	if ( !ximap_read_line(pClient, sLine, sizeof(sLine), pRet) ) {
		return FALSE;
	}
	if ( !ximap_line_is_continuation(sLine) ) {
		ximap_result_error(pRet, "IMAP IDLE continuation missing");
		return FALSE;
	}
	if ( pRet != NULL ) {
		snprintf(pRet->sLastReply, sizeof(pRet->sLastReply), "%s", sLine);
	}
	if ( !ximap_send_line(pClient, "DONE", pRet) ) {
		return FALSE;
	}
	bOK = ximap_read_tagged_response(pClient, sTag, NULL, pRet);
	return bOK;
}

static bool xrtImapIdleCollect(ximapclient* pClient, size_t iMaxEvents, str** ppEvents, size_t* pEventCount, ximapresult* pRet)
{
	str sTagged = NULL;
	char sTag[32];
	char sLine[4096];
	str* arrEvents = NULL;
	size_t iCount = 0u;
	bool bOK;

	if ( ppEvents != NULL ) {
		*ppEvents = NULL;
	}
	if ( pEventCount != NULL ) {
		*pEventCount = 0u;
	}
	if ( pClient == NULL ) {
		ximap_result_error(pRet, "IMAP client is not connected");
		return FALSE;
	}
	if ( ppEvents == NULL || pEventCount == NULL || iMaxEvents == 0u ) {
		ximap_result_error(pRet, "IMAP IDLE collect output invalid");
		return FALSE;
	}
	arrEvents = (str*)xrtCalloc(iMaxEvents, sizeof(str));
	if ( arrEvents == NULL ) {
		ximap_result_error(pRet, "IMAP IDLE events alloc failed");
		return FALSE;
	}
	ximap_result_stage(pRet, XIMAP_STAGE_IDLE);
	sTagged = ximap_build_tagged_command(&pClient->iTagCounter, "IDLE");
	if ( sTagged == NULL || !ximap_get_tag_from_command((const char*)sTagged, sTag, sizeof(sTag)) ) {
		xrtFree(sTagged);
		xrtImapIdleEventsFree(arrEvents, iMaxEvents);
		ximap_result_error(pRet, "IMAP IDLE command alloc failed");
		return FALSE;
	}
	if ( !ximap_send_line(pClient, (const char*)sTagged, pRet) ) {
		xrtFree(sTagged);
		xrtImapIdleEventsFree(arrEvents, iMaxEvents);
		return FALSE;
	}
	xrtFree(sTagged);
	if ( !ximap_read_line(pClient, sLine, sizeof(sLine), pRet) ) {
		xrtImapIdleEventsFree(arrEvents, iMaxEvents);
		return FALSE;
	}
	if ( !ximap_line_is_continuation(sLine) ) {
		xrtImapIdleEventsFree(arrEvents, iMaxEvents);
		ximap_result_error(pRet, "IMAP IDLE continuation missing");
		return FALSE;
	}
	while ( iCount < iMaxEvents ) {
		if ( !ximap_read_line(pClient, sLine, sizeof(sLine), pRet) ) {
			xrtImapIdleEventsFree(arrEvents, iMaxEvents);
			return FALSE;
		}
		arrEvents[iCount] = xrtCopyStr((str)sLine, 0);
		if ( arrEvents[iCount] == NULL ) {
			xrtImapIdleEventsFree(arrEvents, iMaxEvents);
			ximap_result_error(pRet, "IMAP IDLE event alloc failed");
			return FALSE;
		}
		iCount++;
		if ( pRet != NULL ) {
			snprintf(pRet->sLastReply, sizeof(pRet->sLastReply), "%s", sLine);
		}
	}
	if ( !ximap_send_line(pClient, "DONE", pRet) ) {
		xrtImapIdleEventsFree(arrEvents, iMaxEvents);
		return FALSE;
	}
	bOK = ximap_read_tagged_response(pClient, sTag, NULL, pRet);
	if ( !bOK ) {
		xrtImapIdleEventsFree(arrEvents, iMaxEvents);
		return FALSE;
	}
	*ppEvents = arrEvents;
	*pEventCount = iCount;
	return TRUE;
}

static bool xrtImapIdleWatch(ximapclient* pClient, size_t iMaxEvents, ximapidlecallback pCallback, void* pUser, size_t* pEventCount, ximapresult* pRet)
{
	str sTagged = NULL;
	char sTag[32];
	char sLine[4096];
	size_t iCount = 0u;
	bool bOK;
	bool bContinue = TRUE;

	if ( pEventCount != NULL ) {
		*pEventCount = 0u;
	}
	if ( pClient == NULL ) {
		ximap_result_error(pRet, "IMAP client is not connected");
		return FALSE;
	}
	if ( pCallback == NULL ) {
		ximap_result_error(pRet, "IMAP IDLE callback missing");
		return FALSE;
	}
	ximap_result_stage(pRet, XIMAP_STAGE_IDLE);
	sTagged = ximap_build_tagged_command(&pClient->iTagCounter, "IDLE");
	if ( sTagged == NULL || !ximap_get_tag_from_command((const char*)sTagged, sTag, sizeof(sTag)) ) {
		xrtFree(sTagged);
		ximap_result_error(pRet, "IMAP IDLE command alloc failed");
		return FALSE;
	}
	if ( !ximap_send_line(pClient, (const char*)sTagged, pRet) ) {
		xrtFree(sTagged);
		return FALSE;
	}
	xrtFree(sTagged);
	if ( !ximap_read_line(pClient, sLine, sizeof(sLine), pRet) ) {
		return FALSE;
	}
	if ( !ximap_line_is_continuation(sLine) ) {
		ximap_result_error(pRet, "IMAP IDLE continuation missing");
		return FALSE;
	}
	while ( bContinue && (iMaxEvents == 0u || iCount < iMaxEvents) ) {
		if ( !ximap_read_line(pClient, sLine, sizeof(sLine), pRet) ) {
			return FALSE;
		}
		iCount++;
		if ( pRet != NULL ) {
			snprintf(pRet->sLastReply, sizeof(pRet->sLastReply), "%s", sLine);
		}
		bContinue = pCallback(sLine, pUser);
	}
	if ( pEventCount != NULL ) {
		*pEventCount = iCount;
	}
	if ( !ximap_send_line(pClient, "DONE", pRet) ) {
		return FALSE;
	}
	bOK = ximap_read_tagged_response(pClient, sTag, NULL, pRet);
	return bOK;
}

typedef struct {
	ximapidlecallback pUserCallback;
	void* pUser;
	size_t iEvents;
	bool bCancelled;
} ximapidleloopcallbackctx;

static bool ximap_idle_loop_callback(const char* sEvent, void* pUser)
{
	ximapidleloopcallbackctx* pCtx = (ximapidleloopcallbackctx*)pUser;
	bool bContinue;

	if ( pCtx == NULL || pCtx->pUserCallback == NULL ) {
		return FALSE;
	}
	pCtx->iEvents++;
	bContinue = pCtx->pUserCallback(sEvent, pCtx->pUser);
	if ( !bContinue ) {
		pCtx->bCancelled = TRUE;
	}
	return bContinue;
}

static void ximap_idle_loop_trace(const ximapidleloopopts* pOpts, const char* sEvent, size_t iCycle, size_t iEvents, uint32 iDelayMs, const ximapresult* pRet)
{
	if ( pOpts == NULL || pOpts->pTrace == NULL ) {
		return;
	}
	pOpts->pTrace(sEvent, iCycle, iEvents, iDelayMs, pRet, pOpts->pTraceUser);
}

static bool xrtImapIdleLoop(const ximapconfig* pCfg, const ximapidleloopopts* pOpts, ximapidlecallback pCallback, void* pUser, ximapidleloopsummary* pSummary, ximapresult* pRet)
{
	ximapidleloopopts tOpts;
	uint32 iDelay;
	size_t iCycles = 0u;
	bool bHadSuccess = FALSE;
	bool bLastFailed = FALSE;

	if ( pRet != NULL ) {
		xrtImapResultInit(pRet);
	}
	if ( pSummary != NULL ) {
		memset(pSummary, 0, sizeof(*pSummary));
	}
	if ( pCfg == NULL || pCallback == NULL ) {
		ximap_result_error(pRet, "IMAP IDLE loop config or callback missing");
		return FALSE;
	}
	xrtImapIdleLoopOptsInit(&tOpts);
	if ( pOpts != NULL ) {
		tOpts = *pOpts;
		if ( tOpts.iMaxReconnectDelayMs == 0u ) {
			tOpts.iMaxReconnectDelayMs = tOpts.iReconnectDelayMs;
		}
	}
	iDelay = tOpts.iReconnectDelayMs;
	ximap_idle_loop_trace(&tOpts, XIMAP_IDLE_LOOP_EVENT_START, 0u, 0u, 0u, pRet);
	while ( tOpts.iMaxCycles == 0u || iCycles < (size_t)tOpts.iMaxCycles ) {
		ximapclient* pClient = NULL;
		str sSelectRaw = NULL;
		size_t iCycleEvents = 0u;
		ximapidleloopcallbackctx tCallbackCtx;
		bool bCycleOK = FALSE;

		if ( tOpts.pShouldStop != NULL && tOpts.pShouldStop(tOpts.pStopUser) ) {
			if ( pSummary != NULL ) {
				pSummary->bStopped = TRUE;
			}
			ximap_idle_loop_trace(&tOpts, XIMAP_IDLE_LOOP_EVENT_STOP, iCycles, pSummary ? pSummary->iEvents : 0u, 0u, pRet);
			break;
		}
		memset(&tCallbackCtx, 0, sizeof(tCallbackCtx));
		tCallbackCtx.pUserCallback = pCallback;
		tCallbackCtx.pUser = pUser;
		bLastFailed = FALSE;
		iCycles++;
		if ( pSummary != NULL ) {
			pSummary->iCycles = iCycles;
		}
		ximap_idle_loop_trace(&tOpts, XIMAP_IDLE_LOOP_EVENT_CYCLE_START, iCycles, pSummary ? pSummary->iEvents : 0u, 0u, pRet);
		if ( !xrtImapOpen(pCfg, &pClient, pRet) ) {
			bLastFailed = TRUE;
		} else {
			if ( tOpts.sMailbox != NULL && tOpts.sMailbox[0] != '\0' ) {
				if ( !xrtImapSelect(pClient, tOpts.sMailbox, &sSelectRaw, pRet) ) {
					bLastFailed = TRUE;
				}
				if ( sSelectRaw != NULL ) {
					xrtFree(sSelectRaw);
					sSelectRaw = NULL;
				}
			}
			if ( !bLastFailed ) {
				bCycleOK = xrtImapIdleWatch(pClient, tOpts.iMaxEventsPerCycle, ximap_idle_loop_callback, &tCallbackCtx, &iCycleEvents, pRet);
				if ( bCycleOK ) {
					bHadSuccess = TRUE;
					bLastFailed = FALSE;
					if ( pSummary != NULL ) {
						pSummary->iEvents += iCycleEvents;
						pSummary->bCancelled = tCallbackCtx.bCancelled;
					}
					ximap_idle_loop_trace(&tOpts, XIMAP_IDLE_LOOP_EVENT_CYCLE_OK, iCycles, pSummary ? pSummary->iEvents : iCycleEvents, 0u, pRet);
				} else {
					bLastFailed = TRUE;
				}
			}
			if ( pClient != NULL ) {
				if ( bCycleOK ) {
					if ( !xrtImapLogout(pClient, pRet) ) {
						bLastFailed = TRUE;
						pClient = NULL;
					}
				} else {
					xrtImapClose(pClient);
				}
				pClient = NULL;
			}
		}
		if ( tCallbackCtx.bCancelled ) {
			ximap_idle_loop_trace(&tOpts, XIMAP_IDLE_LOOP_EVENT_CANCEL, iCycles, pSummary ? pSummary->iEvents : 0u, 0u, pRet);
			break;
		}
		if ( bLastFailed ) {
			ximap_idle_loop_trace(&tOpts, XIMAP_IDLE_LOOP_EVENT_CYCLE_ERROR, iCycles, pSummary ? pSummary->iEvents : 0u, 0u, pRet);
		}
		if ( tOpts.pHeartbeat != NULL && tOpts.iHeartbeatEveryCycles != 0u && (iCycles % (size_t)tOpts.iHeartbeatEveryCycles) == 0u ) {
			bool bHeartbeatContinue;
			if ( pSummary != NULL ) {
				pSummary->iHeartbeats++;
			}
			ximap_idle_loop_trace(&tOpts, XIMAP_IDLE_LOOP_EVENT_HEARTBEAT, iCycles, pSummary ? pSummary->iEvents : 0u, 0u, pRet);
			bHeartbeatContinue = tOpts.pHeartbeat(iCycles, pSummary ? pSummary->iEvents : 0u, pRet, tOpts.pHeartbeatUser);
			if ( !bHeartbeatContinue ) {
				if ( pSummary != NULL ) {
					pSummary->bHeartbeatStopped = TRUE;
				}
				break;
			}
		}
		if ( tOpts.pShouldStop != NULL && tOpts.pShouldStop(tOpts.pStopUser) ) {
			if ( pSummary != NULL ) {
				pSummary->bStopped = TRUE;
			}
			ximap_idle_loop_trace(&tOpts, XIMAP_IDLE_LOOP_EVENT_STOP, iCycles, pSummary ? pSummary->iEvents : 0u, 0u, pRet);
			break;
		}
		if ( tOpts.iMaxCycles != 0u && iCycles >= (size_t)tOpts.iMaxCycles ) {
			break;
		}
		if ( iDelay > 0u ) {
			ximap_idle_loop_trace(&tOpts, XIMAP_IDLE_LOOP_EVENT_SLEEP, iCycles, pSummary ? pSummary->iEvents : 0u, iDelay, pRet);
			xrtSleep(iDelay);
			if ( tOpts.iMaxReconnectDelayMs > 0u && iDelay < tOpts.iMaxReconnectDelayMs ) {
				uint32 iNext = iDelay > 0x7fffffffu ? tOpts.iMaxReconnectDelayMs : iDelay * 2u;
				iDelay = iNext > tOpts.iMaxReconnectDelayMs ? tOpts.iMaxReconnectDelayMs : iNext;
			}
		}
	}
	if ( pSummary != NULL ) {
		pSummary->bLastCycleFailed = bLastFailed;
	}
	ximap_idle_loop_trace(&tOpts, XIMAP_IDLE_LOOP_EVENT_DONE, iCycles, pSummary ? pSummary->iEvents : 0u, 0u, pRet);
	if ( bHadSuccess ) {
		if ( pRet != NULL ) {
			pRet->bSuccess = TRUE;
		}
		return TRUE;
	}
	return FALSE;
}

static bool xrtImapLogout(ximapclient* pClient, ximapresult* pRet)
{
	bool bOK;

	ximap_result_stage(pRet, XIMAP_STAGE_LOGOUT);
	bOK = ximap_command_raw(pClient, "LOGOUT", XIMAP_STAGE_LOGOUT, NULL, pRet);
	xrtImapClose(pClient);
	return bOK;
}

static void ximap_fetch_task_ctx_free(ximapfetchtaskctx* pCtx)
{
	if ( pCtx == NULL ) {
		return;
	}
	if ( pCtx->sUid != NULL ) xrtFree(pCtx->sUid);
	if ( pCtx->sHost != NULL ) xrtFree(pCtx->sHost);
	if ( pCtx->sUser != NULL ) xrtFree(pCtx->sUser);
	if ( pCtx->sPass != NULL ) xrtFree(pCtx->sPass);
	if ( pCtx->sOAuth2Token != NULL ) xrtFree(pCtx->sOAuth2Token);
	if ( pCtx->sCaFile != NULL ) xrtFree(pCtx->sCaFile);
	if ( pCtx->sDebugName != NULL ) xrtFree(pCtx->sDebugName);
	if ( pCtx->sMailbox != NULL ) xrtFree(pCtx->sMailbox);
	if ( pCtx->sReference != NULL ) xrtFree(pCtx->sReference);
	if ( pCtx->sPattern != NULL ) xrtFree(pCtx->sPattern);
	if ( pCtx->sCriteria != NULL ) xrtFree(pCtx->sCriteria);
	if ( pCtx->sOperation != NULL ) xrtFree(pCtx->sOperation);
	if ( pCtx->sFlags != NULL ) xrtFree(pCtx->sFlags);
	if ( pCtx->sSection != NULL ) xrtFree(pCtx->sSection);
	if ( pCtx->arrFields != NULL ) {
		size_t i;
		for ( i = 0u; i < pCtx->iFieldCount; i++ ) {
			if ( pCtx->arrFields[i] != NULL ) {
				xrtFree(pCtx->arrFields[i]);
			}
		}
		xrtFree(pCtx->arrFields);
	}
	xrtFree(pCtx);
}

static ximapfetchtaskctx* ximap_fetch_task_ctx_create(const ximapconfig* pCfg, const char* sUid, const ximapasyncopts* pOpts, bool bParseMime, bool bExtractLiteral, bool bPeek)
{
	ximapfetchtaskctx* pCtx;

	if ( pCfg == NULL || sUid == NULL || sUid[0] == '\0' ) {
		return NULL;
	}
	pCtx = (ximapfetchtaskctx*)xrtCalloc(1, sizeof(*pCtx));
	if ( pCtx == NULL ) {
		return NULL;
	}
	pCtx->tCfg = *pCfg;
	if ( pOpts != NULL ) {
		pCtx->tOpts = *pOpts;
	}
	pCtx->bParseMime = bParseMime;
	pCtx->bExtractLiteral = bExtractLiteral;
	pCtx->bPeek = bPeek;
	pCtx->sUid = ximap_dup_text(sUid);
	pCtx->sHost = ximap_dup_text(pCfg->sHost);
	pCtx->sUser = ximap_dup_text(pCfg->sUser);
	pCtx->sPass = ximap_dup_text(pCfg->sPass);
	pCtx->sOAuth2Token = ximap_dup_text(pCfg->sOAuth2Token);
	pCtx->sCaFile = ximap_dup_text(pCfg->sCaFile);
	pCtx->sDebugName = ximap_dup_text(pCtx->tOpts.sDebugName);
	pCtx->sMailbox = ximap_dup_text(pCtx->tOpts.sMailbox);
	if ( pCtx->sUid == NULL
		|| (pCfg->sHost != NULL && pCtx->sHost == NULL)
		|| (pCfg->sUser != NULL && pCtx->sUser == NULL)
		|| (pCfg->sPass != NULL && pCtx->sPass == NULL)
		|| (pCfg->sOAuth2Token != NULL && pCtx->sOAuth2Token == NULL)
		|| (pCfg->sCaFile != NULL && pCtx->sCaFile == NULL)
		|| (pCtx->tOpts.sDebugName != NULL && pCtx->sDebugName == NULL)
		|| (pCtx->tOpts.sMailbox != NULL && pCtx->sMailbox == NULL) ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	pCtx->tCfg.sHost = pCtx->sHost;
	pCtx->tCfg.sUser = pCtx->sUser;
	pCtx->tCfg.sPass = pCtx->sPass;
	pCtx->tCfg.sOAuth2Token = pCtx->sOAuth2Token;
	pCtx->tCfg.sCaFile = pCtx->sCaFile;
	pCtx->tOpts.sDebugName = pCtx->sDebugName;
	pCtx->tOpts.sMailbox = pCtx->sMailbox;
	if ( pCtx->tOpts.iTimeoutMs > 0u ) {
		pCtx->tCfg.iTimeoutMs = pCtx->tOpts.iTimeoutMs;
	}
	return pCtx;
}

static ximapfetchtaskctx* ximap_mailbox_task_ctx_create(const ximapconfig* pCfg, const char* sMailbox, const ximapasyncopts* pOpts)
{
	ximapfetchtaskctx* pCtx;

	if ( pCfg == NULL || sMailbox == NULL || sMailbox[0] == '\0' ) {
		return NULL;
	}
	pCtx = (ximapfetchtaskctx*)xrtCalloc(1, sizeof(*pCtx));
	if ( pCtx == NULL ) {
		return NULL;
	}
	pCtx->tCfg = *pCfg;
	if ( pOpts != NULL ) {
		pCtx->tOpts = *pOpts;
	}
	pCtx->sHost = ximap_dup_text(pCfg->sHost);
	pCtx->sUser = ximap_dup_text(pCfg->sUser);
	pCtx->sPass = ximap_dup_text(pCfg->sPass);
	pCtx->sOAuth2Token = ximap_dup_text(pCfg->sOAuth2Token);
	pCtx->sCaFile = ximap_dup_text(pCfg->sCaFile);
	pCtx->sDebugName = ximap_dup_text(pCtx->tOpts.sDebugName);
	pCtx->sMailbox = ximap_dup_text(sMailbox);
	if ( (pCfg->sHost != NULL && pCtx->sHost == NULL)
		|| (pCfg->sUser != NULL && pCtx->sUser == NULL)
		|| (pCfg->sPass != NULL && pCtx->sPass == NULL)
		|| (pCfg->sOAuth2Token != NULL && pCtx->sOAuth2Token == NULL)
		|| (pCfg->sCaFile != NULL && pCtx->sCaFile == NULL)
		|| (pCtx->tOpts.sDebugName != NULL && pCtx->sDebugName == NULL)
		|| pCtx->sMailbox == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	pCtx->tCfg.sHost = pCtx->sHost;
	pCtx->tCfg.sUser = pCtx->sUser;
	pCtx->tCfg.sPass = pCtx->sPass;
	pCtx->tCfg.sOAuth2Token = pCtx->sOAuth2Token;
	pCtx->tCfg.sCaFile = pCtx->sCaFile;
	pCtx->tOpts.sDebugName = pCtx->sDebugName;
	if ( pCtx->tOpts.iTimeoutMs > 0u ) {
		pCtx->tCfg.iTimeoutMs = pCtx->tOpts.iTimeoutMs;
	}
	return pCtx;
}

static ximapfetchtaskctx* ximap_list_task_ctx_create(const ximapconfig* pCfg, const char* sReference, const char* sPattern, const ximapasyncopts* pOpts)
{
	ximapfetchtaskctx* pCtx;

	if ( pCfg == NULL || sPattern == NULL ) {
		return NULL;
	}
	pCtx = (ximapfetchtaskctx*)xrtCalloc(1, sizeof(*pCtx));
	if ( pCtx == NULL ) {
		return NULL;
	}
	pCtx->tCfg = *pCfg;
	if ( pOpts != NULL ) {
		pCtx->tOpts = *pOpts;
	}
	pCtx->sHost = ximap_dup_text(pCfg->sHost);
	pCtx->sUser = ximap_dup_text(pCfg->sUser);
	pCtx->sPass = ximap_dup_text(pCfg->sPass);
	pCtx->sOAuth2Token = ximap_dup_text(pCfg->sOAuth2Token);
	pCtx->sCaFile = ximap_dup_text(pCfg->sCaFile);
	pCtx->sDebugName = ximap_dup_text(pCtx->tOpts.sDebugName);
	pCtx->sReference = ximap_dup_text(ximap_str_or_empty(sReference));
	pCtx->sPattern = ximap_dup_text(sPattern);
	if ( (pCfg->sHost != NULL && pCtx->sHost == NULL)
		|| (pCfg->sUser != NULL && pCtx->sUser == NULL)
		|| (pCfg->sPass != NULL && pCtx->sPass == NULL)
		|| (pCfg->sOAuth2Token != NULL && pCtx->sOAuth2Token == NULL)
		|| (pCfg->sCaFile != NULL && pCtx->sCaFile == NULL)
		|| (pCtx->tOpts.sDebugName != NULL && pCtx->sDebugName == NULL)
		|| pCtx->sReference == NULL || pCtx->sPattern == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	pCtx->tCfg.sHost = pCtx->sHost;
	pCtx->tCfg.sUser = pCtx->sUser;
	pCtx->tCfg.sPass = pCtx->sPass;
	pCtx->tCfg.sOAuth2Token = pCtx->sOAuth2Token;
	pCtx->tCfg.sCaFile = pCtx->sCaFile;
	pCtx->tOpts.sDebugName = pCtx->sDebugName;
	if ( pCtx->tOpts.iTimeoutMs > 0u ) {
		pCtx->tCfg.iTimeoutMs = pCtx->tOpts.iTimeoutMs;
	}
	return pCtx;
}

static ximapfetchtaskctx* ximap_search_task_ctx_create(const ximapconfig* pCfg, const char* sCriteria, const ximapasyncopts* pOpts, bool bUidSearch)
{
	ximapfetchtaskctx* pCtx;

	if ( pCfg == NULL || sCriteria == NULL || sCriteria[0] == '\0' ) {
		return NULL;
	}
	pCtx = (ximapfetchtaskctx*)xrtCalloc(1, sizeof(*pCtx));
	if ( pCtx == NULL ) {
		return NULL;
	}
	pCtx->tCfg = *pCfg;
	if ( pOpts != NULL ) {
		pCtx->tOpts = *pOpts;
	}
	pCtx->bUidSearch = bUidSearch;
	pCtx->sHost = ximap_dup_text(pCfg->sHost);
	pCtx->sUser = ximap_dup_text(pCfg->sUser);
	pCtx->sPass = ximap_dup_text(pCfg->sPass);
	pCtx->sOAuth2Token = ximap_dup_text(pCfg->sOAuth2Token);
	pCtx->sCaFile = ximap_dup_text(pCfg->sCaFile);
	pCtx->sDebugName = ximap_dup_text(pCtx->tOpts.sDebugName);
	pCtx->sMailbox = ximap_dup_text(pCtx->tOpts.sMailbox);
	pCtx->sCriteria = ximap_dup_text(sCriteria);
	if ( (pCfg->sHost != NULL && pCtx->sHost == NULL)
		|| (pCfg->sUser != NULL && pCtx->sUser == NULL)
		|| (pCfg->sPass != NULL && pCtx->sPass == NULL)
		|| (pCfg->sOAuth2Token != NULL && pCtx->sOAuth2Token == NULL)
		|| (pCfg->sCaFile != NULL && pCtx->sCaFile == NULL)
		|| (pCtx->tOpts.sDebugName != NULL && pCtx->sDebugName == NULL)
		|| (pCtx->tOpts.sMailbox != NULL && pCtx->sMailbox == NULL)
		|| pCtx->sCriteria == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	pCtx->tCfg.sHost = pCtx->sHost;
	pCtx->tCfg.sUser = pCtx->sUser;
	pCtx->tCfg.sPass = pCtx->sPass;
	pCtx->tCfg.sOAuth2Token = pCtx->sOAuth2Token;
	pCtx->tCfg.sCaFile = pCtx->sCaFile;
	pCtx->tOpts.sDebugName = pCtx->sDebugName;
	pCtx->tOpts.sMailbox = pCtx->sMailbox;
	if ( pCtx->tOpts.iTimeoutMs > 0u ) {
		pCtx->tCfg.iTimeoutMs = pCtx->tOpts.iTimeoutMs;
	}
	return pCtx;
}

static ximapfetchtaskctx* ximap_store_flags_task_ctx_create(const ximapconfig* pCfg, const char* sSequenceOrUid, const char* sOperation, const char* sFlags, const ximapasyncopts* pOpts, bool bUidStore)
{
	ximapfetchtaskctx* pCtx;

	if ( pCfg == NULL || sSequenceOrUid == NULL || sSequenceOrUid[0] == '\0'
		|| sOperation == NULL || sOperation[0] == '\0'
		|| sFlags == NULL || sFlags[0] == '\0' ) {
		return NULL;
	}
	pCtx = ximap_fetch_task_ctx_create(pCfg, sSequenceOrUid, pOpts, FALSE, FALSE, FALSE);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pCtx->sOperation = ximap_dup_text(sOperation);
	pCtx->sFlags = ximap_dup_text(sFlags);
	if ( pCtx->sOperation == NULL || pCtx->sFlags == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	pCtx->bStoreFlags = TRUE;
	pCtx->bUidStore = bUidStore;
	return pCtx;
}

static ximapfetchtaskctx* ximap_expunge_task_ctx_create(const ximapconfig* pCfg, const char* sUidSet, const ximapasyncopts* pOpts, bool bUidExpunge)
{
	ximapfetchtaskctx* pCtx;

	if ( pCfg == NULL ) {
		return NULL;
	}
	if ( bUidExpunge && !ximap_uid_set_is_safe(sUidSet) ) {
		return NULL;
	}
	pCtx = (ximapfetchtaskctx*)xrtCalloc(1, sizeof(*pCtx));
	if ( pCtx == NULL ) {
		return NULL;
	}
	pCtx->tCfg = *pCfg;
	if ( pOpts != NULL ) {
		pCtx->tOpts = *pOpts;
	}
	pCtx->bUidExpunge = bUidExpunge;
	pCtx->sHost = ximap_dup_text(pCfg->sHost);
	pCtx->sUser = ximap_dup_text(pCfg->sUser);
	pCtx->sPass = ximap_dup_text(pCfg->sPass);
	pCtx->sOAuth2Token = ximap_dup_text(pCfg->sOAuth2Token);
	pCtx->sCaFile = ximap_dup_text(pCfg->sCaFile);
	pCtx->sDebugName = ximap_dup_text(pCtx->tOpts.sDebugName);
	pCtx->sMailbox = ximap_dup_text(pCtx->tOpts.sMailbox);
	pCtx->sUid = bUidExpunge ? ximap_dup_text(sUidSet) : NULL;
	if ( (pCfg->sHost != NULL && pCtx->sHost == NULL)
		|| (pCfg->sUser != NULL && pCtx->sUser == NULL)
		|| (pCfg->sPass != NULL && pCtx->sPass == NULL)
		|| (pCfg->sOAuth2Token != NULL && pCtx->sOAuth2Token == NULL)
		|| (pCfg->sCaFile != NULL && pCtx->sCaFile == NULL)
		|| (pCtx->tOpts.sDebugName != NULL && pCtx->sDebugName == NULL)
		|| (pCtx->tOpts.sMailbox != NULL && pCtx->sMailbox == NULL)
		|| (bUidExpunge && pCtx->sUid == NULL) ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	pCtx->tCfg.sHost = pCtx->sHost;
	pCtx->tCfg.sUser = pCtx->sUser;
	pCtx->tCfg.sPass = pCtx->sPass;
	pCtx->tCfg.sOAuth2Token = pCtx->sOAuth2Token;
	pCtx->tCfg.sCaFile = pCtx->sCaFile;
	pCtx->tOpts.sDebugName = pCtx->sDebugName;
	pCtx->tOpts.sMailbox = pCtx->sMailbox;
	if ( pCtx->tOpts.iTimeoutMs > 0u ) {
		pCtx->tCfg.iTimeoutMs = pCtx->tOpts.iTimeoutMs;
	}
	return pCtx;
}

static ximapfetchtaskctx* ximap_idle_task_ctx_create(const ximapconfig* pCfg, size_t iMaxEvents, const ximapasyncopts* pOpts)
{
	ximapfetchtaskctx* pCtx;

	if ( pCfg == NULL || iMaxEvents == 0u ) {
		return NULL;
	}
	pCtx = (ximapfetchtaskctx*)xrtCalloc(1, sizeof(*pCtx));
	if ( pCtx == NULL ) {
		return NULL;
	}
	pCtx->tCfg = *pCfg;
	if ( pOpts != NULL ) {
		pCtx->tOpts = *pOpts;
	}
	pCtx->iMaxEvents = iMaxEvents;
	pCtx->sHost = ximap_dup_text(pCfg->sHost);
	pCtx->sUser = ximap_dup_text(pCfg->sUser);
	pCtx->sPass = ximap_dup_text(pCfg->sPass);
	pCtx->sOAuth2Token = ximap_dup_text(pCfg->sOAuth2Token);
	pCtx->sCaFile = ximap_dup_text(pCfg->sCaFile);
	pCtx->sDebugName = ximap_dup_text(pCtx->tOpts.sDebugName);
	pCtx->sMailbox = ximap_dup_text(pCtx->tOpts.sMailbox);
	if ( (pCfg->sHost != NULL && pCtx->sHost == NULL)
		|| (pCfg->sUser != NULL && pCtx->sUser == NULL)
		|| (pCfg->sPass != NULL && pCtx->sPass == NULL)
		|| (pCfg->sOAuth2Token != NULL && pCtx->sOAuth2Token == NULL)
		|| (pCfg->sCaFile != NULL && pCtx->sCaFile == NULL)
		|| (pCtx->tOpts.sDebugName != NULL && pCtx->sDebugName == NULL)
		|| (pCtx->tOpts.sMailbox != NULL && pCtx->sMailbox == NULL) ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	pCtx->tCfg.sHost = pCtx->sHost;
	pCtx->tCfg.sUser = pCtx->sUser;
	pCtx->tCfg.sPass = pCtx->sPass;
	pCtx->tCfg.sOAuth2Token = pCtx->sOAuth2Token;
	pCtx->tCfg.sCaFile = pCtx->sCaFile;
	pCtx->tOpts.sDebugName = pCtx->sDebugName;
	pCtx->tOpts.sMailbox = pCtx->sMailbox;
	if ( pCtx->tOpts.iTimeoutMs > 0u ) {
		pCtx->tCfg.iTimeoutMs = pCtx->tOpts.iTimeoutMs;
	}
	return pCtx;
}

static ximapfetchtaskctx* ximap_idle_watch_task_ctx_create(const ximapconfig* pCfg, size_t iMaxEvents, ximapidlecallback pCallback, void* pUser, const ximapasyncopts* pOpts)
{
	ximapfetchtaskctx* pCtx;

	if ( pCfg == NULL || pCallback == NULL ) {
		return NULL;
	}
	pCtx = (ximapfetchtaskctx*)xrtCalloc(1, sizeof(*pCtx));
	if ( pCtx == NULL ) {
		return NULL;
	}
	pCtx->tCfg = *pCfg;
	if ( pOpts != NULL ) {
		pCtx->tOpts = *pOpts;
	}
	pCtx->iMaxEvents = iMaxEvents;
	pCtx->pIdleCallback = pCallback;
	pCtx->pIdleUser = pUser;
	pCtx->bIdleWatch = TRUE;
	pCtx->sHost = ximap_dup_text(pCfg->sHost);
	pCtx->sUser = ximap_dup_text(pCfg->sUser);
	pCtx->sPass = ximap_dup_text(pCfg->sPass);
	pCtx->sOAuth2Token = ximap_dup_text(pCfg->sOAuth2Token);
	pCtx->sCaFile = ximap_dup_text(pCfg->sCaFile);
	pCtx->sDebugName = ximap_dup_text(pCtx->tOpts.sDebugName);
	pCtx->sMailbox = ximap_dup_text(pCtx->tOpts.sMailbox);
	if ( (pCfg->sHost != NULL && pCtx->sHost == NULL)
		|| (pCfg->sUser != NULL && pCtx->sUser == NULL)
		|| (pCfg->sPass != NULL && pCtx->sPass == NULL)
		|| (pCfg->sOAuth2Token != NULL && pCtx->sOAuth2Token == NULL)
		|| (pCfg->sCaFile != NULL && pCtx->sCaFile == NULL)
		|| (pCtx->tOpts.sDebugName != NULL && pCtx->sDebugName == NULL)
		|| (pCtx->tOpts.sMailbox != NULL && pCtx->sMailbox == NULL) ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	pCtx->tCfg.sHost = pCtx->sHost;
	pCtx->tCfg.sUser = pCtx->sUser;
	pCtx->tCfg.sPass = pCtx->sPass;
	pCtx->tCfg.sOAuth2Token = pCtx->sOAuth2Token;
	pCtx->tCfg.sCaFile = pCtx->sCaFile;
	pCtx->tOpts.sDebugName = pCtx->sDebugName;
	pCtx->tOpts.sMailbox = pCtx->sMailbox;
	if ( pCtx->tOpts.iTimeoutMs > 0u ) {
		pCtx->tCfg.iTimeoutMs = pCtx->tOpts.iTimeoutMs;
	}
	return pCtx;
}

static ximapfetchtaskctx* ximap_body_section_task_ctx_create(const ximapconfig* pCfg, const char* sUid, const char* sSection, const ximapasyncopts* pOpts, bool bPeek)
{
	ximapfetchtaskctx* pCtx;

	if ( !ximap_body_section_is_safe(sSection) ) {
		return NULL;
	}
	pCtx = ximap_fetch_task_ctx_create(pCfg, sUid, pOpts, FALSE, FALSE, bPeek);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pCtx->sSection = ximap_dup_text(sSection);
	if ( pCtx->sSection == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	pCtx->bBodySection = TRUE;
	return pCtx;
}

static ximapfetchtaskctx* ximap_header_fields_task_ctx_create(const ximapconfig* pCfg, const char* sUid, const char* const* arrFields, size_t iFieldCount, const ximapasyncopts* pOpts, bool bExtractLiteral, bool bPeek)
{
	ximapfetchtaskctx* pCtx;
	size_t i;

	if ( arrFields == NULL || iFieldCount == 0u ) {
		return NULL;
	}
	for ( i = 0u; i < iFieldCount; i++ ) {
		if ( !ximap_header_field_is_safe(arrFields[i]) ) {
			return NULL;
		}
	}
	pCtx = ximap_fetch_task_ctx_create(pCfg, sUid, pOpts, FALSE, bExtractLiteral, bPeek);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pCtx->arrFields = (char**)xrtCalloc(iFieldCount, sizeof(char*));
	if ( pCtx->arrFields == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	pCtx->iFieldCount = iFieldCount;
	pCtx->bHeaderFields = TRUE;
	for ( i = 0u; i < iFieldCount; i++ ) {
		pCtx->arrFields[i] = ximap_dup_text(arrFields[i]);
		if ( pCtx->arrFields[i] == NULL ) {
			ximap_fetch_task_ctx_free(pCtx);
			return NULL;
		}
	}
	return pCtx;
}

static bool ximap_fetch_run_common(ximapfetchtaskctx* pCtx, ximapresult* pRet, str* psRaw, xmailparsedmessage* pMsg)
{
	ximapclient* pClient = NULL;
	str sSelectRaw = NULL;
	bool bOK = FALSE;

	if ( pRet != NULL ) {
		xrtImapResultInit(pRet);
	}
	if ( pCtx == NULL ) {
		ximap_result_error(pRet, "IMAP async task context missing");
		return FALSE;
	}
	if ( psRaw != NULL ) {
		*psRaw = NULL;
	}
	if ( pMsg != NULL ) {
		memset(pMsg, 0, sizeof(*pMsg));
	}
	if ( !xrtImapOpen(&pCtx->tCfg, &pClient, pRet) ) {
		goto cleanup;
	}
	if ( pCtx->tOpts.sMailbox != NULL && pCtx->tOpts.sMailbox[0] != '\0' ) {
		if ( !xrtImapSelect(pClient, pCtx->tOpts.sMailbox, &sSelectRaw, pRet) ) {
			goto cleanup;
		}
		xrtFree(sSelectRaw);
		sSelectRaw = NULL;
	}
	if ( pCtx->bBodySection ) {
		bOK = pCtx->bPeek
			? xrtImapUidPeekBodySectionRaw(pClient, pCtx->sUid, pCtx->sSection, psRaw, pRet)
			: xrtImapUidFetchBodySectionRaw(pClient, pCtx->sUid, pCtx->sSection, psRaw, pRet);
	} else if ( pCtx->bHeaderFields ) {
		if ( pCtx->bExtractLiteral ) {
			bOK = pCtx->bPeek
				? xrtImapUidPeekHeaderFields(pClient, pCtx->sUid, (const char* const*)pCtx->arrFields, pCtx->iFieldCount, psRaw, pRet)
				: xrtImapUidFetchHeaderFields(pClient, pCtx->sUid, (const char* const*)pCtx->arrFields, pCtx->iFieldCount, psRaw, pRet);
		} else {
			bOK = pCtx->bPeek
				? xrtImapUidPeekHeaderFieldsRaw(pClient, pCtx->sUid, (const char* const*)pCtx->arrFields, pCtx->iFieldCount, psRaw, pRet)
				: xrtImapUidFetchHeaderFieldsRaw(pClient, pCtx->sUid, (const char* const*)pCtx->arrFields, pCtx->iFieldCount, psRaw, pRet);
		}
	} else if ( pMsg != NULL ) {
		bOK = pCtx->bPeek ? xrtImapUidPeekMime(pClient, pCtx->sUid, pMsg, pRet) : xrtImapUidFetchMime(pClient, pCtx->sUid, pMsg, pRet);
	} else if ( pCtx->bExtractLiteral ) {
		bOK = pCtx->bPeek ? xrtImapUidPeekBodyLiteral(pClient, pCtx->sUid, psRaw, pRet) : xrtImapUidFetchBodyLiteral(pClient, pCtx->sUid, psRaw, pRet);
	} else {
		bOK = pCtx->bPeek ? xrtImapUidPeekBodyRaw(pClient, pCtx->sUid, psRaw, pRet) : xrtImapUidFetchBodyRaw(pClient, pCtx->sUid, psRaw, pRet);
	}
	if ( !bOK ) {
		goto cleanup;
	}
	if ( !xrtImapLogout(pClient, pRet) ) {
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
	if ( sSelectRaw != NULL ) {
		xrtFree(sSelectRaw);
	}
	if ( pClient != NULL ) {
		xrtImapClose(pClient);
	}
	if ( !bOK && psRaw != NULL && *psRaw != NULL ) {
		xrtFree(*psRaw);
		*psRaw = NULL;
	}
	return bOK;
}

static bool ximap_status_run_common(ximapfetchtaskctx* pCtx, ximapresult* pRet, ximapmailboxstatus* pStatus)
{
	ximapclient* pClient = NULL;
	bool bOK = FALSE;

	if ( pRet != NULL ) {
		xrtImapResultInit(pRet);
	}
	if ( pStatus != NULL ) {
		memset(pStatus, 0, sizeof(*pStatus));
	}
	if ( pCtx == NULL ) {
		ximap_result_error(pRet, "IMAP async STATUS task context missing");
		return FALSE;
	}
	if ( pStatus == NULL ) {
		ximap_result_error(pRet, "IMAP async STATUS output missing");
		return FALSE;
	}
	if ( !xrtImapOpen(&pCtx->tCfg, &pClient, pRet) ) {
		goto cleanup;
	}
	if ( !xrtImapStatus(pClient, pCtx->sMailbox, pStatus, pRet) ) {
		goto cleanup;
	}
	if ( !xrtImapLogout(pClient, pRet) ) {
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
		xrtImapClose(pClient);
	}
	return bOK;
}

static bool ximap_list_run_common(ximapfetchtaskctx* pCtx, ximapresult* pRet, ximaplist* pList)
{
	ximapclient* pClient = NULL;
	bool bOK = FALSE;

	if ( pRet != NULL ) {
		xrtImapResultInit(pRet);
	}
	if ( pList != NULL ) {
		memset(pList, 0, sizeof(*pList));
	}
	if ( pCtx == NULL ) {
		ximap_result_error(pRet, "IMAP async LIST task context missing");
		return FALSE;
	}
	if ( pList == NULL ) {
		ximap_result_error(pRet, "IMAP async LIST output missing");
		return FALSE;
	}
	if ( !xrtImapOpen(&pCtx->tCfg, &pClient, pRet) ) {
		goto cleanup;
	}
	if ( !xrtImapList(pClient, pCtx->sReference, pCtx->sPattern, pList, pRet) ) {
		goto cleanup;
	}
	if ( !xrtImapLogout(pClient, pRet) ) {
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
		xrtImapClose(pClient);
	}
	if ( !bOK && pList != NULL ) {
		xrtImapListFree(pList);
	}
	return bOK;
}

static bool ximap_search_run_common(ximapfetchtaskctx* pCtx, ximapresult* pRet, ximapsearchids* pIds)
{
	ximapclient* pClient = NULL;
	str sSelectRaw = NULL;
	bool bOK = FALSE;

	if ( pRet != NULL ) {
		xrtImapResultInit(pRet);
	}
	if ( pIds != NULL ) {
		memset(pIds, 0, sizeof(*pIds));
	}
	if ( pCtx == NULL ) {
		ximap_result_error(pRet, "IMAP async SEARCH task context missing");
		return FALSE;
	}
	if ( pIds == NULL ) {
		ximap_result_error(pRet, "IMAP async SEARCH output missing");
		return FALSE;
	}
	if ( !xrtImapOpen(&pCtx->tCfg, &pClient, pRet) ) {
		goto cleanup;
	}
	if ( pCtx->tOpts.sMailbox != NULL && pCtx->tOpts.sMailbox[0] != '\0' ) {
		if ( !xrtImapSelect(pClient, pCtx->tOpts.sMailbox, &sSelectRaw, pRet) ) {
			goto cleanup;
		}
		xrtFree(sSelectRaw);
		sSelectRaw = NULL;
	}
	bOK = pCtx->bUidSearch
		? xrtImapUidSearch(pClient, pCtx->sCriteria, pIds, pRet)
		: xrtImapSearch(pClient, pCtx->sCriteria, pIds, pRet);
	if ( !bOK ) {
		goto cleanup;
	}
	if ( !xrtImapLogout(pClient, pRet) ) {
		pClient = NULL;
		goto cleanup;
	}
	pClient = NULL;
	bOK = TRUE;
	if ( pRet != NULL ) {
		pRet->bSuccess = TRUE;
	}

cleanup:
	if ( sSelectRaw != NULL ) {
		xrtFree(sSelectRaw);
	}
	if ( pClient != NULL ) {
		xrtImapClose(pClient);
	}
	if ( !bOK && pIds != NULL ) {
		xrtImapSearchIdsFree(pIds);
	}
	return bOK;
}

static bool ximap_bodystructure_run_common(ximapfetchtaskctx* pCtx, ximapresult* pRet, ximapbodystructure* pBody)
{
	ximapclient* pClient = NULL;
	str sSelectRaw = NULL;
	bool bOK = FALSE;

	if ( pRet != NULL ) {
		xrtImapResultInit(pRet);
	}
	if ( pBody != NULL ) {
		memset(pBody, 0, sizeof(*pBody));
	}
	if ( pCtx == NULL ) {
		ximap_result_error(pRet, "IMAP async BODYSTRUCTURE task context missing");
		return FALSE;
	}
	if ( pBody == NULL ) {
		ximap_result_error(pRet, "IMAP async BODYSTRUCTURE output missing");
		return FALSE;
	}
	if ( !xrtImapOpen(&pCtx->tCfg, &pClient, pRet) ) {
		goto cleanup;
	}
	if ( pCtx->tOpts.sMailbox != NULL && pCtx->tOpts.sMailbox[0] != '\0' ) {
		if ( !xrtImapSelect(pClient, pCtx->tOpts.sMailbox, &sSelectRaw, pRet) ) {
			goto cleanup;
		}
		xrtFree(sSelectRaw);
		sSelectRaw = NULL;
	}
	if ( !xrtImapFetchBodyStructure(pClient, pCtx->sUid, pBody, pRet) ) {
		goto cleanup;
	}
	if ( !xrtImapLogout(pClient, pRet) ) {
		pClient = NULL;
		goto cleanup;
	}
	pClient = NULL;
	bOK = TRUE;
	if ( pRet != NULL ) {
		pRet->bSuccess = TRUE;
	}

cleanup:
	if ( sSelectRaw != NULL ) {
		xrtFree(sSelectRaw);
	}
	if ( pClient != NULL ) {
		xrtImapClose(pClient);
	}
	if ( !bOK && pBody != NULL ) {
		xrtImapBodyStructureFree(pBody);
	}
	return bOK;
}

static bool ximap_flags_run_common(ximapfetchtaskctx* pCtx, ximapresult* pRet, ximapflags* pFlags)
{
	ximapclient* pClient = NULL;
	str sSelectRaw = NULL;
	bool bOK = FALSE;

	if ( pRet != NULL ) {
		xrtImapResultInit(pRet);
	}
	if ( pFlags != NULL ) {
		memset(pFlags, 0, sizeof(*pFlags));
	}
	if ( pCtx == NULL ) {
		ximap_result_error(pRet, "IMAP async FLAGS task context missing");
		return FALSE;
	}
	if ( pFlags == NULL ) {
		ximap_result_error(pRet, "IMAP async FLAGS output missing");
		return FALSE;
	}
	if ( !xrtImapOpen(&pCtx->tCfg, &pClient, pRet) ) {
		goto cleanup;
	}
	if ( pCtx->tOpts.sMailbox != NULL && pCtx->tOpts.sMailbox[0] != '\0' ) {
		if ( !xrtImapSelect(pClient, pCtx->tOpts.sMailbox, &sSelectRaw, pRet) ) {
			goto cleanup;
		}
		xrtFree(sSelectRaw);
		sSelectRaw = NULL;
	}
	if ( pCtx->bStoreFlags ) {
		if ( pCtx->bUidStore ) {
			if ( !xrtImapUidStoreFlagsParsed(pClient, pCtx->sUid, pCtx->sOperation, pCtx->sFlags, pFlags, pRet) ) {
				goto cleanup;
			}
		} else if ( !xrtImapStoreFlagsParsed(pClient, pCtx->sUid, pCtx->sOperation, pCtx->sFlags, pFlags, pRet) ) {
			goto cleanup;
		}
	} else if ( !xrtImapFetchFlags(pClient, pCtx->sUid, pFlags, pRet) ) {
		goto cleanup;
	}
	if ( !xrtImapLogout(pClient, pRet) ) {
		pClient = NULL;
		goto cleanup;
	}
	pClient = NULL;
	bOK = TRUE;
	if ( pRet != NULL ) {
		pRet->bSuccess = TRUE;
	}

cleanup:
	if ( sSelectRaw != NULL ) {
		xrtFree(sSelectRaw);
	}
	if ( pClient != NULL ) {
		xrtImapClose(pClient);
	}
	return bOK;
}

static bool ximap_expunge_run_common(ximapfetchtaskctx* pCtx, ximapresult* pRet, ximapexpungeresult* pExpunge)
{
	ximapclient* pClient = NULL;
	str sSelectRaw = NULL;
	bool bOK = FALSE;

	if ( pRet != NULL ) {
		xrtImapResultInit(pRet);
	}
	if ( pExpunge != NULL ) {
		memset(pExpunge, 0, sizeof(*pExpunge));
	}
	if ( pCtx == NULL ) {
		ximap_result_error(pRet, "IMAP async EXPUNGE task context missing");
		return FALSE;
	}
	if ( pExpunge == NULL ) {
		ximap_result_error(pRet, "IMAP async EXPUNGE output missing");
		return FALSE;
	}
	if ( !xrtImapOpen(&pCtx->tCfg, &pClient, pRet) ) {
		goto cleanup;
	}
	if ( pCtx->tOpts.sMailbox != NULL && pCtx->tOpts.sMailbox[0] != '\0' ) {
		if ( !xrtImapSelect(pClient, pCtx->tOpts.sMailbox, &sSelectRaw, pRet) ) {
			goto cleanup;
		}
		xrtFree(sSelectRaw);
		sSelectRaw = NULL;
	}
	if ( pCtx->bUidExpunge ) {
		if ( !xrtImapUidExpunge(pClient, pCtx->sUid, pExpunge, pRet) ) {
			goto cleanup;
		}
	} else if ( !xrtImapExpunge(pClient, pExpunge, pRet) ) {
		goto cleanup;
	}
	if ( !xrtImapLogout(pClient, pRet) ) {
		pClient = NULL;
		goto cleanup;
	}
	pClient = NULL;
	bOK = TRUE;
	if ( pRet != NULL ) {
		pRet->bSuccess = TRUE;
	}

cleanup:
	if ( sSelectRaw != NULL ) {
		xrtFree(sSelectRaw);
	}
	if ( pClient != NULL ) {
		xrtImapClose(pClient);
	}
	if ( !bOK && pExpunge != NULL ) {
		xrtImapExpungeResultFree(pExpunge);
	}
	return bOK;
}

static bool ximap_idle_run_common(ximapfetchtaskctx* pCtx, ximapresult* pRet, str** ppEvents, size_t* pEventCount)
{
	ximapclient* pClient = NULL;
	str sSelectRaw = NULL;
	bool bOK = FALSE;

	if ( pRet != NULL ) {
		xrtImapResultInit(pRet);
	}
	if ( ppEvents != NULL ) {
		*ppEvents = NULL;
	}
	if ( pEventCount != NULL ) {
		*pEventCount = 0u;
	}
	if ( pCtx == NULL ) {
		ximap_result_error(pRet, "IMAP async IDLE task context missing");
		return FALSE;
	}
	if ( ppEvents == NULL || pEventCount == NULL || pCtx->iMaxEvents == 0u ) {
		ximap_result_error(pRet, "IMAP async IDLE output missing");
		return FALSE;
	}
	if ( !xrtImapOpen(&pCtx->tCfg, &pClient, pRet) ) {
		goto cleanup;
	}
	if ( pCtx->tOpts.sMailbox != NULL && pCtx->tOpts.sMailbox[0] != '\0' ) {
		if ( !xrtImapSelect(pClient, pCtx->tOpts.sMailbox, &sSelectRaw, pRet) ) {
			goto cleanup;
		}
		xrtFree(sSelectRaw);
		sSelectRaw = NULL;
	}
	if ( !xrtImapIdleCollect(pClient, pCtx->iMaxEvents, ppEvents, pEventCount, pRet) ) {
		goto cleanup;
	}
	if ( !xrtImapLogout(pClient, pRet) ) {
		pClient = NULL;
		goto cleanup;
	}
	pClient = NULL;
	bOK = TRUE;
	if ( pRet != NULL ) {
		pRet->bSuccess = TRUE;
	}

cleanup:
	if ( sSelectRaw != NULL ) {
		xrtFree(sSelectRaw);
	}
	if ( pClient != NULL ) {
		xrtImapClose(pClient);
	}
	if ( !bOK && ppEvents != NULL && pEventCount != NULL && *ppEvents != NULL ) {
		xrtImapIdleEventsFree(*ppEvents, *pEventCount);
		*ppEvents = NULL;
		*pEventCount = 0u;
	}
	return bOK;
}

static bool ximap_idle_watch_run_common(ximapfetchtaskctx* pCtx, ximapresult* pRet, size_t* pEventCount)
{
	ximapclient* pClient = NULL;
	str sSelectRaw = NULL;
	bool bOK = FALSE;

	if ( pRet != NULL ) {
		xrtImapResultInit(pRet);
	}
	if ( pEventCount != NULL ) {
		*pEventCount = 0u;
	}
	if ( pCtx == NULL ) {
		ximap_result_error(pRet, "IMAP async IDLE watch task context missing");
		return FALSE;
	}
	if ( pEventCount == NULL || pCtx->pIdleCallback == NULL ) {
		ximap_result_error(pRet, "IMAP async IDLE watch output missing");
		return FALSE;
	}
	if ( !xrtImapOpen(&pCtx->tCfg, &pClient, pRet) ) {
		goto cleanup;
	}
	if ( pCtx->tOpts.sMailbox != NULL && pCtx->tOpts.sMailbox[0] != '\0' ) {
		if ( !xrtImapSelect(pClient, pCtx->tOpts.sMailbox, &sSelectRaw, pRet) ) {
			goto cleanup;
		}
		xrtFree(sSelectRaw);
		sSelectRaw = NULL;
	}
	if ( !xrtImapIdleWatch(pClient, pCtx->iMaxEvents, pCtx->pIdleCallback, pCtx->pIdleUser, pEventCount, pRet) ) {
		goto cleanup;
	}
	if ( !xrtImapLogout(pClient, pRet) ) {
		pClient = NULL;
		goto cleanup;
	}
	pClient = NULL;
	bOK = TRUE;
	if ( pRet != NULL ) {
		pRet->bSuccess = TRUE;
	}

cleanup:
	if ( sSelectRaw != NULL ) {
		xrtFree(sSelectRaw);
	}
	if ( pClient != NULL ) {
		xrtImapClose(pClient);
	}
	if ( !bOK && pEventCount != NULL ) {
		*pEventCount = 0u;
	}
	return bOK;
}

static int32 ximap_fetch_raw_task(ptr pArg, xfuture_result* pOut)
{
	ximapfetchtaskctx* pCtx = (ximapfetchtaskctx*)pArg;
	ximapfetchrawresult* pResult;

	if ( pOut == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return XRT_NET_ERROR;
	}
	memset(pOut, 0, sizeof(*pOut));
	pResult = (ximapfetchrawresult*)xrtCalloc(1, sizeof(*pResult));
	if ( pResult == NULL ) {
		pOut->iStatus = XRT_NET_ERROR;
		pOut->sError = (str)"IMAP async raw result alloc failed";
		ximap_fetch_task_ctx_free(pCtx);
		return pOut->iStatus;
	}
	(void)ximap_fetch_run_common(pCtx, &pResult->tResult, &pResult->sRaw, NULL);
	pOut->iStatus = XRT_NET_OK;
	pOut->pValue = pResult;
	pOut->iFlags = XFUTURE_RESULT_F_OWN_VALUE;
	ximap_fetch_task_ctx_free(pCtx);
	return pOut->iStatus;
}

static int32 ximap_fetch_mime_task(ptr pArg, xfuture_result* pOut)
{
	ximapfetchtaskctx* pCtx = (ximapfetchtaskctx*)pArg;
	ximapfetchmimeresult* pResult;

	if ( pOut == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return XRT_NET_ERROR;
	}
	memset(pOut, 0, sizeof(*pOut));
	pResult = (ximapfetchmimeresult*)xrtCalloc(1, sizeof(*pResult));
	if ( pResult == NULL ) {
		pOut->iStatus = XRT_NET_ERROR;
		pOut->sError = (str)"IMAP async MIME result alloc failed";
		ximap_fetch_task_ctx_free(pCtx);
		return pOut->iStatus;
	}
	(void)ximap_fetch_run_common(pCtx, &pResult->tResult, NULL, &pResult->tMessage);
	pOut->iStatus = XRT_NET_OK;
	pOut->pValue = pResult;
	pOut->iFlags = XFUTURE_RESULT_F_OWN_VALUE;
	ximap_fetch_task_ctx_free(pCtx);
	return pOut->iStatus;
}

static int32 ximap_flags_task(ptr pArg, xfuture_result* pOut)
{
	ximapfetchtaskctx* pCtx = (ximapfetchtaskctx*)pArg;
	ximapflagsresult* pResult;

	if ( pOut == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return XRT_NET_ERROR;
	}
	memset(pOut, 0, sizeof(*pOut));
	pResult = (ximapflagsresult*)xrtCalloc(1, sizeof(*pResult));
	if ( pResult == NULL ) {
		pOut->iStatus = XRT_NET_ERROR;
		pOut->sError = (str)"IMAP async FLAGS result alloc failed";
		ximap_fetch_task_ctx_free(pCtx);
		return pOut->iStatus;
	}
	(void)ximap_flags_run_common(pCtx, &pResult->tResult, &pResult->tFlags);
	pOut->iStatus = XRT_NET_OK;
	pOut->pValue = pResult;
	pOut->iFlags = XFUTURE_RESULT_F_OWN_VALUE;
	ximap_fetch_task_ctx_free(pCtx);
	return pOut->iStatus;
}

static int32 ximap_expunge_task(ptr pArg, xfuture_result* pOut)
{
	ximapfetchtaskctx* pCtx = (ximapfetchtaskctx*)pArg;
	ximapexpungeasyncresult* pResult;

	if ( pOut == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return XRT_NET_ERROR;
	}
	memset(pOut, 0, sizeof(*pOut));
	pResult = (ximapexpungeasyncresult*)xrtCalloc(1, sizeof(*pResult));
	if ( pResult == NULL ) {
		pOut->iStatus = XRT_NET_ERROR;
		pOut->sError = (str)"IMAP async EXPUNGE result alloc failed";
		ximap_fetch_task_ctx_free(pCtx);
		return pOut->iStatus;
	}
	(void)ximap_expunge_run_common(pCtx, &pResult->tResult, &pResult->tExpunge);
	pOut->iStatus = XRT_NET_OK;
	pOut->pValue = pResult;
	pOut->iFlags = XFUTURE_RESULT_F_OWN_VALUE;
	ximap_fetch_task_ctx_free(pCtx);
	return pOut->iStatus;
}

static int32 ximap_idle_task(ptr pArg, xfuture_result* pOut)
{
	ximapfetchtaskctx* pCtx = (ximapfetchtaskctx*)pArg;
	ximapidleresult* pResult;

	if ( pOut == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return XRT_NET_ERROR;
	}
	memset(pOut, 0, sizeof(*pOut));
	pResult = (ximapidleresult*)xrtCalloc(1, sizeof(*pResult));
	if ( pResult == NULL ) {
		pOut->iStatus = XRT_NET_ERROR;
		pOut->sError = (str)"IMAP async IDLE result alloc failed";
		ximap_fetch_task_ctx_free(pCtx);
		return pOut->iStatus;
	}
	(void)ximap_idle_run_common(pCtx, &pResult->tResult, &pResult->arrEvents, &pResult->iCount);
	pOut->iStatus = XRT_NET_OK;
	pOut->pValue = pResult;
	pOut->iFlags = XFUTURE_RESULT_F_OWN_VALUE;
	ximap_fetch_task_ctx_free(pCtx);
	return pOut->iStatus;
}

static int32 ximap_idle_watch_task(ptr pArg, xfuture_result* pOut)
{
	ximapfetchtaskctx* pCtx = (ximapfetchtaskctx*)pArg;
	ximapidlewatchresult* pResult;

	if ( pOut == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return XRT_NET_ERROR;
	}
	memset(pOut, 0, sizeof(*pOut));
	pResult = (ximapidlewatchresult*)xrtCalloc(1, sizeof(*pResult));
	if ( pResult == NULL ) {
		pOut->iStatus = XRT_NET_ERROR;
		pOut->sError = (str)"IMAP async IDLE watch result alloc failed";
		ximap_fetch_task_ctx_free(pCtx);
		return pOut->iStatus;
	}
	(void)ximap_idle_watch_run_common(pCtx, &pResult->tResult, &pResult->iCount);
	pOut->iStatus = XRT_NET_OK;
	pOut->pValue = pResult;
	pOut->iFlags = XFUTURE_RESULT_F_OWN_VALUE;
	ximap_fetch_task_ctx_free(pCtx);
	return pOut->iStatus;
}

static int32 ximap_bodystructure_task(ptr pArg, xfuture_result* pOut)
{
	ximapfetchtaskctx* pCtx = (ximapfetchtaskctx*)pArg;
	ximapbodystructureresult* pResult;

	if ( pOut == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return XRT_NET_ERROR;
	}
	memset(pOut, 0, sizeof(*pOut));
	pResult = (ximapbodystructureresult*)xrtCalloc(1, sizeof(*pResult));
	if ( pResult == NULL ) {
		pOut->iStatus = XRT_NET_ERROR;
		pOut->sError = (str)"IMAP async BODYSTRUCTURE result alloc failed";
		ximap_fetch_task_ctx_free(pCtx);
		return pOut->iStatus;
	}
	(void)ximap_bodystructure_run_common(pCtx, &pResult->tResult, &pResult->tBody);
	pOut->iStatus = XRT_NET_OK;
	pOut->pValue = pResult;
	pOut->iFlags = XFUTURE_RESULT_F_OWN_VALUE;
	ximap_fetch_task_ctx_free(pCtx);
	return pOut->iStatus;
}

static int32 ximap_search_task(ptr pArg, xfuture_result* pOut)
{
	ximapfetchtaskctx* pCtx = (ximapfetchtaskctx*)pArg;
	ximapsearchresult* pResult;

	if ( pOut == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return XRT_NET_ERROR;
	}
	memset(pOut, 0, sizeof(*pOut));
	pResult = (ximapsearchresult*)xrtCalloc(1, sizeof(*pResult));
	if ( pResult == NULL ) {
		pOut->iStatus = XRT_NET_ERROR;
		pOut->sError = (str)"IMAP async SEARCH result alloc failed";
		ximap_fetch_task_ctx_free(pCtx);
		return pOut->iStatus;
	}
	(void)ximap_search_run_common(pCtx, &pResult->tResult, &pResult->tIds);
	pOut->iStatus = XRT_NET_OK;
	pOut->pValue = pResult;
	pOut->iFlags = XFUTURE_RESULT_F_OWN_VALUE;
	ximap_fetch_task_ctx_free(pCtx);
	return pOut->iStatus;
}

static int32 ximap_list_task(ptr pArg, xfuture_result* pOut)
{
	ximapfetchtaskctx* pCtx = (ximapfetchtaskctx*)pArg;
	ximaplistresult* pResult;

	if ( pOut == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return XRT_NET_ERROR;
	}
	memset(pOut, 0, sizeof(*pOut));
	pResult = (ximaplistresult*)xrtCalloc(1, sizeof(*pResult));
	if ( pResult == NULL ) {
		pOut->iStatus = XRT_NET_ERROR;
		pOut->sError = (str)"IMAP async LIST result alloc failed";
		ximap_fetch_task_ctx_free(pCtx);
		return pOut->iStatus;
	}
	(void)ximap_list_run_common(pCtx, &pResult->tResult, &pResult->tList);
	pOut->iStatus = XRT_NET_OK;
	pOut->pValue = pResult;
	pOut->iFlags = XFUTURE_RESULT_F_OWN_VALUE;
	ximap_fetch_task_ctx_free(pCtx);
	return pOut->iStatus;
}

static int32 ximap_status_task(ptr pArg, xfuture_result* pOut)
{
	ximapfetchtaskctx* pCtx = (ximapfetchtaskctx*)pArg;
	ximapstatusresult* pResult;

	if ( pOut == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return XRT_NET_ERROR;
	}
	memset(pOut, 0, sizeof(*pOut));
	pResult = (ximapstatusresult*)xrtCalloc(1, sizeof(*pResult));
	if ( pResult == NULL ) {
		pOut->iStatus = XRT_NET_ERROR;
		pOut->sError = (str)"IMAP async STATUS result alloc failed";
		ximap_fetch_task_ctx_free(pCtx);
		return pOut->iStatus;
	}
	(void)ximap_status_run_common(pCtx, &pResult->tResult, &pResult->tStatus);
	pOut->iStatus = XRT_NET_OK;
	pOut->pValue = pResult;
	pOut->iFlags = XFUTURE_RESULT_F_OWN_VALUE;
	ximap_fetch_task_ctx_free(pCtx);
	return pOut->iStatus;
}

static xfuture* xrtImapStatusFuture(const ximapconfig* pCfg, const char* sMailbox, const ximapasyncopts* pOpts)
{
	ximapfetchtaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = ximap_mailbox_task_ctx_create(pCfg, sMailbox, pOpts);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pFuture = xTaskRunThread(ximap_status_task, pCtx, 0);
	if ( pFuture == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static xfuture* xrtImapListFuture(const ximapconfig* pCfg, const char* sReference, const char* sPattern, const ximapasyncopts* pOpts)
{
	ximapfetchtaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = ximap_list_task_ctx_create(pCfg, sReference, sPattern, pOpts);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pFuture = xTaskRunThread(ximap_list_task, pCtx, 0);
	if ( pFuture == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static xfuture* xrtImapSearchFuture(const ximapconfig* pCfg, const char* sCriteria, const ximapasyncopts* pOpts)
{
	ximapfetchtaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = ximap_search_task_ctx_create(pCfg, sCriteria, pOpts, FALSE);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pFuture = xTaskRunThread(ximap_search_task, pCtx, 0);
	if ( pFuture == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static xfuture* xrtImapUidSearchFuture(const ximapconfig* pCfg, const char* sCriteria, const ximapasyncopts* pOpts)
{
	ximapfetchtaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = ximap_search_task_ctx_create(pCfg, sCriteria, pOpts, TRUE);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pFuture = xTaskRunThread(ximap_search_task, pCtx, 0);
	if ( pFuture == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static xfuture* xrtImapFetchBodyStructureFuture(const ximapconfig* pCfg, const char* sSequence, const ximapasyncopts* pOpts)
{
	ximapfetchtaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = ximap_fetch_task_ctx_create(pCfg, sSequence, pOpts, FALSE, FALSE, FALSE);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pCtx->bBodyStructure = TRUE;
	pFuture = xTaskRunThread(ximap_bodystructure_task, pCtx, 0);
	if ( pFuture == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static xfuture* xrtImapFetchFlagsFuture(const ximapconfig* pCfg, const char* sSequence, const ximapasyncopts* pOpts)
{
	ximapfetchtaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = ximap_fetch_task_ctx_create(pCfg, sSequence, pOpts, FALSE, FALSE, FALSE);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pCtx->bFetchFlags = TRUE;
	pFuture = xTaskRunThread(ximap_flags_task, pCtx, 0);
	if ( pFuture == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static xfuture* xrtImapStoreFlagsFuture(const ximapconfig* pCfg, const char* sSequence, const char* sOperation, const char* sFlags, const ximapasyncopts* pOpts)
{
	ximapfetchtaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = ximap_store_flags_task_ctx_create(pCfg, sSequence, sOperation, sFlags, pOpts, FALSE);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pFuture = xTaskRunThread(ximap_flags_task, pCtx, 0);
	if ( pFuture == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static xfuture* xrtImapUidStoreFlagsFuture(const ximapconfig* pCfg, const char* sUid, const char* sOperation, const char* sFlags, const ximapasyncopts* pOpts)
{
	ximapfetchtaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = ximap_store_flags_task_ctx_create(pCfg, sUid, sOperation, sFlags, pOpts, TRUE);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pFuture = xTaskRunThread(ximap_flags_task, pCtx, 0);
	if ( pFuture == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static xfuture* xrtImapExpungeFuture(const ximapconfig* pCfg, const ximapasyncopts* pOpts)
{
	ximapfetchtaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = ximap_expunge_task_ctx_create(pCfg, NULL, pOpts, FALSE);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pFuture = xTaskRunThread(ximap_expunge_task, pCtx, 0);
	if ( pFuture == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static xfuture* xrtImapUidExpungeFuture(const ximapconfig* pCfg, const char* sUidSet, const ximapasyncopts* pOpts)
{
	ximapfetchtaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = ximap_expunge_task_ctx_create(pCfg, sUidSet, pOpts, TRUE);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pFuture = xTaskRunThread(ximap_expunge_task, pCtx, 0);
	if ( pFuture == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static xfuture* xrtImapIdleCollectFuture(const ximapconfig* pCfg, size_t iMaxEvents, const ximapasyncopts* pOpts)
{
	ximapfetchtaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = ximap_idle_task_ctx_create(pCfg, iMaxEvents, pOpts);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pFuture = xTaskRunThread(ximap_idle_task, pCtx, 0);
	if ( pFuture == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static xfuture* xrtImapIdleWatchFuture(const ximapconfig* pCfg, size_t iMaxEvents, ximapidlecallback pCallback, void* pUser, const ximapasyncopts* pOpts)
{
	ximapfetchtaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = ximap_idle_watch_task_ctx_create(pCfg, iMaxEvents, pCallback, pUser, pOpts);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pFuture = xTaskRunThread(ximap_idle_watch_task, pCtx, 0);
	if ( pFuture == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static xfuture* xrtImapUidFetchBodyRawFuture(const ximapconfig* pCfg, const char* sUid, const ximapasyncopts* pOpts)
{
	ximapfetchtaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = ximap_fetch_task_ctx_create(pCfg, sUid, pOpts, FALSE, FALSE, FALSE);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pFuture = xTaskRunThread(ximap_fetch_raw_task, pCtx, 0);
	if ( pFuture == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static xfuture* xrtImapUidFetchBodyLiteralFuture(const ximapconfig* pCfg, const char* sUid, const ximapasyncopts* pOpts)
{
	ximapfetchtaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = ximap_fetch_task_ctx_create(pCfg, sUid, pOpts, FALSE, TRUE, FALSE);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pFuture = xTaskRunThread(ximap_fetch_raw_task, pCtx, 0);
	if ( pFuture == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static xfuture* xrtImapUidFetchMimeFuture(const ximapconfig* pCfg, const char* sUid, const ximapasyncopts* pOpts)
{
	ximapfetchtaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = ximap_fetch_task_ctx_create(pCfg, sUid, pOpts, TRUE, FALSE, FALSE);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pFuture = xTaskRunThread(ximap_fetch_mime_task, pCtx, 0);
	if ( pFuture == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static xfuture* xrtImapUidPeekBodyRawFuture(const ximapconfig* pCfg, const char* sUid, const ximapasyncopts* pOpts)
{
	ximapfetchtaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = ximap_fetch_task_ctx_create(pCfg, sUid, pOpts, FALSE, FALSE, TRUE);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pFuture = xTaskRunThread(ximap_fetch_raw_task, pCtx, 0);
	if ( pFuture == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static xfuture* xrtImapUidPeekBodyLiteralFuture(const ximapconfig* pCfg, const char* sUid, const ximapasyncopts* pOpts)
{
	ximapfetchtaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = ximap_fetch_task_ctx_create(pCfg, sUid, pOpts, FALSE, TRUE, TRUE);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pFuture = xTaskRunThread(ximap_fetch_raw_task, pCtx, 0);
	if ( pFuture == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static xfuture* xrtImapUidPeekMimeFuture(const ximapconfig* pCfg, const char* sUid, const ximapasyncopts* pOpts)
{
	ximapfetchtaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = ximap_fetch_task_ctx_create(pCfg, sUid, pOpts, TRUE, FALSE, TRUE);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pFuture = xTaskRunThread(ximap_fetch_mime_task, pCtx, 0);
	if ( pFuture == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static xfuture* xrtImapUidFetchBodySectionRawFuture(const ximapconfig* pCfg, const char* sUid, const char* sSection, const ximapasyncopts* pOpts)
{
	ximapfetchtaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = ximap_body_section_task_ctx_create(pCfg, sUid, sSection, pOpts, FALSE);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pFuture = xTaskRunThread(ximap_fetch_raw_task, pCtx, 0);
	if ( pFuture == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static xfuture* xrtImapUidPeekBodySectionRawFuture(const ximapconfig* pCfg, const char* sUid, const char* sSection, const ximapasyncopts* pOpts)
{
	ximapfetchtaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = ximap_body_section_task_ctx_create(pCfg, sUid, sSection, pOpts, TRUE);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pFuture = xTaskRunThread(ximap_fetch_raw_task, pCtx, 0);
	if ( pFuture == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static xfuture* xrtImapUidFetchHeaderFieldsRawFuture(const ximapconfig* pCfg, const char* sUid, const char* const* arrFields, size_t iFieldCount, const ximapasyncopts* pOpts)
{
	ximapfetchtaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = ximap_header_fields_task_ctx_create(pCfg, sUid, arrFields, iFieldCount, pOpts, FALSE, FALSE);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pFuture = xTaskRunThread(ximap_fetch_raw_task, pCtx, 0);
	if ( pFuture == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static xfuture* xrtImapUidFetchHeaderFieldsFuture(const ximapconfig* pCfg, const char* sUid, const char* const* arrFields, size_t iFieldCount, const ximapasyncopts* pOpts)
{
	ximapfetchtaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = ximap_header_fields_task_ctx_create(pCfg, sUid, arrFields, iFieldCount, pOpts, TRUE, FALSE);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pFuture = xTaskRunThread(ximap_fetch_raw_task, pCtx, 0);
	if ( pFuture == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static xfuture* xrtImapUidPeekHeaderFieldsRawFuture(const ximapconfig* pCfg, const char* sUid, const char* const* arrFields, size_t iFieldCount, const ximapasyncopts* pOpts)
{
	ximapfetchtaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = ximap_header_fields_task_ctx_create(pCfg, sUid, arrFields, iFieldCount, pOpts, FALSE, TRUE);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pFuture = xTaskRunThread(ximap_fetch_raw_task, pCtx, 0);
	if ( pFuture == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static xfuture* xrtImapUidPeekHeaderFieldsFuture(const ximapconfig* pCfg, const char* sUid, const char* const* arrFields, size_t iFieldCount, const ximapasyncopts* pOpts)
{
	ximapfetchtaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = ximap_header_fields_task_ctx_create(pCfg, sUid, arrFields, iFieldCount, pOpts, TRUE, TRUE);
	if ( pCtx == NULL ) {
		return NULL;
	}
	pFuture = xTaskRunThread(ximap_fetch_raw_task, pCtx, 0);
	if ( pFuture == NULL ) {
		ximap_fetch_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static bool xrtImapStatusAsyncWait(const ximapconfig* pCfg, const char* sMailbox, const ximapasyncopts* pOpts, ximapmailboxstatus* pStatus, ximapresult* pOut)
{
	xfuture* pFuture;
	ximapstatusresult* pResult;

	if ( pStatus != NULL ) {
		memset(pStatus, 0, sizeof(*pStatus));
	}
	if ( pOut != NULL ) {
		xrtImapResultInit(pOut);
	}
	if ( pStatus == NULL ) {
		ximap_result_error(pOut, "IMAP async STATUS output missing");
		return FALSE;
	}
	pFuture = xrtImapStatusFuture(pCfg, sMailbox, pOpts);
	if ( pFuture == NULL ) {
		ximap_result_error(pOut, "IMAP async STATUS future create failed");
		return FALSE;
	}
	pResult = (ximapstatusresult*)xFutureWaitValue(pFuture);
	if ( pResult == NULL ) {
		xfuture_result tResult;
		memset(&tResult, 0, sizeof(tResult));
		if ( xFutureGetResult(pFuture, &tResult) && tResult.pValue != NULL ) {
			pResult = (ximapstatusresult*)tResult.pValue;
		}
	}
	if ( pResult == NULL ) {
		str sError = xFutureError(pFuture);
		ximap_result_error(pOut, sError ? (const char*)sError : "IMAP async STATUS wait failed");
		xFutureRelease(pFuture);
		return FALSE;
	}
	if ( pOut != NULL ) {
		*pOut = pResult->tResult;
	}
	*pStatus = pResult->tStatus;
	xrtImapStatusResultFree(pResult);
	xFutureRelease(pFuture);
	return pOut ? pOut->bSuccess : TRUE;
}

static bool xrtImapListAsyncWait(const ximapconfig* pCfg, const char* sReference, const char* sPattern, const ximapasyncopts* pOpts, ximaplist* pList, ximapresult* pOut)
{
	xfuture* pFuture;
	ximaplistresult* pResult;

	if ( pList != NULL ) {
		memset(pList, 0, sizeof(*pList));
	}
	if ( pOut != NULL ) {
		xrtImapResultInit(pOut);
	}
	if ( pList == NULL ) {
		ximap_result_error(pOut, "IMAP async LIST output missing");
		return FALSE;
	}
	pFuture = xrtImapListFuture(pCfg, sReference, sPattern, pOpts);
	if ( pFuture == NULL ) {
		ximap_result_error(pOut, "IMAP async LIST future create failed");
		return FALSE;
	}
	pResult = (ximaplistresult*)xFutureWaitValue(pFuture);
	if ( pResult == NULL ) {
		xfuture_result tResult;
		memset(&tResult, 0, sizeof(tResult));
		if ( xFutureGetResult(pFuture, &tResult) && tResult.pValue != NULL ) {
			pResult = (ximaplistresult*)tResult.pValue;
		}
	}
	if ( pResult == NULL ) {
		str sError = xFutureError(pFuture);
		ximap_result_error(pOut, sError ? (const char*)sError : "IMAP async LIST wait failed");
		xFutureRelease(pFuture);
		return FALSE;
	}
	if ( pOut != NULL ) {
		*pOut = pResult->tResult;
	}
	*pList = pResult->tList;
	memset(&pResult->tList, 0, sizeof(pResult->tList));
	xrtImapListResultFree(pResult);
	xFutureRelease(pFuture);
	return pOut ? pOut->bSuccess : TRUE;
}

static bool ximap_search_async_wait_ex(const ximapconfig* pCfg, const char* sCriteria, const ximapasyncopts* pOpts, bool bUidSearch, ximapsearchids* pIds, ximapresult* pOut)
{
	xfuture* pFuture;
	ximapsearchresult* pResult;

	if ( pIds != NULL ) {
		memset(pIds, 0, sizeof(*pIds));
	}
	if ( pOut != NULL ) {
		xrtImapResultInit(pOut);
	}
	if ( pIds == NULL ) {
		ximap_result_error(pOut, "IMAP async SEARCH output missing");
		return FALSE;
	}
	pFuture = bUidSearch ? xrtImapUidSearchFuture(pCfg, sCriteria, pOpts) : xrtImapSearchFuture(pCfg, sCriteria, pOpts);
	if ( pFuture == NULL ) {
		ximap_result_error(pOut, "IMAP async SEARCH future create failed");
		return FALSE;
	}
	pResult = (ximapsearchresult*)xFutureWaitValue(pFuture);
	if ( pResult == NULL ) {
		xfuture_result tResult;
		memset(&tResult, 0, sizeof(tResult));
		if ( xFutureGetResult(pFuture, &tResult) && tResult.pValue != NULL ) {
			pResult = (ximapsearchresult*)tResult.pValue;
		}
	}
	if ( pResult == NULL ) {
		str sError = xFutureError(pFuture);
		ximap_result_error(pOut, sError ? (const char*)sError : "IMAP async SEARCH wait failed");
		xFutureRelease(pFuture);
		return FALSE;
	}
	if ( pOut != NULL ) {
		*pOut = pResult->tResult;
	}
	*pIds = pResult->tIds;
	memset(&pResult->tIds, 0, sizeof(pResult->tIds));
	xrtImapSearchResultFree(pResult);
	xFutureRelease(pFuture);
	return pOut ? pOut->bSuccess : TRUE;
}

static bool xrtImapSearchAsyncWait(const ximapconfig* pCfg, const char* sCriteria, const ximapasyncopts* pOpts, ximapsearchids* pIds, ximapresult* pOut)
{
	return ximap_search_async_wait_ex(pCfg, sCriteria, pOpts, FALSE, pIds, pOut);
}

static bool xrtImapUidSearchAsyncWait(const ximapconfig* pCfg, const char* sCriteria, const ximapasyncopts* pOpts, ximapsearchids* pIds, ximapresult* pOut)
{
	return ximap_search_async_wait_ex(pCfg, sCriteria, pOpts, TRUE, pIds, pOut);
}

static bool xrtImapFetchBodyStructureAsyncWait(const ximapconfig* pCfg, const char* sSequence, const ximapasyncopts* pOpts, ximapbodystructure* pBody, ximapresult* pOut)
{
	xfuture* pFuture;
	ximapbodystructureresult* pResult;

	if ( pBody != NULL ) {
		memset(pBody, 0, sizeof(*pBody));
	}
	if ( pOut != NULL ) {
		xrtImapResultInit(pOut);
	}
	if ( pBody == NULL ) {
		ximap_result_error(pOut, "IMAP async BODYSTRUCTURE output missing");
		return FALSE;
	}
	pFuture = xrtImapFetchBodyStructureFuture(pCfg, sSequence, pOpts);
	if ( pFuture == NULL ) {
		ximap_result_error(pOut, "IMAP async BODYSTRUCTURE future create failed");
		return FALSE;
	}
	pResult = (ximapbodystructureresult*)xFutureWaitValue(pFuture);
	if ( pResult == NULL ) {
		xfuture_result tResult;
		memset(&tResult, 0, sizeof(tResult));
		if ( xFutureGetResult(pFuture, &tResult) && tResult.pValue != NULL ) {
			pResult = (ximapbodystructureresult*)tResult.pValue;
		}
	}
	if ( pResult == NULL ) {
		str sError = xFutureError(pFuture);
		ximap_result_error(pOut, sError ? (const char*)sError : "IMAP async BODYSTRUCTURE wait failed");
		xFutureRelease(pFuture);
		return FALSE;
	}
	if ( pOut != NULL ) {
		*pOut = pResult->tResult;
	}
	*pBody = pResult->tBody;
	memset(&pResult->tBody, 0, sizeof(pResult->tBody));
	xrtImapBodyStructureResultFree(pResult);
	xFutureRelease(pFuture);
	return pOut ? pOut->bSuccess : TRUE;
}

static bool xrtImapFetchFlagsAsyncWait(const ximapconfig* pCfg, const char* sSequence, const ximapasyncopts* pOpts, ximapflags* pFlags, ximapresult* pOut)
{
	xfuture* pFuture;
	ximapflagsresult* pResult;

	if ( pFlags != NULL ) {
		memset(pFlags, 0, sizeof(*pFlags));
	}
	if ( pOut != NULL ) {
		xrtImapResultInit(pOut);
	}
	if ( pFlags == NULL ) {
		ximap_result_error(pOut, "IMAP async FLAGS output missing");
		return FALSE;
	}
	pFuture = xrtImapFetchFlagsFuture(pCfg, sSequence, pOpts);
	if ( pFuture == NULL ) {
		ximap_result_error(pOut, "IMAP async FLAGS future create failed");
		return FALSE;
	}
	pResult = (ximapflagsresult*)xFutureWaitValue(pFuture);
	if ( pResult == NULL ) {
		xfuture_result tResult;
		memset(&tResult, 0, sizeof(tResult));
		if ( xFutureGetResult(pFuture, &tResult) && tResult.pValue != NULL ) {
			pResult = (ximapflagsresult*)tResult.pValue;
		}
	}
	if ( pResult == NULL ) {
		str sError = xFutureError(pFuture);
		ximap_result_error(pOut, sError ? (const char*)sError : "IMAP async FLAGS wait failed");
		xFutureRelease(pFuture);
		return FALSE;
	}
	if ( pOut != NULL ) {
		*pOut = pResult->tResult;
	}
	*pFlags = pResult->tFlags;
	xrtImapFlagsResultFree(pResult);
	xFutureRelease(pFuture);
	return pOut ? pOut->bSuccess : TRUE;
}

static bool ximap_store_flags_async_wait_ex(const ximapconfig* pCfg, const char* sSequenceOrUid, const char* sOperation, const char* sFlags, const ximapasyncopts* pOpts, bool bUidStore, ximapflags* pFlags, ximapresult* pOut)
{
	xfuture* pFuture;
	ximapflagsresult* pResult;

	if ( pFlags != NULL ) {
		memset(pFlags, 0, sizeof(*pFlags));
	}
	if ( pOut != NULL ) {
		xrtImapResultInit(pOut);
	}
	if ( pFlags == NULL ) {
		ximap_result_error(pOut, "IMAP async STORE FLAGS output missing");
		return FALSE;
	}
	pFuture = bUidStore
		? xrtImapUidStoreFlagsFuture(pCfg, sSequenceOrUid, sOperation, sFlags, pOpts)
		: xrtImapStoreFlagsFuture(pCfg, sSequenceOrUid, sOperation, sFlags, pOpts);
	if ( pFuture == NULL ) {
		ximap_result_error(pOut, "IMAP async STORE FLAGS future create failed");
		return FALSE;
	}
	pResult = (ximapflagsresult*)xFutureWaitValue(pFuture);
	if ( pResult == NULL ) {
		xfuture_result tResult;
		memset(&tResult, 0, sizeof(tResult));
		if ( xFutureGetResult(pFuture, &tResult) && tResult.pValue != NULL ) {
			pResult = (ximapflagsresult*)tResult.pValue;
		}
	}
	if ( pResult == NULL ) {
		str sError = xFutureError(pFuture);
		ximap_result_error(pOut, sError ? (const char*)sError : "IMAP async STORE FLAGS wait failed");
		xFutureRelease(pFuture);
		return FALSE;
	}
	if ( pOut != NULL ) {
		*pOut = pResult->tResult;
	}
	*pFlags = pResult->tFlags;
	xrtImapFlagsResultFree(pResult);
	xFutureRelease(pFuture);
	return pOut ? pOut->bSuccess : TRUE;
}

static bool xrtImapStoreFlagsAsyncWait(const ximapconfig* pCfg, const char* sSequence, const char* sOperation, const char* sFlags, const ximapasyncopts* pOpts, ximapflags* pFlags, ximapresult* pOut)
{
	return ximap_store_flags_async_wait_ex(pCfg, sSequence, sOperation, sFlags, pOpts, FALSE, pFlags, pOut);
}

static bool xrtImapUidStoreFlagsAsyncWait(const ximapconfig* pCfg, const char* sUid, const char* sOperation, const char* sFlags, const ximapasyncopts* pOpts, ximapflags* pFlags, ximapresult* pOut)
{
	return ximap_store_flags_async_wait_ex(pCfg, sUid, sOperation, sFlags, pOpts, TRUE, pFlags, pOut);
}

static bool ximap_expunge_async_wait_ex(const ximapconfig* pCfg, const char* sUidSet, const ximapasyncopts* pOpts, bool bUidExpunge, ximapexpungeresult* pExpunge, ximapresult* pOut)
{
	xfuture* pFuture;
	ximapexpungeasyncresult* pResult;

	if ( pExpunge != NULL ) {
		memset(pExpunge, 0, sizeof(*pExpunge));
	}
	if ( pOut != NULL ) {
		xrtImapResultInit(pOut);
	}
	if ( pExpunge == NULL ) {
		ximap_result_error(pOut, "IMAP async EXPUNGE output missing");
		return FALSE;
	}
	pFuture = bUidExpunge ? xrtImapUidExpungeFuture(pCfg, sUidSet, pOpts) : xrtImapExpungeFuture(pCfg, pOpts);
	if ( pFuture == NULL ) {
		ximap_result_error(pOut, "IMAP async EXPUNGE future create failed");
		return FALSE;
	}
	pResult = (ximapexpungeasyncresult*)xFutureWaitValue(pFuture);
	if ( pResult == NULL ) {
		xfuture_result tResult;
		memset(&tResult, 0, sizeof(tResult));
		if ( xFutureGetResult(pFuture, &tResult) && tResult.pValue != NULL ) {
			pResult = (ximapexpungeasyncresult*)tResult.pValue;
		}
	}
	if ( pResult == NULL ) {
		str sError = xFutureError(pFuture);
		ximap_result_error(pOut, sError ? (const char*)sError : "IMAP async EXPUNGE wait failed");
		xFutureRelease(pFuture);
		return FALSE;
	}
	if ( pOut != NULL ) {
		*pOut = pResult->tResult;
	}
	*pExpunge = pResult->tExpunge;
	memset(&pResult->tExpunge, 0, sizeof(pResult->tExpunge));
	xrtImapExpungeAsyncResultFree(pResult);
	xFutureRelease(pFuture);
	return pOut ? pOut->bSuccess : TRUE;
}

static bool xrtImapExpungeAsyncWait(const ximapconfig* pCfg, const ximapasyncopts* pOpts, ximapexpungeresult* pExpunge, ximapresult* pOut)
{
	return ximap_expunge_async_wait_ex(pCfg, NULL, pOpts, FALSE, pExpunge, pOut);
}

static bool xrtImapUidExpungeAsyncWait(const ximapconfig* pCfg, const char* sUidSet, const ximapasyncopts* pOpts, ximapexpungeresult* pExpunge, ximapresult* pOut)
{
	return ximap_expunge_async_wait_ex(pCfg, sUidSet, pOpts, TRUE, pExpunge, pOut);
}

static bool xrtImapIdleCollectAsyncWait(const ximapconfig* pCfg, size_t iMaxEvents, const ximapasyncopts* pOpts, str** ppEvents, size_t* pEventCount, ximapresult* pOut)
{
	xfuture* pFuture;
	ximapidleresult* pResult;

	if ( ppEvents != NULL ) {
		*ppEvents = NULL;
	}
	if ( pEventCount != NULL ) {
		*pEventCount = 0u;
	}
	if ( pOut != NULL ) {
		xrtImapResultInit(pOut);
	}
	if ( ppEvents == NULL || pEventCount == NULL ) {
		ximap_result_error(pOut, "IMAP async IDLE output missing");
		return FALSE;
	}
	pFuture = xrtImapIdleCollectFuture(pCfg, iMaxEvents, pOpts);
	if ( pFuture == NULL ) {
		ximap_result_error(pOut, "IMAP async IDLE future create failed");
		return FALSE;
	}
	pResult = (ximapidleresult*)xFutureWaitValue(pFuture);
	if ( pResult == NULL ) {
		xfuture_result tResult;
		memset(&tResult, 0, sizeof(tResult));
		if ( xFutureGetResult(pFuture, &tResult) && tResult.pValue != NULL ) {
			pResult = (ximapidleresult*)tResult.pValue;
		}
	}
	if ( pResult == NULL ) {
		str sError = xFutureError(pFuture);
		ximap_result_error(pOut, sError ? (const char*)sError : "IMAP async IDLE wait failed");
		xFutureRelease(pFuture);
		return FALSE;
	}
	if ( pOut != NULL ) {
		*pOut = pResult->tResult;
	}
	*ppEvents = pResult->arrEvents;
	*pEventCount = pResult->iCount;
	pResult->arrEvents = NULL;
	pResult->iCount = 0u;
	xrtImapIdleResultFree(pResult);
	xFutureRelease(pFuture);
	return pOut ? pOut->bSuccess : TRUE;
}

static bool xrtImapIdleWatchAsyncWait(const ximapconfig* pCfg, size_t iMaxEvents, ximapidlecallback pCallback, void* pUser, const ximapasyncopts* pOpts, size_t* pEventCount, ximapresult* pOut)
{
	xfuture* pFuture;
	ximapidlewatchresult* pResult;

	if ( pEventCount != NULL ) {
		*pEventCount = 0u;
	}
	if ( pOut != NULL ) {
		xrtImapResultInit(pOut);
	}
	if ( pEventCount == NULL || pCallback == NULL ) {
		ximap_result_error(pOut, "IMAP async IDLE watch output missing");
		return FALSE;
	}
	pFuture = xrtImapIdleWatchFuture(pCfg, iMaxEvents, pCallback, pUser, pOpts);
	if ( pFuture == NULL ) {
		ximap_result_error(pOut, "IMAP async IDLE watch future create failed");
		return FALSE;
	}
	pResult = (ximapidlewatchresult*)xFutureWaitValue(pFuture);
	if ( pResult == NULL ) {
		xfuture_result tResult;
		memset(&tResult, 0, sizeof(tResult));
		if ( xFutureGetResult(pFuture, &tResult) && tResult.pValue != NULL ) {
			pResult = (ximapidlewatchresult*)tResult.pValue;
		}
	}
	if ( pResult == NULL ) {
		str sError = xFutureError(pFuture);
		ximap_result_error(pOut, sError ? (const char*)sError : "IMAP async IDLE watch wait failed");
		xFutureRelease(pFuture);
		return FALSE;
	}
	if ( pOut != NULL ) {
		*pOut = pResult->tResult;
	}
	*pEventCount = pResult->iCount;
	xrtImapIdleWatchResultFree(pResult);
	xFutureRelease(pFuture);
	return pOut ? pOut->bSuccess : TRUE;
}

static bool xrtImapUidFetchBodyRawAsyncWait(const ximapconfig* pCfg, const char* sUid, const ximapasyncopts* pOpts, str* psRaw, ximapresult* pOut)
{
	xfuture* pFuture;
	ximapfetchrawresult* pResult;

	if ( psRaw != NULL ) {
		*psRaw = NULL;
	}
	if ( pOut != NULL ) {
		xrtImapResultInit(pOut);
	}
	if ( psRaw == NULL ) {
		ximap_result_error(pOut, "IMAP async raw output missing");
		return FALSE;
	}
	pFuture = xrtImapUidFetchBodyRawFuture(pCfg, sUid, pOpts);
	if ( pFuture == NULL ) {
		ximap_result_error(pOut, "IMAP async raw future create failed");
		return FALSE;
	}
	pResult = (ximapfetchrawresult*)xFutureWaitValue(pFuture);
	if ( pResult == NULL ) {
		xfuture_result tResult;
		memset(&tResult, 0, sizeof(tResult));
		if ( xFutureGetResult(pFuture, &tResult) && tResult.pValue != NULL ) {
			pResult = (ximapfetchrawresult*)tResult.pValue;
		}
	}
	if ( pResult == NULL ) {
		str sError = xFutureError(pFuture);
		ximap_result_error(pOut, sError ? (const char*)sError : "IMAP async raw wait failed");
		xFutureRelease(pFuture);
		return FALSE;
	}
	if ( pOut != NULL ) {
		*pOut = pResult->tResult;
	}
	*psRaw = pResult->sRaw;
	pResult->sRaw = NULL;
	xrtImapFetchRawResultFree(pResult);
	xFutureRelease(pFuture);
	return pOut ? pOut->bSuccess : TRUE;
}

static bool xrtImapUidPeekBodyRawAsyncWait(const ximapconfig* pCfg, const char* sUid, const ximapasyncopts* pOpts, str* psRaw, ximapresult* pOut)
{
	xfuture* pFuture;
	ximapfetchrawresult* pResult;

	if ( psRaw != NULL ) {
		*psRaw = NULL;
	}
	if ( pOut != NULL ) {
		xrtImapResultInit(pOut);
	}
	if ( psRaw == NULL ) {
		ximap_result_error(pOut, "IMAP async PEEK raw output missing");
		return FALSE;
	}
	pFuture = xrtImapUidPeekBodyRawFuture(pCfg, sUid, pOpts);
	if ( pFuture == NULL ) {
		ximap_result_error(pOut, "IMAP async PEEK raw future create failed");
		return FALSE;
	}
	pResult = (ximapfetchrawresult*)xFutureWaitValue(pFuture);
	if ( pResult == NULL ) {
		xfuture_result tResult;
		memset(&tResult, 0, sizeof(tResult));
		if ( xFutureGetResult(pFuture, &tResult) && tResult.pValue != NULL ) {
			pResult = (ximapfetchrawresult*)tResult.pValue;
		}
	}
	if ( pResult == NULL ) {
		str sError = xFutureError(pFuture);
		ximap_result_error(pOut, sError ? (const char*)sError : "IMAP async PEEK raw wait failed");
		xFutureRelease(pFuture);
		return FALSE;
	}
	if ( pOut != NULL ) {
		*pOut = pResult->tResult;
	}
	*psRaw = pResult->sRaw;
	pResult->sRaw = NULL;
	xrtImapFetchRawResultFree(pResult);
	xFutureRelease(pFuture);
	return pOut ? pOut->bSuccess : TRUE;
}

static bool xrtImapUidFetchBodyLiteralAsyncWait(const ximapconfig* pCfg, const char* sUid, const ximapasyncopts* pOpts, str* psLiteral, ximapresult* pOut)
{
	xfuture* pFuture;
	ximapfetchrawresult* pResult;

	if ( psLiteral != NULL ) {
		*psLiteral = NULL;
	}
	if ( pOut != NULL ) {
		xrtImapResultInit(pOut);
	}
	if ( psLiteral == NULL ) {
		ximap_result_error(pOut, "IMAP async literal output missing");
		return FALSE;
	}
	pFuture = xrtImapUidFetchBodyLiteralFuture(pCfg, sUid, pOpts);
	if ( pFuture == NULL ) {
		ximap_result_error(pOut, "IMAP async literal future create failed");
		return FALSE;
	}
	pResult = (ximapfetchrawresult*)xFutureWaitValue(pFuture);
	if ( pResult == NULL ) {
		xfuture_result tResult;
		memset(&tResult, 0, sizeof(tResult));
		if ( xFutureGetResult(pFuture, &tResult) && tResult.pValue != NULL ) {
			pResult = (ximapfetchrawresult*)tResult.pValue;
		}
	}
	if ( pResult == NULL ) {
		str sError = xFutureError(pFuture);
		ximap_result_error(pOut, sError ? (const char*)sError : "IMAP async literal wait failed");
		xFutureRelease(pFuture);
		return FALSE;
	}
	if ( pOut != NULL ) {
		*pOut = pResult->tResult;
	}
	*psLiteral = pResult->sRaw;
	pResult->sRaw = NULL;
	xrtImapFetchRawResultFree(pResult);
	xFutureRelease(pFuture);
	return pOut ? pOut->bSuccess : TRUE;
}

static bool xrtImapUidPeekBodyLiteralAsyncWait(const ximapconfig* pCfg, const char* sUid, const ximapasyncopts* pOpts, str* psLiteral, ximapresult* pOut)
{
	xfuture* pFuture;
	ximapfetchrawresult* pResult;

	if ( psLiteral != NULL ) {
		*psLiteral = NULL;
	}
	if ( pOut != NULL ) {
		xrtImapResultInit(pOut);
	}
	if ( psLiteral == NULL ) {
		ximap_result_error(pOut, "IMAP async PEEK literal output missing");
		return FALSE;
	}
	pFuture = xrtImapUidPeekBodyLiteralFuture(pCfg, sUid, pOpts);
	if ( pFuture == NULL ) {
		ximap_result_error(pOut, "IMAP async PEEK literal future create failed");
		return FALSE;
	}
	pResult = (ximapfetchrawresult*)xFutureWaitValue(pFuture);
	if ( pResult == NULL ) {
		xfuture_result tResult;
		memset(&tResult, 0, sizeof(tResult));
		if ( xFutureGetResult(pFuture, &tResult) && tResult.pValue != NULL ) {
			pResult = (ximapfetchrawresult*)tResult.pValue;
		}
	}
	if ( pResult == NULL ) {
		str sError = xFutureError(pFuture);
		ximap_result_error(pOut, sError ? (const char*)sError : "IMAP async PEEK literal wait failed");
		xFutureRelease(pFuture);
		return FALSE;
	}
	if ( pOut != NULL ) {
		*pOut = pResult->tResult;
	}
	*psLiteral = pResult->sRaw;
	pResult->sRaw = NULL;
	xrtImapFetchRawResultFree(pResult);
	xFutureRelease(pFuture);
	return pOut ? pOut->bSuccess : TRUE;
}

static bool ximap_uid_body_section_raw_async_wait_ex(const ximapconfig* pCfg, const char* sUid, const char* sSection, const ximapasyncopts* pOpts, bool bPeek, str* psRaw, ximapresult* pOut)
{
	xfuture* pFuture;
	ximapfetchrawresult* pResult;

	if ( psRaw != NULL ) {
		*psRaw = NULL;
	}
	if ( pOut != NULL ) {
		xrtImapResultInit(pOut);
	}
	if ( psRaw == NULL ) {
		ximap_result_error(pOut, "IMAP async BODY section output missing");
		return FALSE;
	}
	pFuture = bPeek
		? xrtImapUidPeekBodySectionRawFuture(pCfg, sUid, sSection, pOpts)
		: xrtImapUidFetchBodySectionRawFuture(pCfg, sUid, sSection, pOpts);
	if ( pFuture == NULL ) {
		ximap_result_error(pOut, "IMAP async BODY section future create failed");
		return FALSE;
	}
	pResult = (ximapfetchrawresult*)xFutureWaitValue(pFuture);
	if ( pResult == NULL ) {
		xfuture_result tResult;
		memset(&tResult, 0, sizeof(tResult));
		if ( xFutureGetResult(pFuture, &tResult) && tResult.pValue != NULL ) {
			pResult = (ximapfetchrawresult*)tResult.pValue;
		}
	}
	if ( pResult == NULL ) {
		str sError = xFutureError(pFuture);
		ximap_result_error(pOut, sError ? (const char*)sError : "IMAP async BODY section wait failed");
		xFutureRelease(pFuture);
		return FALSE;
	}
	if ( pOut != NULL ) {
		*pOut = pResult->tResult;
	}
	*psRaw = pResult->sRaw;
	pResult->sRaw = NULL;
	xrtImapFetchRawResultFree(pResult);
	xFutureRelease(pFuture);
	return pOut ? pOut->bSuccess : TRUE;
}

static bool xrtImapUidFetchBodySectionRawAsyncWait(const ximapconfig* pCfg, const char* sUid, const char* sSection, const ximapasyncopts* pOpts, str* psRaw, ximapresult* pOut)
{
	return ximap_uid_body_section_raw_async_wait_ex(pCfg, sUid, sSection, pOpts, FALSE, psRaw, pOut);
}

static bool xrtImapUidPeekBodySectionRawAsyncWait(const ximapconfig* pCfg, const char* sUid, const char* sSection, const ximapasyncopts* pOpts, str* psRaw, ximapresult* pOut)
{
	return ximap_uid_body_section_raw_async_wait_ex(pCfg, sUid, sSection, pOpts, TRUE, psRaw, pOut);
}

static bool ximap_uid_header_fields_async_wait_ex(const ximapconfig* pCfg, const char* sUid, const char* const* arrFields, size_t iFieldCount, const ximapasyncopts* pOpts, bool bExtractLiteral, bool bPeek, str* psOut, ximapresult* pOut)
{
	xfuture* pFuture;
	ximapfetchrawresult* pResult;

	if ( psOut != NULL ) {
		*psOut = NULL;
	}
	if ( pOut != NULL ) {
		xrtImapResultInit(pOut);
	}
	if ( psOut == NULL ) {
		ximap_result_error(pOut, "IMAP async HEADER.FIELDS output missing");
		return FALSE;
	}
	if ( bExtractLiteral ) {
		pFuture = bPeek
			? xrtImapUidPeekHeaderFieldsFuture(pCfg, sUid, arrFields, iFieldCount, pOpts)
			: xrtImapUidFetchHeaderFieldsFuture(pCfg, sUid, arrFields, iFieldCount, pOpts);
	} else {
		pFuture = bPeek
			? xrtImapUidPeekHeaderFieldsRawFuture(pCfg, sUid, arrFields, iFieldCount, pOpts)
			: xrtImapUidFetchHeaderFieldsRawFuture(pCfg, sUid, arrFields, iFieldCount, pOpts);
	}
	if ( pFuture == NULL ) {
		ximap_result_error(pOut, "IMAP async HEADER.FIELDS future create failed");
		return FALSE;
	}
	pResult = (ximapfetchrawresult*)xFutureWaitValue(pFuture);
	if ( pResult == NULL ) {
		xfuture_result tResult;
		memset(&tResult, 0, sizeof(tResult));
		if ( xFutureGetResult(pFuture, &tResult) && tResult.pValue != NULL ) {
			pResult = (ximapfetchrawresult*)tResult.pValue;
		}
	}
	if ( pResult == NULL ) {
		str sError = xFutureError(pFuture);
		ximap_result_error(pOut, sError ? (const char*)sError : "IMAP async HEADER.FIELDS wait failed");
		xFutureRelease(pFuture);
		return FALSE;
	}
	if ( pOut != NULL ) {
		*pOut = pResult->tResult;
	}
	*psOut = pResult->sRaw;
	pResult->sRaw = NULL;
	xrtImapFetchRawResultFree(pResult);
	xFutureRelease(pFuture);
	return pOut ? pOut->bSuccess : TRUE;
}

static bool xrtImapUidFetchHeaderFieldsRawAsyncWait(const ximapconfig* pCfg, const char* sUid, const char* const* arrFields, size_t iFieldCount, const ximapasyncopts* pOpts, str* psRaw, ximapresult* pOut)
{
	return ximap_uid_header_fields_async_wait_ex(pCfg, sUid, arrFields, iFieldCount, pOpts, FALSE, FALSE, psRaw, pOut);
}

static bool xrtImapUidFetchHeaderFieldsAsyncWait(const ximapconfig* pCfg, const char* sUid, const char* const* arrFields, size_t iFieldCount, const ximapasyncopts* pOpts, str* psHeader, ximapresult* pOut)
{
	return ximap_uid_header_fields_async_wait_ex(pCfg, sUid, arrFields, iFieldCount, pOpts, TRUE, FALSE, psHeader, pOut);
}

static bool xrtImapUidPeekHeaderFieldsRawAsyncWait(const ximapconfig* pCfg, const char* sUid, const char* const* arrFields, size_t iFieldCount, const ximapasyncopts* pOpts, str* psRaw, ximapresult* pOut)
{
	return ximap_uid_header_fields_async_wait_ex(pCfg, sUid, arrFields, iFieldCount, pOpts, FALSE, TRUE, psRaw, pOut);
}

static bool xrtImapUidPeekHeaderFieldsAsyncWait(const ximapconfig* pCfg, const char* sUid, const char* const* arrFields, size_t iFieldCount, const ximapasyncopts* pOpts, str* psHeader, ximapresult* pOut)
{
	return ximap_uid_header_fields_async_wait_ex(pCfg, sUid, arrFields, iFieldCount, pOpts, TRUE, TRUE, psHeader, pOut);
}

static bool xrtImapUidFetchMimeAsyncWait(const ximapconfig* pCfg, const char* sUid, const ximapasyncopts* pOpts, xmailparsedmessage* pMsg, ximapresult* pOut)
{
	xfuture* pFuture;
	ximapfetchmimeresult* pResult;

	if ( pMsg != NULL ) {
		memset(pMsg, 0, sizeof(*pMsg));
	}
	if ( pOut != NULL ) {
		xrtImapResultInit(pOut);
	}
	if ( pMsg == NULL ) {
		ximap_result_error(pOut, "IMAP async MIME output missing");
		return FALSE;
	}
	pFuture = xrtImapUidFetchMimeFuture(pCfg, sUid, pOpts);
	if ( pFuture == NULL ) {
		ximap_result_error(pOut, "IMAP async MIME future create failed");
		return FALSE;
	}
	pResult = (ximapfetchmimeresult*)xFutureWaitValue(pFuture);
	if ( pResult == NULL ) {
		xfuture_result tResult;
		memset(&tResult, 0, sizeof(tResult));
		if ( xFutureGetResult(pFuture, &tResult) && tResult.pValue != NULL ) {
			pResult = (ximapfetchmimeresult*)tResult.pValue;
		}
	}
	if ( pResult == NULL ) {
		str sError = xFutureError(pFuture);
		ximap_result_error(pOut, sError ? (const char*)sError : "IMAP async MIME wait failed");
		xFutureRelease(pFuture);
		return FALSE;
	}
	if ( pOut != NULL ) {
		*pOut = pResult->tResult;
	}
	*pMsg = pResult->tMessage;
	memset(&pResult->tMessage, 0, sizeof(pResult->tMessage));
	xrtImapFetchMimeResultFree(pResult);
	xFutureRelease(pFuture);
	return pOut ? pOut->bSuccess : TRUE;
}

static bool xrtImapUidPeekMimeAsyncWait(const ximapconfig* pCfg, const char* sUid, const ximapasyncopts* pOpts, xmailparsedmessage* pMsg, ximapresult* pOut)
{
	xfuture* pFuture;
	ximapfetchmimeresult* pResult;

	if ( pMsg != NULL ) {
		memset(pMsg, 0, sizeof(*pMsg));
	}
	if ( pOut != NULL ) {
		xrtImapResultInit(pOut);
	}
	if ( pMsg == NULL ) {
		ximap_result_error(pOut, "IMAP async PEEK MIME output missing");
		return FALSE;
	}
	pFuture = xrtImapUidPeekMimeFuture(pCfg, sUid, pOpts);
	if ( pFuture == NULL ) {
		ximap_result_error(pOut, "IMAP async PEEK MIME future create failed");
		return FALSE;
	}
	pResult = (ximapfetchmimeresult*)xFutureWaitValue(pFuture);
	if ( pResult == NULL ) {
		xfuture_result tResult;
		memset(&tResult, 0, sizeof(tResult));
		if ( xFutureGetResult(pFuture, &tResult) && tResult.pValue != NULL ) {
			pResult = (ximapfetchmimeresult*)tResult.pValue;
		}
	}
	if ( pResult == NULL ) {
		str sError = xFutureError(pFuture);
		ximap_result_error(pOut, sError ? (const char*)sError : "IMAP async PEEK MIME wait failed");
		xFutureRelease(pFuture);
		return FALSE;
	}
	if ( pOut != NULL ) {
		*pOut = pResult->tResult;
	}
	*pMsg = pResult->tMessage;
	memset(&pResult->tMessage, 0, sizeof(pResult->tMessage));
	xrtImapFetchMimeResultFree(pResult);
	xFutureRelease(pFuture);
	return pOut ? pOut->bSuccess : TRUE;
}

#endif
