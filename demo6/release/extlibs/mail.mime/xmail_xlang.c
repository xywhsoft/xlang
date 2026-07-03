#include "xmail_xlang.h"

#include "xrt.h"

#if defined(XMAIL_XLANG_WITH_SMTP) || defined(XMAIL_XLANG_WITH_POP3) || defined(XMAIL_XLANG_WITH_IMAP)
#ifndef XMAIL_XLANG_WITH_PROTOCOLS
#define XMAIL_XLANG_WITH_PROTOCOLS 1
#endif
#endif

#if defined(XMAIL_XLANG_WITH_PROTOCOLS) && !defined(XMAIL_XLANG_WITH_SMTP) && !defined(XMAIL_XLANG_WITH_POP3) && !defined(XMAIL_XLANG_WITH_IMAP)
#define XMAIL_XLANG_WITH_SMTP 1
#define XMAIL_XLANG_WITH_POP3 1
#define XMAIL_XLANG_WITH_IMAP 1
#endif

static ptr xmail_xrt_future_wait_value(xfuture* pFuture)
{
	return xFutureWait(pFuture) ? xFutureValue(pFuture) : NULL;
}

static ptr xmail_xrt_future_wait_co_value(xfuture* pFuture)
{
	return xFutureWaitCo(pFuture) ? xFutureValue(pFuture) : NULL;
}

static xfuture* xmail_xrt_future_create(void)
{
	return NULL;
}

static bool xmail_xrt_future_get_result(xfuture* pFuture, xfuture_result* pOut)
{
	if ( pOut == NULL ) { return FALSE; }
	memset(pOut, 0, sizeof(*pOut));
	if ( pFuture == NULL ) {
		pOut->iStatus = XRT_NET_ERROR;
		return FALSE;
	}
	pOut->pValue = xFutureValue(pFuture);
	pOut->iStatus = pOut->pValue != NULL ? XRT_NET_OK : XRT_NET_ERROR;
	pOut->iFlags = XFUTURE_RESULT_F_NONE;
	return pOut->iStatus == XRT_NET_OK;
}

static bool xmail_xrt_future_set_debug_name(xfuture* pFuture, str sDebugName)
{
	(void)pFuture;
	(void)sDebugName;
	return TRUE;
}

typedef struct xmail_xrt_promise_compat {
	xfuture* pFuture;
} xmail_xrt_promise_compat;

static xpromise* xmail_xrt_promise_create(xfuture* pFuture)
{
	xmail_xrt_promise_compat* pPromise;
	if ( pFuture == NULL ) { return NULL; }
	pPromise = (xmail_xrt_promise_compat*)xrtCalloc(1u, sizeof(*pPromise));
	if ( pPromise == NULL ) { return NULL; }
	pPromise->pFuture = xFutureAddRef(pFuture);
	return (xpromise*)pPromise;
}

static void xmail_xrt_promise_destroy(xpromise* pPromise)
{
	xmail_xrt_promise_compat* pCompat = (xmail_xrt_promise_compat*)pPromise;
	if ( pCompat == NULL ) { return; }
	if ( pCompat->pFuture != NULL ) {
		xFutureRelease(pCompat->pFuture);
	}
	xrtFree(pCompat);
}

static bool xmail_xrt_promise_resolve(xpromise* pPromise, ptr pValue)
{
	(void)pPromise;
	(void)pValue;
	return FALSE;
}

#define xFutureWaitValue xmail_xrt_future_wait_value
#define xFutureWaitCoValue xmail_xrt_future_wait_co_value
#define xFutureCreate xmail_xrt_future_create
#define xFutureGetResult xmail_xrt_future_get_result
#define xFutureSetDebugName xmail_xrt_future_set_debug_name
#define xPromiseCreate xmail_xrt_promise_create
#define xPromiseDestroy xmail_xrt_promise_destroy
#define xPromiseResolve xmail_xrt_promise_resolve

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "xmail_mime/xmail_mime.h"
#if defined(XMAIL_XLANG_WITH_PROTOCOLS)
#include "xsmtp/xsmtp.h"
#include "xpop3/xpop3.h"
#include "ximap/ximap.h"
#endif
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static char* g_xmail_xlang_last_json = NULL;

static xvalue xmail_xlang_null_data(void)
{
	return xvoCreateNull();
}

static void xmail_xlang_table_set_text(xvalue pTbl, const char* sKey, const char* sText);

static const char* xmail_xlang_return_value(xvalue pValue)
{
	str sJson;
	size_t iSize = 0u;

	if ( pValue == NULL ) {
		pValue = xvoCreateNull();
	}
	sJson = xrtStringifyJSON(pValue, 0, &iSize);
	xvoUnref(pValue);
	if ( sJson == NULL ) {
		return "{\"ok\":false,\"error\":\"xmail json stringify failed\",\"stage\":\"json\",\"status\":\"error\",\"server_code\":0,\"capabilities\":[],\"last_reply\":\"\",\"used_tls\":false,\"used_starttls\":false,\"data\":null}";
	}
	if ( g_xmail_xlang_last_json != NULL ) {
		xrtFree(g_xmail_xlang_last_json);
	}
	g_xmail_xlang_last_json = (char*)sJson;
	return g_xmail_xlang_last_json;
}

static xvalue xmail_xlang_result_value(bool bOk, const char* sError, const char* sStage, const char* sStatus, xvalue pData)
{
	xvalue pRet = xvoCreateTable();
	xvoTableSetBool(pRet, "ok", 0, bOk);
	xvoTableSetText(pRet, "error", 0, (ptr)xmailMimeStrOrEmpty(sError), (uint32)strlen(xmailMimeStrOrEmpty(sError)), FALSE);
	xvoTableSetText(pRet, "stage", 0, (ptr)xmailMimeStrOrEmpty(sStage), (uint32)strlen(xmailMimeStrOrEmpty(sStage)), FALSE);
	xvoTableSetText(pRet, "status", 0, (ptr)xmailMimeStrOrEmpty(sStatus), (uint32)strlen(xmailMimeStrOrEmpty(sStatus)), FALSE);
	xvoTableSetInt(pRet, "server_code", 0, 0);
	xvoTableSetValue(pRet, "capabilities", 0, xvoCreateArray(), TRUE);
	xvoTableSetText(pRet, "last_reply", 0, "", 0, FALSE);
	xvoTableSetBool(pRet, "used_tls", 0, FALSE);
	xvoTableSetBool(pRet, "used_starttls", 0, FALSE);
	xvoTableSetValue(pRet, "data", 0, pData ? pData : xmail_xlang_null_data(), TRUE);
	return pRet;
}

static xvalue xmail_xlang_parse_result_json(const char* sJson, const char* sStage)
{
	xvalue pRet;
	const char* sText = xmailMimeStrOrEmpty(sJson);
	pRet = xrtParseJSON((str)sText, strlen(sText));
	if ( pRet == NULL ) {
		return xmail_xlang_result_value(FALSE, "xmail result JSON parse failed", sStage, "error", NULL);
	}
	return pRet;
}

#if !defined(XMAIL_XLANG_PROTOCOL_ONLY)
XMAIL_XLANG_API xvalue xmail_mime_result_native(bool bOk, const char* sError, const char* sStage, xvalue pData)
{
	if ( pData != NULL ) {
		xvoAddRef(pData);
	}
	return xmail_xlang_result_value(bOk, sError, sStage, bOk ? "ok" : "error", pData);
}

XMAIL_XLANG_API xvalue xmail_mime_unsupported_native(const char* sOperation)
{
	char sError[256];
	snprintf(sError, sizeof(sError), "mail.mime.%s native binding is not connected", xmailMimeStrOrEmpty(sOperation));
	return xmail_xlang_result_value(FALSE, sError, "binding", "error", xvoCreateTable());
}

XMAIL_XLANG_API xvalue xmail_mime_address_native(const char* sEmail, const char* sName)
{
	xvalue pRet = xvoCreateTable();
	xmail_xlang_table_set_text(pRet, "email", sEmail);
	xmail_xlang_table_set_text(pRet, "name", sName);
	return pRet;
}

XMAIL_XLANG_API xvalue xmail_mime_attachment_native(const char* sFileName, const char* sContentType, xvalue pContent, const char* sContentId, bool bInline)
{
	xvalue pRet = xvoCreateTable();
	xmail_xlang_table_set_text(pRet, "filename", sFileName);
	xmail_xlang_table_set_text(pRet, "content_type", sContentType);
	if ( pContent != NULL ) {
		xvoAddRef(pContent);
		xvoTableSetValue(pRet, "content", 0, pContent, TRUE);
	} else {
		xvoTableSetValue(pRet, "content", 0, xvoCreateNull(), TRUE);
	}
	xmail_xlang_table_set_text(pRet, "content_id", sContentId);
	xvoTableSetBool(pRet, "inline", 0, bInline);
	return pRet;
}

XMAIL_XLANG_API xvalue xmail_mime_message_native(xvalue pFields)
{
	if ( pFields == NULL ) {
		return xvoCreateTable();
	}
	xvoAddRef(pFields);
	return pFields;
}
#endif

static xvalue xmail_xlang_table_get(xvalue pTbl, const char* sKey)
{
	return xvoTableGetValue(pTbl, sKey, 0);
}

static void xmail_xlang_table_set_text(xvalue pTbl, const char* sKey, const char* sText)
{
	const char* sValue = xmailMimeStrOrEmpty(sText);
	xvoTableSetText(pTbl, sKey, 0, (ptr)sValue, (uint32)strlen(sValue), FALSE);
}

static const char* xmail_xlang_text_at(xvalue pTbl, const char* sKey)
{
	xvalue pVal = xmail_xlang_table_get(pTbl, sKey);
	if ( pVal == NULL || pVal->Type == XVO_DT_NULL ) {
		return "";
	}
	return (const char*)xvoGetText(pVal);
}

static bool xmail_xlang_bool_at(xvalue pTbl, const char* sKey)
{
	xvalue pVal = xmail_xlang_table_get(pTbl, sKey);
	return xvoGetBool(pVal);
}

static uint16 xmail_xlang_tls_max_version_at(xvalue pTbl, const char* sKey)
{
	xvalue pVal = xmail_xlang_table_get(pTbl, sKey);
	const char* sText;
	if ( pVal == NULL || pVal->Type == XVO_DT_NULL ) {
		return 0u;
	}
	if ( pVal->Type != XVO_DT_TEXT ) {
		return (uint16)xvoGetInt(pVal);
	}
	sText = (const char*)xvoGetText(pVal);
	if ( xmailMimeAsciiEqI(sText, "tls1.2") || xmailMimeAsciiEqI(sText, "tlsv1.2") || xmailMimeAsciiEqI(sText, "1.2") ) {
		return 0x0303u;
	}
	if ( xmailMimeAsciiEqI(sText, "tls1.3") || xmailMimeAsciiEqI(sText, "tlsv1.3") || xmailMimeAsciiEqI(sText, "1.3") ) {
		return 0x0304u;
	}
	if ( xmailMimeAsciiEqI(sText, "default") || xmailMimeAsciiEqI(sText, "auto") || sText[0] == '\0' ) {
		return 0u;
	}
	return (uint16)xvoGetInt(pVal);
}

static void xmail_xlang_fill_addr_from_value(xvalue pVal, xmailaddr* pAddr)
{
	memset(pAddr, 0, sizeof(*pAddr));
	if ( pVal != NULL && pVal->Type == XVO_DT_TABLE ) {
		pAddr->sEmail = xmail_xlang_text_at(pVal, "email");
		pAddr->sName = xmail_xlang_text_at(pVal, "name");
	}
}

static xmailaddr* xmail_xlang_addr_array_from_value(xvalue pVal, size_t* pCount)
{
	uint32 i;
	uint32 iCount;
	xmailaddr* arrAddr;

	*pCount = 0u;
	if ( pVal == NULL || pVal->Type != XVO_DT_ARRAY ) {
		return NULL;
	}
	iCount = xvoArrayItemCount(pVal);
	if ( iCount == 0u ) {
		return NULL;
	}
	arrAddr = (xmailaddr*)xrtCalloc(iCount, sizeof(xmailaddr));
	if ( arrAddr == NULL ) {
		return NULL;
	}
	for ( i = 0u; i < iCount; i++ ) {
		xmail_xlang_fill_addr_from_value(xvoArrayGetValue(pVal, i), &arrAddr[*pCount]);
		if ( arrAddr[*pCount].sEmail != NULL && arrAddr[*pCount].sEmail[0] != '\0' ) {
			(*pCount)++;
		}
	}
	return arrAddr;
}

static void xmail_xlang_fill_reply_to(xvalue pRoot, xmailaddr* pReplyTo)
{
	xvalue pVal = xmail_xlang_table_get(pRoot, "reply_to");
	memset(pReplyTo, 0, sizeof(*pReplyTo));
	if ( pVal != NULL && pVal->Type == XVO_DT_ARRAY && xvoArrayItemCount(pVal) > 0u ) {
		xmail_xlang_fill_addr_from_value(xvoArrayGetValue(pVal, 0), pReplyTo);
	} else {
		xmail_xlang_fill_addr_from_value(pVal, pReplyTo);
	}
}

static xmailheader* xmail_xlang_headers_from_value(xvalue pVal, size_t* pCount)
{
	uint32 i;
	uint32 iCount;
	xmailheader* arrHeaders;

	*pCount = 0u;
	if ( pVal == NULL || pVal->Type != XVO_DT_ARRAY ) {
		return NULL;
	}
	iCount = xvoArrayItemCount(pVal);
	if ( iCount == 0u ) {
		return NULL;
	}
	arrHeaders = (xmailheader*)xrtCalloc(iCount, sizeof(xmailheader));
	if ( arrHeaders == NULL ) {
		return NULL;
	}
	for ( i = 0u; i < iCount; i++ ) {
		xvalue pItem = xvoArrayGetValue(pVal, i);
		if ( pItem != NULL && pItem->Type == XVO_DT_TABLE ) {
			const char* sName = xmail_xlang_text_at(pItem, "name");
			const char* sValue = xmail_xlang_text_at(pItem, "value");
			if ( sName[0] != '\0' ) {
				arrHeaders[*pCount].sName = sName;
				arrHeaders[*pCount].sValue = sValue;
				(*pCount)++;
			}
		}
	}
	return arrHeaders;
}

static xmailmimeattachment* xmail_xlang_attachments_from_value(xvalue pVal, size_t* pCount)
{
	uint32 i;
	uint32 iCount;
	xmailmimeattachment* arrAttachments;

	*pCount = 0u;
	if ( pVal == NULL || pVal->Type != XVO_DT_ARRAY ) {
		return NULL;
	}
	iCount = xvoArrayItemCount(pVal);
	if ( iCount == 0u ) {
		return NULL;
	}
	arrAttachments = (xmailmimeattachment*)xrtCalloc(iCount, sizeof(xmailmimeattachment));
	if ( arrAttachments == NULL ) {
		return NULL;
	}
	for ( i = 0u; i < iCount; i++ ) {
		xvalue pItem = xvoArrayGetValue(pVal, i);
		if ( pItem != NULL && pItem->Type == XVO_DT_TABLE ) {
			xvalue pContent = xmail_xlang_table_get(pItem, "content");
			const char* sContent = (pContent != NULL && pContent->Type != XVO_DT_NULL) ? (const char*)xvoGetText(pContent) : "";
			arrAttachments[*pCount].sFileName = xmail_xlang_text_at(pItem, "filename");
			arrAttachments[*pCount].sContentType = xmail_xlang_text_at(pItem, "content_type");
			arrAttachments[*pCount].sContentId = xmail_xlang_text_at(pItem, "content_id");
			arrAttachments[*pCount].pData = sContent;
			arrAttachments[*pCount].iDataLen = strlen(sContent);
			arrAttachments[*pCount].bInline = xmail_xlang_bool_at(pItem, "inline");
			if ( arrAttachments[*pCount].sFileName[0] != '\0' || arrAttachments[*pCount].iDataLen > 0u ) {
				(*pCount)++;
			}
		}
	}
	return arrAttachments;
}

