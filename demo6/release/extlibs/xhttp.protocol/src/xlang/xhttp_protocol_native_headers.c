#include "xhttp_protocol_native_internal.h"



/* 检查 Headers 句柄并保留统一错误口径。 */
static xhttpheaders* xhttpProtocolHeadersRequire(void* pHandle)
{
	if ( pHandle == NULL ) {
		xhttpProtocolNativeError(
			XERR_ARGUMENT,
			"access-headers",
			"Headers handle is invalid"
		);
		return NULL;
	}
	return (xhttpheaders*)pHandle;
}



/* 创建使用 XRT 默认安全限额的空 Header 容器。 */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_headers_create(void)
{
	return xrtHttpHeadersCreate(NULL);
}



/* 解析完整 Header 块并拥有全部字段副本。 */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_headers_parse(
	const char* sText,
	size_t iSize
)
{
	size_t iError = 0u;

	return xrtHttpHeadersParse(
		(xstrview){ sText, iSize }, NULL, &iError
	);
}



/* point 构造直接接管 Header 容器。 */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_headers_wrap(
	void* pHandle
)
{
	return pHandle;
}



/* 深复制 Header 容器。 */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_headers_clone(
	void* pHandle
)
{
	xhttpheaders* pHeaders = xhttpProtocolHeadersRequire(pHandle);

	return pHeaders != NULL ? xrtHttpHeadersClone(pHeaders) : NULL;
}



/* 释放拥有型 Header 容器。 */
XLANG_XHTTP_PROTOCOL_EXPORT void xhttp_protocol_headers_destroy(
	void* pHandle
)
{
	xrtHttpHeadersDestroy((xhttpheaders*)pHandle);
}



/* 返回可传给 XRT C API 的原生 Header 容器句柄。 */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_headers_native(
	void* pHandle
)
{
	return pHandle;
}



/* 返回字段数量或有效名称和值的总字节数。 */
XLANG_XHTTP_PROTOCOL_EXPORT unsigned long long xhttp_protocol_headers_value(
	void* pHandle,
	long long iField
)
{
	xhttpheaders* pHeaders = (xhttpheaders*)pHandle;

	if ( pHeaders == NULL ) {
		return 0u;
	}
	return iField == 0 ?
		xrtHttpHeadersCount(pHeaders) :
		xrtHttpHeadersBytes(pHeaders);
}



/* 清空 Header 容器并保留已分配容量。 */
XLANG_XHTTP_PROTOCOL_EXPORT void xhttp_protocol_headers_clear(
	void* pHandle
)
{
	xhttpheaders* pHeaders = xhttpProtocolHeadersRequire(pHandle);

	if ( pHeaders != NULL ) {
		xrtHttpHeadersClear(pHeaders);
	}
}



/* 预留字段槽和逻辑文本字节。 */
XLANG_XHTTP_PROTOCOL_EXPORT bool xhttp_protocol_headers_reserve(
	void* pHandle,
	size_t iFields,
	size_t iBytes
)
{
	xhttpheaders* pHeaders = xhttpProtocolHeadersRequire(pHandle);

	return (pHeaders != NULL) &&
		xrtHttpHeadersReserve(pHeaders, iFields, iBytes);
}



/* 紧凑 Header 文本存储。 */
XLANG_XHTTP_PROTOCOL_EXPORT bool xhttp_protocol_headers_compact(
	void* pHandle
)
{
	xhttpheaders* pHeaders = xhttpProtocolHeadersRequire(pHandle);

	return (pHeaders != NULL) && xrtHttpHeadersCompact(pHeaders);
}



/* 追加或替换一个 Header 字段。 */
XLANG_XHTTP_PROTOCOL_EXPORT bool xhttp_protocol_headers_write(
	void* pHandle,
	const char* sName,
	size_t iName,
	const char* sValue,
	size_t iValue,
	bool bSet
)
{
	xhttpheaders* pHeaders = xhttpProtocolHeadersRequire(pHandle);
	xstrview Name = { sName, iName };
	xstrview Value = { sValue, iValue };

	if ( pHeaders == NULL ) {
		return false;
	}
	return bSet ?
		xrtHttpHeadersSet(pHeaders, Name, Value) :
		xrtHttpHeadersAdd(pHeaders, Name, Value);
}



