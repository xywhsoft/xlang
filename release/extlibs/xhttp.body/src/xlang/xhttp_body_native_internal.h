#ifndef XLANG_EXTLIB_XHTTP_BODY_NATIVE_INTERNAL_H
#define XLANG_EXTLIB_XHTTP_BODY_NATIVE_INTERNAL_H

#include "../../include/xhttp_body_package.h"

#include <limits.h>
#include <string.h>



#if defined(_WIN32) || defined(_WIN64)
	#define XLANG_XHTTP_BODY_EXPORT __declspec(dllexport)
#else
	#define XLANG_XHTTP_BODY_EXPORT
#endif



/* Reader 包装保存最后一次状态，避免把 EOF、AGAIN 和错误混为一体。 */
typedef struct xhttp_body_native_reader {
	xhttpbodyreader* Reader;
	xhttpbodystatus Status;
} xhttp_body_native_reader;



/* Chunk 包装直接拥有一次 XRT 数据租约。 */
typedef struct xhttp_body_native_chunk {
	xhttpbodychunk Chunk;
} xhttp_body_native_chunk;



/* 构造 xhttp.body 语言桥接自己的结构化错误。 */
void xhttpBodyNativeError(
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
);



/* 把借用字节复制为可由 xlang Buffer 接管的对象。 */
xbuffer* xhttpBodyNativeBuffer(xbytesview Data);



#endif
