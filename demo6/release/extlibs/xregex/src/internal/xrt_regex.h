#ifndef XRT_INTERNAL_REGEX_H
#define XRT_INTERNAL_REGEX_H

#include "../../include/xregex.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "../third_party/bbre/bbre.h"
#include "../third_party/bbre/bbre_xrt.h"



#if defined(XREGEX_FEATURE_REGEX_CORE)

/* 基础错误只依赖 XRT 公开错误契约，并统一归入正则错误域。 */
#define __xrtRegexSetInvalidArgument() \
	xrtSetErrorInfo(XERR_ARGUMENT, "xrt.regex", 0, "invalid argument")
#define __xrtRegexSetOutOfMemory() \
	xrtSetErrorInfo(XERR_MEMORY, "xrt.regex", 0, "out of memory")
#define __xrtRegexSetSizeOverflow() \
	xrtSetErrorInfo(XERR_RANGE, "xrt.regex", 0, "size overflow")
#define __xrtRegexSetRange() \
	xrtSetErrorInfo(XERR_RANGE, "xrt.regex", 0, "value out of range")
#define __xrtRegexSetInvalidState() \
	xrtSetErrorInfo(XERR_STATE, "xrt.regex", 0, "invalid state")
#define __xrtRegexSetInternal() \
	xrtSetErrorInfo(XERR_INTERNAL, "xrt.regex", 0, "internal error")

/* 编译对象只保存不可变引擎程序、原始模式和捕获元数据。 */
struct xregex {
	volatile int32 RefCount;
	bbre* Engine;
	str Pattern;
	size_t PatternSize;
	size_t CaptureCount;
	uint32 Flags;
};



/* 把 XRT 分配器适配为 BBRE 的 realloc 风格回调。 */
void* __xrtRegexAlloc(
	void* pUser,
	void* pMemory,
	size_t iOldSize,
	size_t iNewSize
);



/* 检查字符串视图的指针和大小组合。 */
bool __xrtRegexViewValid(xstrview Text);



/* 检查高级编译配置是否完整且保留字段均为零。 */
bool __xrtRegexConfigValid(const xregexconfig* pConfig);



/* 设置带稳定域、代码和可选字节位置的正则错误。 */
void __xrtRegexError(
	xerrkind Kind,
	xregexerror Code,
	cstr sOperation,
	cstr sMessage,
	bool bHasOffset,
	size_t iOffset
);



/* 将 BBRE 执行错误转换为 XRT 三态结果。 */
xregexresult __xrtRegexResult(int iResult, cstr sOperation);

#endif



#if defined(XREGEX_FEATURE_REGEX_MATCH)

/* matcher 保存独占执行缓存和只借用到下一次调用的输入。 */
struct xregexmatcher {
	xregex* Regex;
	bbre_xrt_context* Context;
	bbre_span* Captures;
	unsigned int* CaptureMatched;
	xstrview Text;
	bool HasText;
	bool HasMatch;
	bool CanNext;
};



/* 执行一次搜索并刷新 matcher 的借用结果。 */
xregexresult __xrtRegexMatcherFind(
	xregexmatcher* pMatcher,
	xstrview Text,
	size_t iStart,
	bool bCanNext
);



/* 返回从当前位置开始应跳过的一个 UTF-8 标量字节数。 */
size_t __xrtRegexAdvance(xstrview Text, size_t iPosition);

#endif



#if defined(XREGEX_FEATURE_REGEX_SET)

/* 集合持有不可变模式引用和 BBRE 合并程序。 */
struct xregexset {
	volatile int32 RefCount;
	bbre_set* Engine;
	xregex** Regex;
	size_t Count;
};



/* 集合 matcher 保存独占执行缓存和升序命中索引。 */
struct xregexsetmatcher {
	xregexset* Set;
	bbre_xrt_context* Context;
	unsigned int* Indices;
	size_t MatchCount;
	bool HasResult;
};

#endif

#endif
