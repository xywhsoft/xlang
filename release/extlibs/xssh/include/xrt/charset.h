#ifndef XRT_CHARSET_H
#define XRT_CHARSET_H

#include <xrt/core.h>



#if (defined(XRT_FEATURE_CHARSET) || defined(XRT_FEATURE_CHARSET_DETECT)) && \
	!defined(XRT_FEATURE_UNICODE)
	#error "XRT charset features require XRT_FEATURE_UNICODE"
#endif

#if defined(XRT_FEATURE_CHARSET_DETECT) && !defined(XRT_FEATURE_CHARSET)
	#error "XRT charset detection requires XRT_FEATURE_CHARSET"
#endif

#if defined(XRT_FEATURE_UNICODE_DISTANCE) && !defined(XRT_FEATURE_UNICODE)
	#error "XRT Unicode distance requires XRT_FEATURE_UNICODE"
#endif

#if defined(XRT_FEATURE_UNICODE_TEXT) && \
	(!defined(XRT_FEATURE_UNICODE) || !defined(XRT_FEATURE_STRING))
	#error "XRT Unicode text requires XRT_FEATURE_UNICODE and XRT_FEATURE_STRING"
#endif



#if defined(XRT_FEATURE_UNICODE)

/* UTF-16 视图的 Size 表示 16 位码元数。 */
typedef struct xutf16view {
	const uint16* Data;
	size_t Size;
} xutf16view;



/* UTF-32 视图的 Size 表示 32 位码元数。 */
typedef struct xutf32view {
	const uint32* Data;
	size_t Size;
} xutf32view;



/* 转换遇到错误输入时可以立即失败，也可以写入 U+FFFD 后继续。 */
typedef enum xutfpolicy {
	XUTF_STRICT = 0,
	XUTF_REPLACE
} xutfpolicy;



/* UTF 原语和转换缓冲区共同使用的状态。 */
typedef enum xutfstatus {
	XUTF_OK = 0,
	XUTF_MORE,
	XUTF_INVALID,
	XUTF_NO_SPACE,
	XUTF_OVERFLOW
} xutfstatus;



/* 转换结果明确区分读取量、写入量和首个错误位置。 */
typedef struct xutfresult {
	xutfstatus Status;
	size_t Read;
	size_t Written;
	size_t Error;
} xutfresult;



/* 流式 UTF-8 校验器最多保留一个未完成标量的前缀。 */
typedef struct xutf8state {
	unsigned char Pending[4];
	size_t Total;
	size_t PendingOffset;
	size_t Error;
	uint8 PendingSize;
	bool Failed;
} xutf8state;



/* Unicode 模块的稳定错误代码。 */
typedef enum xutferror {
	XUTF_ERROR_INVALID = 1,
	XUTF_ERROR_OVERFLOW
} xutferror;



XRT_EXTERN_C_BEGIN



/* 从明确码元数创建 UTF-16 借用视图。 */
XRT_API xutf16view xrtUtf16View(const uint16* pText, size_t iSize);



/* 从明确码元数创建 UTF-32 借用视图。 */
XRT_API xutf32view xrtUtf32View(const uint32* pText, size_t iSize);



/* 返回零结尾 UTF-16 字符串的码元数，空指针返回零。 */
XRT_API size_t xrtUtf16Len(const uint16* pText);



/* 返回零结尾 UTF-32 字符串的码元数，空指针返回零。 */
XRT_API size_t xrtUtf32Len(const uint32* pText);



/* 复制零结尾 UTF-16 字符串，返回值由 xrtFree 释放。 */
XRT_API uint16* xrtUtf16Dup(const uint16* pText);



/* 复制 UTF-16 视图并追加零码元，保留视图中的嵌入零。 */
XRT_API uint16* xrtUtf16DupView(xutf16view Text);



/* 复制零结尾 UTF-32 字符串，返回值由 xrtFree 释放。 */
XRT_API uint32* xrtUtf32Dup(const uint32* pText);



/* 复制 UTF-32 视图并追加零码元，保留视图中的嵌入零。 */
XRT_API uint32* xrtUtf32DupView(xutf32view Text);



/* 判断数值是否是可编码的 Unicode 标量值。 */
XRT_API bool xrtUnicodeScalar(uint32 iScalar);



