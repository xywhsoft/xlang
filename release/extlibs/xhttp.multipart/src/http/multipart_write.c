#include "../internal/xrt_http.h"




#if defined(XHTTP_FEATURE_MULTIPART_WRITE)

/* 安全累加封包长度，避免 size_t 回绕成较小容量。 */
static bool __xrtMultipartWriteSizeAdd(
	size_t* pSize,
	size_t iAdd
)
{
	if ( *pSize > (SIZE_MAX - iAdd) ) {
		__xhttpErrorSetSizeOverflow();
		return false;
	}
	*pSize += iAdd;
	return true;
}



/* 验证借用字节视图的空值一致性。 */
static bool __xrtMultipartWriteBytesValid(
	xbytesview Bytes
)
{
	return (Bytes.Data != NULL) || (Bytes.Size == 0);
}



/* 验证借用字符串视图的空值一致性。 */
static bool __xrtMultipartWriteViewValid(
	xstrview Text
)
{
	return (Text.Data != NULL) || (Text.Size == 0);
}



/* 发布 writer 的参数错误。 */
static bool __xrtMultipartWriteArgument(void)
{
	__xhttpErrorSetInvalidArgument();
	return false;
}



/* 发布 writer 的容量错误，并保留精确需求长度。 */
static bool __xrtMultipartWriteCapacity(
	size_t iRequired,
	size_t* pSize
)
{
	*pSize = iRequired;
	__xhttpErrorSetRange();
	return false;
}



/* 验证公共输出参数以及元数据和输出缓冲区互不重叠。 */
static bool __xrtMultipartWriteOutputValid(
	void* pOutput,
	size_t iCapacity,
	size_t iRequired,
	size_t* pSize
)
{
	if ( (pSize == NULL) ||
		((pOutput == NULL) && (iCapacity != 0)) ) {
		return __xrtMultipartWriteArgument();
	}
	if ( pOutput == NULL ) {
		*pSize = iRequired;
		return true;
	}
	if ( xrtMemRangesOverlap(
		pSize, sizeof(*pSize), pOutput, iRequired
	) ) {
		return __xrtMultipartWriteArgument();
	}
	if ( iCapacity < iRequired ) {
		return __xrtMultipartWriteCapacity(iRequired, pSize);
	}
	return true;
}



/* 判断输出区间是否覆盖任意借用视图。 */
static bool __xrtMultipartWriteOverlaps(
	const void* pOutput,
	size_t iRequired,
	const void* pInput,
	size_t iInput
)
{
	return xrtMemRangesOverlap(
		pOutput, iRequired, pInput, iInput
	);
}



/* 检查输出是否覆盖字段描述符或字段借用的名称和值。 */
static bool __xrtMultipartWriteFieldsOverlap(
	const xhttpfield* pFields,
	size_t iFieldCount,
	const void* pOutput,
	size_t iSize
)
{
	xhttpfield Field;
	size_t i;

	if ( xrtMemRangesOverlap(
		pFields,
		iFieldCount * sizeof(*pFields),
		pOutput,
		iSize
	) ) {
		return true;
	}
	for ( i = 0; i < iFieldCount; i++ ) {
		memcpy(&Field, pFields + i, sizeof(Field));
		if ( xrtMemRangesOverlap(
			Field.Name.Data,
			Field.Name.Size,
			pOutput,
			iSize
		) || xrtMemRangesOverlap(
			Field.Value.Data,
			Field.Value.Size,
			pOutput,
			iSize
		) ) {
			return true;
		}
	}
	return false;
}



/* 写入已确认容量足够的字节片段。 */
static void __xrtMultipartWriteBytes(
	uint8* pOutput,
	size_t* pOffset,
	const void* pData,
	size_t iSize
)
{
	if ( iSize != 0 ) {
		memcpy(pOutput + *pOffset, pData, iSize);
		*pOffset += iSize;
	}
}



