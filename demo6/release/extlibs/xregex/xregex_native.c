#define XRT_MODULE_ERROR_FORMAT

#include "include/xregex.h"
#include "xruntime.h"
#include "../../include/xl_extlib_abi.h"

#include <limits.h>
#include <string.h>



#if defined(_WIN32) || defined(_WIN64)
	#define XREGEX_EXPORT __declspec(dllexport)
#else
	#define XREGEX_EXPORT
#endif



typedef struct xregex_native_splitter {
	xregexsplitter* Splitter;
	str Text;
	xregexsplitpart Current;
} xregex_native_splitter;



/* 返回当前结构化错误的稳定消息。 */
XREGEX_EXPORT const char* xregex_last_error(void)
{
	const xerror* pError = xrtGetError();
	cstr sMessage = pError != NULL ? xrtErrorMessage(pError) : NULL;

	return (sMessage != NULL) && (sMessage[0] != 0) ?
		sMessage : "regular expression operation failed";
}



/* 返回当前错误中的正则字节位置，不包含位置时返回 -1。 */
XREGEX_EXPORT int64 xregex_last_error_offset(void)
{
	size_t iOffset;

	if ( !xrtRegexErrorOffset(xrtGetError(), &iOffset) ||
		 (iOffset > (size_t)INT64_MAX) ) {
		return -1;
	}
	return (int64)iOffset;
}



/* 返回批量编译失败的模式索引，不包含索引时返回 -1。 */
XREGEX_EXPORT int64 xregex_last_error_pattern_index(void)
{
	size_t iIndex;

	if ( !xrtRegexSetErrorIndex(xrtGetError(), &iIndex) ||
		 (iIndex > (size_t)INT64_MAX) ) {
		return -1;
	}
	return (int64)iIndex;
}



/* 把调用方拥有的字符串交给 XLang Buffer。 */
static xbuffer* xregex_take_string(str sText, size_t iSize)
{
	bytes pData = (bytes)sText;
	xbuffer* pBuffer;

	if ( sText == NULL ) { return NULL; }
	pBuffer = xrtBufferCreateTake(&pData, iSize, iSize + 1u);
	if ( pBuffer == NULL ) { xrtFree(sText); }
	return pBuffer;
}



/* 复制一个借用视图，确保它可以越过下一次 matcher 调用。 */
static xbuffer* xregex_copy_view(xstrview Text)
{
	xbytesview Data;

	Data.Data = (const unsigned char*)Text.Data;
	Data.Size = Text.Size;
	return xrtBufferFrom(Data);
}



/* 检查 XLang 整数能否无损映射为本机构建的 size_t。 */
static bool xregex_size(uint64 iValue, size_t* pResult, cstr sName)
{
	if ( (pResult == NULL) || (iValue > (uint64)SIZE_MAX) ) {
		xrtSetErrorFormat(XERR_RANGE, "xlang.extlib.xregex", 0,
			"%s exceeds the native size range", sName);
		return false;
	}
	*pResult = (size_t)iValue;
	return true;
}



/* 构造并校验统一的正则编译配置。 */
static bool xregex_config(
	xregexconfig* pConfig,
	uint32 iFlags,
	uint64 iMaxPatternBytes,
	uint64 iMaxCaptures
)
{
	size_t iPatternLimit;
	size_t iCaptureLimit;

	if ( (pConfig == NULL) ||
		 !xregex_size(iMaxPatternBytes, &iPatternLimit, "maxPatternBytes") ||
		 !xregex_size(iMaxCaptures, &iCaptureLimit, "maxCaptures") ||
		 (iPatternLimit == 0u) || (iCaptureLimit == 0u) ) {
		if ( xrtGetError() == NULL ) {
			xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.xregex", 0,
				"regular expression limits must be greater than zero");
		}
		return false;
	}
	xrtRegexConfigInit(pConfig);
	pConfig->Flags = iFlags;
	pConfig->MaxPatternBytes = iPatternLimit;
	pConfig->MaxCaptures = iCaptureLimit;
	return true;
}



