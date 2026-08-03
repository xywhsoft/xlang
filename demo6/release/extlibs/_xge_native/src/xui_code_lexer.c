#include "../xui.h"

#include <stdio.h>
#include <string.h>

static void __xuiCodeLexerSetError(char* sError, int iCapacity, const char* sMessage)
{
	if ( (sError == NULL) || (iCapacity <= 0) ) return;
	if ( sMessage == NULL ) sMessage = "";
	snprintf(sError, (size_t)iCapacity, "%s", sMessage);
}

static int __xuiCodeLexerAddToken(xui_code_token_t* pTokens, int iCapacity, int* pCount, int iStart, int iEnd, int iKind, int iStyle)
{
	xui_code_token_t* pToken;

	if ( (pCount == NULL) || (iStart >= iEnd) ) return XUI_OK;
	if ( (*pCount >= iCapacity) || (pTokens == NULL) ) {
		(*pCount)++;
		return XUI_OK;
	}
	pToken = &pTokens[*pCount];
	memset(pToken, 0, sizeof(*pToken));
	pToken->iStartOffset = iStart;
	pToken->iEndOffset = iEnd;
	pToken->iKind = iKind;
	pToken->iStyle = iStyle;
	(*pCount)++;
	return XUI_OK;
}

XUI_API int xuiCodeLexerRegexTokenize(const char* sText, int iTextSize, const xui_code_regex_rule_t* pRules, int iRuleCount, xui_code_token_t* pTokens, int iTokenCapacity, int* pTokenCount, char* sError, int iErrorCapacity)
{
#ifdef XRT_NO_REGEX
	(void)sText;
	(void)iTextSize;
	(void)pRules;
	(void)iRuleCount;
	(void)pTokens;
	(void)iTokenCapacity;
	(void)pTokenCount;
	__xuiCodeLexerSetError(sError, iErrorCapacity, "XRT regex support is disabled");
	return XUI_ERROR_UNSUPPORTED;
#else
	xregex** pRegexes;
	xregexbuilder* pBuilder;
	xregexspan tSpan;
	size_t iBegin;
	size_t iEnd;
	int i;
	int iPos;
	int iMatched;
	int iRet;

	if ( pTokenCount == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*pTokenCount = 0;
	if ( (sText == NULL) || (pRules == NULL) || (iRuleCount < 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iTextSize < 0 ) iTextSize = (int)strlen(sText);
	pRegexes = (xregex**)xrtMalloc(sizeof(*pRegexes) * (size_t)iRuleCount);
	if ( pRegexes == NULL ) {
		__xuiCodeLexerSetError(sError, iErrorCapacity, "out of memory");
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	memset(pRegexes, 0, sizeof(*pRegexes) * (size_t)iRuleCount);
	for ( i = 0; i < iRuleCount; i++ ) {
		iRet = xrtRegexBuilderCreate(&pBuilder, pRules[i].sPattern, (pRules[i].sPattern != NULL) ? strlen(pRules[i].sPattern) : 0u, NULL);
		if ( iRet != 0 || pBuilder == NULL ) {
			__xuiCodeLexerSetError(sError, iErrorCapacity, "regex builder create failed");
			goto fail;
		}
		xrtRegexBuilderSetFlags(pBuilder, pRules[i].iFlags);
		iRet = xrtRegexCreateFromBuilder(&pRegexes[i], pBuilder, NULL);
		xrtRegexBuilderDestroy(pBuilder);
		if ( iRet != 0 || pRegexes[i] == NULL ) {
			__xuiCodeLexerSetError(sError, iErrorCapacity, "regex compile failed");
			goto fail;
		}
	}
	iPos = 0;
	while ( iPos < iTextSize ) {
		iMatched = 0;
		for ( i = 0; i < iRuleCount; i++ ) {
			iRet = xrtRegexFindAt(pRegexes[i], sText, (size_t)iTextSize, (size_t)iPos, &tSpan);
			if ( iRet != 1 ) continue;
			iBegin = tSpan.iBegin;
			iEnd = tSpan.iEnd;
			if ( (int)iBegin != iPos || iEnd <= iBegin ) continue;
			(void)__xuiCodeLexerAddToken(pTokens, iTokenCapacity, pTokenCount, (int)iBegin, (int)iEnd, pRules[i].iTokenKind, pRules[i].iStyle);
			iPos = (int)iEnd;
			iMatched = 1;
			break;
		}
		if ( !iMatched ) {
			(void)__xuiCodeLexerAddToken(pTokens, iTokenCapacity, pTokenCount, iPos, iPos + 1, XUI_CODE_TOKEN_TEXT, XUI_CODE_TOKEN_TEXT);
			iPos++;
		}
	}
	for ( i = 0; i < iRuleCount; i++ ) {
		if ( pRegexes[i] != NULL ) xrtRegexDestroy(pRegexes[i]);
	}
	xrtFree(pRegexes);
	__xuiCodeLexerSetError(sError, iErrorCapacity, "");
	return XUI_OK;

fail:
	for ( i = 0; i < iRuleCount; i++ ) {
		if ( pRegexes[i] != NULL ) xrtRegexDestroy(pRegexes[i]);
	}
	xrtFree(pRegexes);
	return XUI_ERROR_INVALID_ARGUMENT;
#endif
}
