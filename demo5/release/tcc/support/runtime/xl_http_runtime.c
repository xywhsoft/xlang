#include "../common/xl_common.h"
#include "xlrt_generated.h"

static int __xlHttpBufGrow(char** psBuf, size_t* piCap, size_t iNeed)
{
	char* sNewBuf;
	size_t iNewCap;

	if ( (psBuf == NULL) || (piCap == NULL) ) {
		return 0;
	}
	if ( (*psBuf != NULL) && (*piCap >= iNeed) ) {
		return 1;
	}

	iNewCap = (*piCap == 0u) ? 128u : *piCap;
	while ( iNewCap < iNeed ) {
		iNewCap *= 2u;
	}

	sNewBuf = (char*)realloc(*psBuf, iNewCap);
	if ( sNewBuf == NULL ) {
		return 0;
	}

	*psBuf = sNewBuf;
	*piCap = iNewCap;
	return 1;
}

static int __xlHttpBufAppend(char** psBuf, size_t* piLen, size_t* piCap, const char* sText)
{
	size_t iNeed;
	size_t iTextLen;

	if ( (psBuf == NULL) || (piLen == NULL) || (piCap == NULL) ) {
		return 0;
	}
	if ( sText == NULL ) {
		sText = "";
	}

	iTextLen = strlen(sText);
	iNeed = *piLen + iTextLen + 1u;
	if ( !__xlHttpBufGrow(psBuf, piCap, iNeed) ) {
		return 0;
	}

	memcpy((*psBuf) + *piLen, sText, iTextLen);
	*piLen += iTextLen;
	(*psBuf)[*piLen] = '\0';
	return 1;
}

static int __xlHttpBufAppendFormat(char** psBuf, size_t* piLen, size_t* piCap, const char* sFormat, ...)
{
	va_list args;
	va_list args2;
	int iNeed;
	size_t iTotalNeed;

	va_start(args, sFormat);
	va_copy(args2, args);
	iNeed = vsnprintf(NULL, 0, sFormat, args);
	va_end(args);
	if ( iNeed < 0 ) {
		va_end(args2);
		return 0;
	}

	iTotalNeed = *piLen + (size_t)iNeed + 1u;
	if ( !__xlHttpBufGrow(psBuf, piCap, iTotalNeed) ) {
		va_end(args2);
		return 0;
	}

	vsnprintf((*psBuf) + *piLen, *piCap - *piLen, sFormat, args2);
	va_end(args2);
	*piLen += (size_t)iNeed;
	return 1;
}

static char* __xlHttpSlice(const char* sText, size_t iLen)
{
	char* sOut;

	sOut = (char*)malloc(iLen + 1u);
	if ( sOut == NULL ) {
		return NULL;
	}
	if ( iLen != 0u ) {
		memcpy(sOut, sText, iLen);
	}
	sOut[iLen] = '\0';
	return sOut;
}

static void __xlHttpTrimRange(const char* sText, size_t* piStart, size_t* piLen)
{
	size_t iStart;
	size_t iLen;

	if ( (sText == NULL) || (piStart == NULL) || (piLen == NULL) ) {
		return;
	}

	iStart = *piStart;
	iLen = *piLen;
	while ( (iLen > 0u) && ((sText[iStart] == ' ') || (sText[iStart] == '\t')) ) {
		++iStart;
		--iLen;
	}
	while ( (iLen > 0u) && ((sText[iStart + iLen - 1u] == ' ') || (sText[iStart + iLen - 1u] == '\t')) ) {
		--iLen;
	}

	*piStart = iStart;
	*piLen = iLen;
}

static int __xlHttpTableSetText(xvalue value, const char* sKey, const char* sText)
{
	xvalue itemValue;

	if ( (value == NULL) || (sKey == NULL) ) {
		return 0;
	}
	if ( sText == NULL ) {
		sText = "";
	}

	itemValue = xvoCreateText((str)sText, (uint32)strlen(sText), FALSE);
	if ( itemValue == NULL ) {
		return 0;
	}
	if ( !xvoTableSetValue(value, (str)sKey, (uint32)strlen(sKey), itemValue, TRUE) ) {
		xvoUnref(itemValue);
		return 0;
	}
	return 1;
}

const char* xl_http_headers(xvalue value)
{
	char* sBuf;
	size_t iLen;
	size_t iCap;
	void* pTable;
	Dict_Key* pKey;

	sBuf = NULL;
	iLen = 0u;
	iCap = 0u;

	if ( value == NULL ) {
		return "";
	}

	pTable = xvoGetTable(value);
	if ( pTable == NULL ) {
		return "";
	}

	xrtAVLTB_IterBegin((xavltbase)pTable);
	for ( pKey = (Dict_Key*)xrtAVLTB_IterNext((xavltbase)pTable); pKey != NULL; pKey = (Dict_Key*)xrtAVLTB_IterNext((xavltbase)pTable) ) {
		xvalue itemValue = *(xvalue*)(&pKey[1]);
		const char* sValue = (const char*)xvoGetText(itemValue);

		if ( !__xlHttpBufAppendFormat(&sBuf, &iLen, &iCap, "%s: %s\r\n", pKey->Key, (sValue != NULL) ? sValue : "") ) {
			xrtAVLTB_IterEnd((xavltbase)pTable);
			free(sBuf);
			xlrt_native_error_raise("HttpError", "http headers build failed", 0, NULL);
			return "";
		}
	}
	xrtAVLTB_IterEnd((xavltbase)pTable);

	if ( sBuf == NULL ) {
		sBuf = (char*)malloc(1u);
		if ( sBuf == NULL ) {
			return "";
		}
		sBuf[0] = '\0';
	}
	return sBuf;
}

