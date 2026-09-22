#ifndef XRT_PEM_H
#define XRT_PEM_H

#include <xrt/codec.h>



#if defined(XRT_FEATURE_PEM) && !defined(XRT_FEATURE_CODEC_BASE64)
	#error "XRT PEM support requires XRT_FEATURE_CODEC_BASE64"
#endif



#if defined(XRT_FEATURE_PEM)

/* PEM 块的标签、正文和完整消费区间都借用原始输入；Raw 包含存在的结束行换行。 */
typedef struct xpemblock {
	xstrview Label;
	xstrview Body;
	xstrview Raw;
} xpemblock;



/* PEM 游标允许输入前后存在说明文本，并按出现顺序遍历多个块。 */
typedef struct xpemcursor {
	xstrview Text;
	size_t Offset;
} xpemcursor;



/* PEM 读取结果把正常结束与格式错误分开。 */
typedef enum xpemresult {
	XPEM_ERROR = -1,
	XPEM_DONE = 0,
	XPEM_BLOCK = 1
} xpemresult;



/* PEM 模块稳定错误码。 */
typedef enum xpemerror {
	XPEM_ERROR_BOUNDARY = 1,
	XPEM_ERROR_LABEL,
	XPEM_ERROR_BODY,
	XPEM_ERROR_NOT_FOUND
} xpemerror;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_PEM)

/* 初始化一个严格有界、借用输入的 PEM 游标。 */
XRT_API bool xrtPemInit(xpemcursor* pCursor, cstr sText, size_t iSize);



/* 读取下一个 PEM 块；失败时游标和输出保持不变。 */
XRT_API xpemresult xrtPemRead(xpemcursor* pCursor, xpemblock* pBlock);



/* 查找第一个标签完全匹配的 PEM 块。 */
XRT_API bool xrtPemFind(
	cstr sText,
	size_t iSize,
	cstr sLabel,
	xpemblock* pBlock
);



/* 解码 PEM 块正文；输出为空且容量为零时只验证并查询长度。 */
XRT_API bool xrtPemDecode(
	const xpemblock* pBlock,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 解码并返回由 xrtFree 释放的字节。 */
XRT_API bytes xrtPemDecodeNew(
	const xpemblock* pBlock,
	size_t* pOutputSize
);



/*
	生成 RFC 7468 文本，Base64 每行 64 字符并统一使用 LF。
	输出为空且容量为零时只查询文本长度；实际写入要求额外的末尾零字节。
*/
XRT_API bool xrtPemEncode(
	cstr sLabel,
	const void* pData,
	size_t iSize,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 生成并返回由 xrtFree 释放的 PEM 文本。 */
XRT_API str xrtPemEncodeNew(cstr sLabel, const void* pData, size_t iSize);

#endif



XRT_EXTERN_C_END

#endif
