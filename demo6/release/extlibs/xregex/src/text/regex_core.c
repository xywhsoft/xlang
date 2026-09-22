#include "../internal/xrt_regex.h"

#include <stdio.h>



#if defined(XREGEX_FEATURE_REGEX_CORE)

#define XRT_REGEX_FLAG_MASK \
	(XREGEX_IGNORE_CASE | XREGEX_MULTILINE | XREGEX_DOT_ALL | XREGEX_UNGREEDY)



/* 把 XRT 分配器适配为 BBRE 的 realloc 风格回调。 */
void* __xrtRegexAlloc(
	void* pUser,
	void* pMemory,
	size_t iOldSize,
	size_t iNewSize
)
{
	(void)pUser;
	(void)iOldSize;
	if ( iNewSize == 0 ) {
		xrtFree(pMemory);
		return NULL;
	}
	return xrtRealloc(pMemory, iNewSize);
}



/* 检查字符串视图的指针和大小组合。 */
bool __xrtRegexViewValid(xstrview Text)
{
	if ( (Text.Data == NULL) && (Text.Size != 0) ) {
		__xrtRegexSetInvalidArgument();
		return false;
	}
	return true;
}



/* 设置带稳定域、代码和可选字节位置的正则错误。 */
void __xrtRegexError(
	xerrkind Kind,
	xregexerror Code,
	cstr sOperation,
	cstr sMessage,
	bool bHasOffset,
	size_t iOffset
)
{
	char arrData[64];
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Code = (int32)Code;
	Desc.Domain = "xrt.regex";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	if ( bHasOffset ) {
		(void)snprintf(arrData, sizeof(arrData), "offset=%llu", (unsigned long long)iOffset);
		Desc.Data = arrData;
	}
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetErrorTake(pError);
	}
}



/* 将 BBRE 执行错误转换为 XRT 三态结果。 */
xregexresult __xrtRegexResult(int iResult, cstr sOperation)
{
	if ( iResult > 0 ) {
		return XREGEX_MATCH;
	}
	if ( iResult == 0 ) {
		return XREGEX_NONE;
	}
	if ( iResult == BBRE_ERR_MEM ) {
		if ( xrtGetError() == NULL ) {
			__xrtRegexSetOutOfMemory();
		}
	} else {
		__xrtRegexError(
			XERR_INTERNAL,
			XREGEX_ERROR_EXECUTE,
			sOperation,
			"regular expression engine failed",
			false,
			0
		);
	}
	return XREGEX_ERROR;
}



/* 检查高级编译配置已经完整初始化。 */
bool __xrtRegexConfigValid(const xregexconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtRegexSetInvalidArgument();
		return false;
	}
	if ( ((pConfig->Flags & ~XRT_REGEX_FLAG_MASK) != 0) ||
		 (pConfig->MaxPatternBytes == 0) ||
		 (pConfig->MaxCaptures == 0) ) {
		__xrtRegexError(
			XERR_ARGUMENT,
			XREGEX_ERROR_CONFIG,
			"compile",
			"invalid regular expression configuration",
			false,
			0
		);
		return false;
	}
	for ( size_t i = 0; i < (sizeof(pConfig->Reserved) / sizeof(pConfig->Reserved[0])); i++ ) {
		if ( pConfig->Reserved[i] != 0 ) {
			__xrtRegexError(
				XERR_ARGUMENT,
				XREGEX_ERROR_CONFIG,
				"compile",
				"reserved regular expression configuration fields must be zero",
				false,
				0
			);
			return false;
		}
	}
	return true;
}



/* 将公共编译标志映射为 BBRE 标志。 */
static bbre_flags __xrtRegexFlagsToEngine(uint32 iFlags)
{
	unsigned int iEngine = 0;

	if ( (iFlags & XREGEX_IGNORE_CASE) != 0 ) {
		iEngine |= BBRE_FLAG_INSENSITIVE;
	}
	if ( (iFlags & XREGEX_MULTILINE) != 0 ) {
		iEngine |= BBRE_FLAG_MULTILINE;
	}
	if ( (iFlags & XREGEX_DOT_ALL) != 0 ) {
		iEngine |= BBRE_FLAG_DOTNEWLINE;
	}
	if ( (iFlags & XREGEX_UNGREEDY) != 0 ) {
		iEngine |= BBRE_FLAG_UNGREEDY;
	}
	return (bbre_flags)iEngine;
}