#if defined(XMAIL_XLANG_WITH_PROTOCOLS)
static xsmtpaddr* xmail_xlang_smtp_addr_array_from_value(xvalue pVal, size_t* pCount, const char* sNotify, const char* sOrcpt)
{
	uint32 i;
	uint32 iCount;
	xsmtpaddr* arrAddr;

	*pCount = 0u;
	if ( pVal == NULL || pVal->Type != XVO_DT_ARRAY ) {
		return NULL;
	}
	iCount = xvoArrayItemCount(pVal);
	if ( iCount == 0u ) {
		return NULL;
	}
	arrAddr = (xsmtpaddr*)xrtCalloc(iCount, sizeof(xsmtpaddr));
	if ( arrAddr == NULL ) {
		return NULL;
	}
	for ( i = 0u; i < iCount; i++ ) {
		xvalue pItem = xvoArrayGetValue(pVal, i);
		if ( pItem != NULL && pItem->Type == XVO_DT_TABLE ) {
			arrAddr[*pCount].sEmail = xmail_xlang_text_at(pItem, "email");
			arrAddr[*pCount].sName = xmail_xlang_text_at(pItem, "name");
			arrAddr[*pCount].sDsnNotify = xmail_xlang_text_at(pItem, "dsn_notify");
			arrAddr[*pCount].sDsnOrcpt = xmail_xlang_text_at(pItem, "dsn_orcpt");
			if ( arrAddr[*pCount].sDsnNotify[0] == '\0' ) arrAddr[*pCount].sDsnNotify = sNotify;
			if ( arrAddr[*pCount].sDsnOrcpt[0] == '\0' ) arrAddr[*pCount].sDsnOrcpt = sOrcpt;
			if ( arrAddr[*pCount].sEmail != NULL && arrAddr[*pCount].sEmail[0] != '\0' ) {
				(*pCount)++;
			}
		}
	}
	return arrAddr;
}

static xsmtpattachment* xmail_xlang_smtp_attachments_from_value(xvalue pVal, size_t* pCount)
{
	uint32 i;
	uint32 iCount;
	xsmtpattachment* arrAttachments;

	*pCount = 0u;
	if ( pVal == NULL || pVal->Type != XVO_DT_ARRAY ) {
		return NULL;
	}
	iCount = xvoArrayItemCount(pVal);
	if ( iCount == 0u ) {
		return NULL;
	}
	arrAttachments = (xsmtpattachment*)xrtCalloc(iCount, sizeof(xsmtpattachment));
	if ( arrAttachments == NULL ) {
		return NULL;
	}
	for ( i = 0u; i < iCount; i++ ) {
		xvalue pItem = xvoArrayGetValue(pVal, i);
		if ( pItem != NULL && pItem->Type == XVO_DT_TABLE ) {
			xvalue pContent = xmail_xlang_table_get(pItem, "content");
			const char* sContent = (pContent != NULL && pContent->Type != XVO_DT_NULL) ? (const char*)xvoGetText(pContent) : "";
			arrAttachments[*pCount].sFileName = xmail_xlang_text_at(pItem, "filename");
			arrAttachments[*pCount].sContentType = xmail_xlang_text_at(pItem, "content_type");
			arrAttachments[*pCount].sContentId = xmail_xlang_text_at(pItem, "content_id");
			arrAttachments[*pCount].pData = sContent;
			arrAttachments[*pCount].iDataLen = strlen(sContent);
			arrAttachments[*pCount].bInline = xmail_xlang_bool_at(pItem, "inline");
			if ( arrAttachments[*pCount].sFileName[0] != '\0' || arrAttachments[*pCount].iDataLen > 0u ) {
				(*pCount)++;
			}
		}
	}
	return arrAttachments;
}

static void xmail_xlang_smtp_headers_from_value(xvalue pVal, const char*** ppNames, const char*** ppValues, size_t* pCount)
{
	uint32 i;
	uint32 iCount;
	const char** arrNames;
	const char** arrValues;

	*ppNames = NULL;
	*ppValues = NULL;
	*pCount = 0u;
	if ( pVal == NULL || pVal->Type != XVO_DT_ARRAY ) {
		return;
	}
	iCount = xvoArrayItemCount(pVal);
	if ( iCount == 0u ) {
		return;
	}
	arrNames = (const char**)xrtCalloc(iCount, sizeof(char*));
	arrValues = (const char**)xrtCalloc(iCount, sizeof(char*));
	if ( arrNames == NULL || arrValues == NULL ) {
		if ( arrNames != NULL ) xrtFree(arrNames);
		if ( arrValues != NULL ) xrtFree(arrValues);
		return;
	}
	for ( i = 0u; i < iCount; i++ ) {
		xvalue pItem = xvoArrayGetValue(pVal, i);
		if ( pItem != NULL && pItem->Type == XVO_DT_TABLE ) {
			const char* sName = xmail_xlang_text_at(pItem, "name");
			if ( sName[0] != '\0' ) {
				arrNames[*pCount] = sName;
				arrValues[*pCount] = xmail_xlang_text_at(pItem, "value");
				(*pCount)++;
			}
		}
	}
	*ppNames = arrNames;
	*ppValues = arrValues;
}

static const char** xmail_xlang_string_array_from_value(xvalue pVal, size_t* pCount)
{
	uint32 i;
	uint32 iCount;
	const char** arrText;

	*pCount = 0u;
	if ( pVal == NULL || pVal->Type != XVO_DT_ARRAY ) {
		return NULL;
	}
	iCount = xvoArrayItemCount(pVal);
	if ( iCount == 0u ) {
		return NULL;
	}
	arrText = (const char**)xrtCalloc(iCount, sizeof(char*));
	if ( arrText == NULL ) {
		return NULL;
	}
	for ( i = 0u; i < iCount; i++ ) {
		xvalue pItem = xvoArrayGetValue(pVal, i);
		const char* sText = (pItem != NULL && pItem->Type != XVO_DT_NULL) ? (const char*)xvoGetText(pItem) : "";
		if ( sText != NULL && sText[0] != '\0' ) {
			arrText[*pCount] = sText;
			(*pCount)++;
		}
	}
	return arrText;
}

static xvalue xmail_xlang_string_array_value(const char* const* arrText, size_t iCount)
{
	size_t i;
	xvalue pArr = xvoCreateArray();
	for ( i = 0u; i < iCount; i++ ) {
		const char* sText = xmailMimeStrOrEmpty(arrText ? arrText[i] : "");
		xvoArrayAppendText(pArr, (ptr)sText, (uint32)strlen(sText), FALSE);
	}
	return pArr;
}

static int xmail_xlang_smtp_secure_mode(const char* sSecure)
{
	if ( xmailMimeAsciiEqI(sSecure, "none") ) return XSMTP_SECURE_NONE;
	if ( xmailMimeAsciiEqI(sSecure, "ssl") ) return XSMTP_SECURE_SSL;
	if ( xmailMimeAsciiEqI(sSecure, "starttls") ) return XSMTP_SECURE_STARTTLS;
	return XSMTP_SECURE_AUTO;
}

static int xmail_xlang_smtp_auth_mode(const char* sAuth)
{
	if ( xmailMimeAsciiEqI(sAuth, "none") ) return XSMTP_AUTH_NONE;
	if ( xmailMimeAsciiEqI(sAuth, "plain") ) return XSMTP_AUTH_PLAIN;
	if ( xmailMimeAsciiEqI(sAuth, "login") ) return XSMTP_AUTH_LOGIN;
	if ( xmailMimeAsciiEqI(sAuth, "xoauth2") ) return XSMTP_AUTH_XOAUTH2;
	return XSMTP_AUTH_AUTO;
}

static const char* xmail_xlang_smtp_stage_name(int iStage)
{
	switch ( iStage ) {
	case XSMTP_STAGE_CONNECT: return "connect";
	case XSMTP_STAGE_TLS: return "tls";
	case XSMTP_STAGE_BANNER: return "banner";
	case XSMTP_STAGE_EHLO: return "ehlo";
	case XSMTP_STAGE_STARTTLS: return "starttls";
	case XSMTP_STAGE_AUTH: return "auth";
	case XSMTP_STAGE_MAIL_FROM: return "mail_from";
	case XSMTP_STAGE_RCPT: return "rcpt";
	case XSMTP_STAGE_DATA: return "data";
	case XSMTP_STAGE_QUIT: return "quit";
	default: return "";
	}
}

static const char* xmail_xlang_smtp_auth_name(int iAuthMode)
{
	switch ( iAuthMode ) {
	case XSMTP_AUTH_NONE: return "none";
	case XSMTP_AUTH_PLAIN: return "plain";
	case XSMTP_AUTH_LOGIN: return "login";
	case XSMTP_AUTH_XOAUTH2: return "xoauth2";
	default: return "auto";
	}
}

static void xmail_xlang_smtp_add_cap(xvalue pArr, uint32 iCaps, uint32 iCap, const char* sName)
{
	if ( iCaps & iCap ) {
		xvoArrayAppendText(pArr, (ptr)sName, (uint32)strlen(sName), FALSE);
	}
}

static xvalue xmail_xlang_smtp_caps_value(uint32 iCaps)
{
	xvalue pArr = xvoCreateArray();
	xmail_xlang_smtp_add_cap(pArr, iCaps, XSMTP_CAP_AUTH_PLAIN, "AUTH PLAIN");
	xmail_xlang_smtp_add_cap(pArr, iCaps, XSMTP_CAP_AUTH_LOGIN, "AUTH LOGIN");
	xmail_xlang_smtp_add_cap(pArr, iCaps, XSMTP_CAP_AUTH_XOAUTH2, "AUTH XOAUTH2");
	xmail_xlang_smtp_add_cap(pArr, iCaps, XSMTP_CAP_STARTTLS, "STARTTLS");
	xmail_xlang_smtp_add_cap(pArr, iCaps, XSMTP_CAP_8BITMIME, "8BITMIME");
	xmail_xlang_smtp_add_cap(pArr, iCaps, XSMTP_CAP_SMTPUTF8, "SMTPUTF8");
	xmail_xlang_smtp_add_cap(pArr, iCaps, XSMTP_CAP_SIZE, "SIZE");
	xmail_xlang_smtp_add_cap(pArr, iCaps, XSMTP_CAP_DSN, "DSN");
	return pArr;
}

static void xmail_xlang_smtp_config_from_value(xvalue pCfgVal, xsmtpconfig* pCfg)
{
	if ( pCfgVal != NULL && pCfgVal->Type == XVO_DT_TABLE ) {
		xvalue pPort = xmail_xlang_table_get(pCfgVal, "port");
		xvalue pTimeout = xmail_xlang_table_get(pCfgVal, "timeout_ms");
		xvalue pVerify = xmail_xlang_table_get(pCfgVal, "verify_peer");
		pCfg->sHost = xmail_xlang_text_at(pCfgVal, "host");
		if ( pPort != NULL && pPort->Type != XVO_DT_NULL ) pCfg->iPort = (uint16)xvoGetInt(pPort);
		if ( pTimeout != NULL && pTimeout->Type != XVO_DT_NULL ) pCfg->iTimeoutMs = (uint32)xvoGetInt(pTimeout);
		if ( pVerify != NULL && pVerify->Type != XVO_DT_NULL ) pCfg->bVerifyPeer = xvoGetBool(pVerify);
		pCfg->iTlsMaxVersion = xmail_xlang_tls_max_version_at(pCfgVal, "tls_max_version");
		pCfg->sCaFile = xmail_xlang_text_at(pCfgVal, "ca_file");
		pCfg->iSecureMode = xmail_xlang_smtp_secure_mode(xmail_xlang_text_at(pCfgVal, "secure"));
		pCfg->iAuthMode = xmail_xlang_smtp_auth_mode(xmail_xlang_text_at(pCfgVal, "auth"));
		pCfg->bAuth = pCfg->iAuthMode != XSMTP_AUTH_NONE;
		pCfg->sUser = xmail_xlang_text_at(pCfgVal, "user");
		pCfg->sPass = xmail_xlang_text_at(pCfgVal, "password");
		pCfg->sOAuth2Token = xmail_xlang_text_at(pCfgVal, "oauth2_token");
		pCfg->sHeloName = xmail_xlang_text_at(pCfgVal, "helo_name");
	}
}

static int xmail_xlang_pop3_secure_mode(const char* sSecure)
{
	if ( xmailMimeAsciiEqI(sSecure, "none") ) return XPOP3_SECURE_NONE;
	if ( xmailMimeAsciiEqI(sSecure, "ssl") ) return XPOP3_SECURE_SSL;
	if ( xmailMimeAsciiEqI(sSecure, "starttls") ) return XPOP3_SECURE_STARTTLS;
	return XPOP3_SECURE_AUTO;
}

static int xmail_xlang_pop3_auth_mode(const char* sAuth)
{
	if ( xmailMimeAsciiEqI(sAuth, "plain") ) return XPOP3_AUTH_PLAIN;
	if ( xmailMimeAsciiEqI(sAuth, "userpass") || xmailMimeAsciiEqI(sAuth, "password") ) return XPOP3_AUTH_USERPASS;
	return XPOP3_AUTH_AUTO;
}

static const char* xmail_xlang_pop3_stage_name(int iStage)
{
	switch ( iStage ) {
	case XPOP3_STAGE_CONNECT: return "connect";
	case XPOP3_STAGE_TLS: return "tls";
	case XPOP3_STAGE_GREETING: return "greeting";
	case XPOP3_STAGE_CAPA: return "capa";
	case XPOP3_STAGE_AUTH: return "auth";
	case XPOP3_STAGE_COMMAND: return "command";
	case XPOP3_STAGE_RETR: return "retr";
	case XPOP3_STAGE_QUIT: return "quit";
	default: return "";
	}
}

static void xmail_xlang_pop3_add_cap(xvalue pArr, uint32 iCaps, uint32 iCap, const char* sName)
{
	if ( iCaps & iCap ) {
		xvoArrayAppendText(pArr, (ptr)sName, (uint32)strlen(sName), FALSE);
	}
}

static xvalue xmail_xlang_pop3_caps_value(uint32 iCaps)
{
	xvalue pArr = xvoCreateArray();
	xmail_xlang_pop3_add_cap(pArr, iCaps, XPOP3_CAP_STLS, "STLS");
	xmail_xlang_pop3_add_cap(pArr, iCaps, XPOP3_CAP_UIDL, "UIDL");
	xmail_xlang_pop3_add_cap(pArr, iCaps, XPOP3_CAP_TOP, "TOP");
	xmail_xlang_pop3_add_cap(pArr, iCaps, XPOP3_CAP_USER, "USER");
	xmail_xlang_pop3_add_cap(pArr, iCaps, XPOP3_CAP_SASL, "SASL");
	return pArr;
}

static void xmail_xlang_pop3_config_from_value(xvalue pCfgVal, xpop3config* pCfg)
{
	if ( pCfgVal != NULL && pCfgVal->Type == XVO_DT_TABLE ) {
		xvalue pPort = xmail_xlang_table_get(pCfgVal, "port");
		xvalue pTimeout = xmail_xlang_table_get(pCfgVal, "timeout_ms");
		xvalue pVerify = xmail_xlang_table_get(pCfgVal, "verify_peer");
		pCfg->sHost = xmail_xlang_text_at(pCfgVal, "host");
		if ( pPort != NULL && pPort->Type != XVO_DT_NULL ) pCfg->iPort = (uint16)xvoGetInt(pPort);
		if ( pTimeout != NULL && pTimeout->Type != XVO_DT_NULL ) pCfg->iTimeoutMs = (uint32)xvoGetInt(pTimeout);
		if ( pVerify != NULL && pVerify->Type != XVO_DT_NULL ) pCfg->bVerifyPeer = xvoGetBool(pVerify);
		pCfg->iTlsMaxVersion = xmail_xlang_tls_max_version_at(pCfgVal, "tls_max_version");
		pCfg->sCaFile = xmail_xlang_text_at(pCfgVal, "ca_file");
		pCfg->iSecureMode = xmail_xlang_pop3_secure_mode(xmail_xlang_text_at(pCfgVal, "secure"));
		pCfg->iAuthMode = xmail_xlang_pop3_auth_mode(xmail_xlang_text_at(pCfgVal, "auth"));
		pCfg->sUser = xmail_xlang_text_at(pCfgVal, "user");
		pCfg->sPass = xmail_xlang_text_at(pCfgVal, "password");
	}
}

