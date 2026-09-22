#ifndef XRT_INTERNAL_HTTP_BODY_STREAM_H
#define XRT_INTERNAL_HTTP_BODY_STREAM_H

#include "xrt_http_body.h"
#include "../../include/xrt/http_body_stream.h"



#if defined(XHTTP_FEATURE_HTTP_BODY_STREAM)

/* 内部构建器把精确长度输出直接写入单分配队列节点。 */
typedef bool (*__xrt_http_body_stream_fill_proc)(
	void* pOutput,
	size_t iSize,
	ptr pData
);



/* 计量完成后以一次载荷分配构建并提交 Chunk。 */
xhttpbodystreamresult __xrtHttpBodyStreamBuild(
	xhttpbodystream* pStream,
	size_t iSize,
	__xrt_http_body_stream_fill_proc pFill,
	ptr pData
);

#endif

#endif

