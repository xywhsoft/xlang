#ifndef XLANG_EXTLIB_XREGEX_H
#define XLANG_EXTLIB_XREGEX_H

#define XREGEX_FEATURE_REGEX_CORE
#define XREGEX_FEATURE_REGEX_MATCH
#define XREGEX_FEATURE_REGEX_REPLACE
#define XREGEX_FEATURE_REGEX_SPLIT
#define XREGEX_FEATURE_REGEX_SET
#define XREGEX_FEATURE_REGEX

/* 公共 C 接口也选择自身依赖，不能只依赖 xlang 包清单传入的编译宏。 */
#ifndef XRT_MODULE_ERROR_FORMAT
#define XRT_MODULE_ERROR_FORMAT
#endif
#ifndef XRT_MODULE_STRING_SPLIT
#define XRT_MODULE_STRING_SPLIT
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif

#include "xrt.h"



#if defined(XREGEX_FEATURE_REGEX_MATCH) && !defined(XREGEX_FEATURE_REGEX_CORE)
	#error "XREGEX_FEATURE_REGEX_MATCH requires XREGEX_FEATURE_REGEX_CORE"
#endif

#if defined(XREGEX_FEATURE_REGEX_MATCH) && !defined(XRT_FEATURE_UNICODE)
	#error "XREGEX_FEATURE_REGEX_MATCH requires XRT_FEATURE_UNICODE"
#endif

#if defined(XREGEX_FEATURE_REGEX_SET) && !defined(XREGEX_FEATURE_REGEX_CORE)
	#error "XREGEX_FEATURE_REGEX_SET requires XREGEX_FEATURE_REGEX_CORE"
#endif

#if defined(XREGEX_FEATURE_REGEX_REPLACE) && !defined(XREGEX_FEATURE_REGEX_MATCH)
	#error "XREGEX_FEATURE_REGEX_REPLACE requires XREGEX_FEATURE_REGEX_MATCH"
#endif

#if defined(XREGEX_FEATURE_REGEX_REPLACE) && !defined(XRT_FEATURE_STRING)
	#error "XREGEX_FEATURE_REGEX_REPLACE requires XRT_FEATURE_STRING"
#endif

#if defined(XREGEX_FEATURE_REGEX_SPLIT) && !defined(XREGEX_FEATURE_REGEX_MATCH)
	#error "XREGEX_FEATURE_REGEX_SPLIT requires XREGEX_FEATURE_REGEX_MATCH"
#endif

#if defined(XREGEX_FEATURE_REGEX_SPLIT) && !defined(XRT_FEATURE_STRING_SPLIT)
	#error "XREGEX_FEATURE_REGEX_SPLIT requires XRT_FEATURE_STRING_SPLIT"
#endif

#if defined(XREGEX_FEATURE_REGEX) && \
	(!defined(XREGEX_FEATURE_REGEX_REPLACE) || \
	 !defined(XREGEX_FEATURE_REGEX_SPLIT) || \
	 !defined(XREGEX_FEATURE_REGEX_SET))
	#error "XREGEX_FEATURE_REGEX requires replace, split and set features"
#endif



/* xlang 包清单明确选择完整正则闭包，因此声明 SDK 模式复用宿主/产物的
 * 唯一执行内核。独立 C 模式仍由所选 XRT feature 或包自身提供实现。 */
#if defined(XRT_DECLARATIONS) && defined(XRT_REGEX_H)
#define XLANG_XREGEX_DECLARED_BY_SDK
#endif

#if defined(XREGEX_FEATURE_REGEX_CORE) && !defined(XRT_FEATURE_REGEX_CORE) && !defined(XLANG_XREGEX_DECLARED_BY_SDK)

/* 默认限制面向不可信表达式，防止编译阶段无界消耗资源。 */
#define XREGEX_PATTERN_DEFAULT (1024u * 1024u)
#define XREGEX_CAPTURES_DEFAULT 4096u



/* 编译标志与表达式内的 (?i)、(?m)、(?s)、(?U) 语义一致。 */
typedef enum xregexflag {
	XREGEX_IGNORE_CASE = UINT32_C(0x00000001),
	XREGEX_MULTILINE = UINT32_C(0x00000002),
	XREGEX_DOT_ALL = UINT32_C(0x00000004),
	XREGEX_UNGREEDY = UINT32_C(0x00000008)
} xregexflag;



/* 所有匹配入口使用同一三态结果，未匹配不属于错误。 */
typedef enum xregexresult {
	XREGEX_ERROR = -1,
	XREGEX_NONE = 0,
	XREGEX_MATCH = 1
} xregexresult;



