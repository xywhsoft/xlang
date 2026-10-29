#include "xhttp_protocol_native_internal.h"



/* 复制并严格解析媒体类型。 */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_media_parse(
	const char* sText,
	size_t iSize
)
{
	xhttp_protocol_media_type* pType;
	str sCopy = xhttpProtocolNativeTextCopy(sText, iSize);

	if ( sCopy == NULL ) {
		return NULL;
	}
	pType = (xhttp_protocol_media_type*)xrtCalloc(
		1u, sizeof(*pType)
	);
	if ( pType == NULL ) {
		xrtFree(sCopy);
		return NULL;
	}
	if ( !xrtHttpMediaTypeParse(
		(xstrview){ sCopy, iSize }, &pType->Value
	) ) {
		xrtFree(sCopy);
		xrtFree(pType);
		return NULL;
	}
	pType->Text = sCopy;
	pType->Size = iSize;
	return pType;
}



/* point 构造直接接管 MediaType 句柄。 */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_media_wrap(
	void* pHandle
)
{
	return pHandle;
}



/* 深复制 MediaType 及其原始文本。 */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_media_clone(
	void* pHandle
)
{
	xhttp_protocol_media_type* pType =
		(xhttp_protocol_media_type*)pHandle;

	return pType != NULL ?
		xhttp_protocol_media_parse(pType->Text, pType->Size) : NULL;
}



/* 释放 MediaType 拥有的文本。 */
XLANG_XHTTP_PROTOCOL_EXPORT void xhttp_protocol_media_destroy(
	void* pHandle
)
{
	xhttp_protocol_media_type* pType =
		(xhttp_protocol_media_type*)pHandle;

	if ( pType != NULL ) {
		xrtFree(pType->Text);
		memset(pType, 0, sizeof(*pType));
		xrtFree(pType);
	}
}



/* 返回可传给 XRT C API 的原生 xmediatype 句柄。 */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_media_native(
	void* pHandle
)
{
	xhttp_protocol_media_type* pType =
		(xhttp_protocol_media_type*)pHandle;

	return pType != NULL ? &pType->Value : NULL;
}



/* 复制媒体类型原文、type、subtype、参数或结构化后缀。 */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_media_text(
	void* pHandle,
	long long iField
)
{
	xhttp_protocol_media_type* pType =
		(xhttp_protocol_media_type*)pHandle;
	xstrview Text;

	if ( pType == NULL ) {
		return NULL;
	}
	switch ( iField ) {
		case 0:
			Text = (xstrview){ pType->Text, pType->Size };
			break;
		case 1: Text = pType->Value.Type; break;
		case 2: Text = pType->Value.Subtype; break;
		case 3: Text = pType->Value.Parameters; break;
		case 4: Text = xrtHttpMediaTypeSuffix(&pType->Value); break;
		default:
			xhttpProtocolNativeError(
				XERR_RANGE,
				"read-media-type",
				"Media type component index is out of range"
			);
			return NULL;
	}
	return xhttpProtocolNativeBuffer((xbytesview){
		(cbytes)Text.Data,
		Text.Size
	});
}



/* 比较媒体类型 essence。 */
XLANG_XHTTP_PROTOCOL_EXPORT bool xhttp_protocol_media_equal(
	void* pHandle,
	const char* sType,
	size_t iType,
	const char* sSubtype,
	size_t iSubtype
)
{
	xhttp_protocol_media_type* pMedia =
		(xhttp_protocol_media_type*)pHandle;

	return (pMedia != NULL) && xrtHttpMediaTypeEqual(
		&pMedia->Value,
		(xstrview){ sType, iType },
		(xstrview){ sSubtype, iSubtype }
	);
}



