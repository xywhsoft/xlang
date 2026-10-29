#include "../xui.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef struct xui_code_language_entry_t {
	xui_code_language_t tLanguage;
	char* sId;
	char* sName;
	char** pExtensions;
	char* sLineComment;
	char* sBlockCommentStart;
	char* sBlockCommentEnd;
	xui_code_regex_rule_t* pRegexRules;
	char** pRuleNames;
	char** pRulePatterns;
} xui_code_language_entry_t;

struct xui_code_language_registry_t {
	xui_code_language_entry_t* pEntries;
	int iCount;
	int iCapacity;
};

static char* __xuiCodeLanguageStrDup(const char* sText)
{
	char* sCopy;
	size_t iLength;

	if ( sText == NULL ) return NULL;
	iLength = strlen(sText);
	sCopy = (char*)xrtMalloc(iLength + 1u);
	if ( sCopy == NULL ) return NULL;
	memcpy(sCopy, sText, iLength + 1u);
	return sCopy;
}

static int __xuiCodeLanguageEquals(const char* a, const char* b)
{
	if ( a == NULL || b == NULL ) return 0;
	while ( *a != '\0' && *b != '\0' ) {
		if ( tolower((unsigned char)*a) != tolower((unsigned char)*b) ) return 0;
		a++;
		b++;
	}
	return *a == '\0' && *b == '\0';
}

static const char* __xuiCodeLanguageNormalizeExtension(const char* sExtension)
{
	if ( sExtension == NULL ) return NULL;
	if ( sExtension[0] == '.' ) return sExtension + 1;
	return sExtension;
}

static void __xuiCodeLanguageSetError(char* sError, int iCapacity, const char* sMessage)
{
	if ( (sError == NULL) || (iCapacity <= 0) ) return;
	if ( sMessage == NULL ) sMessage = "";
	snprintf(sError, (size_t)iCapacity, "%s", sMessage);
}

static void __xuiCodeLanguageFreeEntry(xui_code_language_entry_t* pEntry)
{
	int i;

	if ( pEntry == NULL ) return;
	xrtFree(pEntry->sId);
	xrtFree(pEntry->sName);
	for ( i = 0; i < pEntry->tLanguage.iExtensionCount; i++ ) xrtFree(pEntry->pExtensions[i]);
	xrtFree(pEntry->pExtensions);
	xrtFree(pEntry->sLineComment);
	xrtFree(pEntry->sBlockCommentStart);
	xrtFree(pEntry->sBlockCommentEnd);
	for ( i = 0; i < pEntry->tLanguage.iRegexRuleCount; i++ ) {
		xrtFree(pEntry->pRuleNames[i]);
		xrtFree(pEntry->pRulePatterns[i]);
	}
	xrtFree(pEntry->pRuleNames);
	xrtFree(pEntry->pRulePatterns);
	xrtFree(pEntry->pRegexRules);
	memset(pEntry, 0, sizeof(*pEntry));
}

static void __xuiCodeLanguagePublish(xui_code_language_entry_t* pEntry)
{
	int i;

	pEntry->tLanguage.iSize = sizeof(pEntry->tLanguage);
	pEntry->tLanguage.sId = pEntry->sId;
	pEntry->tLanguage.sName = pEntry->sName;
	pEntry->tLanguage.arrExtensions = (const char* const*)pEntry->pExtensions;
	pEntry->tLanguage.sLineComment = pEntry->sLineComment;
	pEntry->tLanguage.sBlockCommentStart = pEntry->sBlockCommentStart;
	pEntry->tLanguage.sBlockCommentEnd = pEntry->sBlockCommentEnd;
	pEntry->tLanguage.pRegexRules = pEntry->pRegexRules;
	for ( i = 0; i < pEntry->tLanguage.iRegexRuleCount; i++ ) {
		pEntry->pRegexRules[i].iSize = sizeof(pEntry->pRegexRules[i]);
		pEntry->pRegexRules[i].sName = pEntry->pRuleNames[i];
		pEntry->pRegexRules[i].sPattern = pEntry->pRulePatterns[i];
	}
}