/* 删除、计数或查询同名 Header 字段。 */
XLANG_XHTTP_PROTOCOL_EXPORT unsigned long long xhttp_protocol_headers_name(
	void* pHandle,
	const char* sName,
	size_t iName,
	long long iOperation
)
{
	xhttpheaders* pHeaders = xhttpProtocolHeadersRequire(pHandle);
	xstrview Name = { sName, iName };

	if ( pHeaders == NULL ) {
		return 0u;
	}
	if ( iOperation == 0 ) {
		return xrtHttpHeadersRemove(pHeaders, Name);
	}
	if ( iOperation == 1 ) {
		return xrtHttpHeadersCountName(pHeaders, Name);
	}
	return xrtHttpHeadersHas(pHeaders, Name) ? 1u : 0u;
}



/* 复制指定位置 Header 的名称或值。 */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_headers_at_text(
	void* pHandle,
	unsigned long long iIndex,
	long long iField
)
{
	xhttpheaders* pHeaders = xhttpProtocolHeadersRequire(pHandle);
	const xhttpfield* pField;
	xstrview Text;

	if ( pHeaders == NULL ) {
		return NULL;
	}
	pField = xrtHttpHeadersAt(pHeaders, (size_t)iIndex);
	if ( pField == NULL ) {
		return NULL;
	}
	Text = iField == 0 ? pField->Name : pField->Value;
	return xhttpProtocolNativeBuffer((xbytesview){
		(cbytes)Text.Data,
		Text.Size
	});
}



/* 复制第 N 个同名 Header 值；不存在时返回空句柄。 */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_headers_get(
	void* pHandle,
	const char* sName,
	size_t iName,
	unsigned long long iOccurrence
)
{
	xhttpheaders* pHeaders = xhttpProtocolHeadersRequire(pHandle);
	const xhttpfield* pField;

	if ( pHeaders == NULL ) {
		return NULL;
	}
	pField = xrtHttpHeadersGetNth(
		pHeaders,
		(xstrview){ sName, iName },
		(size_t)iOccurrence
	);
	if ( pField == NULL ) {
		return NULL;
	}
	return xhttpProtocolNativeBuffer((xbytesview){
		(cbytes)pField->Value.Data,
		pField->Value.Size
	});
}



/* 事务性解析并追加完整 Header 块。 */
XLANG_XHTTP_PROTOCOL_EXPORT bool xhttp_protocol_headers_add_block(
	void* pHandle,
	const char* sText,
	size_t iSize
)
{
	xhttpheaders* pHeaders = xhttpProtocolHeadersRequire(pHandle);
	size_t iError = 0u;

	return (pHeaders != NULL) && xrtHttpHeadersAddBlock(
		pHeaders,
		(xstrview){ sText, iSize },
		&iError
	);
}



/* 构建包含最终空行的完整 Header 块。 */
static void* xhttp_protocol_headers_build_impl(
	void* pHandle, size_t* outputSize
)
{
	xhttpheaders* pHeaders = xhttpProtocolHeadersRequire(pHandle);
	str sText;
	size_t iSize = 0u;

	if ( pHeaders == NULL ) {
		return NULL;
	}
	sText = xrtHttpHeadersBuild(pHeaders, &iSize);
	if ( outputSize != NULL ) *outputSize = iSize;
	return sText;
}

XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_headers_build_owned(
	void* pHandle
)
{
    return xhttp_protocol_headers_build_impl(pHandle, NULL);
}

/* Compatibility C entry continues to return an owned Buffer. */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_headers_build(
	void* pHandle
)
{
    size_t size = 0;
    str text = (str)xhttp_protocol_headers_build_impl(pHandle, &size);
    return text != NULL ? xhttpProtocolNativeBufferTake((bytes)text, size, size + 1u) : NULL;
}

/* Void destructor ABI used by current XLang native classes. */
XLANG_XHTTP_PROTOCOL_EXPORT void xhttp_protocol_headers_destroy_abi(void* handle)
{
    (void)xhttp_protocol_headers_destroy(handle);
}