/* 把普通文本转义为可直接嵌入表达式的字面量。 */
XREGEX_EXPORT void* xregex_escape_native(const char* sText, size_t iTextSize)
{
	xstrview Text = { sText, iTextSize };
	size_t iOutputSize;
	str sOutput;

	xrtClearError();
	sOutput = xrtRegexEscape(Text, &iOutputSize);
	return xregex_take_string(sOutput, iOutputSize);
}



/* 复制输入字符串中的明确字节范围。 */
XREGEX_EXPORT void* xregex_text_range_native(
	const char* sText,
	size_t iTextSize,
	uint64 iStart,
	uint64 iCount
)
{
	xbytesview Data;
	size_t iNativeStart;
	size_t iNativeCount;

	if ( !xregex_size(iStart, &iNativeStart, "start") ||
		 !xregex_size(iCount, &iNativeCount, "count") ||
		 (iNativeStart > iTextSize) ||
		 (iNativeCount > (iTextSize - iNativeStart)) ) {
		if ( xrtGetError() == NULL ) {
			xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.xregex", 0,
				"text byte range is out of bounds");
		}
		return NULL;
	}
	Data.Data = sText != NULL ?
		(const unsigned char*)sText + iNativeStart : NULL;
	Data.Size = iNativeCount;
	return xrtBufferFrom(Data);
}



/* 编译一个明确长度的 UTF-8 表达式。 */
XREGEX_EXPORT void* xregex_compile_native(
	const char* sPattern,
	size_t iPatternSize,
	uint32 iFlags,
	uint64 iMaxPatternBytes,
	uint64 iMaxCaptures
)
{
	xregexconfig Config;
	xstrview Pattern = { sPattern, iPatternSize };

	xrtClearError();
	if ( !xregex_config(&Config, iFlags, iMaxPatternBytes, iMaxCaptures) ) {
		return NULL;
	}
	return xrtRegexCompileConfig(Pattern, &Config);
}



/* 接管一个已经持有引用的正则句柄。 */
XREGEX_EXPORT void* xregex_adopt(void* pHandle)
{
	return pHandle;
}



/* 释放正则句柄引用。 */
XREGEX_EXPORT void xregex_release_native(void* pHandle)
{
	xrtRegexRelease((xregex*)pHandle);
}



/* 验证表达式；验证失败不污染之后的独立操作。 */
XREGEX_EXPORT bool xregex_valid_native(const char* sPattern, size_t iPatternSize)
{
	xstrview Pattern = { sPattern, iPatternSize };
	bool bValid;

	xrtClearError();
	bValid = xrtRegexValid(Pattern);
	if ( !bValid ) { xrtClearError(); }
	return bValid;
}



/* 返回原始表达式的独立字节副本。 */
XREGEX_EXPORT void* xregex_pattern_native(void* pHandle)
{
	return xregex_copy_view(xrtRegexPattern((const xregex*)pHandle));
}



/* 返回编译标志。 */
XREGEX_EXPORT uint32 xregex_flags_native(void* pHandle)
{
	return xrtRegexFlags((const xregex*)pHandle);
}



/* 返回包含组 0 的捕获数量。 */
XREGEX_EXPORT int64 xregex_capture_count_native(void* pHandle)
{
	size_t iCount = xrtRegexCaptureCount((const xregex*)pHandle);

	if ( iCount > (size_t)INT64_MAX ) {
		xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.xregex", 0,
			"capture count exceeds the XLang integer range");
		return -1;
	}
	return (int64)iCount;
}



/* 返回捕获名称；未命名捕获返回空 Buffer。 */
XREGEX_EXPORT void* xregex_capture_name_native(void* pHandle, int64 iIndex)
{
	xstrview Name;

	if ( (iIndex < 0) ||
		 !xrtRegexCaptureName((const xregex*)pHandle, (size_t)iIndex, &Name) ) {
		return NULL;
	}
	return xregex_copy_view(Name);
}



/* 按名称查找捕获索引，未找到时返回 -1。 */
XREGEX_EXPORT int64 xregex_capture_index_native(
	void* pHandle,
	const char* sName,
	size_t iNameSize
)
{
	xstrview Name = { sName, iNameSize };
	size_t iIndex = xrtRegexCaptureIndex((const xregex*)pHandle, Name);

	if ( iIndex == XRT_NPOS ) { return -1; }
	if ( iIndex > (size_t)INT64_MAX ) {
		xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.xregex", 0,
			"capture index exceeds the XLang integer range");
		return -2;
	}
	return (int64)iIndex;
}



