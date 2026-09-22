#include "xhttp_form_data_native_internal.h"



/* 把语言字符串参数转换为 XRT 借用视图。 */
static xstrview xhttpFormDataNativeText(
	const char* sText,
	size_t iSize
)
{
	return (xstrview){ sText, iSize };
}



/* 按可选文件名参数追加或替换正文 Part。 */
static bool xhttpFormDataNativeBody(
	xformdata* pForm,
	const char* sName,
	size_t iName,
	xhttpbody* pBody,
	const char* sFilename,
	size_t iFilename,
	bool bHasFilename,
	const char* sContentType,
	size_t iContentType,
	bool bSet
)
{
	xstrview Filename = xhttpFormDataNativeText(
		sFilename, iFilename
	);

	if ( bSet ) {
		return xrtFormDataSetBody(
			pForm,
			xhttpFormDataNativeText(sName, iName),
			pBody,
			bHasFilename ? &Filename : NULL,
			xhttpFormDataNativeText(sContentType, iContentType)
		);
	}
	return xrtFormDataAppendBody(
		pForm,
		xhttpFormDataNativeText(sName, iName),
		pBody,
		bHasFilename ? &Filename : NULL,
		xhttpFormDataNativeText(sContentType, iContentType)
	);
}



/* 按可选文件名参数追加或替换复制型字节 Part。 */
static bool xhttpFormDataNativeBytes(
	xformdata* pForm,
	const char* sName,
	size_t iName,
	const void* pData,
	size_t iSize,
	const char* sFilename,
	size_t iFilename,
	bool bHasFilename,
	const char* sContentType,
	size_t iContentType,
	bool bSet
)
{
	xstrview Filename = xhttpFormDataNativeText(
		sFilename, iFilename
	);

	if ( bSet ) {
		return xrtFormDataSetBytes(
			pForm,
			xhttpFormDataNativeText(sName, iName),
			(xbytesview){ (cbytes)pData, iSize },
			bHasFilename ? &Filename : NULL,
			xhttpFormDataNativeText(sContentType, iContentType)
		);
	}
	return xrtFormDataAppendBytes(
		pForm,
		xhttpFormDataNativeText(sName, iName),
		(xbytesview){ (cbytes)pData, iSize },
		bHasFilename ? &Filename : NULL,
		xhttpFormDataNativeText(sContentType, iContentType)
	);
}



/* 创建采用 XRT 默认限制的空 FormData。 */
XLANG_XHTTP_FORM_DATA_EXPORT void* xhttp_form_data_create(void)
{
	return xrtFormDataCreate(NULL);
}



/* 创建采用完整语言配置的空 FormData。 */
XLANG_XHTTP_FORM_DATA_EXPORT void* xhttp_form_data_create_config(
	long long iInitialParts,
	long long iMaxParts,
	long long iMaxName,
	long long iMaxFilename,
	long long iMaxContentType,
	long long iMaxMetadata,
	long long iMaxPartBytes,
	long long iMaxBodyBytes
)
{
	xformdataconfig Config;

	if ( !xhttpFormDataNativeConfig(
		iInitialParts,
		iMaxParts,
		iMaxName,
		iMaxFilename,
		iMaxContentType,
		iMaxMetadata,
		iMaxPartBytes,
		iMaxBodyBytes,
		&Config
	) ) {
		return NULL;
	}
	return xrtFormDataCreate(&Config);
}



/* point 构造直接接管 FormData 句柄。 */
XLANG_XHTTP_FORM_DATA_EXPORT void* xhttp_form_data_wrap(void* pHandle)
{
	return pHandle;
}



/* 深复制容器和正文引用。 */
XLANG_XHTTP_FORM_DATA_EXPORT void* xhttp_form_data_clone(void* pHandle)
{
	return xrtFormDataClone((const xformdata*)pHandle);
}



/* 释放 FormData 容器及其正文引用。 */
XLANG_XHTTP_FORM_DATA_EXPORT long long xhttp_form_data_destroy(
	void* pHandle
)
{
	xrtFormDataDestroy((xformdata*)pHandle);
	return 0;
}



/* 返回可直接传给 XRT C API 的原生句柄。 */
XLANG_XHTTP_FORM_DATA_EXPORT void* xhttp_form_data_native(
	void* pHandle
)
{
	return pHandle;
}



/* 清空全部 Part 并保留数组容量。 */
XLANG_XHTTP_FORM_DATA_EXPORT void xhttp_form_data_clear(void* pHandle)
{
	xrtFormDataClear((xformdata*)pHandle);
}



/* 预留 Part 数组容量。 */
XLANG_XHTTP_FORM_DATA_EXPORT bool xhttp_form_data_reserve(
	void* pHandle,
	size_t iParts
)
{
	return xrtFormDataReserve((xformdata*)pHandle, iParts);
}



/* 返回 Part 数量或元数据字节数。 */
XLANG_XHTTP_FORM_DATA_EXPORT unsigned long long xhttp_form_data_value(
	void* pHandle,
	long long iField
)
{
	const xformdata* pForm = (const xformdata*)pHandle;

	if ( iField == 0 ) {
		return (unsigned long long)xrtFormDataCount(pForm);
	}
	if ( iField == 1 ) {
		return (unsigned long long)xrtFormDataMetadata(pForm);
	}
	return 0u;
}



