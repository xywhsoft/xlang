#ifndef XRT_EXTLIB_XSMTP_H
#define XRT_EXTLIB_XSMTP_H

#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdarg.h>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "../xmail_mime/xmail_mime.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#if defined(XSMTP_DEBUG)
#define XSMTP_TRACE(...) do { fprintf(stderr, "[xsmtp] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); fflush(stderr); } while (0)
#else
#define XSMTP_TRACE(...) ((void)0)
#endif

#define XSMTP_SECURE_AUTO      0
#define XSMTP_SECURE_NONE      1
#define XSMTP_SECURE_SSL       2
#define XSMTP_SECURE_STARTTLS  3

#define XSMTP_AUTH_AUTO        0
#define XSMTP_AUTH_NONE        1
#define XSMTP_AUTH_PLAIN       2
#define XSMTP_AUTH_LOGIN       3
#define XSMTP_AUTH_XOAUTH2     4

#define XSMTP_TLS_VERSION_DEFAULT 0
#define XSMTP_TLS_VERSION_1_2     0x0303u
#define XSMTP_TLS_VERSION_1_3     0x0304u

#define XSMTP_CAP_AUTH_PLAIN   0x0001u
#define XSMTP_CAP_AUTH_LOGIN   0x0002u
#define XSMTP_CAP_STARTTLS     0x0004u
#define XSMTP_CAP_8BITMIME     0x0008u
#define XSMTP_CAP_SMTPUTF8     0x0010u
#define XSMTP_CAP_SIZE         0x0020u
#define XSMTP_CAP_AUTH_XOAUTH2 0x0040u
#define XSMTP_CAP_DSN          0x0080u

#define XSMTP_STAGE_NONE       0
#define XSMTP_STAGE_CONNECT    1
#define XSMTP_STAGE_TLS        2
#define XSMTP_STAGE_BANNER     3
#define XSMTP_STAGE_EHLO       4
#define XSMTP_STAGE_STARTTLS   5
#define XSMTP_STAGE_AUTH       6
#define XSMTP_STAGE_MAIL_FROM  7
#define XSMTP_STAGE_RCPT       8
#define XSMTP_STAGE_DATA       9
#define XSMTP_STAGE_QUIT       10

typedef struct xsmtpsession_struct xsmtpsession;

typedef struct {
	const char* sEmail;
	const char* sName;
	const char* sDsnNotify;
	const char* sDsnOrcpt;
} xsmtpaddr;

typedef struct {
	const char* sFileName;
	const char* sContentType;
	const char* sContentId;
	const void* pData;
	size_t iDataLen;
	bool bInline;
} xsmtpattachment;

typedef struct {
	const char* sHost;
	uint16 iPort;
	uint32 iTimeoutMs;
	int iSecureMode;
	bool bAuth;
	int iAuthMode;
	bool bVerifyPeer;
	uint16 iTlsMaxVersion;
	const char* sCaFile;
	const char* sUser;
	const char* sPass;
	const char* sOAuth2Token;
	const char* sHeloName;
	xsmtpaddr tFrom;
	xsmtpaddr tReplyTo;
} xsmtpconfig;

typedef struct {
	xsmtpaddr tFrom;
	xsmtpaddr tReplyTo;
	const xsmtpaddr* arrTo;
	size_t iToCount;
	const xsmtpaddr* arrCc;
	size_t iCcCount;
	const xsmtpaddr* arrBcc;
	size_t iBccCount;
	const char* sSubject;
	const char* sTextBody;
	const char* sHtmlBody;
	const xsmtpattachment* arrAttachments;
	size_t iAttachmentCount;
	const char* const* arrHeaderNames;
	const char* const* arrHeaderValues;
	size_t iHeaderCount;
	const char* sDsnReturn;
	const char* sDsnEnvid;
} xsmtpmessage;

typedef struct {
	bool bSuccess;
	bool bUsedTLS;
	bool bUsedStartTLS;
	int iServerCode;
	int iAuthMode;
	int iStage;
	uint32 iCapabilities;
	uint64 iSizeLimit;
	char sError[256];
	char sLastReply[1024];
} xsmtpresult;

typedef struct {
	uint32 iTimeoutMs;
	xnetengine* pEngine;
	const char* sDebugName;
} xsmtpasyncopts;

typedef struct {
	char* sData;
	size_t iLen;
	size_t iCap;
} xsmtpbuf;

typedef struct {
	xsmtpconfig tCfg;
	xsmtpmessage tMsg;
	xsmtpresult* pRet;
	xsmtpaddr* arrTo;
	xsmtpaddr* arrCc;
	xsmtpaddr* arrBcc;
	xsmtpattachment* arrAttachments;
	char** arrHeaderNames;
	char** arrHeaderValues;
} xsmtptaskctx;

typedef enum {
	XSMTP_AST_NONE = 0,
	XSMTP_AST_WAIT_BANNER,
	XSMTP_AST_WAIT_EHLO,
	XSMTP_AST_WAIT_STARTTLS,
	XSMTP_AST_WAIT_STARTTLS_TLS,
	XSMTP_AST_WAIT_EHLO_TLS,
	XSMTP_AST_WAIT_AUTH,
	XSMTP_AST_WAIT_AUTH_LOGIN_USER,
	XSMTP_AST_WAIT_AUTH_LOGIN_PASS,
	XSMTP_AST_WAIT_MAIL_FROM,
	XSMTP_AST_WAIT_RCPT,
	XSMTP_AST_WAIT_DATA,
	XSMTP_AST_WAIT_BODY,
	XSMTP_AST_WAIT_QUIT
} xsmtpasyncstate;

typedef struct {
	xnetengine* pEngine;
	xnetstream* pStream;
	xpromise* pPromise;
	xsmtptaskctx* pCtx;
	xsmtpresult* pRet;
	xsmtpsession* pSess;
	xsmtpbuf tReply;
	xsmtpbuf tScratch;
	int iReplyCode;
	uint32 iReplyCaps;
	bool bReplyHasFirst;
	bool bDone;
	bool bUsingNative;
	uint32 iWaitToken;
	xsmtpasyncstate iState;
	uint32 iCapabilities;
	uint64 iSizeLimit;
	uint64 iReplySizeLimit;
	int iAuthMode;
	size_t iRcptIndex;
	int iRcptListKind;
	xtlsconfig tTlsCfg;
} xsmtpasyncop;

typedef struct xsmtpsession_struct {
	xnetengine* pEngine;
	xnetstream* pStream;
	xtlssession* pTls;
	xmutex pLock;
	xcond pCond;
	bool bOwnEngine;
	bool bOpen;
	bool bClosed;
	bool bStreamTls;
	bool bTlsReady;
	bool bRecvOverflow;
	int iCloseReason;
	int iLastSysErr;
	uint32 iTimeoutMs;
	xsmtpbuf tWireRecv;
	char aRecv[32768];
	size_t iRecvLen;
} xsmtpsession;

static bool xsmtp_buf_reserve(xsmtpbuf* pBuf, size_t iNeed);
static bool xsmtp_buf_append_n(xsmtpbuf* pBuf, const char* sText, size_t iLen);
static bool xsmtp_buf_append(xsmtpbuf* pBuf, const char* sText);
static void xsmtp_buf_unit(xsmtpbuf* pBuf);
static void xsmtp_result_errorf(xsmtpresult* pRet, const char* sFormat, ...);

static void xrtSmtpConfigInit(xsmtpconfig* pCfg)
{
	if ( pCfg == NULL ) {
		return;
	}
	memset(pCfg, 0, sizeof(*pCfg));
	pCfg->iPort = 465;
	pCfg->iTimeoutMs = 15000;
	pCfg->iSecureMode = XSMTP_SECURE_AUTO;
	pCfg->bAuth = TRUE;
	pCfg->iAuthMode = XSMTP_AUTH_AUTO;
	pCfg->bVerifyPeer = TRUE;
	pCfg->iTlsMaxVersion = XSMTP_TLS_VERSION_DEFAULT;
}

static void xrtSmtpMessageInit(xsmtpmessage* pMsg)
{
	if ( pMsg == NULL ) {
		return;
	}
	memset(pMsg, 0, sizeof(*pMsg));
}

static void xrtSmtpResultInit(xsmtpresult* pRet)
{
	if ( pRet == NULL ) {
		return;
	}
	memset(pRet, 0, sizeof(*pRet));
}

static void xrtSmtpAsyncOptsInit(xsmtpasyncopts* pOpts)
{
	if ( pOpts == NULL ) {
		return;
	}
	memset(pOpts, 0, sizeof(*pOpts));
}

static void xrtSmtpResultFree(xsmtpresult* pRet)
{
	if ( pRet != NULL ) {
		xrtFree(pRet);
	}
}

static void xsmtp_stream_close_destroy(xnetstream** ppStream, uint32 iTimeoutMs)
{
	xnetstream* pStream;

	if ( ppStream == NULL || *ppStream == NULL ) {
		return;
	}

	pStream = *ppStream;
	xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
	(void)xrtNetStreamWaitTimeoutEx(pStream, XNET_STREAM_WAIT_CLOSE, iTimeoutMs > 0 ? iTimeoutMs : 1000u);
	if ( xrtGetError() ) {
		xrtClearError();
	}
	xrtNetStreamDestroy(pStream);
	if ( xrtGetError() ) {
		xrtClearError();
	}
	*ppStream = NULL;
}

static const char* xsmtp_str_or_empty(const char* sText)
{
	return sText ? sText : "";
}

static char* xsmtp_dup_text(const char* sText)
{
	size_t iLen;
	char* sCopy;

	if ( sText == NULL ) {
		return NULL;
	}
	iLen = strlen(sText);
	sCopy = (char*)xrtMalloc(iLen + 1);
	if ( sCopy == NULL ) {
		return NULL;
	}
	memcpy(sCopy, sText, iLen + 1);
	return sCopy;
}

static void xsmtp_free_addr(xsmtpaddr* pAddr);

static bool xsmtp_copy_addr(xsmtpaddr* pDst, const xsmtpaddr* pSrc)
{
	memset(pDst, 0, sizeof(*pDst));
	if ( pSrc == NULL ) {
		return TRUE;
	}
	if ( pSrc->sEmail != NULL ) {
		pDst->sEmail = xsmtp_dup_text(pSrc->sEmail);
		if ( pDst->sEmail == NULL ) {
			return FALSE;
		}
	}
	if ( pSrc->sName != NULL ) {
		pDst->sName = xsmtp_dup_text(pSrc->sName);
		if ( pDst->sName == NULL ) {
			xrtFree((ptr)pDst->sEmail);
			pDst->sEmail = NULL;
			return FALSE;
		}
	}
	if ( pSrc->sDsnNotify != NULL ) {
		pDst->sDsnNotify = xsmtp_dup_text(pSrc->sDsnNotify);
		if ( pDst->sDsnNotify == NULL ) {
			xsmtp_free_addr(pDst);
			return FALSE;
		}
	}
	if ( pSrc->sDsnOrcpt != NULL ) {
		pDst->sDsnOrcpt = xsmtp_dup_text(pSrc->sDsnOrcpt);
		if ( pDst->sDsnOrcpt == NULL ) {
			xsmtp_free_addr(pDst);
			return FALSE;
		}
	}
	return TRUE;
}

static void xsmtp_free_addr(xsmtpaddr* pAddr)
{
	if ( pAddr == NULL ) {
		return;
	}
	if ( pAddr->sEmail != NULL ) {
		xrtFree((ptr)pAddr->sEmail);
	}
	if ( pAddr->sName != NULL ) {
		xrtFree((ptr)pAddr->sName);
	}
	if ( pAddr->sDsnNotify != NULL ) {
		xrtFree((ptr)pAddr->sDsnNotify);
	}
	if ( pAddr->sDsnOrcpt != NULL ) {
		xrtFree((ptr)pAddr->sDsnOrcpt);
	}
	memset(pAddr, 0, sizeof(*pAddr));
}

static xsmtpaddr* xsmtp_dup_addr_list(const xsmtpaddr* arrSrc, size_t iCount)
{
	xsmtpaddr* arrDst;
	size_t i;

	if ( arrSrc == NULL || iCount == 0 ) {
		return NULL;
	}
	arrDst = (xsmtpaddr*)xrtMalloc(sizeof(xsmtpaddr) * iCount);
	if ( arrDst == NULL ) {
		return NULL;
	}
	memset(arrDst, 0, sizeof(xsmtpaddr) * iCount);
	for ( i = 0; i < iCount; ++i ) {
		if ( !xsmtp_copy_addr(&arrDst[i], &arrSrc[i]) ) {
			while ( i > 0 ) {
				--i;
				xsmtp_free_addr(&arrDst[i]);
			}
			xrtFree(arrDst);
			return NULL;
		}
	}
	return arrDst;
}

static void xsmtp_free_addr_list(xsmtpaddr* arrList, size_t iCount)
{
	size_t i;

	if ( arrList == NULL ) {
		return;
	}
	for ( i = 0; i < iCount; ++i ) {
		xsmtp_free_addr(&arrList[i]);
	}
	xrtFree(arrList);
}

static void xsmtp_free_attachment(xsmtpattachment* pAttachment)
{
	if ( pAttachment == NULL ) {
		return;
	}
	if ( pAttachment->sFileName != NULL ) {
		xrtFree((ptr)pAttachment->sFileName);
	}
	if ( pAttachment->sContentType != NULL ) {
		xrtFree((ptr)pAttachment->sContentType);
	}
	if ( pAttachment->sContentId != NULL ) {
		xrtFree((ptr)pAttachment->sContentId);
	}
	if ( pAttachment->pData != NULL ) {
		xrtFree((ptr)pAttachment->pData);
	}
	memset(pAttachment, 0, sizeof(*pAttachment));
}

static bool xsmtp_copy_attachment(xsmtpattachment* pDst, const xsmtpattachment* pSrc)
{
	void* pData;

	memset(pDst, 0, sizeof(*pDst));
	if ( pSrc == NULL ) {
		return TRUE;
	}
	pDst->iDataLen = pSrc->iDataLen;
	pDst->bInline = pSrc->bInline;
	if ( pSrc->sFileName != NULL ) {
		pDst->sFileName = xsmtp_dup_text(pSrc->sFileName);
		if ( pDst->sFileName == NULL ) goto fail;
	}
	if ( pSrc->sContentType != NULL ) {
		pDst->sContentType = xsmtp_dup_text(pSrc->sContentType);
		if ( pDst->sContentType == NULL ) goto fail;
	}
	if ( pSrc->sContentId != NULL ) {
		pDst->sContentId = xsmtp_dup_text(pSrc->sContentId);
		if ( pDst->sContentId == NULL ) goto fail;
	}
	if ( pSrc->pData != NULL && pSrc->iDataLen > 0 ) {
		pData = xrtMalloc(pSrc->iDataLen);
		if ( pData == NULL ) goto fail;
		memcpy(pData, pSrc->pData, pSrc->iDataLen);
		pDst->pData = pData;
	} else {
		pDst->pData = NULL;
		pDst->iDataLen = 0;
	}
	return TRUE;

fail:
	xsmtp_free_attachment(pDst);
	return FALSE;
}

static xsmtpattachment* xsmtp_dup_attachment_list(const xsmtpattachment* arrSrc, size_t iCount)
{
	xsmtpattachment* arrDst;
	size_t i;

	if ( arrSrc == NULL || iCount == 0 ) {
		return NULL;
	}
	arrDst = (xsmtpattachment*)xrtMalloc(sizeof(xsmtpattachment) * iCount);
	if ( arrDst == NULL ) {
		return NULL;
	}
	memset(arrDst, 0, sizeof(xsmtpattachment) * iCount);
	for ( i = 0; i < iCount; ++i ) {
		if ( !xsmtp_copy_attachment(&arrDst[i], &arrSrc[i]) ) {
			while ( i > 0 ) {
				--i;
				xsmtp_free_attachment(&arrDst[i]);
			}
			xrtFree(arrDst);
			return NULL;
		}
	}
	return arrDst;
}

static void xsmtp_free_attachment_list(xsmtpattachment* arrList, size_t iCount)
{
	size_t i;

	if ( arrList == NULL ) {
		return;
	}
	for ( i = 0; i < iCount; ++i ) {
		xsmtp_free_attachment(&arrList[i]);
	}
	xrtFree(arrList);
}

static bool xsmtp_copy_text_list(char*** ppDst, const char* const* arrSrc, size_t iCount)
{
	char** arrDst;
	size_t i;

	*ppDst = NULL;
	if ( arrSrc == NULL || iCount == 0 ) {
		return TRUE;
	}
	arrDst = (char**)xrtMalloc(sizeof(char*) * iCount);
	if ( arrDst == NULL ) {
		return FALSE;
	}
	memset(arrDst, 0, sizeof(char*) * iCount);
	for ( i = 0; i < iCount; ++i ) {
		if ( arrSrc[i] != NULL ) {
			arrDst[i] = xsmtp_dup_text(arrSrc[i]);
			if ( arrDst[i] == NULL ) {
				while ( i > 0 ) {
					--i;
					if ( arrDst[i] != NULL ) {
						xrtFree(arrDst[i]);
					}
				}
				xrtFree(arrDst);
				return FALSE;
			}
		}
	}
	*ppDst = arrDst;
	return TRUE;
}

static void xsmtp_free_text_list(char** arrText, size_t iCount)
{
	size_t i;

	if ( arrText == NULL ) {
		return;
	}
	for ( i = 0; i < iCount; ++i ) {
		if ( arrText[i] != NULL ) {
			xrtFree(arrText[i]);
		}
	}
	xrtFree(arrText);
}

static void xsmtp_result_error(xsmtpresult* pRet, const char* sError)
{
	if ( pRet == NULL ) {
		return;
	}
	pRet->bSuccess = FALSE;
	snprintf(pRet->sError, sizeof(pRet->sError), "%s", xsmtp_str_or_empty(sError));
}

static void xsmtp_result_tls_error(xsmtpresult* pRet, const char* sError, xnet_result iResult)
{
	if ( pRet == NULL ) {
		return;
	}
	pRet->bSuccess = FALSE;
	snprintf(pRet->sError, sizeof(pRet->sError), "%s (tls=%d)", xsmtp_str_or_empty(sError), (int)iResult);
}

static void xsmtp_result_stage(xsmtpresult* pRet, int iStage)
{
	if ( pRet != NULL ) {
		pRet->iStage = iStage;
	}
}