/* 写入已经通过 quoted-string 测量的未转义正文。 */
static void __xrtMultipartWriteQuoted(
	uint8* pOutput,
	size_t* pOffset,
	xstrview Value
)
{
	size_t i;

	pOutput[(*pOffset)++] = (uint8)'"';
	for ( i = 0; i < Value.Size; i++ ) {
		uint8 iByte = (uint8)Value.Data[i];

		if ( (iByte == (uint8)'"') ||
			(iByte == (uint8)'\\') ) {
			pOutput[(*pOffset)++] = (uint8)'\\';
		}
		pOutput[(*pOffset)++] = iByte;
	}
	pOutput[(*pOffset)++] = (uint8)'"';
}



/* 测量 boundary delimiter 的起始行。 */
static bool __xrtMultipartWriteBoundaryMeasure(
	const xmultipartboundary* pBoundary,
	size_t* pSize
)
{
	xstrview Boundary = xrtMultipartBoundaryView(pBoundary);

	if ( Boundary.Size == 0 ) {
		return false;
	}
	*pSize = 0;
	return __xrtMultipartWriteSizeAdd(pSize, 2u) &&
		__xrtMultipartWriteSizeAdd(pSize, Boundary.Size) &&
		__xrtMultipartWriteSizeAdd(pSize, 2u);
}



/* 写出 boundary delimiter 的起始行。 */
static void __xrtMultipartWriteBoundary(
	uint8* pOutput,
	size_t* pOffset,
	const xmultipartboundary* pBoundary
)
{
	__xrtMultipartWriteBytes(
		pOutput, pOffset, "--", 2u
	);
	__xrtMultipartWriteBytes(
		pOutput, pOffset,
		pBoundary->Data, pBoundary->Size
	);
	__xrtMultipartWriteBytes(
		pOutput, pOffset, "\r\n", 2u
	);
}



/* 测量通用 Part Header。 */
static bool __xrtMultipartWritePartHeadMeasure(
	const xmultipartboundary* pBoundary,
	const xhttpfield* pFields,
	size_t iFieldCount,
	size_t* pSize
)
{
	size_t iFields;

	if ( iFieldCount > (SIZE_MAX / sizeof(*pFields)) ) {
		__xhttpErrorSetSizeOverflow();
		return false;
	}
	if ( !__xrtMultipartWriteBoundaryMeasure(
		pBoundary, pSize
	) || !xrtHttpFieldBlockWrite(
		pFields, iFieldCount, NULL, 0, &iFields
	) ) {
		return false;
	}
	return __xrtMultipartWriteSizeAdd(pSize, iFields);
}



/* 写出已验证且容量足够的通用 Part Header。 */
static bool __xrtMultipartWritePartHead(
	uint8* pOutput,
	const xmultipartboundary* pBoundary,
	const xhttpfield* pFields,
	size_t iFieldCount,
	size_t iRequired
)
{
	size_t iBoundary;
	size_t iFields;
	size_t iOffset = 0u;

	if ( !__xrtMultipartWriteBoundaryMeasure(
		pBoundary,
		&iBoundary
	) || !xrtHttpFieldBlockWrite(
		pFields,
		iFieldCount,
		pOutput + iBoundary,
		iRequired - iBoundary,
		&iFields
	) ) {
		return false;
	}
	__xrtMultipartWriteBoundary(
		pOutput, &iOffset, pBoundary
	);
	return (iOffset == iBoundary) &&
		((iBoundary + iFields) == iRequired);
}



/* 校验公共视图和 pSize 不覆盖输入元数据。 */
static bool __xrtMultipartWriteViewsValid(
	const xstrview* pViews,
	size_t iViewCount,
	const xbytesview* pBytes,
	size_t iBytesCount,
	size_t* pSize
)
{
	size_t i;

	if ( pSize == NULL ) {
		return __xrtMultipartWriteArgument();
	}
	for ( i = 0; i < iViewCount; i++ ) {
		if ( !__xrtMultipartWriteViewValid(pViews[i]) ||
			xrtMemRangesOverlap(
				pSize, sizeof(*pSize),
				pViews[i].Data, pViews[i].Size
			) ) {
			return __xrtMultipartWriteArgument();
		}
	}
	for ( i = 0; i < iBytesCount; i++ ) {
		if ( !__xrtMultipartWriteBytesValid(pBytes[i]) ||
			xrtMemRangesOverlap(
				pSize, sizeof(*pSize),
				pBytes[i].Data, pBytes[i].Size
			) ) {
			return __xrtMultipartWriteArgument();
		}
	}
	return true;
}



