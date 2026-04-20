#include "../common/xl_common.h"
#include "xlrt_generated.h"

static int __xlMultipartBufGrow(char** psBuf, size_t* piCap, size_t iNeed)
{
	char* sNewBuf;
	size_t iNewCap;

	if ( (psBuf == NULL) || (piCap == NULL) ) {
		return 0;
	}
	if ( (*psBuf != NULL) && (*piCap >= iNeed) ) {
		return 1;
	}

	iNewCap = (*piCap == 0u) ? 256u : *piCap;
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

static int __xlMultipartBufAppendN(char** psBuf, size_t* piLen, size_t* piCap, const char* sText, size_t iTextLen)
{
	size_t iNeed;

	if ( (psBuf == NULL) || (piLen == NULL) || (piCap == NULL) ) {
		return 0;
	}
	if ( sText == NULL ) {
		sText = "";
		iTextLen = 0u;
	}

	iNeed = *piLen + iTextLen + 1u;
	if ( !__xlMultipartBufGrow(psBuf, piCap, iNeed) ) {
		return 0;
	}

	if ( iTextLen != 0u ) {
		memcpy((*psBuf) + *piLen, sText, iTextLen);
	}
	*piLen += iTextLen;
	(*psBuf)[*piLen] = '\0';
	return 1;
}

static int __xlMultipartBufAppend(char** psBuf, size_t* piLen, size_t* piCap, const char* sText)
{
	return __xlMultipartBufAppendN(psBuf, piLen, piCap, sText, (sText != NULL) ? strlen(sText) : 0u);
}

static int __xlMultipartBufAppendFormat(char** psBuf, size_t* piLen, size_t* piCap, const char* sFormat, ...)
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
	if ( !__xlMultipartBufGrow(psBuf, piCap, iTotalNeed) ) {
		va_end(args2);
		return 0;
	}

	vsnprintf((*psBuf) + *piLen, *piCap - *piLen, sFormat, args2);
	va_end(args2);
	*piLen += (size_t)iNeed;
	return 1;
}

static char* __xlMultipartSlice(const char* sText, size_t iLen)
{
	char* sOut;

	sOut = (char*)malloc(iLen + 1u);
	if ( sOut == NULL ) {
		return NULL;
	}
	if ( (sText != NULL) && (iLen != 0u) ) {
		memcpy(sOut, sText, iLen);
	}
	sOut[iLen] = '\0';
	return sOut;
}

static void __xlMultipartTrimSpan(const char* sText, size_t* piStart, size_t* piLen)
{
	size_t iStart;
	size_t iLen;

	if ( (sText == NULL) || (piStart == NULL) || (piLen == NULL) ) {
		return;
	}

	iStart = *piStart;
	iLen = *piLen;
	while ( (iLen > 0u) && ((sText[iStart] == ' ') || (sText[iStart] == '\t') || (sText[iStart] == '\r') || (sText[iStart] == '\n')) ) {
		++iStart;
		--iLen;
	}
	while ( (iLen > 0u) && ((sText[iStart + iLen - 1u] == ' ') || (sText[iStart + iLen - 1u] == '\t') || (sText[iStart + iLen - 1u] == '\r') || (sText[iStart + iLen - 1u] == '\n')) ) {
		--iLen;
	}

	*piStart = iStart;
	*piLen = iLen;
}