static void xsmtp_task_ctx_free(xsmtptaskctx* pCtx)
{
	if ( pCtx == NULL ) {
		return;
	}
	xsmtp_free_addr(&pCtx->tCfg.tFrom);
	xsmtp_free_addr(&pCtx->tCfg.tReplyTo);
	if ( pCtx->tCfg.sHost != NULL ) {
		xrtFree((ptr)pCtx->tCfg.sHost);
	}
	if ( pCtx->tCfg.sUser != NULL ) {
		xrtFree((ptr)pCtx->tCfg.sUser);
	}
	if ( pCtx->tCfg.sPass != NULL ) {
		xrtFree((ptr)pCtx->tCfg.sPass);
	}
	if ( pCtx->tCfg.sOAuth2Token != NULL ) {
		xrtFree((ptr)pCtx->tCfg.sOAuth2Token);
	}
	if ( pCtx->tCfg.sCaFile != NULL ) {
		xrtFree((ptr)pCtx->tCfg.sCaFile);
	}
	if ( pCtx->tCfg.sHeloName != NULL ) {
		xrtFree((ptr)pCtx->tCfg.sHeloName);
	}
	xsmtp_free_addr(&pCtx->tMsg.tFrom);
	xsmtp_free_addr(&pCtx->tMsg.tReplyTo);
	xsmtp_free_addr_list(pCtx->arrTo, pCtx->tMsg.iToCount);
	xsmtp_free_addr_list(pCtx->arrCc, pCtx->tMsg.iCcCount);
	xsmtp_free_addr_list(pCtx->arrBcc, pCtx->tMsg.iBccCount);
	xsmtp_free_attachment_list(pCtx->arrAttachments, pCtx->tMsg.iAttachmentCount);
	xsmtp_free_text_list(pCtx->arrHeaderNames, pCtx->tMsg.iHeaderCount);
	xsmtp_free_text_list(pCtx->arrHeaderValues, pCtx->tMsg.iHeaderCount);
	if ( pCtx->tMsg.sSubject != NULL ) {
		xrtFree((ptr)pCtx->tMsg.sSubject);
	}
	if ( pCtx->tMsg.sTextBody != NULL ) {
		xrtFree((ptr)pCtx->tMsg.sTextBody);
	}
	if ( pCtx->tMsg.sHtmlBody != NULL ) {
		xrtFree((ptr)pCtx->tMsg.sHtmlBody);
	}
	if ( pCtx->tMsg.sDsnReturn != NULL ) {
		xrtFree((ptr)pCtx->tMsg.sDsnReturn);
	}
	if ( pCtx->tMsg.sDsnEnvid != NULL ) {
		xrtFree((ptr)pCtx->tMsg.sDsnEnvid);
	}
	xrtFree(pCtx);
}

static xsmtptaskctx* xsmtp_task_ctx_create(const xsmtpconfig* pCfg, const xsmtpmessage* pMsg, const xsmtpasyncopts* pOpts)
{
	xsmtptaskctx* pCtx;

	if ( pCfg == NULL || pMsg == NULL ) {
		return NULL;
	}

	pCtx = (xsmtptaskctx*)xrtMalloc(sizeof(*pCtx));
	if ( pCtx == NULL ) {
		return NULL;
	}
	memset(pCtx, 0, sizeof(*pCtx));

	pCtx->tCfg = *pCfg;
	pCtx->tMsg = *pMsg;
	pCtx->tCfg.sHost = xsmtp_dup_text(pCfg->sHost);
	pCtx->tCfg.sUser = xsmtp_dup_text(pCfg->sUser);
	pCtx->tCfg.sPass = xsmtp_dup_text(pCfg->sPass);
	pCtx->tCfg.sOAuth2Token = xsmtp_dup_text(pCfg->sOAuth2Token);
	pCtx->tCfg.sCaFile = xsmtp_dup_text(pCfg->sCaFile);
	pCtx->tCfg.sHeloName = xsmtp_dup_text(pCfg->sHeloName);
	if ( !xsmtp_copy_addr(&pCtx->tCfg.tFrom, &pCfg->tFrom) ) {
		xsmtp_task_ctx_free(pCtx);
		return NULL;
	}
	if ( !xsmtp_copy_addr(&pCtx->tCfg.tReplyTo, &pCfg->tReplyTo) ) {
		xsmtp_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pCfg->sHost != NULL && pCtx->tCfg.sHost == NULL ) {
		xsmtp_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pCfg->sUser != NULL && pCtx->tCfg.sUser == NULL ) {
		xsmtp_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pCfg->sPass != NULL && pCtx->tCfg.sPass == NULL ) {
		xsmtp_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pCfg->sOAuth2Token != NULL && pCtx->tCfg.sOAuth2Token == NULL ) {
		xsmtp_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pCfg->sCaFile != NULL && pCtx->tCfg.sCaFile == NULL ) {
		xsmtp_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pCfg->sHeloName != NULL && pCtx->tCfg.sHeloName == NULL ) {
		xsmtp_task_ctx_free(pCtx);
		return NULL;
	}

	if ( pOpts != NULL && pOpts->iTimeoutMs > 0 ) {
		pCtx->tCfg.iTimeoutMs = pOpts->iTimeoutMs;
	}

	if ( !xsmtp_copy_addr(&pCtx->tMsg.tFrom, &pMsg->tFrom) ) {
		xsmtp_task_ctx_free(pCtx);
		return NULL;
	}
	if ( !xsmtp_copy_addr(&pCtx->tMsg.tReplyTo, &pMsg->tReplyTo) ) {
		xsmtp_task_ctx_free(pCtx);
		return NULL;
	}
	pCtx->arrTo = xsmtp_dup_addr_list(pMsg->arrTo, pMsg->iToCount);
	pCtx->arrCc = xsmtp_dup_addr_list(pMsg->arrCc, pMsg->iCcCount);
	pCtx->arrBcc = xsmtp_dup_addr_list(pMsg->arrBcc, pMsg->iBccCount);
	if ( (pMsg->iToCount > 0 && pCtx->arrTo == NULL)
		|| (pMsg->iCcCount > 0 && pCtx->arrCc == NULL)
		|| (pMsg->iBccCount > 0 && pCtx->arrBcc == NULL) ) {
		xsmtp_task_ctx_free(pCtx);
		return NULL;
	}
	pCtx->tMsg.arrTo = pCtx->arrTo;
	pCtx->tMsg.arrCc = pCtx->arrCc;
	pCtx->tMsg.arrBcc = pCtx->arrBcc;

	pCtx->arrAttachments = xsmtp_dup_attachment_list(pMsg->arrAttachments, pMsg->iAttachmentCount);
	if ( pMsg->iAttachmentCount > 0 && pCtx->arrAttachments == NULL ) {
		xsmtp_task_ctx_free(pCtx);
		return NULL;
	}
	pCtx->tMsg.arrAttachments = pCtx->arrAttachments;

	pCtx->tMsg.sSubject = xsmtp_dup_text(pMsg->sSubject);
	pCtx->tMsg.sTextBody = xsmtp_dup_text(pMsg->sTextBody);
	pCtx->tMsg.sHtmlBody = xsmtp_dup_text(pMsg->sHtmlBody);
	pCtx->tMsg.sDsnReturn = xsmtp_dup_text(pMsg->sDsnReturn);
	pCtx->tMsg.sDsnEnvid = xsmtp_dup_text(pMsg->sDsnEnvid);
	if ( (pMsg->sSubject != NULL && pCtx->tMsg.sSubject == NULL)
		|| (pMsg->sTextBody != NULL && pCtx->tMsg.sTextBody == NULL)
		|| (pMsg->sHtmlBody != NULL && pCtx->tMsg.sHtmlBody == NULL)
		|| (pMsg->sDsnReturn != NULL && pCtx->tMsg.sDsnReturn == NULL)
		|| (pMsg->sDsnEnvid != NULL && pCtx->tMsg.sDsnEnvid == NULL) ) {
		xsmtp_task_ctx_free(pCtx);
		return NULL;
	}

	if ( !xsmtp_copy_text_list(&pCtx->arrHeaderNames, pMsg->arrHeaderNames, pMsg->iHeaderCount)
		|| !xsmtp_copy_text_list(&pCtx->arrHeaderValues, pMsg->arrHeaderValues, pMsg->iHeaderCount) ) {
		xsmtp_task_ctx_free(pCtx);
		return NULL;
	}
	pCtx->tMsg.arrHeaderNames = (const char* const*)pCtx->arrHeaderNames;
	pCtx->tMsg.arrHeaderValues = (const char* const*)pCtx->arrHeaderValues;

	return pCtx;
}

static bool xsmtp_try_read_line_from_buffer(xsmtpsession* pSess, char* sOut, size_t iOutCap)
{
	size_t i;

	if ( pSess == NULL || sOut == NULL || iOutCap == 0 ) {
		return FALSE;
	}
	for ( i = 0; i + 1 < pSess->iRecvLen; ++i ) {
		if ( pSess->aRecv[i] == '\r' && pSess->aRecv[i + 1] == '\n' ) {
			size_t iCopy = i;
			if ( iCopy >= iOutCap ) {
				iCopy = iOutCap - 1;
			}
			memcpy(sOut, pSess->aRecv, iCopy);
			sOut[iCopy] = '\0';
			memmove(pSess->aRecv, pSess->aRecv + i + 2, pSess->iRecvLen - (i + 2));
			pSess->iRecvLen -= (i + 2);
			pSess->aRecv[pSess->iRecvLen] = '\0';
			return TRUE;
		}
	}
	return FALSE;
}

static char* xsmtp_build_data_body_bytes(const char* sData)
{
	xsmtpbuf tBuf;
	const char* p;

	memset(&tBuf, 0, sizeof(tBuf));
	if ( !xsmtp_buf_reserve(&tBuf, strlen(xsmtp_str_or_empty(sData)) + 16) ) {
		return NULL;
	}
	for ( p = xsmtp_str_or_empty(sData); *p; ) {
		const char* sLineEnd = strstr(p, "\r\n");
		size_t iLineLen = sLineEnd ? (size_t)(sLineEnd - p) : strlen(p);
		if ( iLineLen > 0 && p[0] == '.' ) {
			if ( !xsmtp_buf_append(&tBuf, ".") ) {
				xsmtp_buf_unit(&tBuf);
				return NULL;
			}
		}
		if ( !xsmtp_buf_append_n(&tBuf, p, iLineLen) || !xsmtp_buf_append(&tBuf, "\r\n") ) {
			xsmtp_buf_unit(&tBuf);
			return NULL;
		}
		if ( !sLineEnd ) {
			break;
		}
		p = sLineEnd + 2;
	}
	if ( !xsmtp_buf_append(&tBuf, ".\r\n") ) {
		xsmtp_buf_unit(&tBuf);
		return NULL;
	}
	return tBuf.sData;
}

static bool xsmtp_starts_with_i(const char* sText, const char* sPrefix)
{
	size_t i;

	if ( sText == NULL || sPrefix == NULL ) {
		return FALSE;
	}
	for ( i = 0; sPrefix[i] != '\0'; i++ ) {
		unsigned char a = (unsigned char)sText[i];
		unsigned char b = (unsigned char)sPrefix[i];
		if ( a == '\0' ) {
			return FALSE;
		}
		if ( tolower(a) != tolower(b) ) {
			return FALSE;
		}
	}
	return TRUE;
}

static bool xsmtp_find_i(const char* sText, const char* sSub)
{
	size_t i;
	size_t j;

	if ( sText == NULL || sSub == NULL || sSub[0] == '\0' ) {
		return FALSE;
	}
	for ( i = 0; sText[i] != '\0'; i++ ) {
		for ( j = 0; sSub[j] != '\0'; j++ ) {
			if ( sText[i + j] == '\0' ) {
				return FALSE;
			}
			if ( tolower((unsigned char)sText[i + j]) != tolower((unsigned char)sSub[j]) ) {
				break;
			}
		}
		if ( sSub[j] == '\0' ) {
			return TRUE;
		}
	}
	return FALSE;
}

static bool xsmtp_cap_name_match(const char* sCap, const char* sName)
{
	size_t i;

	if ( sCap == NULL || sName == NULL ) {
		return FALSE;
	}
	for ( i = 0; sName[i] != '\0'; i++ ) {
		if ( sCap[i] == '\0' || tolower((unsigned char)sCap[i]) != tolower((unsigned char)sName[i]) ) {
			return FALSE;
		}
	}
	return sCap[i] == '\0' || isspace((unsigned char)sCap[i]);
}

static uint64 xsmtp_parse_uint64_decimal(const char* sText)
{
	uint64 iValue = 0;
	const unsigned char* p = (const unsigned char*)sText;

	if ( p == NULL ) {
		return 0;
	}
	while ( *p && isspace(*p) ) {
		p++;
	}
	while ( *p && isdigit(*p) ) {
		uint64 iDigit = (uint64)(*p - '0');
		if ( iValue > ((((uint64)-1) - iDigit) / 10u) ) {
			return (uint64)-1;
		}
		iValue = iValue * 10u + iDigit;
		p++;
	}
	return iValue;
}

static void xsmtp_parse_capability_line(const char* sCap, uint32* pCaps, uint64* pSizeLimit)
{
	if ( sCap == NULL || pCaps == NULL ) {
		return;
	}
	if ( xsmtp_cap_name_match(sCap, "STARTTLS") ) {
		*pCaps |= XSMTP_CAP_STARTTLS;
	}
	if ( xsmtp_cap_name_match(sCap, "8BITMIME") ) {
		*pCaps |= XSMTP_CAP_8BITMIME;
	}
	if ( xsmtp_cap_name_match(sCap, "SMTPUTF8") ) {
		*pCaps |= XSMTP_CAP_SMTPUTF8;
	}
	if ( xsmtp_cap_name_match(sCap, "SIZE") ) {
		*pCaps |= XSMTP_CAP_SIZE;
		if ( pSizeLimit != NULL ) {
			const char* p = sCap + 4;
			*pSizeLimit = xsmtp_parse_uint64_decimal(p);
		}
	}
	if ( xsmtp_cap_name_match(sCap, "DSN") ) {
		*pCaps |= XSMTP_CAP_DSN;
	}
	if ( xsmtp_starts_with_i(sCap, "AUTH ") ) {
		if ( xsmtp_find_i(sCap, "PLAIN") ) {
			*pCaps |= XSMTP_CAP_AUTH_PLAIN;
		}
		if ( xsmtp_find_i(sCap, "LOGIN") ) {
			*pCaps |= XSMTP_CAP_AUTH_LOGIN;
		}
		if ( xsmtp_find_i(sCap, "XOAUTH2") ) {
			*pCaps |= XSMTP_CAP_AUTH_XOAUTH2;
		}
	}
}

static bool xsmtp_buf_reserve(xsmtpbuf* pBuf, size_t iNeed)
{
	char* sNew;
	size_t iCap;

	if ( pBuf == NULL ) {
		return FALSE;
	}
	if ( pBuf->iLen + iNeed + 1 <= pBuf->iCap ) {
		return TRUE;
	}
	iCap = pBuf->iCap ? pBuf->iCap : 256;
	while ( iCap < pBuf->iLen + iNeed + 1 ) {
		iCap *= 2;
	}
	sNew = (char*)xrtRealloc(pBuf->sData, iCap);
	if ( sNew == NULL ) {
		return FALSE;
	}
	pBuf->sData = sNew;
	pBuf->iCap = iCap;
	return TRUE;
}

static bool xsmtp_buf_append_n(xsmtpbuf* pBuf, const char* sText, size_t iLen)
{
	if ( pBuf == NULL || sText == NULL ) {
		return FALSE;
	}
	if ( !xsmtp_buf_reserve(pBuf, iLen) ) {
		return FALSE;
	}
	memcpy(pBuf->sData + pBuf->iLen, sText, iLen);
	pBuf->iLen += iLen;
	pBuf->sData[pBuf->iLen] = '\0';
	return TRUE;
}

static bool xsmtp_buf_append(xsmtpbuf* pBuf, const char* sText)
{
	return xsmtp_buf_append_n(pBuf, xsmtp_str_or_empty(sText), strlen(xsmtp_str_or_empty(sText)));
}

static void xsmtp_buf_unit(xsmtpbuf* pBuf)
{
	if ( pBuf && pBuf->sData ) {
		xrtFree(pBuf->sData);
		pBuf->sData = NULL;
		pBuf->iLen = 0;
		pBuf->iCap = 0;
	}
}

static bool xsmtp_count_recipients(const xsmtpmessage* pMsg, size_t* pCount)
{
	size_t iTotal = 0;
	if ( pMsg == NULL || pCount == NULL ) {
		return FALSE;
	}
	iTotal += pMsg->iToCount;
	iTotal += pMsg->iCcCount;
	iTotal += pMsg->iBccCount;
	*pCount = iTotal;
	return iTotal > 0;
}

static xmailaddr* xsmtp_copy_xmail_addr_list(const xsmtpaddr* arrSrc, size_t iCount)
{
	xmailaddr* arrOut;
	size_t i;

	if ( arrSrc == NULL || iCount == 0 ) {
		return NULL;
	}
	arrOut = (xmailaddr*)xrtMalloc(sizeof(xmailaddr) * iCount);
	if ( arrOut == NULL ) {
		return NULL;
	}
	for ( i = 0; i < iCount; i++ ) {
		arrOut[i].sEmail = arrSrc[i].sEmail;
		arrOut[i].sName = arrSrc[i].sName;
	}
	return arrOut;
}

static xmailmimeattachment* xsmtp_copy_xmail_attachment_list(const xsmtpattachment* arrSrc, size_t iCount)
{
	xmailmimeattachment* arrOut;
	size_t i;

	if ( arrSrc == NULL || iCount == 0 ) {
		return NULL;
	}
	arrOut = (xmailmimeattachment*)xrtMalloc(sizeof(xmailmimeattachment) * iCount);
	if ( arrOut == NULL ) {
		return NULL;
	}
	for ( i = 0; i < iCount; i++ ) {
		arrOut[i].sFileName = arrSrc[i].sFileName;
		arrOut[i].sContentType = arrSrc[i].sContentType;
		arrOut[i].sContentId = arrSrc[i].sContentId;
		arrOut[i].pData = arrSrc[i].pData;
		arrOut[i].iDataLen = arrSrc[i].iDataLen;
		arrOut[i].bInline = arrSrc[i].bInline;
	}
	return arrOut;
}

