#include "../internal/xrt_regex.h"

#include <stdio.h>



#if defined(XREGEX_FEATURE_REGEX_SET)

/* 释放尚未公开或最后一个引用持有的集合。 */
static void __xrtRegexSetDestroy(xregexset* pSet)
{
	if ( pSet == NULL ) {
		return;
	}
	bbre_set_destroy(pSet->Engine);
	for ( size_t i = 0; i < pSet->Count; i++ ) {
		xrtRegexRelease(pSet->Regex[i]);
	}
	xrtFree(pSet);
}



/* 将 BBRE 集合构建错误映射到 XRT 错误模型。 */
static void __xrtRegexSetEngineError(void)
{
	if ( xrtGetError() == NULL ) {
		__xrtRegexSetOutOfMemory();
	}
}



/* 用模式索引包装批量编译失败，同时保留原始原因链。 */
static void __xrtRegexSetPatternError(size_t iIndex)
{
	char arrData[64];
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	if ( (pCause != NULL) && (xrtErrorKind(pCause) == XERR_MEMORY) ) {
		xrtSetErrorTake(pCause);
		return;
	}
	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : XERR_VALUE;
	Desc.Code = pCause != NULL ? xrtErrorCode(pCause) : XREGEX_ERROR_PATTERN;
	Desc.Domain = "xrt.regex";
	Desc.Operation = "set_compile";
	Desc.Message = "regular expression set pattern failed to compile";
	(void)snprintf(arrData, sizeof(arrData), "pattern=%llu", (unsigned long long)iIndex);
	Desc.Data = arrData;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	xrtErrorFree(pCause);
	if ( pError != NULL ) {
		xrtSetErrorTake(pError);
	}
}



/* 检查 matcher 已经完成一轮匹配并可读取结果。 */
static bool __xrtRegexSetMatcherResultValid(const xregexsetmatcher* pMatcher)
{
	if ( pMatcher == NULL ) {
		__xrtRegexSetInvalidArgument();
		return false;
	}
	if ( !pMatcher->HasResult ) {
		__xrtRegexSetInvalidState();
		return false;
	}
	return true;
}



/* 从已有编译对象创建集合。 */
XRT_API xregexset* xrtRegexSetCreate(
	xregex* const* arrRegex,
	size_t iCount
)
{
	bbre_alloc Alloc;
	bbre_set_builder* pBuilder = NULL;
	xregexset* pSet;
	size_t iBytes;
	int iResult = 0;

	if ( (arrRegex == NULL) && (iCount != 0) ) {
		__xrtRegexSetInvalidArgument();
		return NULL;
	}
	if ( (iCount > UINT_MAX) ||
		 (iCount > ((SIZE_MAX - sizeof(*pSet)) / sizeof(xregex*))) ) {
		__xrtRegexSetSizeOverflow();
		return NULL;
	}
	iBytes = sizeof(*pSet) + (iCount * sizeof(xregex*));
	pSet = (xregexset*)xrtCalloc(1, iBytes);
	if ( pSet == NULL ) {
		return NULL;
	}
	pSet->RefCount = 1;
	pSet->Count = iCount;
	pSet->Regex = (xregex**)(pSet + 1);
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( arrRegex[i] == NULL ) {
			__xrtRegexSetInvalidArgument();
			__xrtRegexSetDestroy(pSet);
			return NULL;
		}
		pSet->Regex[i] = xrtRegexRef(arrRegex[i]);
		if ( pSet->Regex[i] == NULL ) {
			__xrtRegexSetDestroy(pSet);
			return NULL;
		}
	}
	if ( iCount == 0 ) {
		return pSet;
	}
	Alloc.user = NULL;
	Alloc.cb = __xrtRegexAlloc;
	iResult = bbre_set_builder_init(&pBuilder, &Alloc);
	for ( size_t i = 0; (iResult == 0) && (i < iCount); i++ ) {
		iResult = bbre_set_builder_add(pBuilder, pSet->Regex[i]->Engine);
	}
	if ( iResult == 0 ) {
		iResult = bbre_set_init(&pSet->Engine, pBuilder, &Alloc);
	}
	bbre_set_builder_destroy(pBuilder);
	if ( iResult != 0 ) {
		__xrtRegexSetEngineError();
		__xrtRegexSetDestroy(pSet);
		return NULL;
	}
	return pSet;
}