static xvalue xmail_xlang_pop3_result_value(bool bOk, const xpop3result* pRet, xvalue pData)
{
	xvalue pRetValue = xmail_xlang_result_value(bOk, pRet ? pRet->sError : "", pRet ? xmail_xlang_pop3_stage_name(pRet->iStage) : "", bOk ? "ok" : "err", pData);
	if ( pRet != NULL ) {
		xvoTableSetInt(pRetValue, "server_code", 0, pRet->iServerCode);
		xvoTableSetValue(pRetValue, "capabilities", 0, xmail_xlang_pop3_caps_value(pRet->iCapabilities), TRUE);
		xmail_xlang_table_set_text(pRetValue, "last_reply", pRet->sLastReply);
		xvoTableSetBool(pRetValue, "used_tls", 0, pRet->bUsedTLS);
		xvoTableSetBool(pRetValue, "used_starttls", 0, pRet->bUsedStartTLS);
	}
	return pRetValue;
}

static int xmail_xlang_imap_secure_mode(const char* sSecure)
{
	if ( xmailMimeAsciiEqI(sSecure, "none") ) return XIMAP_SECURE_NONE;
	if ( xmailMimeAsciiEqI(sSecure, "ssl") ) return XIMAP_SECURE_SSL;
	if ( xmailMimeAsciiEqI(sSecure, "starttls") ) return XIMAP_SECURE_STARTTLS;
	return XIMAP_SECURE_AUTO;
}

static const char* xmail_xlang_imap_stage_name(int iStage)
{
	switch ( iStage ) {
	case XIMAP_STAGE_CONNECT: return "connect";
	case XIMAP_STAGE_TLS: return "tls";
	case XIMAP_STAGE_GREETING: return "greeting";
	case XIMAP_STAGE_CAPABILITY: return "capability";
	case XIMAP_STAGE_AUTH: return "auth";
	case XIMAP_STAGE_SELECT: return "select";
	case XIMAP_STAGE_COMMAND: return "command";
	case XIMAP_STAGE_FETCH: return "fetch";
	case XIMAP_STAGE_LOGOUT: return "logout";
	case XIMAP_STAGE_IDLE: return "idle";
	default: return "";
	}
}

static const char* xmail_xlang_imap_status_name(int iStatus, bool bOk)
{
	switch ( iStatus ) {
	case XIMAP_STATUS_OK: return "ok";
	case XIMAP_STATUS_NO: return "no";
	case XIMAP_STATUS_BAD: return "bad";
	case XIMAP_STATUS_BYE: return "bye";
	case XIMAP_STATUS_PREAUTH: return "preauth";
	default: return bOk ? "ok" : "error";
	}
}

static void xmail_xlang_imap_add_cap(xvalue pArr, uint32 iCaps, uint32 iCap, const char* sName)
{
	if ( iCaps & iCap ) {
		xvoArrayAppendText(pArr, (ptr)sName, (uint32)strlen(sName), FALSE);
	}
}

static xvalue xmail_xlang_imap_caps_value(uint32 iCaps)
{
	xvalue pArr = xvoCreateArray();
	xmail_xlang_imap_add_cap(pArr, iCaps, XIMAP_CAP_IMAP4REV1, "IMAP4rev1");
	xmail_xlang_imap_add_cap(pArr, iCaps, XIMAP_CAP_STARTTLS, "STARTTLS");
	xmail_xlang_imap_add_cap(pArr, iCaps, XIMAP_CAP_IDLE, "IDLE");
	xmail_xlang_imap_add_cap(pArr, iCaps, XIMAP_CAP_UIDPLUS, "UIDPLUS");
	xmail_xlang_imap_add_cap(pArr, iCaps, XIMAP_CAP_XOAUTH2, "AUTH XOAUTH2");
	return pArr;
}

static void xmail_xlang_imap_config_from_value(xvalue pCfgVal, ximapconfig* pCfg)
{
	if ( pCfgVal != NULL && pCfgVal->Type == XVO_DT_TABLE ) {
		xvalue pPort = xmail_xlang_table_get(pCfgVal, "port");
		xvalue pTimeout = xmail_xlang_table_get(pCfgVal, "timeout_ms");
		xvalue pVerify = xmail_xlang_table_get(pCfgVal, "verify_peer");
		pCfg->sHost = xmail_xlang_text_at(pCfgVal, "host");
		if ( pPort != NULL && pPort->Type != XVO_DT_NULL ) pCfg->iPort = (uint16)xvoGetInt(pPort);
		if ( pTimeout != NULL && pTimeout->Type != XVO_DT_NULL ) pCfg->iTimeoutMs = (uint32)xvoGetInt(pTimeout);
		if ( pVerify != NULL && pVerify->Type != XVO_DT_NULL ) pCfg->bVerifyPeer = xvoGetBool(pVerify);
		pCfg->iTlsMaxVersion = xmail_xlang_tls_max_version_at(pCfgVal, "tls_max_version");
		pCfg->sCaFile = xmail_xlang_text_at(pCfgVal, "ca_file");
		pCfg->iSecureMode = xmail_xlang_imap_secure_mode(xmail_xlang_text_at(pCfgVal, "secure"));
		pCfg->sUser = xmail_xlang_text_at(pCfgVal, "user");
		pCfg->sPass = xmail_xlang_text_at(pCfgVal, "password");
		pCfg->sOAuth2Token = xmail_xlang_text_at(pCfgVal, "oauth2_token");
	}
}

static xvalue xmail_xlang_imap_result_value(bool bOk, const ximapresult* pRet, xvalue pData)
{
	xvalue pRetValue = xmail_xlang_result_value(bOk, pRet ? pRet->sError : "", pRet ? xmail_xlang_imap_stage_name(pRet->iStage) : "", pRet ? xmail_xlang_imap_status_name(pRet->iStatus, bOk) : (bOk ? "ok" : "error"), pData);
	if ( pRet != NULL ) {
		xvoTableSetValue(pRetValue, "capabilities", 0, xmail_xlang_imap_caps_value(pRet->iCapabilities), TRUE);
		xmail_xlang_table_set_text(pRetValue, "last_reply", pRet->sLastReply);
		xvoTableSetBool(pRetValue, "used_tls", 0, pRet->bUsedTLS);
		xvoTableSetBool(pRetValue, "used_starttls", 0, pRet->bUsedStartTLS);
	}
	return pRetValue;
}

static xvalue xmail_xlang_imap_mailbox_status_value(const ximapmailboxstatus* pStatus)
{
	xvalue pData = xvoCreateTable();
	xvoTableSetInt(pData, "messages", 0, pStatus ? (int64)pStatus->iExists : 0);
	xvoTableSetInt(pData, "exists", 0, pStatus ? (int64)pStatus->iExists : 0);
	xvoTableSetInt(pData, "recent", 0, pStatus ? (int64)pStatus->iRecent : 0);
	xvoTableSetInt(pData, "unseen", 0, pStatus ? (int64)pStatus->iUnseen : 0);
	xvoTableSetInt(pData, "uid_validity", 0, pStatus ? (int64)pStatus->iUidValidity : 0);
	xvoTableSetInt(pData, "uid_next", 0, pStatus ? (int64)pStatus->iUidNext : 0);
	xvoTableSetBool(pData, "read_only", 0, pStatus ? pStatus->bReadOnly : FALSE);
	xvoTableSetBool(pData, "read_write", 0, pStatus ? pStatus->bReadWrite : FALSE);
	return pData;
}

static xvalue xmail_xlang_imap_list_value(const ximaplist* pList)
{
	size_t i;
	xvalue pArr = xvoCreateArray();
	for ( i = 0u; pList != NULL && i < pList->iCount; i++ ) {
		const ximaplistitem* pItem = &pList->arrItems[i];
		xvalue pObj = xvoCreateTable();
		xmail_xlang_table_set_text(pObj, "mailbox", pItem->sMailbox);
		xmail_xlang_table_set_text(pObj, "delimiter", pItem->sDelimiter);
		xmail_xlang_table_set_text(pObj, "flags_raw", pItem->sFlags);
		xvoTableSetBool(pObj, "no_select", 0, pItem->bNoSelect);
		xvoTableSetBool(pObj, "has_no_children", 0, pItem->bHasNoChildren);
		xvoTableSetBool(pObj, "has_children", 0, pItem->bHasChildren);
		xvoTableSetBool(pObj, "marked", 0, pItem->bMarked);
		xvoTableSetBool(pObj, "unmarked", 0, pItem->bUnmarked);
		xvoArrayAppendValue(pArr, pObj, TRUE);
	}
	return pArr;
}

static xvalue xmail_xlang_imap_flags_value(const ximapflags* pFlags)
{
	xvalue pData = xvoCreateTable();
	xvoTableSetBool(pData, "seen", 0, pFlags ? pFlags->bSeen : FALSE);
	xvoTableSetBool(pData, "answered", 0, pFlags ? pFlags->bAnswered : FALSE);
	xvoTableSetBool(pData, "flagged", 0, pFlags ? pFlags->bFlagged : FALSE);
	xvoTableSetBool(pData, "deleted", 0, pFlags ? pFlags->bDeleted : FALSE);
	xvoTableSetBool(pData, "draft", 0, pFlags ? pFlags->bDraft : FALSE);
	xvoTableSetBool(pData, "recent", 0, pFlags ? pFlags->bRecent : FALSE);
	xmail_xlang_table_set_text(pData, "raw", pFlags ? pFlags->sRaw : "");
	return pData;
}

static xvalue xmail_xlang_imap_expunge_sequences_value(const ximapexpungeresult* pExpunge)
{
	size_t i;
	xvalue pArr = xvoCreateArray();
	for ( i = 0u; pExpunge != NULL && i < pExpunge->iCount; i++ ) {
		xvoArrayAppendInt(pArr, (int64)pExpunge->arrSequences[i]);
	}
	return pArr;
}

static xvalue xmail_xlang_imap_param_array(const ximapparamelem* arrParams, uint32 iCount)
{
	uint32 i;
	xvalue pArr = xvoCreateArray();
	for ( i = 0u; arrParams != NULL && i < iCount; i++ ) {
		xvalue pObj = xvoCreateTable();
		xmail_xlang_table_set_text(pObj, "name", arrParams[i].sName);
		xmail_xlang_table_set_text(pObj, "value", arrParams[i].sValue);
		xvoArrayAppendValue(pArr, pObj, TRUE);
	}
	return pArr;
}

static xvalue xmail_xlang_imap_bodystructure_value(const ximapbodystructure* pBody)
{
	uint32 i;
	xvalue pObj = xvoCreateTable();
	xvalue pChildren = xvoCreateArray();

	xvoTableSetBool(pObj, "multipart", 0, pBody ? pBody->bMultipart : FALSE);
	xvoTableSetInt(pObj, "part_count", 0, pBody ? (int64)pBody->iPartCount : 0);
	xmail_xlang_table_set_text(pObj, "type", pBody ? pBody->sType : "");
	xmail_xlang_table_set_text(pObj, "subtype", pBody ? pBody->sSubType : "");
	xmail_xlang_table_set_text(pObj, "id", pBody ? pBody->sId : "");
	xmail_xlang_table_set_text(pObj, "description", pBody ? pBody->sDescription : "");
	xmail_xlang_table_set_text(pObj, "encoding", pBody ? pBody->sEncoding : "");
	xvoTableSetInt(pObj, "octets", 0, pBody ? (int64)pBody->iOctets : 0);
	xvoTableSetInt(pObj, "lines", 0, pBody ? (int64)pBody->iLines : 0);
	xmail_xlang_table_set_text(pObj, "md5", pBody ? pBody->sMd5 : "");
	xmail_xlang_table_set_text(pObj, "disposition", pBody ? pBody->sDisposition : "");
	xmail_xlang_table_set_text(pObj, "language", pBody ? pBody->sLanguage : "");
	xmail_xlang_table_set_text(pObj, "location", pBody ? pBody->sLocation : "");
	xmail_xlang_table_set_text(pObj, "raw", pBody ? (const char*)pBody->sRaw : "");
	xmail_xlang_table_set_text(pObj, "envelope", pBody ? (const char*)pBody->sEnvelope : "");
	xvoTableSetInt(pObj, "message_lines", 0, pBody ? (int64)pBody->iMessageLines : 0);
	xvoTableSetValue(pObj, "params", 0, xmail_xlang_imap_param_array(pBody ? pBody->arrParams : NULL, pBody ? pBody->iParamCount : 0u), TRUE);
	xvoTableSetValue(pObj, "disposition_params", 0, xmail_xlang_imap_param_array(pBody ? pBody->arrDispositionParams : NULL, pBody ? pBody->iDispositionParamCount : 0u), TRUE);
	for ( i = 0u; pBody != NULL && pBody->arrChildren != NULL && i < pBody->iPartCount; i++ ) {
		xvoArrayAppendValue(pChildren, xmail_xlang_imap_bodystructure_value(&pBody->arrChildren[i]), TRUE);
	}
	xvoTableSetValue(pObj, "children", 0, pChildren, TRUE);
	if ( pBody != NULL && pBody->pMessageBody != NULL ) {
		xvoTableSetValue(pObj, "message_body", 0, xmail_xlang_imap_bodystructure_value(pBody->pMessageBody), TRUE);
	} else {
		xvoTableSetNull(pObj, "message_body", 0);
	}
	return pObj;
}

static bool xmail_xlang_parse_config_request(const char* sRequestJson, xvalue* ppRoot, xvalue* ppCfgVal, xvalue* ppReqVal)
{
	xvalue pRoot;
	xvalue pCfgVal;
	xvalue pReqVal;

	if ( ppRoot != NULL ) *ppRoot = NULL;
	if ( ppCfgVal != NULL ) *ppCfgVal = NULL;
	if ( ppReqVal != NULL ) *ppReqVal = NULL;
	pRoot = xrtParseJSON((str)xmailMimeStrOrEmpty(sRequestJson), strlen(xmailMimeStrOrEmpty(sRequestJson)));
	if ( pRoot == NULL || pRoot->Type != XVO_DT_TABLE ) {
		if ( pRoot != NULL ) xvoUnref(pRoot);
		return FALSE;
	}
	pCfgVal = xmail_xlang_table_get(pRoot, "config");
	pReqVal = xmail_xlang_table_get(pRoot, "request");
	if ( pCfgVal == NULL || pCfgVal->Type != XVO_DT_TABLE ) pCfgVal = pRoot;
	if ( pReqVal == NULL || pReqVal->Type != XVO_DT_TABLE ) pReqVal = pRoot;
	if ( ppRoot != NULL ) *ppRoot = pRoot;
	if ( ppCfgVal != NULL ) *ppCfgVal = pCfgVal;
	if ( ppReqVal != NULL ) *ppReqVal = pReqVal;
	return TRUE;
}

#endif

static xvalue xmail_xlang_header_array(const xmailheader* arrHeaders, size_t iCount)
{
	size_t i;
	xvalue pArr = xvoCreateArray();
	for ( i = 0u; i < iCount; i++ ) {
		xvalue pItem = xvoCreateTable();
		xvoTableSetText(pItem, "name", 0, (ptr)xmailMimeStrOrEmpty(arrHeaders[i].sName), (uint32)strlen(xmailMimeStrOrEmpty(arrHeaders[i].sName)), FALSE);
		xvoTableSetText(pItem, "value", 0, (ptr)xmailMimeStrOrEmpty(arrHeaders[i].sValue), (uint32)strlen(xmailMimeStrOrEmpty(arrHeaders[i].sValue)), FALSE);
		xvoArrayAppendValue(pArr, pItem, TRUE);
	}
	return pArr;
}

static xvalue xmail_xlang_part_value(const xmailmimepart* pPart);

static xvalue xmail_xlang_part_children_value(const xmailmimepart* pPart)
{
	size_t i;
	xvalue pArr = xvoCreateArray();
	for ( i = 0u; pPart != NULL && i < pPart->iChildCount; i++ ) {
		xvoArrayAppendValue(pArr, xmail_xlang_part_value(&pPart->arrChildren[i]), TRUE);
	}
	return pArr;
}