/* 检查输出区间与一组借用视图互不重叠。 */
static bool __xrtMultipartWriteViewsOverlap(
	const xstrview* pViews,
	size_t iViewCount,
	const xbytesview* pBytes,
	size_t iBytesCount,
	const void* pOutput,
	size_t iRequired
)
{
	size_t i;

	for ( i = 0; i < iViewCount; i++ ) {
		if ( __xrtMultipartWriteOverlaps(
			pOutput, iRequired,
			pViews[i].Data, pViews[i].Size
		) ) {
			return true;
		}
	}
	for ( i = 0; i < iBytesCount; i++ ) {
		if ( __xrtMultipartWriteOverlaps(
			pOutput, iRequired,
			pBytes[i].Data, pBytes[i].Size
		) ) {
			return true;
		}
	}
	return false;
}



/* 写出 multipart/form-data Content-Type。 */
XRT_API bool xrtMultipartContentTypeWrite(
	const xmultipartboundary* pBoundary,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	static const char Prefix[] =
		"multipart/form-data; boundary=";
	xstrview Boundary;
	size_t iQuoted;
	size_t iRequired = sizeof(Prefix) - 1u;
	size_t iOffset = 0;

	if ( pSize == NULL ) {
		return __xrtMultipartWriteArgument();
	}
	Boundary = xrtMultipartBoundaryView(pBoundary);
	if ( (Boundary.Size == 0) ||
		xrtMemRangesOverlap(
			pSize, sizeof(*pSize),
			pBoundary, sizeof(*pBoundary)
		) || !xrtHttpQuotedWrite(
			Boundary, NULL, 0, &iQuoted
		) || !__xrtMultipartWriteSizeAdd(
			&iRequired, iQuoted
		) ) {
		return false;
	}
	if ( (pOutput != NULL) && __xrtMultipartWriteOverlaps(
		pOutput, iRequired,
		pBoundary, sizeof(*pBoundary)
	) ) {
		return __xrtMultipartWriteArgument();
	}
	if ( !__xrtMultipartWriteOutputValid(
		pOutput, iCapacity, iRequired, pSize
	) ) {
		return false;
	}
	if ( pOutput == NULL ) {
		return true;
	}
	__xrtMultipartWriteBytes(
		(uint8*)pOutput, &iOffset,
		Prefix, sizeof(Prefix) - 1u
	);
	__xrtMultipartWriteQuoted(
		(uint8*)pOutput, &iOffset, Boundary
	);
	*pSize = iOffset;
	return true;
}



/* 写出原始 Part Header。 */
XRT_API bool xrtMultipartPartHeadWrite(
	const xmultipartboundary* pBoundary,
	const xhttpfield* pFields,
	size_t iFieldCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	size_t iRequired;

	if ( pSize == NULL ) {
		return __xrtMultipartWriteArgument();
	}
	if ( iFieldCount > (SIZE_MAX / sizeof(*pFields)) ) {
		__xhttpErrorSetSizeOverflow();
		return false;
	}
	if (
		xrtMemRangesOverlap(
			pSize, sizeof(*pSize),
			pBoundary, sizeof(*pBoundary)
		) || xrtMemRangesOverlap(
			pSize, sizeof(*pSize),
			pFields, iFieldCount * sizeof(*pFields)
		) || !__xrtMultipartWritePartHeadMeasure(
			pBoundary, pFields, iFieldCount, &iRequired
		) ) {
		return false;
	}
	if ( (pOutput != NULL) && (
		__xrtMultipartWriteOverlaps(
			pOutput, iRequired,
			pBoundary, sizeof(*pBoundary)
		) || __xrtMultipartWriteFieldsOverlap(
			pFields, iFieldCount, pOutput, iRequired
		)
	) ) {
		return __xrtMultipartWriteArgument();
	}
	if ( !__xrtMultipartWriteOutputValid(
		pOutput, iCapacity, iRequired, pSize
	) ) {
		return false;
	}
	if ( pOutput == NULL ) {
		return true;
	}
	if ( !__xrtMultipartWritePartHead(
		(uint8*)pOutput, pBoundary,
		pFields, iFieldCount, iRequired
	) ) {
		return false;
	}
	*pSize = iRequired;
	return true;
}