/* 公开底层不可变正则句柄，所有权仍属于当前对象。 */
XREGEX_EXPORT void* xregex_native_handle(void* pHandle)
{
	return pHandle;
}



/* 创建独占执行缓存的 matcher。 */
XREGEX_EXPORT void* xregex_matcher_create_native(void* pRegex)
{
	xrtClearError();
	return xrtRegexMatcherCreate((xregex*)pRegex);
}



/* 释放 matcher。 */
XREGEX_EXPORT void xregex_matcher_destroy_native(void* pHandle)
{
	xrtRegexMatcherFree((xregexmatcher*)pHandle);
}



/* 从字节位置开始搜索首个匹配。 */
XREGEX_EXPORT int xregex_matcher_find_native(
	void* pHandle,
	const char* sText,
	size_t iTextSize,
	uint64 iStart
)
{
	size_t iOffset;
	xstrview Text = { sText, iTextSize };

	xrtClearError();
	if ( !xregex_size(iStart, &iOffset, "start") ) { return XREGEX_ERROR; }
	return (int)xrtRegexMatcherFind((xregexmatcher*)pHandle, Text, iOffset);
}



/* 要求匹配从指定字节位置开始。 */
XREGEX_EXPORT int xregex_matcher_at_native(
	void* pHandle,
	const char* sText,
	size_t iTextSize,
	uint64 iStart
)
{
	size_t iOffset;
	xstrview Text = { sText, iTextSize };

	xrtClearError();
	if ( !xregex_size(iStart, &iOffset, "start") ) { return XREGEX_ERROR; }
	return (int)xrtRegexMatcherAt((xregexmatcher*)pHandle, Text, iOffset);
}



/* 要求表达式覆盖完整输入。 */
XREGEX_EXPORT int xregex_matcher_full_native(
	void* pHandle,
	const char* sText,
	size_t iTextSize
)
{
	xstrview Text = { sText, iTextSize };

	xrtClearError();
	return (int)xrtRegexMatcherFull((xregexmatcher*)pHandle, Text);
}



/* 继续搜索下一个匹配。 */
XREGEX_EXPORT int xregex_matcher_next_native(void* pHandle)
{
	xrtClearError();
	return (int)xrtRegexMatcherNext((xregexmatcher*)pHandle);
}



/* 返回 matcher 当前是否持有成功匹配。 */
XREGEX_EXPORT bool xregex_matcher_matched_native(void* pHandle)
{
	return xrtRegexMatcherMatched((const xregexmatcher*)pHandle);
}



/* 读取一次捕获并保留未参与匹配状态。 */
static bool xregex_matcher_capture(
	void* pHandle,
	int64 iIndex,
	xregexcapture* pCapture
)
{
	if ( (iIndex < 0) || (pCapture == NULL) ) {
		xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.xregex", 0,
			"capture index is out of range");
		return false;
	}
	return xrtRegexMatcherCapture(
		(const xregexmatcher*)pHandle,
		(size_t)iIndex,
		pCapture
	);
}



/* 返回捕获是否参与了当前匹配。 */
XREGEX_EXPORT int xregex_matcher_capture_matched_native(void* pHandle, int64 iIndex)
{
	xregexcapture Capture;

	return xregex_matcher_capture(pHandle, iIndex, &Capture) ?
		(Capture.Matched ? 1 : 0) : -1;
}



/* 返回捕获的起始字节位置。 */
XREGEX_EXPORT int64 xregex_matcher_capture_begin_native(void* pHandle, int64 iIndex)
{
	xregexcapture Capture;

	if ( !xregex_matcher_capture(pHandle, iIndex, &Capture) ||
		 (Capture.Span.Begin > (size_t)INT64_MAX) ) {
		return -1;
	}
	return (int64)Capture.Span.Begin;
}



