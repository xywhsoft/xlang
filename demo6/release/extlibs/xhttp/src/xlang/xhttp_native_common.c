#include "xhttp_native_internal.h"



/* 构造统一使用 xlang.extlib.xhttp 域的结构化错误。 */
void xhttpNativeError(
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xlang.extlib.xhttp";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 把借用字节复制到稳定 Buffer。 */
xbuffer* xhttpNativeBuffer(xbytesview Data)
{
	if ( (Data.Data == NULL) && (Data.Size != 0u) ) {
		xhttpNativeError(
			XERR_ARGUMENT,
			"copy-xhttp-text",
			"xhttp text view is invalid"
		);
		return NULL;
	}
	return xrtBufferFrom(Data);
}



/* 接管带零结尾的 XRT 分配结果，Buffer 长度不包含结尾零。 */
xbuffer* xhttpNativeBufferTake(
	bytes pData,
	size_t iSize,
	size_t iCapacity
)
{
	xbuffer* pBuffer;

	if ( (pData == NULL) || (iCapacity < iSize) ) {
		xrtFree(pData);
		xhttpNativeError(
			XERR_ARGUMENT,
			"take-xhttp-text",
			"xhttp owned text range is invalid"
		);
		return NULL;
	}
	pBuffer = xrtBufferCreateTake(&pData, iSize, iCapacity);
	if ( pBuffer == NULL ) {
		xrtFree(pData);
	}
	return pBuffer;
}



/* 错误读取返回独立 Buffer，避免跨下一次 XRT 调用借用线程错误文本。 */
XLANG_XHTTP_EXPORT void* xhttp_last_error(void)
{
	const xerror* pError = xrtGetError();
	cstr sMessage = pError != NULL ? xrtErrorMessage(pError) : NULL;

	if ( (sMessage == NULL) || (sMessage[0] == 0) ) {
		sMessage = "xhttp operation failed";
	}
	return xhttpNativeBuffer((xbytesview){
		(cbytes)sMessage,
		strlen(sMessage)
	});
}
