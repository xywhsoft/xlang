#ifndef XRT_HTTP_BODY_COMPOSE_H
#define XRT_HTTP_BODY_COMPOSE_H

#include <xrt.h>



#if defined(XHTTP_FEATURE_HTTP_BODY_COMPOSE) && \
	!defined(XHTTP_FEATURE_HTTP_BODY)
	#error "XRT HTTP body compose support requires HTTP body support"
#endif



#if defined(XHTTP_FEATURE_HTTP_BODY_COMPOSE)

/* 组合片段明确区分立即复制的字节与仅保留引用的正文对象。 */
typedef enum xhttpbodypiecekind {
	XHTTP_BODY_PIECE_BYTES = 1,
	XHTTP_BODY_PIECE_BODY = 2
} xhttpbodypiecekind;



/* 未被 Kind 选中的字段必须保持为空，避免无意保留无效对象。 */
typedef struct xhttpbodypiece {
	xhttpbodypiecekind Kind;
	xbytesview Bytes;
	xhttpbody* Body;
} xhttpbodypiece;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_BODY_COMPOSE)

/* 构造一个创建组合正文时立即复制的字节片段描述。 */
XRT_API xhttpbodypiece xrtHttpBodyPieceBytes(xbytesview Data);



/* 构造一个创建组合正文时增加引用的子正文片段描述。 */
XRT_API xhttpbodypiece xrtHttpBodyPieceBody(xhttpbody* pBody);



/*
	按顺序组合全部片段；空数组返回可重放空正文。
	字节在调用期间复制，子正文引用由返回对象持有，输入数组无需长期保存。
	片段数组和非空字节范围必须有效、完整且地址计算不回绕。
*/
XRT_API xhttpbody* xrtHttpBodyCompose(
	const xhttpbodypiece* pPieces,
	size_t iCount
);

#endif



XRT_EXTERN_C_END

#endif

