#include "xhttp_protocol_native_internal.h"



/* 构造统一使用 xlang.extlib.xhttp.protocol 域的结构化错误。 */
void xhttpProtocolNativeError(
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xlang.extlib.xhttp.protocol";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 把借用字节复制到稳定 Buffer。 */
xbuffer* xhttpProtocolNativeBuffer(xbytesview Data)
{
	if ( (Data.Data == NULL) && (Data.Size != 0u) ) {
		xhttpProtocolNativeError(
			XERR_ARGUMENT,
			"copy-protocol-text",
			"xhttp.protocol text view is invalid"
		);
		return NULL;
	}
	return xrtBufferFrom(Data);
}



/* 接管带零结尾的 XRT 分配结果，Buffer 长度不包含结尾零。 */
xbuffer* xhttpProtocolNativeBufferTake(
	bytes pData,
	size_t iSize,
	size_t iCapacity
)
{
	xbuffer* pBuffer;

	if ( (pData == NULL) || (iCapacity < iSize) ) {
		xrtFree(pData);
		xhttpProtocolNativeError(
			XERR_ARGUMENT,
			"take-protocol-text",
			"xhttp.protocol owned text range is invalid"
		);
		return NULL;
	}
	pBuffer = xrtBufferCreateTake(&pData, iSize, iCapacity);
	if ( pBuffer == NULL ) {
		xrtFree(pData);
	}
	return pBuffer;
}



/* 复制语言字符串并追加零结尾。 */
str xhttpProtocolNativeTextCopy(
	const char* sText,
	size_t iSize
)
{
	str sCopy;

	if ( ((sText == NULL) && (iSize != 0u)) || (iSize == SIZE_MAX) ) {
		xhttpProtocolNativeError(
			XERR_ARGUMENT,
			"copy-protocol-input",
			"xhttp.protocol input text range is invalid"
		);
		return NULL;
	}
	sCopy = (str)xrtMalloc(iSize + 1u);
	if ( sCopy == NULL ) {
		return NULL;
	}
	if ( iSize != 0u ) {
		memcpy(sCopy, sText, iSize);
	}
	sCopy[iSize] = 0;
	return sCopy;
}



/* 返回当前线程错误文本的独立副本。 */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_last_error(void)
{
	const xerror* pError = xrtGetError();
	cstr sMessage = pError != NULL ? xrtErrorMessage(pError) : NULL;

	if ( (sMessage == NULL) || (sMessage[0] == 0) ) {
		sMessage = "xhttp.protocol operation failed";
	}
	return xhttpProtocolNativeBuffer((xbytesview){
		(cbytes)sMessage,
		strlen(sMessage)
	});
}
