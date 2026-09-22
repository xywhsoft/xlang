#include "../internal/xrt_http_body_transform.h"
#include "../../include/xrt/http_body_inflate.h"



#if defined(XHTTP_FEATURE_HTTP_BODY_INFLATE)

/* 从未对齐的工厂配置副本创建一个独立 Inflate 解码器。 */
static ptr __xrtHttpBodyInflateCreate(const void* pConfig)
{
	xinflateconfig Config;

	memcpy(&Config, pConfig, sizeof(Config));
	return xrtInflateCreate(&Config);
}



/* 把通用正文变换的结束标志直接交给 Inflate 完整性校验。 */
static bool __xrtHttpBodyInflateWrite(
	ptr pCodec,
	xbytesview Input,
	bool bFinal,
	xrt_http_body_transform_output_proc pOutput,
	ptr pData
)
{
	if ( bFinal && xrtInflateDone((xinflate*)pCodec) ) {
		return true;
	}
	return xrtInflateWrite(
		(xinflate*)pCodec,
		Input,
		bFinal,
		pOutput,
		pData
	);
}



/* 销毁通用正文变换持有的 Inflate 解码器。 */
static void __xrtHttpBodyInflateDestroy(ptr pCodec)
{
	xrtInflateDestroy((xinflate*)pCodec);
}



/* 初始化公开解压正文配置。 */
XRT_API void xrtHttpBodyInflateConfigInit(
	xhttpbodyinflateconfig* pConfig
)
{
	xhttpbodyinflateconfig Config;

	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xhttpErrorSetInvalidArgument();
		return;
	}
	memset(&Config, 0, sizeof(Config));
	xrtInflateConfigInit(&Config.Inflate);
	Config.Inflate.OutputLimit =
		XHTTP_BODY_INFLATE_OUTPUT_DEFAULT;
	Config.ReadSize = XHTTP_BODY_INFLATE_READ_DEFAULT;
	Config.QueueLimit = XHTTP_BODY_INFLATE_QUEUE_DEFAULT;
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 创建持有来源独立引用的流式解压正文。 */
XRT_API xhttpbody* xrtHttpBodyInflate(
	xhttpbody* pSource,
	const xhttpbodyinflateconfig* pConfig
)
{
	static const xrt_http_body_transform_ops Ops = {
		__xrtHttpBodyInflateCreate,
		__xrtHttpBodyInflateWrite,
		__xrtHttpBodyInflateDestroy
	};
	xhttpbodyinflateconfig Config;

	if ( pSource == NULL ) {
		__xhttpErrorSetInvalidArgument();
		return NULL;
	}
	xrtHttpBodyInflateConfigInit(&Config);
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
	if ( !xrtInflateConfigValid(&Config.Inflate) ) {
		return NULL;
	}
	return __xrtHttpBodyTransformCreate(
		pSource,
		&Ops,
		&Config.Inflate,
		sizeof(Config.Inflate),
		Config.ReadSize,
		Config.QueueLimit
	);
}

#endif
