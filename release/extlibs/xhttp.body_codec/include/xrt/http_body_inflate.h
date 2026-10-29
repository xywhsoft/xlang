#ifndef XRT_HTTP_BODY_INFLATE_H
#define XRT_HTTP_BODY_INFLATE_H

#include "http_body.h"



#if defined(XHTTP_FEATURE_HTTP_BODY_INFLATE) && \
	!defined(XHTTP_FEATURE_HTTP_BODY)
	#error "XRT HTTP Inflate body support requires HTTP body support"
#endif

#if defined(XHTTP_FEATURE_HTTP_BODY_INFLATE) && \
	!defined(XRT_FEATURE_INFLATE)
	#error "XRT HTTP Inflate body support requires Inflate support"
#endif

#if defined(XHTTP_FEATURE_HTTP_BODY_INFLATE) && \
	!defined(XHTTP_FEATURE_HTTP_BODY_TRANSFORM)
	#error "XRT HTTP Inflate body support requires HTTP body transform support"
#endif

#if defined(XHTTP_FEATURE_HTTP_BODY_INFLATE)

/* 默认推进粒度不是固定缓冲，算法窗口由每个已打开 Reader 按需拥有。 */
#define XHTTP_BODY_INFLATE_READ_DEFAULT 32768u
#define XHTTP_BODY_INFLATE_OUTPUT_DEFAULT UINT64_C(67108864)
#define XHTTP_BODY_INFLATE_QUEUE_DEFAULT 67108864u



/*
	每个已打开 Reader 独立拥有解码器。
	ReadSize 控制每次向来源请求的最大字节数，必须大于零。
	QueueLimit 限制尚未交付的解码载荷字节；零表示不限制。
*/
typedef struct xhttpbodyinflateconfig {
	xinflateconfig Inflate;
	size_t ReadSize;
	size_t QueueLimit;
} xhttpbodyinflateconfig;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_BODY_INFLATE)

/* 初始化 HTTP deflate、64 MiB 输出与队列上限和 32 KiB 推进粒度。 */
XRT_API void xrtHttpBodyInflateConfigInit(
	xhttpbodyinflateconfig* pConfig
);



/*
	创建流式解压正文，并持有 Source 的独立引用。
	结果长度始终未知；可重放能力与 Source 相同。
*/
XRT_API xhttpbody* xrtHttpBodyInflate(
	xhttpbody* pSource,
	const xhttpbodyinflateconfig* pConfig
);

#endif



XRT_EXTERN_C_END

#endif