/* 判断媒体类型是否适合通用内容编码压缩。 */
XLANG_XHTTP_PROTOCOL_EXPORT bool xhttp_protocol_media_compressible(
	void* pHandle
)
{
	xhttp_protocol_media_type* pType =
		(xhttp_protocol_media_type*)pHandle;

	return (pType != NULL) &&
		xrtHttpMediaTypeCompressible(&pType->Value);
}



/* 查找媒体类型参数；返回标志，未找到返回 -1，错误返回 -2。 */
XLANG_XHTTP_PROTOCOL_EXPORT long long xhttp_protocol_media_param_flags(
	void* pHandle,
	const char* sName,
	size_t iName
)
{
	xhttp_protocol_media_type* pType =
		(xhttp_protocol_media_type*)pHandle;
	xhttpparam Param;
	xhttpnext Next;

	if ( pType == NULL ) {
		return -2;
	}
	Next = xrtHttpMediaTypeParam(
		&pType->Value,
		(xstrview){ sName, iName },
		&Param
	);
	if ( Next == XHTTP_NEXT_ERROR ) {
		return -2;
	}
	return Next == XHTTP_NEXT_ITEM ? (long long)Param.Flags : -1;
}



/* 复制媒体类型参数值；未找到时返回空句柄。 */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_media_param_text(
	void* pHandle,
	const char* sName,
	size_t iName
)
{
	xhttp_protocol_media_type* pType =
		(xhttp_protocol_media_type*)pHandle;
	xhttpparam Param;
	xhttpnext Next;

	if ( pType == NULL ) {
		return NULL;
	}
	Next = xrtHttpMediaTypeParam(
		&pType->Value,
		(xstrview){ sName, iName },
		&Param
	);
	if ( Next != XHTTP_NEXT_ITEM ) {
		return NULL;
	}
	return xhttpProtocolNativeBuffer((xbytesview){
		(cbytes)Param.Value.Data,
		Param.Value.Size
	});
}



/* 规范构建媒体类型文本。 */
static void* xhttp_protocol_media_build_impl(
	void* pHandle, size_t* outputSize
)
{
	xhttp_protocol_media_type* pType =
		(xhttp_protocol_media_type*)pHandle;
	str sText;
	size_t iSize = 0u;

	if ( pType == NULL ) {
		return NULL;
	}
	sText = xrtHttpMediaTypeBuild(&pType->Value, &iSize);
	if ( outputSize != NULL ) *outputSize = iSize;
	return sText;
}

XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_media_build_owned(
	void* pHandle
)
{
    return xhttp_protocol_media_build_impl(pHandle, NULL);
}

/* Compatibility C entry continues to return an owned Buffer. */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_media_build(
	void* pHandle
)
{
    size_t size = 0;
    str text = (str)xhttp_protocol_media_build_impl(pHandle, &size);
    return text != NULL ? xhttpProtocolNativeBufferTake((bytes)text, size, size + 1u) : NULL;
}



/* 按扩展名或路径复制内置 MIME 类型。 */
XLANG_XHTTP_PROTOCOL_EXPORT void* xhttp_protocol_mime_lookup(
	const char* sText,
	size_t iSize,
	bool bPath
)
{
	xstrview Result = bPath ?
		xrtMimeByPath((xstrview){ sText, iSize }) :
		xrtMimeByExt((xstrview){ sText, iSize });

	return xhttpProtocolNativeBuffer((xbytesview){
		(cbytes)Result.Data,
		Result.Size
	});
}



/* 解析 Content-Type 并判断是否适合通用压缩。 */
XLANG_XHTTP_PROTOCOL_EXPORT bool xhttp_protocol_content_compressible(
	const char* sText,
	size_t iSize
)
{
	return xrtHttpContentTypeCompressible(
		(xstrview){ sText, iSize }
	);
}

/* Void destructor ABI used by current XLang native classes. */
XLANG_XHTTP_PROTOCOL_EXPORT void xhttp_protocol_media_destroy_abi(void* handle)
{
    (void)xhttp_protocol_media_destroy(handle);
}
