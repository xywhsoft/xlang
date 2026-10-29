#ifndef XRT_HTTP_EXT_VALUE_H
#define XRT_HTTP_EXT_VALUE_H

#include <xrt.h>



#if defined(XHTTP_FEATURE_HTTP_EXT_VALUE) && \
	(!defined(XHTTP_FEATURE_HTTP_LANGUAGE_CORE) || \
	 !defined(XRT_FEATURE_CODEC_PERCENT))
	#error "xhttp extended values require HTTP language core and percent codec support"
#endif



#if defined(XHTTP_FEATURE_HTTP_EXT_VALUE)

/* RFC 8187 扩展值的三个部分都借用原始字段值。 */
typedef struct xhttpextvalue {
	xstrview Charset;
	xstrview Language;
	xstrview Encoded;
} xhttpextvalue;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_EXT_VALUE)

/* 严格解析 charset'language'value，并验证字符集、语言和转义语法。 */
XRT_API bool xrtHttpExtValueParse(
	xstrview Text,
	xhttpextvalue* pValue
);



/* 严格百分号解码扩展值；不执行字符集转换，也不附加零字节。 */
XRT_API bool xrtHttpExtValueRead(
	const xhttpextvalue* pValue,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 写出扩展值；Charset 为空时使用 UTF-8。 */
XRT_API bool xrtHttpExtValueWrite(
	xstrview Charset,
	xstrview Language,
	xbytesview Value,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 分配并构建零结尾扩展值。 */
XRT_API str xrtHttpExtValueBuild(
	xstrview Charset,
	xstrview Language,
	xbytesview Value,
	size_t* pSize
);

#endif



XRT_EXTERN_C_END

#endif
