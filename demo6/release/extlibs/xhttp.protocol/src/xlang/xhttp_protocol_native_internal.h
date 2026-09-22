#ifndef XLANG_EXTLIB_XHTTP_PROTOCOL_NATIVE_INTERNAL_H
#define XLANG_EXTLIB_XHTTP_PROTOCOL_NATIVE_INTERNAL_H

#include "../../include/xhttp_protocol_package.h"

#include <string.h>



#if defined(_WIN32) || defined(_WIN64)
	#define XLANG_XHTTP_PROTOCOL_EXPORT __declspec(dllexport)
#else
	#define XLANG_XHTTP_PROTOCOL_EXPORT
#endif



/* Cookies 拥有原始字段文本及全部借用该文本的 pair。 */
typedef struct xhttp_protocol_cookies {
	str Text;
	size_t Size;
	xcookiepair* Pairs;
	size_t Count;
} xhttp_protocol_cookies;



/* SetCookie 拥有原始字段文本及借用该文本的解析结果。 */
typedef struct xhttp_protocol_set_cookie {
	str Text;
	size_t Size;
	xsetcookie Value;
} xhttp_protocol_set_cookie;



/* MediaType 拥有原始字段文本及借用该文本的解析结果。 */
typedef struct xhttp_protocol_media_type {
	str Text;
	size_t Size;
	xmediatype Value;
} xhttp_protocol_media_type;



/* 构造 xhttp.protocol 语言桥接自己的结构化错误。 */
void xhttpProtocolNativeError(
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
);



/* 把借用字节复制为可由 xlang Buffer 接管的对象。 */
xbuffer* xhttpProtocolNativeBuffer(xbytesview Data);



/* 把 xrtMalloc 所有权转移给 xlang Buffer。 */
xbuffer* xhttpProtocolNativeBufferTake(
	bytes pData,
	size_t iSize,
	size_t iCapacity
);



/* 复制语言字符串并追加零结尾。 */
str xhttpProtocolNativeTextCopy(
	const char* sText,
	size_t iSize
);



#endif
