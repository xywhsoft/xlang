#include "../../include/xhttp_body_file_package.h"
#include "../internal/xrt_http_body_file.h"



#if defined(_WIN32) || defined(_WIN64)
	#define XLANG_XHTTP_BODY_FILE_EXPORT __declspec(dllexport)
#else
	#define XLANG_XHTTP_BODY_FILE_EXPORT
#endif



typedef struct xhttp_body_file_mapper {
	xvalue* Boxer;
} xhttp_body_file_mapper;



/* 释放 Future 拥有的 xlang 正文对象。 */
static void xhttpBodyFileValueFree(void* pValue)
{
	xvalue** ppValue = (xvalue**)pValue;

	if ( ppValue != NULL ) {
		xrtValueRelease(*ppValue);
		xrtFree(ppValue);
	}
}



/* 释放异步映射持有的 xlang 函数环境。 */
static void xhttpBodyFileMapperFree(void* pArgument)
{
	xhttp_body_file_mapper* pMapper =
		(xhttp_body_file_mapper*)pArgument;

	if ( pMapper != NULL ) {
		xrtValueRelease(pMapper->Boxer);
		xrtFree(pMapper);
	}
}



/* 把当前 XRT 错误复制到 Future 结果；没有具体错误时补充内部错误。 */
static int xhttpBodyFileMapError(
	xl6_future_result* pOutput,
	cstr sMessage
)
{
	if ( xrtGetError() == NULL ) {
		xrtSetErrorInfo(
			XERR_INTERNAL,
			"xlang.extlib.xhttp.body_file",
			1,
			sMessage
		);
	}
	pOutput->iStatus = XL6_FUTURE_STATUS_FAILED;
	pOutput->Error = xrtErrorRef(xrtGetError());
	return -1;
}



/* 把 XRT 借用的正文映射为拥有独立引用的 xlang Body 对象。 */
static int xhttpBodyFileMap(
	const xl6_future_result* pInput,
	void* pArgument,
	xl6_future_result* pOutput
)
{
	xhttp_body_file_mapper* pMapper =
		(xhttp_body_file_mapper*)pArgument;
	xvalue* pArgumentValue;
	xvalue* pResult;
	xvalue** ppResult;
	xrtcallframe Frame;
	xrtcallresult Result = XRT_CALL_RESULT_INIT;
	bool bInvoked;

	if ( (pInput == NULL) || (pInput->pValue == NULL) ||
		(pMapper == NULL) ||
		!xrtValueIsCallable(pMapper->Boxer) ) {
		xrtSetErrorInfo(
			XERR_INTERNAL,
			"xlang.extlib.xhttp.body_file",
			2,
			"the file body mapper received an invalid result"
		);
		return xhttpBodyFileMapError(
			pOutput,
			"the file body mapper received an invalid result"
		);
	}
	xrtClearError();
	pArgumentValue = xrtValuePointer(pInput->pValue);
	if ( pArgumentValue == NULL ) {
		return xhttpBodyFileMapError(
			pOutput,
			"failed to create the file body callback argument"
		);
	}
	memset(&Frame, 0, sizeof(Frame));
	Frame.ArgumentCount = 1u;
	Frame.Arguments = &pArgumentValue;
	bInvoked = xrtValueInvoke(pMapper->Boxer, &Frame, &Result);
	xrtValueRelease(pArgumentValue);
	if ( !bInvoked ) {
		/* xcall 失败已带有 XRT 结构化错误，不存在第二份语言异常槽。 */
		xrtCallResultUnit(&Result);
		return xhttpBodyFileMapError(
			pOutput,
			"the file body callback failed"
		);
	}
	pResult = xrtValueRetain(xrtCallResultGet(&Result, 0u));
	xrtCallResultUnit(&Result);
	if ( pResult == NULL ) {
		return xhttpBodyFileMapError(
			pOutput,
			"the file body callback returned no object"
		);
	}
	ppResult = (xvalue**)xrtMalloc(sizeof(*ppResult));
	if ( ppResult == NULL ) {
		xrtValueRelease(pResult);
		return xhttpBodyFileMapError(
			pOutput,
			"failed to allocate the file body future result"
		);
	}
	*ppResult = pResult;
	pOutput->iStatus = XL6_FUTURE_STATUS_OK;
	pOutput->pValue = ppResult;
	pOutput->iFlags = XL6_FUTURE_OWN_VALUE;
	pOutput->pfnFreeValue = xhttpBodyFileValueFree;
	return 0;
}