/* 写出 Part 正文后的 CRLF。 */
XRT_API bool xrtMultipartPartEndWrite(
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	if ( !__xrtMultipartWriteOutputValid(
		pOutput, iCapacity, 2u, pSize
	) ) {
		return false;
	}
	if ( pOutput != NULL ) {
		memcpy(pOutput, "\r\n", 2u);
		*pSize = 2u;
	}
	return true;
}



/* 一次写出原始 Part。 */
XRT_API bool xrtMultipartPartWrite(
	const xmultipartboundary* pBoundary,
	const xhttpfield* pFields,
	size_t iFieldCount,
	xbytesview Body,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	size_t iHead;
	size_t iRequired;
	size_t iOffset;

	if ( pSize == NULL ) {
		return __xrtMultipartWriteArgument();
	}
	if ( iFieldCount > (SIZE_MAX / sizeof(*pFields)) ) {
		__xhttpErrorSetSizeOverflow();
		return false;
	}
	if ( !__xrtMultipartWriteViewsValid(
		NULL, 0, &Body, 1, pSize
	) || xrtMemRangesOverlap(
		pSize, sizeof(*pSize),
		pBoundary, sizeof(*pBoundary)
	) || xrtMemRangesOverlap(
		pSize, sizeof(*pSize),
		pFields, iFieldCount * sizeof(*pFields)
	) || !__xrtMultipartWritePartHeadMeasure(
		pBoundary, pFields, iFieldCount, &iHead
	) ) {
		return false;
	}
	iRequired = iHead;
	if ( !__xrtMultipartWriteSizeAdd(
		&iRequired, Body.Size
	) || !__xrtMultipartWriteSizeAdd(
		&iRequired, 2u
	) ) {
		return false;
	}
	if ( (pOutput != NULL) && (
		__xrtMultipartWriteOverlaps(
			pOutput, iRequired,
			pBoundary, sizeof(*pBoundary)
		) || __xrtMultipartWriteFieldsOverlap(
			pFields, iFieldCount, pOutput, iRequired
		) || __xrtMultipartWriteOverlaps(
			pOutput, iRequired, Body.Data, Body.Size
		)
	) ) {
		return __xrtMultipartWriteArgument();
	}
	if ( !__xrtMultipartWriteOutputValid(
		pOutput, iCapacity, iRequired, pSize
	) ) {
		return false;
	}
	if ( pOutput == NULL ) {
		return true;
	}
	if ( !__xrtMultipartWritePartHead(
		(uint8*)pOutput, pBoundary,
		pFields, iFieldCount, iHead
	) ) {
		return false;
	}
	iOffset = iHead;
	__xrtMultipartWriteBytes(
		(uint8*)pOutput, &iOffset,
		Body.Data, Body.Size
	);
	__xrtMultipartWriteBytes(
		(uint8*)pOutput, &iOffset, "\r\n", 2u
	);
	*pSize = iOffset;
	return true;
}



