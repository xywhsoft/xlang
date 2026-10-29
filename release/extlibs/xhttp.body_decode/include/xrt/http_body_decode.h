#ifndef XRT_HTTP_BODY_DECODE_H
#define XRT_HTTP_BODY_DECODE_H

#include "http_body_inflate.h"



#if defined(XHTTP_FEATURE_HTTP_BODY_DECODE) && \
	(!defined(XHTTP_FEATURE_HTTP_BODY_INFLATE) || \
	 !defined(XRT_FEATURE_HTTP_ENCODING))
	#error "XRT HTTP Body decoding requires HTTP encoding and Inflate body support"
#endif



#if defined(XHTTP_FEATURE_HTTP_BODY_DECODE)

/* 解码结果区分原样、已解码、未知编码回退和真正失败。 */
typedef enum xhttpbodydecoderesult {
	XHTTP_BODY_DECODE_ERROR = -1,
	XHTTP_BODY_DECODE_UNCHANGED = 0,
	XHTTP_BODY_DECODE_APPLIED = 1,
	XHTTP_BODY_DECODE_UNSUPPORTED = 2
} xhttpbodydecoderesult;



/* Inflate 配置会复制到每一层，Format 由 Content-Encoding 覆盖。 */
typedef struct xhttpbodydecodeconfig {
	xhttpbodyinflateconfig Inflate;
	size_t MaxCodings;
} xhttpbodydecodeconfig;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_BODY_DECODE)

/* 初始化通用 Inflate 默认值和四层内容编码上限。 */
XRT_API void xrtHttpBodyDecodeConfigInit(
	xhttpbodydecodeconfig* pConfig
);



/*
	按重复 Content-Encoding 字段逆序构建流式解码 Body。
	所有非错误结果都会向 Output 发布一个调用方负责销毁的 Body 引用。
	未知编码返回 UNSUPPORTED 和未做部分解码的原始 Body。
*/
XRT_API xhttpbodydecoderesult xrtHttpBodyDecodeFields(
	xhttpbody* pSource,
	const xhttpfield* pFields,
	size_t iCount,
	const xhttpbodydecodeconfig* pConfig,
	xhttpbody** ppOutput
);



/* 解码单个 Content-Encoding 字段值的常用便利路径。 */
XRT_API xhttpbodydecoderesult xrtHttpBodyDecode(
	xhttpbody* pSource,
	xstrview ContentEncoding,
	const xhttpbodydecodeconfig* pConfig,
	xhttpbody** ppOutput
);

#endif



XRT_EXTERN_C_END

#endif