/* 解码一个 UTF-8 标量；输入不足返回 XUTF_MORE。 */
XRT_API xutfstatus xrtUtf8Decode(xstrview Text, uint32* pScalar, size_t* pRead);



/* 解码一个 UTF-16 标量；输入不足返回 XUTF_MORE。 */
XRT_API xutfstatus xrtUtf16Decode(xutf16view Text, uint32* pScalar, size_t* pRead);



/* 把一个 Unicode 标量编码到至少 4 字节的缓冲区，失败返回零。 */
XRT_API size_t xrtUtf8Encode(uint32 iScalar, char arrOutput[4]);



/* 把一个 Unicode 标量编码到至少 2 个码元的缓冲区，失败返回零。 */
XRT_API size_t xrtUtf16Encode(uint32 iScalar, uint16 arrOutput[2]);



/* 严格校验 UTF-8，失败时可返回首个错误字节位置。 */
XRT_API bool xrtUtf8Valid(xstrview Text, size_t* pError);



/* 严格校验 UTF-16，失败时可返回首个错误码元位置。 */
XRT_API bool xrtUtf16Valid(xutf16view Text, size_t* pError);



/* 严格校验 UTF-32，失败时可返回首个错误码元位置。 */
XRT_API bool xrtUtf32Valid(xutf32view Text, size_t* pError);



/* 统计 UTF-8 标量数，输入无效时返回 XRT_NPOS。 */
XRT_API size_t xrtUtf8Count(xstrview Text);



/* 把 UTF-8 标量索引转换为字节偏移，索引越界或输入无效时返回 XRT_NPOS。 */
XRT_API size_t xrtUtf8Offset(xstrview Text, size_t iIndex);



/* 把 UTF-8 标量边界上的字节偏移转换为标量索引。 */
XRT_API size_t xrtUtf8Index(xstrview Text, size_t iOffset);



/* 读取指定 UTF-8 标量索引处的标量值。 */
XRT_API bool xrtUtf8At(xstrview Text, size_t iIndex, uint32* pScalar);



/* 按 UTF-8 标量索引返回借用切片，范围末端会钳制到输入末尾。 */
XRT_API bool xrtUtf8Slice(xstrview Text, size_t iStart, size_t iCount, xstrview* pSlice);



/* 统计 UTF-16 标量数，输入无效时返回 XRT_NPOS。 */
XRT_API size_t xrtUtf16Count(xutf16view Text);



/* 初始化可跨任意分块边界工作的 UTF-8 校验状态。 */
XRT_API void xrtUtf8StateInit(xutf8state* pState);



/* 校验一个流式分块；最后一块必须把 bFinal 设为 true。 */
XRT_API xutfstatus xrtUtf8StateFeed(xutf8state* pState, xstrview Text, bool bFinal);



/* 返回流式校验器记录的绝对错误字节位置。 */
XRT_API size_t xrtUtf8StateError(const xutf8state* pState);



/* 六个缓冲转换函数都拒绝源与目标重叠，目标不足时只发布完整标量。 */
/* UTF-8 转 UTF-16；目标为空时只计算所需码元数。 */
XRT_API xutfresult xrtUtf8To16Buffer(xstrview Source, uint16* pTarget,
	size_t iCapacity, xutfpolicy Policy);



/* UTF-8 转 UTF-32；目标为空时只计算所需码元数。 */
XRT_API xutfresult xrtUtf8To32Buffer(xstrview Source, uint32* pTarget,
	size_t iCapacity, xutfpolicy Policy);



/* UTF-16 转 UTF-8；目标为空时只计算所需字节数。 */
XRT_API xutfresult xrtUtf16To8Buffer(xutf16view Source, char* pTarget,
	size_t iCapacity, xutfpolicy Policy);



/* UTF-16 转 UTF-32；目标为空时只计算所需码元数。 */
XRT_API xutfresult xrtUtf16To32Buffer(xutf16view Source, uint32* pTarget,
	size_t iCapacity, xutfpolicy Policy);



/* UTF-32 转 UTF-8；目标为空时只计算所需字节数。 */
XRT_API xutfresult xrtUtf32To8Buffer(xutf32view Source, char* pTarget,
	size_t iCapacity, xutfpolicy Policy);



/* UTF-32 转 UTF-16；目标为空时只计算所需码元数。 */
XRT_API xutfresult xrtUtf32To16Buffer(xutf32view Source, uint16* pTarget,
	size_t iCapacity, xutfpolicy Policy);



