#include "xhttp_multipart_native_internal.h"



/* 只需要 boundary 的 writer 上下文。 */
typedef struct xhttp_multipart_native_boundary_write {
	const xmultipartboundary* Boundary;
	bool Close;
} xhttp_multipart_native_boundary_write;



/* 原始 Header 数组 writer 上下文。 */
typedef struct xhttp_multipart_native_part_write {
	const xmultipartboundary* Boundary;
	const xhttpfield* Fields;
	size_t FieldCount;
	xbytesview Body;
	bool Complete;
} xhttp_multipart_native_part_write;



/* form-data 头部 writer 上下文。 */
typedef struct xhttp_multipart_native_form_head_write {
	const xmultipartboundary* Boundary;
	xstrview Name;
	xstrview Filename;
	xstrview ContentType;
	bool HasFilename;
} xhttp_multipart_native_form_head_write;



/* 普通字段 writer 上下文。 */
typedef struct xhttp_multipart_native_field_write {
	const xmultipartboundary* Boundary;
	xstrview Name;
	xbytesview Value;
} xhttp_multipart_native_field_write;



/* 文件字段 writer 上下文。 */
typedef struct xhttp_multipart_native_file_write {
	const xmultipartboundary* Boundary;
	xstrview Name;
	xstrview Filename;
	xstrview ContentType;
	xbytesview Body;
} xhttp_multipart_native_file_write;



/* 写出 Content-Type 或关闭 boundary。 */
static bool xhttpMultipartNativeBoundaryWrite(
	const void* pContext,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	const xhttp_multipart_native_boundary_write* pWrite =
		(const xhttp_multipart_native_boundary_write*)pContext;

	return pWrite->Close ?
		xrtMultipartCloseWrite(
			pWrite->Boundary, pOutput, iCapacity, pSize
		) :
		xrtMultipartContentTypeWrite(
			pWrite->Boundary, pOutput, iCapacity, pSize
		);
}



/* 写出原始 Part 头或完整 Part。 */
static bool xhttpMultipartNativePartWrite(
	const void* pContext,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	const xhttp_multipart_native_part_write* pWrite =
		(const xhttp_multipart_native_part_write*)pContext;

	return pWrite->Complete ?
		xrtMultipartPartWrite(
			pWrite->Boundary,
			pWrite->Fields,
			pWrite->FieldCount,
			pWrite->Body,
			pOutput,
			iCapacity,
			pSize
		) :
		xrtMultipartPartHeadWrite(
			pWrite->Boundary,
			pWrite->Fields,
			pWrite->FieldCount,
			pOutput,
			iCapacity,
			pSize
		);
}



/* 写出 form-data Part 头。 */
static bool xhttpMultipartNativeFormHeadWrite(
	const void* pContext,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	const xhttp_multipart_native_form_head_write* pWrite =
		(const xhttp_multipart_native_form_head_write*)pContext;
	const xstrview* pFilename = pWrite->HasFilename ?
		&pWrite->Filename : NULL;

	return xrtMultipartFormHeadWrite(
		pWrite->Boundary,
		pWrite->Name,
		pFilename,
		pWrite->ContentType,
		pOutput,
		iCapacity,
		pSize
	);
}



/* 写出普通 form-data 字段。 */
static bool xhttpMultipartNativeFieldWrite(
	const void* pContext,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	const xhttp_multipart_native_field_write* pWrite =
		(const xhttp_multipart_native_field_write*)pContext;

	return xrtMultipartFieldWrite(
		pWrite->Boundary,
		pWrite->Name,
		pWrite->Value,
		pOutput,
		iCapacity,
		pSize
	);
}



/* 写出完整 form-data 文件字段。 */
static bool xhttpMultipartNativeFileWrite(
	const void* pContext,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	const xhttp_multipart_native_file_write* pWrite =
		(const xhttp_multipart_native_file_write*)pContext;

	return xrtMultipartFileWrite(
		pWrite->Boundary,
		pWrite->Name,
		pWrite->Filename,
		pWrite->ContentType,
		pWrite->Body,
		pOutput,
		iCapacity,
		pSize
	);
}