/* 释放尚未公开或最后一个引用持有的编译对象。 */
static void __xrtRegexDestroy(xregex* pRegex)
{
	if ( pRegex == NULL ) {
		return;
	}
	bbre_destroy(pRegex->Engine);
	xrtFree(pRegex->Pattern);
	xrtFree(pRegex);
}



/* 初始化默认编译标志和有限资源预算。 */
XRT_API void xrtRegexConfigInit(xregexconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtRegexSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->MaxPatternBytes = XREGEX_PATTERN_DEFAULT;
	pConfig->MaxCaptures = XREGEX_CAPTURES_DEFAULT;
}



/* 使用高级配置编译正则表达式。 */
XRT_API xregex* xrtRegexCompileConfig(
	xstrview Pattern,
	const xregexconfig* pConfig
)
{
	static const char sEmpty[] = "";
	bbre_alloc Alloc;
	bbre_builder* pBuilder = NULL;
	xregex* pRegex;
	int iResult;

	if ( !__xrtRegexViewValid(Pattern) || !__xrtRegexConfigValid(pConfig) ) {
		return NULL;
	}
	if ( Pattern.Size > pConfig->MaxPatternBytes ) {
		__xrtRegexError(
			XERR_RANGE,
			XREGEX_ERROR_LIMIT,
			"compile",
			"regular expression pattern exceeds its byte limit",
			false,
			0
		);
		return NULL;
	}
	pRegex = (xregex*)xrtCalloc(1, sizeof(*pRegex));
	if ( pRegex == NULL ) {
		return NULL;
	}
	if ( Pattern.Size == SIZE_MAX ) {
		__xrtRegexSetSizeOverflow();
		__xrtRegexDestroy(pRegex);
		return NULL;
	}
	pRegex->Pattern = (str)xrtMalloc(Pattern.Size + 1u);
	if ( pRegex->Pattern == NULL ) {
		__xrtRegexDestroy(pRegex);
		return NULL;
	}
	if ( Pattern.Size != 0 ) {
		memcpy(pRegex->Pattern, Pattern.Data != NULL ? Pattern.Data : sEmpty, Pattern.Size);
	}
	pRegex->Pattern[Pattern.Size] = 0;
	pRegex->PatternSize = Pattern.Size;
	pRegex->Flags = pConfig->Flags;
	pRegex->RefCount = 1;
	Alloc.user = NULL;
	Alloc.cb = __xrtRegexAlloc;
	iResult = bbre_builder_init(
		&pBuilder,
		pRegex->Pattern,
		pRegex->PatternSize,
		&Alloc
	);
	if ( iResult == 0 ) {
		bbre_builder_flags(pBuilder, __xrtRegexFlagsToEngine(pConfig->Flags));
		iResult = bbre_init(&pRegex->Engine, pBuilder, &Alloc);
	}
	bbre_builder_destroy(pBuilder);
	if ( iResult != 0 ) {
		if ( iResult == BBRE_ERR_MEM ) {
			if ( xrtGetError() == NULL ) {
				__xrtRegexSetOutOfMemory();
			}
		} else {
			__xrtRegexError(
				iResult == BBRE_ERR_LIMIT ? XERR_RANGE : XERR_VALUE,
				iResult == BBRE_ERR_LIMIT ? XREGEX_ERROR_LIMIT : XREGEX_ERROR_PATTERN,
				"compile",
				pRegex->Engine != NULL && bbre_get_err_msg(pRegex->Engine) != NULL ?
					bbre_get_err_msg(pRegex->Engine) : "invalid regular expression pattern",
				iResult == BBRE_ERR_PARSE,
				pRegex->Engine != NULL ? bbre_get_err_pos(pRegex->Engine) : 0
			);
		}
		__xrtRegexDestroy(pRegex);
		return NULL;
	}
	pRegex->CaptureCount = (size_t)bbre_capture_count(pRegex->Engine);
	if ( pRegex->CaptureCount > pConfig->MaxCaptures ) {
		__xrtRegexError(
			XERR_RANGE,
			XREGEX_ERROR_LIMIT,
			"compile",
			"regular expression exceeds its capture limit",
			false,
			0
		);
		__xrtRegexDestroy(pRegex);
		return NULL;
	}
	return pRegex;
}



/* 使用默认配置编译明确长度的 UTF-8 正则表达式。 */
XRT_API xregex* xrtRegexCompile(xstrview Pattern)
{
	xregexconfig Config;

	xrtRegexConfigInit(&Config);
	return xrtRegexCompileConfig(Pattern, &Config);
}



