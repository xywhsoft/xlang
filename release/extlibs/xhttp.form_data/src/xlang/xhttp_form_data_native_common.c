#include "xhttp_form_data_native_internal.h"



/* 发布语言桥接自身的结构化错误。 */
void xhttpFormDataNativeError(
	xerrkind Kind,
	int32 iCode,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Code = iCode;
	Desc.Domain = "xlang.extlib.xhttp.form_data";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetErrorTake(pError);
	}
}



/* 把借用字节复制为可由 xlang Buffer 接管的对象。 */
xbuffer* xhttpFormDataNativeBuffer(xbytesview Data)
{
	if ( !xrtMemRangeValid(Data.Data, Data.Size) ) {
		xhttpFormDataNativeError(
			XERR_ARGUMENT,
			1,
			"copy-view",
			"FormData byte view is invalid"
		);
		return NULL;
	}
	return xrtBufferFrom(Data);
}



/* 返回当前线程错误文本的独立副本。 */
XLANG_XHTTP_FORM_DATA_EXPORT void* xhttp_form_data_last_error(void)
{
	const xerror* pError = xrtGetError();
	cstr sMessage = pError != NULL ? xrtErrorMessage(pError) : NULL;

	if ( (sMessage == NULL) || (sMessage[0] == 0) ) {
		sMessage = "xhttp.form_data operation failed";
	}
	return xhttpFormDataNativeBuffer((xbytesview){
		(cbytes)sMessage,
		strlen(sMessage)
	});
}



/* 把语言层限制转换为平台 size_t。 */
bool xhttpFormDataNativeSize(
	long long iValue,
	bool bAllowUnlimited,
	size_t* pResult
)
{
	if ( pResult == NULL ) {
		return false;
	}
	if ( bAllowUnlimited && (iValue == -1) ) {
		*pResult = SIZE_MAX;
		return true;
	}
	if ( (iValue < 0) ||
		((unsigned long long)iValue >
		 (unsigned long long)SIZE_MAX) ) {
		xhttpFormDataNativeError(
			XERR_RANGE,
			2,
			"convert-limit",
			"FormData limits are outside the platform range"
		);
		return false;
	}
	*pResult = (size_t)iValue;
	return true;
}



/* 从语言层字段构建完整 FormData 配置。 */
bool xhttpFormDataNativeConfig(
	long long iInitialParts,
	long long iMaxParts,
	long long iMaxName,
	long long iMaxFilename,
	long long iMaxContentType,
	long long iMaxMetadata,
	long long iMaxPartBytes,
	long long iMaxBodyBytes,
	xformdataconfig* pConfig
)
{
	xformdataconfig Config;
	size_t iPartBytes;
	size_t iBodyBytes;

	if ( pConfig == NULL ) {
		return false;
	}
	if ( !xhttpFormDataNativeSize(
		iInitialParts, false, &Config.InitialParts
	) || !xhttpFormDataNativeSize(
		iMaxParts, false, &Config.MaxParts
	) || !xhttpFormDataNativeSize(
		iMaxName, false, &Config.MaxName
	) || !xhttpFormDataNativeSize(
		iMaxFilename, false, &Config.MaxFilename
	) || !xhttpFormDataNativeSize(
		iMaxContentType, false, &Config.MaxContentType
	) || !xhttpFormDataNativeSize(
		iMaxMetadata, false, &Config.MaxMetadata
	) || !xhttpFormDataNativeSize(
		iMaxPartBytes, true, &iPartBytes
	) || !xhttpFormDataNativeSize(
		iMaxBodyBytes, true, &iBodyBytes
	) ) {
		return false;
	}
	Config.MaxPartBytes = iPartBytes == SIZE_MAX ?
		XHTTP_BODY_UNKNOWN : (uint64)iPartBytes;
	Config.MaxBodyBytes = iBodyBytes == SIZE_MAX ?
		XHTTP_BODY_UNKNOWN : (uint64)iBodyBytes;
	memcpy(pConfig, &Config, sizeof(Config));
	return true;
}



/* 从语言层字段构建完整 multipart 限制。 */
bool xhttpFormDataNativeLimits(
	long long iMaxParts,
	long long iMaxHeaders,
	long long iMaxDelimiterBytes,
	long long iMaxHeaderBytes,
	long long iMaxPartBytes,
	long long iMaxBodyBytes,
	xmultipartlimits* pLimits
)
{
	xmultipartlimits Limits;

	if ( pLimits == NULL ) {
		return false;
	}
	if ( !xhttpFormDataNativeSize(
		iMaxParts, false, &Limits.MaxParts
	) || !xhttpFormDataNativeSize(
		iMaxHeaders, false, &Limits.MaxHeaders
	) || !xhttpFormDataNativeSize(
		iMaxDelimiterBytes, false, &Limits.MaxDelimiterBytes
	) || !xhttpFormDataNativeSize(
		iMaxHeaderBytes, false, &Limits.MaxHeaderBytes
	) || !xhttpFormDataNativeSize(
		iMaxPartBytes, true, &Limits.MaxPartBytes
	) || !xhttpFormDataNativeSize(
		iMaxBodyBytes, true, &Limits.MaxBodyBytes
	) ) {
		return false;
	}
	memcpy(pLimits, &Limits, sizeof(Limits));
	return true;
}



/* 深复制一个借用 Part 为稳定语言句柄。 */
xhttp_form_data_native_part* xhttpFormDataNativePartCreate(
	const xformdatapart* pPart
)
{
	xhttp_form_data_native_part* pResult;
	size_t iMetadata;
	str pWrite;

	if ( pPart == NULL ) {
		return NULL;
	}
	if ( (pPart->Name.Size > (SIZE_MAX - pPart->Filename.Size)) ||
		((pPart->Name.Size + pPart->Filename.Size) >
		 (SIZE_MAX - pPart->ContentType.Size)) ) {
		xhttpFormDataNativeError(
			XERR_RANGE,
			3,
			"snapshot-part",
			"FormData Part metadata size overflow"
		);
		return NULL;
	}
	iMetadata = pPart->Name.Size + pPart->Filename.Size +
		pPart->ContentType.Size;
	pResult = (xhttp_form_data_native_part*)xrtCalloc(
		1u, sizeof(*pResult)
	);
	if ( pResult == NULL ) {
		return NULL;
	}
	pResult->Metadata = (str)xrtMalloc(iMetadata != 0u ? iMetadata : 1u);
	if ( pResult->Metadata == NULL ) {
		xrtFree(pResult);
		return NULL;
	}
	pWrite = pResult->Metadata;
	if ( pPart->Name.Size != 0u ) {
		memcpy(pWrite, pPart->Name.Data, pPart->Name.Size);
		pWrite += pPart->Name.Size;
	}
	if ( pPart->Filename.Size != 0u ) {
		memcpy(pWrite, pPart->Filename.Data, pPart->Filename.Size);
		pWrite += pPart->Filename.Size;
	}
	if ( pPart->ContentType.Size != 0u ) {
		memcpy(pWrite, pPart->ContentType.Data, pPart->ContentType.Size);
	}
	pResult->Body = xrtHttpBodyRef(pPart->Body);
	if ( pResult->Body == NULL ) {
		xrtFree(pResult->Metadata);
		xrtFree(pResult);
		return NULL;
	}
	pResult->NameSize = pPart->Name.Size;
	pResult->FilenameSize = pPart->Filename.Size;
	pResult->ContentTypeSize = pPart->ContentType.Size;
	pResult->Length = pPart->Length;
	pResult->Flags = pPart->Flags;
	return pResult;
}