/* 测量 form-data Content-Disposition 行。 */
static bool __xrtMultipartWriteDispositionMeasure(
	xstrview Name,
	const xstrview* pFilename,
	size_t* pSize,
	size_t* pNameQuoted,
	size_t* pFilenameQuoted
)
{
	static const char Prefix[] =
		"Content-Disposition: form-data; name=";
	static const char FilePrefix[] = "; filename=";

	if ( !xrtHttpQuotedWrite(
			Name, NULL, 0, pNameQuoted
		) ) {
		return false;
	}
	if ( !__xrtMultipartWriteSizeAdd(
		pSize, sizeof(Prefix) - 1u
	) || !__xrtMultipartWriteSizeAdd(
		pSize, *pNameQuoted
	) ) {
		return false;
	}
	if ( pFilename != NULL ) {
		if ( !xrtHttpQuotedWrite(
			*pFilename, NULL, 0, pFilenameQuoted
		) || !__xrtMultipartWriteSizeAdd(
			pSize, sizeof(FilePrefix) - 1u
		) || !__xrtMultipartWriteSizeAdd(
			pSize, *pFilenameQuoted
		) ) {
			return false;
		}
	}
	return __xrtMultipartWriteSizeAdd(pSize, 2u);
}



/* 写出 form-data Content-Disposition 行。 */
static void __xrtMultipartWriteDisposition(
	uint8* pOutput,
	size_t* pOffset,
	xstrview Name,
	const xstrview* pFilename
)
{
	static const char Prefix[] =
		"Content-Disposition: form-data; name=";
	static const char FilePrefix[] = "; filename=";

	__xrtMultipartWriteBytes(
		pOutput, pOffset, Prefix, sizeof(Prefix) - 1u
	);
	__xrtMultipartWriteQuoted(pOutput, pOffset, Name);
	if ( pFilename != NULL ) {
		__xrtMultipartWriteBytes(
			pOutput, pOffset,
			FilePrefix, sizeof(FilePrefix) - 1u
		);
		__xrtMultipartWriteQuoted(
			pOutput, pOffset, *pFilename
		);
	}
	__xrtMultipartWriteBytes(
		pOutput, pOffset, "\r\n", 2u
	);
}



/* 写出一个 form-data 普通字段 Part。 */
/* 测量标准 form-data Part 头。 */
static bool __xrtMultipartWriteFormHeadMeasure(
	const xmultipartboundary* pBoundary,
	xstrview Name,
	const xstrview* pFilename,
	xstrview ContentType,
	size_t* pRequired
)
{
	static const char TypePrefix[] = "Content-Type: ";
	xmediatype MediaType;
	size_t iNameQuoted;
	size_t iFilenameQuoted = 0;

	if ( ((ContentType.Size != 0) &&
		!xrtHttpMediaTypeParse(
			ContentType, &MediaType
		)) || !__xrtMultipartWriteBoundaryMeasure(
		pBoundary, pRequired
	) || !__xrtMultipartWriteDispositionMeasure(
		Name,
		pFilename,
		pRequired,
		&iNameQuoted,
		&iFilenameQuoted
	) ) {
		return false;
	}
	if ( ContentType.Size != 0 ) {
		if ( !__xrtMultipartWriteSizeAdd(
			pRequired, sizeof(TypePrefix) - 1u
		) || !__xrtMultipartWriteSizeAdd(
			pRequired, ContentType.Size
		) || !__xrtMultipartWriteSizeAdd(
			pRequired, 2u
		) ) {
			return false;
		}
	}
	return __xrtMultipartWriteSizeAdd(pRequired, 2u);
}



/* 向已确认容量的输出写入标准 form-data Part 头。 */
static size_t __xrtMultipartWriteFormHead(
	uint8* pOutput,
	const xmultipartboundary* pBoundary,
	xstrview Name,
	const xstrview* pFilename,
	xstrview ContentType
)
{
	static const char TypePrefix[] = "Content-Type: ";
	size_t iOffset = 0;

	__xrtMultipartWriteBoundary(
		pOutput, &iOffset, pBoundary
	);
	__xrtMultipartWriteDisposition(
		pOutput, &iOffset, Name, pFilename
	);
	if ( ContentType.Size != 0 ) {
		__xrtMultipartWriteBytes(
			pOutput, &iOffset,
			TypePrefix, sizeof(TypePrefix) - 1u
		);
		__xrtMultipartWriteBytes(
			pOutput, &iOffset,
			ContentType.Data, ContentType.Size
		);
		__xrtMultipartWriteBytes(
			pOutput, &iOffset, "\r\n", 2u
		);
	}
	__xrtMultipartWriteBytes(
		pOutput, &iOffset, "\r\n", 2u
	);
	return iOffset;
}