/* 正则模块错误代码在 xrt.regex 域内保持稳定。 */
typedef enum xregexerror {
	XREGEX_ERROR_CONFIG = 1501,
	XREGEX_ERROR_PATTERN,
	XREGEX_ERROR_LIMIT,
	XREGEX_ERROR_EXECUTE,
	XREGEX_ERROR_REPLACEMENT,
	XREGEX_ERROR_CALLBACK
} xregexerror;



/* 编译配置同时控制语义标志与可由调用方收紧的资源预算。 */
typedef struct xregexconfig {
	uint32 Flags;
	size_t MaxPatternBytes;
	size_t MaxCaptures;
	uint32 Reserved[4];
} xregexconfig;



/* 匹配范围使用零基半开字节区间 [Begin, End)。 */
typedef struct xregexspan {
	size_t Begin;
	size_t End;
} xregexspan;



/* 编译对象不可变、可跨线程共享并通过引用计数管理。 */
typedef struct xregex xregex;



XRT_EXTERN_C_BEGIN



/* 初始化默认编译标志和有限资源预算。 */
XRT_API void xrtRegexConfigInit(xregexconfig* pConfig);



/* 返回字面量文本转义为正则表达式后的精确字节数，不包含末尾零。 */
XRT_API bool xrtRegexEscapeSize(xstrview Text, size_t* pOutputSize);



