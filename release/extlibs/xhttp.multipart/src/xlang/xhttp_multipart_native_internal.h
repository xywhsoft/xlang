#ifndef XLANG_EXTLIB_XHTTP_MULTIPART_NATIVE_INTERNAL_H
#define XLANG_EXTLIB_XHTTP_MULTIPART_NATIVE_INTERNAL_H

#include "../../include/xhttp_multipart_package.h"

#include <limits.h>
#include <string.h>



#if defined(_WIN32) || defined(_WIN64)
	#define XLANG_XHTTP_MULTIPART_EXPORT __declspec(dllexport)
#else
	#define XLANG_XHTTP_MULTIPART_EXPORT
#endif



/* 整包解析器借用语言层保活的 Buffer，不复制正文。 */
typedef struct xhttp_multipart_native_parser {
	xbytesview Body;
	xmultipartboundary Boundary;
	xmultipartlimits Limits;
	size_t Offset;
	size_t Parts;
	xmultiparterrorinfo Error;
	bool Done;
	bool Failed;
} xhttp_multipart_native_parser;



/* Part 描述符借用 Parser 或流事件保活的输入。 */
typedef struct xhttp_multipart_native_part {
	xmultipartpart Value;
} xhttp_multipart_native_part;



/* 流 Reader 自身只持有协议状态，不持有输入缓冲。 */
typedef struct xhttp_multipart_native_reader {
	xmultipartreader Value;
} xhttp_multipart_native_reader;



/* 每个事件固定保存一次 Read 输出，视图借用语言层保活的输入。 */
typedef struct xhttp_multipart_native_event {
	xmultipartreadstatus Status;
	size_t Consumed;
	xmultipartpart Part;
	xbytesview Data;
	xmultiparterrorinfo Error;
} xhttp_multipart_native_event;



/* Writer 的统一回调支持精确测量后一次写入。 */
typedef bool (*xhttp_multipart_native_write_proc)(
	const void* pContext,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 发布语言桥接自己的结构化错误。 */
void xhttpMultipartNativeError(
	xerrkind Kind,
	int32 iCode,
	cstr sOperation,
	cstr sMessage
);



/* 把借用字节复制为可由 xlang Buffer 接管的对象。 */
xbuffer* xhttpMultipartNativeBuffer(xbytesview Data);



/* 接管 XRT 分配的字节，并在构建失败时负责释放。 */
xbuffer* xhttpMultipartNativeBufferTake(
	bytes pData,
	size_t iSize,
	size_t iCapacity
);



/* 对无部分结果写出器执行测量和精确 Buffer 构建。 */
xbuffer* xhttpMultipartNativeBuild(
	xhttp_multipart_native_write_proc pWrite,
	const void* pContext
);



/* 把语言层 -1 或非负预算转换成平台 size_t。 */
bool xhttpMultipartNativeLimit(
	long long iValue,
	size_t* pResult
);



/* 从六个语言预算构建 XRT 限制结构。 */
bool xhttpMultipartNativeLimits(
	long long iMaxParts,
	long long iMaxHeaders,
	long long iMaxDelimiterBytes,
	long long iMaxHeaderBytes,
	long long iMaxPartBytes,
	long long iMaxBodyBytes,
	xmultipartlimits* pLimits
);



/* 复制一个借用 Part 描述符到语言拥有的轻量句柄。 */
xhttp_multipart_native_part* xhttpMultipartNativePartCreate(
	const xmultipartpart* pPart
);



#endif