static int __xuiCodeLanguageReserve(xui_code_language_registry pRegistry, int iCapacity)
{
	xui_code_language_entry_t* pNew;

	if ( iCapacity <= pRegistry->iCapacity ) return XUI_OK;
	if ( iCapacity < pRegistry->iCapacity * 2 ) iCapacity = pRegistry->iCapacity * 2;
	if ( iCapacity < 8 ) iCapacity = 8;
	pNew = (xui_code_language_entry_t*)xrtRealloc(pRegistry->pEntries, sizeof(*pNew) * (size_t)iCapacity);
	if ( pNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	memset(pNew + pRegistry->iCapacity, 0, sizeof(*pNew) * (size_t)(iCapacity - pRegistry->iCapacity));
	pRegistry->pEntries = pNew;
	pRegistry->iCapacity = iCapacity;
	return XUI_OK;
}

static int __xuiCodeLanguageCopyEntry(xui_code_language_entry_t* pEntry, const xui_code_language_t* pLanguage)
{
	int i;

	memset(pEntry, 0, sizeof(*pEntry));
	pEntry->tLanguage = *pLanguage;
	pEntry->sId = __xuiCodeLanguageStrDup(pLanguage->sId);
	pEntry->sName = __xuiCodeLanguageStrDup((pLanguage->sName != NULL) ? pLanguage->sName : pLanguage->sId);
	pEntry->sLineComment = __xuiCodeLanguageStrDup(pLanguage->sLineComment);
	pEntry->sBlockCommentStart = __xuiCodeLanguageStrDup(pLanguage->sBlockCommentStart);
	pEntry->sBlockCommentEnd = __xuiCodeLanguageStrDup(pLanguage->sBlockCommentEnd);
	if ( pEntry->sId == NULL || pEntry->sName == NULL ) goto oom;
	if ( pLanguage->sLineComment != NULL && pEntry->sLineComment == NULL ) goto oom;
	if ( pLanguage->sBlockCommentStart != NULL && pEntry->sBlockCommentStart == NULL ) goto oom;
	if ( pLanguage->sBlockCommentEnd != NULL && pEntry->sBlockCommentEnd == NULL ) goto oom;
	if ( pLanguage->iExtensionCount > 0 ) {
		pEntry->pExtensions = (char**)xrtMalloc(sizeof(char*) * (size_t)pLanguage->iExtensionCount);
		if ( pEntry->pExtensions == NULL ) goto oom;
		memset(pEntry->pExtensions, 0, sizeof(char*) * (size_t)pLanguage->iExtensionCount);
		for ( i = 0; i < pLanguage->iExtensionCount; i++ ) {
			pEntry->pExtensions[i] = __xuiCodeLanguageStrDup(__xuiCodeLanguageNormalizeExtension(pLanguage->arrExtensions[i]));
			if ( pEntry->pExtensions[i] == NULL ) goto oom;
		}
	}
	if ( pLanguage->iRegexRuleCount > 0 ) {
		pEntry->pRegexRules = (xui_code_regex_rule_t*)xrtMalloc(sizeof(*pEntry->pRegexRules) * (size_t)pLanguage->iRegexRuleCount);
		pEntry->pRuleNames = (char**)xrtMalloc(sizeof(char*) * (size_t)pLanguage->iRegexRuleCount);
		pEntry->pRulePatterns = (char**)xrtMalloc(sizeof(char*) * (size_t)pLanguage->iRegexRuleCount);
		if ( pEntry->pRegexRules == NULL || pEntry->pRuleNames == NULL || pEntry->pRulePatterns == NULL ) goto oom;
		memset(pEntry->pRegexRules, 0, sizeof(*pEntry->pRegexRules) * (size_t)pLanguage->iRegexRuleCount);
		memset(pEntry->pRuleNames, 0, sizeof(char*) * (size_t)pLanguage->iRegexRuleCount);
		memset(pEntry->pRulePatterns, 0, sizeof(char*) * (size_t)pLanguage->iRegexRuleCount);
		for ( i = 0; i < pLanguage->iRegexRuleCount; i++ ) {
			pEntry->pRegexRules[i] = pLanguage->pRegexRules[i];
			pEntry->pRuleNames[i] = __xuiCodeLanguageStrDup(pLanguage->pRegexRules[i].sName);
			pEntry->pRulePatterns[i] = __xuiCodeLanguageStrDup(pLanguage->pRegexRules[i].sPattern);
			if ( pLanguage->pRegexRules[i].sPattern != NULL && pEntry->pRulePatterns[i] == NULL ) goto oom;
		}
	}
	__xuiCodeLanguagePublish(pEntry);
	return XUI_OK;

oom:
	__xuiCodeLanguageFreeEntry(pEntry);
	return XUI_ERROR_OUT_OF_MEMORY;
}

static int __xuiCodeLanguageCLex(xui_code_document_t* pDocument, int iStartLine, int iEndLine, xui_code_token_t* pTokens, int iTokenCapacity, int* pTokenCount, void* pUser)
{
	int iStartOffset;
	int iEndOffset;
	int iLineCount;
	int iRet;

	(void)pUser;
	if ( (pDocument == NULL) || (pTokenCount == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	iLineCount = xuiCodeDocumentGetLineCount(pDocument);
	if ( iStartLine < 0 ) iStartLine = 0;
	if ( iEndLine < 0 || iEndLine >= iLineCount ) iEndLine = iLineCount - 1;
	if ( iStartLine > iEndLine ) {
		*pTokenCount = 0;
		return XUI_OK;
	}
	iRet = xuiCodeDocumentGetLineRange(pDocument, iStartLine, &iStartOffset, NULL);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiCodeDocumentGetLineRange(pDocument, iEndLine, NULL, &iEndOffset);
	if ( iRet != XUI_OK ) return iRet;
	return xuiCodeLexerCTokenizeDocumentRange(pDocument, iStartOffset, iEndOffset,
		pTokens, iTokenCapacity, pTokenCount);
}

static int __xuiCodeLanguageRegexLexRange(const xui_code_language_t* pLanguage,
	xui_code_document pDocument, int iStartLine, int iEndLine,
	xui_code_token_t* pTokens, int iTokenCapacity, int* pTokenCount,
	char* sError, int iErrorCapacity)
{
	char* sRange;
	int iLineCount;
	int iStartOffset;
	int iEndOffset;
	int iRangeLength;
	int iStoredCount;
	int i;
	int iRet;

	iLineCount = xuiCodeDocumentGetLineCount(pDocument);
	if ( iStartLine < 0 ) iStartLine = 0;
	if ( iEndLine < 0 || iEndLine >= iLineCount ) iEndLine = iLineCount - 1;
	if ( iStartLine > iEndLine ) {
		*pTokenCount = 0;
		return XUI_OK;
	}
	iRet = xuiCodeDocumentGetLineRange(pDocument, iStartLine, &iStartOffset, NULL);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiCodeDocumentGetLineRange(pDocument, iEndLine, NULL, &iEndOffset);
	if ( iRet != XUI_OK ) return iRet;
	iRangeLength = iEndOffset - iStartOffset;
	sRange = (char*)xrtMalloc((size_t)iRangeLength + 1u);
	if ( sRange == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	iRet = xuiCodeDocumentCopyRange(pDocument, iStartOffset, iEndOffset,
		sRange, iRangeLength + 1, NULL);
	if ( iRet == XUI_OK ) {
		iRet = xuiCodeLexerRegexTokenize(sRange, iRangeLength,
			pLanguage->pRegexRules, pLanguage->iRegexRuleCount,
			pTokens, iTokenCapacity, pTokenCount, sError, iErrorCapacity);
	}
	if ( iRet == XUI_OK && pTokens != NULL ) {
		iStoredCount = *pTokenCount;
		if ( iStoredCount > iTokenCapacity ) iStoredCount = iTokenCapacity;
		for ( i = 0; i < iStoredCount; i++ ) {
			pTokens[i].iStartOffset += iStartOffset;
			pTokens[i].iEndOffset += iStartOffset;
		}
	}
	xrtFree(sRange);
	return iRet;
}

static int __xuiCodeLanguageCFold(xui_code_document_t* pDocument, xui_code_fold_range_t* pRanges, int iRangeCapacity, int* pRangeCount, void* pUser)
{
	(void)pUser;
	if ( pDocument == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiCodeFoldCBuildDocumentRanges(pDocument, pRanges, iRangeCapacity, pRangeCount);
}

XUI_API int xuiCodeLanguageRegistryCreate(xui_code_language_registry* ppRegistry)
{
	xui_code_language_registry pRegistry;

	if ( ppRegistry == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppRegistry = NULL;
	pRegistry = (xui_code_language_registry)xrtMalloc(sizeof(*pRegistry));
	if ( pRegistry == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	memset(pRegistry, 0, sizeof(*pRegistry));
	*ppRegistry = pRegistry;
	return XUI_OK;
}

XUI_API void xuiCodeLanguageRegistryDestroy(xui_code_language_registry pRegistry)
{
	if ( pRegistry == NULL ) return;
	xuiCodeLanguageRegistryClear(pRegistry);
	xrtFree(pRegistry->pEntries);
	xrtFree(pRegistry);
}

XUI_API void xuiCodeLanguageRegistryClear(xui_code_language_registry pRegistry)
{
	int i;

	if ( pRegistry == NULL ) return;
	for ( i = 0; i < pRegistry->iCount; i++ ) __xuiCodeLanguageFreeEntry(&pRegistry->pEntries[i]);
	pRegistry->iCount = 0;
}

XUI_API int xuiCodeLanguageRegistryLoadDefaults(xui_code_language_registry pRegistry)
{
	static const char* arrCExtensions[] = { "c", "h" };
	xui_code_language_t tLanguage;

	if ( pRegistry == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&tLanguage, 0, sizeof(tLanguage));
	tLanguage.iSize = sizeof(tLanguage);
	tLanguage.sId = "c";
	tLanguage.sName = "C";
	tLanguage.arrExtensions = arrCExtensions;
	tLanguage.iExtensionCount = 2;
	tLanguage.sLineComment = "//";
	tLanguage.sBlockCommentStart = "/*";
	tLanguage.sBlockCommentEnd = "*/";
	tLanguage.iLexerType = XUI_CODE_LEXER_BUILTIN_C;
	tLanguage.onLex = __xuiCodeLanguageCLex;
	tLanguage.onFold = __xuiCodeLanguageCFold;
	return xuiCodeLanguageRegistryRegister(pRegistry, &tLanguage);
}

XUI_API int xuiCodeLanguageRegistryRegister(xui_code_language_registry pRegistry, const xui_code_language_t* pLanguage)
{
	xui_code_language_entry_t tEntry;
	int i;
	int iRet;

	if ( (pRegistry == NULL) || (pLanguage == NULL) || (pLanguage->sId == NULL) || (pLanguage->sId[0] == '\0') ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pLanguage->iExtensionCount < 0 || (pLanguage->iExtensionCount > 0 && pLanguage->arrExtensions == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pLanguage->iRegexRuleCount < 0 || (pLanguage->iRegexRuleCount > 0 && pLanguage->pRegexRules == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiCodeLanguageCopyEntry(&tEntry, pLanguage);
	if ( iRet != XUI_OK ) return iRet;
	for ( i = 0; i < pRegistry->iCount; i++ ) {
		if ( __xuiCodeLanguageEquals(pRegistry->pEntries[i].tLanguage.sId, pLanguage->sId) ) {
			__xuiCodeLanguageFreeEntry(&pRegistry->pEntries[i]);
			pRegistry->pEntries[i] = tEntry;
			return XUI_OK;
		}
	}
	iRet = __xuiCodeLanguageReserve(pRegistry, pRegistry->iCount + 1);
	if ( iRet != XUI_OK ) {
		__xuiCodeLanguageFreeEntry(&tEntry);
		return iRet;
	}
	pRegistry->pEntries[pRegistry->iCount++] = tEntry;
	return XUI_OK;
}

XUI_API int xuiCodeLanguageRegistryGetCount(xui_code_language_registry pRegistry)
{
	return (pRegistry != NULL) ? pRegistry->iCount : 0;
}

XUI_API int xuiCodeLanguageRegistryGet(xui_code_language_registry pRegistry, int iIndex, xui_code_language_t* pLanguage)
{
	if ( (pRegistry == NULL) || (pLanguage == NULL) || (iIndex < 0) || (iIndex >= pRegistry->iCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pLanguage = pRegistry->pEntries[iIndex].tLanguage;
	pLanguage->iSize = sizeof(*pLanguage);
	return XUI_OK;
}

XUI_API int xuiCodeLanguageRegistryFind(xui_code_language_registry pRegistry, const char* sId, xui_code_language_t* pLanguage)
{
	int i;

	if ( (pRegistry == NULL) || (sId == NULL) || (pLanguage == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < pRegistry->iCount; i++ ) {
		if ( __xuiCodeLanguageEquals(pRegistry->pEntries[i].tLanguage.sId, sId) ) {
			*pLanguage = pRegistry->pEntries[i].tLanguage;
			pLanguage->iSize = sizeof(*pLanguage);
			return XUI_OK;
		}
	}
	return XUI_ERROR_UNSUPPORTED;
}

XUI_API int xuiCodeLanguageRegistryFindByExtension(xui_code_language_registry pRegistry, const char* sExtension, xui_code_language_t* pLanguage)
{
	const char* sNorm;
	int i;
	int j;

	if ( (pRegistry == NULL) || (sExtension == NULL) || (pLanguage == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	sNorm = __xuiCodeLanguageNormalizeExtension(sExtension);
	for ( i = 0; i < pRegistry->iCount; i++ ) {
		for ( j = 0; j < pRegistry->pEntries[i].tLanguage.iExtensionCount; j++ ) {
			if ( __xuiCodeLanguageEquals(pRegistry->pEntries[i].tLanguage.arrExtensions[j], sNorm) ) {
				*pLanguage = pRegistry->pEntries[i].tLanguage;
				pLanguage->iSize = sizeof(*pLanguage);
				return XUI_OK;
			}
		}
	}
	return XUI_ERROR_UNSUPPORTED;
}

XUI_API int xuiCodeLanguageLex(const xui_code_language_t* pLanguage, xui_code_document pDocument, int iStartLine, int iEndLine, xui_code_token_t* pTokens, int iTokenCapacity, int* pTokenCount, char* sError, int iErrorCapacity)
{
	if ( (pLanguage == NULL) || (pDocument == NULL) || (pTokenCount == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pTokenCount = 0;
	__xuiCodeLanguageSetError(sError, iErrorCapacity, "");
	if ( pLanguage->onLex != NULL ) return pLanguage->onLex(pDocument, iStartLine, iEndLine, pTokens, iTokenCapacity, pTokenCount, pLanguage->pUser);
	if ( pLanguage->iLexerType == XUI_CODE_LEXER_REGEX && pLanguage->pRegexRules != NULL ) {
		return __xuiCodeLanguageRegexLexRange(pLanguage, pDocument,
			iStartLine, iEndLine, pTokens, iTokenCapacity, pTokenCount,
			sError, iErrorCapacity);
	}
	__xuiCodeLanguageSetError(sError, iErrorCapacity, "language has no lexer");
	return XUI_ERROR_UNSUPPORTED;
}

XUI_API int xuiCodeLanguageFold(const xui_code_language_t* pLanguage, xui_code_document pDocument, xui_code_fold_range_t* pRanges, int iRangeCapacity, int* pRangeCount)
{
	if ( (pLanguage == NULL) || (pDocument == NULL) || (pRangeCount == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pRangeCount = 0;
	if ( pLanguage->onFold != NULL ) return pLanguage->onFold(pDocument, pRanges, iRangeCapacity, pRangeCount, pLanguage->pUser);
	return XUI_ERROR_UNSUPPORTED;
}
