#include "xhttp_body_native_internal.h"



/* 构造统一使用 xlang.extlib.xhttp.body 域的结构化错误。 */
void xhttpBodyNativeError(
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xlang.extlib.xhttp.body";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetErrorTake(pError);
	}
}



/* 把借用字节复制到稳定 Buffer。 */
xbuffer* xhttpBodyNativeBuffer(xbytesview Data)
{
	if ( (Data.Data == NULL) && (Data.Size != 0u) ) {
		xhttpBodyNativeError(
			XERR_ARGUMENT,
			"copy-body-data",
			"HTTP body data view is invalid"
		);
		return NULL;
	}
	return xrtBufferFrom(Data);
}



/* 返回当前线程错误文本的独立副本。 */
XLANG_XHTTP_BODY_EXPORT void* xhttp_body_last_error(void)
{
	const xerror* pError = xrtGetError();
	cstr sMessage = pError != NULL ? xrtErrorMessage(pError) : NULL;

	if ( (sMessage == NULL) || (sMessage[0] == 0) ) {
		sMessage = "xhttp.body operation failed";
	}
	return xhttpBodyNativeBuffer((xbytesview){
		(cbytes)sMessage,
		strlen(sMessage)
	});
}
