#ifndef XLANG_EXTLIB_XHTTP_NATIVE_INTERNAL_H
#define XLANG_EXTLIB_XHTTP_NATIVE_INTERNAL_H

#include "../../include/xhttp_package.h"

#include <limits.h>
#include <string.h>



#if defined(_WIN32) || defined(_WIN64)
	#define XLANG_XHTTP_EXPORT __declspec(dllexport)
#else
	#define XLANG_XHTTP_EXPORT
#endif



typedef struct xhttp_native_url {
	str Text;
	size_t Size;
	xurl Value;
} xhttp_native_url;



/* 构造 xhttp 语言桥接自己的结构化错误。 */
void xhttpNativeError(
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
);



/* 把借用字节复制为可由 xlang Buffer 接管的对象。 */
xbuffer* xhttpNativeBuffer(xbytesview Data);



/* 把 xrtMalloc 所有权转移给 xlang Buffer。 */
xbuffer* xhttpNativeBufferTake(
	bytes pData,
	size_t iSize,
	size_t iCapacity
);



/* 返回当前 XRT 结构化错误文本的独立副本。 */
XLANG_XHTTP_EXPORT void* xhttp_last_error(void);

/* 普通文本可转交底层分配；含 NUL 的 Buffer 仍由调用方按长度读取。 */
XLANG_XHTTP_EXPORT void* xhttp_buffer_take_cstr(void* handle);

#endif