static str xsmtp_build_message_data(const xsmtpconfig* pCfg, const xsmtpmessage* pMsg)
{
	xmailmessage tMail;
	xmailheader* arrHeaders = NULL;
	xmailaddr* arrTo = NULL;
	xmailaddr* arrCc = NULL;
	xmailaddr* arrBcc = NULL;
	xmailmimeattachment* arrAttachments = NULL;
	size_t i;
	str sOut = NULL;

	if ( pCfg == NULL || pMsg == NULL ) {
		return NULL;
	}

	memset(&tMail, 0, sizeof(tMail));
	tMail.tFrom.sEmail = (pMsg->tFrom.sEmail && pMsg->tFrom.sEmail[0]) ? pMsg->tFrom.sEmail : pCfg->tFrom.sEmail;
	tMail.tFrom.sName = (pMsg->tFrom.sEmail && pMsg->tFrom.sEmail[0]) ? pMsg->tFrom.sName : pCfg->tFrom.sName;
	tMail.tReplyTo.sEmail = (pMsg->tReplyTo.sEmail && pMsg->tReplyTo.sEmail[0]) ? pMsg->tReplyTo.sEmail : pCfg->tReplyTo.sEmail;
	tMail.tReplyTo.sName = (pMsg->tReplyTo.sEmail && pMsg->tReplyTo.sEmail[0]) ? pMsg->tReplyTo.sName : pCfg->tReplyTo.sName;
	tMail.sSubject = pMsg->sSubject;
	tMail.sTextBody = pMsg->sTextBody;
	tMail.sHtmlBody = pMsg->sHtmlBody;
	tMail.sMessageIdDomain = pCfg->sHost;

	if ( pMsg->iToCount > 0 ) {
		arrTo = xsmtp_copy_xmail_addr_list(pMsg->arrTo, pMsg->iToCount);
		if ( arrTo == NULL ) goto cleanup;
		tMail.arrTo = arrTo;
		tMail.iToCount = pMsg->iToCount;
	}
	if ( pMsg->iCcCount > 0 ) {
		arrCc = xsmtp_copy_xmail_addr_list(pMsg->arrCc, pMsg->iCcCount);
		if ( arrCc == NULL ) goto cleanup;
		tMail.arrCc = arrCc;
		tMail.iCcCount = pMsg->iCcCount;
	}
	if ( pMsg->iBccCount > 0 ) {
		arrBcc = xsmtp_copy_xmail_addr_list(pMsg->arrBcc, pMsg->iBccCount);
		if ( arrBcc == NULL ) goto cleanup;
		tMail.arrBcc = arrBcc;
		tMail.iBccCount = pMsg->iBccCount;
	}
	if ( pMsg->iAttachmentCount > 0 ) {
		arrAttachments = xsmtp_copy_xmail_attachment_list(pMsg->arrAttachments, pMsg->iAttachmentCount);
		if ( arrAttachments == NULL ) goto cleanup;
		tMail.arrAttachments = arrAttachments;
		tMail.iAttachmentCount = pMsg->iAttachmentCount;
	}

	if ( pMsg->arrHeaderNames && pMsg->arrHeaderValues && pMsg->iHeaderCount > 0 ) {
		arrHeaders = (xmailheader*)xrtMalloc(sizeof(xmailheader) * pMsg->iHeaderCount);
		if ( arrHeaders == NULL ) goto cleanup;
		for ( i = 0; i < pMsg->iHeaderCount; i++ ) {
			arrHeaders[i].sName = pMsg->arrHeaderNames[i];
			arrHeaders[i].sValue = pMsg->arrHeaderValues[i];
		}
		tMail.arrHeaders = arrHeaders;
		tMail.iHeaderCount = pMsg->iHeaderCount;
	}

	sOut = xmailMimeBuildMessage(&tMail);

cleanup:
	if ( arrHeaders ) xrtFree(arrHeaders);
	if ( arrTo ) xrtFree(arrTo);
	if ( arrCc ) xrtFree(arrCc);
	if ( arrBcc ) xrtFree(arrBcc);
	if ( arrAttachments ) xrtFree(arrAttachments);
	return sOut;
}

static bool xsmtp_check_message_size(const char* sData, uint32 iCaps, uint64 iSizeLimit, xsmtpresult* pRet)
{
	uint64 iLen;

	if ( !(iCaps & XSMTP_CAP_SIZE) || iSizeLimit == 0 ) {
		return TRUE;
	}
	iLen = (uint64)strlen(xsmtp_str_or_empty(sData));
	if ( iLen <= iSizeLimit ) {
		return TRUE;
	}
	xsmtp_result_errorf(pRet, "SMTP message exceeds SIZE limit (%llu > %llu)",
		(unsigned long long)iLen,
		(unsigned long long)iSizeLimit);
	return FALSE;
}

static int xsmtp_stage_from_async_state(xsmtpasyncstate iState)
{
	switch ( iState ) {
	case XSMTP_AST_WAIT_BANNER:
		return XSMTP_STAGE_BANNER;
	case XSMTP_AST_WAIT_EHLO:
	case XSMTP_AST_WAIT_EHLO_TLS:
		return XSMTP_STAGE_EHLO;
	case XSMTP_AST_WAIT_STARTTLS:
		return XSMTP_STAGE_STARTTLS;
	case XSMTP_AST_WAIT_STARTTLS_TLS:
		return XSMTP_STAGE_TLS;
	case XSMTP_AST_WAIT_AUTH:
	case XSMTP_AST_WAIT_AUTH_LOGIN_USER:
	case XSMTP_AST_WAIT_AUTH_LOGIN_PASS:
		return XSMTP_STAGE_AUTH;
	case XSMTP_AST_WAIT_MAIL_FROM:
		return XSMTP_STAGE_MAIL_FROM;
	case XSMTP_AST_WAIT_RCPT:
		return XSMTP_STAGE_RCPT;
	case XSMTP_AST_WAIT_DATA:
	case XSMTP_AST_WAIT_BODY:
		return XSMTP_STAGE_DATA;
	case XSMTP_AST_WAIT_QUIT:
		return XSMTP_STAGE_QUIT;
	default:
		return XSMTP_STAGE_NONE;
	}
}

static int xsmtp_secure_mode_auto(const xsmtpconfig* pCfg)
{
	if ( pCfg == NULL ) {
		return XSMTP_SECURE_SSL;
	}
	if ( pCfg->iSecureMode != XSMTP_SECURE_AUTO ) {
		return pCfg->iSecureMode;
	}
	if ( pCfg->iPort == 465 ) {
		return XSMTP_SECURE_SSL;
	}
	if ( pCfg->iPort == 587 ) {
		return XSMTP_SECURE_STARTTLS;
	}
	return XSMTP_SECURE_NONE;
}

static int xsmtp_auth_mode_auto(const xsmtpconfig* pCfg, uint32 iCaps)
{
	if ( pCfg == NULL ) {
		return XSMTP_AUTH_NONE;
	}
	if ( !pCfg->bAuth || pCfg->sUser == NULL || pCfg->sUser[0] == '\0' ) {
		return XSMTP_AUTH_NONE;
	}
	if ( pCfg->iAuthMode != XSMTP_AUTH_AUTO ) {
		return pCfg->iAuthMode;
	}
	if ( (iCaps & XSMTP_CAP_AUTH_XOAUTH2) && pCfg->sOAuth2Token != NULL && pCfg->sOAuth2Token[0] != '\0' ) {
		return XSMTP_AUTH_XOAUTH2;
	}
	if ( iCaps & XSMTP_CAP_AUTH_PLAIN ) {
		return XSMTP_AUTH_PLAIN;
	}
	if ( iCaps & XSMTP_CAP_AUTH_LOGIN ) {
		return XSMTP_AUTH_LOGIN;
	}
	return XSMTP_AUTH_LOGIN;
}

static uint64 xsmtp_now_ms(void)
{
	double fNow = xrtTimer();
	if ( fNow <= 0.0 ) {
		return 0;
	}
	return (uint64)(fNow * 1000.0);
}

static void xsmtp_session_reset_state(xsmtpsession* pSess)
{
	if ( pSess == NULL ) {
		return;
	}
	pSess->bOpen = FALSE;
	pSess->bClosed = FALSE;
	pSess->bStreamTls = FALSE;
	pSess->bTlsReady = FALSE;
	pSess->bRecvOverflow = FALSE;
	pSess->iCloseReason = 0;
	pSess->iLastSysErr = 0;
	pSess->iRecvLen = 0;
	if ( pSess->tWireRecv.sData ) {
		pSess->tWireRecv.iLen = 0;
		pSess->tWireRecv.sData[0] = '\0';
	}
}

static void xsmtp_stream_signal(xsmtpsession* pSess)
{
	if ( pSess == NULL || pSess->pCond == NULL ) {
		return;
	}
	xrtCondBroadcast(pSess->pCond);
}

static void xsmtp_stream_on_open(ptr pOwner, xnetstream* pStream)
{
	xsmtpsession* pSess = (xsmtpsession*)pOwner;
	(void)pStream;
	if ( pSess == NULL || pSess->pLock == NULL ) {
		return;
	}
	xrtMutexLock(pSess->pLock);
	pSess->bOpen = TRUE;
	if ( pSess->bStreamTls ) {
		pSess->bTlsReady = TRUE;
	}
	xsmtp_stream_signal(pSess);
	xrtMutexUnlock(pSess->pLock);
}

static void xsmtp_stream_on_recv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	xsmtpsession* pSess = (xsmtpsession*)pOwner;
	char aBuf[4096];
	size_t iLeft;
	(void)pStream;

	if ( pSess == NULL || pSess->pLock == NULL || pChain == NULL ) {
		return;
	}

	xrtMutexLock(pSess->pLock);
	iLeft = xrtNetChainBytes(pChain);
	while ( iLeft > 0 ) {
		size_t iChunk = iLeft > sizeof(aBuf) ? sizeof(aBuf) : iLeft;
		size_t iRead = xrtNetChainPeek(pChain, aBuf, iChunk);
		if ( iRead == 0 ) {
			break;
		}
		if ( !xsmtp_buf_append_n(&pSess->tWireRecv, aBuf, iRead) ) {
			pSess->bRecvOverflow = TRUE;
			xsmtp_stream_signal(pSess);
			xrtMutexUnlock(pSess->pLock);
			xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
			return;
		}
		xrtNetChainConsume(pChain, iRead);
		iLeft -= iRead;
	}
	xsmtp_stream_signal(pSess);
	xrtMutexUnlock(pSess->pLock);
}

static void xsmtp_stream_on_close(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	xsmtpsession* pSess = (xsmtpsession*)pOwner;
	(void)pStream;
	if ( pSess == NULL || pSess->pLock == NULL ) {
		return;
	}
	xrtMutexLock(pSess->pLock);
	pSess->bClosed = TRUE;
	pSess->iCloseReason = (int)iReason;
	xsmtp_stream_signal(pSess);
	xrtMutexUnlock(pSess->pLock);
}

static void xsmtp_stream_on_error(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	xsmtpsession* pSess = (xsmtpsession*)pOwner;
	(void)pStream;
	if ( pSess == NULL || pSess->pLock == NULL ) {
		return;
	}
	xrtMutexLock(pSess->pLock);
	pSess->iLastSysErr = iSysErr;
	XSMTP_TRACE("stream error sys=%d", iSysErr);
	xsmtp_stream_signal(pSess);
	xrtMutexUnlock(pSess->pLock);
}

static const xnetstreamevents* xsmtp_stream_events(void)
{
	static const xnetstreamevents tEvents = {
		xsmtp_stream_on_open,
		xsmtp_stream_on_recv,
		NULL,
		xsmtp_stream_on_close,
		xsmtp_stream_on_error,
		NULL,
		NULL
	};
	return &tEvents;
}

static void xsmtp_result_errorf(xsmtpresult* pRet, const char* sFormat, ...)
{
	va_list ap;
	char sBuf[256];

	if ( pRet == NULL || sFormat == NULL ) {
		return;
	}
	va_start(ap, sFormat);
	vsnprintf(sBuf, sizeof(sBuf), sFormat, ap);
	va_end(ap);
	xsmtp_result_error(pRet, sBuf);
}

static bool xsmtp_stream_wait_connect_state(xsmtpsession* pSess, uint64 iDeadlineMs)
{
	uint32 iRemain = 0;
	int iWaitRet;

	if ( pSess == NULL || pSess->pLock == NULL || pSess->pCond == NULL ) {
		return FALSE;
	}

	for (;;) {
		if ( pSess->bOpen || pSess->bClosed || pSess->iLastSysErr != 0 || pSess->bRecvOverflow ) {
			return TRUE;
		}
		if ( iDeadlineMs == 0 ) {
			xrtCondWait(pSess->pCond, pSess->pLock);
			continue;
		}
		if ( xsmtp_now_ms() >= iDeadlineMs ) {
			return FALSE;
		}
		iRemain = (uint32)(iDeadlineMs - xsmtp_now_ms());
		if ( iRemain == 0 ) {
			return FALSE;
		}
		iWaitRet = xrtCondWaitTimeout(pSess->pCond, pSess->pLock, iRemain);
		if ( iWaitRet == XRT_WAIT_TIMEOUT ) {
			return FALSE;
		}
		if ( iWaitRet == XRT_WAIT_ERROR ) {
			return FALSE;
		}
	}
}

static bool xsmtp_stream_wait_recv_state(xsmtpsession* pSess, uint64 iDeadlineMs)
{
	uint32 iRemain = 0;
	int iWaitRet;

	if ( pSess == NULL || pSess->pLock == NULL || pSess->pCond == NULL ) {
		return FALSE;
	}

	for (;;) {
		if ( pSess->tWireRecv.iLen > 0 || pSess->bClosed || pSess->iLastSysErr != 0 || pSess->bRecvOverflow ) {
			return TRUE;
		}
		if ( iDeadlineMs == 0 ) {
			xrtCondWait(pSess->pCond, pSess->pLock);
			continue;
		}
		if ( xsmtp_now_ms() >= iDeadlineMs ) {
			return FALSE;
		}
		iRemain = (uint32)(iDeadlineMs - xsmtp_now_ms());
		if ( iRemain == 0 ) {
			return FALSE;
		}
		iWaitRet = xrtCondWaitTimeout(pSess->pCond, pSess->pLock, iRemain);
		if ( iWaitRet == XRT_WAIT_TIMEOUT ) {
			return FALSE;
		}
		if ( iWaitRet == XRT_WAIT_ERROR ) {
			return FALSE;
		}
	}
}

static bool xsmtp_stream_connect_once(xsmtpsession* pSess, const xsmtpconfig* pCfg, int iSecureMode, const char* sConnectHost, xsmtpresult* pRet)
{
	xnetengineconfig tEngineCfg;
	xnetconnectconfig tConnCfg;
	xtlsconfig tTlsCfg;
	uint64 iDeadlineMs;
	str sErr;
	const char* sErrText;

	if ( pSess == NULL || pCfg == NULL || sConnectHost == NULL || sConnectHost[0] == '\0' ) {
		xsmtp_result_error(pRet, "SMTP host is empty");
		return FALSE;
	}

	pSess->iTimeoutMs = pCfg->iTimeoutMs ? pCfg->iTimeoutMs : 15000;
	xsmtp_session_reset_state(pSess);
	xrtNetEngineConfigInit(&tEngineCfg);
	tEngineCfg.iWorkerCount = 1;
	pSess->pEngine = xrtNetEngineCreate(&tEngineCfg);
	if ( pSess->pEngine == NULL ) {
		sErr = xrtGetError();
		sErrText = (sErr && sErr[0]) ? (const char*)sErr : "";
		xsmtp_result_errorf(pRet, "SMTP engine create failed%s%s", (sErrText[0] != '\0') ? ": " : "", sErrText);
		return FALSE;
	}
	pSess->bOwnEngine = TRUE;
	if ( xrtNetEngineStart(pSess->pEngine) != XRT_NET_OK ) {
		sErr = xrtGetError();
		sErrText = (sErr && sErr[0]) ? (const char*)sErr : "";
		xsmtp_result_errorf(pRet, "SMTP engine start failed%s%s", (sErrText[0] != '\0') ? ": " : "", sErrText);
		return FALSE;
	}

	pSess->pStream = xrtNetStreamCreate(pSess->pEngine, xsmtp_stream_events(), pSess);
	if ( pSess->pStream == NULL ) {
		sErr = xrtGetError();
		sErrText = (sErr && sErr[0]) ? (const char*)sErr : "";
		xsmtp_result_errorf(pRet, "SMTP stream create failed%s%s", (sErrText[0] != '\0') ? ": " : "", sErrText);
		return FALSE;
	}

	xrtNetConnectConfigInit(&tConnCfg);
	tConnCfg.sHost = sConnectHost;
	tConnCfg.iPort = pCfg->iPort;
	tConnCfg.iConnectTimeoutMs = pSess->iTimeoutMs;
	tConnCfg.iRecvLimit = sizeof(pSess->aRecv) * 4u;
	if ( iSecureMode == XSMTP_SECURE_SSL ) {
		memset(&tTlsCfg, 0, sizeof(tTlsCfg));
		tTlsCfg.sHostName = pCfg->sHost;
		tTlsCfg.bVerifyPeer = pCfg->bVerifyPeer;
		tTlsCfg.iMaxVersion = pCfg->iTlsMaxVersion;
		tTlsCfg.sCaFile = pCfg->sCaFile;
		tConnCfg.pTlsConfig = &tTlsCfg;
		pSess->bStreamTls = TRUE;
	}
	XSMTP_TRACE("connect submit host=%s port=%u secure=%d", sConnectHost, (unsigned)pCfg->iPort, iSecureMode);
	if ( xrtNetStreamConnect(pSess->pStream, &tConnCfg) != XRT_NET_OK ) {
		sErr = xrtGetError();
		sErrText = (sErr && sErr[0]) ? (const char*)sErr : "";
		xsmtp_result_errorf(pRet, "SMTP stream connect submit failed%s%s", (sErrText[0] != '\0') ? ": " : "", sErrText);
		return FALSE;
	}

	iDeadlineMs = xsmtp_now_ms() + pSess->iTimeoutMs;
	xrtMutexLock(pSess->pLock);
	if ( !xsmtp_stream_wait_connect_state(pSess, iDeadlineMs) ) {
		xrtMutexUnlock(pSess->pLock);
		xsmtp_result_errorf(pRet, "SMTP connect timeout (%u ms)", pSess->iTimeoutMs);
		return FALSE;
	}
	if ( pSess->iLastSysErr != 0 ) {
		int iSysErr = pSess->iLastSysErr;
		xrtMutexUnlock(pSess->pLock);
		xsmtp_result_errorf(pRet, "SMTP stream connect failed (sys=%d)", iSysErr);
		return FALSE;
	}
	if ( pSess->bClosed && !pSess->bOpen ) {
		int iCloseReason = pSess->iCloseReason;
		xrtMutexUnlock(pSess->pLock);
		xsmtp_result_errorf(pRet, "SMTP stream closed during connect (reason=%d)", iCloseReason);
		return FALSE;
	}
	xrtMutexUnlock(pSess->pLock);
	XSMTP_TRACE("connect ready host=%s secure=%d streamTls=%d", sConnectHost, iSecureMode, pSess->bStreamTls ? 1 : 0);
	return TRUE;
}