/* 写出固定的 Part 结束 CRLF。 */
static bool xhttpMultipartNativePartEndWrite(
	const void* pContext,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	(void)pContext;
	return xrtMultipartPartEndWrite(pOutput, iCapacity, pSize);
}



/* 把 Headers 容器快照成调用期间有效的字段描述符数组。 */
static xhttpfield* xhttpMultipartNativeFields(
	const xhttpheaders* pHeaders,
	size_t* pCount
)
{
	xhttpfield* pFields;
	size_t iCount;
	size_t i;

	if ( (pHeaders == NULL) || (pCount == NULL) ) {
		xhttpMultipartNativeError(
			XERR_ARGUMENT,
			20,
			"read-writer-headers",
			"multipart Headers value is required"
		);
		return NULL;
	}
	iCount = xrtHttpHeadersCount(pHeaders);
	*pCount = iCount;
	if ( iCount == 0u ) {
		return (xhttpfield*)pHeaders;
	}
	if ( iCount > (SIZE_MAX / sizeof(*pFields)) ) {
		xhttpMultipartNativeError(
			XERR_RANGE,
			21,
			"read-writer-headers",
			"multipart Header descriptor count is too large"
		);
		return NULL;
	}
	pFields = (xhttpfield*)xrtMalloc(iCount * sizeof(*pFields));
	if ( pFields == NULL ) {
		return NULL;
	}
	for ( i = 0u; i < iCount; i++ ) {
		const xhttpfield* pField = xrtHttpHeadersAt(pHeaders, i);

		if ( pField == NULL ) {
			xrtFree(pFields);
			return NULL;
		}
		memcpy(&pFields[i], pField, sizeof(pFields[i]));
	}
	return pFields;
}