/* 创建完整文件或严格区间正文，并转换为 future<Body> 的拥有型结果。 */
XLANG_XHTTP_BODY_FILE_EXPORT xfuture* xhttp_body_file_open(
	const char* sPath,
	uint64 iReadSize,
	bool bRange,
	uint64 iOffset,
	uint64 iLength,
	xvalue* pBoxer
)
{
	xhttp_body_file_mapper* pMapper;
	xhttpbodyfileconfig Config;
	xtaskpool* pPool;
	xfuture* pSource;
	xfuture* pResult;

	if ( (sPath == NULL) || (sPath[0] == '\0') ||
		(iReadSize == 0u) || (iReadSize > (uint64)SIZE_MAX) ||
		(pBoxer == NULL) || !xrtValueIsCallable(pBoxer) ) {
		xrtSetErrorInfo(
			XERR_ARGUMENT,
			"xlang.extlib.xhttp.body_file",
			3,
			"path, read size and body mapper must be valid"
		);
		return NULL;
	}
	pPool = (xtaskpool*)xl6ExtlibAsyncFileTaskPool();
	if ( pPool == NULL ) {
		xrtSetErrorInfo(
			XERR_STATE,
			"xlang.extlib.xhttp.body_file",
			4,
			"the shared asynchronous file task pool is unavailable"
		);
		return NULL;
	}
	xrtHttpBodyFileConfigInit(&Config);
	Config.ReadSize = (size_t)iReadSize;
	pMapper = (xhttp_body_file_mapper*)xrtCalloc(
		1u,
		sizeof(*pMapper)
	);
	if ( pMapper == NULL ) {
		return NULL;
	}
	pMapper->Boxer = xrtValueRetain(pBoxer);
	if ( pMapper->Boxer == NULL ) {
		xrtFree(pMapper);
		return NULL;
	}
	pSource = bRange ?
		xrtHttpBodyFileRangeFuture(
			pPool,
			sPath,
			iOffset,
			iLength,
			&Config
		) :
		xrtHttpBodyFileFuture(pPool, sPath, &Config);
	if ( pSource == NULL ) {
		xhttpBodyFileMapperFree(pMapper);
		return NULL;
	}
	pResult = xl6FutureThenOwned(
		pSource,
		xhttpBodyFileMap,
		pMapper,
		xhttpBodyFileMapperFree
	);
	if ( pResult == NULL ) {
		xhttpBodyFileMapperFree(pMapper);
		xl6FutureDestroy(pSource);
		return NULL;
	}
	if ( !xl6FutureLinkCancel(pResult, pSource) ) {
		xl6FutureDestroy(pResult);
		xl6FutureDestroy(pSource);
		return NULL;
	}
	xl6FutureDestroy(pSource);
	return pResult;
}



/* 接管 AsyncFile 原生句柄；失败时也关闭已从 xlang 对象分离的文件。 */
XLANG_XHTTP_BODY_FILE_EXPORT void* xhttp_body_file_adopt(
	void* pFile,
	uint64 iOffset,
	uint64 iLength,
	uint64 iReadSize
)
{
	xhttpbodyfileconfig Config;
	xhttpbody* pBody;

	if ( (pFile == NULL) || (iReadSize == 0u) ||
		(iReadSize > (uint64)SIZE_MAX) ) {
		xrtSetErrorInfo(
			XERR_ARGUMENT,
			"xlang.extlib.xhttp.body_file",
			5,
			"asynchronous file and read size must be valid"
		);
		__xrtHttpBodyFileCloseAsync((xasyncfile*)pFile);
		return NULL;
	}
	xrtHttpBodyFileConfigInit(&Config);
	Config.ReadSize = (size_t)iReadSize;
	pBody = xrtHttpBodyFileAdopt(
		(xasyncfile*)pFile,
		iOffset,
		iLength,
		&Config
	);
	if ( pBody == NULL ) {
		__xrtHttpBodyFileCloseAsync((xasyncfile*)pFile);
	}
	return pBody;
}