static xvalue xmail_xlang_part_value(const xmailmimepart* pPart)
{
	xvalue pObj = xvoCreateTable();
	if ( pPart == NULL ) {
		return pObj;
	}
	xmail_xlang_table_set_text(pObj, "content_type", pPart->sContentType);
	xmail_xlang_table_set_text(pObj, "media_type", pPart->sMediaType);
	xmail_xlang_table_set_text(pObj, "sub_type", pPart->sSubType);
	xmail_xlang_table_set_text(pObj, "content_disposition", pPart->sDisposition);
	xmail_xlang_table_set_text(pObj, "filename", pPart->sFileName);
	xmail_xlang_table_set_text(pObj, "content_id", pPart->sContentId);
	xmail_xlang_table_set_text(pObj, "encoding", pPart->sTransferEncoding);
	xvoTableSetBool(pObj, "attachment", 0, pPart->bAttachment);
	xvoTableSetBool(pObj, "inline", 0, pPart->bInline);
	xvoTableSetText(pObj, "body", 0, (ptr)xmailMimeStrOrEmpty((const char*)pPart->sDecodedBody), (uint32)strlen(xmailMimeStrOrEmpty((const char*)pPart->sDecodedBody)), FALSE);
	xvoTableSetValue(pObj, "headers", 0, xmail_xlang_header_array(pPart->arrHeaders, pPart->iHeaderCount), TRUE);
	xvoTableSetValue(pObj, "children", 0, xmail_xlang_part_children_value(pPart), TRUE);
	return pObj;
}

static void xmail_xlang_find_text_html(const xmailmimepart* pPart, const char** psText, const char** psHtml)
{
	size_t i;
	if ( pPart == NULL ) {
		return;
	}
	if ( !pPart->bAttachment && !pPart->bInline && pPart->iChildCount == 0u ) {
		if ( *psText == NULL && xmailMimeAsciiEqI(pPart->sMediaType, "text") && xmailMimeAsciiEqI(pPart->sSubType, "plain") ) {
			*psText = xmailMimeStrOrEmpty((const char*)pPart->sDecodedBody);
		}
		if ( *psHtml == NULL && xmailMimeAsciiEqI(pPart->sMediaType, "text") && xmailMimeAsciiEqI(pPart->sSubType, "html") ) {
			*psHtml = xmailMimeStrOrEmpty((const char*)pPart->sDecodedBody);
		}
	}
	for ( i = 0u; i < pPart->iChildCount; i++ ) {
		xmail_xlang_find_text_html(&pPart->arrChildren[i], psText, psHtml);
	}
}

static void xmail_xlang_collect_attachments(const xmailmimepart* pPart, xvalue pArr)
{
	size_t i;
	if ( pPart == NULL ) {
		return;
	}
	if ( pPart->bAttachment || pPart->bInline ) {
		xvalue pItem = xvoCreateTable();
		xmail_xlang_table_set_text(pItem, "filename", pPart->sFileName);
		xmail_xlang_table_set_text(pItem, "content_type", pPart->sContentType);
		xmail_xlang_table_set_text(pItem, "content_id", pPart->sContentId);
		xvoTableSetBool(pItem, "inline", 0, pPart->bInline);
		xvoTableSetInt(pItem, "size", 0, (int64)strlen(xmailMimeStrOrEmpty((const char*)pPart->sDecodedBody)));
		xvoArrayAppendValue(pArr, pItem, TRUE);
	}
	for ( i = 0u; i < pPart->iChildCount; i++ ) {
		xmail_xlang_collect_attachments(&pPart->arrChildren[i], pArr);
	}
}

static xvalue xmail_xlang_parsed_message_data(const xmailparsedmessage* pParsed)
{
	xvalue pData;
	xvalue pAttachments;
	const char* sText = NULL;
	const char* sHtml = NULL;

	xmail_xlang_find_text_html(pParsed ? pParsed->pRootPart : NULL, &sText, &sHtml);
	pAttachments = xvoCreateArray();
	xmail_xlang_collect_attachments(pParsed ? pParsed->pRootPart : NULL, pAttachments);
	pData = xvoCreateTable();
	xvoTableSetValue(pData, "headers", 0, xmail_xlang_header_array(pParsed ? pParsed->arrHeaders : NULL, pParsed ? pParsed->iHeaderCount : 0u), TRUE);
	xvoTableSetText(pData, "subject", 0, (ptr)xmailMimeStrOrEmpty(pParsed ? xmailMimeParsedGetHeader(pParsed, "Subject") : ""), (uint32)strlen(xmailMimeStrOrEmpty(pParsed ? xmailMimeParsedGetHeader(pParsed, "Subject") : "")), FALSE);
	xvoTableSetText(pData, "date", 0, (ptr)xmailMimeStrOrEmpty(pParsed ? xmailMimeParsedGetHeader(pParsed, "Date") : ""), (uint32)strlen(xmailMimeStrOrEmpty(pParsed ? xmailMimeParsedGetHeader(pParsed, "Date") : "")), FALSE);
	xvoTableSetText(pData, "message_id", 0, (ptr)xmailMimeStrOrEmpty(pParsed ? xmailMimeParsedGetHeader(pParsed, "Message-ID") : ""), (uint32)strlen(xmailMimeStrOrEmpty(pParsed ? xmailMimeParsedGetHeader(pParsed, "Message-ID") : "")), FALSE);
	xvoTableSetText(pData, "from_raw", 0, (ptr)xmailMimeStrOrEmpty(pParsed ? xmailMimeParsedGetHeader(pParsed, "From") : ""), (uint32)strlen(xmailMimeStrOrEmpty(pParsed ? xmailMimeParsedGetHeader(pParsed, "From") : "")), FALSE);
	xvoTableSetText(pData, "to_raw", 0, (ptr)xmailMimeStrOrEmpty(pParsed ? xmailMimeParsedGetHeader(pParsed, "To") : ""), (uint32)strlen(xmailMimeStrOrEmpty(pParsed ? xmailMimeParsedGetHeader(pParsed, "To") : "")), FALSE);
	xvoTableSetText(pData, "cc_raw", 0, (ptr)xmailMimeStrOrEmpty(pParsed ? xmailMimeParsedGetHeader(pParsed, "Cc") : ""), (uint32)strlen(xmailMimeStrOrEmpty(pParsed ? xmailMimeParsedGetHeader(pParsed, "Cc") : "")), FALSE);
	xvoTableSetText(pData, "text", 0, (ptr)xmailMimeStrOrEmpty(sText), (uint32)strlen(xmailMimeStrOrEmpty(sText)), FALSE);
	xvoTableSetText(pData, "html", 0, (ptr)xmailMimeStrOrEmpty(sHtml), (uint32)strlen(xmailMimeStrOrEmpty(sHtml)), FALSE);
	xvoTableSetValue(pData, "parts", 0, (pParsed != NULL && pParsed->pRootPart != NULL) ? xmail_xlang_part_children_value(pParsed->pRootPart) : xvoCreateArray(), TRUE);
	xvoTableSetValue(pData, "root_part", 0, xmail_xlang_part_value(pParsed ? pParsed->pRootPart : NULL), TRUE);
	xvoTableSetValue(pData, "attachments", 0, pAttachments, TRUE);
	return pData;
}

#if !defined(XMAIL_XLANG_PROTOCOL_ONLY)
const char* xmail_mime_build_json_native(const char* sMessageJson)
{
	xvalue pRoot;
	xvalue pData;
	xmailmessage tMsg;
	str sRaw;
	xmailaddr* arrTo = NULL;
	xmailaddr* arrCc = NULL;
	xmailaddr* arrBcc = NULL;
	xmailheader* arrHeaders = NULL;
	xmailmimeattachment* arrAttachments = NULL;

	memset(&tMsg, 0, sizeof(tMsg));
	pRoot = xrtParseJSON((str)xmailMimeStrOrEmpty(sMessageJson), strlen(xmailMimeStrOrEmpty(sMessageJson)));
	if ( pRoot == NULL || pRoot->Type != XVO_DT_TABLE ) {
		if ( pRoot != NULL ) xvoUnref(pRoot);
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "message_json must be a JSON object", "json", "error", NULL));
	}
	xmail_xlang_fill_addr_from_value(xmail_xlang_table_get(pRoot, "from"), &tMsg.tFrom);
	xmail_xlang_fill_reply_to(pRoot, &tMsg.tReplyTo);
	arrTo = xmail_xlang_addr_array_from_value(xmail_xlang_table_get(pRoot, "to"), &tMsg.iToCount);
	arrCc = xmail_xlang_addr_array_from_value(xmail_xlang_table_get(pRoot, "cc"), &tMsg.iCcCount);
	arrBcc = xmail_xlang_addr_array_from_value(xmail_xlang_table_get(pRoot, "bcc"), &tMsg.iBccCount);
	arrHeaders = xmail_xlang_headers_from_value(xmail_xlang_table_get(pRoot, "headers"), &tMsg.iHeaderCount);
	arrAttachments = xmail_xlang_attachments_from_value(xmail_xlang_table_get(pRoot, "attachments"), &tMsg.iAttachmentCount);
	tMsg.arrTo = arrTo;
	tMsg.arrCc = arrCc;
	tMsg.arrBcc = arrBcc;
	tMsg.arrHeaders = arrHeaders;
	tMsg.arrAttachments = arrAttachments;
	tMsg.sSubject = xmail_xlang_text_at(pRoot, "subject");
	tMsg.sTextBody = xmail_xlang_text_at(pRoot, "text");
	tMsg.sHtmlBody = xmail_xlang_text_at(pRoot, "html");
	tMsg.sMessageIdDomain = xmail_xlang_text_at(pRoot, "message_id_domain");
	sRaw = xmailMimeBuildMessage(&tMsg);
	if ( arrTo != NULL ) xrtFree(arrTo);
	if ( arrCc != NULL ) xrtFree(arrCc);
	if ( arrBcc != NULL ) xrtFree(arrBcc);
	if ( arrHeaders != NULL ) xrtFree(arrHeaders);
	if ( arrAttachments != NULL ) xrtFree(arrAttachments);
	if ( sRaw == NULL ) {
		xvoUnref(pRoot);
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "MIME build failed", "mime_build", "error", NULL));
	}
	pData = xvoCreateTable();
	xvoTableSetText(pData, "raw", 0, (ptr)sRaw, (uint32)strlen((const char*)sRaw), FALSE);
	xvoTableSetText(pData, "message", 0, (ptr)sRaw, (uint32)strlen((const char*)sRaw), FALSE);
	xmailMimeFree(sRaw);
	xvoUnref(pRoot);
	return xmail_xlang_return_value(xmail_xlang_result_value(TRUE, "", "mime_build", "ok", pData));
}

const char* xmail_mime_parse_json_native(const char* sRawMessage)
{
	xmailparsedmessage tParsed;
	xvalue pData;
	bool bOk;

	memset(&tParsed, 0, sizeof(tParsed));
	bOk = xmailMimeParseMessage(xmailMimeStrOrEmpty(sRawMessage), &tParsed);
	if ( !bOk ) {
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "MIME parse failed", "mime_parse", "error", NULL));
	}
	pData = xmail_xlang_parsed_message_data(&tParsed);
	xmailMimeParsedMessageFree(&tParsed);
	return xmail_xlang_return_value(xmail_xlang_result_value(TRUE, "", "mime_parse", "ok", pData));
}

XMAIL_XLANG_API xvalue xmail_mime_build_native(xvalue pMessageValue)
{
	str sJson;
	size_t iSize = 0u;
	xvalue pRet;

	if ( pMessageValue == NULL ) {
		return xmail_xlang_result_value(FALSE, "message must be an object", "json", "error", NULL);
	}
	sJson = xrtStringifyJSON(pMessageValue, 0, &iSize);
	if ( sJson == NULL ) {
		return xmail_xlang_result_value(FALSE, "message JSON stringify failed", "json", "error", NULL);
	}
	pRet = xmail_xlang_parse_result_json(xmail_mime_build_json_native((const char*)sJson), "mime_build");
	xrtFree(sJson);
	return pRet;
}

XMAIL_XLANG_API xvalue xmail_mime_parse_native(const char* sRawMessage)
{
	return xmail_xlang_parse_result_json(xmail_mime_parse_json_native(sRawMessage), "mime_parse");
}
#endif

#if defined(XMAIL_XLANG_WITH_PROTOCOLS)
#if defined(XMAIL_XLANG_WITH_SMTP)
const char* xmail_smtp_send_json_native(const char* sRequestJson)
{
	xvalue pRoot;
	xvalue pCfgVal;
	xvalue pMsgVal;
	xvalue pDsnVal;
	xvalue pData;
	xvalue pRetValue;
	xsmtpconfig tCfg;
	xsmtpmessage tMsg;
	xsmtpresult tRet;
	xsmtpaddr* arrTo = NULL;
	xsmtpaddr* arrCc = NULL;
	xsmtpaddr* arrBcc = NULL;
	xsmtpattachment* arrAttachments = NULL;
	const char** arrHeaderNames = NULL;
	const char** arrHeaderValues = NULL;
	const char* sDsnNotify = "";
	const char* sDsnOrcpt = "";
	bool bSendOk;

	xrtSmtpConfigInit(&tCfg);
	xrtSmtpMessageInit(&tMsg);
	xrtSmtpResultInit(&tRet);
	pRoot = xrtParseJSON((str)xmailMimeStrOrEmpty(sRequestJson), strlen(xmailMimeStrOrEmpty(sRequestJson)));
	if ( pRoot == NULL || pRoot->Type != XVO_DT_TABLE ) {
		if ( pRoot != NULL ) xvoUnref(pRoot);
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "request_json must be a JSON object", "json", "error", NULL));
	}
	pCfgVal = xmail_xlang_table_get(pRoot, "config");
	pMsgVal = xmail_xlang_table_get(pRoot, "message");
	pDsnVal = xmail_xlang_table_get(pRoot, "dsn");
	if ( pCfgVal != NULL && pCfgVal->Type == XVO_DT_TABLE ) {
		xmail_xlang_smtp_config_from_value(pCfgVal, &tCfg);
	}
	if ( pDsnVal != NULL && pDsnVal->Type == XVO_DT_TABLE ) {
		tMsg.sDsnReturn = xmail_xlang_text_at(pDsnVal, "ret");
		tMsg.sDsnEnvid = xmail_xlang_text_at(pDsnVal, "envid");
		sDsnNotify = xmail_xlang_text_at(pDsnVal, "notify");
		sDsnOrcpt = xmail_xlang_text_at(pDsnVal, "orcpt");
	}
	if ( pMsgVal != NULL && pMsgVal->Type == XVO_DT_TABLE ) {
		xmail_xlang_fill_addr_from_value(xmail_xlang_table_get(pMsgVal, "from"), (xmailaddr*)&tMsg.tFrom);
		xmail_xlang_fill_reply_to(pMsgVal, (xmailaddr*)&tMsg.tReplyTo);
		arrTo = xmail_xlang_smtp_addr_array_from_value(xmail_xlang_table_get(pMsgVal, "to"), &tMsg.iToCount, sDsnNotify, sDsnOrcpt);
		arrCc = xmail_xlang_smtp_addr_array_from_value(xmail_xlang_table_get(pMsgVal, "cc"), &tMsg.iCcCount, sDsnNotify, sDsnOrcpt);
		arrBcc = xmail_xlang_smtp_addr_array_from_value(xmail_xlang_table_get(pMsgVal, "bcc"), &tMsg.iBccCount, sDsnNotify, sDsnOrcpt);
		arrAttachments = xmail_xlang_smtp_attachments_from_value(xmail_xlang_table_get(pMsgVal, "attachments"), &tMsg.iAttachmentCount);
		xmail_xlang_smtp_headers_from_value(xmail_xlang_table_get(pMsgVal, "headers"), &arrHeaderNames, &arrHeaderValues, &tMsg.iHeaderCount);
		tMsg.arrTo = arrTo;
		tMsg.arrCc = arrCc;
		tMsg.arrBcc = arrBcc;
		tMsg.arrAttachments = arrAttachments;
		tMsg.arrHeaderNames = arrHeaderNames;
		tMsg.arrHeaderValues = arrHeaderValues;
		tMsg.sSubject = xmail_xlang_text_at(pMsgVal, "subject");
		tMsg.sTextBody = xmail_xlang_text_at(pMsgVal, "text");
		tMsg.sHtmlBody = xmail_xlang_text_at(pMsgVal, "html");
		tCfg.tFrom = tMsg.tFrom;
		tCfg.tReplyTo = tMsg.tReplyTo;
	}
	bSendOk = xrtSmtpSendMail(&tCfg, &tMsg, &tRet);
	if ( arrTo != NULL ) xrtFree(arrTo);
	if ( arrCc != NULL ) xrtFree(arrCc);
	if ( arrBcc != NULL ) xrtFree(arrBcc);
	if ( arrAttachments != NULL ) xrtFree(arrAttachments);
	if ( arrHeaderNames != NULL ) xrtFree((ptr)arrHeaderNames);
	if ( arrHeaderValues != NULL ) xrtFree((ptr)arrHeaderValues);
	xvoUnref(pRoot);
	pData = xvoCreateTable();
	xvoTableSetText(pData, "auth_mode", 0, (ptr)xmail_xlang_smtp_auth_name(tRet.iAuthMode), (uint32)strlen(xmail_xlang_smtp_auth_name(tRet.iAuthMode)), FALSE);
	xvoTableSetInt(pData, "size_limit", 0, (int64)tRet.iSizeLimit);
	xvoTableSetValue(pData, "capabilities", 0, xmail_xlang_smtp_caps_value(tRet.iCapabilities), TRUE);
	pRetValue = xmail_xlang_result_value(bSendOk, tRet.sError, xmail_xlang_smtp_stage_name(tRet.iStage), bSendOk ? "ok" : "error", pData);
	xvoTableSetInt(pRetValue, "server_code", 0, tRet.iServerCode);
	xvoTableSetValue(pRetValue, "capabilities", 0, xmail_xlang_smtp_caps_value(tRet.iCapabilities), TRUE);
	xvoTableSetText(pRetValue, "last_reply", 0, tRet.sLastReply, (uint32)strlen(tRet.sLastReply), FALSE);
	xvoTableSetBool(pRetValue, "used_tls", 0, tRet.bUsedTLS);
	xvoTableSetBool(pRetValue, "used_starttls", 0, tRet.bUsedStartTLS);
	return xmail_xlang_return_value(pRetValue);
}

