#include "xui_config.h"
#if XUI_ENABLE_FIND
#include "xui_internal.h"
#include "xui_xrt_port.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct xui_find_engine_t {
	char* sPattern;
	char* sReplacement;
	int iPatternLength;
	int iReplacementLength;
	uint32_t iFlags;
#ifndef XRT_NO_REGEX
	xui_xrt_regex_t* pRegex;
#endif
} xui_find_engine_t;

static int __xuiFindFlagsValid(uint32_t iFlags)
{
	return (iFlags & ~(XUI_FIND_CASE_SENSITIVE | XUI_FIND_WHOLE_WORD |
		XUI_FIND_BACKWARD | XUI_FIND_WRAP | XUI_FIND_MULTILINE |
		XUI_FIND_DOT_NEWLINE | XUI_FIND_REGEX | XUI_FIND_ESCAPE |
		XUI_FIND_SELECTION)) == 0u;
}

static void __xuiFindSetError(char* sError, int iCapacity, const char* sMessage)
{
	if ( sError == NULL || iCapacity <= 0 ) return;
	if ( sMessage == NULL ) sMessage = "";
	snprintf(sError, (size_t)iCapacity, "%s", sMessage);
}

static void __xuiFindSetStatusError(char* sError, int iCapacity, int iStatus)
{
	if ( sError == NULL || iCapacity <= 0 || sError[0] != '\0' || iStatus == XUI_OK ) return;
	switch ( iStatus ) {
	case XUI_ERROR_INVALID_ARGUMENT: __xuiFindSetError(sError, iCapacity, "invalid argument"); break;
	case XUI_ERROR_OUT_OF_MEMORY: __xuiFindSetError(sError, iCapacity, "out of memory"); break;
	case XUI_ERROR_LIMIT_EXCEEDED: __xuiFindSetError(sError, iCapacity, "result is too large"); break;
	case XUI_ERROR_INVALID_STATE: __xuiFindSetError(sError, iCapacity, "regex match failed"); break;
	case XUI_ERROR_UNSUPPORTED: __xuiFindSetError(sError, iCapacity, "not found"); break;
	default: __xuiFindSetError(sError, iCapacity, "find operation failed"); break;
	}
}

static int __xuiFindClamp(int iValue, int iMin, int iMax)
{
	if ( iValue < iMin ) return iMin;
	if ( iValue > iMax ) return iMax;
	return iValue;
}

static int __xuiFindResolveTextLength(const char* sText, int iTextLength, int* pResolvedLength)
{
	size_t iLength;

	if ( sText == NULL || pResolvedLength == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iTextLength >= 0 ) {
		*pResolvedLength = iTextLength;
		return XUI_OK;
	}
	iLength = strlen(sText);
	if ( iLength > (size_t)INT_MAX ) return XUI_ERROR_LIMIT_EXCEEDED;
	*pResolvedLength = (int)iLength;
	return XUI_OK;
}

static void __xuiFindNormalizeRange(int iTextLength, int* pRangeStart, int* pRangeEnd, int* pStartOffset)
{
	int iStart;
	int iEnd;

	iStart = (pRangeStart != NULL) ? *pRangeStart : 0;
	iEnd = (pRangeEnd != NULL) ? *pRangeEnd : iTextLength;
	if ( iEnd < 0 ) iEnd = iTextLength;
	iStart = __xuiFindClamp(iStart, 0, iTextLength);
	iEnd = __xuiFindClamp(iEnd, 0, iTextLength);
	if ( iEnd < iStart ) {
		int iSwap = iStart;
		iStart = iEnd;
		iEnd = iSwap;
	}
	if ( pRangeStart != NULL ) *pRangeStart = iStart;
	if ( pRangeEnd != NULL ) *pRangeEnd = iEnd;
	if ( pStartOffset != NULL ) *pStartOffset = __xuiFindClamp(*pStartOffset, iStart, iEnd);
}