/* 将字面量文本转义到调用方缓冲区；容量必须包含末尾零。 */
XRT_API bool xrtRegexEscapeWrite(
	xstrview Text,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的零结尾正则字面量，长度输出可以为空。 */
XRT_API str xrtRegexEscape(xstrview Text, size_t* pOutputSize);



/* 使用默认配置编译明确长度的 UTF-8 正则表达式。 */
XRT_API xregex* xrtRegexCompile(xstrview Pattern);



/* 使用高级配置编译正则表达式。 */
XRT_API xregex* xrtRegexCompileConfig(
	xstrview Pattern,
	const xregexconfig* pConfig
);



/* 验证表达式能否使用默认配置完成编译。 */
XRT_API bool xrtRegexValid(xstrview Pattern);



/* 增加编译对象引用并返回原指针。 */
XRT_API xregex* xrtRegexRef(xregex* pRegex);



/* 释放编译对象引用。 */
XRT_API void xrtRegexRelease(xregex* pRegex);



/* 返回编译对象持有的原始表达式视图。 */
XRT_API xstrview xrtRegexPattern(const xregex* pRegex);



/* 返回编译时使用的标志。 */
XRT_API uint32 xrtRegexFlags(const xregex* pRegex);



/* 返回包含组 0 在内的捕获数量。 */
XRT_API size_t xrtRegexCaptureCount(const xregex* pRegex);



/* 返回指定捕获的借用名称；未命名捕获返回空视图。 */
XRT_API bool xrtRegexCaptureName(
	const xregex* pRegex,
	size_t iIndex,
	xstrview* pName
);



/* 按名称查找捕获索引，未找到时返回 XRT_NPOS。 */
XRT_API size_t xrtRegexCaptureIndex(
	const xregex* pRegex,
	xstrview Name
);



/* 从 xrt.regex 错误的机器数据中读取字节位置。 */
XRT_API bool xrtRegexErrorOffset(
	const xerror* pError,
	size_t* pOffset
);



XRT_EXTERN_C_END

#endif



#if defined(XREGEX_FEATURE_REGEX_MATCH) && !defined(XRT_FEATURE_REGEX_MATCH) && !defined(XLANG_XREGEX_DECLARED_BY_SDK)

/* matcher 独占可变执行缓存，捕获视图在下一次匹配前有效。 */
typedef struct xregexmatcher xregexmatcher;



/* 捕获记录区分未参与匹配与合法的空匹配。 */
typedef struct xregexcapture {
	bool Matched;
	xregexspan Span;
	xstrview Text;
} xregexcapture;



XRT_EXTERN_C_BEGIN



/* 为一个不可变编译对象创建可复用 matcher。 */
XRT_API xregexmatcher* xrtRegexMatcherCreate(xregex* pRegex);



/* 释放 matcher、执行缓存和持有的编译对象引用。 */
XRT_API void xrtRegexMatcherFree(xregexmatcher* pMatcher);



/* 从字节位置开始搜索首个匹配。 */
XRT_API xregexresult xrtRegexMatcherFind(
	xregexmatcher* pMatcher,
	xstrview Text,
	size_t iStart
);



/* 要求首个匹配恰好从指定字节位置开始。 */
XRT_API xregexresult xrtRegexMatcherAt(
	xregexmatcher* pMatcher,
	xstrview Text,
	size_t iStart
);



/* 要求表达式覆盖完整输入。 */
XRT_API xregexresult xrtRegexMatcherFull(
	xregexmatcher* pMatcher,
	xstrview Text
);



/* 继续查找下一项，空匹配会按一个 UTF-8 标量向前推进。 */
XRT_API xregexresult xrtRegexMatcherNext(xregexmatcher* pMatcher);



/* 返回 matcher 当前是否持有一次成功匹配。 */
XRT_API bool xrtRegexMatcherMatched(const xregexmatcher* pMatcher);



/* 返回当前输入的借用视图。 */
XRT_API xstrview xrtRegexMatcherText(const xregexmatcher* pMatcher);



/* 返回指定捕获的参与状态、绝对字节范围和借用文本。 */
XRT_API bool xrtRegexMatcherCapture(
	const xregexmatcher* pMatcher,
	size_t iIndex,
	xregexcapture* pCapture
);



/* 按名称返回当前捕获。 */
XRT_API bool xrtRegexMatcherCaptureNamed(
	const xregexmatcher* pMatcher,
	xstrview Name,
	xregexcapture* pCapture
);



/* 使用临时 matcher 搜索编译表达式。 */
XRT_API xregexresult xrtRegexTest(
	xregex* pRegex,
	xstrview Text
);



/* 使用临时 matcher 检查编译表达式是否覆盖完整输入。 */
XRT_API xregexresult xrtRegexFullTest(
	xregex* pRegex,
	xstrview Text
);



/* 编译默认表达式并执行一次搜索。 */
XRT_API xregexresult xrtRegexMatch(
	xstrview Pattern,
	xstrview Text
);



/* 编译默认表达式并执行一次完整匹配。 */
XRT_API xregexresult xrtRegexFullMatch(
	xstrview Pattern,
	xstrview Text
);



XRT_EXTERN_C_END

#endif



#if defined(XREGEX_FEATURE_REGEX_REPLACE) && !defined(XRT_FEATURE_REGEX_REPLACE) && !defined(XLANG_XREGEX_DECLARED_BY_SDK)

/* 自定义替换器只能向输出尾部追加内容，返回 false 表示终止并报告错误。 */
typedef bool (*xregexreplacefn)(
	const xregexmatcher* pMatcher,
	xstrbuf* pOutput,
	ptr pUserData
);



XRT_EXTERN_C_BEGIN



/* 按模板替换至构建器，SIZE_MAX 表示不限制替换次数。 */
XRT_API bool xrtRegexReplaceTo(
	xregex* pRegex,
	xstrview Text,
	xstrview Replacement,
	size_t iLimit,
	xstrbuf* pOutput,
	size_t* pCount
);



/* 由回调生成每次替换内容，失败时撤销本次调用追加的全部数据。 */
XRT_API bool xrtRegexReplaceFuncTo(
	xregex* pRegex,
	xstrview Text,
	size_t iLimit,
	xregexreplacefn pReplace,
	ptr pUserData,
	xstrbuf* pOutput,
	size_t* pCount
);



/* 替换全部匹配并返回零结尾独立字符串。 */
XRT_API str xrtRegexReplace(
	xregex* pRegex,
	xstrview Text,
	xstrview Replacement
);



/* 只替换第一个匹配并返回零结尾独立字符串。 */
XRT_API str xrtRegexReplaceFirst(
	xregex* pRegex,
	xstrview Text,
	xstrview Replacement
);



XRT_EXTERN_C_END

#endif



#if defined(XREGEX_FEATURE_REGEX_SPLIT) && !defined(XRT_FEATURE_REGEX_SPLIT) && !defined(XLANG_XREGEX_DECLARED_BY_SDK)

/* 拆分标志控制捕获输出和空项过滤。 */
typedef enum xregexsplitflag {
	XREGEX_SPLIT_CAPTURES = UINT32_C(0x00000001),
	XREGEX_SPLIT_SKIP_EMPTY = UINT32_C(0x00000002)
} xregexsplitflag;



/* Limit 是最多使用的分隔匹配数，SIZE_MAX 表示不限制。 */
typedef struct xregexsplitconfig {
	size_t Limit;
	uint32 Flags;
	uint32 Reserved[4];
} xregexsplitconfig;



/* 流式拆分器借用输入，并独占一个可重用 matcher。 */
typedef struct xregexsplitter xregexsplitter;



/* Capture 为 XRT_NPOS 时是普通字段，否则是捕获索引。 */
typedef struct xregexsplitpart {
	xstrview Text;
	size_t Capture;
	bool Matched;
} xregexsplitpart;



XRT_EXTERN_C_BEGIN



/* 初始化不限制分隔次数且保留空字段的拆分配置。 */
XRT_API void xrtRegexSplitConfigInit(xregexsplitconfig* pConfig);



/* 创建借用输入的流式正则拆分器。 */
XRT_API xregexsplitter* xrtRegexSplitterCreate(
	xregex* pRegex,
	xstrview Text,
	const xregexsplitconfig* pConfig
);



/* 释放拆分器、matcher 和持有的正则引用。 */
XRT_API void xrtRegexSplitterFree(xregexsplitter* pSplitter);



/* 返回下一字段或捕获，XREGEX_NONE 表示遍历结束。 */
XRT_API xregexresult xrtRegexSplitterNext(
	xregexsplitter* pSplitter,
	xregexsplitpart* pPart
);



/* 使用默认配置拆分并返回一个分配块内的零结尾字段。 */
XRT_API xstrlist* xrtRegexSplit(
	xregex* pRegex,
	xstrview Text
);



XRT_EXTERN_C_END

#endif



#if defined(XREGEX_FEATURE_REGEX_SET) && !defined(XRT_FEATURE_REGEX_SET) && !defined(XLANG_XREGEX_DECLARED_BY_SDK)

/* 编译集合不可变并持有各模式引用。 */
typedef struct xregexset xregexset;



/* 集合 matcher 独占执行缓存和本轮命中索引。 */
typedef struct xregexsetmatcher xregexsetmatcher;



XRT_EXTERN_C_BEGIN



/* 从已有编译对象创建集合，各模式可以使用不同标志。 */
XRT_API xregexset* xrtRegexSetCreate(
	xregex* const* arrRegex,
	size_t iCount
);



/* 使用默认配置批量编译模式并创建集合。 */
XRT_API xregexset* xrtRegexSetCompile(
	const xstrview* arrPattern,
	size_t iCount
);



/* 使用同一高级配置批量编译模式并创建集合。 */
XRT_API xregexset* xrtRegexSetCompileConfig(
	const xstrview* arrPattern,
	size_t iCount,
	const xregexconfig* pConfig
);



/* 增加集合引用并返回原指针。 */
XRT_API xregexset* xrtRegexSetRef(xregexset* pSet);



/* 释放集合引用。 */
XRT_API void xrtRegexSetRelease(xregexset* pSet);



/* 返回集合中的模式数量。 */
XRT_API size_t xrtRegexSetCount(const xregexset* pSet);



/* 返回集合借用的指定编译对象。 */
XRT_API const xregex* xrtRegexSetRegex(
	const xregexset* pSet,
	size_t iIndex
);



/* 从批量编译错误中读取失败的模式索引。 */
XRT_API bool xrtRegexSetErrorIndex(
	const xerror* pError,
	size_t* pIndex
);



/* 为不可变集合创建可复用 matcher。 */
XRT_API xregexsetmatcher* xrtRegexSetMatcherCreate(xregexset* pSet);



/* 释放集合 matcher 及其命中索引。 */
XRT_API void xrtRegexSetMatcherFree(xregexsetmatcher* pMatcher);



/* 从指定字节位置开始计算所有命中的模式。 */
XRT_API xregexresult xrtRegexSetMatcherMatch(
	xregexsetmatcher* pMatcher,
	xstrview Text,
	size_t iStart
);



/* 返回本轮命中的模式数量。 */
XRT_API size_t xrtRegexSetMatcherCount(const xregexsetmatcher* pMatcher);



/* 返回本轮第 iIndex 个命中的模式索引。 */
XRT_API size_t xrtRegexSetMatcherIndex(
	const xregexsetmatcher* pMatcher,
	size_t iIndex
);



/* 判断指定模式是否在本轮命中。 */
XRT_API bool xrtRegexSetMatcherMatched(
	const xregexsetmatcher* pMatcher,
	size_t iPattern
);



/* 返回本轮最小的命中模式索引，未命中时返回 XRT_NPOS。 */
XRT_API size_t xrtRegexSetMatcherFirst(const xregexsetmatcher* pMatcher);



/* 使用临时 matcher 检查集合中是否有模式命中。 */
XRT_API xregexresult xrtRegexSetTest(
	xregexset* pSet,
	xstrview Text
);



XRT_EXTERN_C_END

#endif

#endif