const char* xmail_smtp_capability_json_native(const char* sConfigJson)
{
	xvalue pRoot;
	xvalue pData;
	xvalue pRetValue;
	xsmtpconfig tCfg;
	xsmtpresult tRet;
	bool bOk;

	xrtSmtpConfigInit(&tCfg);
	xrtSmtpResultInit(&tRet);
	pRoot = xrtParseJSON((str)xmailMimeStrOrEmpty(sConfigJson), strlen(xmailMimeStrOrEmpty(sConfigJson)));
	if ( pRoot == NULL || pRoot->Type != XVO_DT_TABLE ) {
		if ( pRoot != NULL ) xvoUnref(pRoot);
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "config_json must be a JSON object", "json", "error", NULL));
	}
	xmail_xlang_smtp_config_from_value(pRoot, &tCfg);
	bOk = xrtSmtpCapability(&tCfg, &tRet);
	pData = xvoCreateTable();
	xvoTableSetInt(pData, "capability_mask", 0, (int64)tRet.iCapabilities);
	xvoTableSetValue(pData, "capabilities", 0, xmail_xlang_smtp_caps_value(tRet.iCapabilities), TRUE);
	xvoTableSetInt(pData, "size_limit", 0, (int64)tRet.iSizeLimit);
	pRetValue = xmail_xlang_result_value(bOk, tRet.sError, xmail_xlang_smtp_stage_name(tRet.iStage), bOk ? "ok" : "error", pData);
	xvoTableSetInt(pRetValue, "server_code", 0, tRet.iServerCode);
	xvoTableSetValue(pRetValue, "capabilities", 0, xmail_xlang_smtp_caps_value(tRet.iCapabilities), TRUE);
	xvoTableSetText(pRetValue, "last_reply", 0, tRet.sLastReply, (uint32)strlen(tRet.sLastReply), FALSE);
	xvoTableSetBool(pRetValue, "used_tls", 0, tRet.bUsedTLS);
	xvoTableSetBool(pRetValue, "used_starttls", 0, tRet.bUsedStartTLS);
	xvoUnref(pRoot);
	return xmail_xlang_return_value(pRetValue);
}
#endif

#if defined(XMAIL_XLANG_WITH_POP3)
const char* xmail_pop3_capability_json_native(const char* sConfigJson)
{
	xvalue pRoot;
	xvalue pData;
	xpop3config tCfg;
	xpop3result tRet;
	xpop3client* pClient = NULL;
	uint32 iCaps = 0u;
	bool bOk = FALSE;

	xrtPop3ConfigInit(&tCfg);
	xrtPop3ResultInit(&tRet);
	pRoot = xrtParseJSON((str)xmailMimeStrOrEmpty(sConfigJson), strlen(xmailMimeStrOrEmpty(sConfigJson)));
	if ( pRoot == NULL || pRoot->Type != XVO_DT_TABLE ) {
		if ( pRoot != NULL ) xvoUnref(pRoot);
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "config_json must be a JSON object", "json", "error", NULL));
	}
	xmail_xlang_pop3_config_from_value(pRoot, &tCfg);
	if ( xrtPop3Open(&tCfg, &pClient, &tRet) ) {
		bOk = xrtPop3Capa(pClient, &iCaps, &tRet);
		if ( bOk ) {
			(void)xrtPop3Quit(pClient, &tRet);
			pClient = NULL;
		}
	}
	if ( pClient != NULL ) {
		xrtPop3Close(pClient);
	}
	pData = xvoCreateTable();
	xvoTableSetInt(pData, "capability_mask", 0, (int64)iCaps);
	xvoTableSetValue(pData, "capabilities", 0, xmail_xlang_pop3_caps_value(iCaps), TRUE);
	xvoUnref(pRoot);
	return xmail_xlang_return_value(xmail_xlang_pop3_result_value(bOk, &tRet, pData));
}

const char* xmail_pop3_status_json_native(const char* sConfigJson)
{
	xvalue pRoot;
	xvalue pData;
	xpop3config tCfg;
	xpop3result tRet;
	xpop3client* pClient = NULL;
	xpop3stat tStat;
	bool bOk = FALSE;

	memset(&tStat, 0, sizeof(tStat));
	xrtPop3ConfigInit(&tCfg);
	xrtPop3ResultInit(&tRet);
	pRoot = xrtParseJSON((str)xmailMimeStrOrEmpty(sConfigJson), strlen(xmailMimeStrOrEmpty(sConfigJson)));
	if ( pRoot == NULL || pRoot->Type != XVO_DT_TABLE ) {
		if ( pRoot != NULL ) xvoUnref(pRoot);
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "config_json must be a JSON object", "json", "error", NULL));
	}
	xmail_xlang_pop3_config_from_value(pRoot, &tCfg);
	if ( xrtPop3Open(&tCfg, &pClient, &tRet) ) {
		bOk = xrtPop3Stat(pClient, &tStat, &tRet);
		if ( bOk ) {
			(void)xrtPop3Quit(pClient, &tRet);
			pClient = NULL;
		}
	}
	if ( pClient != NULL ) {
		xrtPop3Close(pClient);
	}
	pData = xvoCreateTable();
	xvoTableSetInt(pData, "count", 0, (int64)tStat.iCount);
	xvoTableSetInt(pData, "messages", 0, (int64)tStat.iCount);
	xvoTableSetInt(pData, "size", 0, (int64)tStat.iSize);
	xvoUnref(pRoot);
	return xmail_xlang_return_value(xmail_xlang_pop3_result_value(bOk, &tRet, pData));
}

const char* xmail_pop3_list_json_native(const char* sConfigJson)
{
	xvalue pRoot;
	xvalue pData;
	xvalue pMessages;
	xpop3config tCfg;
	xpop3result tRet;
	xpop3client* pClient = NULL;
	xpop3summary* arrItems = NULL;
	size_t iCount = 0u;
	size_t i;
	bool bOk = FALSE;

	xrtPop3ConfigInit(&tCfg);
	xrtPop3ResultInit(&tRet);
	pRoot = xrtParseJSON((str)xmailMimeStrOrEmpty(sConfigJson), strlen(xmailMimeStrOrEmpty(sConfigJson)));
	if ( pRoot == NULL || pRoot->Type != XVO_DT_TABLE ) {
		if ( pRoot != NULL ) xvoUnref(pRoot);
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "config_json must be a JSON object", "json", "error", NULL));
	}
	xmail_xlang_pop3_config_from_value(pRoot, &tCfg);
	if ( xrtPop3Open(&tCfg, &pClient, &tRet) && xrtPop3ListSummaries(pClient, &arrItems, &iCount, &tRet) ) {
		bOk = TRUE;
		(void)xrtPop3Quit(pClient, &tRet);
		pClient = NULL;
	}
	if ( pClient != NULL ) {
		xrtPop3Close(pClient);
	}
	pMessages = xvoCreateArray();
	for ( i = 0u; i < iCount; i++ ) {
		xvalue pItem = xvoCreateTable();
		xvoTableSetInt(pItem, "number", 0, arrItems[i].iNumber);
		xvoTableSetText(pItem, "uid", 0, arrItems[i].sUid, (uint32)strlen(arrItems[i].sUid), FALSE);
		xvoTableSetInt(pItem, "size", 0, (int64)arrItems[i].iSize);
		xvoTableSetText(pItem, "subject", 0, arrItems[i].sSubject, (uint32)strlen(arrItems[i].sSubject), FALSE);
		xvoTableSetText(pItem, "from", 0, arrItems[i].sFrom, (uint32)strlen(arrItems[i].sFrom), FALSE);
		xvoTableSetText(pItem, "date", 0, arrItems[i].sDate, (uint32)strlen(arrItems[i].sDate), FALSE);
		xvoArrayAppendValue(pMessages, pItem, TRUE);
	}
	pData = xvoCreateTable();
	xvoTableSetValue(pData, "messages", 0, pMessages, TRUE);
	xvoTableSetInt(pData, "count", 0, (int64)iCount);
	xrtPop3SummariesFree(arrItems);
	xvoUnref(pRoot);
	return xmail_xlang_return_value(xmail_xlang_pop3_result_value(bOk, &tRet, pData));
}

const char* xmail_pop3_fetch_json_native(const char* sRequestJson)
{
	xvalue pRoot;
	xvalue pCfgVal;
	xvalue pReqVal;
	xvalue pData = NULL;
	xpop3config tCfg;
	xpop3result tRet;
	xpop3client* pClient = NULL;
	xmailparsedmessage tParsed;
	str sRaw = NULL;
	uint32 iNumber = 0u;
	uint32 iTopLineCount = 0u;
	bool bParseMime = FALSE;
	bool bDeleteAfterRetr = FALSE;
	bool bUseTop = FALSE;
	bool bOk = FALSE;

	memset(&tParsed, 0, sizeof(tParsed));
	xrtPop3ConfigInit(&tCfg);
	xrtPop3ResultInit(&tRet);
	pRoot = xrtParseJSON((str)xmailMimeStrOrEmpty(sRequestJson), strlen(xmailMimeStrOrEmpty(sRequestJson)));
	if ( pRoot == NULL || pRoot->Type != XVO_DT_TABLE ) {
		if ( pRoot != NULL ) xvoUnref(pRoot);
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "request_json must be a JSON object", "json", "error", NULL));
	}
	pCfgVal = xmail_xlang_table_get(pRoot, "config");
	pReqVal = xmail_xlang_table_get(pRoot, "request");
	if ( pCfgVal == NULL || pCfgVal->Type != XVO_DT_TABLE ) pCfgVal = pRoot;
	if ( pReqVal == NULL || pReqVal->Type != XVO_DT_TABLE ) pReqVal = pRoot;
	xmail_xlang_pop3_config_from_value(pCfgVal, &tCfg);
	if ( pReqVal != NULL && pReqVal->Type == XVO_DT_TABLE ) {
		xvalue pNumber = xmail_xlang_table_get(pReqVal, "number");
		xvalue pTopLines = xmail_xlang_table_get(pReqVal, "top_lines");
		if ( pNumber != NULL && pNumber->Type != XVO_DT_NULL ) iNumber = (uint32)xvoGetInt(pNumber);
		if ( pTopLines != NULL && pTopLines->Type != XVO_DT_NULL ) {
			iTopLineCount = (uint32)xvoGetInt(pTopLines);
			bUseTop = TRUE;
		}
		bParseMime = xmail_xlang_bool_at(pReqVal, "parse_mime");
		bDeleteAfterRetr = xmail_xlang_bool_at(pReqVal, "delete_after_retr");
		if ( xmail_xlang_bool_at(pReqVal, "top") ) bUseTop = TRUE;
	}
	if ( iNumber == 0u ) {
		xvoUnref(pRoot);
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "POP3 message number is required", "request", "error", NULL));
	}
	if ( xrtPop3Open(&tCfg, &pClient, &tRet) ) {
		if ( bParseMime ) {
			bOk = bUseTop
				? xrtPop3TopMime(pClient, iNumber, iTopLineCount, &tParsed, &tRet)
				: xrtPop3RetrMime(pClient, iNumber, &tParsed, &tRet);
			if ( bOk ) {
				pData = xmail_xlang_parsed_message_data(&tParsed);
			}
		} else {
			bOk = bUseTop
				? xrtPop3TopRaw(pClient, iNumber, iTopLineCount, &sRaw, &tRet)
				: xrtPop3RetrRaw(pClient, iNumber, &sRaw, &tRet);
			if ( bOk ) {
				pData = xvoCreateTable();
				xvoTableSetText(pData, "raw", 0, sRaw, (uint32)strlen((const char*)sRaw), FALSE);
			}
		}
		if ( bOk && !bUseTop && bDeleteAfterRetr ) {
			bOk = xrtPop3Delete(pClient, iNumber, &tRet);
		}
		if ( bOk ) {
			(void)xrtPop3Quit(pClient, &tRet);
			pClient = NULL;
		}
	}
	if ( pClient != NULL ) {
		xrtPop3Close(pClient);
	}
	if ( pData == NULL ) {
		pData = xvoCreateTable();
	}
	xvoTableSetInt(pData, "number", 0, iNumber);
	xvoTableSetBool(pData, "top", 0, bUseTop);
	xvoTableSetBool(pData, "parse_mime", 0, bParseMime);
	if ( sRaw != NULL ) {
		xrtFree(sRaw);
	}
	xmailMimeParsedMessageFree(&tParsed);
	xvoUnref(pRoot);
	return xmail_xlang_return_value(xmail_xlang_pop3_result_value(bOk, &tRet, pData));
}