static bool xsmtp_sockaddr_to_host(const struct sockaddr* pAddr, char* sHost, size_t iHostCap)
{
	if ( pAddr == NULL || sHost == NULL || iHostCap == 0 ) {
		return FALSE;
	}
	sHost[0] = '\0';
	if ( pAddr->sa_family == AF_INET ) {
		const struct sockaddr_in* pIPv4 = (const struct sockaddr_in*)pAddr;
		return inet_ntop(AF_INET, &pIPv4->sin_addr, sHost, (socklen_t)iHostCap) != NULL;
	}
	if ( pAddr->sa_family == AF_INET6 ) {
		const struct sockaddr_in6* pIPv6 = (const struct sockaddr_in6*)pAddr;
		return inet_ntop(AF_INET6, &pIPv6->sin6_addr, sHost, (socklen_t)iHostCap) != NULL;
	}
	return FALSE;
}

static bool xsmtp_stream_connect(xsmtpsession* pSess, const xsmtpconfig* pCfg, int iSecureMode, xsmtpresult* pRet)
{
	xnetaddr tAddr;
	struct addrinfo tHints;
	struct addrinfo* pRes = NULL;
	int iPass;

	if ( pSess == NULL || pCfg == NULL || pCfg->sHost == NULL || pCfg->sHost[0] == '\0' ) {
		xsmtp_result_error(pRet, "SMTP host is empty");
		return FALSE;
	}
	(void)xrtInit();

	pSess->pLock = xrtMutexCreate();
	pSess->pCond = xrtCondCreate();
	if ( pSess->pLock == NULL || pSess->pCond == NULL ) {
		xsmtp_result_error(pRet, "SMTP session sync init failed");
		return FALSE;
	}

	memset(&tAddr, 0, sizeof(tAddr));
	tAddr.iPort = pCfg->iPort;
	if ( xrtNetAddrParse(&tAddr, pCfg->sHost, pCfg->iPort) == XRT_NET_OK ) {
		return xsmtp_stream_connect_once(pSess, pCfg, iSecureMode, pCfg->sHost, pRet);
	}

	memset(&tHints, 0, sizeof(tHints));
	tHints.ai_family = AF_UNSPEC;
	tHints.ai_socktype = SOCK_STREAM;
	if ( getaddrinfo(pCfg->sHost, NULL, &tHints, &pRes) != 0 || pRes == NULL ) {
		xsmtp_result_error(pRet, "SMTP host resolve failed");
		return FALSE;
	}

	for ( iPass = 0; iPass < 2; ++iPass ) {
		struct addrinfo* pIt;
		int iFamilyWant = (iPass == 0) ? AF_INET : AF_INET6;

		for ( pIt = pRes; pIt != NULL; pIt = pIt->ai_next ) {
			char sHost[INET6_ADDRSTRLEN + 1];

			if ( pIt->ai_addr == NULL || pIt->ai_family != iFamilyWant ) {
				continue;
			}
			if ( !xsmtp_sockaddr_to_host(pIt->ai_addr, sHost, sizeof(sHost)) ) {
				continue;
			}
			if ( xsmtp_stream_connect_once(pSess, pCfg, iSecureMode, sHost, pRet) ) {
				freeaddrinfo(pRes);
				return TRUE;
			}
			if ( pSess->pStream ) {
				xsmtp_stream_close_destroy(&pSess->pStream, pSess->iTimeoutMs);
			}
			if ( pSess->pEngine ) {
				xrtNetEngineDestroy(pSess->pEngine);
				pSess->pEngine = NULL;
			}
			xsmtp_session_reset_state(pSess);
		}
	}

	freeaddrinfo(pRes);
	if ( pRet == NULL || pRet->sError[0] == '\0' ) {
		xsmtp_result_error(pRet, "SMTP stream connect failed");
	}
	return FALSE;
}

static void xsmtp_session_close(xsmtpsession* pSess)
{
	if ( pSess == NULL ) {
		return;
	}
	if ( pSess->pTls ) {
		xrtNetTlsSessionDestroy(pSess->pTls);
		pSess->pTls = NULL;
	}
	if ( pSess->pStream ) {
		xsmtp_stream_close_destroy(&pSess->pStream, pSess->iTimeoutMs);
	}
	if ( pSess->pEngine ) {
		if ( pSess->bOwnEngine ) {
			xrtNetEngineDestroy(pSess->pEngine);
		}
		pSess->pEngine = NULL;
	}
	xsmtp_buf_unit(&pSess->tWireRecv);
	if ( pSess->pCond ) {
		xrtCondDestroy(pSess->pCond);
		pSess->pCond = NULL;
	}
	if ( pSess->pLock ) {
		xrtMutexDestroy(pSess->pLock);
		pSess->pLock = NULL;
	}
	pSess->bOpen = FALSE;
	pSess->bClosed = FALSE;
	pSess->bStreamTls = FALSE;
	pSess->bTlsReady = FALSE;
	pSess->bRecvOverflow = FALSE;
	pSess->iCloseReason = 0;
	pSess->iLastSysErr = 0;
	pSess->iRecvLen = 0;
	pSess->bOwnEngine = FALSE;
}

static bool xsmtp_stream_send_all(xsmtpsession* pSess, const void* pData, size_t iLen, xsmtpresult* pRet)
{
	xnet_result iWaitRet;
	str sErr;
	const char* sErrText;

	if ( pSess == NULL || pSess->pStream == NULL ) {
		xsmtp_result_error(pRet, "SMTP stream missing");
		return FALSE;
	}
	if ( iLen == 0 ) {
		return TRUE;
	}
	if ( xrtNetStreamSend(pSess->pStream, pData, iLen) != XRT_NET_OK ) {
		sErr = xrtGetError();
		sErrText = (sErr && sErr[0]) ? (const char*)sErr : "";
		xsmtp_result_errorf(pRet, "SMTP stream send failed%s%s", (sErrText[0] != '\0') ? ": " : "", sErrText);
		return FALSE;
	}
	iWaitRet = xrtNetStreamWaitTimeoutEx(pSess->pStream, XNET_STREAM_WAIT_DRAIN, pSess->iTimeoutMs);
	if ( iWaitRet != XRT_NET_OK ) {
		if ( xrtNetStreamPendingSend(pSess->pStream) == 0u ) {
			return TRUE;
		}
		if ( pSess->iLastSysErr != 0 ) {
			xsmtp_result_errorf(pRet, "SMTP stream drain failed (sys=%d)", pSess->iLastSysErr);
		} else {
			sErr = xrtGetError();
			sErrText = (sErr && sErr[0]) ? (const char*)sErr : "";
			xsmtp_result_errorf(pRet, "SMTP stream drain failed%s%s", (sErrText[0] != '\0') ? ": " : "", sErrText);
		}
		return FALSE;
	}
	return TRUE;
}

static bool xsmtp_stream_recv_some(xsmtpsession* pSess, void* pBuf, size_t iCap, size_t* pRead, xsmtpresult* pRet)
{
	uint64 iDeadlineMs;
	size_t iCopy = 0;

	if ( pRead ) {
		*pRead = 0;
	}
	if ( pSess == NULL || pBuf == NULL || iCap == 0 ) {
		xsmtp_result_error(pRet, "SMTP recv buffer invalid");
		return FALSE;
	}

	iDeadlineMs = xsmtp_now_ms() + pSess->iTimeoutMs;
	xrtMutexLock(pSess->pLock);
	for (;;) {
		if ( pSess->tWireRecv.iLen > 0 ) {
			break;
		}
		if ( pSess->bRecvOverflow ) {
			xrtMutexUnlock(pSess->pLock);
			xsmtp_result_error(pRet, "SMTP recv buffer overflow");
			return FALSE;
		}
		if ( pSess->iLastSysErr != 0 ) {
			int iSysErr = pSess->iLastSysErr;
			xrtMutexUnlock(pSess->pLock);
			xsmtp_result_errorf(pRet, "SMTP recv failed (sys=%d)", iSysErr);
			return FALSE;
		}
		if ( pSess->bClosed ) {
			int iCloseReason = pSess->iCloseReason;
			xrtMutexUnlock(pSess->pLock);
			xsmtp_result_errorf(pRet, "SMTP stream closed (reason=%d)", iCloseReason);
			return FALSE;
		}
		if ( !xsmtp_stream_wait_recv_state(pSess, iDeadlineMs) ) {
			xrtMutexUnlock(pSess->pLock);
			xsmtp_result_errorf(pRet, "SMTP recv timeout (%u ms)", pSess->iTimeoutMs);
			return FALSE;
		}
	}

	iCopy = pSess->tWireRecv.iLen > iCap ? iCap : pSess->tWireRecv.iLen;
	memcpy(pBuf, pSess->tWireRecv.sData, iCopy);
	if ( iCopy < pSess->tWireRecv.iLen ) {
		memmove(pSess->tWireRecv.sData, pSess->tWireRecv.sData + iCopy, pSess->tWireRecv.iLen - iCopy);
	}
	pSess->tWireRecv.iLen -= iCopy;
	if ( pSess->tWireRecv.sData ) {
		pSess->tWireRecv.sData[pSess->tWireRecv.iLen] = '\0';
	}
	xrtMutexUnlock(pSess->pLock);

	if ( pRead ) {
		*pRead = iCopy;
	}
	return TRUE;
}

static bool xsmtp_tls_flush(xsmtpsession* pSess, xsmtpresult* pRet)
{
	char aBuf[4096];
	size_t iRead = 0;

	if ( pSess == NULL || pSess->pTls == NULL ) {
		return TRUE;
	}

	while ( xrtNetTlsSessionPendingCipher(pSess->pTls) > 0 ) {
		if ( xrtNetTlsSessionPeekCipher(pSess->pTls, aBuf, sizeof(aBuf), &iRead) != XRT_NET_OK || iRead == 0 ) {
			xsmtp_result_error(pRet, "SMTP TLS cipher peek failed");
			return FALSE;
		}
		if ( !xsmtp_stream_send_all(pSess, aBuf, iRead, pRet) ) {
			return FALSE;
		}
		xrtNetTlsSessionConsumeCipher(pSess->pTls, iRead);
	}
	return TRUE;
}

static bool xsmtp_tls_handshake(xsmtpsession* pSess, const xsmtpconfig* pCfg, xsmtpresult* pRet)
{
	xtlsconfig tTLS;
	char aBuf[4096];
	size_t iRead = 0;

	if ( pSess == NULL || pCfg == NULL ) {
		xsmtp_result_error(pRet, "SMTP TLS config invalid");
		return FALSE;
	}

	memset(&tTLS, 0, sizeof(tTLS));
	tTLS.sHostName = pCfg->sHost;
	tTLS.bVerifyPeer = pCfg->bVerifyPeer;
	tTLS.iMaxVersion = pCfg->iTlsMaxVersion;
	tTLS.sCaFile = pCfg->sCaFile;
	pSess->pTls = xrtNetTlsSessionCreate(&tTLS, FALSE);
	if ( pSess->pTls == NULL ) {
		xsmtp_result_error(pRet, "SMTP TLS session create failed");
		return FALSE;
	}

	while ( !xrtNetTlsSessionIsReady(pSess->pTls) ) {
		xnet_result iResult = xrtNetTlsSessionDriveHandshake(pSess->pTls);
		if ( iResult != XRT_NET_OK && iResult != XRT_NET_AGAIN ) {
			xsmtp_result_tls_error(pRet, "SMTP TLS handshake failed", iResult);
			return FALSE;
		}
		if ( !xsmtp_tls_flush(pSess, pRet) ) {
			return FALSE;
		}
		if ( xrtNetTlsSessionIsReady(pSess->pTls) ) {
			break;
		}
		if ( !xsmtp_stream_recv_some(pSess, aBuf, sizeof(aBuf), &iRead, pRet) ) {
			return FALSE;
		}
		iResult = xrtNetTlsSessionFeedCipher(pSess->pTls, aBuf, iRead);
		if ( iResult != XRT_NET_OK ) {
			xsmtp_result_tls_error(pRet, "SMTP TLS handshake feed failed", iResult);
			return FALSE;
		}
	}

	pSess->bTlsReady = TRUE;
	if ( pRet ) {
		pRet->bUsedTLS = TRUE;
	}
	return TRUE;
}

static bool xsmtp_send_bytes(xsmtpsession* pSess, const void* pData, size_t iLen, xsmtpresult* pRet)
{
	const char* p = (const char*)pData;

	if ( pSess == NULL ) {
		xsmtp_result_error(pRet, "SMTP session missing");
		return FALSE;
	}
	if ( pSess->pTls == NULL ) {
		return xsmtp_stream_send_all(pSess, pData, iLen, pRet);
	}

	while ( iLen > 0 ) {
		size_t iWritten = 0;
		xnet_result iResult = xrtNetTlsSessionWritePlain(pSess->pTls, p, iLen, &iWritten);
		if ( iResult != XRT_NET_OK && iResult != XRT_NET_AGAIN ) {
			xsmtp_result_tls_error(pRet, "SMTP TLS write failed", iResult);
			return FALSE;
		}
		if ( !xsmtp_tls_flush(pSess, pRet) ) {
			return FALSE;
		}
		if ( iWritten == 0 && iLen > 0 ) {
			xsmtp_result_error(pRet, "SMTP TLS write stalled");
			return FALSE;
		}
		p += iWritten;
		iLen -= iWritten;
	}
	return TRUE;
}

static bool xsmtp_append_recv(xsmtpsession* pSess, const char* pData, size_t iLen, xsmtpresult* pRet)
{
	if ( pSess == NULL || pData == NULL ) {
		return FALSE;
	}
	if ( pSess->iRecvLen + iLen >= sizeof(pSess->aRecv) ) {
		xsmtp_result_error(pRet, "SMTP recv buffer overflow");
		return FALSE;
	}
	memcpy(pSess->aRecv + pSess->iRecvLen, pData, iLen);
	pSess->iRecvLen += iLen;
	pSess->aRecv[pSess->iRecvLen] = '\0';
	return TRUE;
}

static bool xsmtp_pump_recv(xsmtpsession* pSess, xsmtpresult* pRet)
{
	char aBuf[4096];
	size_t iRead = 0;

	if ( pSess == NULL ) {
		return FALSE;
	}

	if ( pSess->pTls == NULL ) {
		if ( !xsmtp_stream_recv_some(pSess, aBuf, sizeof(aBuf), &iRead, pRet) ) {
			return FALSE;
		}
		return xsmtp_append_recv(pSess, aBuf, iRead, pRet);
	}

	for (;;) {
		size_t iPlain = 0;
		xnet_result iRes = xrtNetTlsSessionReadPlain(pSess->pTls, aBuf, sizeof(aBuf), &iPlain);
		if ( iRes == XRT_NET_AGAIN ) {
			xnet_result iFeed;
			if ( pSess->iRecvLen > 0 ) {
				break;
			}
			if ( !xsmtp_stream_recv_some(pSess, aBuf, sizeof(aBuf), &iRead, pRet) ) {
				return FALSE;
			}
			iFeed = xrtNetTlsSessionFeedCipher(pSess->pTls, aBuf, iRead);
			if ( iFeed != XRT_NET_OK ) {
				xsmtp_result_tls_error(pRet, "SMTP TLS feed failed", iFeed);
				return FALSE;
			}
			if ( !xsmtp_tls_flush(pSess, pRet) ) {
				return FALSE;
			}
			continue;
		}
		if ( iRes == XRT_NET_CLOSED ) {
			xsmtp_result_tls_error(pRet, "SMTP TLS stream closed", iRes);
			return FALSE;
		}
		if ( iRes != XRT_NET_OK ) {
			xsmtp_result_tls_error(pRet, "SMTP TLS read plain failed", iRes);
			return FALSE;
		}
		if ( iPlain == 0 ) {
			break;
		}
		if ( !xsmtp_append_recv(pSess, aBuf, iPlain, pRet) ) {
			return FALSE;
		}
	}
	return TRUE;
}

static bool xsmtp_read_line(xsmtpsession* pSess, char* sOut, size_t iOutCap, xsmtpresult* pRet)
{
	size_t i;

	if ( sOut == NULL || iOutCap == 0 ) {
		xsmtp_result_error(pRet, "SMTP output buffer invalid");
		return FALSE;
	}

	for (;;) {
		for ( i = 0; i + 1 < pSess->iRecvLen; i++ ) {
			if ( pSess->aRecv[i] == '\r' && pSess->aRecv[i + 1] == '\n' ) {
				size_t iCopy = i;
				if ( iCopy >= iOutCap ) {
					iCopy = iOutCap - 1;
				}
				memcpy(sOut, pSess->aRecv, iCopy);
				sOut[iCopy] = '\0';
				memmove(pSess->aRecv, pSess->aRecv + i + 2, pSess->iRecvLen - (i + 2));
				pSess->iRecvLen -= (i + 2);
				pSess->aRecv[pSess->iRecvLen] = '\0';
				return TRUE;
			}
		}

		if ( !xsmtp_pump_recv(pSess, pRet) ) {
			return FALSE;
		}
	}
}