static int __xuiFindHex(char c)
{
	if ( c >= '0' && c <= '9' ) return c - '0';
	if ( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
	if ( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
	return -1;
}

static int __xuiFindAppendBytes(char** psBuffer, int* pLength, int* pCapacity, const char* sText, int iTextLength)
{
	char* sNew;
	int iRequired;
	int iNewCapacity;

	if ( psBuffer == NULL || pLength == NULL || pCapacity == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iTextLength < 0 || *pLength < 0 || *pCapacity < 0 || *pLength > *pCapacity ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iTextLength == 0 ) return XUI_OK;
	if ( sText == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iTextLength > INT_MAX - *pLength - 1 ) return XUI_ERROR_LIMIT_EXCEEDED;
	iRequired = *pLength + iTextLength + 1;
	if ( iRequired > *pCapacity ) {
		iNewCapacity = (*pCapacity > 0) ? *pCapacity : 64;
		while ( iRequired > iNewCapacity ) {
			if ( iNewCapacity > INT_MAX / 2 ) {
				iNewCapacity = iRequired;
				break;
			}
			iNewCapacity *= 2;
		}
		sNew = (char*)xrtRealloc(*psBuffer, (size_t)iNewCapacity);
		if ( sNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
		*psBuffer = sNew;
		*pCapacity = iNewCapacity;
	}
	memcpy(*psBuffer + *pLength, sText, (size_t)iTextLength);
	*pLength += iTextLength;
	(*psBuffer)[*pLength] = '\0';
	return XUI_OK;
}

static int __xuiFindAppendByte(char** psBuffer, int* pLength, int* pCapacity, char c)
{
	return __xuiFindAppendBytes(psBuffer, pLength, pCapacity, &c, 1);
}

static int __xuiFindAppendUtf8(char** psBuffer, int* pLength, int* pCapacity, unsigned int iCodepoint)
{
	char sBytes[4];
	int iCount;

	if ( iCodepoint <= 0x7Fu ) {
		sBytes[0] = (char)iCodepoint;
		iCount = 1;
	} else if ( iCodepoint <= 0x7FFu ) {
		sBytes[0] = (char)(0xC0u | ((iCodepoint >> 6) & 0x1Fu));
		sBytes[1] = (char)(0x80u | (iCodepoint & 0x3Fu));
		iCount = 2;
	} else if ( iCodepoint >= 0xD800u && iCodepoint <= 0xDFFFu ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	} else if ( iCodepoint <= 0xFFFFu ) {
		sBytes[0] = (char)(0xE0u | ((iCodepoint >> 12) & 0x0Fu));
		sBytes[1] = (char)(0x80u | ((iCodepoint >> 6) & 0x3Fu));
		sBytes[2] = (char)(0x80u | (iCodepoint & 0x3Fu));
		iCount = 3;
	} else if ( iCodepoint <= 0x10FFFFu ) {
		sBytes[0] = (char)(0xF0u | ((iCodepoint >> 18) & 0x07u));
		sBytes[1] = (char)(0x80u | ((iCodepoint >> 12) & 0x3Fu));
		sBytes[2] = (char)(0x80u | ((iCodepoint >> 6) & 0x3Fu));
		sBytes[3] = (char)(0x80u | (iCodepoint & 0x3Fu));
		iCount = 4;
	} else {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return __xuiFindAppendBytes(psBuffer, pLength, pCapacity, sBytes, iCount);
}

static int __xuiFindParseHex4(const char* sText, int iTextLength, int iOffset, unsigned int* pValue)
{
	unsigned int iValue;
	int i;
	int iHex;

	if ( sText == NULL || pValue == NULL || iOffset < 0 || iTextLength - iOffset < 4 ) return XUI_ERROR_INVALID_ARGUMENT;
	iValue = 0u;
	for ( i = 0; i < 4; i++ ) {
		iHex = __xuiFindHex(sText[iOffset + i]);
		if ( iHex < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
		iValue = (iValue << 4) | (unsigned int)iHex;
	}
	*pValue = iValue;
	return XUI_OK;
}

static int __xuiFindAppendEscape(const char* sText, int iTextLength, int* pOffset,
	char** psOutput, int* pLength, int* pCapacity, char* sError, int iErrorCapacity)
{
	unsigned int iCodepoint;
	unsigned int iLowSurrogate;
	int iHex0;
	int iHex1;
	int iOffset;
	int iRet;

	if ( sText == NULL || pOffset == NULL || psOutput == NULL || pLength == NULL || pCapacity == NULL )
		return XUI_ERROR_INVALID_ARGUMENT;
	iOffset = *pOffset;
	if ( iOffset < 0 || iOffset >= iTextLength ) return XUI_ERROR_INVALID_ARGUMENT;
	switch ( sText[iOffset] ) {
	case 'n': return __xuiFindAppendByte(psOutput, pLength, pCapacity, '\n');
	case 'r': return __xuiFindAppendByte(psOutput, pLength, pCapacity, '\r');
	case 't': return __xuiFindAppendByte(psOutput, pLength, pCapacity, '\t');
	case '\\': return __xuiFindAppendByte(psOutput, pLength, pCapacity, '\\');
	case 'x':
		if ( iTextLength - iOffset < 3 ) {
			__xuiFindSetError(sError, iErrorCapacity, "invalid hex escape");
			return XUI_ERROR_INVALID_ARGUMENT;
		}
		iHex0 = __xuiFindHex(sText[iOffset + 1]);
		iHex1 = __xuiFindHex(sText[iOffset + 2]);
		if ( iHex0 < 0 || iHex1 < 0 ) {
			__xuiFindSetError(sError, iErrorCapacity, "invalid hex escape");
			return XUI_ERROR_INVALID_ARGUMENT;
		}
		if ( iHex0 == 0 && iHex1 == 0 ) {
			__xuiFindSetError(sError, iErrorCapacity, "nul escape is unsupported");
			return XUI_ERROR_INVALID_ARGUMENT;
		}
		*pOffset = iOffset + 2;
		return __xuiFindAppendByte(psOutput, pLength, pCapacity, (char)((iHex0 << 4) | iHex1));
	case 'u':
		iRet = __xuiFindParseHex4(sText, iTextLength, iOffset + 1, &iCodepoint);
		if ( iRet != XUI_OK ) {
			__xuiFindSetError(sError, iErrorCapacity, "invalid unicode escape");
			return iRet;
		}
		if ( iCodepoint >= 0xD800u && iCodepoint <= 0xDBFFu ) {
			if ( iTextLength - iOffset < 11 || sText[iOffset + 5] != '\\' || sText[iOffset + 6] != 'u' ||
			     __xuiFindParseHex4(sText, iTextLength, iOffset + 7, &iLowSurrogate) != XUI_OK ||
			     iLowSurrogate < 0xDC00u || iLowSurrogate > 0xDFFFu ) {
				__xuiFindSetError(sError, iErrorCapacity, "invalid unicode surrogate pair");
				return XUI_ERROR_INVALID_ARGUMENT;
			}
			iCodepoint = 0x10000u + ((iCodepoint - 0xD800u) << 10) + (iLowSurrogate - 0xDC00u);
			*pOffset = iOffset + 10;
		} else {
			if ( iCodepoint == 0u ) {
				__xuiFindSetError(sError, iErrorCapacity, "nul escape is unsupported");
				return XUI_ERROR_INVALID_ARGUMENT;
			}
			if ( iCodepoint >= 0xDC00u && iCodepoint <= 0xDFFFu ) {
				__xuiFindSetError(sError, iErrorCapacity, "invalid unicode surrogate pair");
				return XUI_ERROR_INVALID_ARGUMENT;
			}
			*pOffset = iOffset + 4;
		}
		return __xuiFindAppendUtf8(psOutput, pLength, pCapacity, iCodepoint);
	default:
		return __xuiFindAppendByte(psOutput, pLength, pCapacity, sText[iOffset]);
	}
}

static int __xuiFindDecodeEscapes(const char* sText, char** psOutput, char* sError, int iErrorCapacity)
{
	char* sOutput;
	size_t iSourceLength;
	int iTextLength;
	int iLength;
	int iCapacity;
	int i;
	int iRet;

	if ( sText == NULL || psOutput == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*psOutput = NULL;
	iSourceLength = strlen(sText);
	if ( iSourceLength > (size_t)INT_MAX ) {
		__xuiFindSetError(sError, iErrorCapacity, "text is too large");
		return XUI_ERROR_LIMIT_EXCEEDED;
	}
	iTextLength = (int)iSourceLength;
	sOutput = NULL;
	iLength = 0;
	iCapacity = 0;
	for ( i = 0; i < iTextLength; i++ ) {
		if ( sText[i] != '\\' ) {
			iRet = __xuiFindAppendByte(&sOutput, &iLength, &iCapacity, sText[i]);
			if ( iRet != XUI_OK ) goto fail;
			continue;
		}
		i++;
		if ( i >= iTextLength ) {
			iRet = __xuiFindAppendByte(&sOutput, &iLength, &iCapacity, '\\');
			if ( iRet != XUI_OK ) goto fail;
			break;
		}
		if ( sText[i] == '0' ) {
			__xuiFindSetError(sError, iErrorCapacity, "nul escape is unsupported");
			iRet = XUI_ERROR_INVALID_ARGUMENT;
			goto fail;
		}
		iRet = __xuiFindAppendEscape(sText, iTextLength, &i, &sOutput, &iLength, &iCapacity,
			sError, iErrorCapacity);
		if ( iRet != XUI_OK ) goto fail;
	}
	iRet = __xuiFindAppendBytes(&sOutput, &iLength, &iCapacity, "", 1);
	if ( iRet != XUI_OK ) goto fail;
	if ( iLength > 0 ) iLength--;
	if ( !xrtUtf8Valid((xstrview){sOutput, (size_t)iLength}, NULL) ) {
        __xuiFindSetError(sError, iErrorCapacity, "escape produces invalid UTF-8");
        iRet = XUI_ERROR_INVALID_ARGUMENT;
        goto fail;
    }
	*psOutput = sOutput;
	__xuiFindSetError(sError, iErrorCapacity, "");
	return XUI_OK;

fail:
	xrtFree(sOutput);
	if ( iRet == XUI_ERROR_OUT_OF_MEMORY ) __xuiFindSetError(sError, iErrorCapacity, "out of memory");
	return iRet;
}

static int __xuiFindDupMaybeEscaped(const char* sText, uint32_t iFlags, char** psOutput, char* sError, int iErrorCapacity)
{
	char* sCopy;
	size_t iLength;

	if ( sText == NULL || psOutput == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( (iFlags & XUI_FIND_ESCAPE) != 0 ) return __xuiFindDecodeEscapes(sText, psOutput, sError, iErrorCapacity);
	iLength = strlen(sText);
	if ( iLength > (size_t)INT_MAX ) {
		__xuiFindSetError(sError, iErrorCapacity, "text is too large");
		return XUI_ERROR_LIMIT_EXCEEDED;
	}
	sCopy = (char*)xrtMalloc(iLength + 1u);
	if ( sCopy == NULL ) {
		__xuiFindSetError(sError, iErrorCapacity, "out of memory");
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	memcpy(sCopy, sText, iLength + 1u);
	*psOutput = sCopy;
	__xuiFindSetError(sError, iErrorCapacity, "");
	return XUI_OK;
}

static int __xuiFindDupRaw(const char* sText, char** psOutput, char* sError, int iErrorCapacity)
{
	char* sCopy;
	size_t iLength;

	if ( sText == NULL || psOutput == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iLength = strlen(sText);
	if ( iLength > (size_t)INT_MAX ) {
		__xuiFindSetError(sError, iErrorCapacity, "text is too large");
		return XUI_ERROR_LIMIT_EXCEEDED;
	}
	sCopy = (char*)xrtMalloc(iLength + 1u);
	if ( sCopy == NULL ) {
		__xuiFindSetError(sError, iErrorCapacity, "out of memory");
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	memcpy(sCopy, sText, iLength + 1u);
	*psOutput = sCopy;
	return XUI_OK;
}

static int __xuiFindNextBoundary(const char* sText, int iTextLength, int iOffset)
{
	int iNext;

	if ( iOffset >= iTextLength ) return iTextLength;
	iNext = xuiInternalTextGraphemeNext(sText, iTextLength, iOffset);
	if ( iNext <= iOffset || iNext > iTextLength ) iNext = iOffset + 1;
	return iNext;
}

/* Regex empty matches advance one scalar, including combining marks. */
static int __xuiFindNextScalar(const char* sText, int iTextLength, int iOffset)
{
    size_t iRead = 0;
    uint32 iScalar = 0;
    if ( iOffset >= iTextLength ) return iTextLength;
    (void)xrtUtf8Decode((xstrview){sText + iOffset, (size_t)(iTextLength - iOffset)}, &iScalar, &iRead);
    if ( iRead == 0 || iRead > (size_t)(iTextLength - iOffset) ) iRead = 1;
    return iOffset + (int)iRead;
}

static int __xuiFindIsWholeWord(const char* sText, int iTextLength, int iStart, int iEnd)
{
	return xuiInternalTextWordBoundary(sText, iTextLength, iStart,
		XUI_INTERNAL_WORD_NATURAL) &&
		xuiInternalTextWordBoundary(sText, iTextLength, iEnd,
		XUI_INTERNAL_WORD_NATURAL);
}

static int __xuiFindPlainInRange(const char* sText, int iTextLength, const xui_find_engine_t* pEngine, int iStart, int iEnd, xui_find_result_t* pResult)
{
	xstrview tPattern;
	xstrview tRange;
	size_t iFound;
	size_t iSearchStart;
	int iOffset;
	int iLastOffset;

	if ( pEngine->iPatternLength <= 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iStart < 0 || iEnd < iStart || iEnd > iTextLength ||
	     pEngine->iPatternLength > iEnd - iStart ) return XUI_ERROR_UNSUPPORTED;
	tPattern = xuiXrtText(pEngine->sPattern, (size_t)pEngine->iPatternLength);
	if ( (pEngine->iFlags & XUI_FIND_BACKWARD) != 0 ) {
		tRange = xuiXrtText(sText + iStart, (size_t)(iEnd - iStart));
		if ( (pEngine->iFlags & XUI_FIND_WHOLE_WORD) == 0 ) {
			iFound = ((pEngine->iFlags & XUI_FIND_CASE_SENSITIVE) != 0) ?
				xrtStrRFind(tRange, tPattern) : xrtStrCaseRFind(tRange, tPattern);
			if ( iFound == XRT_NPOS ) return XUI_ERROR_UNSUPPORTED;
			iOffset = iStart + (int)iFound;
		} else {
			iSearchStart = 0u;
			iLastOffset = -1;
			while ( iSearchStart + (size_t)pEngine->iPatternLength <= tRange.Size ) {
				iFound = ((pEngine->iFlags & XUI_FIND_CASE_SENSITIVE) != 0) ?
					xrtStrFind(tRange, tPattern, iSearchStart) :
					xrtStrCaseFind(tRange, tPattern, iSearchStart);
				if ( iFound == XRT_NPOS ) break;
				iOffset = iStart + (int)iFound;
				if ( __xuiFindIsWholeWord(sText, iTextLength, iOffset,
					iOffset + pEngine->iPatternLength) ) iLastOffset = iOffset;
				iSearchStart = iFound + 1u;
			}
			if ( iLastOffset < 0 ) return XUI_ERROR_UNSUPPORTED;
			iOffset = iLastOffset;
		}
		memset(pResult, 0, sizeof(*pResult));
		pResult->iSize = sizeof(*pResult);
		pResult->iStart = iOffset;
		pResult->iEnd = iOffset + pEngine->iPatternLength;
		return XUI_OK;
	}
	tRange = xuiXrtText(sText + iStart, (size_t)(iEnd - iStart));
	iSearchStart = 0u;
	while ( iSearchStart + (size_t)pEngine->iPatternLength <= tRange.Size ) {
		iFound = ((pEngine->iFlags & XUI_FIND_CASE_SENSITIVE) != 0) ?
			xrtStrFind(tRange, tPattern, iSearchStart) : xrtStrCaseFind(tRange, tPattern, iSearchStart);
		if ( iFound == XRT_NPOS ) return XUI_ERROR_UNSUPPORTED;
		iOffset = iStart + (int)iFound;
		if ( (pEngine->iFlags & XUI_FIND_WHOLE_WORD) == 0 ||
		     __xuiFindIsWholeWord(sText, iTextLength, iOffset, iOffset + pEngine->iPatternLength) ) {
			memset(pResult, 0, sizeof(*pResult));
			pResult->iSize = sizeof(*pResult);
			pResult->iStart = iOffset;
			pResult->iEnd = iOffset + pEngine->iPatternLength;
			return XUI_OK;
		}
		iSearchStart = iFound + 1u;
	}
	return XUI_ERROR_UNSUPPORTED;
}

#ifndef XRT_NO_REGEX
static int __xuiFindRegexCreate(xui_find_engine_t* pEngine, char* sError, int iErrorCapacity)
{
	uint32 iRegexFlags;
	int iRet;

	pEngine->pRegex = NULL;
	iRegexFlags = 0;
	if ( (pEngine->iFlags & XUI_FIND_CASE_SENSITIVE) == 0 ) iRegexFlags |= 1u;
	if ( (pEngine->iFlags & XUI_FIND_MULTILINE) != 0 ) iRegexFlags |= 2u;
	if ( (pEngine->iFlags & XUI_FIND_DOT_NEWLINE) != 0 ) iRegexFlags |= 4u;
	iRet = xuiXrtRegexCreateChecked(pEngine->sPattern, strlen(pEngine->sPattern),
		iRegexFlags, &pEngine->pRegex);
	if ( iRet != XUI_OK ) {
		if ( iRet == XUI_ERROR_OUT_OF_MEMORY ) {
			__xuiFindSetError(sError, iErrorCapacity, "out of memory");
			return iRet;
		}
		__xuiFindSetError(sError, iErrorCapacity, "regex compile failed");
		return iRet;
	}
	return XUI_OK;
}

static int __xuiFindRegexFillResult(xui_find_engine_t* pEngine, const char* sText, int iTextLength, xregexspan tSpan, xui_find_result_t* pResult)
{
	xregexcapture tCapture;
	uint32_t iCaptureCount;
	uint32_t i;

	if ( (pEngine->iFlags & XUI_FIND_WHOLE_WORD) != 0 ) {
		if ( !__xuiFindIsWholeWord(sText, iTextLength, (int)tSpan.Begin, (int)tSpan.End) ) return XUI_ERROR_UNSUPPORTED;
	}
	memset(pResult, 0, sizeof(*pResult));
	pResult->iSize = sizeof(*pResult);
	pResult->iStart = (int)tSpan.Begin;
	pResult->iEnd = (int)tSpan.End;
	iCaptureCount = (uint32_t)xrtRegexCaptureCount(pEngine->pRegex->pRegex);
	if ( iCaptureCount > XUI_FIND_MAX_CAPTURES ) iCaptureCount = XUI_FIND_MAX_CAPTURES;
	pResult->iCaptureCount = (int)iCaptureCount;
	for ( i = 0; i < iCaptureCount; i++ ) {
		if ( xrtRegexMatcherCapture(pEngine->pRegex->pMatcher, i, &tCapture) && tCapture.Matched ) {
			pResult->arrCaptures[i].iStart = (int)tCapture.Span.Begin;
			pResult->arrCaptures[i].iEnd = (int)tCapture.Span.End;
		} else {
			pResult->arrCaptures[i].iStart = -1;
			pResult->arrCaptures[i].iEnd = -1;
		}
	}
	return XUI_OK;
}

static int __xuiFindRegexForward(xui_find_engine_t* pEngine, const char* sText, int iTextLength, int iStart, int iEnd, xui_find_result_t* pResult)
{
	xregexspan tSpan = { 0 };
	int iNext;
	int iPos;
	int iRet;

	iPos = iStart;
	while ( iPos <= iEnd ) {
		iRet = xuiXrtRegexFindAt(pEngine->pRegex, sText, (size_t)iEnd, (size_t)iPos, &tSpan);
		if ( iRet == XREGEX_ERROR ) return xuiXrtRegexFailureStatus();
		if ( iRet != XREGEX_MATCH || (int)tSpan.Begin > iEnd ) return XUI_ERROR_UNSUPPORTED;
		if ( (int)tSpan.End <= iEnd && __xuiFindRegexFillResult(pEngine, sText, iTextLength, tSpan, pResult) == XUI_OK ) return XUI_OK;
		iNext = __xuiFindNextScalar(sText, iTextLength, (int)tSpan.Begin);
		if ( iNext <= (int)tSpan.Begin ) break;
		iPos = iNext;
	}
	return XUI_ERROR_UNSUPPORTED;
}

static int __xuiFindRegexBackward(xui_find_engine_t* pEngine, const char* sText, int iTextLength, int iStart, int iEnd, xui_find_result_t* pResult)
{
	xui_find_result_t tLast;
	xui_find_result_t tCurrent;
	int iPos;
	int iFound;
	int iRet;

	iFound = 0;
	iPos = iStart;
	while ( (iRet = __xuiFindRegexForward(pEngine, sText, iTextLength, iPos, iEnd, &tCurrent)) == XUI_OK ) {
		if ( tCurrent.iEnd > iEnd ) break;
		tLast = tCurrent;
		iFound = 1;
		iPos = __xuiFindNextScalar(sText, iTextLength, tCurrent.iStart);
		if ( iPos <= tCurrent.iStart ) break;
	}
	if ( iRet != XUI_ERROR_UNSUPPORTED && iRet != XUI_OK ) return iRet;
	if ( !iFound ) return XUI_ERROR_UNSUPPORTED;
	*pResult = tLast;
	return XUI_OK;
}
#endif

static int __xuiFindAppendReplacement(xui_find_engine_t* pEngine, char** psOutput, int* pLength,
    int* pCapacity, const char* sText, const xui_find_result_t* pResult,
    char* sError, int iErrorCapacity);

static void __xuiFindEngineDestroy(xui_find_engine_t* pEngine);

static int __xuiFindEngineCreate(xui_find_engine_t* pEngine, const char* sPattern, const char* sReplacement, uint32_t iFlags, char* sError, int iErrorCapacity)
{
	int iRet;

	if ( pEngine == NULL || sPattern == NULL || sPattern[0] == '\0' ||
	     !__xuiFindFlagsValid(iFlags) ) {
		__xuiFindSetError(sError, iErrorCapacity, "invalid argument");
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(pEngine, 0, sizeof(*pEngine));
	pEngine->iFlags = iFlags;
	iRet = __xuiFindDupMaybeEscaped(sPattern, iFlags, &pEngine->sPattern, sError, iErrorCapacity);
	if ( iRet != XUI_OK ) goto fail;
	pEngine->iPatternLength = (int)strlen(pEngine->sPattern);
	if ( pEngine->iPatternLength <= 0 ) {
		__xuiFindSetError(sError, iErrorCapacity, "empty pattern");
		iRet = XUI_ERROR_INVALID_ARGUMENT;
		goto fail;
	}
	if ( sReplacement != NULL ) {
		if ( (iFlags & XUI_FIND_REGEX) != 0 ) {
			iRet = __xuiFindDupRaw(sReplacement, &pEngine->sReplacement, sError, iErrorCapacity);
		} else {
			iRet = __xuiFindDupMaybeEscaped(sReplacement, iFlags, &pEngine->sReplacement, sError, iErrorCapacity);
		}
		if ( iRet != XUI_OK ) goto fail;
		pEngine->iReplacementLength = (int)strlen(pEngine->sReplacement);
        if ( (iFlags & (XUI_FIND_REGEX | XUI_FIND_ESCAPE)) == (XUI_FIND_REGEX | XUI_FIND_ESCAPE) ) {
            xui_find_result_t tEmpty = {0};
            char* sValidation = NULL;
            int iLength = 0, iCapacity = 0;
            iRet = __xuiFindAppendReplacement(pEngine, &sValidation, &iLength, &iCapacity,
                "", &tEmpty, sError, iErrorCapacity);
            if ( iRet == XUI_OK && !xrtUtf8Valid((xstrview){sValidation, (size_t)iLength}, NULL) ) {
                __xuiFindSetError(sError, iErrorCapacity, "invalid replacement escape");
                iRet = XUI_ERROR_INVALID_ARGUMENT;
            }
            xrtFree(sValidation);
            if ( iRet != XUI_OK ) goto fail;
        }
	}
	if ( (iFlags & XUI_FIND_REGEX) != 0 ) {
#ifdef XRT_NO_REGEX
		__xuiFindSetError(sError, iErrorCapacity, "XRT regex support is disabled");
		iRet = XUI_ERROR_UNSUPPORTED;
		goto fail;
#else
		iRet = __xuiFindRegexCreate(pEngine, sError, iErrorCapacity);
		if ( iRet != XUI_OK ) goto fail;
#endif
	}
	return XUI_OK;

fail:
	__xuiFindEngineDestroy(pEngine);
	return iRet;
}

static void __xuiFindEngineDestroy(xui_find_engine_t* pEngine)
{
	if ( pEngine == NULL ) return;
#ifndef XRT_NO_REGEX
	if ( pEngine->pRegex != NULL ) xuiXrtRegexDestroy(pEngine->pRegex);
#endif
	xrtFree(pEngine->sPattern);
	xrtFree(pEngine->sReplacement);
	memset(pEngine, 0, sizeof(*pEngine));
}

static int __xuiFindEngineFind(const xui_find_engine_t* pEngineConst, const char* sText, int iTextLength, int iStartOffset, int iRangeStart, int iRangeEnd, xui_find_result_t* pResult)
{
	xui_find_engine_t* pEngine;
	uint32_t iFlags;
	int iRet;

	if ( pEngineConst == NULL || sText == NULL || pResult == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pEngine = (xui_find_engine_t*)pEngineConst;
	iFlags = pEngine->iFlags;
	iRangeStart = __xuiFindClamp(iRangeStart, 0, iTextLength);
	iRangeEnd = __xuiFindClamp(iRangeEnd, 0, iTextLength);
	iStartOffset = __xuiFindClamp(iStartOffset, iRangeStart, iRangeEnd);
	if ( (iFlags & XUI_FIND_REGEX) != 0 ) {
#ifdef XRT_NO_REGEX
		return XUI_ERROR_UNSUPPORTED;
#else
		if ( (iFlags & XUI_FIND_BACKWARD) != 0 ) {
			iRet = __xuiFindRegexBackward(pEngine, sText, iTextLength, iRangeStart, iStartOffset, pResult);
			if ( iRet == XUI_OK || (iFlags & XUI_FIND_WRAP) == 0 ) return iRet;
			return __xuiFindRegexBackward(pEngine, sText, iTextLength, iStartOffset, iRangeEnd, pResult);
		}
		iRet = __xuiFindRegexForward(pEngine, sText, iTextLength, iStartOffset, iRangeEnd, pResult);
		if ( iRet == XUI_OK || (iFlags & XUI_FIND_WRAP) == 0 ) return iRet;
		return __xuiFindRegexForward(pEngine, sText, iTextLength, iRangeStart, iStartOffset, pResult);
#endif
	}
	if ( (iFlags & XUI_FIND_BACKWARD) != 0 ) {
		iRet = __xuiFindPlainInRange(sText, iTextLength, pEngine, iRangeStart, iStartOffset, pResult);
		if ( iRet == XUI_OK || (iFlags & XUI_FIND_WRAP) == 0 ) return iRet;
		return __xuiFindPlainInRange(sText, iTextLength, pEngine, iStartOffset, iRangeEnd, pResult);
	}
	iRet = __xuiFindPlainInRange(sText, iTextLength, pEngine, iStartOffset, iRangeEnd, pResult);
	if ( iRet == XUI_OK || (iFlags & XUI_FIND_WRAP) == 0 ) return iRet;
	return __xuiFindPlainInRange(sText, iTextLength, pEngine, iRangeStart, iStartOffset, pResult);
}

static int __xuiFindAppendReplacement(xui_find_engine_t* pEngine, char** psOutput, int* pLength,
	int* pCapacity, const char* sText, const xui_find_result_t* pResult,
	char* sError, int iErrorCapacity)
{
	const char* sReplacement;
	int i;
	int iCapture;
	int iRet;

	if ( pEngine == NULL || psOutput == NULL || pLength == NULL || pCapacity == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	sReplacement = (pEngine->sReplacement != NULL) ? pEngine->sReplacement : "";
	if ( (pEngine->iFlags & XUI_FIND_REGEX) == 0 ) {
		return __xuiFindAppendBytes(psOutput, pLength, pCapacity, sReplacement, pEngine->iReplacementLength);
	}
	for ( i = 0; i < pEngine->iReplacementLength; i++ ) {
		if ( sReplacement[i] == '$' && i + 1 < pEngine->iReplacementLength &&
		     sReplacement[i + 1] >= '0' && sReplacement[i + 1] <= '9' ) {
			iCapture = sReplacement[i + 1] - '0';
			i++;
			if ( iCapture < pResult->iCaptureCount && pResult->arrCaptures[iCapture].iStart >= 0 ) {
				iRet = __xuiFindAppendBytes(psOutput, pLength, pCapacity, sText + pResult->arrCaptures[iCapture].iStart, pResult->arrCaptures[iCapture].iEnd - pResult->arrCaptures[iCapture].iStart);
				if ( iRet != XUI_OK ) return iRet;
			}
			continue;
		}
		if ( sReplacement[i] == '\\' ) {
			if ( i + 1 < pEngine->iReplacementLength &&
			     sReplacement[i + 1] >= '0' && sReplacement[i + 1] <= '9' ) {
				iCapture = sReplacement[i + 1] - '0';
				i++;
				if ( iCapture < pResult->iCaptureCount && pResult->arrCaptures[iCapture].iStart >= 0 ) {
					iRet = __xuiFindAppendBytes(psOutput, pLength, pCapacity, sText + pResult->arrCaptures[iCapture].iStart, pResult->arrCaptures[iCapture].iEnd - pResult->arrCaptures[iCapture].iStart);
					if ( iRet != XUI_OK ) return iRet;
				}
				continue;
			}
			if ( (pEngine->iFlags & XUI_FIND_ESCAPE) != 0 ) {
				i++;
				if ( i >= pEngine->iReplacementLength ) return __xuiFindAppendByte(psOutput, pLength, pCapacity, '\\');
				iRet = __xuiFindAppendEscape(sReplacement, pEngine->iReplacementLength, &i,
					psOutput, pLength, pCapacity, sError, iErrorCapacity);
				if ( iRet != XUI_OK ) return iRet;
				continue;
			}
		}
		iRet = __xuiFindAppendByte(psOutput, pLength, pCapacity, sReplacement[i]);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiFindValidateResult(const xui_find_result_t* pResult, int iTextLength)
{
	int i;
	int iStart;
	int iEnd;

	if ( pResult == NULL || pResult->iStart < 0 || pResult->iEnd < pResult->iStart ||
	     pResult->iEnd > iTextLength || pResult->iCaptureCount < 0 ||
	     pResult->iCaptureCount > XUI_FIND_MAX_CAPTURES ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < pResult->iCaptureCount; i++ ) {
		iStart = pResult->arrCaptures[i].iStart;
		iEnd = pResult->arrCaptures[i].iEnd;
		if ( iStart == -1 && iEnd == -1 ) continue;
		if ( iStart < 0 || iEnd < iStart || iEnd > iTextLength ) return XUI_ERROR_INVALID_ARGUMENT;
	}
	return XUI_OK;
}

XUI_API int xuiFindText(const char* sText, int iTextLength, const char* sPattern, int iStartOffset, int iRangeStart, int iRangeEnd, uint32_t iFlags, xui_find_result_t* pResult, char* sError, int iErrorCapacity)
{
	xui_find_engine_t tEngine;
	int iRet;

	__xuiFindSetError(sError, iErrorCapacity, "");
	if ( pResult != NULL ) memset(pResult, 0, sizeof(*pResult));
	if ( sText == NULL || sPattern == NULL || pResult == NULL ) {
		__xuiFindSetStatusError(sError, iErrorCapacity, XUI_ERROR_INVALID_ARGUMENT);
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiFindResolveTextLength(sText, iTextLength, &iTextLength);
	if ( iRet != XUI_OK ) {
		__xuiFindSetStatusError(sError, iErrorCapacity, iRet);
		return iRet;
	}
	__xuiFindNormalizeRange(iTextLength, &iRangeStart, &iRangeEnd, &iStartOffset);
	memset(&tEngine, 0, sizeof(tEngine));
	iRet = __xuiFindEngineCreate(&tEngine, sPattern, NULL, iFlags, sError, iErrorCapacity);
	if ( iRet == XUI_OK ) iRet = __xuiFindEngineFind(&tEngine, sText, iTextLength, iStartOffset, iRangeStart, iRangeEnd, pResult);
	__xuiFindEngineDestroy(&tEngine);
	__xuiFindSetStatusError(sError, iErrorCapacity, iRet);
	return iRet;
}

XUI_API int xuiFindCollectText(const char* sText, int iTextLength, const char* sPattern, int iRangeStart, int iRangeEnd, uint32_t iFlags, xui_find_result_t* pResults, int iResultCapacity, int* pResultCount, char* sError, int iErrorCapacity)
{
	xui_find_engine_t tEngine;
	xui_find_result_t tResult;
	int iOffset;
	int iNext;
	int iCount;
	int iRet;

	__xuiFindSetError(sError, iErrorCapacity, "");
	if ( pResultCount != NULL ) *pResultCount = 0;
	if ( sText == NULL || sPattern == NULL || pResultCount == NULL || iResultCapacity < 0 || (iResultCapacity > 0 && pResults == NULL) ) {
		__xuiFindSetStatusError(sError, iErrorCapacity, XUI_ERROR_INVALID_ARGUMENT);
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiFindResolveTextLength(sText, iTextLength, &iTextLength);
	if ( iRet != XUI_OK ) {
		__xuiFindSetStatusError(sError, iErrorCapacity, iRet);
		return iRet;
	}
	iOffset = iRangeStart;
	__xuiFindNormalizeRange(iTextLength, &iRangeStart, &iRangeEnd, &iOffset);
	iFlags &= ~(XUI_FIND_BACKWARD | XUI_FIND_WRAP);
	iCount = 0;
	memset(&tEngine, 0, sizeof(tEngine));
	iRet = __xuiFindEngineCreate(&tEngine, sPattern, NULL, iFlags, sError, iErrorCapacity);
	if ( iRet != XUI_OK ) {
		__xuiFindSetStatusError(sError, iErrorCapacity, iRet);
		return iRet;
	}
	iOffset = iRangeStart;
	for ( ;; ) {
		iRet = __xuiFindEngineFind(&tEngine, sText, iTextLength, iOffset, iRangeStart, iRangeEnd, &tResult);
		if ( iRet == XUI_ERROR_UNSUPPORTED ) break;
		if ( iRet != XUI_OK ) goto cleanup;
		if ( pResults != NULL && iCount < iResultCapacity ) pResults[iCount] = tResult;
		if ( iCount == INT_MAX ) {
			iRet = XUI_ERROR_LIMIT_EXCEEDED;
			goto cleanup;
		}
		iCount++;
		iNext = (tResult.iEnd > tResult.iStart) ? tResult.iEnd :
			__xuiFindNextScalar(sText, iTextLength, tResult.iStart);
		if ( iNext <= tResult.iStart ) break;
		iOffset = iNext;
		if ( iOffset > iRangeEnd ) break;
	}
	*pResultCount = iCount;
	__xuiFindEngineDestroy(&tEngine);
	__xuiFindSetError(sError, iErrorCapacity, "");
	return XUI_OK;

cleanup:
	__xuiFindEngineDestroy(&tEngine);
	__xuiFindSetStatusError(sError, iErrorCapacity, iRet);
	return iRet;
}

XUI_API int xuiFindReplaceAllText(const char* sText, int iTextLength, const char* sPattern, const char* sReplacement, int iRangeStart, int iRangeEnd, uint32_t iFlags, char** psOutput, int* pOutputLength, int* pReplaceCount, char* sError, int iErrorCapacity)
{
	xui_find_engine_t tEngine;
	xui_find_result_t tResult;
	char* sOutput;
	int iOutputLength;
	int iCapacity;
	int iCursor;
	int iNext;
	int bNextBeyondRange;
	int iCount;
	int iRet;

	__xuiFindSetError(sError, iErrorCapacity, "");
	if ( psOutput != NULL ) *psOutput = NULL;
	if ( pOutputLength != NULL ) *pOutputLength = 0;
	if ( pReplaceCount != NULL ) *pReplaceCount = 0;
	if ( sText == NULL || sPattern == NULL || sReplacement == NULL || psOutput == NULL ) {
		__xuiFindSetStatusError(sError, iErrorCapacity, XUI_ERROR_INVALID_ARGUMENT);
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiFindResolveTextLength(sText, iTextLength, &iTextLength);
	if ( iRet != XUI_OK ) {
		__xuiFindSetStatusError(sError, iErrorCapacity, iRet);
		return iRet;
	}
	iCursor = iRangeStart;
	__xuiFindNormalizeRange(iTextLength, &iRangeStart, &iRangeEnd, &iCursor);
	iFlags &= ~(XUI_FIND_BACKWARD | XUI_FIND_WRAP);
	memset(&tEngine, 0, sizeof(tEngine));
	iRet = __xuiFindEngineCreate(&tEngine, sPattern, sReplacement, iFlags, sError, iErrorCapacity);
	if ( iRet != XUI_OK ) {
		__xuiFindSetStatusError(sError, iErrorCapacity, iRet);
		return iRet;
	}
	sOutput = NULL;
	iOutputLength = 0;
	iCapacity = 0;
	iCount = 0;
	iRet = __xuiFindAppendBytes(&sOutput, &iOutputLength, &iCapacity, sText, iRangeStart);
	if ( iRet != XUI_OK ) goto cleanup;
	iCursor = iRangeStart;
	for ( ;; ) {
		iRet = __xuiFindEngineFind(&tEngine, sText, iTextLength, iCursor, iRangeStart, iRangeEnd, &tResult);
		if ( iRet == XUI_ERROR_UNSUPPORTED ) break;
		if ( iRet != XUI_OK ) goto cleanup;
		iRet = __xuiFindAppendBytes(&sOutput, &iOutputLength, &iCapacity, sText + iCursor, tResult.iStart - iCursor);
		if ( iRet == XUI_OK ) iRet = __xuiFindAppendReplacement(&tEngine, &sOutput, &iOutputLength,
			&iCapacity, sText, &tResult, sError, iErrorCapacity);
		if ( iRet != XUI_OK ) goto cleanup;
		if ( iCount == INT_MAX ) {
			iRet = XUI_ERROR_LIMIT_EXCEEDED;
			goto cleanup;
		}
		iCount++;
		if ( tResult.iEnd > tResult.iStart ) {
			iCursor = tResult.iEnd;
		} else {
			iNext = __xuiFindNextScalar(sText, iTextLength, tResult.iStart);
			bNextBeyondRange = iNext > iRangeEnd;
			if ( iNext > iRangeEnd ) iNext = iRangeEnd;
			if ( iNext > tResult.iStart ) {
				iRet = __xuiFindAppendBytes(&sOutput, &iOutputLength, &iCapacity,
					sText + tResult.iStart, iNext - tResult.iStart);
				if ( iRet != XUI_OK ) goto cleanup;
			}
			iCursor = iNext;
			if ( tResult.iStart >= iRangeEnd || bNextBeyondRange ) break;
		}
		if ( iCursor > iRangeEnd ) break;
	}
	iRet = __xuiFindAppendBytes(&sOutput, &iOutputLength, &iCapacity, sText + iCursor, iRangeEnd - iCursor);
	if ( iRet == XUI_OK ) iRet = __xuiFindAppendBytes(&sOutput, &iOutputLength, &iCapacity, sText + iRangeEnd, iTextLength - iRangeEnd);
	if ( iRet == XUI_OK ) iRet = __xuiFindAppendBytes(&sOutput, &iOutputLength, &iCapacity, "", 1);
	if ( iRet != XUI_OK ) goto cleanup;
	if ( iOutputLength > 0 ) iOutputLength--;
	*psOutput = sOutput;
	if ( pOutputLength != NULL ) *pOutputLength = iOutputLength;
	if ( pReplaceCount != NULL ) *pReplaceCount = iCount;
	__xuiFindSetError(sError, iErrorCapacity, "");
	__xuiFindEngineDestroy(&tEngine);
	return XUI_OK;

cleanup:
	xrtFree(sOutput);
	__xuiFindEngineDestroy(&tEngine);
	__xuiFindSetStatusError(sError, iErrorCapacity, iRet);
	return iRet;
}

XUI_API int xuiFindBuildReplacement(const char* sText, int iTextLength, const char* sPattern,
	const char* sReplacement, uint32_t iFlags, const xui_find_result_t* pResult,
	char** psOutput, int* pOutputLength, char* sError, int iErrorCapacity)
{
	xui_find_engine_t tEngine;
	char* sOutput = NULL;
	int iLength = 0;
	int iCapacity = 0;
	int iRet;
	__xuiFindSetError(sError, iErrorCapacity, "");
	if ( psOutput != NULL ) *psOutput = NULL;
	if ( pOutputLength != NULL ) *pOutputLength = 0;
	if ( sText == NULL || sPattern == NULL || sReplacement == NULL || pResult == NULL || psOutput == NULL ) {
		__xuiFindSetStatusError(sError, iErrorCapacity, XUI_ERROR_INVALID_ARGUMENT);
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiFindResolveTextLength(sText, iTextLength, &iTextLength);
	if ( iRet == XUI_OK ) iRet = __xuiFindValidateResult(pResult, iTextLength);
	if ( iRet != XUI_OK ) {
		__xuiFindSetStatusError(sError, iErrorCapacity, iRet);
		return iRet;
	}
	memset(&tEngine, 0, sizeof(tEngine));
	iRet = __xuiFindEngineCreate(&tEngine, sPattern, sReplacement, iFlags, sError, iErrorCapacity);
	if ( iRet == XUI_OK ) iRet = __xuiFindAppendReplacement(&tEngine, &sOutput, &iLength,
		&iCapacity, sText, pResult, sError, iErrorCapacity);
	if ( iRet == XUI_OK ) iRet = __xuiFindAppendBytes(&sOutput, &iLength, &iCapacity, "", 1);
	__xuiFindEngineDestroy(&tEngine);
	if ( iRet != XUI_OK ) {
		xrtFree(sOutput);
		__xuiFindSetStatusError(sError, iErrorCapacity, iRet);
		return iRet;
	}
	iLength--;
	*psOutput = sOutput;
	if ( pOutputLength != NULL ) *pOutputLength = iLength;
	__xuiFindSetError(sError, iErrorCapacity, "");
	return XUI_OK;
}

XUI_API void xuiFindFreeText(char* sText)
{
	xrtFree(sText);
}

#endif
