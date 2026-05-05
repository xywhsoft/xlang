#include "../common/xl_common.h"
#include "xlrt_generated.h"

typedef struct XL_MimeMapEntry {
	const char* sExt;
	const char* sMime;
} XL_MimeMapEntry;

static const XL_MimeMapEntry __gXLMimeMap[] = {
	{ "txt", "text/plain" },
	{ "html", "text/html" },
	{ "htm", "text/html" },
	{ "css", "text/css" },
	{ "js", "application/javascript" },
	{ "json", "application/json" },
	{ "xml", "application/xml" },
	{ "csv", "text/csv" },
	{ "md", "text/markdown" },
	{ "png", "image/png" },
	{ "jpg", "image/jpeg" },
	{ "jpeg", "image/jpeg" },
	{ "gif", "image/gif" },
	{ "svg", "image/svg+xml" },
	{ "webp", "image/webp" },
	{ "pdf", "application/pdf" },
	{ "zip", "application/zip" },
	{ "gz", "application/gzip" },
	{ "tar", "application/x-tar" },
	{ "mp3", "audio/mpeg" },
	{ "mp4", "video/mp4" },
	{ "bin", "application/octet-stream" }
};

static int __xlMimeTextEqualIgnoreCase(const char* sLeft, const char* sRight)
{
	unsigned char chLeft;
	unsigned char chRight;

	if ( (sLeft == NULL) || (sRight == NULL) ) {
		return 0;
	}

	while ( (*sLeft != '\0') && (*sRight != '\0') ) {
		chLeft = (unsigned char)*sLeft;
		chRight = (unsigned char)*sRight;
		if ( (chLeft >= 'A') && (chLeft <= 'Z') ) {
			chLeft = (unsigned char)(chLeft - 'A' + 'a');
		}
		if ( (chRight >= 'A') && (chRight <= 'Z') ) {
			chRight = (unsigned char)(chRight - 'A' + 'a');
		}
		if ( chLeft != chRight ) {
			return 0;
		}
		++sLeft;
		++sRight;
	}

	return (*sLeft == '\0') && (*sRight == '\0');
}

static void __xlMimeTrimSpan(const char* sText, size_t* piStart, size_t* piLen)
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

static char* __xlMimeSliceLower(const char* sText, size_t iStart, size_t iLen)
{
	char* sOut;
	size_t i;

	sOut = (char*)malloc(iLen + 1u);
	if ( sOut == NULL ) {
		return NULL;
	}
	for ( i = 0u; i < iLen; ++i ) {
		unsigned char ch = (unsigned char)sText[iStart + i];
		if ( (ch >= 'A') && (ch <= 'Z') ) {
			ch = (unsigned char)(ch - 'A' + 'a');
		}
		sOut[i] = (char)ch;
	}
	sOut[iLen] = '\0';
	return sOut;
}

static char* __xlMimeSlice(const char* sText, size_t iStart, size_t iLen)
{
	char* sOut;

	sOut = (char*)malloc(iLen + 1u);
	if ( sOut == NULL ) {
		return NULL;
	}
	if ( iLen != 0u ) {
		memcpy(sOut, sText + iStart, iLen);
	}
	sOut[iLen] = '\0';
	return sOut;
}

static int __xlMimeTableSetText(xvalue value, const char* sKey, const char* sText)
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

static const char* __xlMimeTableGetText(xvalue value, const char* sKey)
{
	xvalue itemValue;

	if ( (value == NULL) || (sKey == NULL) ) {
		return "";
	}

	itemValue = xvoTableGetValue(value, (str)sKey, (uint32)strlen(sKey));
	if ( itemValue == NULL ) {
		return "";
	}
	return (const char*)xvoGetText(itemValue);
}

const char* xl_mime_by_ext(const char* sExt)
{
	size_t i;
	const char* sScan;
	char sBuf[64];
	size_t iLen;

	if ( sExt == NULL ) {
		return "application/octet-stream";
	}

	sScan = sExt;
	while ( *sScan == '.' ) {
		++sScan;
	}

	iLen = strlen(sScan);
	if ( iLen >= sizeof(sBuf) ) {
		iLen = sizeof(sBuf) - 1u;
	}
	for ( i = 0u; i < iLen; ++i ) {
		unsigned char ch = (unsigned char)sScan[i];
		if ( (ch >= 'A') && (ch <= 'Z') ) {
			ch = (unsigned char)(ch - 'A' + 'a');
		}
		sBuf[i] = (char)ch;
	}
	sBuf[iLen] = '\0';

	for ( i = 0u; i < XL_COUNTOF(__gXLMimeMap); ++i ) {
		if ( strcmp(sBuf, __gXLMimeMap[i].sExt) == 0 ) {
			return __gXLMimeMap[i].sMime;
		}
	}
	return "application/octet-stream";
}

const char* xl_mime_by_path(const char* sPath)
{
	const char* sDot;
	const char* sSlash;
	const char* sBackslash;
	const char* sTail;

	if ( sPath == NULL ) {
		return "application/octet-stream";
	}

	sDot = strrchr(sPath, '.');
	sSlash = strrchr(sPath, '/');
	sBackslash = strrchr(sPath, '\\');
	sTail = sSlash;
	if ( (sBackslash != NULL) && ((sTail == NULL) || (sBackslash > sTail)) ) {
		sTail = sBackslash;
	}

	if ( (sDot == NULL) || ((sTail != NULL) && (sDot < sTail)) ) {
		return "application/octet-stream";
	}
	return xl_mime_by_ext(sDot + 1);
}