const char* xmail_pop3_delete_json_native(const char* sRequestJson)
{
	xvalue pRoot;
	xvalue pCfgVal;
	xvalue pReqVal;
	xvalue pData;
	xpop3config tCfg;
	xpop3result tRet;
	xpop3client* pClient = NULL;
	uint32 iNumber = 0u;
	bool bOk = FALSE;

	xrtPop3ConfigInit(&tCfg);
	xrtPop3ResultInit(&tRet);
	pRoot = xrtParseJSON((str)xmailMimeStrOrEmpty(sRequestJson), strlen(xmailMimeStrOrEmpty(sRequestJson)));
	if ( pRoot == NULL || pRoot->Type != XVO_DT_TABLE ) {
		if ( pRoot != NULL ) xvoUnref(pRoot);
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "request_json must be a JSON object", "json", "error", NULL));
	}
	pCfgVal = xmail_xlang_table_get(pRoot, "config");
	pReqVal = xmail_xlang_table_get(pRoot, "request");
	if ( pCfgVal == NULL || pCfgVal->Type != XVO_DT_TABLE ) pCfgVal = pRoot;
	if ( pReqVal == NULL || pReqVal->Type != XVO_DT_TABLE ) pReqVal = pRoot;
	xmail_xlang_pop3_config_from_value(pCfgVal, &tCfg);
	if ( pReqVal != NULL && pReqVal->Type == XVO_DT_TABLE ) {
		xvalue pNumber = xmail_xlang_table_get(pReqVal, "number");
		if ( pNumber != NULL && pNumber->Type != XVO_DT_NULL ) iNumber = (uint32)xvoGetInt(pNumber);
	}
	if ( iNumber == 0u ) {
		xvoUnref(pRoot);
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "POP3 message number is required", "request", "error", NULL));
	}
	if ( xrtPop3Open(&tCfg, &pClient, &tRet) ) {
		bOk = xrtPop3Delete(pClient, iNumber, &tRet);
		if ( bOk ) {
			(void)xrtPop3Quit(pClient, &tRet);
			pClient = NULL;
		}
	}
	if ( pClient != NULL ) {
		xrtPop3Close(pClient);
	}
	pData = xvoCreateTable();
	xvoTableSetInt(pData, "number", 0, iNumber);
	xvoTableSetBool(pData, "deleted", 0, bOk);
	xvoUnref(pRoot);
	return xmail_xlang_return_value(xmail_xlang_pop3_result_value(bOk, &tRet, pData));
}
#endif

#if defined(XMAIL_XLANG_WITH_IMAP)
const char* xmail_imap_capability_json_native(const char* sRequestJson)
{
	xvalue pRoot = NULL;
	xvalue pCfgVal = NULL;
	xvalue pReqVal = NULL;
	xvalue pData;
	ximapconfig tCfg;
	ximapresult tRet;
	ximapclient* pClient = NULL;
	uint32 iCaps = 0u;
	bool bOk = FALSE;

	(void)pReqVal;
	xrtImapConfigInit(&tCfg);
	xrtImapResultInit(&tRet);
	if ( !xmail_xlang_parse_config_request(sRequestJson, &pRoot, &pCfgVal, &pReqVal) ) {
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "request_json must be a JSON object", "json", "error", NULL));
	}
	xmail_xlang_imap_config_from_value(pCfgVal, &tCfg);
	if ( xrtImapOpen(&tCfg, &pClient, &tRet) ) {
		bOk = xrtImapCapability(pClient, &iCaps, &tRet);
		if ( bOk ) {
			(void)xrtImapLogout(pClient, &tRet);
			pClient = NULL;
		}
	}
	if ( pClient != NULL ) {
		xrtImapLogout(pClient, &tRet);
		pClient = NULL;
	}
	pData = xvoCreateTable();
	xvoTableSetInt(pData, "capability_mask", 0, (int64)iCaps);
	xvoTableSetValue(pData, "capabilities", 0, xmail_xlang_imap_caps_value(iCaps), TRUE);
	xvoUnref(pRoot);
	return xmail_xlang_return_value(xmail_xlang_imap_result_value(bOk, &tRet, pData));
}

const char* xmail_imap_status_json_native(const char* sRequestJson)
{
	xvalue pRoot = NULL;
	xvalue pCfgVal = NULL;
	xvalue pReqVal = NULL;
	xvalue pData;
	ximapconfig tCfg;
	ximapresult tRet;
	ximapclient* pClient = NULL;
	ximapmailboxstatus tStatus;
	const char* sMailbox = "INBOX";
	bool bOk = FALSE;

	memset(&tStatus, 0, sizeof(tStatus));
	xrtImapConfigInit(&tCfg);
	xrtImapResultInit(&tRet);
	if ( !xmail_xlang_parse_config_request(sRequestJson, &pRoot, &pCfgVal, &pReqVal) ) {
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "request_json must be a JSON object", "json", "error", NULL));
	}
	xmail_xlang_imap_config_from_value(pCfgVal, &tCfg);
	if ( pReqVal != NULL && pReqVal->Type == XVO_DT_TABLE && xmail_xlang_text_at(pReqVal, "mailbox")[0] != '\0' ) {
		sMailbox = xmail_xlang_text_at(pReqVal, "mailbox");
	}
	if ( xrtImapOpen(&tCfg, &pClient, &tRet) ) {
		bOk = xrtImapStatus(pClient, sMailbox, &tStatus, &tRet);
		if ( bOk ) {
			(void)xrtImapLogout(pClient, &tRet);
			pClient = NULL;
		}
	}
	if ( pClient != NULL ) {
		xrtImapLogout(pClient, &tRet);
		pClient = NULL;
	}
	pData = xmail_xlang_imap_mailbox_status_value(&tStatus);
	xvoTableSetText(pData, "mailbox", 0, (ptr)sMailbox, (uint32)strlen(sMailbox), FALSE);
	xvoUnref(pRoot);
	return xmail_xlang_return_value(xmail_xlang_imap_result_value(bOk, &tRet, pData));
}

const char* xmail_imap_list_json_native(const char* sRequestJson)
{
	xvalue pRoot = NULL;
	xvalue pCfgVal = NULL;
	xvalue pReqVal = NULL;
	xvalue pData;
	ximapconfig tCfg;
	ximapresult tRet;
	ximapclient* pClient = NULL;
	ximaplist tList;
	const char* sReference = "";
	const char* sPattern = "*";
	bool bOk = FALSE;

	memset(&tList, 0, sizeof(tList));
	xrtImapConfigInit(&tCfg);
	xrtImapResultInit(&tRet);
	if ( !xmail_xlang_parse_config_request(sRequestJson, &pRoot, &pCfgVal, &pReqVal) ) {
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "request_json must be a JSON object", "json", "error", NULL));
	}
	xmail_xlang_imap_config_from_value(pCfgVal, &tCfg);
	if ( pReqVal != NULL && pReqVal->Type == XVO_DT_TABLE ) {
		if ( xmail_xlang_text_at(pReqVal, "reference")[0] != '\0' ) sReference = xmail_xlang_text_at(pReqVal, "reference");
		if ( xmail_xlang_text_at(pReqVal, "pattern")[0] != '\0' ) sPattern = xmail_xlang_text_at(pReqVal, "pattern");
	}
	if ( xrtImapOpen(&tCfg, &pClient, &tRet) ) {
		bOk = xrtImapList(pClient, sReference, sPattern, &tList, &tRet);
		if ( bOk ) {
			(void)xrtImapLogout(pClient, &tRet);
			pClient = NULL;
		}
	}
	if ( pClient != NULL ) {
		xrtImapLogout(pClient, &tRet);
		pClient = NULL;
	}
	pData = xvoCreateTable();
	xvoTableSetValue(pData, "mailboxes", 0, xmail_xlang_imap_list_value(&tList), TRUE);
	xvoTableSetInt(pData, "count", 0, (int64)tList.iCount);
	xvoTableSetText(pData, "reference", 0, (ptr)sReference, (uint32)strlen(sReference), FALSE);
	xvoTableSetText(pData, "pattern", 0, (ptr)sPattern, (uint32)strlen(sPattern), FALSE);
	xrtImapListFree(&tList);
	xvoUnref(pRoot);
	return xmail_xlang_return_value(xmail_xlang_imap_result_value(bOk, &tRet, pData));
}

const char* xmail_imap_search_json_native(const char* sRequestJson)
{
	xvalue pRoot;
	xvalue pCfgVal;
	xvalue pReqVal;
	xvalue pData;
	xvalue pIdsArray;
	ximapconfig tCfg;
	ximapresult tRet;
	ximapclient* pClient = NULL;
	ximapsearchids tIds;
	str sSelectRaw = NULL;
	const char* sMailbox = "INBOX";
	const char* sCriteria = "ALL";
	bool bUid = TRUE;
	bool bOk = FALSE;
	size_t i;

	memset(&tIds, 0, sizeof(tIds));
	xrtImapConfigInit(&tCfg);
	xrtImapResultInit(&tRet);
	pRoot = xrtParseJSON((str)xmailMimeStrOrEmpty(sRequestJson), strlen(xmailMimeStrOrEmpty(sRequestJson)));
	if ( pRoot == NULL || pRoot->Type != XVO_DT_TABLE ) {
		if ( pRoot != NULL ) xvoUnref(pRoot);
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "request_json must be a JSON object", "json", "error", NULL));
	}
	pCfgVal = xmail_xlang_table_get(pRoot, "config");
	pReqVal = xmail_xlang_table_get(pRoot, "request");
	if ( pCfgVal == NULL || pCfgVal->Type != XVO_DT_TABLE ) pCfgVal = pRoot;
	if ( pReqVal == NULL || pReqVal->Type != XVO_DT_TABLE ) pReqVal = pRoot;
	xmail_xlang_imap_config_from_value(pCfgVal, &tCfg);
	if ( pReqVal != NULL && pReqVal->Type == XVO_DT_TABLE ) {
		xvalue pUid = xmail_xlang_table_get(pReqVal, "uid");
		if ( xmail_xlang_text_at(pReqVal, "mailbox")[0] != '\0' ) sMailbox = xmail_xlang_text_at(pReqVal, "mailbox");
		if ( xmail_xlang_text_at(pReqVal, "criteria")[0] != '\0' ) sCriteria = xmail_xlang_text_at(pReqVal, "criteria");
		if ( pUid != NULL && pUid->Type != XVO_DT_NULL ) bUid = xvoGetBool(pUid);
	}
	if ( xrtImapOpen(&tCfg, &pClient, &tRet) ) {
		if ( xrtImapSelect(pClient, sMailbox, &sSelectRaw, &tRet) ) {
			if ( sSelectRaw != NULL ) {
				xrtFree(sSelectRaw);
				sSelectRaw = NULL;
			}
			bOk = bUid ? xrtImapUidSearch(pClient, sCriteria, &tIds, &tRet) : xrtImapSearch(pClient, sCriteria, &tIds, &tRet);
		}
		if ( bOk ) {
			(void)xrtImapLogout(pClient, &tRet);
			pClient = NULL;
		}
	}
	if ( pClient != NULL ) {
		xrtImapLogout(pClient, &tRet);
		pClient = NULL;
	}
	if ( sSelectRaw != NULL ) {
		xrtFree(sSelectRaw);
	}
	pIdsArray = xvoCreateArray();
	for ( i = 0u; i < tIds.iCount; i++ ) {
		xvoArrayAppendInt(pIdsArray, (int64)tIds.arrIds[i]);
	}
	pData = xvoCreateTable();
	xvoTableSetValue(pData, "ids", 0, pIdsArray, TRUE);
	xvoTableSetInt(pData, "count", 0, (int64)tIds.iCount);
	xvoTableSetText(pData, "mailbox", 0, (ptr)sMailbox, (uint32)strlen(sMailbox), FALSE);
	xvoTableSetBool(pData, "uid", 0, bUid);
	xrtImapSearchIdsFree(&tIds);
	xvoUnref(pRoot);
	return xmail_xlang_return_value(xmail_xlang_imap_result_value(bOk, &tRet, pData));
}

const char* xmail_imap_fetch_json_native(const char* sRequestJson)
{
	xvalue pRoot;
	xvalue pCfgVal;
	xvalue pReqVal;
	xvalue pData = NULL;
	ximapconfig tCfg;
	ximapresult tRet;
	ximapclient* pClient = NULL;
	xmailparsedmessage tParsed;
	str sSelectRaw = NULL;
	str sRaw = NULL;
	const char* sMailbox = "INBOX";
	const char* sUidText = "";
	const char* sSequence = "";
	const char* sSection = "";
	const char** arrHeaders = NULL;
	size_t iHeaderCount = 0u;
	char sUidBuf[32];
	char sSeqBuf[32];
	bool bUidMode = TRUE;
	bool bPeek = TRUE;
	bool bParseMime = FALSE;
	bool bHeaderFields = FALSE;
	bool bOk = FALSE;

	memset(&tParsed, 0, sizeof(tParsed));
	xrtImapConfigInit(&tCfg);
	xrtImapResultInit(&tRet);
	sUidBuf[0] = '\0';
	sSeqBuf[0] = '\0';
	pRoot = xrtParseJSON((str)xmailMimeStrOrEmpty(sRequestJson), strlen(xmailMimeStrOrEmpty(sRequestJson)));
	if ( pRoot == NULL || pRoot->Type != XVO_DT_TABLE ) {
		if ( pRoot != NULL ) xvoUnref(pRoot);
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "request_json must be a JSON object", "json", "error", NULL));
	}
	pCfgVal = xmail_xlang_table_get(pRoot, "config");
	pReqVal = xmail_xlang_table_get(pRoot, "request");
	if ( pCfgVal == NULL || pCfgVal->Type != XVO_DT_TABLE ) pCfgVal = pRoot;
	if ( pReqVal == NULL || pReqVal->Type != XVO_DT_TABLE ) pReqVal = pRoot;
	xmail_xlang_imap_config_from_value(pCfgVal, &tCfg);
	if ( pReqVal != NULL && pReqVal->Type == XVO_DT_TABLE ) {
		xvalue pUid = xmail_xlang_table_get(pReqVal, "uid");
		xvalue pSeq = xmail_xlang_table_get(pReqVal, "sequence");
		xvalue pPeek = xmail_xlang_table_get(pReqVal, "peek");
		if ( xmail_xlang_text_at(pReqVal, "mailbox")[0] != '\0' ) sMailbox = xmail_xlang_text_at(pReqVal, "mailbox");
		if ( xmail_xlang_text_at(pReqVal, "section")[0] != '\0' ) sSection = xmail_xlang_text_at(pReqVal, "section");
		if ( pUid != NULL && pUid->Type != XVO_DT_NULL ) {
			if ( pUid->Type == XVO_DT_TEXT ) {
				sUidText = (const char*)xvoGetText(pUid);
			} else {
				snprintf(sUidBuf, sizeof(sUidBuf), "%llu", (unsigned long long)xvoGetInt(pUid));
				sUidText = sUidBuf;
			}
			bUidMode = TRUE;
		}
		if ( pSeq != NULL && pSeq->Type != XVO_DT_NULL ) {
			if ( pSeq->Type == XVO_DT_TEXT ) {
				sSequence = (const char*)xvoGetText(pSeq);
			} else {
				snprintf(sSeqBuf, sizeof(sSeqBuf), "%llu", (unsigned long long)xvoGetInt(pSeq));
				sSequence = sSeqBuf;
			}
			if ( sUidText[0] == '\0' ) bUidMode = FALSE;
		}
		if ( pPeek != NULL && pPeek->Type != XVO_DT_NULL ) bPeek = xvoGetBool(pPeek);
		bParseMime = xmail_xlang_bool_at(pReqVal, "parse_mime");
		arrHeaders = xmail_xlang_string_array_from_value(xmail_xlang_table_get(pReqVal, "headers"), &iHeaderCount);
		bHeaderFields = iHeaderCount > 0u;
	}
	if ( bUidMode && sUidText[0] == '\0' ) {
		if ( arrHeaders != NULL ) xrtFree((ptr)arrHeaders);
		xvoUnref(pRoot);
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "IMAP uid is required", "request", "error", NULL));
	}
	if ( !bUidMode && sSequence[0] == '\0' ) {
		if ( arrHeaders != NULL ) xrtFree((ptr)arrHeaders);
		xvoUnref(pRoot);
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "IMAP sequence is required", "request", "error", NULL));
	}
	if ( bHeaderFields && !bUidMode ) {
		if ( arrHeaders != NULL ) xrtFree((ptr)arrHeaders);
		xvoUnref(pRoot);
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "IMAP uid is required for header fields", "request", "error", NULL));
	}
	if ( xrtImapOpen(&tCfg, &pClient, &tRet) ) {
		if ( xrtImapSelect(pClient, sMailbox, &sSelectRaw, &tRet) ) {
			if ( sSelectRaw != NULL ) {
				xrtFree(sSelectRaw);
				sSelectRaw = NULL;
			}
			if ( bHeaderFields && bUidMode ) {
				xmailparsedmessage tHeaderParsed;
				memset(&tHeaderParsed, 0, sizeof(tHeaderParsed));
				bOk = bPeek ? xrtImapUidPeekHeaderFields(pClient, sUidText, arrHeaders, iHeaderCount, &sRaw, &tRet) : xrtImapUidFetchHeaderFields(pClient, sUidText, arrHeaders, iHeaderCount, &sRaw, &tRet);
				if ( bOk ) {
					pData = xvoCreateTable();
					xvoTableSetText(pData, "raw", 0, sRaw, sRaw ? (uint32)strlen((const char*)sRaw) : 0u, FALSE);
					xvoTableSetText(pData, "headers_raw", 0, sRaw, sRaw ? (uint32)strlen((const char*)sRaw) : 0u, FALSE);
					xvoTableSetValue(pData, "requested_headers", 0, xmail_xlang_string_array_value(arrHeaders, iHeaderCount), TRUE);
					xvoTableSetBool(pData, "headers_only", 0, TRUE);
					if ( sRaw != NULL && xmailMimeParseMessage((const char*)sRaw, &tHeaderParsed) ) {
						xvoTableSetValue(pData, "headers", 0, xmail_xlang_header_array(tHeaderParsed.arrHeaders, tHeaderParsed.iHeaderCount), TRUE);
						xmailMimeParsedMessageFree(&tHeaderParsed);
					} else {
						xvoTableSetValue(pData, "headers", 0, xvoCreateArray(), TRUE);
					}
				}
			} else if ( bParseMime && bUidMode ) {
				bOk = bPeek ? xrtImapUidPeekMime(pClient, sUidText, &tParsed, &tRet) : xrtImapUidFetchMime(pClient, sUidText, &tParsed, &tRet);
				if ( bOk ) {
					pData = xmail_xlang_parsed_message_data(&tParsed);
				}
			} else if ( bUidMode && sSection[0] != '\0' ) {
				bOk = bPeek ? xrtImapUidPeekBodySectionRaw(pClient, sUidText, sSection, &sRaw, &tRet) : xrtImapUidFetchBodySectionRaw(pClient, sUidText, sSection, &sRaw, &tRet);
			} else if ( bUidMode ) {
				bOk = bPeek ? xrtImapUidPeekBodyLiteral(pClient, sUidText, &sRaw, &tRet) : xrtImapUidFetchBodyLiteral(pClient, sUidText, &sRaw, &tRet);
			} else {
				bOk = xrtImapFetchRaw(pClient, sSequence, sSection[0] != '\0' ? sSection : "BODY[]", &sRaw, &tRet);
			}
			if ( bOk && pData == NULL ) {
				pData = xvoCreateTable();
				xvoTableSetText(pData, "raw", 0, sRaw, sRaw ? (uint32)strlen((const char*)sRaw) : 0u, FALSE);
			}
		}
		if ( bOk ) {
			(void)xrtImapLogout(pClient, &tRet);
			pClient = NULL;
		}
	}
	if ( pClient != NULL ) {
		xrtImapLogout(pClient, &tRet);
		pClient = NULL;
	}
	if ( sSelectRaw != NULL ) {
		xrtFree(sSelectRaw);
	}
	if ( pData == NULL ) {
		pData = xvoCreateTable();
	}
	xvoTableSetText(pData, "mailbox", 0, (ptr)sMailbox, (uint32)strlen(sMailbox), FALSE);
	xvoTableSetText(pData, "uid", 0, (ptr)sUidText, (uint32)strlen(sUidText), FALSE);
	xvoTableSetText(pData, "sequence", 0, (ptr)sSequence, (uint32)strlen(sSequence), FALSE);
	xvoTableSetBool(pData, "peek", 0, bPeek);
	xvoTableSetBool(pData, "parse_mime", 0, bParseMime);
	xvoTableSetBool(pData, "headers_only", 0, bHeaderFields);
	if ( bHeaderFields && xmail_xlang_table_get(pData, "requested_headers") == NULL ) {
		xvoTableSetValue(pData, "requested_headers", 0, xmail_xlang_string_array_value(arrHeaders, iHeaderCount), TRUE);
	}
	if ( sRaw != NULL ) {
		xrtFree(sRaw);
	}
	if ( arrHeaders != NULL ) {
		xrtFree((ptr)arrHeaders);
	}
	xmailMimeParsedMessageFree(&tParsed);
	xvoUnref(pRoot);
	return xmail_xlang_return_value(xmail_xlang_imap_result_value(bOk, &tRet, pData));
}