/* 严格转换零结尾 UTF-8，并分配零结尾 UTF-16 字符串。 */
XRT_API uint16* xrtUtf8To16(cstr sText, size_t* pSize);



/* 严格转换零结尾 UTF-8，并分配零结尾 UTF-32 字符串。 */
XRT_API uint32* xrtUtf8To32(cstr sText, size_t* pSize);



/* 严格转换零结尾 UTF-16，并分配零结尾 UTF-8 字符串。 */
XRT_API str xrtUtf16To8(const uint16* pText, size_t* pSize);



/* 严格转换零结尾 UTF-16，并分配零结尾 UTF-32 字符串。 */
XRT_API uint32* xrtUtf16To32(const uint16* pText, size_t* pSize);



/* 严格转换零结尾 UTF-32，并分配零结尾 UTF-8 字符串。 */
XRT_API str xrtUtf32To8(const uint32* pText, size_t* pSize);



/* 严格转换零结尾 UTF-32，并分配零结尾 UTF-16 字符串。 */
XRT_API uint16* xrtUtf32To16(const uint32* pText, size_t* pSize);



/* 转换明确长度 UTF-8，可选择严格失败或替换错误输入。 */
XRT_API uint16* xrtUtf8ViewTo16(xstrview Source, xutfpolicy Policy, size_t* pSize);



/* 转换明确长度 UTF-8，可选择严格失败或替换错误输入。 */
XRT_API uint32* xrtUtf8ViewTo32(xstrview Source, xutfpolicy Policy, size_t* pSize);



/* 转换明确长度 UTF-16，可选择严格失败或替换错误输入。 */
XRT_API str xrtUtf16ViewTo8(xutf16view Source, xutfpolicy Policy, size_t* pSize);



/* 转换明确长度 UTF-16，可选择严格失败或替换错误输入。 */
XRT_API uint32* xrtUtf16ViewTo32(xutf16view Source, xutfpolicy Policy, size_t* pSize);



/* 转换明确长度 UTF-32，可选择严格失败或替换错误输入。 */
XRT_API str xrtUtf32ViewTo8(xutf32view Source, xutfpolicy Policy, size_t* pSize);



/* 转换明确长度 UTF-32，可选择严格失败或替换错误输入。 */
XRT_API uint16* xrtUtf32ViewTo16(xutf32view Source, xutfpolicy Policy, size_t* pSize);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_UNICODE_TEXT)

XRT_EXTERN_C_BEGIN



/*
	按 Unicode 标量解析带负索引的范围并返回借用视图。
	负起点从末尾计数，负数量表示一直到末尾，越界起点会钳制到边界。
*/
XRT_API bool xrtUtf8Range(xstrview Text, int64 iStart, int64 iCount,
	xstrview* pRange);



/* 按 Unicode 标量复制带负索引的范围。 */
XRT_API str xrtUtf8Substr(xstrview Text, int64 iStart, int64 iCount);



/* 从指定标量索引查找严格 UTF-8 子串，未找到返回 XRT_NPOS。 */
XRT_API size_t xrtUtf8Find(xstrview Text, xstrview Part, size_t iStart);



/* 按 ASCII 大小写不敏感规则查找严格 UTF-8 子串。 */
XRT_API size_t xrtUtf8CaseFind(xstrview Text, xstrview Part, size_t iStart);



/* 从右侧查找严格 UTF-8 子串并返回标量索引。 */
XRT_API size_t xrtUtf8RFind(xstrview Text, xstrview Part);



/* 按 ASCII 大小写不敏感规则从右侧查找严格 UTF-8 子串。 */
XRT_API size_t xrtUtf8CaseRFind(xstrview Text, xstrview Part);



/* 判断文本是否包含集合中的任意 Unicode 标量。 */
XRT_API bool xrtUtf8ContainsAny(xstrview Text, xstrview Set);



/* 删除左侧属于指定 Unicode 标量集合的内容并返回借用视图。 */
XRT_API bool xrtUtf8TrimLeftSet(xstrview Text, xstrview Set,
	xstrview* pResult);



/* 删除右侧属于指定 Unicode 标量集合的内容并返回借用视图。 */
XRT_API bool xrtUtf8TrimRightSet(xstrview Text, xstrview Set,
	xstrview* pResult);