/* 返回捕获的结束字节位置。 */
XREGEX_EXPORT int64 xregex_matcher_capture_end_native(void* pHandle, int64 iIndex)
{
	xregexcapture Capture;

	if ( !xregex_matcher_capture(pHandle, iIndex, &Capture) ||
		 (Capture.Span.End > (size_t)INT64_MAX) ) {
		return -1;
	}
	return (int64)Capture.Span.End;
}



/* 返回捕获文本的独立 Buffer。 */
XREGEX_EXPORT void* xregex_matcher_capture_text_native(void* pHandle, int64 iIndex)
{
	xregexcapture Capture;

	if ( !xregex_matcher_capture(pHandle, iIndex, &Capture) ) { return NULL; }
	return xregex_copy_view(Capture.Text);
}



/* 按模板执行有限或无限次替换并返回独立 Buffer。 */
XREGEX_EXPORT void* xregex_replace_native(
	void* pHandle,
	const char* sText,
	size_t iTextSize,
	const char* sReplacement,
	size_t iReplacementSize,
	int64 iLimit
)
{
	xregex* pRegex = (xregex*)pHandle;
	xstrview Text = { sText, iTextSize };
	xstrview Replacement = { sReplacement, iReplacementSize };
	xstrbuf Output;
	str sOutput;
	size_t iOutputSize;
	size_t iNativeLimit;

	xrtClearError();
	if ( iLimit < 0 ) {
		iNativeLimit = SIZE_MAX;
	} else if ( !xregex_size((uint64)iLimit, &iNativeLimit, "limit") ) {
		return NULL;
	}
	xrtStrBufInit(&Output);
	if ( !xrtRegexReplaceTo(
		pRegex,
		Text,
		Replacement,
		iNativeLimit,
		&Output,
		NULL
	) ) {
		xrtStrBufFree(&Output);
		return NULL;
	}
	iOutputSize = Output.Size;
	sOutput = xrtStrBufTake(&Output);
	xrtStrBufFree(&Output);
	return xregex_take_string(sOutput, iOutputSize);
}



/* 创建持有输入副本的流式拆分器，避免跨越 XLang 参数生命周期。 */
XREGEX_EXPORT void* xregex_splitter_create_native(
	void* pRegex,
	const char* sText,
	size_t iTextSize,
	int64 iLimit,
	uint32 iFlags
)
{
	xregex_native_splitter* pNative;
	xregexsplitconfig Config;
	xstrview Text;

	xrtClearError();
	if ( ((sText == NULL) && (iTextSize != 0u)) || (iTextSize == SIZE_MAX) ) {
		xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.xregex", 0,
			"splitter text is invalid or too large");
		return NULL;
	}
	pNative = (xregex_native_splitter*)xrtCalloc(1u, sizeof(*pNative));
	if ( pNative == NULL ) { return NULL; }
	pNative->Text = (str)xrtMalloc(iTextSize + 1u);
	if ( pNative->Text == NULL ) {
		xrtFree(pNative);
		return NULL;
	}
	if ( iTextSize != 0u ) { memcpy(pNative->Text, sText, iTextSize); }
	pNative->Text[iTextSize] = 0;
	Text.Data = pNative->Text;
	Text.Size = iTextSize;
	xrtRegexSplitConfigInit(&Config);
	Config.Flags = iFlags;
	if ( iLimit < 0 ) {
		Config.Limit = SIZE_MAX;
	} else if ( !xregex_size((uint64)iLimit, &Config.Limit, "limit") ) {
		xrtFree(pNative->Text);
		xrtFree(pNative);
		return NULL;
	}
	pNative->Splitter = xrtRegexSplitterCreate((xregex*)pRegex, Text, &Config);
	if ( pNative->Splitter == NULL ) {
		xrtFree(pNative->Text);
		xrtFree(pNative);
		return NULL;
	}
	return pNative;
}



/* 释放流式拆分器。 */
XREGEX_EXPORT void xregex_splitter_destroy_native(void* pHandle)
{
	xregex_native_splitter* pNative = (xregex_native_splitter*)pHandle;

	if ( pNative == NULL ) { return; }
	xrtRegexSplitterFree(pNative->Splitter);
	xrtFree(pNative->Text);
	xrtFree(pNative);
}