static bool xsmtp_read_reply(xsmtpsession* pSess, xsmtpresult* pRet, int* pCode, uint32* pCaps)
{
	char sLine[1024];
	int iReplyCode = 0;
	bool bFirst = TRUE;
	xsmtpbuf tReply;

	memset(&tReply, 0, sizeof(tReply));
	if ( pCaps ) {
		*pCaps = 0;
	}
	if ( pCaps && pRet ) {
		pRet->iSizeLimit = 0;
	}

	for (;;) {
		if ( !xsmtp_read_line(pSess, sLine, sizeof(sLine), pRet) ) {
			xsmtp_buf_unit(&tReply);
			return FALSE;
		}

		if ( bFirst ) {
			if ( strlen(sLine) < 3 || !isdigit((unsigned char)sLine[0]) || !isdigit((unsigned char)sLine[1]) || !isdigit((unsigned char)sLine[2]) ) {
				xsmtp_result_error(pRet, "SMTP invalid reply line");
				xsmtp_buf_unit(&tReply);
				return FALSE;
			}
			iReplyCode = (sLine[0] - '0') * 100 + (sLine[1] - '0') * 10 + (sLine[2] - '0');
			if ( pRet ) {
				pRet->iServerCode = iReplyCode;
			}
			bFirst = FALSE;
		}

		if ( !xsmtp_buf_append(&tReply, sLine) || !xsmtp_buf_append(&tReply, "\n") ) {
			xsmtp_result_error(pRet, "SMTP reply buffer alloc failed");
			xsmtp_buf_unit(&tReply);
			return FALSE;
		}

		if ( pCaps && strlen(sLine) > 4 && iReplyCode == 250 ) {
			xsmtp_parse_capability_line(sLine + 4, pCaps, pRet ? &pRet->iSizeLimit : NULL);
		}

		if ( strlen(sLine) < 4 || sLine[3] != '-' ) {
			break;
		}
	}

	if ( pCode ) {
		*pCode = iReplyCode;
	}
	if ( pRet && tReply.sData ) {
		snprintf(pRet->sLastReply, sizeof(pRet->sLastReply), "%s", tReply.sData);
	}
	xsmtp_buf_unit(&tReply);
	return TRUE;
}

static bool xsmtp_expect_reply(xsmtpsession* pSess, xsmtpresult* pRet, int iExpect, uint32* pCaps)
{
	int iCode = 0;
	if ( !xsmtp_read_reply(pSess, pRet, &iCode, pCaps) ) {
		return FALSE;
	}
	if ( iCode != iExpect ) {
		xsmtp_result_error(pRet, pRet ? pRet->sLastReply : "SMTP unexpected reply");
		return FALSE;
	}
	return TRUE;
}

static bool xsmtp_send_line(xsmtpsession* pSess, xsmtpresult* pRet, const char* sLine)
{
	xsmtpbuf tBuf;
	bool bOK;
	memset(&tBuf, 0, sizeof(tBuf));
	bOK = xsmtp_buf_append(&tBuf, xsmtp_str_or_empty(sLine)) && xsmtp_buf_append(&tBuf, "\r\n");
	if ( bOK ) {
		bOK = xsmtp_send_bytes(pSess, tBuf.sData, tBuf.iLen, pRet);
	}
	xsmtp_buf_unit(&tBuf);
	return bOK;
}

static bool xsmtp_send_ehlo(xsmtpsession* pSess, const xsmtpconfig* pCfg, xsmtpresult* pRet, uint32* pCaps)
{
	const char* sHelo = (pCfg && pCfg->sHeloName && pCfg->sHeloName[0]) ? pCfg->sHeloName : "localhost";
	str sLine = xrtFormat("EHLO %s", sHelo);
	bool bOK;
	if ( sLine == NULL ) {
		xsmtp_result_error(pRet, "SMTP EHLO alloc failed");
		return FALSE;
	}
	bOK = xsmtp_send_line(pSess, pRet, (const char*)sLine) && xsmtp_expect_reply(pSess, pRet, 250, pCaps);
	xrtFree(sLine);
	return bOK;
}

static bool xsmtp_send_auth_plain(xsmtpsession* pSess, const xsmtpconfig* pCfg, xsmtpresult* pRet)
{
	xsmtpbuf tBuf;
	str sBase64 = NULL;
	str sLine = NULL;
	bool bOK = FALSE;

	memset(&tBuf, 0, sizeof(tBuf));
	xsmtp_buf_append_n(&tBuf, "", 1);
	xsmtp_buf_append(&tBuf, xsmtp_str_or_empty(pCfg->sUser));
	xsmtp_buf_append_n(&tBuf, "", 1);
	xsmtp_buf_append(&tBuf, xsmtp_str_or_empty(pCfg->sPass));
	sBase64 = xrtBase64Encode(tBuf.sData, tBuf.iLen, NULL);
	if ( sBase64 == NULL ) {
		xsmtp_result_error(pRet, "SMTP AUTH PLAIN base64 failed");
		goto cleanup;
	}
	sLine = xrtFormat("AUTH PLAIN %s", sBase64);
	if ( sLine == NULL ) {
		xsmtp_result_error(pRet, "SMTP AUTH PLAIN alloc failed");
		goto cleanup;
	}
	bOK = xsmtp_send_line(pSess, pRet, (const char*)sLine) && xsmtp_expect_reply(pSess, pRet, 235, NULL);

cleanup:
	xsmtp_buf_unit(&tBuf);
	if ( sBase64 ) xrtFree(sBase64);
	if ( sLine ) xrtFree(sLine);
	return bOK;
}

static bool xsmtp_send_auth_login(xsmtpsession* pSess, const xsmtpconfig* pCfg, xsmtpresult* pRet)
{
	str sUser64 = NULL;
	str sPass64 = NULL;
	bool bOK = FALSE;

	sUser64 = xrtBase64Encode((ptr)xsmtp_str_or_empty(pCfg->sUser), strlen(xsmtp_str_or_empty(pCfg->sUser)), NULL);
	sPass64 = xrtBase64Encode((ptr)xsmtp_str_or_empty(pCfg->sPass), strlen(xsmtp_str_or_empty(pCfg->sPass)), NULL);
	if ( sUser64 == NULL || sPass64 == NULL ) {
		xsmtp_result_error(pRet, "SMTP AUTH LOGIN base64 failed");
		goto cleanup;
	}

	if ( !xsmtp_send_line(pSess, pRet, "AUTH LOGIN") || !xsmtp_expect_reply(pSess, pRet, 334, NULL) ) {
		goto cleanup;
	}
	if ( !xsmtp_send_line(pSess, pRet, (const char*)sUser64) || !xsmtp_expect_reply(pSess, pRet, 334, NULL) ) {
		goto cleanup;
	}
	if ( !xsmtp_send_line(pSess, pRet, (const char*)sPass64) || !xsmtp_expect_reply(pSess, pRet, 235, NULL) ) {
		goto cleanup;
	}

	bOK = TRUE;

cleanup:
	if ( sUser64 ) xrtFree(sUser64);
	if ( sPass64 ) xrtFree(sPass64);
	return bOK;
}

static str xsmtp_build_xoauth2_response(const char* sUser, const char* sToken)
{
	xsmtpbuf tBuf;
	static const char chSep = 0x01;
	str sBase64;

	memset(&tBuf, 0, sizeof(tBuf));
	if ( sUser == NULL || sUser[0] == '\0' || sToken == NULL || sToken[0] == '\0' ) {
		return NULL;
	}
	if ( !xsmtp_buf_append(&tBuf, "user=")
		|| !xsmtp_buf_append(&tBuf, sUser)
		|| !xsmtp_buf_append_n(&tBuf, &chSep, 1)
		|| !xsmtp_buf_append(&tBuf, "auth=Bearer ")
		|| !xsmtp_buf_append(&tBuf, sToken)
		|| !xsmtp_buf_append_n(&tBuf, &chSep, 1)
		|| !xsmtp_buf_append_n(&tBuf, &chSep, 1) ) {
		xsmtp_buf_unit(&tBuf);
		return NULL;
	}
	sBase64 = xrtBase64Encode(tBuf.sData, tBuf.iLen, NULL);
	xsmtp_buf_unit(&tBuf);
	return sBase64;
}

static bool xsmtp_send_auth_xoauth2(xsmtpsession* pSess, const xsmtpconfig* pCfg, xsmtpresult* pRet)
{
	str sBase64 = NULL;
	str sLine = NULL;
	bool bOK = FALSE;

	sBase64 = xsmtp_build_xoauth2_response(pCfg ? pCfg->sUser : NULL, pCfg ? pCfg->sOAuth2Token : NULL);
	if ( sBase64 == NULL ) {
		xsmtp_result_error(pRet, "SMTP AUTH XOAUTH2 token missing or base64 failed");
		goto cleanup;
	}
	sLine = xrtFormat("AUTH XOAUTH2 %s", sBase64);
	if ( sLine == NULL ) {
		xsmtp_result_error(pRet, "SMTP AUTH XOAUTH2 alloc failed");
		goto cleanup;
	}
	bOK = xsmtp_send_line(pSess, pRet, (const char*)sLine) && xsmtp_expect_reply(pSess, pRet, 235, NULL);

cleanup:
	if ( sBase64 ) xrtFree(sBase64);
	if ( sLine ) xrtFree(sLine);
	return bOK;
}

static bool xsmtp_send_auth(xsmtpsession* pSess, const xsmtpconfig* pCfg, uint32 iCaps, xsmtpresult* pRet)
{
	int iAuthMode;
	if ( pRet ) {
		pRet->iCapabilities = iCaps;
	}
	iAuthMode = xsmtp_auth_mode_auto(pCfg, iCaps);
	if ( pRet ) {
		pRet->iAuthMode = iAuthMode;
	}

	if ( iAuthMode == XSMTP_AUTH_NONE ) {
		return TRUE;
	}
	if ( iAuthMode == XSMTP_AUTH_PLAIN ) {
		return xsmtp_send_auth_plain(pSess, pCfg, pRet);
	}
	if ( iAuthMode == XSMTP_AUTH_LOGIN ) {
		return xsmtp_send_auth_login(pSess, pCfg, pRet);
	}
	if ( iAuthMode == XSMTP_AUTH_XOAUTH2 ) {
		if ( !(iCaps & XSMTP_CAP_AUTH_XOAUTH2) ) {
			xsmtp_result_error(pRet, "SMTP server does not support AUTH XOAUTH2");
			return FALSE;
		}
		return xsmtp_send_auth_xoauth2(pSess, pCfg, pRet);
	}

	xsmtp_result_error(pRet, "SMTP auth mode unsupported");
	return FALSE;
}

static bool xsmtp_dsn_value_is_safe(const char* sText)
{
	const unsigned char* p;

	if ( sText == NULL || sText[0] == '\0' ) {
		return FALSE;
	}
	for ( p = (const unsigned char*)sText; *p != '\0'; p++ ) {
		if ( *p <= 0x20u || *p >= 0x7Fu || *p == '<' || *p == '>' ) {
			return FALSE;
		}
	}
	return TRUE;
}

static bool xsmtp_ascii_equal_i(const char* a, const char* b)
{
	size_t i;

	if ( a == NULL || b == NULL ) {
		return FALSE;
	}
	for ( i = 0u; a[i] != '\0' || b[i] != '\0'; i++ ) {
		if ( tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]) ) {
			return FALSE;
		}
	}
	return TRUE;
}

static bool xsmtp_dsn_return_is_valid(const char* sText)
{
	return sText == NULL || sText[0] == '\0' || xsmtp_ascii_equal_i(sText, "FULL") || xsmtp_ascii_equal_i(sText, "HDRS");
}

static str xsmtp_build_mail_from_line(const xsmtpconfig* pCfg, const xsmtpmessage* pMsg, uint32 iCaps, xsmtpresult* pRet)
{
	const char* sEmail = NULL;
	xsmtpbuf tBuf;
	bool bHasDsn;
	str sLine = NULL;

	memset(&tBuf, 0, sizeof(tBuf));
	bHasDsn = pMsg != NULL && ((pMsg->sDsnReturn != NULL && pMsg->sDsnReturn[0] != '\0')
		|| (pMsg->sDsnEnvid != NULL && pMsg->sDsnEnvid[0] != '\0'));

	if ( bHasDsn && !(iCaps & XSMTP_CAP_DSN) ) {
		xsmtp_result_error(pRet, "SMTP server does not support DSN");
		return NULL;
	}
	if ( pMsg != NULL && !xsmtp_dsn_return_is_valid(pMsg->sDsnReturn) ) {
		xsmtp_result_error(pRet, "SMTP DSN RET value invalid");
		return NULL;
	}
	if ( pMsg != NULL && pMsg->sDsnEnvid != NULL && pMsg->sDsnEnvid[0] != '\0' && !xsmtp_dsn_value_is_safe(pMsg->sDsnEnvid) ) {
		xsmtp_result_error(pRet, "SMTP DSN ENVID value invalid");
		return NULL;
	}
	if ( pMsg && pMsg->tFrom.sEmail && pMsg->tFrom.sEmail[0] ) {
		sEmail = pMsg->tFrom.sEmail;
	} else if ( pCfg && pCfg->tFrom.sEmail && pCfg->tFrom.sEmail[0] ) {
		sEmail = pCfg->tFrom.sEmail;
	}
	if ( sEmail == NULL || sEmail[0] == '\0' ) {
		xsmtp_result_error(pRet, "SMTP from email is empty");
		return NULL;
	}
	if ( !xsmtp_buf_append(&tBuf, "MAIL FROM:<") || !xsmtp_buf_append(&tBuf, sEmail) || !xsmtp_buf_append(&tBuf, ">") ) {
		xsmtp_buf_unit(&tBuf);
		xsmtp_result_error(pRet, "SMTP MAIL FROM alloc failed");
		return NULL;
	}
	if ( pMsg != NULL && pMsg->sDsnReturn != NULL && pMsg->sDsnReturn[0] != '\0' ) {
		if ( !xsmtp_buf_append(&tBuf, " RET=") || !xsmtp_buf_append(&tBuf, pMsg->sDsnReturn) ) {
			xsmtp_buf_unit(&tBuf);
			xsmtp_result_error(pRet, "SMTP MAIL FROM alloc failed");
			return NULL;
		}
	}
	if ( pMsg != NULL && pMsg->sDsnEnvid != NULL && pMsg->sDsnEnvid[0] != '\0' ) {
		if ( !xsmtp_buf_append(&tBuf, " ENVID=") || !xsmtp_buf_append(&tBuf, pMsg->sDsnEnvid) ) {
			xsmtp_buf_unit(&tBuf);
			xsmtp_result_error(pRet, "SMTP MAIL FROM alloc failed");
			return NULL;
		}
	}
	sLine = (str)tBuf.sData;
	memset(&tBuf, 0, sizeof(tBuf));
	return sLine;
}

static str xsmtp_build_rcpt_to_line(const xsmtpaddr* pAddr, uint32 iCaps, xsmtpresult* pRet)
{
	xsmtpbuf tBuf;
	bool bHasDsn;

	memset(&tBuf, 0, sizeof(tBuf));
	if ( pAddr == NULL || pAddr->sEmail == NULL || pAddr->sEmail[0] == '\0' ) {
		return NULL;
	}
	bHasDsn = (pAddr->sDsnNotify != NULL && pAddr->sDsnNotify[0] != '\0')
		|| (pAddr->sDsnOrcpt != NULL && pAddr->sDsnOrcpt[0] != '\0');
	if ( bHasDsn && !(iCaps & XSMTP_CAP_DSN) ) {
		xsmtp_result_error(pRet, "SMTP server does not support DSN");
		return NULL;
	}
	if ( pAddr->sDsnNotify != NULL && pAddr->sDsnNotify[0] != '\0' && !xsmtp_dsn_value_is_safe(pAddr->sDsnNotify) ) {
		xsmtp_result_error(pRet, "SMTP DSN NOTIFY value invalid");
		return NULL;
	}
	if ( pAddr->sDsnOrcpt != NULL && pAddr->sDsnOrcpt[0] != '\0' && !xsmtp_dsn_value_is_safe(pAddr->sDsnOrcpt) ) {
		xsmtp_result_error(pRet, "SMTP DSN ORCPT value invalid");
		return NULL;
	}
	if ( !xsmtp_buf_append(&tBuf, "RCPT TO:<") || !xsmtp_buf_append(&tBuf, pAddr->sEmail) || !xsmtp_buf_append(&tBuf, ">") ) {
		xsmtp_buf_unit(&tBuf);
		xsmtp_result_error(pRet, "SMTP RCPT TO alloc failed");
		return NULL;
	}
	if ( pAddr->sDsnNotify != NULL && pAddr->sDsnNotify[0] != '\0' ) {
		if ( !xsmtp_buf_append(&tBuf, " NOTIFY=") || !xsmtp_buf_append(&tBuf, pAddr->sDsnNotify) ) {
			xsmtp_buf_unit(&tBuf);
			xsmtp_result_error(pRet, "SMTP RCPT TO alloc failed");
			return NULL;
		}
	}
	if ( pAddr->sDsnOrcpt != NULL && pAddr->sDsnOrcpt[0] != '\0' ) {
		if ( !xsmtp_buf_append(&tBuf, " ORCPT=") || !xsmtp_buf_append(&tBuf, pAddr->sDsnOrcpt) ) {
			xsmtp_buf_unit(&tBuf);
			xsmtp_result_error(pRet, "SMTP RCPT TO alloc failed");
			return NULL;
		}
	}
	return (str)tBuf.sData;
}

static bool xsmtp_send_mail_from(xsmtpsession* pSess, const xsmtpconfig* pCfg, const xsmtpmessage* pMsg, uint32 iCaps, xsmtpresult* pRet)
{
	str sLine = NULL;
	bool bOK;

	sLine = xsmtp_build_mail_from_line(pCfg, pMsg, iCaps, pRet);
	if ( sLine == NULL ) {
		return FALSE;
	}
	bOK = xsmtp_send_line(pSess, pRet, (const char*)sLine) && xsmtp_expect_reply(pSess, pRet, 250, NULL);
	xrtFree(sLine);
	return bOK;
}

static bool xsmtp_send_rcpt_list(xsmtpsession* pSess, const xsmtpaddr* arrList, size_t iCount, uint32 iCaps, xsmtpresult* pRet)
{
	size_t i;
	for ( i = 0; i < iCount; i++ ) {
		str sLine;
		if ( arrList[i].sEmail == NULL || arrList[i].sEmail[0] == '\0' ) {
			continue;
		}
		sLine = xsmtp_build_rcpt_to_line(&arrList[i], iCaps, pRet);
		if ( sLine == NULL ) {
			return FALSE;
		}
		if ( !xsmtp_send_line(pSess, pRet, (const char*)sLine) || !xsmtp_expect_reply(pSess, pRet, 250, NULL) ) {
			xrtFree(sLine);
			return FALSE;
		}
		xrtFree(sLine);
	}
	return TRUE;
}

