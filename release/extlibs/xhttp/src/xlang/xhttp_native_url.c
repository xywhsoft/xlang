#include "xhttp_native_internal.h"



typedef bool (*xhttp_native_url_writer)(
	const xurl* pUrl,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 接管稳定文本并建立全部借用该文本的 URL 视图。 */
static xhttp_native_url* xhttpNativeUrlTake(
	str sText,
	size_t iSize
)
{
	xhttp_native_url* pOwner;

	if ( (sText == NULL) || (iSize == SIZE_MAX) ) {
		xrtFree(sText);
		xhttpNativeError(
			XERR_ARGUMENT,
			"parse-url",
			"URL text range is invalid"
		);
		return NULL;
	}
	pOwner = (xhttp_native_url*)xrtCalloc(1u, sizeof(*pOwner));
	if ( pOwner == NULL ) {
		xrtFree(sText);
		return NULL;
	}
	if ( !xrtUrlParse((xstrview){ sText, iSize }, &pOwner->Value) ) {
		xrtFree(sText);
		xrtFree(pOwner);
		return NULL;
	}
	pOwner->Text = sText;
	pOwner->Size = iSize;
	return pOwner;
}



/* 复制语言字符串，保证 URL 对象的借用视图覆盖完整生命周期。 */
XLANG_XHTTP_EXPORT void* xhttp_url_parse(
	const char* sText,
	size_t iSize
)
{
	str sCopy;

	if ( ((sText == NULL) && (iSize != 0u)) || (iSize == SIZE_MAX) ) {
		xhttpNativeError(
			XERR_ARGUMENT,
			"parse-url",
			"URL text range is invalid"
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
	return xhttpNativeUrlTake(sCopy, iSize);
}



/* point 构造接管已经创建的语言 URL 包装。 */
XLANG_XHTTP_EXPORT void* xhttp_url_wrap(void* pHandle)
{
	return pHandle;
}



/* 释放 URL 及其全部组件视图依赖的文本存储。 */
XLANG_XHTTP_EXPORT long long xhttp_url_destroy(void* pHandle)
{
	xhttp_native_url* pOwner = (xhttp_native_url*)pHandle;

	if ( pOwner != NULL ) {
		xrtFree(pOwner->Text);
		memset(pOwner, 0, sizeof(*pOwner));
		xrtFree(pOwner);
	}
	return 0;
}



/* 暴露只在语言 URL 对象存活期间有效的底层 xurl 借用句柄。 */
XLANG_XHTTP_EXPORT void* xhttp_url_native(void* pHandle)
{
	xhttp_native_url* pOwner = (xhttp_native_url*)pHandle;

	return pOwner != NULL ? &pOwner->Value : NULL;
}



/* 复制原始 URL 文本。 */
XLANG_XHTTP_EXPORT void* xhttp_url_text(void* pHandle)
{
	xhttp_native_url* pOwner = (xhttp_native_url*)pHandle;

	return pOwner != NULL ?
		xhttpNativeBuffer((xbytesview){
			(cbytes)pOwner->Text,
			pOwner->Size
		}) : NULL;
}



/* 按稳定字段编号复制 URL 组件。 */
XLANG_XHTTP_EXPORT void* xhttp_url_component(
	void* pHandle,
	long long iField
)
{
	xhttp_native_url* pOwner = (xhttp_native_url*)pHandle;
	xstrview Text;

	if ( pOwner == NULL ) {
		return NULL;
	}
	switch ( iField ) {
		case 0: Text = pOwner->Value.Scheme; break;
		case 1: Text = pOwner->Value.Authority; break;
		case 2: Text = pOwner->Value.UserInfo; break;
		case 3: Text = pOwner->Value.Host; break;
		case 4: Text = pOwner->Value.PortText; break;
		case 5: Text = pOwner->Value.Path; break;
		case 6: Text = pOwner->Value.Query; break;
		case 7: Text = pOwner->Value.Fragment; break;
		default:
			xhttpNativeError(
				XERR_RANGE,
				"read-url-component",
				"URL component index is out of range"
			);
			return NULL;
	}
	return xhttpNativeBuffer((xbytesview){
		(cbytes)Text.Data,
		Text.Size
	});
}



/* 返回存在位、显式数值端口或布尔属性。 */
XLANG_XHTTP_EXPORT unsigned long long xhttp_url_value(
	void* pHandle,
	long long iField
)
{
	xhttp_native_url* pOwner = (xhttp_native_url*)pHandle;

	if ( pOwner == NULL ) {
		return 0u;
	}
	if ( iField == 0 ) {
		return pOwner->Value.Flags;
	}
	if ( iField == 1 ) {
		return pOwner->Value.Port;
	}
	if ( iField == 2 ) {
		return xrtUrlSecure(&pOwner->Value) ? 1u : 0u;
	}
	if ( iField == 3 ) {
		return xrtUrlPortIsDefault(&pOwner->Value) ? 1u : 0u;
	}
	return 0u;
}



/* 解析适合网络连接的显式或默认端口，失败返回负值并保留 XRT 错误。 */
XLANG_XHTTP_EXPORT long long xhttp_url_port(void* pHandle)
{
	xhttp_native_url* pOwner = (xhttp_native_url*)pHandle;
	uint16 iPort;

	if ( (pOwner == NULL) || !xrtUrlPort(&pOwner->Value, &iPort) ) {
		return -1;
	}
	return iPort;
}



/* 按 ASCII 不区分大小写比较 scheme。 */
XLANG_XHTTP_EXPORT bool xhttp_url_scheme_is(
	void* pHandle,
	const char* sScheme,
	size_t iSize
)
{
	xhttp_native_url* pOwner = (xhttp_native_url*)pHandle;

	return (pOwner != NULL) &&
		xrtUrlSchemeIs(&pOwner->Value, (xstrview){ sScheme, iSize });
}



/* 以两遍无部分写入方式复制 URL 的派生线路文本。 */
static xbuffer* xhttpNativeUrlWrite(
	const xurl* pUrl,
	xhttp_native_url_writer pWriter
)
{
	bytes pData;
	size_t iSize = 0u;

	if ( (pUrl == NULL) || (pWriter == NULL) ||
		 !pWriter(pUrl, NULL, 0u, &iSize) ||
		 (iSize == SIZE_MAX) ) {
		return NULL;
	}
	pData = (bytes)xrtMalloc(iSize + 1u);
	if ( pData == NULL ) {
		return NULL;
	}
	if ( !pWriter(pUrl, pData, iSize, &iSize) ) {
		xrtFree(pData);
		return NULL;
	}
	pData[iSize] = 0;
	return xhttpNativeBufferTake(pData, iSize, iSize + 1u);
}



/* 写出完整 URL、authority、Host 字段值或 origin-form target。 */
XLANG_XHTTP_EXPORT void* xhttp_url_write(
	void* pHandle,
	long long iMode
)
{
	xhttp_native_url* pOwner = (xhttp_native_url*)pHandle;
	xhttp_native_url_writer pWriter;

	if ( pOwner == NULL ) {
		return NULL;
	}
	switch ( iMode ) {
		case 0: pWriter = xrtUrlWrite; break;
		case 1: pWriter = xrtUrlAuthorityWrite; break;
		case 2: pWriter = xrtUrlHostWrite; break;
		case 3: pWriter = xrtUrlTargetWrite; break;
		default:
			xhttpNativeError(
				XERR_RANGE,
				"write-url",
				"URL output mode is out of range"
			);
			return NULL;
	}
	return xhttpNativeUrlWrite(&pOwner->Value, pWriter);
}



/* 按 RFC 3986 解析相对引用并返回拥有结果文本的新 URL。 */
XLANG_XHTTP_EXPORT void* xhttp_url_resolve(
	void* pHandle,
	const char* sReference,
	size_t iSize
)
{
	xhttp_native_url* pOwner = (xhttp_native_url*)pHandle;
	str sResolved;
	size_t iResolved = 0u;

	if ( pOwner == NULL ) {
		return NULL;
	}
	sResolved = xrtUrlResolveBuild(
		&pOwner->Value,
		(xstrview){ sReference, iSize },
		&iResolved
	);
	return sResolved != NULL ?
		xhttpNativeUrlTake(sResolved, iResolved) : NULL;
}



/* 分配并返回 RFC 3986 remove_dot_segments 结果。 */
XLANG_XHTTP_EXPORT void* xhttp_url_path_normalize(
	const char* sPath,
	size_t iSize
)
{
	str sNormalized;
	size_t iNormalized = 0u;

	sNormalized = xrtUrlPathNormalizeBuild(
		(xstrview){ sPath, iSize },
		&iNormalized
	);
	return sNormalized != NULL ?
		xhttpNativeBufferTake(
			(bytes)sNormalized,
			iNormalized,
			iNormalized + 1u
		) : NULL;
}



/* 返回 HTTP、HTTPS、WS 和 WSS 的默认端口。 */
XLANG_XHTTP_EXPORT long long xhttp_url_default_port(
	const char* sScheme,
	size_t iSize
)
{
	return xrtUrlDefaultPort((xstrview){ sScheme, iSize });
}

/* Void destructor ABI used by current XLang native classes. */
XLANG_XHTTP_EXPORT void xhttp_url_destroy_abi(void* handle)
{
    (void)xhttp_url_destroy(handle);
}