/* 使用同一高级配置批量编译模式并创建集合。 */
XRT_API xregexset* xrtRegexSetCompileConfig(
	const xstrview* arrPattern,
	size_t iCount,
	const xregexconfig* pConfig
)
{
	xregex** arrRegex;
	xregexset* pSet = NULL;
	size_t iCompiled = 0;

	if ( !__xrtRegexConfigValid(pConfig) ) {
		return NULL;
	}
	if ( (arrPattern == NULL) && (iCount != 0) ) {
		__xrtRegexSetInvalidArgument();
		return NULL;
	}
	if ( (iCount > UINT_MAX) || (iCount > (SIZE_MAX / sizeof(xregex*))) ) {
		__xrtRegexSetSizeOverflow();
		return NULL;
	}
	if ( iCount == 0 ) {
		return xrtRegexSetCreate(NULL, 0);
	}
	arrRegex = (xregex**)xrtCalloc(iCount, sizeof(xregex*));
	if ( arrRegex == NULL ) {
		return NULL;
	}
	for ( ; iCompiled < iCount; iCompiled++ ) {
		arrRegex[iCompiled] = xrtRegexCompileConfig(arrPattern[iCompiled], pConfig);
		if ( arrRegex[iCompiled] == NULL ) {
			__xrtRegexSetPatternError(iCompiled);
			break;
		}
	}
	if ( iCompiled == iCount ) {
		pSet = xrtRegexSetCreate(arrRegex, iCount);
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		xrtRegexRelease(arrRegex[i]);
	}
	xrtFree(arrRegex);
	return pSet;
}



/* 使用默认配置批量编译模式并创建集合。 */
XRT_API xregexset* xrtRegexSetCompile(
	const xstrview* arrPattern,
	size_t iCount
)
{
	xregexconfig Config;

	xrtRegexConfigInit(&Config);
	return xrtRegexSetCompileConfig(arrPattern, iCount, &Config);
}



/* 增加集合引用并返回原指针。 */
XRT_API xregexset* xrtRegexSetRef(xregexset* pSet)
{
	if ( pSet == NULL ) {
		__xrtRegexSetInvalidArgument();
		return NULL;
	}
	if ( xrtRefRetain(&pSet->RefCount) < 0 ) {
		__xrtRegexSetInvalidState();
		return NULL;
	}
	return pSet;
}



/* 释放集合引用。 */
XRT_API void xrtRegexSetRelease(xregexset* pSet)
{
	if ( (pSet != NULL) && (xrtRefRelease(&pSet->RefCount) == 0) ) {
		__xrtRegexSetDestroy(pSet);
	}
}



/* 返回集合中的模式数量。 */
XRT_API size_t xrtRegexSetCount(const xregexset* pSet)
{
	if ( pSet == NULL ) {
		__xrtRegexSetInvalidArgument();
		return 0;
	}
	return pSet->Count;
}



/* 返回集合借用的指定编译对象。 */
XRT_API const xregex* xrtRegexSetRegex(
	const xregexset* pSet,
	size_t iIndex
)
{
	if ( pSet == NULL ) {
		__xrtRegexSetInvalidArgument();
		return NULL;
	}
	if ( iIndex >= pSet->Count ) {
		__xrtRegexSetRange();
		return NULL;
	}
	return pSet->Regex[iIndex];
}



/* 从批量编译错误中读取失败的模式索引。 */
XRT_API bool xrtRegexSetErrorIndex(
	const xerror* pError,
	size_t* pIndex
)
{
	cstr sData;
	unsigned long long iValue;
	char iTail;

	if ( pIndex == NULL ) {
		__xrtRegexSetInvalidArgument();
		return false;
	}
	if ( (pError == NULL) || (xrtErrorDomain(pError) == NULL) ||
		 (strcmp(xrtErrorDomain(pError), "xrt.regex") != 0) ) {
		return false;
	}
	sData = xrtErrorData(pError);
	if ( (sData == NULL) ||
		 (sscanf(sData, "pattern=%llu%c", &iValue, &iTail) != 1) ||
		 (iValue > (unsigned long long)SIZE_MAX) ) {
		return false;
	}
	*pIndex = (size_t)iValue;
	return true;
}



/* 为不可变集合创建可复用 matcher。 */
XRT_API xregexsetmatcher* xrtRegexSetMatcherCreate(xregexset* pSet)
{
	bbre_alloc Alloc;
	xregexsetmatcher* pMatcher;
	size_t iIndexBytes;

	if ( pSet == NULL ) {
		__xrtRegexSetInvalidArgument();
		return NULL;
	}
	if ( pSet->Count > ((SIZE_MAX - sizeof(*pMatcher)) / sizeof(unsigned int)) ) {
		__xrtRegexSetSizeOverflow();
		return NULL;
	}
	iIndexBytes = pSet->Count * sizeof(unsigned int);
	pMatcher = (xregexsetmatcher*)xrtCalloc(1, sizeof(*pMatcher) + iIndexBytes);
	if ( pMatcher == NULL ) {
		return NULL;
	}
	pMatcher->Indices = (unsigned int*)(pMatcher + 1);
	pMatcher->Set = xrtRegexSetRef(pSet);
	if ( pMatcher->Set == NULL ) {
		xrtFree(pMatcher);
		return NULL;
	}
	if ( pSet->Count == 0 ) {
		return pMatcher;
	}
	Alloc.user = NULL;
	Alloc.cb = __xrtRegexAlloc;
	if ( bbre_xrt_context_init_set(&pMatcher->Context, pSet->Engine, &Alloc) != 0 ) {
		__xrtRegexSetEngineError();
		xrtRegexSetMatcherFree(pMatcher);
		return NULL;
	}
	return pMatcher;
}