static bool xsmtp_send_data_body(xsmtpsession* pSess, const char* sData, xsmtpresult* pRet)
{
	xsmtpbuf tBuf;
	const char* p;

	memset(&tBuf, 0, sizeof(tBuf));
	if ( !xsmtp_buf_reserve(&tBuf, strlen(xsmtp_str_or_empty(sData)) + 16) ) {
		xsmtp_result_error(pRet, "SMTP DATA buffer alloc failed");
		return FALSE;
	}
	for ( p = xsmtp_str_or_empty(sData); *p; ) {
		const char* sLineEnd = strstr(p, "\r\n");
		size_t iLineLen;
		if ( sLineEnd ) {
			iLineLen = (size_t)(sLineEnd - p);
		} else {
			iLineLen = strlen(p);
		}
		if ( iLineLen > 0 && p[0] == '.' ) {
			xsmtp_buf_append(&tBuf, ".");
		}
		xsmtp_buf_append_n(&tBuf, p, iLineLen);
		xsmtp_buf_append(&tBuf, "\r\n");
		if ( !sLineEnd ) {
			break;
		}
		p = sLineEnd + 2;
	}
	xsmtp_buf_append(&tBuf, ".\r\n");

	if ( !xsmtp_send_bytes(pSess, tBuf.sData, tBuf.iLen, pRet) ) {
		xsmtp_buf_unit(&tBuf);
		return FALSE;
	}
	xsmtp_buf_unit(&tBuf);
	return xsmtp_expect_reply(pSess, pRet, 250, NULL);
}

static bool xrtSmtpSendMail(const xsmtpconfig* pCfg, const xsmtpmessage* pMsg, xsmtpresult* pRet)
{
	xsmtpsession tSess;
	uint32 iCaps = 0;
	int iSecureMode;
	str sData = NULL;
	size_t iRecipientCount = 0;
	bool bOK = FALSE;

	if ( pRet ) {
		xrtSmtpResultInit(pRet);
	}
	memset(&tSess, 0, sizeof(tSess));

	if ( pCfg == NULL || pMsg == NULL ) {
		xsmtp_result_error(pRet, "SMTP config or message missing");
		goto cleanup;
	}
	if ( !xsmtp_count_recipients(pMsg, &iRecipientCount) ) {
		xsmtp_result_error(pRet, "SMTP recipients are empty");
		goto cleanup;
	}

	xsmtp_result_stage(pRet, XSMTP_STAGE_CONNECT);
	iSecureMode = xsmtp_secure_mode_auto(pCfg);
	XSMTP_TRACE("connect host=%s port=%u secure=%d", pCfg->sHost ? pCfg->sHost : "", (unsigned)pCfg->iPort, iSecureMode);
	if ( !xsmtp_stream_connect(&tSess, pCfg, iSecureMode, pRet) ) {
		goto cleanup;
	}
	XSMTP_TRACE("connect ok");

	if ( iSecureMode == XSMTP_SECURE_SSL ) {
		XSMTP_TRACE("implicit tls ready via xrt stream");
		if ( pRet ) {
			pRet->bUsedTLS = TRUE;
		}
	}

	xsmtp_result_stage(pRet, XSMTP_STAGE_BANNER);
	XSMTP_TRACE("wait banner");
	if ( !xsmtp_expect_reply(&tSess, pRet, 220, NULL) ) {
		goto cleanup;
	}
	XSMTP_TRACE("banner ok");

	xsmtp_result_stage(pRet, XSMTP_STAGE_EHLO);
	XSMTP_TRACE("ehlo begin");
	if ( !xsmtp_send_ehlo(&tSess, pCfg, pRet, &iCaps) ) {
		goto cleanup;
	}
	XSMTP_TRACE("ehlo ok caps=0x%08x", (unsigned)iCaps);

	if ( iSecureMode == XSMTP_SECURE_STARTTLS ) {
		xsmtp_result_stage(pRet, XSMTP_STAGE_STARTTLS);
		XSMTP_TRACE("starttls begin");
		if ( !(iCaps & XSMTP_CAP_STARTTLS) ) {
			xsmtp_result_error(pRet, "SMTP server does not support STARTTLS");
			goto cleanup;
		}
		if ( !xsmtp_send_line(&tSess, pRet, "STARTTLS") || !xsmtp_expect_reply(&tSess, pRet, 220, NULL) ) {
			goto cleanup;
		}
		xsmtp_result_stage(pRet, XSMTP_STAGE_TLS);
		if ( !xsmtp_tls_handshake(&tSess, pCfg, pRet) ) {
			goto cleanup;
		}
		XSMTP_TRACE("starttls handshake ok");
		if ( pRet ) {
			pRet->bUsedStartTLS = TRUE;
		}
		iCaps = 0;
		xsmtp_result_stage(pRet, XSMTP_STAGE_EHLO);
		if ( !xsmtp_send_ehlo(&tSess, pCfg, pRet, &iCaps) ) {
			goto cleanup;
		}
		XSMTP_TRACE("post-starttls ehlo ok caps=0x%08x", (unsigned)iCaps);
	}

	xsmtp_result_stage(pRet, XSMTP_STAGE_AUTH);
	XSMTP_TRACE("auth begin");
	if ( !xsmtp_send_auth(&tSess, pCfg, iCaps, pRet) ) {
		goto cleanup;
	}
	XSMTP_TRACE("auth ok");
	xsmtp_result_stage(pRet, XSMTP_STAGE_MAIL_FROM);
	if ( !xsmtp_send_mail_from(&tSess, pCfg, pMsg, iCaps, pRet) ) {
		goto cleanup;
	}
	XSMTP_TRACE("mail from ok");
	xsmtp_result_stage(pRet, XSMTP_STAGE_RCPT);
	if ( !xsmtp_send_rcpt_list(&tSess, pMsg->arrTo, pMsg->iToCount, iCaps, pRet) ) {
		goto cleanup;
	}
	if ( !xsmtp_send_rcpt_list(&tSess, pMsg->arrCc, pMsg->iCcCount, iCaps, pRet) ) {
		goto cleanup;
	}
	if ( !xsmtp_send_rcpt_list(&tSess, pMsg->arrBcc, pMsg->iBccCount, iCaps, pRet) ) {
		goto cleanup;
	}
	xsmtp_result_stage(pRet, XSMTP_STAGE_DATA);
	if ( !xsmtp_send_line(&tSess, pRet, "DATA") || !xsmtp_expect_reply(&tSess, pRet, 354, NULL) ) {
		goto cleanup;
	}
	XSMTP_TRACE("data begin");

	sData = xsmtp_build_message_data(pCfg, pMsg);
	if ( sData == NULL ) {
		xsmtp_result_error(pRet, "SMTP build message failed");
		goto cleanup;
	}
	if ( !xsmtp_check_message_size((const char*)sData, iCaps, pRet ? pRet->iSizeLimit : 0, pRet) ) {
		goto cleanup;
	}
	if ( !xsmtp_send_data_body(&tSess, (const char*)sData, pRet) ) {
		goto cleanup;
	}
	XSMTP_TRACE("data ok");

	xsmtp_result_stage(pRet, XSMTP_STAGE_QUIT);
	xsmtp_send_line(&tSess, pRet, "QUIT");
	xsmtp_expect_reply(&tSess, pRet, 221, NULL);
	XSMTP_TRACE("quit done");

	bOK = TRUE;
	if ( pRet ) {
		pRet->bSuccess = TRUE;
	}

cleanup:
	if ( sData ) {
		xrtFree(sData);
	}
	xsmtp_session_close(&tSess);
	return bOK;
}

static bool xrtSmtpCapability(const xsmtpconfig* pCfg, xsmtpresult* pRet)
{
	xsmtpsession tSess;
	uint32 iCaps = 0;
	int iSecureMode;
	bool bOK = FALSE;

	if ( pRet ) {
		xrtSmtpResultInit(pRet);
	}
	memset(&tSess, 0, sizeof(tSess));
	if ( pCfg == NULL ) {
		xsmtp_result_error(pRet, "SMTP config missing");
		goto cleanup;
	}

	xsmtp_result_stage(pRet, XSMTP_STAGE_CONNECT);
	iSecureMode = xsmtp_secure_mode_auto(pCfg);
	if ( !xsmtp_stream_connect(&tSess, pCfg, iSecureMode, pRet) ) {
		goto cleanup;
	}
	if ( iSecureMode == XSMTP_SECURE_SSL && pRet != NULL ) {
		pRet->bUsedTLS = TRUE;
	}
	xsmtp_result_stage(pRet, XSMTP_STAGE_BANNER);
	if ( !xsmtp_expect_reply(&tSess, pRet, 220, NULL) ) {
		goto cleanup;
	}
	xsmtp_result_stage(pRet, XSMTP_STAGE_EHLO);
	if ( !xsmtp_send_ehlo(&tSess, pCfg, pRet, &iCaps) ) {
		goto cleanup;
	}
	if ( iSecureMode == XSMTP_SECURE_STARTTLS ) {
		xsmtp_result_stage(pRet, XSMTP_STAGE_STARTTLS);
		if ( !(iCaps & XSMTP_CAP_STARTTLS) ) {
			xsmtp_result_error(pRet, "SMTP server does not support STARTTLS");
			goto cleanup;
		}
		if ( !xsmtp_send_line(&tSess, pRet, "STARTTLS") || !xsmtp_expect_reply(&tSess, pRet, 220, NULL) ) {
			goto cleanup;
		}
		xsmtp_result_stage(pRet, XSMTP_STAGE_TLS);
		if ( !xsmtp_tls_handshake(&tSess, pCfg, pRet) ) {
			goto cleanup;
		}
		if ( pRet != NULL ) {
			pRet->bUsedStartTLS = TRUE;
		}
		iCaps = 0;
		xsmtp_result_stage(pRet, XSMTP_STAGE_EHLO);
		if ( !xsmtp_send_ehlo(&tSess, pCfg, pRet, &iCaps) ) {
			goto cleanup;
		}
	}
	if ( pRet != NULL ) {
		pRet->iCapabilities = iCaps;
	}
	xsmtp_result_stage(pRet, XSMTP_STAGE_QUIT);
	(void)xsmtp_send_line(&tSess, pRet, "QUIT");
	(void)xsmtp_expect_reply(&tSess, pRet, 221, NULL);
	bOK = TRUE;
	if ( pRet != NULL ) {
		pRet->bSuccess = TRUE;
		pRet->iCapabilities = iCaps;
	}

cleanup:
	xsmtp_session_close(&tSess);
	return bOK;
}

static void xsmtp_async_fail(xsmtpasyncop* pOp, const char* sError);
static void xsmtp_async_fail_tls(xsmtpasyncop* pOp, const char* sError, xnet_result iResult);
static void xsmtp_async_arm_timeout(xsmtpasyncop* pOp, const char* sReason);
static void xsmtp_async_advance(xsmtpasyncop* pOp);
static bool xsmtp_async_run_post_ehlo(xsmtpasyncop* pOp, bool bAllowStartTLS);

static bool xsmtp_async_send_stream_raw(xsmtpasyncop* pOp, const void* pData, size_t iLen)
{
	if ( pOp == NULL || pOp->pStream == NULL ) {
		return FALSE;
	}
	return xrtNetStreamSend(pOp->pStream, pData, iLen) == XRT_NET_OK;
}

static bool xsmtp_async_tls_flush_manual(xsmtpasyncop* pOp)
{
	char aBuf[4096];
	size_t iRead = 0;

	if ( pOp == NULL || pOp->pSess == NULL || pOp->pSess->pTls == NULL || pOp->pSess->bStreamTls ) {
		return TRUE;
	}

	while ( xrtNetTlsSessionPendingCipher(pOp->pSess->pTls) > 0 ) {
		if ( xrtNetTlsSessionPeekCipher(pOp->pSess->pTls, aBuf, sizeof(aBuf), &iRead) != XRT_NET_OK || iRead == 0 ) {
			xsmtp_async_fail(pOp, "SMTP TLS cipher peek failed");
			return FALSE;
		}
		if ( !xsmtp_async_send_stream_raw(pOp, aBuf, iRead) ) {
			xsmtp_async_fail(pOp, "SMTP TLS cipher send failed");
			return FALSE;
		}
		xrtNetTlsSessionConsumeCipher(pOp->pSess->pTls, iRead);
	}
	return TRUE;
}

static bool xsmtp_async_tls_drain_plain(xsmtpasyncop* pOp)
{
	char aBuf[4096];

	if ( pOp == NULL || pOp->pSess == NULL || pOp->pSess->pTls == NULL || pOp->pSess->bStreamTls ) {
		return TRUE;
	}

	while ( xrtNetTlsSessionPendingRecv(pOp->pSess->pTls) > 0 ) {
		size_t iPlain = 0;
		xnet_result iRes = xrtNetTlsSessionReadPlain(pOp->pSess->pTls, aBuf, sizeof(aBuf), &iPlain);
		if ( iRes == XRT_NET_AGAIN ) {
			break;
		}
		if ( iRes == XRT_NET_CLOSED ) {
			xsmtp_async_fail_tls(pOp, "SMTP TLS stream closed", iRes);
			return FALSE;
		}
		if ( iRes != XRT_NET_OK ) {
			xsmtp_async_fail_tls(pOp, "SMTP TLS read plain failed", iRes);
			return FALSE;
		}
		if ( iPlain == 0 ) {
			break;
		}
		if ( !xsmtp_append_recv(pOp->pSess, aBuf, iPlain, pOp->pRet) ) {
			xsmtp_async_fail(pOp, pOp->pRet ? pOp->pRet->sError : "SMTP recv append failed");
			return FALSE;
		}
	}
	return TRUE;
}

static bool xsmtp_async_tls_drive_handshake(xsmtpasyncop* pOp)
{
	xnet_result iResult;
	int iStep;

	if ( pOp == NULL || pOp->pSess == NULL || pOp->pSess->pTls == NULL || pOp->pSess->bStreamTls ) {
		return FALSE;
	}

	for ( iStep = 0; iStep < 32 && !xrtNetTlsSessionIsReady(pOp->pSess->pTls); ++iStep ) {
		uint32 iPendingBefore = (uint32)xrtNetTlsSessionPendingRecv(pOp->pSess->pTls);
		uint32 iCipherBefore = (uint32)xrtNetTlsSessionPendingCipher(pOp->pSess->pTls);

		iResult = xrtNetTlsSessionDriveHandshake(pOp->pSess->pTls);
		if ( iResult != XRT_NET_OK && iResult != XRT_NET_AGAIN ) {
			xsmtp_async_fail_tls(pOp, "SMTP TLS handshake failed", iResult);
			return FALSE;
		}
		if ( !xsmtp_async_tls_flush_manual(pOp) ) {
			return FALSE;
		}
		if ( xrtNetTlsSessionIsReady(pOp->pSess->pTls) ) {
			break;
		}
		if ( xrtNetTlsSessionPendingRecv(pOp->pSess->pTls) == 0 ) {
			return TRUE;
		}
		if ( xrtNetTlsSessionPendingRecv(pOp->pSess->pTls) == iPendingBefore
			&& xrtNetTlsSessionPendingCipher(pOp->pSess->pTls) == iCipherBefore
			&& iResult == XRT_NET_AGAIN ) {
			return TRUE;
		}
	}

	if ( !xrtNetTlsSessionIsReady(pOp->pSess->pTls) ) {
		return TRUE;
	}

	pOp->pSess->bTlsReady = TRUE;
	if ( pOp->pRet != NULL ) {
		pOp->pRet->bUsedTLS = TRUE;
		pOp->pRet->bUsedStartTLS = TRUE;
	}
	return TRUE;
}

static bool xsmtp_async_starttls_begin(xsmtpasyncop* pOp)
{
	xtlsconfig tTlsCfg;

	if ( pOp == NULL || pOp->pSess == NULL ) {
		return FALSE;
	}
	if ( pOp->pSess->pTls != NULL ) {
		return TRUE;
	}

	memset(&tTlsCfg, 0, sizeof(tTlsCfg));
	tTlsCfg.sHostName = pOp->pCtx->tCfg.sHost;
	tTlsCfg.bVerifyPeer = pOp->pCtx->tCfg.bVerifyPeer;
	tTlsCfg.iMaxVersion = pOp->pCtx->tCfg.iTlsMaxVersion;
	tTlsCfg.sCaFile = pOp->pCtx->tCfg.sCaFile;
	pOp->pSess->pTls = xrtNetTlsSessionCreate(&tTlsCfg, FALSE);
	if ( pOp->pSess->pTls == NULL ) {
		xsmtp_async_fail(pOp, "SMTP TLS session create failed");
		return FALSE;
	}
	pOp->pSess->bStreamTls = FALSE;
	pOp->pSess->bTlsReady = FALSE;
	if ( !xsmtp_async_tls_drive_handshake(pOp) ) {
		return FALSE;
	}
	return TRUE;
}

static bool xsmtp_async_send_plain(xsmtpasyncop* pOp, const void* pData, size_t iLen)
{
	const char* p = (const char*)pData;

	if ( pOp == NULL || pOp->pSess == NULL ) {
		return FALSE;
	}
	if ( iLen == 0 ) {
		return TRUE;
	}
	if ( pOp->pSess->pTls == NULL || pOp->pSess->bStreamTls ) {
		return xsmtp_async_send_stream_raw(pOp, pData, iLen);
	}
	if ( !pOp->pSess->bTlsReady ) {
		xsmtp_async_fail(pOp, "SMTP TLS not ready");
		return FALSE;
	}

	while ( iLen > 0 ) {
		size_t iWritten = 0;
		xnet_result iResult = xrtNetTlsSessionWritePlain(pOp->pSess->pTls, p, iLen, &iWritten);
		if ( iResult != XRT_NET_OK && iResult != XRT_NET_AGAIN ) {
			xsmtp_async_fail_tls(pOp, "SMTP TLS write failed", iResult);
			return FALSE;
		}
		if ( !xsmtp_async_tls_flush_manual(pOp) ) {
			return FALSE;
		}
		if ( iWritten == 0 ) {
			xsmtp_async_fail(pOp, "SMTP TLS write stalled");
			return FALSE;
		}
		p += iWritten;
		iLen -= iWritten;
	}
	return TRUE;
}

static void xsmtp_async_cleanup(xsmtpasyncop* pOp)
{
	if ( pOp == NULL ) {
		return;
	}
	if ( pOp->pStream != NULL && pOp->pSess == NULL ) {
		xrtNetStreamDestroy(pOp->pStream);
		pOp->pStream = NULL;
	}
	if ( pOp->pPromise != NULL ) {
		xPromiseDestroy(pOp->pPromise);
		pOp->pPromise = NULL;
	}
	if ( pOp->pSess != NULL ) {
		xsmtp_session_close(pOp->pSess);
		xrtFree(pOp->pSess);
		pOp->pSess = NULL;
		pOp->pStream = NULL;
		pOp->pEngine = NULL;
	}
	xsmtp_buf_unit(&pOp->tReply);
	xsmtp_buf_unit(&pOp->tScratch);
	if ( pOp->pCtx != NULL ) {
		xsmtp_task_ctx_free(pOp->pCtx);
		pOp->pCtx = NULL;
	}
	xrtFree(pOp);
}