/* 写出标准 form-data Part 头。 */
XRT_API bool xrtMultipartFormHeadWrite(
	const xmultipartboundary* pBoundary,
	xstrview Name,
	const xstrview* pFilename,
	xstrview ContentType,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xstrview Views[3];
	size_t iViewCount = 2;
	size_t iRequired;

	Views[0] = Name;
	Views[1] = ContentType;
	if ( pFilename != NULL ) {
		Views[iViewCount++] = *pFilename;
	}
	if ( !__xrtMultipartWriteViewsValid(
		Views, iViewCount, NULL, 0, pSize
	) || xrtMemRangesOverlap(
		pSize, sizeof(*pSize),
		pBoundary, sizeof(*pBoundary)
	) || ((pFilename != NULL) && xrtMemRangesOverlap(
		pSize, sizeof(*pSize),
		pFilename, sizeof(*pFilename)
	)) || !__xrtMultipartWriteFormHeadMeasure(
		pBoundary,
		Name,
		pFilename,
		ContentType,
		&iRequired
	) ) {
		return false;
	}
	if ( (pOutput != NULL) && (
		__xrtMultipartWriteOverlaps(
			pOutput, iRequired,
			pBoundary, sizeof(*pBoundary)
		) || ((pFilename != NULL) &&
		 __xrtMultipartWriteOverlaps(
			pOutput, iRequired,
			pFilename, sizeof(*pFilename)
		)) || __xrtMultipartWriteViewsOverlap(
			Views, iViewCount, NULL, 0,
			pOutput, iRequired
		)
	) ) {
		return __xrtMultipartWriteArgument();
	}
	if ( !__xrtMultipartWriteOutputValid(
		pOutput, iCapacity, iRequired, pSize
	) ) {
		return false;
	}
	if ( pOutput == NULL ) {
		return true;
	}
	*pSize = __xrtMultipartWriteFormHead(
		(uint8*)pOutput,
		pBoundary,
		Name,
		pFilename,
		ContentType
	);
	return true;
}



/* 写出一个 form-data 普通字段 Part。 */
XRT_API bool xrtMultipartFieldWrite(
	const xmultipartboundary* pBoundary,
	xstrview Name,
	xbytesview Value,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xstrview Views[1];
	xbytesview Bytes[1];
	size_t iRequired;
	size_t iOffset;

	Views[0] = Name;
	Bytes[0] = Value;
	if ( !__xrtMultipartWriteViewsValid(
		Views, 1, Bytes, 1, pSize
	) || xrtMemRangesOverlap(
		pSize, sizeof(*pSize),
		pBoundary, sizeof(*pBoundary)
	) || !__xrtMultipartWriteFormHeadMeasure(
		pBoundary,
		Name,
		NULL,
		(xstrview){ NULL, 0 },
		&iRequired
	) || !__xrtMultipartWriteSizeAdd(
		&iRequired, Value.Size
	) || !__xrtMultipartWriteSizeAdd(
		&iRequired, 2u
	) ) {
		return false;
	}
	if ( (pOutput != NULL) && (
		__xrtMultipartWriteOverlaps(
			pOutput, iRequired,
			pBoundary, sizeof(*pBoundary)
		) || __xrtMultipartWriteViewsOverlap(
			Views, 1, Bytes, 1,
			pOutput, iRequired
		)
	) ) {
		return __xrtMultipartWriteArgument();
	}
	if ( !__xrtMultipartWriteOutputValid(
		pOutput, iCapacity, iRequired, pSize
	) ) {
		return false;
	}
	if ( pOutput == NULL ) {
		return true;
	}
	iOffset = __xrtMultipartWriteFormHead(
		(uint8*)pOutput,
		pBoundary,
		Name,
		NULL,
		(xstrview){ NULL, 0 }
	);
	__xrtMultipartWriteBytes(
		(uint8*)pOutput, &iOffset,
		Value.Data, Value.Size
	);
	__xrtMultipartWriteBytes(
		(uint8*)pOutput, &iOffset, "\r\n", 2u
	);
	*pSize = iOffset;
	return true;
}