/* 推进到下一个字段或捕获。 */
XREGEX_EXPORT int xregex_splitter_next_native(void* pHandle)
{
	xregex_native_splitter* pNative = (xregex_native_splitter*)pHandle;

	if ( pNative == NULL ) {
		xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.xregex", 0,
			"splitter handle is null");
		return XREGEX_ERROR;
	}
	memset(&pNative->Current, 0, sizeof(pNative->Current));
	return (int)xrtRegexSplitterNext(pNative->Splitter, &pNative->Current);
}



/* 返回当前拆分项文本的独立 Buffer。 */
XREGEX_EXPORT void* xregex_splitter_text_native(void* pHandle)
{
	xregex_native_splitter* pNative = (xregex_native_splitter*)pHandle;

	if ( pNative == NULL ) { return NULL; }
	return xregex_copy_view(pNative->Current.Text);
}



/* 返回当前项对应的捕获索引，普通字段返回 -1。 */
XREGEX_EXPORT int64 xregex_splitter_capture_native(void* pHandle)
{
	xregex_native_splitter* pNative = (xregex_native_splitter*)pHandle;

	if ( (pNative == NULL) || (pNative->Current.Capture == XRT_NPOS) ) {
		return -1;
	}
	if ( pNative->Current.Capture > (size_t)INT64_MAX ) { return -2; }
	return (int64)pNative->Current.Capture;
}



/* 返回当前捕获是否参与匹配；普通字段始终为真。 */
XREGEX_EXPORT bool xregex_splitter_matched_native(void* pHandle)
{
	xregex_native_splitter* pNative = (xregex_native_splitter*)pHandle;

	return (pNative != NULL) && pNative->Current.Matched;
}



/* 把 XLang 字符串数组转换为临时明确长度视图。 */
static bool xregex_pattern_views(
	xtarray pPatterns,
	xstrview** pViews,
	size_t* pCount
)
{
	const char* const* arrText;
	const xtypedarray* pNativeArray;
	xstrview* arrView = NULL;
	size_t iCount;
	size_t i;

	if ( (pViews == NULL) || (pCount == NULL) ||
		 (pPatterns == NULL) ||
		 (pPatterns->Kind != XL6_TYPED_CONTAINER_ARRAY) ||
		 (pPatterns->Value == NULL) ) {
		xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.xregex", 0,
			"RegexSet patterns must be an array of strings");
		return false;
	}
	*pViews = NULL;
	*pCount = 0u;
	pNativeArray = (const xtypedarray*)pPatterns->Value;
	iCount = xrtTypedArrayCount(pNativeArray);
	if ( iCount > (SIZE_MAX / sizeof(*arrView)) ) {
		xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.xregex", 0,
			"RegexSet pattern count exceeds the native size range");
		return false;
	}
	if ( iCount != 0u ) {
		arrView = (xstrview*)xrtMalloc(iCount * sizeof(*arrView));
		if ( arrView == NULL ) { return false; }
	}
	arrText = (const char* const*)xrtTypedArrayConstData(pNativeArray);
	if ( (iCount != 0u) && (arrText == NULL) ) {
		xrtSetErrorInfo(XERR_STATE, "xlang.extlib.xregex", 0,
			"RegexSet pattern storage is unavailable");
		xrtFree(arrView);
		return false;
	}
	for ( i = 0; i < iCount; i += 1 ) {
		if ( arrText[i] == NULL ) {
			xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.xregex", 0,
				"RegexSet patterns must not contain null strings");
			xrtFree(arrView);
			return false;
		}
		arrView[i].Data = arrText[i];
		arrView[i].Size = strlen(arrText[i]);
	}
	*pViews = arrView;
	*pCount = iCount;
	return true;
}



/* 批量编译字符串模式并创建不可变集合。 */
XREGEX_EXPORT void* xregex_set_compile_native(
	xtarray pPatterns,
	uint32 iFlags,
	uint64 iMaxPatternBytes,
	uint64 iMaxCaptures
)
{
	xregexconfig Config;
	xstrview* arrPattern;
	size_t iCount;
	xregexset* pSet;

	xrtClearError();
	if ( !xregex_config(&Config, iFlags, iMaxPatternBytes, iMaxCaptures) ||
		 !xregex_pattern_views(pPatterns, &arrPattern, &iCount) ) {
		return NULL;
	}
	pSet = xrtRegexSetCompileConfig(arrPattern, iCount, &Config);
	xrtFree(arrPattern);
	return pSet;
}



