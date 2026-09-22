#include "../internal/xrt_http_body_transform.h"
#include "../../include/xrt/http_body_deflate.h"



#if defined(XHTTP_FEATURE_HTTP_BODY_DEFLATE)

/* 从未对齐的工厂配置副本创建一个独立 Deflate 编码器。 */
static ptr __xrtHttpBodyDeflateCreate(const void* pConfig)
{
	xdeflateconfig Config;

	memcpy(&Config, pConfig, sizeof(Config));
	return xrtDeflateCreate(&Config);
}



/* 把通用正文变换的结束标志转换为 Deflate Flush。 */
static bool __xrtHttpBodyDeflateWrite(
	ptr pCodec,
	xbytesview Input,
	bool bFinal,
	xrt_http_body_transform_output_proc pOutput,
	ptr pData
)
{
	return xrtDeflateWrite(
		(xdeflate*)pCodec,
		Input,
		bFinal ? XDEFLATE_FLUSH_FINISH : XDEFLATE_FLUSH_NONE,
		pOutput,
		pData
	);
}



/* 销毁通用正文变换持有的 Deflate 编码器。 */
static void __xrtHttpBodyDeflateDestroy(ptr pCodec)
{
	xrtDeflateDestroy((xdeflate*)pCodec);
}



/* 初始化公开压缩正文配置。 */
XRT_API void xrtHttpBodyDeflateConfigInit(
	xhttpbodydeflateconfig* pConfig
)
{
	xhttpbodydeflateconfig Config;

	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xhttpErrorSetInvalidArgument();
		return;
	}
	memset(&Config, 0, sizeof(Config));
	xrtDeflateConfigInit(&Config.Deflate);
	Config.ReadSize = XHTTP_BODY_DEFLATE_READ_DEFAULT;
	Config.QueueLimit = XHTTP_BODY_DEFLATE_QUEUE_DEFAULT;
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 创建持有来源独立引用的流式压缩正文。 */
XRT_API xhttpbody* xrtHttpBodyDeflate(
	xhttpbody* pSource,
	const xhttpbodydeflateconfig* pConfig
)
{
	static const xrt_http_body_transform_ops Ops = {
		__xrtHttpBodyDeflateCreate,
		__xrtHttpBodyDeflateWrite,
		__xrtHttpBodyDeflateDestroy
	};
	xhttpbodydeflateconfig Config;

	if ( pSource == NULL ) {
		__xhttpErrorSetInvalidArgument();
		return NULL;
	}
	xrtHttpBodyDeflateConfigInit(&Config);
	if ( pConfig != NULL ) {
		if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
			__xhttpErrorSetInvalidArgument();
			return NULL;
		}
		memcpy(&Config, pConfig, sizeof(Config));
	}
	if ( Config.ReadSize == 0 ) {
		__xhttpErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtDeflateConfigValid(&Config.Deflate) ) {
		return NULL;
	}
	return __xrtHttpBodyTransformCreate(
		pSource,
		&Ops,
		&Config.Deflate,
		sizeof(Config.Deflate),
		Config.ReadSize,
		Config.QueueLimit
	);
}

#endif