/* 写出一个 form-data 文件 Part。 */
XRT_API bool xrtMultipartFileWrite(
	const xmultipartboundary* pBoundary,
	xstrview Name,
	xstrview Filename,
	xstrview ContentType,
	xbytesview Body,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xstrview Views[3];
	xbytesview Bytes[1];
	size_t iRequired;
	size_t iOffset;

	Views[0] = Name;
	Views[1] = Filename;
	Views[2] = ContentType;
	Bytes[0] = Body;
	if ( !__xrtMultipartWriteViewsValid(
		Views, 3, Bytes, 1, pSize
	) || xrtMemRangesOverlap(
		pSize, sizeof(*pSize),
		pBoundary, sizeof(*pBoundary)
	) || !__xrtMultipartWriteFormHeadMeasure(
		pBoundary,
		Name,
		&Filename,
		ContentType,
		&iRequired
	) ) {
		return false;
	}
	if ( !__xrtMultipartWriteSizeAdd(
		&iRequired, Body.Size
	) || !__xrtMultipartWriteSizeAdd(
		&iRequired, 2u
	) ) {
		return false;
	}
	if ( (pOutput != NULL) && (
		__xrtMultipartWriteOverlaps(
			pOutput, iRequired,
			pBoundary, sizeof(*pBoundary)
		) || __xrtMultipartWriteViewsOverlap(
			Views, 3, Bytes, 1,
			pOutput, iRequired
		)
	) ) {
		return __xrtMultipartWriteArgument();
	}
	if ( !__xrtMultipartWriteOutputValid(
		pOutput, iCapacity, iRequired, pSize
	) ) {
		return false;
	}
	if ( pOutput == NULL ) {
		return true;
	}
	iOffset = __xrtMultipartWriteFormHead(
		(uint8*)pOutput,
		pBoundary,
		Name,
		&Filename,
		ContentType
	);
	__xrtMultipartWriteBytes(
		(uint8*)pOutput, &iOffset,
		Body.Data, Body.Size
	);
	__xrtMultipartWriteBytes(
		(uint8*)pOutput, &iOffset, "\r\n", 2u
	);
	*pSize = iOffset;
	return true;
}



/* 写出关闭 boundary。 */
XRT_API bool xrtMultipartCloseWrite(
	const xmultipartboundary* pBoundary,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xstrview Boundary;
	size_t iRequired;
	size_t iOffset = 0;

	if ( pSize == NULL ) {
		return __xrtMultipartWriteArgument();
	}
	Boundary = xrtMultipartBoundaryView(pBoundary);
	if ( (Boundary.Size == 0) ||
		xrtMemRangesOverlap(
			pSize, sizeof(*pSize),
			pBoundary, sizeof(*pBoundary)
		) ) {
		return false;
	}
	iRequired = 2u;
	if ( !__xrtMultipartWriteSizeAdd(
		&iRequired, Boundary.Size
	) || !__xrtMultipartWriteSizeAdd(
		&iRequired, 4u
	) ) {
		return false;
	}
	if ( (pOutput != NULL) && __xrtMultipartWriteOverlaps(
		pOutput, iRequired,
		pBoundary, sizeof(*pBoundary)
	) ) {
		return __xrtMultipartWriteArgument();
	}
	if ( !__xrtMultipartWriteOutputValid(
		pOutput, iCapacity, iRequired, pSize
	) ) {
		return false;
	}
	if ( pOutput == NULL ) {
		return true;
	}
	__xrtMultipartWriteBytes(
		(uint8*)pOutput, &iOffset, "--", 2u
	);
	__xrtMultipartWriteBytes(
		(uint8*)pOutput, &iOffset,
		Boundary.Data, Boundary.Size
	);
	__xrtMultipartWriteBytes(
		(uint8*)pOutput, &iOffset, "--\r\n", 4u
	);
	*pSize = iOffset;
	return true;
}

#endif
