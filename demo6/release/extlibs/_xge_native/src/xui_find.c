#include "../xui.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef struct xui_find_engine_t {
	char* sPattern;
	char* sReplacement;
	int iPatternLength;
	uint32_t iFlags;
#ifndef XRT_NO_REGEX
	xregex* pRegex;
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
	int iHex0;
	int iHex1;
	unsigned int iCodepoint;
	int iRet;

	if ( sText == NULL || psOutput == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*psOutput = NULL;
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
			iHex0 = __xuiFindHex(sText[i + 1]);
			iHex1 = __xuiFindHex(sText[i + 2]);
			if ( iHex0 < 0 || iHex1 < 0 ) {
				__xuiFindSetError(sError, iErrorCapacity, "invalid hex escape");
				iRet = XUI_ERROR_INVALID_ARGUMENT;
				goto fail;
			}
			i += 2;
			iRet = __xuiFindAppendByte(&sOutput, &iLength, &iCapacity, (char)((iHex0 << 4) | iHex1));
			break;
		case 'u':
			iCodepoint = 0u;
			if ( __xuiFindHex(sText[i + 1]) < 0 || __xuiFindHex(sText[i + 2]) < 0 ||
			     __xuiFindHex(sText[i + 3]) < 0 || __xuiFindHex(sText[i + 4]) < 0 ) {
				__xuiFindSetError(sError, iErrorCapacity, "invalid unicode escape");
				iRet = XUI_ERROR_INVALID_ARGUMENT;
				goto fail;
			}
			iCodepoint = (unsigned int)((__xuiFindHex(sText[i + 1]) << 12) |
			                           (__xuiFindHex(sText[i + 2]) << 8) |
			                           (__xuiFindHex(sText[i + 3]) << 4) |
			                           __xuiFindHex(sText[i + 4]));
			i += 4;
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

static int __xuiFindIsWordChar(char c)
{
	unsigned char ch = (unsigned char)c;
	return isalnum(ch) || ch == '_';
}

static int __xuiFindIsWholeWord(const char* sText, int iTextLength, int iStart, int iEnd)
{
	if ( iStart > 0 && __xuiFindIsWordChar(sText[iStart - 1]) ) return 0;
	if ( iEnd < iTextLength && __xuiFindIsWordChar(sText[iEnd]) ) return 0;
	return 1;
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
	xregexbuilder* pBuilder;
	xregexflags iRegexFlags;
	int iRet;

	pEngine->pRegex = NULL;
	pBuilder = NULL;
	iRet = xrtRegexBuilderCreate(&pBuilder, pEngine->sPattern, strlen(pEngine->sPattern), NULL);
	if ( iRet != 0 || pBuilder == NULL ) {
		__xuiFindSetError(sError, iErrorCapacity, "regex builder create failed");
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRegexFlags = 0;
	if ( (pEngine->iFlags & XUI_FIND_CASE_SENSITIVE) == 0 ) iRegexFlags |= XRT_REGEX_FLAG_INSENSITIVE;
	if ( (pEngine->iFlags & XUI_FIND_MULTILINE) != 0 ) iRegexFlags |= XRT_REGEX_FLAG_MULTILINE;
	if ( (pEngine->iFlags & XUI_FIND_DOT_NEWLINE) != 0 ) iRegexFlags |= XRT_REGEX_FLAG_DOTNEWLINE;
	xrtRegexBuilderSetFlags(pBuilder, iRegexFlags);
	iRet = xrtRegexCreateFromBuilder(&pEngine->pRegex, pBuilder, NULL);
	xrtRegexBuilderDestroy(pBuilder);
	if ( iRet != 0 || pEngine->pRegex == NULL ) {
		__xuiFindSetError(sError, iErrorCapacity, "regex compile failed");
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return XUI_OK;
}

static int __xuiFindRegexFillResult(xui_find_engine_t* pEngine, const char* sText, int iTextLength, xregexspan tSpan, xui_find_result_t* pResult)
{
	xregexspan arrCaptures[XUI_FIND_MAX_CAPTURES];
	uint32_t arrMatched[XUI_FIND_MAX_CAPTURES];
	uint32_t iCaptureCount;
	uint32_t i;
	int iRet;

	if ( (pEngine->iFlags & XUI_FIND_WHOLE_WORD) != 0 ) {
		if ( !__xuiFindIsWholeWord(sText, iTextLength, (int)tSpan.iBegin, (int)tSpan.iEnd) ) return XUI_ERROR_UNSUPPORTED;
	}
	memset(pResult, 0, sizeof(*pResult));
	pResult->iSize = sizeof(*pResult);
	pResult->iStart = (int)tSpan.iBegin;
	pResult->iEnd = (int)tSpan.iEnd;
	iCaptureCount = xrtRegexCaptureCount(pEngine->pRegex);
	if ( iCaptureCount > XUI_FIND_MAX_CAPTURES ) iCaptureCount = XUI_FIND_MAX_CAPTURES;
	memset(arrCaptures, 0, sizeof(arrCaptures));
	memset(arrMatched, 0, sizeof(arrMatched));
	iRet = xrtRegexWhichCaptures(pEngine->pRegex, sText + tSpan.iBegin, (size_t)(tSpan.iEnd - tSpan.iBegin), arrCaptures, arrMatched, iCaptureCount);
	if ( iRet == 1 ) {
		pResult->iCaptureCount = (int)iCaptureCount;
		for ( i = 0; i < iCaptureCount; i++ ) {
			if ( arrMatched[i] ) {
				pResult->arrCaptures[i].iStart = (int)(tSpan.iBegin + arrCaptures[i].iBegin);
				pResult->arrCaptures[i].iEnd = (int)(tSpan.iBegin + arrCaptures[i].iEnd);
			} else {
				pResult->arrCaptures[i].iStart = -1;
				pResult->arrCaptures[i].iEnd = -1;
			}
		}
	}
	return XUI_OK;
}

static int __xuiFindRegexForward(xui_find_engine_t* pEngine, const char* sText, int iTextLength, int iStart, int iEnd, xui_find_result_t* pResult)
{
	xregexspan tSpan;
	int iPos;
	int iRet;

	iPos = iStart;
	while ( iPos <= iEnd ) {
		iRet = xrtRegexFindAt(pEngine->pRegex, sText, (size_t)iTextLength, (size_t)iPos, &tSpan);
		if ( iRet != 1 || (int)tSpan.iBegin > iEnd ) return XUI_ERROR_UNSUPPORTED;
		if ( (int)tSpan.iEnd <= iEnd && __xuiFindRegexFillResult(pEngine, sText, iTextLength, tSpan, pResult) == XUI_OK ) return XUI_OK;
		iPos = ((int)tSpan.iEnd > iPos) ? (int)tSpan.iEnd : (int)tSpan.iBegin + 1;
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
		iPos = (tCurrent.iEnd > tCurrent.iStart) ? tCurrent.iEnd : tCurrent.iStart + 1;
	}
	if ( !iFound ) return XUI_ERROR_UNSUPPORTED;
	*pResult = tLast;
	return XUI_OK;
}
#endif

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
	if ( pEngine->pRegex != NULL ) xrtRegexDestroy(pEngine->pRegex);
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
	int iHex0;
	int iHex1;
	unsigned int iCodepoint;
	int iRet;

	if ( pEngine == NULL || psOutput == NULL || pLength == NULL || pCapacity == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	sReplacement = (pEngine->sReplacement != NULL) ? pEngine->sReplacement : "";
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
					iHex0 = __xuiFindHex(sReplacement[i + 1]);
					iHex1 = __xuiFindHex(sReplacement[i + 2]);
					if ( iHex0 < 0 || iHex1 < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
					i += 2;
					iRet = __xuiFindAppendByte(psOutput, pLength, pCapacity, (char)((iHex0 << 4) | iHex1));
					break;
				case 'u':
					if ( __xuiFindHex(sReplacement[i + 1]) < 0 || __xuiFindHex(sReplacement[i + 2]) < 0 ||
					     __xuiFindHex(sReplacement[i + 3]) < 0 || __xuiFindHex(sReplacement[i + 4]) < 0 ) {
						return XUI_ERROR_INVALID_ARGUMENT;
					}
					iCodepoint = (unsigned int)((__xuiFindHex(sReplacement[i + 1]) << 12) |
					                           (__xuiFindHex(sReplacement[i + 2]) << 8) |
					                           (__xuiFindHex(sReplacement[i + 3]) << 4) |
					                           __xuiFindHex(sReplacement[i + 4]));
					i += 4;
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
	__xuiFindSetError(sError, iErrorCapacity, (iRet == XUI_OK) ? "" : "not found");
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
		iOffset = (tResult.iEnd > tResult.iStart) ? tResult.iEnd : tResult.iStart + 1;
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
			if ( iCursor < iRangeEnd ) {
				iRet = __xuiFindAppendBytes(&sOutput, &iOutputLength, &iCapacity, sText + iCursor, 1);
				if ( iRet != XUI_OK ) goto cleanup;
			}
			iCursor = tResult.iStart + 1;
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

XUI_API void xuiFindFreeText(char* sText)
{
	xrtFree(sText);
}