/* 追加复制型文本字段。 */
XLANG_XHTTP_FORM_DATA_EXPORT bool xhttp_form_data_append_text(
	void* pHandle,
	const char* sName,
	size_t iName,
	const char* sValue,
	size_t iValue
)
{
	return xrtFormDataAppendText(
		(xformdata*)pHandle,
		xhttpFormDataNativeText(sName, iName),
		xhttpFormDataNativeText(sValue, iValue)
	);
}



/* 追加复制型字节或文件字段。 */
XLANG_XHTTP_FORM_DATA_EXPORT bool xhttp_form_data_append_bytes(
	void* pHandle,
	const char* sName,
	size_t iName,
	const void* pData,
	size_t iSize,
	const char* sFilename,
	size_t iFilename,
	bool bHasFilename,
	const char* sContentType,
	size_t iContentType
)
{
	return xhttpFormDataNativeBytes(
		(xformdata*)pHandle,
		sName,
		iName,
		pData,
		iSize,
		sFilename,
		iFilename,
		bHasFilename,
		sContentType,
		iContentType,
		false
	);
}



/* 追加正文引用；固定或流式 Body 都不会被强制读回内存。 */
XLANG_XHTTP_FORM_DATA_EXPORT bool xhttp_form_data_append_body(
	void* pHandle,
	const char* sName,
	size_t iName,
	void* pBody,
	const char* sFilename,
	size_t iFilename,
	bool bHasFilename,
	const char* sContentType,
	size_t iContentType
)
{
	return xhttpFormDataNativeBody(
		(xformdata*)pHandle,
		sName,
		iName,
		(xhttpbody*)pBody,
		sFilename,
		iFilename,
		bHasFilename,
		sContentType,
		iContentType,
		false
	);
}



/* 原子替换全部同名文本字段。 */
XLANG_XHTTP_FORM_DATA_EXPORT bool xhttp_form_data_set_text(
	void* pHandle,
	const char* sName,
	size_t iName,
	const char* sValue,
	size_t iValue
)
{
	return xrtFormDataSetText(
		(xformdata*)pHandle,
		xhttpFormDataNativeText(sName, iName),
		xhttpFormDataNativeText(sValue, iValue)
	);
}



/* 原子替换全部同名复制型字节或文件字段。 */
XLANG_XHTTP_FORM_DATA_EXPORT bool xhttp_form_data_set_bytes(
	void* pHandle,
	const char* sName,
	size_t iName,
	const void* pData,
	size_t iSize,
	const char* sFilename,
	size_t iFilename,
	bool bHasFilename,
	const char* sContentType,
	size_t iContentType
)
{
	return xhttpFormDataNativeBytes(
		(xformdata*)pHandle,
		sName,
		iName,
		pData,
		iSize,
		sFilename,
		iFilename,
		bHasFilename,
		sContentType,
		iContentType,
		true
	);
}



/* 原子替换全部同名正文引用。 */
XLANG_XHTTP_FORM_DATA_EXPORT bool xhttp_form_data_set_body(
	void* pHandle,
	const char* sName,
	size_t iName,
	void* pBody,
	const char* sFilename,
	size_t iFilename,
	bool bHasFilename,
	const char* sContentType,
	size_t iContentType
)
{
	return xhttpFormDataNativeBody(
		(xformdata*)pHandle,
		sName,
		iName,
		(xhttpbody*)pBody,
		sFilename,
		iFilename,
		bHasFilename,
		sContentType,
		iContentType,
		true
	);
}



/* 删除全部同名字段并返回删除数量。 */
XLANG_XHTTP_FORM_DATA_EXPORT unsigned long long xhttp_form_data_remove(
	void* pHandle,
	const char* sName,
	size_t iName
)
{
	return (unsigned long long)xrtFormDataRemove(
		(xformdata*)pHandle,
		xhttpFormDataNativeText(sName, iName)
	);
}



/* 返回同名字段数量。 */
XLANG_XHTTP_FORM_DATA_EXPORT unsigned long long xhttp_form_data_count_name(
	void* pHandle,
	const char* sName,
	size_t iName
)
{
	return (unsigned long long)xrtFormDataCountName(
		(const xformdata*)pHandle,
		xhttpFormDataNativeText(sName, iName)
	);
}



/* 判断是否存在同名字段。 */
XLANG_XHTTP_FORM_DATA_EXPORT bool xhttp_form_data_has(
	void* pHandle,
	const char* sName,
	size_t iName
)
{
	return xrtFormDataHas(
		(const xformdata*)pHandle,
		xhttpFormDataNativeText(sName, iName)
	);
}



/* 返回指定位置的稳定 Part 快照。 */
XLANG_XHTTP_FORM_DATA_EXPORT void* xhttp_form_data_at(
	void* pHandle,
	unsigned long long iIndex
)
{
	xformdatapart Part;

	if ( (iIndex > (unsigned long long)SIZE_MAX) ||
		!xrtFormDataAt(
			(const xformdata*)pHandle,
			(size_t)iIndex,
			&Part
		) ) {
		return NULL;
	}
	return xhttpFormDataNativePartCreate(&Part);
}