static void xsmtp_async_resolve(xsmtpasyncop* pOp)
{
	xsmtpresult* pRet;
	xpromise* pPromise;

	if ( pOp == NULL || pOp->bDone ) {
		return;
	}
	pOp->bDone = TRUE;
	pRet = pOp->pRet;
	pPromise = pOp->pPromise;
	pOp->pRet = NULL;
	pOp->pPromise = NULL;
	if ( pPromise != NULL ) {
		(void)xPromiseResolve(pPromise, pRet);
	}
	xsmtp_async_cleanup(pOp);
}

static void xsmtp_async_fail(xsmtpasyncop* pOp, const char* sError)
{
	if ( pOp == NULL || pOp->bDone ) {
		return;
	}
	xsmtp_result_stage(pOp->pRet, xsmtp_stage_from_async_state(pOp->iState));
	xsmtp_result_error(pOp->pRet, sError);
	xsmtp_async_resolve(pOp);
}

static void xsmtp_async_fail_tls(xsmtpasyncop* pOp, const char* sError, xnet_result iResult)
{
	if ( pOp == NULL || pOp->bDone ) {
		return;
	}
	xsmtp_result_stage(pOp->pRet, xsmtp_stage_from_async_state(pOp->iState));
	xsmtp_result_tls_error(pOp->pRet, sError, iResult);
	xsmtp_async_resolve(pOp);
}

static bool xsmtp_async_send_line(xsmtpasyncop* pOp, const char* sLine)
{
	xsmtpbuf tBuf;
	xnet_result iRet;

	memset(&tBuf, 0, sizeof(tBuf));
	if ( !xsmtp_buf_append(&tBuf, xsmtp_str_or_empty(sLine)) || !xsmtp_buf_append(&tBuf, "\r\n") ) {
		xsmtp_buf_unit(&tBuf);
		xsmtp_async_fail(pOp, "SMTP async line alloc failed");
		return FALSE;
	}
	iRet = xsmtp_async_send_plain(pOp, tBuf.sData, tBuf.iLen) ? XRT_NET_OK : XRT_NET_ERROR;
	xsmtp_buf_unit(&tBuf);
	if ( iRet != XRT_NET_OK ) {
		xsmtp_async_fail(pOp, "SMTP async send failed");
		return FALSE;
	}
	return TRUE;
}

static void xsmtp_async_reply_reset(xsmtpasyncop* pOp)
{
	if ( pOp == NULL ) {
		return;
	}
	pOp->iReplyCode = 0;
	pOp->iReplyCaps = 0;
	pOp->iReplySizeLimit = 0;
	pOp->bReplyHasFirst = FALSE;
	pOp->tReply.iLen = 0;
	if ( pOp->tReply.sData != NULL ) {
		pOp->tReply.sData[0] = '\0';
	}
}

static void xsmtp_async_set_state(xsmtpasyncop* pOp, xsmtpasyncstate iState, const char* sReason)
{
	if ( pOp == NULL || pOp->bDone ) {
		return;
	}
	pOp->iState = iState;
	xsmtp_result_stage(pOp->pRet, xsmtp_stage_from_async_state(iState));
	xsmtp_async_reply_reset(pOp);
	xsmtp_async_arm_timeout(pOp, sReason);
}

static const xsmtpaddr* xsmtp_async_current_rcpt(xsmtpasyncop* pOp)
{
	if ( pOp == NULL || pOp->pCtx == NULL ) {
		return NULL;
	}
	if ( pOp->iRcptListKind == 0 ) {
		if ( pOp->iRcptIndex < pOp->pCtx->tMsg.iToCount ) {
			return &pOp->pCtx->tMsg.arrTo[pOp->iRcptIndex];
		}
	}
	else if ( pOp->iRcptListKind == 1 ) {
		if ( pOp->iRcptIndex < pOp->pCtx->tMsg.iCcCount ) {
			return &pOp->pCtx->tMsg.arrCc[pOp->iRcptIndex];
		}
	}
	else if ( pOp->iRcptListKind == 2 ) {
		if ( pOp->iRcptIndex < pOp->pCtx->tMsg.iBccCount ) {
			return &pOp->pCtx->tMsg.arrBcc[pOp->iRcptIndex];
		}
	}
	return NULL;
}

static void xsmtp_async_next_rcpt_cursor(xsmtpasyncop* pOp)
{
	if ( pOp == NULL ) {
		return;
	}
	pOp->iRcptIndex++;
	for (;;) {
		size_t iCount = 0;
		if ( pOp->iRcptListKind == 0 ) iCount = pOp->pCtx->tMsg.iToCount;
		else if ( pOp->iRcptListKind == 1 ) iCount = pOp->pCtx->tMsg.iCcCount;
		else if ( pOp->iRcptListKind == 2 ) iCount = pOp->pCtx->tMsg.iBccCount;
		if ( pOp->iRcptIndex < iCount ) {
			return;
		}
		pOp->iRcptListKind++;
		pOp->iRcptIndex = 0;
		if ( pOp->iRcptListKind > 2 ) {
			return;
		}
	}
}

static bool xsmtp_async_send_auth_plain_line(xsmtpasyncop* pOp)
{
	xsmtpbuf tBuf;
	str sBase64 = NULL;
	str sLine = NULL;
	bool bOK = FALSE;

	memset(&tBuf, 0, sizeof(tBuf));
	xsmtp_buf_append_n(&tBuf, "", 1);
	xsmtp_buf_append(&tBuf, xsmtp_str_or_empty(pOp->pCtx->tCfg.sUser));
	xsmtp_buf_append_n(&tBuf, "", 1);
	xsmtp_buf_append(&tBuf, xsmtp_str_or_empty(pOp->pCtx->tCfg.sPass));
	sBase64 = xrtBase64Encode(tBuf.sData, tBuf.iLen, NULL);
	if ( sBase64 == NULL ) {
		xsmtp_buf_unit(&tBuf);
		xsmtp_async_fail(pOp, "SMTP AUTH PLAIN base64 failed");
		return FALSE;
	}
	sLine = xrtFormat("AUTH PLAIN %s", sBase64);
	if ( sLine == NULL ) {
		xsmtp_buf_unit(&tBuf);
		xrtFree(sBase64);
		xsmtp_async_fail(pOp, "SMTP AUTH PLAIN alloc failed");
		return FALSE;
	}
	bOK = xsmtp_async_send_line(pOp, (const char*)sLine);
	xsmtp_buf_unit(&tBuf);
	xrtFree(sBase64);
	xrtFree(sLine);
	return bOK;
}

static bool xsmtp_async_send_auth_xoauth2_line(xsmtpasyncop* pOp)
{
	str sBase64 = NULL;
	str sLine = NULL;
	bool bOK = FALSE;

	sBase64 = xsmtp_build_xoauth2_response(
		pOp && pOp->pCtx ? pOp->pCtx->tCfg.sUser : NULL,
		pOp && pOp->pCtx ? pOp->pCtx->tCfg.sOAuth2Token : NULL);
	if ( sBase64 == NULL ) {
		xsmtp_async_fail(pOp, "SMTP AUTH XOAUTH2 token missing or base64 failed");
		return FALSE;
	}
	sLine = xrtFormat("AUTH XOAUTH2 %s", sBase64);
	if ( sLine == NULL ) {
		xrtFree(sBase64);
		xsmtp_async_fail(pOp, "SMTP AUTH XOAUTH2 alloc failed");
		return FALSE;
	}
	bOK = xsmtp_async_send_line(pOp, (const char*)sLine);
	xrtFree(sBase64);
	xrtFree(sLine);
	return bOK;
}

static void xsmtp_async_send_next_rcpt(xsmtpasyncop* pOp)
{
	const xsmtpaddr* pAddr;
	str sLine;

	pAddr = xsmtp_async_current_rcpt(pOp);
	if ( pAddr == NULL ) {
		if ( xsmtp_async_send_line(pOp, "DATA") ) {
			xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_DATA, "SMTP DATA timeout");
		}
		return;
	}
	if ( pAddr->sEmail == NULL || pAddr->sEmail[0] == '\0' ) {
		xsmtp_async_next_rcpt_cursor(pOp);
		xsmtp_async_send_next_rcpt(pOp);
		return;
	}
	sLine = xsmtp_build_rcpt_to_line(pAddr, pOp->iCapabilities, pOp->pRet);
	if ( sLine == NULL ) {
		xsmtp_async_fail(pOp, pOp->pRet->sError[0] ? pOp->pRet->sError : "SMTP RCPT TO alloc failed");
		return;
	}
	if ( xsmtp_async_send_line(pOp, (const char*)sLine) ) {
		xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_RCPT, "SMTP RCPT timeout");
	}
	xrtFree(sLine);
}

static bool xsmtp_async_run_post_ehlo(xsmtpasyncop* pOp, bool bAllowStartTLS)
{
	str sLine;
	int iSecureMode;

	if ( pOp == NULL || pOp->pCtx == NULL || pOp->pRet == NULL ) {
		return FALSE;
	}

	pOp->iCapabilities = pOp->iReplyCaps;
	pOp->iSizeLimit = pOp->iReplySizeLimit;
	pOp->pRet->iCapabilities = pOp->iCapabilities;
	pOp->pRet->iSizeLimit = pOp->iSizeLimit;
	iSecureMode = xsmtp_secure_mode_auto(&pOp->pCtx->tCfg);
	if ( bAllowStartTLS && iSecureMode == XSMTP_SECURE_STARTTLS ) {
		if ( !(pOp->iCapabilities & XSMTP_CAP_STARTTLS) ) {
			xsmtp_async_fail(pOp, "SMTP server does not support STARTTLS");
			return FALSE;
		}
		if ( xsmtp_async_send_line(pOp, "STARTTLS") ) {
			xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_STARTTLS, "SMTP STARTTLS timeout");
			return TRUE;
		}
		return FALSE;
	}

	pOp->iAuthMode = xsmtp_auth_mode_auto(&pOp->pCtx->tCfg, pOp->iCapabilities);
	pOp->pRet->iAuthMode = pOp->iAuthMode;
	if ( pOp->iAuthMode == XSMTP_AUTH_NONE ) {
		sLine = xsmtp_build_mail_from_line(&pOp->pCtx->tCfg, &pOp->pCtx->tMsg, pOp->iCapabilities, pOp->pRet);
		if ( sLine == NULL ) {
			xsmtp_async_fail(pOp, pOp->pRet->sError[0] ? pOp->pRet->sError : "SMTP MAIL FROM alloc failed");
			return FALSE;
		}
		if ( xsmtp_async_send_line(pOp, (const char*)sLine) ) {
			xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_MAIL_FROM, "SMTP MAIL FROM timeout");
		}
		xrtFree(sLine);
		return TRUE;
	}
	if ( pOp->iAuthMode == XSMTP_AUTH_PLAIN ) {
		if ( xsmtp_async_send_auth_plain_line(pOp) ) {
			xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_AUTH, "SMTP AUTH timeout");
		}
		return TRUE;
	}
	if ( pOp->iAuthMode == XSMTP_AUTH_LOGIN ) {
		if ( xsmtp_async_send_line(pOp, "AUTH LOGIN") ) {
			xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_AUTH_LOGIN_USER, "SMTP AUTH LOGIN timeout");
		}
		return TRUE;
	}
	if ( pOp->iAuthMode == XSMTP_AUTH_XOAUTH2 ) {
		if ( !(pOp->iCapabilities & XSMTP_CAP_AUTH_XOAUTH2) ) {
			xsmtp_async_fail(pOp, "SMTP server does not support AUTH XOAUTH2");
			return FALSE;
		}
		if ( xsmtp_async_send_auth_xoauth2_line(pOp) ) {
			xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_AUTH, "SMTP AUTH XOAUTH2 timeout");
		}
		return TRUE;
	}

	xsmtp_async_fail(pOp, "SMTP auth mode unsupported");
	return FALSE;
}

static void xsmtp_async_advance(xsmtpasyncop* pOp)
{
	const char* sHelo;
	str sLine;
	str sData;
	char* sBody;

	if ( pOp == NULL || pOp->bDone ) {
		return;
	}

	switch ( pOp->iState ) {
	case XSMTP_AST_WAIT_BANNER:
		sHelo = (pOp->pCtx->tCfg.sHeloName && pOp->pCtx->tCfg.sHeloName[0]) ? pOp->pCtx->tCfg.sHeloName : "localhost";
		sLine = xrtFormat("EHLO %s", sHelo);
		if ( sLine == NULL ) {
			xsmtp_async_fail(pOp, "SMTP EHLO alloc failed");
			return;
		}
		if ( xsmtp_async_send_line(pOp, (const char*)sLine) ) {
			xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_EHLO, "SMTP EHLO timeout");
		}
		xrtFree(sLine);
		return;
	case XSMTP_AST_WAIT_EHLO:
		(void)xsmtp_async_run_post_ehlo(pOp, TRUE);
		return;
	case XSMTP_AST_WAIT_STARTTLS:
		if ( !xsmtp_async_starttls_begin(pOp) ) {
			return;
		}
		xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_STARTTLS_TLS, "SMTP TLS handshake timeout");
		if ( pOp->pSess->bTlsReady ) {
			xsmtp_async_advance(pOp);
		}
		return;
	case XSMTP_AST_WAIT_STARTTLS_TLS:
		sHelo = (pOp->pCtx->tCfg.sHeloName && pOp->pCtx->tCfg.sHeloName[0]) ? pOp->pCtx->tCfg.sHeloName : "localhost";
		sLine = xrtFormat("EHLO %s", sHelo);
		if ( sLine == NULL ) {
			xsmtp_async_fail(pOp, "SMTP EHLO alloc failed");
			return;
		}
		if ( xsmtp_async_send_line(pOp, (const char*)sLine) ) {
			xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_EHLO_TLS, "SMTP EHLO timeout");
		}
		xrtFree(sLine);
		return;
	case XSMTP_AST_WAIT_EHLO_TLS:
		(void)xsmtp_async_run_post_ehlo(pOp, FALSE);
		return;
	case XSMTP_AST_WAIT_AUTH:
		sLine = xsmtp_build_mail_from_line(&pOp->pCtx->tCfg, &pOp->pCtx->tMsg, pOp->iCapabilities, pOp->pRet);
		if ( sLine == NULL ) {
			xsmtp_async_fail(pOp, pOp->pRet->sError[0] ? pOp->pRet->sError : "SMTP MAIL FROM alloc failed");
			return;
		}
		if ( xsmtp_async_send_line(pOp, (const char*)sLine) ) {
			xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_MAIL_FROM, "SMTP MAIL FROM timeout");
		}
		xrtFree(sLine);
		return;
	case XSMTP_AST_WAIT_AUTH_LOGIN_USER:
		sLine = xrtBase64Encode((ptr)xsmtp_str_or_empty(pOp->pCtx->tCfg.sUser), strlen(xsmtp_str_or_empty(pOp->pCtx->tCfg.sUser)), NULL);
		if ( sLine == NULL ) {
			xsmtp_async_fail(pOp, "SMTP AUTH LOGIN base64 user failed");
			return;
		}
		if ( xsmtp_async_send_line(pOp, (const char*)sLine) ) {
			xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_AUTH_LOGIN_PASS, "SMTP AUTH LOGIN user timeout");
		}
		xrtFree(sLine);
		return;
	case XSMTP_AST_WAIT_AUTH_LOGIN_PASS:
		sLine = xrtBase64Encode((ptr)xsmtp_str_or_empty(pOp->pCtx->tCfg.sPass), strlen(xsmtp_str_or_empty(pOp->pCtx->tCfg.sPass)), NULL);
		if ( sLine == NULL ) {
			xsmtp_async_fail(pOp, "SMTP AUTH LOGIN base64 pass failed");
			return;
		}
		if ( xsmtp_async_send_line(pOp, (const char*)sLine) ) {
			xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_AUTH, "SMTP AUTH LOGIN pass timeout");
		}
		xrtFree(sLine);
		return;
	case XSMTP_AST_WAIT_MAIL_FROM:
		pOp->iRcptListKind = 0;
		pOp->iRcptIndex = 0;
		xsmtp_async_send_next_rcpt(pOp);
		return;
	case XSMTP_AST_WAIT_RCPT:
		xsmtp_async_next_rcpt_cursor(pOp);
		xsmtp_async_send_next_rcpt(pOp);
		return;
	case XSMTP_AST_WAIT_DATA:
		sData = xsmtp_build_message_data(&pOp->pCtx->tCfg, &pOp->pCtx->tMsg);
		if ( sData == NULL ) {
			xsmtp_async_fail(pOp, "SMTP build message failed");
			return;
		}
		if ( !xsmtp_check_message_size((const char*)sData, pOp->iCapabilities, pOp->iSizeLimit, pOp->pRet) ) {
			xrtFree(sData);
			xsmtp_async_resolve(pOp);
			return;
		}
		sBody = xsmtp_build_data_body_bytes((const char*)sData);
		xrtFree(sData);
		if ( sBody == NULL ) {
			xsmtp_async_fail(pOp, "SMTP DATA body alloc failed");
			return;
		}
		if ( !xsmtp_async_send_plain(pOp, sBody, strlen(sBody)) ) {
			xrtFree(sBody);
			xsmtp_async_fail(pOp, "SMTP DATA send failed");
			return;
		}
		xrtFree(sBody);
		xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_BODY, "SMTP DATA body timeout");
		return;
	case XSMTP_AST_WAIT_BODY:
		if ( xsmtp_async_send_line(pOp, "QUIT") ) {
			xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_QUIT, "SMTP QUIT timeout");
		}
		return;
	case XSMTP_AST_WAIT_QUIT:
		pOp->pRet->bSuccess = TRUE;
		xsmtp_async_resolve(pOp);
		return;
	default:
		xsmtp_async_fail(pOp, "SMTP async invalid state");
		return;
	}
}