/* 接管一个已经持有引用的集合句柄。 */
XREGEX_EXPORT void* xregex_set_adopt(void* pHandle)
{
	return pHandle;
}



/* 释放集合句柄引用。 */
XREGEX_EXPORT void xregex_set_release_native(void* pHandle)
{
	xrtRegexSetRelease((xregexset*)pHandle);
}



/* 返回集合中的模式数量。 */
XREGEX_EXPORT int64 xregex_set_count_native(void* pHandle)
{
	size_t iCount = xrtRegexSetCount((const xregexset*)pHandle);

	return iCount <= (size_t)INT64_MAX ? (int64)iCount : -1;
}



/* 返回集合中指定正则的新引用。 */
XREGEX_EXPORT void* xregex_set_regex_native(void* pHandle, int64 iIndex)
{
	const xregex* pRegex;

	if ( iIndex < 0 ) { return NULL; }
	pRegex = xrtRegexSetRegex((const xregexset*)pHandle, (size_t)iIndex);
	return pRegex != NULL ? xrtRegexRef((xregex*)pRegex) : NULL;
}



/* 公开底层不可变集合句柄，所有权仍属于当前对象。 */
XREGEX_EXPORT void* xregex_set_native_handle(void* pHandle)
{
	return pHandle;
}



/* 创建集合的可复用 matcher。 */
XREGEX_EXPORT void* xregex_set_matcher_create_native(void* pSet)
{
	xrtClearError();
	return xrtRegexSetMatcherCreate((xregexset*)pSet);
}



/* 释放集合 matcher。 */
XREGEX_EXPORT void xregex_set_matcher_destroy_native(void* pHandle)
{
	xrtRegexSetMatcherFree((xregexsetmatcher*)pHandle);
}



/* 从指定字节位置计算全部命中模式。 */
XREGEX_EXPORT int xregex_set_matcher_match_native(
	void* pHandle,
	const char* sText,
	size_t iTextSize,
	uint64 iStart
)
{
	size_t iOffset;
	xstrview Text = { sText, iTextSize };

	xrtClearError();
	if ( !xregex_size(iStart, &iOffset, "start") ) { return XREGEX_ERROR; }
	return (int)xrtRegexSetMatcherMatch(
		(xregexsetmatcher*)pHandle,
		Text,
		iOffset
	);
}



/* 返回本轮命中数量。 */
XREGEX_EXPORT int64 xregex_set_matcher_count_native(void* pHandle)
{
	size_t iCount = xrtRegexSetMatcherCount((const xregexsetmatcher*)pHandle);

	return iCount <= (size_t)INT64_MAX ? (int64)iCount : -1;
}



/* 返回本轮第几个命中的模式索引。 */
XREGEX_EXPORT int64 xregex_set_matcher_index_native(void* pHandle, int64 iIndex)
{
	size_t iPattern;

	if ( iIndex < 0 ) { return -1; }
	iPattern = xrtRegexSetMatcherIndex(
		(const xregexsetmatcher*)pHandle,
		(size_t)iIndex
	);
	return (iPattern != XRT_NPOS) && (iPattern <= (size_t)INT64_MAX) ?
		(int64)iPattern : -1;
}



/* 判断指定模式是否在本轮命中。 */
XREGEX_EXPORT bool xregex_set_matcher_matched_native(void* pHandle, int64 iPattern)
{
	return (iPattern >= 0) && xrtRegexSetMatcherMatched(
		(const xregexsetmatcher*)pHandle,
		(size_t)iPattern
	);
}



/* 返回最小命中模式索引，未命中时返回 -1。 */
XREGEX_EXPORT int64 xregex_set_matcher_first_native(void* pHandle)
{
	size_t iPattern = xrtRegexSetMatcherFirst((const xregexsetmatcher*)pHandle);

	return (iPattern != XRT_NPOS) && (iPattern <= (size_t)INT64_MAX) ?
		(int64)iPattern : -1;
}