/* 构建 multipart Content-Type。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_writer_content_type(
	const void* pBoundaryHandle
)
{
	xhttp_multipart_native_boundary_write Write = {
		(const xmultipartboundary*)pBoundaryHandle,
		false
	};

	if ( pBoundaryHandle == NULL ) {
		return NULL;
	}
	return xhttpMultipartNativeBuild(
		xhttpMultipartNativeBoundaryWrite, &Write
	);
}



/* 构建不带 filename 的 form-data Part 头。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_writer_field_head(
	const void* pBoundaryHandle,
	const char* sName,
	size_t iName
)
{
	xhttp_multipart_native_form_head_write Write = {
		(const xmultipartboundary*)pBoundaryHandle,
		{ sName, iName },
		{ NULL, 0u },
		{ NULL, 0u },
		false
	};

	if ( pBoundaryHandle == NULL ) {
		return NULL;
	}
	return xhttpMultipartNativeBuild(
		xhttpMultipartNativeFormHeadWrite, &Write
	);
}



/* 构建带 filename 和可选媒体类型的 form-data Part 头。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_writer_file_head(
	const void* pBoundaryHandle,
	const char* sName,
	size_t iName,
	const char* sFilename,
	size_t iFilename,
	const char* sContentType,
	size_t iContentType
)
{
	xhttp_multipart_native_form_head_write Write = {
		(const xmultipartboundary*)pBoundaryHandle,
		{ sName, iName },
		{ sFilename, iFilename },
		{ sContentType, iContentType },
		true
	};

	if ( pBoundaryHandle == NULL ) {
		return NULL;
	}
	return xhttpMultipartNativeBuild(
		xhttpMultipartNativeFormHeadWrite, &Write
	);
}



/* 构建原始 Header Part 头。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_writer_part_head(
	const void* pBoundaryHandle,
	const void* pHeadersHandle
)
{
	xhttp_multipart_native_part_write Write;
	xhttpfield* pFields;
	xbuffer* pResult;

	if ( pBoundaryHandle == NULL ) {
		return NULL;
	}
	pFields = xhttpMultipartNativeFields(
		(const xhttpheaders*)pHeadersHandle, &Write.FieldCount
	);
	if ( pFields == NULL ) {
		return NULL;
	}
	Write.Boundary = (const xmultipartboundary*)pBoundaryHandle;
	Write.Fields = Write.FieldCount != 0u ? pFields : NULL;
	Write.Body = (xbytesview){ NULL, 0u };
	Write.Complete = false;
	pResult = xhttpMultipartNativeBuild(
		xhttpMultipartNativePartWrite, &Write
	);
	if ( Write.FieldCount != 0u ) {
		xrtFree(pFields);
	}
	return pResult;
}



/* 构建 Part 正文结束 CRLF。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_writer_part_end(void)
{
	return xhttpMultipartNativeBuild(
		xhttpMultipartNativePartEndWrite, NULL
	);
}



/* 构建完整普通 form-data 字段。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_writer_field(
	const void* pBoundaryHandle,
	const char* sName,
	size_t iName,
	const void* pValue,
	size_t iValue
)
{
	xhttp_multipart_native_field_write Write = {
		(const xmultipartboundary*)pBoundaryHandle,
		{ sName, iName },
		{ (cbytes)pValue, iValue }
	};

	if ( pBoundaryHandle == NULL ) {
		return NULL;
	}
	return xhttpMultipartNativeBuild(
		xhttpMultipartNativeFieldWrite, &Write
	);
}



/* 语言字符串字段复用字节优先写出路径。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_writer_field_text(
	const void* pBoundaryHandle,
	const char* sName,
	size_t iName,
	const char* sValue,
	size_t iValue
)
{
	return xhttp_multipart_writer_field(
		pBoundaryHandle,
		sName,
		iName,
		sValue,
		iValue
	);
}



/* 构建完整 form-data 文件字段。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_writer_file(
	const void* pBoundaryHandle,
	const char* sName,
	size_t iName,
	const char* sFilename,
	size_t iFilename,
	const char* sContentType,
	size_t iContentType,
	const void* pBody,
	size_t iBody
)
{
	xhttp_multipart_native_file_write Write = {
		(const xmultipartboundary*)pBoundaryHandle,
		{ sName, iName },
		{ sFilename, iFilename },
		{ sContentType, iContentType },
		{ (cbytes)pBody, iBody }
	};

	if ( pBoundaryHandle == NULL ) {
		return NULL;
	}
	return xhttpMultipartNativeBuild(
		xhttpMultipartNativeFileWrite, &Write
	);
}



/* 构建带原始 Headers 的完整 Part。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_writer_part(
	const void* pBoundaryHandle,
	const void* pHeadersHandle,
	const void* pBody,
	size_t iBody
)
{
	xhttp_multipart_native_part_write Write;
	xhttpfield* pFields;
	xbuffer* pResult;

	if ( pBoundaryHandle == NULL ) {
		return NULL;
	}
	pFields = xhttpMultipartNativeFields(
		(const xhttpheaders*)pHeadersHandle, &Write.FieldCount
	);
	if ( pFields == NULL ) {
		return NULL;
	}
	Write.Boundary = (const xmultipartboundary*)pBoundaryHandle;
	Write.Fields = Write.FieldCount != 0u ? pFields : NULL;
	Write.Body = (xbytesview){ (cbytes)pBody, iBody };
	Write.Complete = true;
	pResult = xhttpMultipartNativeBuild(
		xhttpMultipartNativePartWrite, &Write
	);
	if ( Write.FieldCount != 0u ) {
		xrtFree(pFields);
	}
	return pResult;
}



/* 构建关闭 boundary。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_writer_close(
	const void* pBoundaryHandle
)
{
	xhttp_multipart_native_boundary_write Write = {
		(const xmultipartboundary*)pBoundaryHandle,
		true
	};

	if ( pBoundaryHandle == NULL ) {
		return NULL;
	}
	return xhttpMultipartNativeBuild(
		xhttpMultipartNativeBoundaryWrite, &Write
	);
}