static void xsmtp_async_on_reply_complete(xsmtpasyncop* pOp)
{
	int iExpect = 0;

	if ( pOp == NULL || pOp->bDone ) {
		return;
	}
	if ( pOp->pRet != NULL && pOp->tReply.sData != NULL ) {
		snprintf(pOp->pRet->sLastReply, sizeof(pOp->pRet->sLastReply), "%s", pOp->tReply.sData);
	}
	switch ( pOp->iState ) {
	case XSMTP_AST_WAIT_BANNER: iExpect = 220; break;
	case XSMTP_AST_WAIT_EHLO: iExpect = 250; break;
	case XSMTP_AST_WAIT_STARTTLS: iExpect = 220; break;
	case XSMTP_AST_WAIT_EHLO_TLS: iExpect = 250; break;
	case XSMTP_AST_WAIT_AUTH: iExpect = 235; break;
	case XSMTP_AST_WAIT_AUTH_LOGIN_USER: iExpect = 334; break;
	case XSMTP_AST_WAIT_AUTH_LOGIN_PASS: iExpect = 334; break;
	case XSMTP_AST_WAIT_MAIL_FROM: iExpect = 250; break;
	case XSMTP_AST_WAIT_RCPT: iExpect = 250; break;
	case XSMTP_AST_WAIT_DATA: iExpect = 354; break;
	case XSMTP_AST_WAIT_BODY: iExpect = 250; break;
	case XSMTP_AST_WAIT_QUIT: iExpect = 221; break;
	default: iExpect = 0; break;
	}
	if ( iExpect > 0 && pOp->iReplyCode != iExpect ) {
		xsmtp_async_fail(pOp, pOp->pRet ? pOp->pRet->sLastReply : "SMTP unexpected reply");
		return;
	}
	if ( pOp->pRet != NULL ) {
		pOp->pRet->iServerCode = pOp->iReplyCode;
	}
	xsmtp_async_advance(pOp);
}

static void xsmtp_async_arm_timeout(xsmtpasyncop* pOp, const char* sReason)
{
	if ( pOp == NULL || pOp->bDone ) {
		return;
	}
	pOp->iWaitToken++;
	pOp->tScratch.iLen = 0;
	if ( pOp->tScratch.sData != NULL ) {
		pOp->tScratch.sData[0] = '\0';
	}
	(void)xsmtp_buf_append(&pOp->tScratch, sReason ? sReason : "SMTP async timeout");
}

static void xsmtp_async_process_recv(xsmtpasyncop* pOp)
{
	char sLine[1024];
	xsmtpsession* pSess;

	if ( pOp == NULL || pOp->bDone || pOp->pCtx == NULL ) {
		return;
	}
	pSess = pOp->pSess;
	while ( xsmtp_try_read_line_from_buffer(pSess, sLine, sizeof(sLine)) ) {
		if ( !pOp->bReplyHasFirst ) {
			if ( strlen(sLine) < 3 || !isdigit((unsigned char)sLine[0]) || !isdigit((unsigned char)sLine[1]) || !isdigit((unsigned char)sLine[2]) ) {
				xsmtp_async_fail(pOp, "SMTP invalid reply line");
				return;
			}
			pOp->iReplyCode = (sLine[0] - '0') * 100 + (sLine[1] - '0') * 10 + (sLine[2] - '0');
			pOp->bReplyHasFirst = TRUE;
		}
		if ( !xsmtp_buf_append(&pOp->tReply, sLine) || !xsmtp_buf_append(&pOp->tReply, "\n") ) {
			xsmtp_async_fail(pOp, "SMTP reply buffer alloc failed");
			return;
		}
		if ( strlen(sLine) > 4 && pOp->iReplyCode == 250 ) {
			xsmtp_parse_capability_line(sLine + 4, &pOp->iReplyCaps, &pOp->iReplySizeLimit);
		}
		if ( strlen(sLine) < 4 || sLine[3] != '-' ) {
			xsmtp_async_on_reply_complete(pOp);
			if ( pOp->bDone ) {
				return;
			}
			xsmtp_async_reply_reset(pOp);
		}
	}
}

static void xsmtp_async_stream_on_open(ptr pOwner, xnetstream* pStream)
{
	xsmtpasyncop* pOp = (xsmtpasyncop*)pOwner;
	(void)pStream;
	if ( pOp == NULL || pOp->bDone ) {
		return;
	}
	if ( pOp->pRet != NULL && xsmtp_secure_mode_auto(&pOp->pCtx->tCfg) == XSMTP_SECURE_SSL ) {
		pOp->pRet->bUsedTLS = TRUE;
	}
	xsmtp_async_set_state(pOp, XSMTP_AST_WAIT_BANNER, "SMTP banner timeout");
}

static void xsmtp_async_stream_on_recv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	xsmtpasyncop* pOp = (xsmtpasyncop*)pOwner;
	char aBuf[4096];
	size_t iLeft;
	(void)pStream;

	if ( pOp == NULL || pOp->bDone || pChain == NULL ) {
		return;
	}
	iLeft = xrtNetChainBytes(pChain);
	while ( iLeft > 0 ) {
		size_t iChunk = iLeft > sizeof(aBuf) ? sizeof(aBuf) : iLeft;
		size_t iRead = xrtNetChainPeek(pChain, aBuf, iChunk);
		if ( iRead == 0 ) {
			break;
		}
		if ( pOp->pSess != NULL && pOp->pSess->pTls != NULL && !pOp->pSess->bStreamTls ) {
			xnet_result iFeed = xrtNetTlsSessionFeedCipher(pOp->pSess->pTls, aBuf, iRead);
			if ( iFeed != XRT_NET_OK ) {
				xsmtp_async_fail_tls(pOp, "SMTP TLS feed failed", iFeed);
				return;
			}
			if ( !pOp->pSess->bTlsReady ) {
				if ( !xsmtp_async_tls_drive_handshake(pOp) ) {
					return;
				}
				if ( pOp->bDone ) {
					return;
				}
				if ( pOp->pSess->bTlsReady && pOp->iState == XSMTP_AST_WAIT_STARTTLS_TLS ) {
					xsmtp_async_advance(pOp);
				}
			}
			if ( pOp->pSess->bTlsReady && !xsmtp_async_tls_drain_plain(pOp) ) {
				return;
			}
		} else if ( !xsmtp_append_recv(pOp->pSess, aBuf, iRead, pOp->pRet) ) {
			xsmtp_async_fail(pOp, pOp->pRet->sError);
			return;
		}
		xrtNetChainConsume(pChain, iRead);
		iLeft -= iRead;
	}
	xsmtp_async_process_recv(pOp);
}

static void xsmtp_async_stream_on_close(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	xsmtpasyncop* pOp = (xsmtpasyncop*)pOwner;
	(void)pStream;
	if ( pOp == NULL || pOp->bDone ) {
		return;
	}
	XSMTP_TRACE("async stream: close reason=%d", (int)iReason);
	if ( pOp->pRet != NULL && !pOp->pRet->bSuccess ) {
		xsmtp_result_errorf(pOp->pRet, "SMTP stream closed (%d)", (int)iReason);
	}
	xsmtp_async_resolve(pOp);
}

static void xsmtp_async_stream_on_error(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	xsmtpasyncop* pOp = (xsmtpasyncop*)pOwner;
	(void)pStream;
	if ( pOp == NULL || pOp->bDone ) {
		return;
	}
	XSMTP_TRACE("async stream: error sys=%d", iSysErr);
	xsmtp_result_errorf(pOp->pRet, "SMTP stream error (%d)", iSysErr);
	xsmtp_async_resolve(pOp);
}

static const xnetstreamevents* xsmtp_async_stream_events(void)
{
	static const xnetstreamevents tEvents = {
		xsmtp_async_stream_on_open,
		xsmtp_async_stream_on_recv,
		NULL,
		xsmtp_async_stream_on_close,
		xsmtp_async_stream_on_error,
		NULL,
		NULL
	};
	return &tEvents;
}

static xfuture* xsmtp_send_mail_future_thread(const xsmtpconfig* pCfg, const xsmtpmessage* pMsg, const xsmtpasyncopts* pOpts);

static xfuture* xsmtp_send_mail_future_native(const xsmtpconfig* pCfg, const xsmtpmessage* pMsg, const xsmtpasyncopts* pOpts)
{
	xsmtpasyncop* pOp;
	xsmtptaskctx* pCtx;
	xfuture* pFuture;
	xpromise* pPromise;
	xnetengine* pEngine;
	bool bOwnEngine = FALSE;
	xnetconnectconfig tConnCfg;
	xnetengineconfig tEngineCfg;

	(void)xrtInit();

	pEngine = (pOpts != NULL) ? pOpts->pEngine : NULL;
	if ( pEngine == NULL ) {
		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tEngineCfg);
		if ( pEngine == NULL ) {
			XSMTP_TRACE("async native: engine create failed, fallback thread");
			return xsmtp_send_mail_future_thread(pCfg, pMsg, pOpts);
		}
		if ( xrtNetEngineStart(pEngine) != XRT_NET_OK ) {
			XSMTP_TRACE("async native: engine start failed, fallback thread");
			xrtNetEngineDestroy(pEngine);
			return xsmtp_send_mail_future_thread(pCfg, pMsg, pOpts);
		}
		bOwnEngine = TRUE;
	}

	pCtx = xsmtp_task_ctx_create(pCfg, pMsg, pOpts);
	if ( pCtx == NULL ) {
		XSMTP_TRACE("async native: task ctx create failed");
		if ( bOwnEngine ) {
			xrtNetEngineDestroy(pEngine);
		}
		return NULL;
	}

	pFuture = xFutureCreate();
	if ( pFuture == NULL ) {
		XSMTP_TRACE("async native: future create failed");
		xsmtp_task_ctx_free(pCtx);
		if ( bOwnEngine ) {
			xrtNetEngineDestroy(pEngine);
		}
		return NULL;
	}
	pPromise = xPromiseCreate(pFuture);
	if ( pPromise == NULL ) {
		XSMTP_TRACE("async native: promise create failed");
		xsmtp_task_ctx_free(pCtx);
		if ( bOwnEngine ) {
			xrtNetEngineDestroy(pEngine);
		}
		xFutureRelease(pFuture);
		return NULL;
	}

	pOp = (xsmtpasyncop*)xrtMalloc(sizeof(*pOp));
	if ( pOp == NULL ) {
		XSMTP_TRACE("async native: op alloc failed");
		xsmtp_task_ctx_free(pCtx);
		if ( bOwnEngine ) {
			xrtNetEngineDestroy(pEngine);
		}
		xPromiseDestroy(pPromise);
		xFutureRelease(pFuture);
		return NULL;
	}
	memset(pOp, 0, sizeof(*pOp));
	pOp->pEngine = pEngine;
	pOp->pPromise = pPromise;
	pOp->pCtx = pCtx;
	pOp->pSess = (xsmtpsession*)xrtCalloc(1, sizeof(xsmtpsession));
	if ( pOp->pSess == NULL ) {
		xsmtp_async_cleanup(pOp);
		xFutureRelease(pFuture);
		return NULL;
	}
	pOp->pSess->pEngine = pEngine;
	pOp->pSess->bOwnEngine = bOwnEngine;
	pOp->pSess->iTimeoutMs = pCtx->tCfg.iTimeoutMs;
	pOp->pRet = (xsmtpresult*)xrtMalloc(sizeof(xsmtpresult));
	if ( pOp->pRet == NULL ) {
		XSMTP_TRACE("async native: result alloc failed");
		xsmtp_async_cleanup(pOp);
		xFutureRelease(pFuture);
		return NULL;
	}
	xrtSmtpResultInit(pOp->pRet);
	xsmtp_result_stage(pOp->pRet, XSMTP_STAGE_CONNECT);
	xrtNetConnectConfigInit(&tConnCfg);
	tConnCfg.sHost = pCtx->tCfg.sHost;
	tConnCfg.iPort = pCtx->tCfg.iPort;
	tConnCfg.iConnectTimeoutMs = pCtx->tCfg.iTimeoutMs;
	if ( xsmtp_secure_mode_auto(&pCtx->tCfg) == XSMTP_SECURE_SSL ) {
		memset(&pOp->tTlsCfg, 0, sizeof(pOp->tTlsCfg));
		pOp->tTlsCfg.sHostName = pCtx->tCfg.sHost;
		pOp->tTlsCfg.bVerifyPeer = pCtx->tCfg.bVerifyPeer;
		pOp->tTlsCfg.iMaxVersion = pCtx->tCfg.iTlsMaxVersion;
		pOp->tTlsCfg.sCaFile = pCtx->tCfg.sCaFile;
		tConnCfg.pTlsConfig = &pOp->tTlsCfg;
	}
	pOp->pStream = xrtNetStreamCreate(pEngine, xsmtp_async_stream_events(), pOp);
	if ( pOp->pStream == NULL ) {
		XSMTP_TRACE("async native: stream create failed, fallback thread");
		pOp->bDone = TRUE;
		xsmtp_async_cleanup(pOp);
		xFutureRelease(pFuture);
		return xsmtp_send_mail_future_thread(pCfg, pMsg, pOpts);
	}
	pOp->pSess->pStream = pOp->pStream;
	if ( xrtNetStreamConnect(pOp->pStream, &tConnCfg) != XRT_NET_OK ) {
		str sErr = xrtGetError();
		(void)sErr;
		XSMTP_TRACE("async native: connect submit failed: %s, fallback thread", sErr ? (const char*)sErr : "");
		pOp->bDone = TRUE;
		xsmtp_async_cleanup(pOp);
		xFutureRelease(pFuture);
		return xsmtp_send_mail_future_thread(pCfg, pMsg, pOpts);
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	XSMTP_TRACE("async native: connect submitted");
	return pFuture;
}

static int32 xsmtp_send_mail_task(ptr pArg, xfuture_result* pOut)
{
	xsmtptaskctx* pCtx = (xsmtptaskctx*)pArg;
	xsmtpresult* pRet;

	if ( pOut == NULL ) {
		xsmtp_task_ctx_free(pCtx);
		return XRT_NET_ERROR;
	}
	memset(pOut, 0, sizeof(*pOut));
	if ( pCtx == NULL ) {
		pOut->iStatus = XRT_NET_ERROR;
		pOut->sError = (str)"SMTP async task context missing";
		return pOut->iStatus;
	}

	pRet = (xsmtpresult*)xrtMalloc(sizeof(*pRet));
	if ( pRet == NULL ) {
		pOut->iStatus = XRT_NET_ERROR;
		pOut->sError = (str)"SMTP async result alloc failed";
		xsmtp_task_ctx_free(pCtx);
		return pOut->iStatus;
	}
	xrtSmtpResultInit(pRet);
	(void)xrtSmtpSendMail(&pCtx->tCfg, &pCtx->tMsg, pRet);

	pOut->iStatus = XRT_NET_OK;
	pOut->pValue = pRet;
	pOut->iFlags = XFUTURE_RESULT_F_OWN_VALUE;
	xsmtp_task_ctx_free(pCtx);
	return pOut->iStatus;
}

static xfuture* xsmtp_send_mail_future_thread(const xsmtpconfig* pCfg, const xsmtpmessage* pMsg, const xsmtpasyncopts* pOpts)
{
	xsmtptaskctx* pCtx;
	xfuture* pFuture;

	(void)xrtInit();
	pCtx = xsmtp_task_ctx_create(pCfg, pMsg, pOpts);
	if ( pCtx == NULL ) {
		XSMTP_TRACE("async thread fallback: task ctx create failed");
		return NULL;
	}

	pFuture = xTaskRunThread(xsmtp_send_mail_task, pCtx, 0);
	if ( pFuture == NULL ) {
		XSMTP_TRACE("async thread fallback: xTaskRunThread failed");
		xsmtp_task_ctx_free(pCtx);
		return NULL;
	}
	if ( pOpts != NULL && pOpts->sDebugName != NULL && pOpts->sDebugName[0] != '\0' ) {
		(void)xFutureSetDebugName(pFuture, (str)pOpts->sDebugName);
	}
	return pFuture;
}

static xfuture* xrtSmtpSendMailFuture(const xsmtpconfig* pCfg, const xsmtpmessage* pMsg, const xsmtpasyncopts* pOpts)
{
	if ( pCfg == NULL || pMsg == NULL ) {
		return NULL;
	}
	return xsmtp_send_mail_future_native(pCfg, pMsg, pOpts);
}

static bool xrtSmtpSendMailCo(const xsmtpconfig* pCfg, const xsmtpmessage* pMsg, xsmtpresult* pOut)
{
	xfuture* pFuture;
	xsmtpresult* pRet;

	if ( pOut == NULL ) {
		return FALSE;
	}
	xrtSmtpResultInit(pOut);

	pFuture = xrtSmtpSendMailFuture(pCfg, pMsg, NULL);
	if ( pFuture == NULL ) {
		xsmtp_result_error(pOut, "SMTP async future create failed");
		return FALSE;
	}

	pRet = (xsmtpresult*)xFutureWaitCoValue(pFuture);
	if ( pRet == NULL ) {
		xfuture_result tResult;
		memset(&tResult, 0, sizeof(tResult));
		if ( xFutureGetResult(pFuture, &tResult) && tResult.pValue != NULL ) {
			pRet = (xsmtpresult*)tResult.pValue;
		}
	}
	if ( pRet == NULL ) {
		str sError = xFutureError(pFuture);
		xsmtp_result_error(pOut, sError ? (const char*)sError : "SMTP async wait failed");
		xFutureRelease(pFuture);
		return FALSE;
	}

	*pOut = *pRet;
	xrtSmtpResultFree(pRet);
	xFutureRelease(pFuture);
	return pOut->bSuccess;
}

static bool xrtSmtpSendMailAsyncWait(const xsmtpconfig* pCfg, const xsmtpmessage* pMsg, const xsmtpasyncopts* pOpts, xsmtpresult* pOut)
{
	xfuture* pFuture;
	xsmtpresult* pRet;

	if ( pOut == NULL ) {
		return FALSE;
	}
	xrtSmtpResultInit(pOut);

	pFuture = xrtSmtpSendMailFuture(pCfg, pMsg, pOpts);
	if ( pFuture == NULL ) {
		XSMTP_TRACE("async wait: future create returned null");
		xsmtp_result_error(pOut, "SMTP async future create failed");
		return FALSE;
	}
	pRet = (xsmtpresult*)xFutureWaitValue(pFuture);
	if ( pRet == NULL ) {
		xfuture_result tResult;
		memset(&tResult, 0, sizeof(tResult));
		if ( xFutureGetResult(pFuture, &tResult) && tResult.pValue != NULL ) {
			pRet = (xsmtpresult*)tResult.pValue;
		}
	}
	if ( pRet == NULL ) {
		str sError = xFutureError(pFuture);
		xsmtp_result_error(pOut, sError ? (const char*)sError : "SMTP async wait failed");
		xFutureRelease(pFuture);
		return FALSE;
	}

	*pOut = *pRet;
	xrtSmtpResultFree(pRet);
	xFutureRelease(pFuture);
	return pOut->bSuccess;
}

#endif