/* 返回第 occurrence 个同名字段的稳定快照。 */
XLANG_XHTTP_FORM_DATA_EXPORT void* xhttp_form_data_find(
	void* pHandle,
	const char* sName,
	size_t iName,
	unsigned long long iOccurrence
)
{
	const xformdata* pForm = (const xformdata*)pHandle;
	xstrview Name = xhttpFormDataNativeText(sName, iName);
	xformdatapart Part;
	xhttpnext Next;
	size_t iIndex = 0u;
	unsigned long long i;

	for ( i = 0u; i <= iOccurrence; i++ ) {
		Next = xrtFormDataFind(pForm, Name, &iIndex, &Part);
		if ( Next != XHTTP_NEXT_ITEM ) {
			return NULL;
		}
	}
	return xhttpFormDataNativePartCreate(&Part);
}



/* 创建组合式 multipart 正文，不强制拼接为连续字节。 */
XLANG_XHTTP_FORM_DATA_EXPORT void* xhttp_form_data_body(
	void* pHandle,
	void* pBoundary
)
{
	return xrtFormDataBody(
		(const xformdata*)pHandle,
		(const xmultipartboundary*)pBoundary
	);
}



/* point 构造直接接管稳定 Part 快照。 */
XLANG_XHTTP_FORM_DATA_EXPORT void* xhttp_form_data_part_wrap(
	void* pHandle
)
{
	return pHandle;
}



/* 释放 Part 快照及其正文引用。 */
XLANG_XHTTP_FORM_DATA_EXPORT long long xhttp_form_data_part_destroy(
	void* pHandle
)
{
	xhttp_form_data_native_part* pPart =
		(xhttp_form_data_native_part*)pHandle;

	if ( pPart != NULL ) {
		xrtHttpBodyDestroy(pPart->Body);
		xrtFree(pPart->Metadata);
		memset(pPart, 0, sizeof(*pPart));
		xrtFree(pPart);
	}
	return 0;
}



/* 返回 Part 元数据的独立文本 Buffer。 */
XLANG_XHTTP_FORM_DATA_EXPORT void* xhttp_form_data_part_text(
	void* pHandle,
	long long iField
)
{
	const xhttp_form_data_native_part* pPart =
		(const xhttp_form_data_native_part*)pHandle;
	xstrview Text;

	if ( pPart == NULL ) {
		return NULL;
	}
	if ( iField == 0 ) {
		Text = (xstrview){ pPart->Metadata, pPart->NameSize };
	} else if ( iField == 1 ) {
		Text = (xstrview){
			pPart->Metadata + pPart->NameSize,
			pPart->FilenameSize
		};
	} else if ( iField == 2 ) {
		Text = (xstrview){
			pPart->Metadata + pPart->NameSize +
				pPart->FilenameSize,
			pPart->ContentTypeSize
		};
	} else {
		return NULL;
	}
	return xhttpFormDataNativeBuffer((xbytesview){
		(cbytes)Text.Data,
		Text.Size
	});
}



/* 判断 Part 是否显式包含可选元数据。 */
XLANG_XHTTP_FORM_DATA_EXPORT bool xhttp_form_data_part_has(
	void* pHandle,
	long long iField
)
{
	const xhttp_form_data_native_part* pPart =
		(const xhttp_form_data_native_part*)pHandle;

	if ( pPart == NULL ) {
		return false;
	}
	if ( iField == 1 ) {
		return (pPart->Flags & XFORM_DATA_PART_FILENAME) != 0u;
	}
	if ( iField == 2 ) {
		return (pPart->Flags & XFORM_DATA_PART_CONTENT_TYPE) != 0u;
	}
	return false;
}



/* 返回独立正文引用。 */
XLANG_XHTTP_FORM_DATA_EXPORT void* xhttp_form_data_part_body(
	void* pHandle
)
{
	const xhttp_form_data_native_part* pPart =
		(const xhttp_form_data_native_part*)pHandle;

	return pPart != NULL ? xrtHttpBodyRef(pPart->Body) : NULL;
}



/* 返回正文长度；未知或超出语言 int 时返回 -1。 */
XLANG_XHTTP_FORM_DATA_EXPORT long long xhttp_form_data_part_length(
	void* pHandle
)
{
	const xhttp_form_data_native_part* pPart =
		(const xhttp_form_data_native_part*)pHandle;

	if ( (pPart == NULL) ||
		(pPart->Length == XHTTP_BODY_UNKNOWN) ||
		(pPart->Length > (uint64)LLONG_MAX) ) {
		return -1;
	}
	return (long long)pPart->Length;
}



/* 返回 Part 标志。 */
XLANG_XHTTP_FORM_DATA_EXPORT unsigned long long xhttp_form_data_part_flags(
	void* pHandle
)
{
	const xhttp_form_data_native_part* pPart =
		(const xhttp_form_data_native_part*)pHandle;

	return pPart != NULL ? (unsigned long long)pPart->Flags : 0u;
}