/* 释放集合 matcher 及其命中索引。 */
XRT_API void xrtRegexSetMatcherFree(xregexsetmatcher* pMatcher)
{
	if ( pMatcher == NULL ) {
		return;
	}
	bbre_xrt_context_destroy(pMatcher->Context);
	xrtRegexSetRelease(pMatcher->Set);
	xrtFree(pMatcher);
}



/* 从指定字节位置开始计算所有命中的模式。 */
XRT_API xregexresult xrtRegexSetMatcherMatch(
	xregexsetmatcher* pMatcher,
	xstrview Text,
	size_t iStart
)
{
	static const char sEmpty[] = "";
	unsigned int iMatched = 0;
	xregexresult Result;
	int iEngine;

	if ( (pMatcher == NULL) || !__xrtRegexViewValid(Text) || (iStart > Text.Size) ) {
		if ( (pMatcher == NULL) || (iStart > Text.Size) ) {
			__xrtRegexSetInvalidArgument();
		}
		return XREGEX_ERROR;
	}
	pMatcher->HasResult = true;
	pMatcher->MatchCount = 0;
	if ( pMatcher->Set->Count == 0 ) {
		return XREGEX_NONE;
	}
	iEngine = bbre_xrt_context_set_match(
		pMatcher->Context,
		Text.Data != NULL ? Text.Data : sEmpty,
		Text.Size,
		iStart,
		pMatcher->Indices,
		(unsigned int)pMatcher->Set->Count,
		&iMatched
	);
	Result = __xrtRegexResult(iEngine, "set_match");
	if ( Result == XREGEX_MATCH ) {
		pMatcher->MatchCount = iMatched;
	}
	return Result;
}



/* 返回本轮命中的模式数量。 */
XRT_API size_t xrtRegexSetMatcherCount(const xregexsetmatcher* pMatcher)
{
	if ( !__xrtRegexSetMatcherResultValid(pMatcher) ) {
		return 0;
	}
	return pMatcher->MatchCount;
}



/* 返回本轮第 iIndex 个命中的模式索引。 */
XRT_API size_t xrtRegexSetMatcherIndex(
	const xregexsetmatcher* pMatcher,
	size_t iIndex
)
{
	if ( !__xrtRegexSetMatcherResultValid(pMatcher) ) {
		return XRT_NPOS;
	}
	if ( iIndex >= pMatcher->MatchCount ) {
		__xrtRegexSetRange();
		return XRT_NPOS;
	}
	return pMatcher->Indices[iIndex];
}



/* 判断指定模式是否在本轮命中。 */
XRT_API bool xrtRegexSetMatcherMatched(
	const xregexsetmatcher* pMatcher,
	size_t iPattern
)
{
	if ( !__xrtRegexSetMatcherResultValid(pMatcher) ) {
		return false;
	}
	if ( iPattern >= pMatcher->Set->Count ) {
		__xrtRegexSetRange();
		return false;
	}
	for ( size_t i = 0; i < pMatcher->MatchCount; i++ ) {
		if ( pMatcher->Indices[i] == iPattern ) {
			return true;
		}
	}
	return false;
}



/* 返回本轮最小的命中模式索引。 */
XRT_API size_t xrtRegexSetMatcherFirst(const xregexsetmatcher* pMatcher)
{
	if ( !__xrtRegexSetMatcherResultValid(pMatcher) ) {
		return XRT_NPOS;
	}
	return pMatcher->MatchCount != 0 ? pMatcher->Indices[0] : XRT_NPOS;
}



/* 使用临时 matcher 检查集合中是否有模式命中。 */
XRT_API xregexresult xrtRegexSetTest(
	xregexset* pSet,
	xstrview Text
)
{
	xregexsetmatcher* pMatcher = xrtRegexSetMatcherCreate(pSet);
	xregexresult Result;

	if ( pMatcher == NULL ) {
		return XREGEX_ERROR;
	}
	Result = xrtRegexSetMatcherMatch(pMatcher, Text, 0);
	xrtRegexSetMatcherFree(pMatcher);
	return Result;
}

#endif
