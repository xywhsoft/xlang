#ifndef XRT_INTERNAL_HTTP_CLIENT_H
#define XRT_INTERNAL_HTTP_CLIENT_H

#include "../../include/xhttp_client_package.h"
#include "../../../xhttp.body/src/internal/xrt_http_body.h"

#include <string.h>



/* Header 所有权检查由 xhttp.protocol 包提供。 */
bool __xrtHttpHeadersOwnedOverlap(
	const xhttpheaders* pHeaders,
	const void* pOutput,
	size_t iSize
);




#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST)

/* 请求只拥有实际使用的动态存储；可选 Trailer 容器保持独立裁剪。 */
struct xhttprequest {
	str Method;
	size_t MethodSize;
	str UrlText;
	size_t UrlSize;
	xurl Url;
	xhttpheaders* Headers;
	xhttpbody* Body;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_TRAILERS)
		xhttpheaders* Trailers;
	#endif
};



/* 发布稳定的客户端请求构建或准备错误。 */
void __xrtHttpRequestError(
	xhttprequesterror Code,
	cstr sOperation,
	cstr sMessage
);



/* 用稳定请求错误域包裹当前线程错误并保留完整原因链。 */
void __xrtHttpRequestWrapError(
	xerrkind DefaultKind,
	xhttprequesterror Code,
	cstr sOperation,
	cstr sMessage
);



/* 设置请求实现使用的通用参数错误。 */
void __xrtHttpRequestInvalidArgument(void);



/* 设置请求实现使用的大小溢出错误。 */
void __xrtHttpRequestSizeOverflow(void);



/* 设置请求实现使用的内部契约错误。 */
void __xrtHttpRequestInternal(void);



/* 校验并接管拥有型 URL 文本；失败时所有权仍归调用方。 */
bool __xrtHttpRequestTakeUrl(
	xhttprequest* pRequest,
	str sUrl,
	size_t iSize
);



/* 校验公开输出不会覆盖请求对象及其借用可见存储。 */
bool __xrtHttpRequestOutputValid(
	const xhttprequest* pRequest,
	const void* pOutput,
	size_t iSize
);



/* 提交拥有型正文与可选 Content-Type；调用方始终转移正文引用。 */
bool __xrtHttpRequestCommitBody(
	xhttprequest* pRequest,
	xhttpbody* pNewBody,
	xstrview ContentType
);



#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_AUTH)

/* 写出、设置并清零临时认证值，字段设置保持失败原子。 */
bool __xrtHttpRequestSetWrittenAuth(
	xhttprequest* pRequest,
	xstrview Name,
	__xrtHttpAuthWriteFunction pWrite,
	const void* pContext
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_TRAILERS)

/* 清空已经存在的请求 Trailer；未创建容器时保持无分配。 */
void __xrtHttpRequestClearTrailers(xhttprequest* pRequest);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_FORM_DATA)

/* 生成 Content-Type 并提交 FormData 正文；调用方始终转移正文引用。 */
bool __xrtHttpRequestCommitFormData(
	xhttprequest* pRequest,
	xhttpbody* pBody,
	const xmultipartboundary* pBoundary
);

#endif

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_PREPARE)

/* 补充字段回调在最终 target 已确定且基础请求验证完成后执行。 */
typedef bool (*__xrtHttp1RequestFieldFunction)(
	xstrview Method,
	xstrview Target,
	ptr pContext,
	xhttpfield* pField
);



/* 冻结请求并用回调生成的单个字段替换全部同名用户字段。 */
xhttp1requestplan* __xrtHttp1RequestPrepareField(
	const xhttprequest* pRequest,
	const xhttp1requestoptions* pOptions,
	__xrtHttp1RequestFieldFunction pField,
	ptr pContext
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE)

/* 响应对象只由客户端事务修改，发布给调用方后保持只读。 */
struct xhttpresponse {
	xhttpversion Version;
	uint16 Status;
	uint32 Flags;
	str Reason;
	size_t ReasonSize;
	xhttpheaders* Headers;
	xhttpheaders* Trailers;
	uint8* Body;
	size_t BodySize;
	size_t BodyCapacity;
	uint64 BodyBytes;
	uint64 WireBodyBytes;
	str Url;
	size_t UrlSize;
	size_t Redirects;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
		str OriginalEncoding;
		size_t OriginalEncodingSize;
	#endif
};



