#include "../xui.h"

#include <string.h>

#define XUI_CODE_FOLD_STACK_MAX 256

typedef struct xui_code_fold_stack_t {
	int iLine;
	int iLevel;
	uint32_t iFlags;
} xui_code_fold_stack_t;

typedef struct xui_code_fold_source_t {
	const char* sText;
	xui_code_document pDocument;
	int iLength;
} xui_code_fold_source_t;

static char __xuiCodeFoldByteAt(const xui_code_fold_source_t* pSource, int iOffset)
{
	char c;

	if ( pSource == NULL || iOffset < 0 || iOffset >= pSource->iLength ) return '\0';
	if ( pSource->sText != NULL ) return pSource->sText[iOffset];
	c = '\0';
	(void)xuiCodeDocumentGetByte(pSource->pDocument, iOffset, &c);
	return c;
}

static int __xuiCodeFoldMatch(const xui_code_fold_source_t* pSource, int iOffset, const char* sValue)
{
	int i;

	if ( pSource == NULL || sValue == NULL ) return 0;
	for ( i = 0; sValue[i] != '\0'; i++ ) {
		if ( iOffset + i >= pSource->iLength || __xuiCodeFoldByteAt(pSource, iOffset + i) != sValue[i] ) return 0;
	}
	return 1;
}

static int __xuiCodeFoldLineForOffset(const char* sText, int iOffset)
{
	int i;
	int iLine;

	iLine = 0;
	for ( i = 0; i < iOffset; i++ ) {
		if ( sText[i] == '\n' ) iLine++;
	}
	return iLine;
}

static int __xuiCodeFoldAddRange(xui_code_fold_range_t* pRanges, int iCapacity, int* pCount, int iStartLine, int iEndLine, int iLevel, uint32_t iFlags)
{
	xui_code_fold_range_t* pRange;

	if ( pCount == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iEndLine <= iStartLine ) return XUI_OK;
	if ( (*pCount >= iCapacity) || (pRanges == NULL) ) {
		(*pCount)++;
		return XUI_OK;
	}
	pRange = &pRanges[*pCount];
	memset(pRange, 0, sizeof(*pRange));
	pRange->iSize = sizeof(*pRange);
	pRange->iStartLine = iStartLine;
	pRange->iEndLine = iEndLine;
	pRange->iLevel = iLevel;
	pRange->iFlags = iFlags | XUI_CODE_FOLD_HEADER;
	(*pCount)++;
	return XUI_OK;
}