int xl_mime_is_text(const char* sMime)
{
	if ( sMime == NULL ) {
		return FALSE;
	}
	if ( strstr(sMime, "text/") == sMime ) {
		return TRUE;
	}
	if ( strstr(sMime, "application/json") == sMime ) {
		return TRUE;
	}
	if ( strstr(sMime, "application/xml") == sMime ) {
		return TRUE;
	}
	if ( strstr(sMime, "application/javascript") == sMime ) {
		return TRUE;
	}
	if ( strstr(sMime, "image/svg+xml") == sMime ) {
		return TRUE;
	}
	return FALSE;
}

xvalue xl_mime_parse(const char* sText)
{
	xvalue value;
	const char* sCur;
	const char* sSemi;
	size_t iStart;
	size_t iLen;
	char* sType;

	if ( sText == NULL ) {
		sText = "";
	}

	value = xvoCreateTable();
	if ( value == NULL ) {
		xlrt_native_error_raise("MimeError", "mime parse failed", 0, sText);
		return NULL;
	}

	if ( !__xlMimeTableSetText(value, "type", "") ) {
		xvoUnref(value);
		xlrt_native_error_raise("MimeError", "mime parse failed", 0, sText);
		return NULL;
	}

	sSemi = strchr(sText, ';');
	iStart = 0u;
	iLen = (sSemi != NULL) ? (size_t)(sSemi - sText) : strlen(sText);
	__xlMimeTrimSpan(sText, &iStart, &iLen);
	sType = __xlMimeSliceLower(sText, iStart, iLen);
	if ( (sType == NULL) || !__xlMimeTableSetText(value, "type", sType) ) {
		free(sType);
		xvoUnref(value);
		xlrt_native_error_raise("MimeError", "mime parse failed", 0, sText);
		return NULL;
	}
	free(sType);

	sCur = (sSemi != NULL) ? (sSemi + 1) : NULL;
	while ( (sCur != NULL) && (*sCur != '\0') ) {
		const char* sNext = strchr(sCur, ';');
		const char* sEq;
		size_t iPartLen = (sNext != NULL) ? (size_t)(sNext - sCur) : strlen(sCur);
		size_t iKeyStart = 0u;
		size_t iKeyLen;
		size_t iValueStart;
		size_t iValueLen;
		char* sKey;
		char* sValue;

		sEq = memchr(sCur, '=', iPartLen);
		if ( sEq != NULL ) {
			iKeyLen = (size_t)(sEq - sCur);
			iValueStart = iKeyLen + 1u;
			iValueLen = iPartLen - iValueStart;
			__xlMimeTrimSpan(sCur, &iKeyStart, &iKeyLen);
			__xlMimeTrimSpan(sCur, &iValueStart, &iValueLen);
			if ( (iValueLen >= 2u) && (sCur[iValueStart] == '"') && (sCur[iValueStart + iValueLen - 1u] == '"') ) {
				++iValueStart;
				iValueLen -= 2u;
			}
			sKey = __xlMimeSliceLower(sCur, iKeyStart, iKeyLen);
			sValue = __xlMimeSlice(sCur, iValueStart, iValueLen);
			if ( (sKey == NULL) || (sValue == NULL) || !__xlMimeTableSetText(value, sKey, sValue) ) {
				free(sKey);
				free(sValue);
				xvoUnref(value);
				xlrt_native_error_raise("MimeError", "mime parse failed", 0, sText);
				return NULL;
			}
			free(sKey);
			free(sValue);
		}

		sCur = (sNext != NULL) ? (sNext + 1) : NULL;
	}

	return value;
}

const char* xl_mime_build(xvalue value)
{
	char* sBuf;
	size_t iCap;
	size_t iLen;
	const char* sType;
	void* pTable;
	Dict_Key* pKey;

	sType = __xlMimeTableGetText(value, "type");
	if ( (sType == NULL) || (sType[0] == '\0') ) {
		sType = "application/octet-stream";
	}

	iCap = strlen(sType) + 64u;
	sBuf = (char*)malloc(iCap);
	if ( sBuf == NULL ) {
		xlrt_native_error_raise("MimeError", "mime build failed", 0, NULL);
		return "";
	}

	iLen = (size_t)snprintf(sBuf, iCap, "%s", sType);
	pTable = (value != NULL) ? xvoGetTable(value) : NULL;
	if ( pTable != NULL ) {
		xrtAVLTB_IterBegin((xavltbase)pTable);
		for ( pKey = (Dict_Key*)xrtAVLTB_IterNext((xavltbase)pTable); pKey != NULL; pKey = (Dict_Key*)xrtAVLTB_IterNext((xavltbase)pTable) ) {
			xvalue itemValue = *(xvalue*)(&pKey[1]);
			const char* sKey = pKey->Key;
			const char* sParamValue = (const char*)xvoGetText(itemValue);
			size_t iNeed;

			if ( __xlMimeTextEqualIgnoreCase(sKey, "type") ) {
				continue;
			}

			iNeed = iLen + strlen(sKey) + ((sParamValue != NULL) ? strlen(sParamValue) : 0u) + 8u;
			if ( iNeed > iCap ) {
				char* sNewBuf;
				while ( iCap < iNeed ) {
					iCap *= 2u;
				}
				sNewBuf = (char*)realloc(sBuf, iCap);
				if ( sNewBuf == NULL ) {
					xrtAVLTB_IterEnd((xavltbase)pTable);
					free(sBuf);
					xlrt_native_error_raise("MimeError", "mime build failed", 0, NULL);
					return "";
				}
				sBuf = sNewBuf;
			}
			iLen += (size_t)snprintf(sBuf + iLen, iCap - iLen, "; %s=%s", sKey, (sParamValue != NULL) ? sParamValue : "");
		}
		xrtAVLTB_IterEnd((xavltbase)pTable);
	}

	return sBuf;
}