const char* xmail_imap_fetch_flags_json_native(const char* sRequestJson)
{
	xvalue pRoot = NULL;
	xvalue pCfgVal = NULL;
	xvalue pReqVal = NULL;
	xvalue pData;
	xvalue pSeq;
	ximapconfig tCfg;
	ximapresult tRet;
	ximapclient* pClient = NULL;
	ximapflags tFlags;
	str sSelectRaw = NULL;
	const char* sMailbox = "INBOX";
	const char* sSequence = "";
	char sSeqBuf[32];
	bool bOk = FALSE;

	memset(&tFlags, 0, sizeof(tFlags));
	xrtImapConfigInit(&tCfg);
	xrtImapResultInit(&tRet);
	sSeqBuf[0] = '\0';
	if ( !xmail_xlang_parse_config_request(sRequestJson, &pRoot, &pCfgVal, &pReqVal) ) {
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "request_json must be a JSON object", "json", "error", NULL));
	}
	xmail_xlang_imap_config_from_value(pCfgVal, &tCfg);
	if ( pReqVal != NULL && pReqVal->Type == XVO_DT_TABLE ) {
		if ( xmail_xlang_text_at(pReqVal, "mailbox")[0] != '\0' ) sMailbox = xmail_xlang_text_at(pReqVal, "mailbox");
		pSeq = xmail_xlang_table_get(pReqVal, "sequence");
		if ( pSeq != NULL && pSeq->Type != XVO_DT_NULL ) {
			if ( pSeq->Type == XVO_DT_TEXT ) {
				sSequence = (const char*)xvoGetText(pSeq);
			} else {
				snprintf(sSeqBuf, sizeof(sSeqBuf), "%llu", (unsigned long long)xvoGetInt(pSeq));
				sSequence = sSeqBuf;
			}
		}
	}
	if ( sSequence[0] == '\0' ) {
		xvoUnref(pRoot);
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "IMAP sequence is required", "request", "error", NULL));
	}
	if ( xrtImapOpen(&tCfg, &pClient, &tRet) ) {
		if ( xrtImapSelect(pClient, sMailbox, &sSelectRaw, &tRet) ) {
			if ( sSelectRaw != NULL ) {
				xrtFree(sSelectRaw);
				sSelectRaw = NULL;
			}
			bOk = xrtImapFetchFlags(pClient, sSequence, &tFlags, &tRet);
		}
		if ( bOk ) {
			(void)xrtImapLogout(pClient, &tRet);
			pClient = NULL;
		}
	}
	if ( pClient != NULL ) {
		xrtImapLogout(pClient, &tRet);
		pClient = NULL;
	}
	if ( sSelectRaw != NULL ) {
		xrtFree(sSelectRaw);
	}
	pData = xvoCreateTable();
	xvoTableSetValue(pData, "flags", 0, xmail_xlang_imap_flags_value(&tFlags), TRUE);
	xmail_xlang_table_set_text(pData, "mailbox", sMailbox);
	xmail_xlang_table_set_text(pData, "sequence", sSequence);
	xvoUnref(pRoot);
	return xmail_xlang_return_value(xmail_xlang_imap_result_value(bOk, &tRet, pData));
}

const char* xmail_imap_bodystructure_json_native(const char* sRequestJson)
{
	xvalue pRoot = NULL;
	xvalue pCfgVal = NULL;
	xvalue pReqVal = NULL;
	xvalue pData;
	xvalue pSeq;
	ximapconfig tCfg;
	ximapresult tRet;
	ximapclient* pClient = NULL;
	ximapbodystructure tBody;
	str sSelectRaw = NULL;
	const char* sMailbox = "INBOX";
	const char* sSequence = "";
	char sSeqBuf[32];
	bool bOk = FALSE;

	memset(&tBody, 0, sizeof(tBody));
	xrtImapConfigInit(&tCfg);
	xrtImapResultInit(&tRet);
	sSeqBuf[0] = '\0';
	if ( !xmail_xlang_parse_config_request(sRequestJson, &pRoot, &pCfgVal, &pReqVal) ) {
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "request_json must be a JSON object", "json", "error", NULL));
	}
	xmail_xlang_imap_config_from_value(pCfgVal, &tCfg);
	if ( pReqVal != NULL && pReqVal->Type == XVO_DT_TABLE ) {
		if ( xmail_xlang_text_at(pReqVal, "mailbox")[0] != '\0' ) sMailbox = xmail_xlang_text_at(pReqVal, "mailbox");
		pSeq = xmail_xlang_table_get(pReqVal, "sequence");
		if ( pSeq != NULL && pSeq->Type != XVO_DT_NULL ) {
			if ( pSeq->Type == XVO_DT_TEXT ) {
				sSequence = (const char*)xvoGetText(pSeq);
			} else {
				snprintf(sSeqBuf, sizeof(sSeqBuf), "%llu", (unsigned long long)xvoGetInt(pSeq));
				sSequence = sSeqBuf;
			}
		}
	}
	if ( sSequence[0] == '\0' ) {
		xvoUnref(pRoot);
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "IMAP sequence is required", "request", "error", NULL));
	}
	if ( xrtImapOpen(&tCfg, &pClient, &tRet) ) {
		if ( xrtImapSelect(pClient, sMailbox, &sSelectRaw, &tRet) ) {
			if ( sSelectRaw != NULL ) {
				xrtFree(sSelectRaw);
				sSelectRaw = NULL;
			}
			bOk = xrtImapFetchBodyStructure(pClient, sSequence, &tBody, &tRet);
		}
		if ( bOk ) {
			(void)xrtImapLogout(pClient, &tRet);
			pClient = NULL;
		}
	}
	if ( pClient != NULL ) {
		xrtImapLogout(pClient, &tRet);
		pClient = NULL;
	}
	if ( sSelectRaw != NULL ) {
		xrtFree(sSelectRaw);
	}
	pData = xvoCreateTable();
	xvoTableSetValue(pData, "bodystructure", 0, xmail_xlang_imap_bodystructure_value(&tBody), TRUE);
	xmail_xlang_table_set_text(pData, "mailbox", sMailbox);
	xmail_xlang_table_set_text(pData, "sequence", sSequence);
	xrtImapBodyStructureFree(&tBody);
	xvoUnref(pRoot);
	return xmail_xlang_return_value(xmail_xlang_imap_result_value(bOk, &tRet, pData));
}

const char* xmail_imap_store_flags_json_native(const char* sRequestJson)
{
	xvalue pRoot = NULL;
	xvalue pCfgVal = NULL;
	xvalue pReqVal = NULL;
	xvalue pData;
	xvalue pUid;
	xvalue pSeq;
	ximapconfig tCfg;
	ximapresult tRet;
	ximapclient* pClient = NULL;
	ximapflags tFlags;
	str sSelectRaw = NULL;
	const char* sMailbox = "INBOX";
	const char* sUid = "";
	const char* sSequence = "";
	const char* sOperation = "+FLAGS";
	const char* sFlags = "(\\Seen)";
	char sUidBuf[32];
	char sSeqBuf[32];
	bool bUidMode = FALSE;
	bool bOk = FALSE;

	memset(&tFlags, 0, sizeof(tFlags));
	xrtImapConfigInit(&tCfg);
	xrtImapResultInit(&tRet);
	sUidBuf[0] = '\0';
	sSeqBuf[0] = '\0';
	if ( !xmail_xlang_parse_config_request(sRequestJson, &pRoot, &pCfgVal, &pReqVal) ) {
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "request_json must be a JSON object", "json", "error", NULL));
	}
	xmail_xlang_imap_config_from_value(pCfgVal, &tCfg);
	if ( pReqVal != NULL && pReqVal->Type == XVO_DT_TABLE ) {
		if ( xmail_xlang_text_at(pReqVal, "mailbox")[0] != '\0' ) sMailbox = xmail_xlang_text_at(pReqVal, "mailbox");
		if ( xmail_xlang_text_at(pReqVal, "operation")[0] != '\0' ) sOperation = xmail_xlang_text_at(pReqVal, "operation");
		if ( xmail_xlang_text_at(pReqVal, "flags")[0] != '\0' ) sFlags = xmail_xlang_text_at(pReqVal, "flags");
		pUid = xmail_xlang_table_get(pReqVal, "uid");
		pSeq = xmail_xlang_table_get(pReqVal, "sequence");
		if ( pUid != NULL && pUid->Type != XVO_DT_NULL ) {
			if ( pUid->Type == XVO_DT_TEXT ) {
				sUid = (const char*)xvoGetText(pUid);
			} else {
				snprintf(sUidBuf, sizeof(sUidBuf), "%llu", (unsigned long long)xvoGetInt(pUid));
				sUid = sUidBuf;
			}
			bUidMode = TRUE;
		}
		if ( pSeq != NULL && pSeq->Type != XVO_DT_NULL ) {
			if ( pSeq->Type == XVO_DT_TEXT ) {
				sSequence = (const char*)xvoGetText(pSeq);
			} else {
				snprintf(sSeqBuf, sizeof(sSeqBuf), "%llu", (unsigned long long)xvoGetInt(pSeq));
				sSequence = sSeqBuf;
			}
		}
	}
	if ( bUidMode && sUid[0] == '\0' ) {
		xvoUnref(pRoot);
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "IMAP uid is required", "request", "error", NULL));
	}
	if ( !bUidMode && sSequence[0] == '\0' ) {
		xvoUnref(pRoot);
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "IMAP sequence is required", "request", "error", NULL));
	}
	if ( xrtImapOpen(&tCfg, &pClient, &tRet) ) {
		if ( xrtImapSelect(pClient, sMailbox, &sSelectRaw, &tRet) ) {
			if ( sSelectRaw != NULL ) {
				xrtFree(sSelectRaw);
				sSelectRaw = NULL;
			}
			bOk = bUidMode
				? xrtImapUidStoreFlagsParsed(pClient, sUid, sOperation, sFlags, &tFlags, &tRet)
				: xrtImapStoreFlagsParsed(pClient, sSequence, sOperation, sFlags, &tFlags, &tRet);
		}
		if ( bOk ) {
			(void)xrtImapLogout(pClient, &tRet);
			pClient = NULL;
		}
	}
	if ( pClient != NULL ) {
		xrtImapLogout(pClient, &tRet);
		pClient = NULL;
	}
	if ( sSelectRaw != NULL ) {
		xrtFree(sSelectRaw);
	}
	pData = xvoCreateTable();
	xvoTableSetValue(pData, "flags", 0, xmail_xlang_imap_flags_value(&tFlags), TRUE);
	xmail_xlang_table_set_text(pData, "mailbox", sMailbox);
	xmail_xlang_table_set_text(pData, "uid", sUid);
	xmail_xlang_table_set_text(pData, "sequence", sSequence);
	xmail_xlang_table_set_text(pData, "operation", sOperation);
	xmail_xlang_table_set_text(pData, "requested_flags", sFlags);
	xvoTableSetBool(pData, "uid_mode", 0, bUidMode);
	xvoUnref(pRoot);
	return xmail_xlang_return_value(xmail_xlang_imap_result_value(bOk, &tRet, pData));
}

