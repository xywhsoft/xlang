#ifndef XRT_INTERNAL_HTTP_BODY_TRANSFORM_H
#define XRT_INTERNAL_HTTP_BODY_TRANSFORM_H

#include "xhttp_internal.h"
#include "../../include/xrt/http_body.h"



#if defined(XHTTP_FEATURE_HTTP_BODY_TRANSFORM) && \
	!defined(XHTTP_FEATURE_HTTP_BODY)
	#error "XRT HTTP body transform support requires HTTP body support"
#endif



#if defined(XHTTP_FEATURE_HTTP_BODY_TRANSFORM)

/* 变换器把临时算法输出同步交给正文适配层复制并排队。 */
typedef bool (*xrt_http_body_transform_output_proc)(
	xbytesview Data,
	ptr pData
);



/* 每次打开正文时根据工厂保存的配置创建一个独立算法对象。 */
typedef ptr (*xrt_http_body_transform_create_proc)(
	const void* pConfig
);



/*
	同步消费一段来源输入。
	Final 表示来源已经完整结束，算法必须完成并校验整个数据流。
*/
typedef bool (*xrt_http_body_transform_write_proc)(
	ptr pCodec,
	xbytesview Input,
	bool bFinal,
	xrt_http_body_transform_output_proc pOutput,
	ptr pData
);



/* 销毁一个独立算法对象；空指针语义由具体算法适配器负责。 */
typedef void (*xrt_http_body_transform_destroy_proc)(ptr pCodec);



/* 不可变算法操作由调用模块静态保存，生命周期必须覆盖正文工厂。 */
typedef struct xrt_http_body_transform_ops {
	xrt_http_body_transform_create_proc Create;
	xrt_http_body_transform_write_proc Write;
	xrt_http_body_transform_destroy_proc Destroy;
} xrt_http_body_transform_ops;



/*
	创建持有来源引用和配置副本的流式变换正文。
	结果长度未知，可重放能力与来源相同。
	QueueLimit 只约束 Reader 内部尚未交付的载荷；零表示不限制。
*/
xhttpbody* __xrtHttpBodyTransformCreate(
	xhttpbody* pSource,
	const xrt_http_body_transform_ops* pOps,
	const void* pConfig,
	size_t iConfigSize,
	size_t iReadSize,
	size_t iQueueLimit
);

#endif

#endif
