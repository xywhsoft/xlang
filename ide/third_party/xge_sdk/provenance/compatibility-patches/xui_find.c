#include "xui_config.h"
#if XUI_ENABLE_FIND
#include "xui_internal.h"
#include "xui_xrt_port.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef struct xui_find_engine_t {
	char* sPattern;
	char* sReplacement;
	int iPatternLength;
	uint32_t iFlags;
#ifndef XRT_NO_REGEX
	xui_xrt_regex_t* pRegex;
#endif
} xui_find_engine_t;

static void __xuiFindSetError(char* sError, int iCapacity, const char* sMessage)
{
	if ( sError == NULL || iCapacity <= 0 ) return;
	if ( sMessage == NULL ) sMessage = "";
	snprintf(sError, (size_t)iCapacity, "%s", sMessage);
}

static int __xuiFindClamp(int iValue, int iMin, int iMax)
{
	if ( iValue < iMin ) return iMin;
	if ( iValue > iMax ) return iMax;
	return iValue;
}

static int __xuiFindTextLength(const char* sText, int iTextLength)
{
	if ( sText == NULL ) return 0;
	if ( iTextLength < 0 ) return (int)strlen(sText);
	return iTextLength;
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

/* Check the remaining length before reading any escape digits. */
static int __xuiFindReadHex(const char* sText, int iAvailable, int iDigits, unsigned int* pValue)
{
	unsigned int iValue = 0u;
	int i;
	if ( iAvailable < iDigits ) return 0;
	for ( i = 0; i < iDigits; i++ ) {
		int iHex = __xuiFindHex(sText[i]);
		if ( iHex < 0 ) return 0;
		iValue = (iValue << 4) | (unsigned int)iHex;
	}
	*pValue = iValue;
	return 1;
}

/* Empty matches advance by one complete UTF-8 scalar, preserving bytes. */
static int __xuiFindNextUtf8Offset(const char* sText, int iOffset, int iLimit)
{
	size_t iRead = 0;
	uint32 iScalar = 0;
	if ( iOffset >= iLimit ) return iLimit;
	(void)xrtUtf8Decode((xstrview){sText + iOffset, (size_t)(iLimit - iOffset)}, &iScalar, &iRead);
	if ( iRead == 0 || iRead > (size_t)(iLimit - iOffset) ) iRead = 1;
	return iOffset + (int)iRead;
}

static int __xuiFindAppendBytes(char** psBuffer, int* pLength, int* pCapacity, const char* sText, int iTextLength)
{
	char* sNew;
	int iNewCapacity;

	if ( psBuffer == NULL || pLength == NULL || pCapacity == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( sText == NULL || iTextLength <= 0 ) return XUI_OK;
	if ( *pLength + iTextLength + 1 > *pCapacity ) {
		iNewCapacity = (*pCapacity > 0) ? *pCapacity : 64;
		while ( *pLength + iTextLength + 1 > iNewCapacity ) iNewCapacity *= 2;
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

static int __xuiFindDecodeEscapes(const char* sText, char** psOutput, char* sError, int iErrorCapacity)
{
	char* sOutput;
	int iLength;
	int iCapacity;
	int i;
	int iTextLength;
	unsigned int iCodepoint;
	int iRet;

	if ( sText == NULL || psOutput == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*psOutput = NULL;
	iTextLength = (int)strlen(sText);
	sOutput = NULL;
	iLength = 0;
	iCapacity = 0;
	for ( i = 0; sText[i] != '\0'; i++ ) {
		if ( sText[i] != '\\' ) {
			iRet = __xuiFindAppendByte(&sOutput, &iLength, &iCapacity, sText[i]);
			if ( iRet != XUI_OK ) goto fail;
			continue;
		}
		i++;
		if ( sText[i] == '\0' ) {
			iRet = __xuiFindAppendByte(&sOutput, &iLength, &iCapacity, '\\');
			if ( iRet != XUI_OK ) goto fail;
			break;
		}
		switch ( sText[i] ) {
		case 'n': iRet = __xuiFindAppendByte(&sOutput, &iLength, &iCapacity, '\n'); break;
		case 'r': iRet = __xuiFindAppendByte(&sOutput, &iLength, &iCapacity, '\r'); break;
		case 't': iRet = __xuiFindAppendByte(&sOutput, &iLength, &iCapacity, '\t'); break;
		case '0':
			__xuiFindSetError(sError, iErrorCapacity, "nul escape is unsupported");
			iRet = XUI_ERROR_INVALID_ARGUMENT;
			goto fail;
		case '\\': iRet = __xuiFindAppendByte(&sOutput, &iLength, &iCapacity, '\\'); break;
		case 'x':
			if ( !__xuiFindReadHex(sText + i + 1, iTextLength - i - 1, 2, &iCodepoint) ) {
				__xuiFindSetError(sError, iErrorCapacity, "invalid hex escape");
				iRet = XUI_ERROR_INVALID_ARGUMENT;
				goto fail;
			}
			if ( iCodepoint == 0u ) {
				__xuiFindSetError(sError, iErrorCapacity, "nul escape is unsupported");
				iRet = XUI_ERROR_INVALID_ARGUMENT;
				goto fail;
			}
			i += 2;
			iRet = __xuiFindAppendByte(&sOutput, &iLength, &iCapacity, (char)iCodepoint);
			break;
		case 'u':
			if ( !__xuiFindReadHex(sText + i + 1, iTextLength - i - 1, 4, &iCodepoint) ) {
				__xuiFindSetError(sError, iErrorCapacity, "invalid unicode escape");
				iRet = XUI_ERROR_INVALID_ARGUMENT;
				goto fail;
			}
			i += 4;
			if ( iCodepoint >= 0xD800u && iCodepoint <= 0xDBFFu ) {
				unsigned int iLow;
				/* A UTF-16 high surrogate must be followed by a complete low one. */
				if ( iTextLength - i - 1 < 6 || sText[i + 1] != '\\' || sText[i + 2] != 'u' ||
				     !__xuiFindReadHex(sText + i + 3, iTextLength - i - 3, 4, &iLow) ||
				     iLow < 0xDC00u || iLow > 0xDFFFu ) {
					__xuiFindSetError(sError, iErrorCapacity, "invalid unicode surrogate pair");
					iRet = XUI_ERROR_INVALID_ARGUMENT;
					goto fail;
				}
				iCodepoint = 0x10000u + ((iCodepoint - 0xD800u) << 10) + (iLow - 0xDC00u);
				i += 6;
			} else if ( iCodepoint >= 0xDC00u && iCodepoint <= 0xDFFFu ) {
				__xuiFindSetError(sError, iErrorCapacity, "unpaired unicode low surrogate");
				iRet = XUI_ERROR_INVALID_ARGUMENT;
				goto fail;
			}
			if ( iCodepoint == 0u ) {
				__xuiFindSetError(sError, iErrorCapacity, "nul escape is unsupported");
				iRet = XUI_ERROR_INVALID_ARGUMENT;
				goto fail;
			}
			iRet = __xuiFindAppendUtf8(&sOutput, &iLength, &iCapacity, iCodepoint);
			break;
		default:
			iRet = __xuiFindAppendByte(&sOutput, &iLength, &iCapacity, sText[i]);
			break;
		}
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
	sCopy = (char*)xrtMalloc(iLength + 1u);
	if ( sCopy == NULL ) {
		__xuiFindSetError(sError, iErrorCapacity, "out of memory");
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	memcpy(sCopy, sText, iLength + 1u);
	*psOutput = sCopy;
	return XUI_OK;
}

static int __xuiFindIsWholeWord(const char* sText, int iTextLength, int iStart, int iEnd)
{
	return xuiInternalTextWordBoundary(sText, iTextLength, iStart,
		XUI_INTERNAL_WORD_NATURAL) &&
		xuiInternalTextWordBoundary(sText, iTextLength, iEnd,
		XUI_INTERNAL_WORD_NATURAL);
}

static int __xuiFindCharEquals(char a, char b, uint32_t iFlags)
{
	if ( (iFlags & XUI_FIND_CASE_SENSITIVE) != 0 ) return a == b;
	return tolower((unsigned char)a) == tolower((unsigned char)b);
}

static int __xuiFindPlainMatchAt(const char* sText, int iTextLength, const char* sPattern, int iPatternLength, int iOffset, uint32_t iFlags)
{
	int i;

	if ( iOffset < 0 || iOffset + iPatternLength > iTextLength ) return 0;
	for ( i = 0; i < iPatternLength; i++ ) {
		if ( !__xuiFindCharEquals(sText[iOffset + i], sPattern[i], iFlags) ) return 0;
	}
	if ( (iFlags & XUI_FIND_WHOLE_WORD) != 0 ) {
		return __xuiFindIsWholeWord(sText, iTextLength, iOffset, iOffset + iPatternLength);
	}
	return 1;
}

static int __xuiFindPlainInRange(const char* sText, int iTextLength, const xui_find_engine_t* pEngine, int iStart, int iEnd, xui_find_result_t* pResult)
{
	int i;

	if ( pEngine->iPatternLength <= 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( (pEngine->iFlags & XUI_FIND_BACKWARD) != 0 ) {
		for ( i = iEnd - pEngine->iPatternLength; i >= iStart; i-- ) {
			if ( __xuiFindPlainMatchAt(sText, iTextLength, pEngine->sPattern, pEngine->iPatternLength, i, pEngine->iFlags) ) {
				memset(pResult, 0, sizeof(*pResult));
				pResult->iSize = sizeof(*pResult);
				pResult->iStart = i;
				pResult->iEnd = i + pEngine->iPatternLength;
				return XUI_OK;
			}
		}
		return XUI_ERROR_UNSUPPORTED;
	}
	for ( i = iStart; i + pEngine->iPatternLength <= iEnd; i++ ) {
		if ( __xuiFindPlainMatchAt(sText, iTextLength, pEngine->sPattern, pEngine->iPatternLength, i, pEngine->iFlags) ) {
			memset(pResult, 0, sizeof(*pResult));
			pResult->iSize = sizeof(*pResult);
			pResult->iStart = i;
			pResult->iEnd = i + pEngine->iPatternLength;
			return XUI_OK;
		}
	}
	return XUI_ERROR_UNSUPPORTED;
}

#ifndef XRT_NO_REGEX
static int __xuiFindRegexCreate(xui_find_engine_t* pEngine, char* sError, int iErrorCapacity)
{
	uint32 iRegexFlags;

	pEngine->pRegex = NULL;
	iRegexFlags = 0;
	if ( (pEngine->iFlags & XUI_FIND_CASE_SENSITIVE) == 0 ) iRegexFlags |= 1u;
	if ( (pEngine->iFlags & XUI_FIND_MULTILINE) != 0 ) iRegexFlags |= 2u;
	if ( (pEngine->iFlags & XUI_FIND_DOT_NEWLINE) != 0 ) iRegexFlags |= 4u;
	pEngine->pRegex = xuiXrtRegexCreate(pEngine->sPattern, strlen(pEngine->sPattern), iRegexFlags);
	if ( pEngine->pRegex == NULL ) {
		__xuiFindSetError(sError, iErrorCapacity, "regex compile failed");
		return XUI_ERROR_INVALID_ARGUMENT;
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
	int iPos;
	int iRet;

	iPos = iStart;
	while ( iPos <= iEnd ) {
		iRet = xuiXrtRegexFindAt(pEngine->pRegex, sText, (size_t)iTextLength, (size_t)iPos, &tSpan);
		if ( iRet != XREGEX_MATCH || (int)tSpan.Begin > iEnd ) return XUI_ERROR_UNSUPPORTED;
		if ( (int)tSpan.End <= iEnd && __xuiFindRegexFillResult(pEngine, sText, iTextLength, tSpan, pResult) == XUI_OK ) return XUI_OK;
		if ( (int)tSpan.Begin >= iEnd ) break;
		iPos = ((int)tSpan.End > (int)tSpan.Begin) ? (int)tSpan.End : __xuiFindNextUtf8Offset(sText, (int)tSpan.Begin, iEnd);
	}
	return XUI_ERROR_UNSUPPORTED;
}

static int __xuiFindRegexBackward(xui_find_engine_t* pEngine, const char* sText, int iTextLength, int iStart, int iEnd, xui_find_result_t* pResult)
{
	xui_find_result_t tLast;
	xui_find_result_t tCurrent;
	int iPos;
	int iFound;

	iFound = 0;
	iPos = iStart;
	while ( __xuiFindRegexForward(pEngine, sText, iTextLength, iPos, iEnd, &tCurrent) == XUI_OK ) {
		if ( tCurrent.iEnd > iEnd ) break;
		tLast = tCurrent;
		iFound = 1;
		if ( tCurrent.iEnd == tCurrent.iStart && tCurrent.iStart >= iEnd ) break;
		iPos = (tCurrent.iEnd > tCurrent.iStart) ? tCurrent.iEnd : __xuiFindNextUtf8Offset(sText, tCurrent.iStart, iEnd);
	}
	if ( !iFound ) return XUI_ERROR_UNSUPPORTED;
	*pResult = tLast;
	return XUI_OK;
}
#endif

static int __xuiFindAppendReplacement(xui_find_engine_t* pEngine, char** psOutput, int* pLength, int* pCapacity, const char* sText, const xui_find_result_t* pResult);

static int __xuiFindEngineCreate(xui_find_engine_t* pEngine, const char* sPattern, const char* sReplacement, uint32_t iFlags, char* sError, int iErrorCapacity)
{
	int iRet;

	if ( pEngine == NULL || sPattern == NULL || sPattern[0] == '\0' ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(pEngine, 0, sizeof(*pEngine));
	pEngine->iFlags = iFlags;
	iRet = __xuiFindDupMaybeEscaped(sPattern, iFlags, &pEngine->sPattern, sError, iErrorCapacity);
	if ( iRet != XUI_OK ) return iRet;
	pEngine->iPatternLength = (int)strlen(pEngine->sPattern);
	if ( pEngine->iPatternLength <= 0 ) {
		__xuiFindSetError(sError, iErrorCapacity, "empty pattern");
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( sReplacement != NULL ) {
		if ( (iFlags & XUI_FIND_REGEX) != 0 ) {
			iRet = __xuiFindDupRaw(sReplacement, &pEngine->sReplacement, sError, iErrorCapacity);
		} else {
			iRet = __xuiFindDupMaybeEscaped(sReplacement, iFlags, &pEngine->sReplacement, sError, iErrorCapacity);
		}
		if ( iRet != XUI_OK ) return iRet;
		if ( (iFlags & (XUI_FIND_REGEX | XUI_FIND_ESCAPE)) == (XUI_FIND_REGEX | XUI_FIND_ESCAPE) ) {
			/* Validate before matching, including when there are no matches. */
			xui_find_result_t tEmpty = {0};
			char* sValidation = NULL;
			int iLength = 0, iCapacity = 0;
			iRet = __xuiFindAppendReplacement(pEngine, &sValidation, &iLength, &iCapacity, "", &tEmpty);
			if ( iRet == XUI_OK && !xrtUtf8Valid((xstrview){sValidation, (size_t)iLength}, NULL) ) iRet = XUI_ERROR_INVALID_ARGUMENT;
			xrtFree(sValidation);
			if ( iRet != XUI_OK ) {
				__xuiFindSetError(sError, iErrorCapacity, "invalid replacement escape");
				return iRet;
			}
		}
	}
	if ( (iFlags & XUI_FIND_REGEX) != 0 ) {
#ifdef XRT_NO_REGEX
		__xuiFindSetError(sError, iErrorCapacity, "XRT regex support is disabled");
		return XUI_ERROR_UNSUPPORTED;
#else
		iRet = __xuiFindRegexCreate(pEngine, sError, iErrorCapacity);
		if ( iRet != XUI_OK ) return iRet;
#endif
	}
	return XUI_OK;
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

static int __xuiFindAppendReplacement(xui_find_engine_t* pEngine, char** psOutput, int* pLength, int* pCapacity, const char* sText, const xui_find_result_t* pResult)
{
	const char* sReplacement;
	int i;
	int iCapture;
	int iTextLength;
	unsigned int iCodepoint;
	int iRet;

	if ( pEngine == NULL || psOutput == NULL || pLength == NULL || pCapacity == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	sReplacement = (pEngine->sReplacement != NULL) ? pEngine->sReplacement : "";
	iTextLength = (int)strlen(sReplacement);
	if ( (pEngine->iFlags & XUI_FIND_REGEX) == 0 ) {
		return __xuiFindAppendBytes(psOutput, pLength, pCapacity, sReplacement, (int)strlen(sReplacement));
	}
	for ( i = 0; sReplacement[i] != '\0'; i++ ) {
		if ( sReplacement[i] == '$' && sReplacement[i + 1] >= '0' && sReplacement[i + 1] <= '9' ) {
			iCapture = sReplacement[i + 1] - '0';
			i++;
			if ( iCapture < pResult->iCaptureCount && pResult->arrCaptures[iCapture].iStart >= 0 ) {
				iRet = __xuiFindAppendBytes(psOutput, pLength, pCapacity, sText + pResult->arrCaptures[iCapture].iStart, pResult->arrCaptures[iCapture].iEnd - pResult->arrCaptures[iCapture].iStart);
				if ( iRet != XUI_OK ) return iRet;
			}
			continue;
		}
		if ( sReplacement[i] == '\\' ) {
			if ( sReplacement[i + 1] >= '0' && sReplacement[i + 1] <= '9' ) {
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
				if ( sReplacement[i] == '\0' ) return __xuiFindAppendByte(psOutput, pLength, pCapacity, '\\');
				switch ( sReplacement[i] ) {
				case 'n': iRet = __xuiFindAppendByte(psOutput, pLength, pCapacity, '\n'); break;
				case 'r': iRet = __xuiFindAppendByte(psOutput, pLength, pCapacity, '\r'); break;
				case 't': iRet = __xuiFindAppendByte(psOutput, pLength, pCapacity, '\t'); break;
				case '\\': iRet = __xuiFindAppendByte(psOutput, pLength, pCapacity, '\\'); break;
				case 'x':
					if ( !__xuiFindReadHex(sReplacement + i + 1, iTextLength - i - 1, 2, &iCodepoint) ) {
						iRet = XUI_ERROR_INVALID_ARGUMENT;
						return iRet;
					}
					if ( iCodepoint == 0u ) {
						iRet = XUI_ERROR_INVALID_ARGUMENT;
						return iRet;
					}
					i += 2;
					iRet = __xuiFindAppendByte(psOutput, pLength, pCapacity, (char)iCodepoint);
					break;
				case 'u':
					if ( !__xuiFindReadHex(sReplacement + i + 1, iTextLength - i - 1, 4, &iCodepoint) ) {
						iRet = XUI_ERROR_INVALID_ARGUMENT;
						return iRet;
					}
					i += 4;
					if ( iCodepoint >= 0xD800u && iCodepoint <= 0xDBFFu ) {
						unsigned int iLow;
						/* A UTF-16 high surrogate must be followed by a complete low one. */
						if ( iTextLength - i - 1 < 6 || sReplacement[i + 1] != '\\' || sReplacement[i + 2] != 'u' ||
						     !__xuiFindReadHex(sReplacement + i + 3, iTextLength - i - 3, 4, &iLow) ||
						     iLow < 0xDC00u || iLow > 0xDFFFu ) {
							iRet = XUI_ERROR_INVALID_ARGUMENT;
							return iRet;
						}
						iCodepoint = 0x10000u + ((iCodepoint - 0xD800u) << 10) + (iLow - 0xDC00u);
						i += 6;
					} else if ( iCodepoint >= 0xDC00u && iCodepoint <= 0xDFFFu ) {
						iRet = XUI_ERROR_INVALID_ARGUMENT;
						return iRet;
					}
					if ( iCodepoint == 0u ) {
						iRet = XUI_ERROR_INVALID_ARGUMENT;
						return iRet;
					}
					iRet = __xuiFindAppendUtf8(psOutput, pLength, pCapacity, iCodepoint);
					break;
				default:
					iRet = __xuiFindAppendByte(psOutput, pLength, pCapacity, sReplacement[i]);
					break;
				}
				if ( iRet != XUI_OK ) return iRet;
				continue;
			}
		}
		iRet = __xuiFindAppendByte(psOutput, pLength, pCapacity, sReplacement[i]);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

XUI_API int xuiFindText(const char* sText, int iTextLength, const char* sPattern, int iStartOffset, int iRangeStart, int iRangeEnd, uint32_t iFlags, xui_find_result_t* pResult, char* sError, int iErrorCapacity)
{
	xui_find_engine_t tEngine;
	int iRet;

	if ( sText == NULL || sPattern == NULL || pResult == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iTextLength = __xuiFindTextLength(sText, iTextLength);
	__xuiFindNormalizeRange(iTextLength, &iRangeStart, &iRangeEnd, &iStartOffset);
	memset(pResult, 0, sizeof(*pResult));
	memset(&tEngine, 0, sizeof(tEngine));
	iRet = __xuiFindEngineCreate(&tEngine, sPattern, NULL, iFlags, sError, iErrorCapacity);
	if ( iRet == XUI_OK ) iRet = __xuiFindEngineFind(&tEngine, sText, iTextLength, iStartOffset, iRangeStart, iRangeEnd, pResult);
	__xuiFindEngineDestroy(&tEngine);
	if ( iRet == XUI_OK ) __xuiFindSetError(sError, iErrorCapacity, "");
	else if ( iRet == XUI_ERROR_UNSUPPORTED ) __xuiFindSetError(sError, iErrorCapacity, "not found");
	return iRet;
}

XUI_API int xuiFindCollectText(const char* sText, int iTextLength, const char* sPattern, int iRangeStart, int iRangeEnd, uint32_t iFlags, xui_find_result_t* pResults, int iResultCapacity, int* pResultCount, char* sError, int iErrorCapacity)
{
	xui_find_engine_t tEngine;
	xui_find_result_t tResult;
	int iOffset;
	int iCount;
	int iRet;

	if ( sText == NULL || sPattern == NULL || pResultCount == NULL || iResultCapacity < 0 || (iResultCapacity > 0 && pResults == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	iTextLength = __xuiFindTextLength(sText, iTextLength);
	iOffset = iRangeStart;
	__xuiFindNormalizeRange(iTextLength, &iRangeStart, &iRangeEnd, &iOffset);
	iFlags &= ~(XUI_FIND_BACKWARD | XUI_FIND_WRAP);
	*pResultCount = 0;
	iCount = 0;
	memset(&tEngine, 0, sizeof(tEngine));
	iRet = __xuiFindEngineCreate(&tEngine, sPattern, NULL, iFlags, sError, iErrorCapacity);
	if ( iRet != XUI_OK ) return iRet;
	iOffset = iRangeStart;
	while ( __xuiFindEngineFind(&tEngine, sText, iTextLength, iOffset, iRangeStart, iRangeEnd, &tResult) == XUI_OK ) {
		if ( pResults != NULL && iCount < iResultCapacity ) pResults[iCount] = tResult;
		iCount++;
		if ( tResult.iEnd == tResult.iStart && tResult.iStart >= iRangeEnd ) break;
		iOffset = (tResult.iEnd > tResult.iStart) ? tResult.iEnd : __xuiFindNextUtf8Offset(sText, tResult.iStart, iRangeEnd);
		if ( iOffset > iRangeEnd ) break;
	}
	*pResultCount = iCount;
	__xuiFindEngineDestroy(&tEngine);
	__xuiFindSetError(sError, iErrorCapacity, "");
	return XUI_OK;
}

XUI_API int xuiFindReplaceAllText(const char* sText, int iTextLength, const char* sPattern, const char* sReplacement, int iRangeStart, int iRangeEnd, uint32_t iFlags, char** psOutput, int* pOutputLength, int* pReplaceCount, char* sError, int iErrorCapacity)
{
	xui_find_engine_t tEngine;
	xui_find_result_t tResult;
	char* sOutput;
	int iOutputLength;
	int iCapacity;
	int iCursor;
	int iCount;
	int iRet;

	if ( sText == NULL || sPattern == NULL || sReplacement == NULL || psOutput == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*psOutput = NULL;
	if ( pOutputLength != NULL ) *pOutputLength = 0;
	if ( pReplaceCount != NULL ) *pReplaceCount = 0;
	iTextLength = __xuiFindTextLength(sText, iTextLength);
	iCursor = iRangeStart;
	__xuiFindNormalizeRange(iTextLength, &iRangeStart, &iRangeEnd, &iCursor);
	iFlags &= ~(XUI_FIND_BACKWARD | XUI_FIND_WRAP);
	memset(&tEngine, 0, sizeof(tEngine));
	iRet = __xuiFindEngineCreate(&tEngine, sPattern, sReplacement, iFlags, sError, iErrorCapacity);
	if ( iRet != XUI_OK ) return iRet;
	sOutput = NULL;
	iOutputLength = 0;
	iCapacity = 0;
	iCount = 0;
	iRet = __xuiFindAppendBytes(&sOutput, &iOutputLength, &iCapacity, sText, iRangeStart);
	if ( iRet != XUI_OK ) goto cleanup;
	iCursor = iRangeStart;
	while ( __xuiFindEngineFind(&tEngine, sText, iTextLength, iCursor, iRangeStart, iRangeEnd, &tResult) == XUI_OK ) {
		iRet = __xuiFindAppendBytes(&sOutput, &iOutputLength, &iCapacity, sText + iCursor, tResult.iStart - iCursor);
		if ( iRet == XUI_OK ) iRet = __xuiFindAppendReplacement(&tEngine, &sOutput, &iOutputLength, &iCapacity, sText, &tResult);
		if ( iRet != XUI_OK ) goto cleanup;
		iCount++;
		if ( tResult.iEnd > tResult.iStart ) {
			iCursor = tResult.iEnd;
		} else {
			int iNext;
			iCursor = tResult.iEnd;
			if ( iCursor >= iRangeEnd ) break;
			iNext = __xuiFindNextUtf8Offset(sText, iCursor, iRangeEnd);
			iRet = __xuiFindAppendBytes(&sOutput, &iOutputLength, &iCapacity, sText + iCursor, iNext - iCursor);
			if ( iRet != XUI_OK ) goto cleanup;
			iCursor = iNext;
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
	if ( iRet == XUI_ERROR_OUT_OF_MEMORY ) __xuiFindSetError(sError, iErrorCapacity, "out of memory");
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
	if ( sText == NULL || sPattern == NULL || sReplacement == NULL || pResult == NULL || psOutput == NULL )
		return XUI_ERROR_INVALID_ARGUMENT;
	*psOutput = NULL;
	if ( pOutputLength != NULL ) *pOutputLength = 0;
	iTextLength = __xuiFindTextLength(sText, iTextLength);
	if ( pResult->iStart < 0 || pResult->iEnd < pResult->iStart || pResult->iEnd > iTextLength )
		return XUI_ERROR_INVALID_ARGUMENT;
	memset(&tEngine, 0, sizeof(tEngine));
	iRet = __xuiFindEngineCreate(&tEngine, sPattern, sReplacement, iFlags, sError, iErrorCapacity);
	if ( iRet == XUI_OK ) iRet = __xuiFindAppendReplacement(&tEngine, &sOutput, &iLength, &iCapacity, sText, pResult);
	if ( iRet == XUI_OK ) iRet = __xuiFindAppendBytes(&sOutput, &iLength, &iCapacity, "", 1);
	__xuiFindEngineDestroy(&tEngine);
	if ( iRet != XUI_OK ) { xrtFree(sOutput); return iRet; }
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

#endif /* XUI_ENABLE_FIND */