const char* xmail_imap_expunge_json_native(const char* sRequestJson)
{
	xvalue pRoot = NULL;
	xvalue pCfgVal = NULL;
	xvalue pReqVal = NULL;
	xvalue pData;
	ximapconfig tCfg;
	ximapresult tRet;
	ximapclient* pClient = NULL;
	ximapexpungeresult tExpunge;
	str sSelectRaw = NULL;
	const char* sMailbox = "INBOX";
	const char* sUidSet = "";
	bool bUidMode = FALSE;
	bool bOk = FALSE;

	memset(&tExpunge, 0, sizeof(tExpunge));
	xrtImapConfigInit(&tCfg);
	xrtImapResultInit(&tRet);
	if ( !xmail_xlang_parse_config_request(sRequestJson, &pRoot, &pCfgVal, &pReqVal) ) {
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "request_json must be a JSON object", "json", "error", NULL));
	}
	xmail_xlang_imap_config_from_value(pCfgVal, &tCfg);
	if ( pReqVal != NULL && pReqVal->Type == XVO_DT_TABLE ) {
		if ( xmail_xlang_text_at(pReqVal, "mailbox")[0] != '\0' ) sMailbox = xmail_xlang_text_at(pReqVal, "mailbox");
		if ( xmail_xlang_text_at(pReqVal, "uid_set")[0] != '\0' ) {
			sUidSet = xmail_xlang_text_at(pReqVal, "uid_set");
			bUidMode = TRUE;
		}
	}
	if ( xrtImapOpen(&tCfg, &pClient, &tRet) ) {
		if ( xrtImapSelect(pClient, sMailbox, &sSelectRaw, &tRet) ) {
			if ( sSelectRaw != NULL ) {
				xrtFree(sSelectRaw);
				sSelectRaw = NULL;
			}
			bOk = bUidMode ? xrtImapUidExpunge(pClient, sUidSet, &tExpunge, &tRet) : xrtImapExpunge(pClient, &tExpunge, &tRet);
		}
		if ( bOk ) {
			(void)xrtImapLogout(pClient, &tRet);
			pClient = NULL;
		}
	}
	if ( pClient != NULL ) {
		xrtImapLogout(pClient, &tRet);
		pClient = NULL;
	}
	if ( sSelectRaw != NULL ) {
		xrtFree(sSelectRaw);
	}
	pData = xvoCreateTable();
	xvoTableSetValue(pData, "sequences", 0, xmail_xlang_imap_expunge_sequences_value(&tExpunge), TRUE);
	xvoTableSetInt(pData, "count", 0, (int64)tExpunge.iCount);
	xmail_xlang_table_set_text(pData, "mailbox", sMailbox);
	xmail_xlang_table_set_text(pData, "uid_set", sUidSet);
	xvoTableSetBool(pData, "uid_mode", 0, bUidMode);
	xrtImapExpungeResultFree(&tExpunge);
	xvoUnref(pRoot);
	return xmail_xlang_return_value(xmail_xlang_imap_result_value(bOk, &tRet, pData));
}

const char* xmail_imap_idle_probe_json_native(const char* sRequestJson)
{
	xvalue pRoot;
	xvalue pCfgVal;
	xvalue pReqVal;
	xvalue pData;
	ximapconfig tCfg;
	ximapresult tRet;
	ximapclient* pClient = NULL;
	str sSelectRaw = NULL;
	const char* sMailbox = "INBOX";
	bool bOk = FALSE;

	xrtImapConfigInit(&tCfg);
	xrtImapResultInit(&tRet);
	pRoot = xrtParseJSON((str)xmailMimeStrOrEmpty(sRequestJson), strlen(xmailMimeStrOrEmpty(sRequestJson)));
	if ( pRoot == NULL || pRoot->Type != XVO_DT_TABLE ) {
		if ( pRoot != NULL ) xvoUnref(pRoot);
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "request_json must be a JSON object", "json", "error", NULL));
	}
	pCfgVal = xmail_xlang_table_get(pRoot, "config");
	pReqVal = xmail_xlang_table_get(pRoot, "request");
	if ( pCfgVal == NULL || pCfgVal->Type != XVO_DT_TABLE ) pCfgVal = pRoot;
	if ( pReqVal == NULL || pReqVal->Type != XVO_DT_TABLE ) pReqVal = pRoot;
	xmail_xlang_imap_config_from_value(pCfgVal, &tCfg);
	if ( pReqVal != NULL && pReqVal->Type == XVO_DT_TABLE && xmail_xlang_text_at(pReqVal, "mailbox")[0] != '\0' ) {
		sMailbox = xmail_xlang_text_at(pReqVal, "mailbox");
	}
	if ( xrtImapOpen(&tCfg, &pClient, &tRet) ) {
		if ( xrtImapSelect(pClient, sMailbox, &sSelectRaw, &tRet) ) {
			if ( sSelectRaw != NULL ) {
				xrtFree(sSelectRaw);
				sSelectRaw = NULL;
			}
			bOk = xrtImapIdleProbe(pClient, &tRet);
		}
		if ( bOk ) {
			(void)xrtImapLogout(pClient, &tRet);
			pClient = NULL;
		}
	}
	if ( pClient != NULL ) {
		xrtImapLogout(pClient, &tRet);
		pClient = NULL;
	}
	if ( sSelectRaw != NULL ) {
		xrtFree(sSelectRaw);
	}
	pData = xvoCreateTable();
	xvoTableSetText(pData, "mailbox", 0, (ptr)sMailbox, (uint32)strlen(sMailbox), FALSE);
	xvoTableSetBool(pData, "idle_supported", 0, bOk);
	xvoUnref(pRoot);
	return xmail_xlang_return_value(xmail_xlang_imap_result_value(bOk, &tRet, pData));
}

const char* xmail_imap_idle_json_native(const char* sRequestJson)
{
	xvalue pRoot;
	xvalue pCfgVal;
	xvalue pReqVal;
	xvalue pData;
	xvalue pEventsArray;
	ximapconfig tCfg;
	ximapresult tRet;
	ximapclient* pClient = NULL;
	str sSelectRaw = NULL;
	str* arrEvents = NULL;
	size_t iEventCount = 0u;
	size_t iMaxEvents = 1u;
	size_t i;
	const char* sMailbox = "INBOX";
	bool bOk = FALSE;

	xrtImapConfigInit(&tCfg);
	xrtImapResultInit(&tRet);
	pRoot = xrtParseJSON((str)xmailMimeStrOrEmpty(sRequestJson), strlen(xmailMimeStrOrEmpty(sRequestJson)));
	if ( pRoot == NULL || pRoot->Type != XVO_DT_TABLE ) {
		if ( pRoot != NULL ) xvoUnref(pRoot);
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "request_json must be a JSON object", "json", "error", NULL));
	}
	pCfgVal = xmail_xlang_table_get(pRoot, "config");
	pReqVal = xmail_xlang_table_get(pRoot, "request");
	if ( pCfgVal == NULL || pCfgVal->Type != XVO_DT_TABLE ) pCfgVal = pRoot;
	if ( pReqVal == NULL || pReqVal->Type != XVO_DT_TABLE ) pReqVal = pRoot;
	xmail_xlang_imap_config_from_value(pCfgVal, &tCfg);
	if ( pReqVal != NULL && pReqVal->Type == XVO_DT_TABLE ) {
		xvalue pMaxEvents = xmail_xlang_table_get(pReqVal, "max_events");
		xvalue pMaxEventsPerCycle = xmail_xlang_table_get(pReqVal, "max_events_per_cycle");
		if ( xmail_xlang_text_at(pReqVal, "mailbox")[0] != '\0' ) sMailbox = xmail_xlang_text_at(pReqVal, "mailbox");
		if ( pMaxEvents != NULL && pMaxEvents->Type != XVO_DT_NULL ) iMaxEvents = (size_t)xvoGetInt(pMaxEvents);
		if ( pMaxEventsPerCycle != NULL && pMaxEventsPerCycle->Type != XVO_DT_NULL ) iMaxEvents = (size_t)xvoGetInt(pMaxEventsPerCycle);
	}
	if ( iMaxEvents == 0u ) {
		iMaxEvents = 1u;
	}
	if ( xrtImapOpen(&tCfg, &pClient, &tRet) ) {
		if ( xrtImapSelect(pClient, sMailbox, &sSelectRaw, &tRet) ) {
			if ( sSelectRaw != NULL ) {
				xrtFree(sSelectRaw);
				sSelectRaw = NULL;
			}
			bOk = xrtImapIdleCollect(pClient, iMaxEvents, &arrEvents, &iEventCount, &tRet);
		}
		if ( bOk ) {
			(void)xrtImapLogout(pClient, &tRet);
			pClient = NULL;
		}
	}
	if ( pClient != NULL ) {
		xrtImapLogout(pClient, &tRet);
		pClient = NULL;
	}
	if ( sSelectRaw != NULL ) {
		xrtFree(sSelectRaw);
	}
	pEventsArray = xvoCreateArray();
	for ( i = 0u; i < iEventCount; i++ ) {
		xvoArrayAppendText(pEventsArray, arrEvents[i], arrEvents[i] ? (uint32)strlen((const char*)arrEvents[i]) : 0u, FALSE);
	}
	pData = xvoCreateTable();
	xvoTableSetText(pData, "mailbox", 0, (ptr)sMailbox, (uint32)strlen(sMailbox), FALSE);
	xvoTableSetValue(pData, "events", 0, pEventsArray, TRUE);
	xvoTableSetInt(pData, "count", 0, (int64)iEventCount);
	xvoTableSetInt(pData, "max_events", 0, (int64)iMaxEvents);
	xrtImapIdleEventsFree(arrEvents, iEventCount);
	xvoUnref(pRoot);
	return xmail_xlang_return_value(xmail_xlang_imap_result_value(bOk, &tRet, pData));
}

typedef struct {
	xvalue pEventsArray;
	size_t iCount;
	size_t iMaxEvents;
} xmail_xlang_imap_watch_ctx;

static bool xmail_xlang_imap_watch_event(const char* sEvent, void* pUser)
{
	xmail_xlang_imap_watch_ctx* pCtx = (xmail_xlang_imap_watch_ctx*)pUser;

	if ( pCtx == NULL || pCtx->pEventsArray == NULL ) {
		return FALSE;
	}
	xvoArrayAppendText(pCtx->pEventsArray, (ptr)xmailMimeStrOrEmpty(sEvent), (uint32)strlen(xmailMimeStrOrEmpty(sEvent)), FALSE);
	pCtx->iCount++;
	if ( pCtx->iMaxEvents != 0u && pCtx->iCount >= pCtx->iMaxEvents ) {
		return FALSE;
	}
	return TRUE;
}

const char* xmail_imap_watch_json_native(const char* sRequestJson)
{
	xvalue pRoot;
	xvalue pCfgVal;
	xvalue pReqVal;
	xvalue pData;
	xvalue pEventsArray;
	ximapconfig tCfg;
	ximapresult tRet;
	ximapidleloopopts tOpts;
	ximapidleloopsummary tSummary;
	xmail_xlang_imap_watch_ctx tCtx;
	const char* sMailbox = "INBOX";
	size_t iMaxEvents = 0u;
	bool bOk;

	xrtImapConfigInit(&tCfg);
	xrtImapResultInit(&tRet);
	xrtImapIdleLoopOptsInit(&tOpts);
	memset(&tSummary, 0, sizeof(tSummary));
	memset(&tCtx, 0, sizeof(tCtx));
	tOpts.iMaxCycles = 1u;
	tOpts.iMaxEventsPerCycle = 1u;
	tOpts.iReconnectDelayMs = 0u;
	tOpts.iMaxReconnectDelayMs = 0u;
	pRoot = xrtParseJSON((str)xmailMimeStrOrEmpty(sRequestJson), strlen(xmailMimeStrOrEmpty(sRequestJson)));
	if ( pRoot == NULL || pRoot->Type != XVO_DT_TABLE ) {
		if ( pRoot != NULL ) xvoUnref(pRoot);
		return xmail_xlang_return_value(xmail_xlang_result_value(FALSE, "request_json must be a JSON object", "json", "error", NULL));
	}
	pCfgVal = xmail_xlang_table_get(pRoot, "config");
	pReqVal = xmail_xlang_table_get(pRoot, "request");
	if ( pCfgVal == NULL || pCfgVal->Type != XVO_DT_TABLE ) pCfgVal = pRoot;
	if ( pReqVal == NULL || pReqVal->Type != XVO_DT_TABLE ) pReqVal = pRoot;
	xmail_xlang_imap_config_from_value(pCfgVal, &tCfg);
	if ( pReqVal != NULL && pReqVal->Type == XVO_DT_TABLE ) {
		xvalue pMaxEvents = xmail_xlang_table_get(pReqVal, "max_events");
		xvalue pMaxEventsPerCycle = xmail_xlang_table_get(pReqVal, "max_events_per_cycle");
		xvalue pMaxCycles = xmail_xlang_table_get(pReqVal, "max_cycles");
		xvalue pReconnectDelay = xmail_xlang_table_get(pReqVal, "reconnect_delay_ms");
		xvalue pMaxReconnectDelay = xmail_xlang_table_get(pReqVal, "max_reconnect_delay_ms");
		xvalue pHeartbeatEvery = xmail_xlang_table_get(pReqVal, "heartbeat_every_cycles");
		if ( xmail_xlang_text_at(pReqVal, "mailbox")[0] != '\0' ) sMailbox = xmail_xlang_text_at(pReqVal, "mailbox");
		if ( pMaxEvents != NULL && pMaxEvents->Type != XVO_DT_NULL && xvoGetInt(pMaxEvents) > 0 ) iMaxEvents = (size_t)xvoGetInt(pMaxEvents);
		if ( pMaxEventsPerCycle != NULL && pMaxEventsPerCycle->Type != XVO_DT_NULL && xvoGetInt(pMaxEventsPerCycle) > 0 ) tOpts.iMaxEventsPerCycle = (size_t)xvoGetInt(pMaxEventsPerCycle);
		if ( pMaxCycles != NULL && pMaxCycles->Type != XVO_DT_NULL && xvoGetInt(pMaxCycles) > 0 ) tOpts.iMaxCycles = (uint32)xvoGetInt(pMaxCycles);
		if ( pReconnectDelay != NULL && pReconnectDelay->Type != XVO_DT_NULL && xvoGetInt(pReconnectDelay) >= 0 ) tOpts.iReconnectDelayMs = (uint32)xvoGetInt(pReconnectDelay);
		if ( pMaxReconnectDelay != NULL && pMaxReconnectDelay->Type != XVO_DT_NULL && xvoGetInt(pMaxReconnectDelay) >= 0 ) tOpts.iMaxReconnectDelayMs = (uint32)xvoGetInt(pMaxReconnectDelay);
		if ( pHeartbeatEvery != NULL && pHeartbeatEvery->Type != XVO_DT_NULL && xvoGetInt(pHeartbeatEvery) >= 0 ) tOpts.iHeartbeatEveryCycles = (uint32)xvoGetInt(pHeartbeatEvery);
	}
	tOpts.sMailbox = sMailbox;
	if ( tOpts.iMaxEventsPerCycle == 0u ) {
		tOpts.iMaxEventsPerCycle = 1u;
	}
	if ( tOpts.iMaxCycles == 0u && iMaxEvents == 0u ) {
		tOpts.iMaxCycles = 1u;
	}
	pEventsArray = xvoCreateArray();
	tCtx.pEventsArray = pEventsArray;
	tCtx.iMaxEvents = iMaxEvents;
	bOk = xrtImapIdleLoop(&tCfg, &tOpts, xmail_xlang_imap_watch_event, &tCtx, &tSummary, &tRet);
	pData = xvoCreateTable();
	xvoTableSetText(pData, "mailbox", 0, (ptr)sMailbox, (uint32)strlen(sMailbox), FALSE);
	xvoTableSetValue(pData, "events", 0, pEventsArray, TRUE);
	xvoTableSetInt(pData, "count", 0, (int64)tCtx.iCount);
	xvoTableSetInt(pData, "cycles", 0, (int64)tSummary.iCycles);
	xvoTableSetInt(pData, "heartbeats", 0, (int64)tSummary.iHeartbeats);
	xvoTableSetBool(pData, "cancelled", 0, tSummary.bCancelled);
	xvoTableSetBool(pData, "stopped", 0, tSummary.bStopped);
	xvoTableSetBool(pData, "heartbeat_stopped", 0, tSummary.bHeartbeatStopped);
	xvoTableSetBool(pData, "last_cycle_failed", 0, tSummary.bLastCycleFailed);
	xvoTableSetInt(pData, "max_events", 0, (int64)iMaxEvents);
	xvoTableSetInt(pData, "max_events_per_cycle", 0, (int64)tOpts.iMaxEventsPerCycle);
	xvoTableSetInt(pData, "max_cycles", 0, (int64)tOpts.iMaxCycles);
	xvoUnref(pRoot);
	return xmail_xlang_return_value(xmail_xlang_imap_result_value(bOk, &tRet, pData));
}
#endif
#endif
