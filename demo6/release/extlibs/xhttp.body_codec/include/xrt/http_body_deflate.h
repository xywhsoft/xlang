#ifndef XRT_HTTP_BODY_DEFLATE_H
#define XRT_HTTP_BODY_DEFLATE_H

#include "http_body.h"



#if defined(XHTTP_FEATURE_HTTP_BODY_DEFLATE) && \
	!defined(XHTTP_FEATURE_HTTP_BODY)
	#error "XRT HTTP Deflate body support requires HTTP body support"
#endif

#if defined(XHTTP_FEATURE_HTTP_BODY_DEFLATE) && \
	!defined(XRT_FEATURE_DEFLATE)
	#error "XRT HTTP Deflate body support requires Deflate support"
#endif

#if defined(XHTTP_FEATURE_HTTP_BODY_DEFLATE) && \
	!defined(XHTTP_FEATURE_HTTP_BODY_TRANSFORM)
	#error "XRT HTTP Deflate body support requires HTTP body transform support"
#endif



#if defined(XHTTP_FEATURE_HTTP_BODY_DEFLATE)

/* 每次从来源读取的默认上限只影响推进粒度，不是对象固定缓冲。 */
#define XHTTP_BODY_DEFLATE_READ_DEFAULT 32768u
#define XHTTP_BODY_DEFLATE_QUEUE_DEFAULT 1048576u



/*
	每个已打开 Reader 独立拥有编码器。
	ReadSize 控制每次向来源请求的最大字节数，必须大于零。
	QueueLimit 限制尚未交付的编码载荷字节；零表示不限制。
*/
typedef struct xhttpbodydeflateconfig {
	xdeflateconfig Deflate;
	size_t ReadSize;
	size_t QueueLimit;
} xhttpbodydeflateconfig;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_BODY_DEFLATE)

/* 初始化 gzip、级别 6、默认策略、32 KiB 推进粒度和 1 MiB 队列上限。 */
XRT_API void xrtHttpBodyDeflateConfigInit(
	xhttpbodydeflateconfig* pConfig
);



/*
	创建流式压缩正文，并持有 Source 的独立引用。
	结果长度始终未知；可重放能力与 Source 相同。
*/
XRT_API xhttpbody* xrtHttpBodyDeflate(
	xhttpbody* pSource,
	const xhttpbodydeflateconfig* pConfig
);

#endif



XRT_EXTERN_C_END

#endif