/* 删除两侧属于指定 Unicode 标量集合的内容并返回借用视图。 */
XRT_API bool xrtUtf8TrimSet(xstrview Text, xstrview Set,
	xstrview* pResult);



/* 按 Unicode 标量位置插入严格 UTF-8 子串。 */
XRT_API str xrtUtf8Insert(xstrview Text, int64 iPosition, xstrview Part);



/* 按 Unicode 标量范围删除内容，负数量表示一直删除到末尾。 */
XRT_API str xrtUtf8Remove(xstrview Text, int64 iStart, int64 iCount);



/* 按 Unicode 标量宽度在左侧重复填充严格 UTF-8 文本。 */
XRT_API str xrtUtf8PadLeft(xstrview Text, size_t iWidth, xstrview Fill);



/* 按 Unicode 标量宽度在右侧重复填充严格 UTF-8 文本。 */
XRT_API str xrtUtf8PadRight(xstrview Text, size_t iWidth, xstrview Fill);



/* 按 Unicode 标量宽度在两侧重复填充严格 UTF-8 文本。 */
XRT_API str xrtUtf8PadCenter(xstrview Text, size_t iWidth, xstrview Fill);



/* 按 Unicode 标量反转严格 UTF-8 文本到调用方缓冲区；允许原地反转。 */
XRT_API bool xrtUtf8ReverseTo(xstrview Text, char* sOutput, size_t iCapacity);



/* 按 Unicode 标量反转严格 UTF-8 文本并创建独立字符串。 */
XRT_API str xrtUtf8Reverse(xstrview Text);



/*
	按 Unicode 标量集合过滤严格 UTF-8 文本。
	输出为空且容量为零时只查询所需长度；允许输入和输出起点相同。
*/
XRT_API bool xrtUtf8FilterTo(xstrview Text, xstrview Set,
	char* sOutput, size_t iCapacity, size_t* pOutputSize);



/* 按 Unicode 标量集合过滤严格 UTF-8 文本并创建独立字符串。 */
XRT_API str xrtUtf8Filter(xstrview Text, xstrview Set);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_UNICODE_DISTANCE)

XRT_EXTERN_C_BEGIN



/* 按 Unicode 标量计算 UTF-8 编辑距离，超过限制时返回 XRT_NPOS。 */
XRT_API size_t xrtUtf8Distance(xstrview Left, xstrview Right, size_t iLimit);



/* 按 Unicode 标量返回 0.0 至 1.0 的 UTF-8 相似度，失败返回负值。 */
XRT_API double xrtUtf8Similarity(xstrview Left, xstrview Right);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_CHARSET)

/* 字符集层只处理 Unicode 编码方案，不把本机代码页伪装成 UTF-8。 */
typedef enum xencoding {
	XENCODING_UNKNOWN = 0,
	XENCODING_UTF8,
	XENCODING_UTF16_LE,
	XENCODING_UTF16_BE,
	XENCODING_UTF32_LE,
	XENCODING_UTF32_BE
} xencoding;



XRT_EXTERN_C_BEGIN



/* 返回编码方案的码元字节数，未知编码返回零。 */
XRT_API size_t xrtEncodingUnitSize(xencoding Encoding);



/* 检查开头的 Unicode BOM；pSize 不得与输入字节重叠。 */
XRT_API xencoding xrtEncodingBom(xbytesview Data, size_t* pSize);



/* 写出指定编码的 BOM；目标为空时返回所需字节数，目标不足设置 XERR_RANGE。 */
XRT_API size_t xrtEncodingWriteBom(xencoding Encoding, bytes pTarget, size_t iCapacity);



/* 在五种 Unicode 编码方案之间转码；pSize 不得与源重叠。 */
XRT_API bytes xrtTranscode(xbytesview Source, xencoding SourceEncoding,
	xencoding TargetEncoding, xutfpolicy Policy, bool bWriteBom, size_t* pSize);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_CHARSET_DETECT)

/* 检测结果明确表达猜测强度，零表示没有可靠结论。 */
typedef struct xencodingguess {
	xencoding Encoding;
	size_t BomSize;
	uint8 Confidence;
} xencodingguess;



XRT_EXTERN_C_BEGIN



/* 根据 BOM、严格合法性和零字节分布猜测 Unicode 编码。 */
XRT_API xencodingguess xrtEncodingGuess(xbytesview Data);



XRT_EXTERN_C_END

#endif

#endif