const char* xl_http_request(const char* sMethod, const char* sTarget, xvalue headers, const char* sBody)
{
	char* sBuf;
	size_t iLen;
	size_t iCap;
	const char* sHeaderText;

	if ( sMethod == NULL ) {
		sMethod = "GET";
	}
	if ( sTarget == NULL ) {
		sTarget = "/";
	}
	if ( sBody == NULL ) {
		sBody = "";
	}

	sBuf = NULL;
	iLen = 0u;
	iCap = 0u;
	sHeaderText = xl_http_headers(headers);
	if ( !__xlHttpBufAppendFormat(&sBuf, &iLen, &iCap, "%s %s HTTP/1.1\r\n", sMethod, sTarget) ||
		!__xlHttpBufAppend(&sBuf, &iLen, &iCap, sHeaderText) ||
		!__xlHttpBufAppend(&sBuf, &iLen, &iCap, "\r\n") ||
		!__xlHttpBufAppend(&sBuf, &iLen, &iCap, sBody) ) {
		free(sBuf);
		xlrt_native_error_raise("HttpError", "http request build failed", 0, NULL);
		return "";
	}
	return sBuf;
}

const char* xl_http_response(int iStatus, const char* sReason, xvalue headers, const char* sBody)
{
	char* sBuf;
	size_t iLen;
	size_t iCap;
	const char* sHeaderText;

	if ( sReason == NULL ) {
		sReason = "OK";
	}
	if ( sBody == NULL ) {
		sBody = "";
	}

	sBuf = NULL;
	iLen = 0u;
	iCap = 0u;
	sHeaderText = xl_http_headers(headers);
	if ( !__xlHttpBufAppendFormat(&sBuf, &iLen, &iCap, "HTTP/1.1 %d %s\r\n", iStatus, sReason) ||
		!__xlHttpBufAppend(&sBuf, &iLen, &iCap, sHeaderText) ||
		!__xlHttpBufAppend(&sBuf, &iLen, &iCap, "\r\n") ||
		!__xlHttpBufAppend(&sBuf, &iLen, &iCap, sBody) ) {
		free(sBuf);
		xlrt_native_error_raise("HttpError", "http response build failed", 0, NULL);
		return "";
	}
	return sBuf;
}

xvalue xl_http_parse_headers(const char* sText)
{
	xvalue value;
	const char* sCur;

	if ( sText == NULL ) {
		sText = "";
	}

	value = xvoCreateTable();
	if ( value == NULL ) {
		xlrt_native_error_raise("HttpError", "http headers parse failed", 0, sText);
		return NULL;
	}

	sCur = sText;
	while ( *sCur != '\0' ) {
		const char* sLineEnd;
		const char* sColon;
		size_t iLineLen;
		size_t iKeyStart;
		size_t iKeyLen;
		size_t iValueStart;
		size_t iValueLen;
		char* sKey;
		char* sValue;

		sLineEnd = strstr(sCur, "\r\n");
		if ( sLineEnd == NULL ) {
			sLineEnd = strchr(sCur, '\n');
		}
		iLineLen = (sLineEnd != NULL) ? (size_t)(sLineEnd - sCur) : strlen(sCur);
		if ( iLineLen == 0u ) {
			break;
		}

		sColon = memchr(sCur, ':', iLineLen);
		if ( sColon != NULL ) {
			iKeyStart = 0u;
			iKeyLen = (size_t)(sColon - sCur);
			iValueStart = iKeyLen + 1u;
			iValueLen = iLineLen - iValueStart;
			__xlHttpTrimRange(sCur, &iKeyStart, &iKeyLen);
			__xlHttpTrimRange(sCur, &iValueStart, &iValueLen);
			sKey = __xlHttpSlice(sCur + iKeyStart, iKeyLen);
			sValue = __xlHttpSlice(sCur + iValueStart, iValueLen);
			if ( (sKey == NULL) || (sValue == NULL) || !__xlHttpTableSetText(value, sKey, sValue) ) {
				free(sKey);
				free(sValue);
				xvoUnref(value);
				xlrt_native_error_raise("HttpError", "http headers parse failed", 0, sText);
				return NULL;
			}
			free(sKey);
			free(sValue);
		}

		if ( sLineEnd == NULL ) {
			break;
		}
		sCur = sLineEnd;
		if ( (sCur[0] == '\r') && (sCur[1] == '\n') ) {
			sCur += 2;
		} else if ( sCur[0] == '\n' ) {
			sCur += 1;
		}
	}

	return value;
}
