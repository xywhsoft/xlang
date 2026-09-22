#ifndef XHTTP_INTERNAL_XRT_HTTP_BRIDGE_H
#define XHTTP_INTERNAL_XRT_HTTP_BRIDGE_H

#include "../../include/xhttp_multipart_package.h"
#include "xhttp_internal.h"

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SESSION)
	#include <xrt/http_auth.h>
#endif



#if defined(XHTTP_FEATURE_HTTP_AUTH)

/* 认证值和敏感凭据的常见路径使用固定栈缓冲，超出后才回退到堆。 */
#define XRT_HTTP_AUTH_LOCAL_BYTES 256u



/* 内部认证写出回调支持长度查询和精确缓冲写入。 */
typedef bool (*__xrtHttpAuthWriteFunction)(
	const void* pContext,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 内部认证消费回调必须在返回前复制需要保留的值。 */
typedef bool (*__xrtHttpAuthConsumeFunction)(
	void* pContext,
	xstrview Value
);



/* 用临时缓冲写出认证值、同步消费，并在所有路径清零。 */
bool __xrtHttpAuthWriteTemporary(
	__xrtHttpAuthWriteFunction pWrite,
	const void* pWriteContext,
	__xrtHttpAuthConsumeFunction pConsume,
	void* pConsumeContext
);

#endif



#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST)

/* 把单个十六进制字节转换为数值，无效字节返回负数。 */
int __xrtHttpDigestHexValue(uint8 iByte);



/* 验证线路参数中未转义的非空十六进制值，并返回字节数。 */
bool __xrtHttpDigestHexParamValid(
	const xhttpparam* pParam,
	size_t* pSize
);



/* 验证借用视图是非空十六进制文本。 */
bool __xrtHttpDigestHexViewValid(xstrview Text);



/* 严格读取恰好八位且非 quoted 的 Digest nonce count。 */
bool __xrtHttpDigestNonceCountRead(
	const xhttpparam* pParam,
	uint32* pCount
);



/* 把 nonce count 规范写为八位小写十六进制。 */
void __xrtHttpDigestNonceCountWrite(
	uint32 iCount,
	char sOutput[8]
);



/* 以 quoted-string 形式写出规范小写十六进制参数。 */
bool __xrtHttpDigestWriterHex(
	xrt_http_param_writer* pWriter,
	xstrview Name,
	xstrview Value,
	bool bFirst
);

#endif



#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SESSION)

/* 校验写出区不会覆盖 Digest 会话及可选 Exchange 的保留状态。 */
bool __xrtHttpDigestSessionOutputValid(
	const xhttpdigestsession* pSession,
	const xhttpdigestexchange* pExchange,
	const void* pOutput,
	size_t iSize
);

#endif



#if defined(XHTTP_FEATURE_HTTP_LANGUAGE_CORE)

/* 验证 basic language range 使用的语言标签分段边界。 */
bool __xrtHttpLanguageTextValid(
	xstrview Text,
	bool bRange,
	bool bAllowEmpty,
	size_t* pSubtags
);

#endif



#if defined(XHTTP_FEATURE_HTTP_EXT_VALUE)

/* 不设置错误地拆分并验证 RFC 8187 扩展值。 */
bool __xrtHttpExtValueSplit(
	xstrview Text,
	xhttpextvalue* pValue
);

#endif



#if defined(XHTTP_FEATURE_MIME)

/* MIME 参数访问器在严格参数校验通过后接收借用条目。 */
typedef bool (*xrt_mime_param_visitor)(
	const xhttpparam* pParam,
	void* pContext
);



/* 验证并扫描严格参数列表。 */
bool __xrtMimeParametersInspect(
	xstrview Parameters,
	xrt_mime_param_visitor Visitor,
	void* pContext
);



/* 安全累加 MIME 写出长度。 */
bool __xrtMimeSizeAdd(size_t* pSize, size_t iAdd);



/* 严格验证非空 MIME 参数列表，并拒绝重复参数名称。 */
bool __xrtMimeParametersValid(xstrview Parameters);



/* 按 MIME Sniff 规则静默解析媒体类型 essence，并忽略参数尾。 */
bool __xrtMimeSniffTypeParse(
	xstrview Text,
	xmediatype* pType
);



/* 判断输出范围是否覆盖任意借用视图。 */
bool __xrtMimeOutputOverlap(
	const xstrview* pViews,
	size_t iCount,
	const void* pOutput,
	size_t iSize
);



/* 为同构 Writer 分配零结尾结果。 */
str __xrtMimeBuild(
	const void* pValue,
	bool (*Write)(const void*, void*, size_t, size_t*),
	size_t* pSize
);



/* 验证已解析媒体类型结构与严格参数列表。 */
bool __xrtHttpMediaTypeValid(const xmediatype* pType);

#endif



#if defined(XHTTP_FEATURE_MULTIPART)

/* 内部分隔行结果额外区分流输入尚未完整。 */
typedef enum xrt_multipart_delimiter {
	XRT_MULTIPART_DELIMITER_INVALID = 0,
	XRT_MULTIPART_DELIMITER_PART,
	XRT_MULTIPART_DELIMITER_CLOSE,
	XRT_MULTIPART_DELIMITER_MORE
} xrt_multipart_delimiter;



/* 验证拥有型 boundary 结构没有被调用方破坏。 */
bool __xrtMultipartBoundaryValid(
	const xmultipartboundary* pBoundary
);



/* 验证借用字节视图的空值一致性。 */
bool __xrtMultipartBytesValid(xbytesview Bytes);



/* 解析一条 boundary delimiter。 */
xrt_multipart_delimiter __xrtMultipartDelimiterAt(
	xbytesview Body,
	const xmultipartboundary* pBoundary,
	size_t iDash,
	bool bEnd,
	size_t* pNext
);



/* 定位 Part Header 与正文之间的空行。 */
bool __xrtMultipartHeaderBlock(
	xbytesview Body,
	size_t iStart,
	xstrview* pHeaders,
	size_t* pBodyStart
);



/* 解析 Part Header，并拒绝语义歧义的重复专用字段。 */
bool __xrtMultipartPartHeaders(
	xmultipartpart* pPart,
	xmultiparterrorinfo* pError,
	size_t iBase
);

#endif



#if defined(XHTTP_FEATURE_HTTP_HEADERS)

/* 判断一段输出内存是否覆盖 Header 容器拥有的任意存储。 */
bool __xrtHttpHeadersOwnedOverlap(
	const xhttpheaders* pHeaders,
	const void* pMemory,
	size_t iSize
);



/* 验证 Header 容器配置的容量关系和算术边界。 */
bool __xrtHttpHeadersConfigValid(
	const xhttpheadersconfig* pConfig
);

#endif

#endif