/* 设置不占用客户端 Call 终态槽的响应读取错误。 */
void __xrtHttpResponseSetError(
	xerrkind Kind,
	xhttpresponseerror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



/* 提取并包装当前线程错误，保留底层类别和完整原因链。 */
void __xrtHttpResponseWrapError(
	xerrkind DefaultKind,
	xhttpresponseerror Code,
	cstr sOperation,
	cstr sMessage
);



/* 校验公开输出不会覆盖响应对象及其借用可见存储。 */
bool __xrtHttpResponseOutputValid(
	const xhttpresponse* pResponse,
	const void* pOutput,
	size_t iSize
);



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH)

/* 专用 challenge 解码回调只负责协议层验证和调用方缓冲写入。 */
typedef bool (*__xrtHttpResponseChallengeReadFunction)(
	xstrview Value,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	void* pChallenge
);



/* 查找并解码指定 scheme；查询和失败不推进游标，实际成功后提交。 */
xhttpnext __xrtHttpResponseChallengeRead(
	const xhttpresponse* pResponse,
	xstrview Name,
	xstrview Scheme,
	xhttpauthcursor* pCursor,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	void* pChallenge,
	size_t iChallengeSize,
	__xrtHttpResponseChallengeReadFunction pRead,
	cstr sOperation,
	cstr sMessage
);

#endif



/* 创建客户端事务内部可填充的响应对象。 */
xhttpresponse* __xrtHttpResponseCreate(
	xhttpversion Version,
	uint16 iStatus,
	xstrview Reason,
	const xhttpheadersconfig* pHeaders
);



/* 追加一个拥有型响应 Header。 */
bool __xrtHttpResponseAddHeader(
	xhttpresponse* pResponse,
	xstrview Name,
	xstrview Value
);



/* 按需创建 trailer 容器并追加一个拥有型字段。 */
bool __xrtHttpResponseAddTrailer(
	xhttpresponse* pResponse,
	const xhttpheadersconfig* pConfig,
	xstrview Name,
	xstrview Value
);



/* 缓冲一段已交付正文并累计 BodyBytes。 */
bool __xrtHttpResponseAppendBody(
	xhttpresponse* pResponse,
	xbytesview Data
);



/*
	复制已经通过内部流式包装器交付的正文。
	Exchange 随后统一累计 BodyBytes，因此这里仅增长缓冲内容。
*/
bool __xrtHttpResponseBufferDeliveredBody(
	xhttpresponse* pResponse,
	xbytesview Data
);



/* 在流式模式累计已交付正文，不分配正文缓冲。 */
bool __xrtHttpResponseDeliverBody(
	xhttpresponse* pResponse,
	uint64 iBytes
);



/* 累计线上编码正文载荷字节数。 */
bool __xrtHttpResponseAddWireBody(
	xhttpresponse* pResponse,
	uint64 iBytes
);



/* 失败原子地替换最终有效 URL。 */
bool __xrtHttpResponseSetUrl(
	xhttpresponse* pResponse,
	xstrview Url
);



/* 保存高层客户端已经完成的重定向跳数。 */
void __xrtHttpResponseSetRedirects(
	xhttpresponse* pResponse,
	size_t iRedirects
);



/* 清除内部包装器产生的流式标志，恢复调用方选择的缓冲语义。 */
void __xrtHttpResponseSetBuffered(xhttpresponse* pResponse);



/* 增加由客户端事务确认的响应标志。 */
void __xrtHttpResponseSetFlags(
	xhttpresponse* pResponse,
	uint32 iFlags
);



#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)

/* 接管原始编码文本，删除失效的表示元数据并标记响应已经解码。 */
void __xrtHttpResponseSetDecoded(
	xhttpresponse* pResponse,
	str sOriginalEncoding,
	size_t iOriginalEncodingSize
);



/* 在全部解码层结束后发布调用方可见的准确正文长度。 */
void __xrtHttpResponseSetBodyBytes(
	xhttpresponse* pResponse,
	uint64 iBodyBytes
);

#endif

#endif

#endif