static int __xuiCodeFoldCBuildRanges(const xui_code_fold_source_t* pSource,
	xui_code_fold_range_t* pRanges, int iRangeCapacity, int* pRangeCount)
{
	xui_code_fold_stack_t arrStack[XUI_CODE_FOLD_STACK_MAX];
	xui_code_fold_stack_t arrPreproc[XUI_CODE_FOLD_STACK_MAX];
	int iStackCount;
	int iPreprocCount;
	int iLine;
	int iLineStart;
	int i;
	int iRet;

	if ( pRangeCount == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*pRangeCount = 0;
	if ( pSource == NULL || (pSource->sText == NULL && pSource->pDocument == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	iStackCount = 0;
	iPreprocCount = 0;
	iLine = 0;
	iLineStart = 0;
	i = 0;
	while ( i < pSource->iLength ) {
		if ( __xuiCodeFoldByteAt(pSource, i) == '\n' ) {
			iLine++;
			iLineStart = i + 1;
			i++;
			continue;
		}
		if ( (i == iLineStart) || ((i > iLineStart) &&
		     (__xuiCodeFoldByteAt(pSource, i - 1) == ' ' || __xuiCodeFoldByteAt(pSource, i - 1) == '\t')) ) {
			int j = iLineStart;
			while ( j < pSource->iLength &&
			      (__xuiCodeFoldByteAt(pSource, j) == ' ' || __xuiCodeFoldByteAt(pSource, j) == '\t') ) j++;
			if ( i == j && __xuiCodeFoldByteAt(pSource, j) == '#' ) {
				if ( __xuiCodeFoldMatch(pSource, j, "#if") ) {
					if ( iPreprocCount < XUI_CODE_FOLD_STACK_MAX ) {
						arrPreproc[iPreprocCount].iLine = iLine;
						arrPreproc[iPreprocCount].iLevel = iPreprocCount;
						arrPreproc[iPreprocCount].iFlags = XUI_CODE_FOLD_PREPROCESSOR;
						iPreprocCount++;
					}
				} else if ( __xuiCodeFoldMatch(pSource, j, "#endif") && iPreprocCount > 0 ) {
					iPreprocCount--;
					iRet = __xuiCodeFoldAddRange(pRanges, iRangeCapacity, pRangeCount,
						arrPreproc[iPreprocCount].iLine, iLine, arrPreproc[iPreprocCount].iLevel, arrPreproc[iPreprocCount].iFlags);
					if ( iRet != XUI_OK ) return iRet;
				}
			}
		}
		if ( __xuiCodeFoldByteAt(pSource, i) == '"' ) {
			i++;
			while ( i < pSource->iLength ) {
				if ( __xuiCodeFoldByteAt(pSource, i) == '\\' && i + 1 < pSource->iLength ) i += 2;
				else if ( __xuiCodeFoldByteAt(pSource, i) == '"' ) { i++; break; }
				else i++;
			}
			continue;
		}
		if ( __xuiCodeFoldByteAt(pSource, i) == '\'' ) {
			i++;
			while ( i < pSource->iLength ) {
				if ( __xuiCodeFoldByteAt(pSource, i) == '\\' && i + 1 < pSource->iLength ) i += 2;
				else if ( __xuiCodeFoldByteAt(pSource, i) == '\'' ) { i++; break; }
				else i++;
			}
			continue;
		}
		if ( __xuiCodeFoldByteAt(pSource, i) == '/' && i + 1 < pSource->iLength &&
		     __xuiCodeFoldByteAt(pSource, i + 1) == '/' ) {
			while ( i < pSource->iLength && __xuiCodeFoldByteAt(pSource, i) != '\n' ) i++;
			continue;
		}
		if ( __xuiCodeFoldByteAt(pSource, i) == '/' && i + 1 < pSource->iLength &&
		     __xuiCodeFoldByteAt(pSource, i + 1) == '*' ) {
			int iStartLine = iLine;
			i += 2;
			while ( i + 1 < pSource->iLength &&
			      !(__xuiCodeFoldByteAt(pSource, i) == '*' && __xuiCodeFoldByteAt(pSource, i + 1) == '/') ) {
				if ( __xuiCodeFoldByteAt(pSource, i) == '\n' ) iLine++;
				i++;
			}
			if ( i + 1 < pSource->iLength ) i += 2;
			iRet = __xuiCodeFoldAddRange(pRanges, iRangeCapacity, pRangeCount, iStartLine, iLine, 0, XUI_CODE_FOLD_COMMENT);
			if ( iRet != XUI_OK ) return iRet;
			continue;
		}
		if ( __xuiCodeFoldByteAt(pSource, i) == '{' ) {
			if ( iStackCount < XUI_CODE_FOLD_STACK_MAX ) {
				arrStack[iStackCount].iLine = iLine;
				arrStack[iStackCount].iLevel = iStackCount;
				arrStack[iStackCount].iFlags = 0;
				iStackCount++;
			}
		} else if ( __xuiCodeFoldByteAt(pSource, i) == '}' && iStackCount > 0 ) {
			iStackCount--;
			iRet = __xuiCodeFoldAddRange(pRanges, iRangeCapacity, pRangeCount,
				arrStack[iStackCount].iLine, iLine, arrStack[iStackCount].iLevel, arrStack[iStackCount].iFlags);
			if ( iRet != XUI_OK ) return iRet;
		}
		i++;
	}
	(void)__xuiCodeFoldLineForOffset;
	return XUI_OK;
}

XUI_API int xuiCodeFoldCBuildRanges(const char* sText, int iTextSize, xui_code_fold_range_t* pRanges, int iRangeCapacity, int* pRangeCount)
{
	xui_code_fold_source_t tSource;

	if ( sText == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	tSource.sText = sText;
	tSource.pDocument = NULL;
	tSource.iLength = (iTextSize < 0) ? (int)strlen(sText) : iTextSize;
	return __xuiCodeFoldCBuildRanges(&tSource, pRanges, iRangeCapacity, pRangeCount);
}

XUI_API int xuiCodeFoldCBuildDocumentRanges(xui_code_document pDocument, xui_code_fold_range_t* pRanges, int iRangeCapacity, int* pRangeCount)
{
	xui_code_fold_source_t tSource;

	if ( pDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	tSource.sText = NULL;
	tSource.pDocument = pDocument;
	tSource.iLength = xuiCodeDocumentGetLength(pDocument);
	return __xuiCodeFoldCBuildRanges(&tSource, pRanges, iRangeCapacity, pRangeCount);
}

XUI_API int xuiCodeFoldBuildVisibleLines(int iLineCount, const xui_code_fold_range_t* pRanges, int iRangeCount, int* pVisibleLines, int iVisibleCapacity, int* pVisibleCount)
{
	int i;
	int j;
	int bHidden;

	if ( (iLineCount < 0) || (pVisibleCount == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pVisibleCount = 0;
	for ( i = 0; i < iLineCount; i++ ) {
		bHidden = 0;
		for ( j = 0; j < iRangeCount; j++ ) {
			if ( (pRanges[j].iFlags & XUI_CODE_FOLD_COLLAPSED) != 0 &&
			     i > pRanges[j].iStartLine && i <= pRanges[j].iEndLine ) {
				bHidden = 1;
				break;
			}
		}
		if ( bHidden ) continue;
		if ( (*pVisibleCount < iVisibleCapacity) && (pVisibleLines != NULL) ) {
			pVisibleLines[*pVisibleCount] = i;
		}
		(*pVisibleCount)++;
	}
	return XUI_OK;
}