static int __xlMultipartTextEqualIgnoreCase(const char* sLeft, const char* sRight)
{
	unsigned char chLeft;
	unsigned char chRight;

	if ( (sLeft == NULL) || (sRight == NULL) ) {
		return 0;
	}

	while ( (*sLeft != '\0') && (*sRight != '\0') ) {
		chLeft = (unsigned char)(*sLeft);
		chRight = (unsigned char)(*sRight);
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

static int __xlMultipartTableSetText(xvalue value, const char* sKey, const char* sText)
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

static const char* __xlMultipartTableGetText(xvalue value, const char* sKey)
{
	xvalue itemValue;
	const char* sText;

	if ( (value == NULL) || (sKey == NULL) ) {
		return "";
	}

	itemValue = xvoTableGetValue(value, (str)sKey, (uint32)strlen(sKey));
	if ( itemValue == NULL ) {
		return "";
	}
	sText = (const char*)xvoGetText(itemValue);
	return (sText != NULL) ? sText : "";
}

static int __xlMultipartAppendPart(xvalue parts, xvalue part)
{
	if ( (parts == NULL) || (part == NULL) ) {
		return 0;
	}
	return xvoArrayAppendValue(parts, part, TRUE) ? 1 : 0;
}

static char* __xlMultipartFindBoundaryParam(const char* sText)
{
	const char* sScan;

	if ( sText == NULL ) {
		return NULL;
	}

	sScan = sText;
	while ( *sScan != '\0' ) {
		const char* sEq = strchr(sScan, '=');
		const char* sSemi;

		if ( sEq == NULL ) {
			break;
		}

		sSemi = strchr(sEq + 1, ';');
		{
			size_t iKeyStart = 0u;
			size_t iKeyLen = (size_t)(sEq - sScan);
			size_t iValueStart = 0u;
			size_t iValueLen = (sSemi != NULL) ? (size_t)(sSemi - (sEq + 1)) : strlen(sEq + 1);
			char* sKey;
			char* sValue;

			__xlMultipartTrimSpan(sScan, &iKeyStart, &iKeyLen);
			sKey = __xlMultipartSlice(sScan + iKeyStart, iKeyLen);
			if ( sKey == NULL ) {
				return NULL;
			}
			if ( !__xlMultipartTextEqualIgnoreCase(sKey, "boundary") ) {
				free(sKey);
				if ( sSemi == NULL ) {
					break;
				}
				sScan = sSemi + 1;
				continue;
			}
			free(sKey);

			__xlMultipartTrimSpan(sEq + 1, &iValueStart, &iValueLen);
			if ( (iValueLen >= 2u) && ((sEq[1 + iValueStart] == '"') && (sEq[1 + iValueStart + iValueLen - 1u] == '"')) ) {
				++iValueStart;
				iValueLen -= 2u;
			}
			sValue = __xlMultipartSlice(sEq + 1 + iValueStart, iValueLen);
			return sValue;
		}
	}

	return NULL;
}

static int __xlMultipartSetIfText(xvalue part, const char* sKey, xvalue src)
{
	const char* sText;

	if ( (part == NULL) || (sKey == NULL) || (src == NULL) ) {
		return 1;
	}

	sText = (const char*)xvoGetText(src);
	if ( sText == NULL ) {
		sText = "";
	}
	if ( sText[0] == '\0' ) {
		return 1;
	}
	return __xlMultipartTableSetText(part, sKey, sText);
}

const char* xl_multipart_boundary(const char* sContentType)
{
	xvalue value;
	const char* sBoundary;
	char* sOut;

	value = xl_mime_parse(sContentType);
	if ( value == NULL ) {
		return "";
	}

	sBoundary = __xlMultipartTableGetText(value, "boundary");
	sOut = __xlMultipartSlice(sBoundary, strlen(sBoundary));
	xvoUnref(value);
	if ( sOut == NULL ) {
		return "";
	}
	return sOut;
}

xvalue xl_multipart_parse(const char* sText, const char* sBoundary)
{
	xvalue parts;
	char* sBoundaryOwned;
	char* sMarker;
	size_t iMarkerLen;
	const char* sCur;

	if ( sText == NULL ) {
		sText = "";
	}
	if ( (sBoundary == NULL) || (sBoundary[0] == '\0') ) {
		xlrt_native_error_raise("MultipartError", "multipart parse failed: boundary required", 0, sText);
		return NULL;
	}

	parts = xvoCreateArray();
	if ( parts == NULL ) {
		xlrt_native_error_raise("MultipartError", "multipart parse failed", 0, sText);
		return NULL;
	}

	sBoundaryOwned = __xlMultipartSlice(sBoundary, strlen(sBoundary));
	if ( sBoundaryOwned == NULL ) {
		xvoUnref(parts);
		xlrt_native_error_raise("MultipartError", "multipart parse failed", 0, sText);
		return NULL;
	}

	sMarker = (char*)malloc(strlen(sBoundaryOwned) + 3u);
	if ( sMarker == NULL ) {
		free(sBoundaryOwned);
		xvoUnref(parts);
		xlrt_native_error_raise("MultipartError", "multipart parse failed", 0, sText);
		return NULL;
	}
	snprintf(sMarker, strlen(sBoundaryOwned) + 3u, "--%s", sBoundaryOwned);
	iMarkerLen = strlen(sMarker);

	sCur = strstr(sText, sMarker);
	while ( sCur != NULL ) {
		const char* sPartStart;
		const char* sNextBoundary;
		const char* sHeaderEnd;
		const char* sBodyStart;
		const char* sBodyEnd;
		size_t iHeaderLen;
		size_t iBodyLen;
		char* sHeadersText;
		char* sBodyText;
		xvalue headers;
		xvalue part;
		xvalue disp;

		sCur += iMarkerLen;
		if ( (sCur[0] == '-') && (sCur[1] == '-') ) {
			break;
		}
		if ( (sCur[0] == '\r') && (sCur[1] == '\n') ) {
			sCur += 2;
		} else if ( sCur[0] == '\n' ) {
			sCur += 1;
		}

		sPartStart = sCur;
		sNextBoundary = strstr(sPartStart, sMarker);
		if ( sNextBoundary == NULL ) {
			break;
		}

		sHeaderEnd = strstr(sPartStart, "\r\n\r\n");
		if ( (sHeaderEnd == NULL) || (sHeaderEnd > sNextBoundary) ) {
			sHeaderEnd = strstr(sPartStart, "\n\n");
			if ( (sHeaderEnd == NULL) || (sHeaderEnd > sNextBoundary) ) {
				sCur = sNextBoundary;
				continue;
			}
			sBodyStart = sHeaderEnd + 2;
			iHeaderLen = (size_t)(sHeaderEnd - sPartStart);
		} else {
			sBodyStart = sHeaderEnd + 4;
			iHeaderLen = (size_t)(sHeaderEnd - sPartStart);
		}

		sBodyEnd = sNextBoundary;
		while ( (sBodyEnd > sBodyStart) && ((sBodyEnd[-1] == '\r') || (sBodyEnd[-1] == '\n')) ) {
			--sBodyEnd;
		}
		iBodyLen = (size_t)(sBodyEnd - sBodyStart);

		sHeadersText = __xlMultipartSlice(sPartStart, iHeaderLen);
		sBodyText = __xlMultipartSlice(sBodyStart, iBodyLen);
		headers = xl_http_parse_headers((sHeadersText != NULL) ? sHeadersText : "");
		part = xvoCreateTable();
		if ( (sHeadersText == NULL) || (sBodyText == NULL) || (headers == NULL) || (part == NULL) ) {
			free(sHeadersText);
			free(sBodyText);
			if ( headers != NULL ) {
				xvoUnref(headers);
			}
			if ( part != NULL ) {
				xvoUnref(part);
			}
			free(sMarker);
			free(sBoundaryOwned);
			xvoUnref(parts);
			xlrt_native_error_raise("MultipartError", "multipart parse failed", 0, sText);
			return NULL;
		}

		if ( !__xlMultipartTableSetText(part, "headers_text", sHeadersText) ||
			!__xlMultipartTableSetText(part, "body", sBodyText) ) {
			free(sHeadersText);
			free(sBodyText);
			xvoUnref(headers);
			xvoUnref(part);
			free(sMarker);
			free(sBoundaryOwned);
			xvoUnref(parts);
			xlrt_native_error_raise("MultipartError", "multipart parse failed", 0, sText);
			return NULL;
		}

		disp = xl_mime_parse(__xlMultipartTableGetText(headers, "Content-Disposition"));
		if ( disp != NULL ) {
			xvalue typeValue = xvoTableGetValue(disp, (str)"type", (uint32)4u);
			xvalue nameValue = xvoTableGetValue(disp, (str)"name", (uint32)4u);
			xvalue fileValue = xvoTableGetValue(disp, (str)"filename", (uint32)8u);

			if ( !__xlMultipartSetIfText(part, "disposition", typeValue) ||
				!__xlMultipartSetIfText(part, "name", nameValue) ||
				!__xlMultipartSetIfText(part, "filename", fileValue) ) {
				free(sHeadersText);
				free(sBodyText);
				xvoUnref(disp);
				xvoUnref(part);
				free(sMarker);
				free(sBoundaryOwned);
				xvoUnref(parts);
				xlrt_native_error_raise("MultipartError", "multipart parse failed", 0, sText);
				return NULL;
			}
			xvoUnref(disp);
		}
		if ( !__xlMultipartTableSetText(part, "type", __xlMultipartTableGetText(headers, "Content-Type")) ) {
			free(sHeadersText);
			free(sBodyText);
			xvoUnref(part);
			free(sMarker);
			free(sBoundaryOwned);
			xvoUnref(parts);
			xlrt_native_error_raise("MultipartError", "multipart parse failed", 0, sText);
			return NULL;
		}

		if ( !__xlMultipartAppendPart(parts, part) ) {
			free(sHeadersText);
			free(sBodyText);
			xvoUnref(part);
			free(sMarker);
			free(sBoundaryOwned);
			xvoUnref(parts);
			xlrt_native_error_raise("MultipartError", "multipart parse failed", 0, sText);
			return NULL;
		}
		free(sHeadersText);
		free(sBodyText);
		xvoUnref(headers);
		sCur = sNextBoundary;
	}

	free(sMarker);
	free(sBoundaryOwned);
	return parts;
}

static int __xlMultipartAppendHeaderLines(char** psBuf, size_t* piLen, size_t* piCap, xvalue headers)
{
	void* pTable;
	Dict_Key* pKey;

	pTable = (headers != NULL) ? xvoGetTable(headers) : NULL;
	if ( pTable == NULL ) {
		return 1;
	}

	xrtAVLTB_IterBegin((xavltbase)pTable);
	for ( pKey = (Dict_Key*)xrtAVLTB_IterNext((xavltbase)pTable); pKey != NULL; pKey = (Dict_Key*)xrtAVLTB_IterNext((xavltbase)pTable) ) {
		xvalue itemValue = *(xvalue*)(&pKey[1]);
		const char* sValue = (const char*)xvoGetText(itemValue);

		if ( sValue == NULL ) {
			sValue = "";
		}

		if ( !__xlMultipartBufAppendFormat(psBuf, piLen, piCap, "%s: %s\r\n", pKey->Key, sValue) ) {
			xrtAVLTB_IterEnd((xavltbase)pTable);
			return 0;
		}
	}
	xrtAVLTB_IterEnd((xavltbase)pTable);
	return 1;
}

static int __xlMultipartHasHeader(xvalue headers, const char* sNeedle)
{
	void* pTable;
	Dict_Key* pKey;

	pTable = (headers != NULL) ? xvoGetTable(headers) : NULL;
	if ( pTable == NULL ) {
		return 0;
	}

	xrtAVLTB_IterBegin((xavltbase)pTable);
	for ( pKey = (Dict_Key*)xrtAVLTB_IterNext((xavltbase)pTable); pKey != NULL; pKey = (Dict_Key*)xrtAVLTB_IterNext((xavltbase)pTable) ) {
		if ( __xlMultipartTextEqualIgnoreCase(pKey->Key, sNeedle) ) {
			xrtAVLTB_IterEnd((xavltbase)pTable);
			return 1;
		}
	}
	xrtAVLTB_IterEnd((xavltbase)pTable);
	return 0;
}

const char* xl_multipart_build(xvalue parts, const char* sBoundary)
{
	char* sBuf;
	size_t iLen;
	size_t iCap;
	uint32 i;
	uint32 iCount;

	if ( (sBoundary == NULL) || (sBoundary[0] == '\0') ) {
		xlrt_native_error_raise("MultipartError", "multipart build failed: boundary required", 0, NULL);
		return "";
	}

	sBuf = NULL;
	iLen = 0u;
	iCap = 0u;
	iCount = (parts != NULL) ? xvoArrayItemCount(parts) : 0u;
	for ( i = 0u; i < iCount; ++i ) {
		xvalue part = xvoArrayGetValue(parts, i);
		xvalue headers = xvoTableGetValue(part, (str)"headers", (uint32)7u);
		const char* sBody = __xlMultipartTableGetText(part, "body");
		const char* sName = __xlMultipartTableGetText(part, "name");
		const char* sFileName = __xlMultipartTableGetText(part, "filename");
		const char* sType = __xlMultipartTableGetText(part, "type");
		int bHasDisposition = __xlMultipartHasHeader(headers, "Content-Disposition");
		int bHasType = __xlMultipartHasHeader(headers, "Content-Type");

		if ( !__xlMultipartBufAppendFormat(&sBuf, &iLen, &iCap, "--%s\r\n", sBoundary) ) {
			free(sBuf);
			xlrt_native_error_raise("MultipartError", "multipart build failed", 0, NULL);
			return "";
		}
		if ( !bHasDisposition && ((sName[0] != '\0') || (sFileName[0] != '\0')) ) {
			if ( !__xlMultipartBufAppendFormat(&sBuf, &iLen, &iCap, "Content-Disposition: form-data") ) {
				free(sBuf);
				xlrt_native_error_raise("MultipartError", "multipart build failed", 0, NULL);
				return "";
			}
			if ( sName[0] != '\0' ) {
				if ( !__xlMultipartBufAppendFormat(&sBuf, &iLen, &iCap, "; name=\"%s\"", sName) ) {
					free(sBuf);
					xlrt_native_error_raise("MultipartError", "multipart build failed", 0, NULL);
					return "";
				}
			}
			if ( sFileName[0] != '\0' ) {
				if ( !__xlMultipartBufAppendFormat(&sBuf, &iLen, &iCap, "; filename=\"%s\"", sFileName) ) {
					free(sBuf);
					xlrt_native_error_raise("MultipartError", "multipart build failed", 0, NULL);
					return "";
				}
			}
			if ( !__xlMultipartBufAppend(&sBuf, &iLen, &iCap, "\r\n") ) {
				free(sBuf);
				xlrt_native_error_raise("MultipartError", "multipart build failed", 0, NULL);
				return "";
			}
		}
		if ( !bHasType && (sType[0] != '\0') ) {
			if ( !__xlMultipartBufAppendFormat(&sBuf, &iLen, &iCap, "Content-Type: %s\r\n", sType) ) {
				free(sBuf);
				xlrt_native_error_raise("MultipartError", "multipart build failed", 0, NULL);
				return "";
			}
		}
		if ( !__xlMultipartAppendHeaderLines(&sBuf, &iLen, &iCap, headers) ||
			!__xlMultipartBufAppend(&sBuf, &iLen, &iCap, "\r\n") ||
			!__xlMultipartBufAppend(&sBuf, &iLen, &iCap, sBody) ||
			!__xlMultipartBufAppend(&sBuf, &iLen, &iCap, "\r\n") ) {
			free(sBuf);
			xlrt_native_error_raise("MultipartError", "multipart build failed", 0, NULL);
			return "";
		}
	}

	if ( !__xlMultipartBufAppendFormat(&sBuf, &iLen, &iCap, "--%s--\r\n", sBoundary) ) {
		free(sBuf);
		xlrt_native_error_raise("MultipartError", "multipart build failed", 0, NULL);
		return "";
	}

	return (sBuf != NULL) ? sBuf : "";
}
