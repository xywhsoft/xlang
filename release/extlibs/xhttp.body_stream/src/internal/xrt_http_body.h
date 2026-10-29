#ifndef XRT_INTERNAL_HTTP_BODY_H
#define XRT_INTERNAL_HTTP_BODY_H

#include "xhttp_internal.h"

#include "../../include/xrt/http_body.h"



#if defined(XHTTP_FEATURE_HTTP_BODY)

#define XRT_HTTP_BODY_CUSTOM UINT32_C(1)
#define XRT_HTTP_BODY_BYTES UINT32_C(2)



/* 正文对象保存不可变工厂和线程安全引用状态。 */
struct xhttpbody {
	volatile int32 RefCount;
	xatomic32 Opened;
	uint32 Kind;
	uint32 Flags;
	uint64 Length;
	xhttpbodyops Ops;
	ptr Factory;
	xbytesview Bytes;
	xhttpbodyreleaseproc Release;
	ptr ReleaseContext;
};



/* Reader 是单消费状态机，失败错误在销毁前保持稳定。 */
struct xhttpbodyreader {
	xhttpbody* Body;
	xhttpbodyreaderops Ops;
	ptr Context;
	uint64 Bytes;
	size_t Offset;
	xerror* Error;
	bool Done;
	bool Failed;
	bool Again;
};



/* 发布并保存正文域错误。 */
xhttpbodystatus __xrtHttpBodyReaderFail(
	xhttpbodyreader* pReader,
	xerrkind Kind,
	xhttpbodyerror Code,
	cstr sOperation,
	cstr sMessage
);



/* 把来源回调发布的新错误装回当前执行上下文。 */
void __xrtHttpBodySourceErrorCommit(xerror* pSourceError);




/* 消费来源发布的错误；来源未设置错误时补充正文域错误。 */
void __xrtHttpBodyReaderCaptureSource(
	xhttpbodyreader* pReader,
	xerror* pSourceError,
	xhttpbodyerror Code,
	cstr sOperation,
	cstr sMessage
);

#endif

#endif