/* 验证表达式能否使用默认配置完成编译。 */
XRT_API bool xrtRegexValid(xstrview Pattern)
{
	xregex* pRegex = xrtRegexCompile(Pattern);

	if ( pRegex == NULL ) {
		return false;
	}
	xrtRegexRelease(pRegex);
	return true;
}



/* 增加编译对象引用并返回原指针。 */
XRT_API xregex* xrtRegexRef(xregex* pRegex)
{
	if ( pRegex == NULL ) {
		__xrtRegexSetInvalidArgument();
		return NULL;
	}
	if ( xrtRefRetain(&pRegex->RefCount) < 0 ) {
		__xrtRegexSetInvalidState();
		return NULL;
	}
	return pRegex;
}



/* 释放编译对象引用。 */
XRT_API void xrtRegexRelease(xregex* pRegex)
{
	if ( (pRegex != NULL) && (xrtRefRelease(&pRegex->RefCount) == 0) ) {
		__xrtRegexDestroy(pRegex);
	}
}



/* 返回编译对象持有的原始表达式视图。 */
XRT_API xstrview xrtRegexPattern(const xregex* pRegex)
{
	if ( pRegex == NULL ) {
		__xrtRegexSetInvalidArgument();
		return (xstrview){ NULL, 0 };
	}
	return (xstrview){ pRegex->Pattern, pRegex->PatternSize };
}



/* 返回编译时使用的标志。 */
XRT_API uint32 xrtRegexFlags(const xregex* pRegex)
{
	if ( pRegex == NULL ) {
		__xrtRegexSetInvalidArgument();
		return 0;
	}
	return pRegex->Flags;
}



/* 返回包含组 0 在内的捕获数量。 */
XRT_API size_t xrtRegexCaptureCount(const xregex* pRegex)
{
	if ( pRegex == NULL ) {
		__xrtRegexSetInvalidArgument();
		return 0;
	}
	return pRegex->CaptureCount;
}



/* 返回指定捕获的借用名称。 */
XRT_API bool xrtRegexCaptureName(
	const xregex* pRegex,
	size_t iIndex,
	xstrview* pName
)
{
	static const char sEmpty[] = "";
	cstr sName;
	size_t iSize;

	if ( (pRegex == NULL) || (pName == NULL) ) {
		__xrtRegexSetInvalidArgument();
		return false;
	}
	if ( (iIndex >= pRegex->CaptureCount) || (iIndex > UINT_MAX) ) {
		__xrtRegexSetRange();
		return false;
	}
	sName = bbre_capture_name(pRegex->Engine, (unsigned int)iIndex, &iSize);
	if ( sName == NULL ) {
		if ( iSize != 0 ) {
			__xrtRegexSetInternal();
			return false;
		}
		sName = sEmpty;
	}
	pName->Data = sName;
	pName->Size = iSize;
	return true;
}



/* 按名称查找捕获索引。 */
XRT_API size_t xrtRegexCaptureIndex(
	const xregex* pRegex,
	xstrview Name
)
{
	xstrview Current;

	if ( (pRegex == NULL) || !__xrtRegexViewValid(Name) ) {
		if ( pRegex == NULL ) {
			__xrtRegexSetInvalidArgument();
		}
		return XRT_NPOS;
	}
	for ( size_t i = 0; i < pRegex->CaptureCount; i++ ) {
		if ( !xrtRegexCaptureName(pRegex, i, &Current) ) {
			return XRT_NPOS;
		}
		if ( (Current.Size == Name.Size) &&
			 ((Name.Size == 0) || (memcmp(Current.Data, Name.Data, Name.Size) == 0)) ) {
			return i;
		}
	}
	return XRT_NPOS;
}



/* 从 xrt.regex 错误的机器数据中读取字节位置。 */
XRT_API bool xrtRegexErrorOffset(
	const xerror* pError,
	size_t* pOffset
)
{
	cstr sData;
	unsigned long long iValue;
	char iTail;

	if ( pOffset == NULL ) {
		__xrtRegexSetInvalidArgument();
		return false;
	}
	if ( (pError == NULL) || (xrtErrorDomain(pError) == NULL) ||
		 (strcmp(xrtErrorDomain(pError), "xrt.regex") != 0) ) {
		return false;
	}
	sData = xrtErrorData(pError);
	if ( (sData == NULL) ||
		 (sscanf(sData, "offset=%llu%c", &iValue, &iTail) != 1) ||
		 (iValue > (unsigned long long)SIZE_MAX) ) {
		return false;
	}
	*pOffset = (size_t)iValue;
	return true;
}

#undef XRT_REGEX_FLAG_MASK

#endif
