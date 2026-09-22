#include "../internal/xrt_http.h"




#if defined(XHTTP_FEATURE_MULTIPART)

/* 清空可选的 Multipart 错误位置。 */
static void __xrtMultipartErrorClear(
	xmultiparterrorinfo* pError
)
{
	const xmultiparterrorinfo Error = {
		XMULTIPART_ERROR_NONE,
		0
	};

	if ( xrtMemRangeValid(pError, sizeof(Error)) ) {
		memcpy(pError, &Error, sizeof(Error));
	}
}



/* 发布参数、语法或范围错误，并保留稳定的 Multipart 错误码。 */
static xhttpnext __xrtMultipartFail(
	xmultiparterrorinfo* pError,
	xmultiparterror Code,
	size_t iOffset,
	xerrkind Kind
)
{
	const xmultiparterrorinfo Error = {
		Code,
		iOffset
	};

	if ( xrtMemRangeValid(pError, sizeof(Error)) ) {
		memcpy(pError, &Error, sizeof(Error));
	}
	if ( Kind == XERR_ARGUMENT ) {
		__xhttpErrorSetInvalidArgument();
	} else if ( Kind == XERR_RANGE ) {
		__xhttpErrorSetRange();
	} else if ( Kind == XERR_MEMORY ) {
		__xhttpErrorSetOutOfMemory();
	} else {
		__xhttpErrorSetValue();
	}
	return XHTTP_NEXT_ERROR;
}



/* 把迭代失败转换为布尔接口的失败值。 */
static bool __xrtMultipartFailBool(
	xmultiparterrorinfo* pError,
	xmultiparterror Code,
	size_t iOffset,
	xerrkind Kind
)
{
	(void)__xrtMultipartFail(pError, Code, iOffset, Kind);
	return false;
}



/* 判断字符是否属于 RFC 2046 boundary 的 bcharsnospace 集合。 */
static bool __xrtMultipartBoundaryByte(
	uint8 iByte
)
{
	if ( ((iByte >= (uint8)'0') && (iByte <= (uint8)'9')) ||
		((iByte >= (uint8)'A') && (iByte <= (uint8)'Z')) ||
		((iByte >= (uint8)'a') && (iByte <= (uint8)'z')) ) {
		return true;
	}
	return (iByte == (uint8)'\'') ||
		(iByte == (uint8)'(') ||
		(iByte == (uint8)')') ||
		(iByte == (uint8)'+') ||
		(iByte == (uint8)'_') ||
		(iByte == (uint8)',') ||
		(iByte == (uint8)'-') ||
		(iByte == (uint8)'.') ||
		(iByte == (uint8)'/') ||
		(iByte == (uint8)':') ||
		(iByte == (uint8)'=') ||
		(iByte == (uint8)'?');
}



/* 验证拥有型 boundary 结构没有被调用方破坏。 */
bool __xrtMultipartBoundaryValid(
	const xmultipartboundary* pBoundary
)
{
	xmultipartboundary Boundary;
	size_t i;

	if ( !xrtMemRangeValid(pBoundary, sizeof(Boundary)) ) {
		return false;
	}
	memcpy(&Boundary, pBoundary, sizeof(Boundary));
	if ( (Boundary.Size == 0) ||
		(Boundary.Size > XMULTIPART_BOUNDARY_MAX) ||
		(Boundary.Data[Boundary.Size] != '\0') ||
		(Boundary.Data[Boundary.Size - 1u] == ' ') ) {
		return false;
	}
	for ( i = 0; i < Boundary.Size; i++ ) {
		uint8 iByte = (uint8)Boundary.Data[i];

		if ( (iByte != (uint8)' ') &&
			!__xrtMultipartBoundaryByte(iByte) ) {
			return false;
		}
	}
	return true;
}



/* 验证借用字节视图的空值一致性。 */
bool __xrtMultipartBytesValid(
	xbytesview Bytes
)
{
	return xrtMemRangeValid(Bytes.Data, Bytes.Size);
}



/* 解析从两个连字符开始的一条 boundary delimiter line。 */
xrt_multipart_delimiter __xrtMultipartDelimiterAt(
	xbytesview Body,
	const xmultipartboundary* pBoundary,
	size_t iDash,
	bool bEnd,
	size_t* pNext
)
{
	size_t iAvailable;
	size_t iPrefix;
	size_t iPosition;
	size_t i;
	bool bClose = false;

	if ( iDash > Body.Size ) {
		return XRT_MULTIPART_DELIMITER_INVALID;
	}
	iAvailable = Body.Size - iDash;
	iPrefix = 2u + pBoundary->Size;
	for ( i = 0; (i < iAvailable) && (i < iPrefix); i++ ) {
		uint8 iExpected = (i < 2u) ?
			(uint8)'-' :
			(uint8)pBoundary->Data[i - 2u];

		if ( Body.Data[iDash + i] != iExpected ) {
			return XRT_MULTIPART_DELIMITER_INVALID;
		}
	}
	if ( iAvailable < iPrefix ) {
		return bEnd ?
			XRT_MULTIPART_DELIMITER_INVALID :
			XRT_MULTIPART_DELIMITER_MORE;
	}
	iPosition = iDash + 2u + pBoundary->Size;
	if ( (iPosition < Body.Size) &&
		(Body.Data[iPosition] == (uint8)'-') ) {
		if ( Body.Size - iPosition < 2u ) {
			return bEnd ?
				XRT_MULTIPART_DELIMITER_INVALID :
				XRT_MULTIPART_DELIMITER_MORE;
		}
		if ( Body.Data[iPosition + 1u] != (uint8)'-' ) {
			return XRT_MULTIPART_DELIMITER_INVALID;
		}
		bClose = true;
		iPosition += 2u;
	}
	while ( (iPosition < Body.Size) &&
		((Body.Data[iPosition] == (uint8)' ') ||
		 (Body.Data[iPosition] == (uint8)'\t')) ) {
		iPosition++;
	}
	if ( iPosition == Body.Size ) {
		if ( !bEnd ) {
			return XRT_MULTIPART_DELIMITER_MORE;
		}
		if ( !bClose ) {
			return XRT_MULTIPART_DELIMITER_INVALID;
		}
		*pNext = iPosition;
		return XRT_MULTIPART_DELIMITER_CLOSE;
	}
	if ( Body.Data[iPosition] != (uint8)'\r' ) {
		return XRT_MULTIPART_DELIMITER_INVALID;
	}
	if ( Body.Size - iPosition < 2u ) {
		return bEnd ?
			XRT_MULTIPART_DELIMITER_INVALID :
			XRT_MULTIPART_DELIMITER_MORE;
	}
	if ( Body.Data[iPosition + 1u] != (uint8)'\n' ) {
		return XRT_MULTIPART_DELIMITER_INVALID;
	}
	*pNext = iPosition + 2u;
	return bClose ?
		XRT_MULTIPART_DELIMITER_CLOSE :
		XRT_MULTIPART_DELIMITER_PART;
}



/* 在可选 preamble 中查找第一条合法 boundary delimiter line。 */
static xrt_multipart_delimiter __xrtMultipartFindFirst(
	xbytesview Body,
	const xmultipartboundary* pBoundary,
	size_t* pDash,
	size_t* pNext
)
{
	xrt_multipart_delimiter Delimiter;
	size_t i;

	for ( i = 0; i < Body.Size; i++ ) {
		bool bLineStart = (i == 0) ||
			((i >= 2u) &&
			 (Body.Data[i - 2u] == (uint8)'\r') &&
			 (Body.Data[i - 1u] == (uint8)'\n'));

		if ( !bLineStart || (Body.Data[i] != (uint8)'-') ) {
			continue;
		}
		Delimiter = __xrtMultipartDelimiterAt(
			Body, pBoundary, i, true, pNext
		);
		if ( Delimiter != XRT_MULTIPART_DELIMITER_INVALID ) {
			*pDash = i;
			return Delimiter;
		}
	}
	return XRT_MULTIPART_DELIMITER_INVALID;
}



/* 查找 Part 正文后的下一条合法 delimiter，并排除附属于 delimiter 的 CRLF。 */
static xrt_multipart_delimiter __xrtMultipartFindNext(
	xbytesview Body,
	const xmultipartboundary* pBoundary,
	size_t iStart,
	size_t* pBodyEnd,
	size_t* pDash
)
{
	xrt_multipart_delimiter Delimiter;
	size_t iNext;
	size_t i;

	for ( i = iStart; (Body.Size - i) >= 4u; i++ ) {
		if ( (Body.Data[i] != (uint8)'\r') ||
			(Body.Data[i + 1u] != (uint8)'\n') ||
			(Body.Data[i + 2u] != (uint8)'-') ||
			(Body.Data[i + 3u] != (uint8)'-') ) {
			continue;
		}
		Delimiter = __xrtMultipartDelimiterAt(
			Body, pBoundary, i + 2u, true, &iNext
		);
		if ( Delimiter != XRT_MULTIPART_DELIMITER_INVALID ) {
			*pBodyEnd = i;
			*pDash = i + 2u;
			return Delimiter;
		}
	}
	return XRT_MULTIPART_DELIMITER_INVALID;
}



/* 定位 Part Header 与正文之间的空行。 */
bool __xrtMultipartHeaderBlock(
	xbytesview Body,
	size_t iStart,
	xstrview* pHeaders,
	size_t* pBodyStart
)
{
	size_t i;

	if ( (Body.Size - iStart >= 2u) &&
		(Body.Data[iStart] == (uint8)'\r') &&
		(Body.Data[iStart + 1u] == (uint8)'\n') ) {
		*pHeaders = (xstrview){
			(cstr)(Body.Data + iStart), 0
		};
		*pBodyStart = iStart + 2u;
		return true;
	}
	for ( i = iStart; (Body.Size - i) >= 4u; i++ ) {
		if ( (Body.Data[i] == (uint8)'\r') &&
			(Body.Data[i + 1u] == (uint8)'\n') &&
			(Body.Data[i + 2u] == (uint8)'\r') &&
			(Body.Data[i + 3u] == (uint8)'\n') ) {
			pHeaders->Data = (cstr)(Body.Data + iStart);
			pHeaders->Size = i - iStart;
			*pBodyStart = i + 4u;
			return true;
		}
	}
	return false;
}



/* 解析 Part Header，并拒绝会造成语义歧义的重复专用字段。 */
bool __xrtMultipartPartHeaders(
	xmultipartpart* pPart,
	xmultiparterrorinfo* pError,
	size_t iBase
)
{
	xhttpfield Field;
	xhttpnext Next;
	size_t iOffset = 0;
	size_t iCount = 0;

	for ( ;; ) {
		size_t iFieldOffset = iOffset;

		Next = xrtHttpFieldNext(
			pPart->Headers, &iOffset, &Field
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return __xrtMultipartFailBool(
				pError, XMULTIPART_ERROR_HEADER,
				iBase + iFieldOffset, XERR_VALUE
			);
		}
		if ( Next == XHTTP_NEXT_END ) {
			pPart->HeaderCount = iCount;
			return true;
		}
		if ( iCount == SIZE_MAX ) {
			return __xrtMultipartFailBool(
				pError, XMULTIPART_ERROR_HEADERS_LIMIT,
				iBase + iFieldOffset, XERR_RANGE
			);
		}
		iCount++;

		if ( xrtHttpFieldNameEqual(
			Field.Name, XRT_STR_LITERAL("Content-Disposition")
		) ) {
			if ( (pPart->Flags &
				XMULTIPART_PART_DISPOSITION) != 0 ) {
				return __xrtMultipartFailBool(
					pError, XMULTIPART_ERROR_DUPLICATE_HEADER,
					iBase + iFieldOffset, XERR_VALUE
				);
			}
			if ( !xrtHttpContentDispositionParse(
				Field.Value, &pPart->Disposition
			) ) {
				return __xrtMultipartFailBool(
					pError, XMULTIPART_ERROR_DISPOSITION,
					iBase + iFieldOffset, XERR_VALUE
				);
			}
			pPart->Flags |= XMULTIPART_PART_DISPOSITION;
		} else if ( xrtHttpFieldNameEqual(
			Field.Name, XRT_STR_LITERAL("Content-Type")
		) ) {
			if ( (pPart->Flags &
				XMULTIPART_PART_CONTENT_TYPE) != 0 ) {
				return __xrtMultipartFailBool(
					pError, XMULTIPART_ERROR_DUPLICATE_HEADER,
					iBase + iFieldOffset, XERR_VALUE
				);
			}
			if ( !xrtHttpMediaTypeParse(
				Field.Value, &pPart->ContentType
			) ) {
				return __xrtMultipartFailBool(
					pError, XMULTIPART_ERROR_CONTENT_TYPE,
					iBase + iFieldOffset, XERR_VALUE
				);
			}
			pPart->Flags |= XMULTIPART_PART_CONTENT_TYPE;
		} else if ( xrtHttpFieldNameEqual(
			Field.Name,
			XRT_STR_LITERAL("Content-Transfer-Encoding")
		) ) {
			if ( (pPart->Flags &
				XMULTIPART_PART_TRANSFER_ENCODING) != 0 ) {
				return __xrtMultipartFailBool(
					pError, XMULTIPART_ERROR_DUPLICATE_HEADER,
					iBase + iFieldOffset, XERR_VALUE
				);
			}
			if ( !xrtHttpTokenValid(Field.Value) ) {
				return __xrtMultipartFailBool(
					pError,
					XMULTIPART_ERROR_TRANSFER_ENCODING,
					iBase + iFieldOffset, XERR_VALUE
				);
			}
			pPart->TransferEncoding = Field.Value;
			pPart->Flags |=
				XMULTIPART_PART_TRANSFER_ENCODING;
		}
	}
}



/* 解析一个已经定位到普通 delimiter 后的完整 Part。 */
static bool __xrtMultipartPartParse(
	xbytesview Body,
	const xmultipartboundary* pBoundary,
	size_t iHeaderStart,
	xmultipartpart* pPart,
	size_t* pNextDash,
	xmultiparterrorinfo* pError
)
{
	xrt_multipart_delimiter Delimiter;
	size_t iBodyStart;
	size_t iBodyEnd;

	if ( !__xrtMultipartHeaderBlock(
		Body, iHeaderStart, &pPart->Headers, &iBodyStart
	) ) {
		return __xrtMultipartFailBool(
			pError, XMULTIPART_ERROR_TRUNCATED,
			iHeaderStart, XERR_VALUE
		);
	}
	if ( !__xrtMultipartPartHeaders(
		pPart, pError, iHeaderStart
	) ) {
		return false;
	}
	Delimiter = __xrtMultipartFindNext(
		Body, pBoundary, iBodyStart,
		&iBodyEnd, pNextDash
	);
	if ( Delimiter == XRT_MULTIPART_DELIMITER_INVALID ) {
		return __xrtMultipartFailBool(
			pError, XMULTIPART_ERROR_TRUNCATED,
			Body.Size, XERR_VALUE
		);
	}
	pPart->Body.Data = Body.Data + iBodyStart;
	pPart->Body.Size = iBodyEnd - iBodyStart;
	return true;
}



/* 解析并验证调用方限制配置。 */
static bool __xrtMultipartLimitsResolve(
	const xmultipartlimits* pInput,
	xmultipartlimits* pLimits
)
{
	if ( pLimits == NULL ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	xrtMultipartLimitsInit(pLimits);
	if ( pInput != NULL ) {
		if ( !xrtMemRangeValid(pInput, sizeof(*pLimits)) ) {
			__xhttpErrorSetInvalidArgument();
			return false;
		}
		memcpy(pLimits, pInput, sizeof(*pLimits));
	}
	return true;
}



/* 初始化适合一般 HTTP 表单与 MIME 内容的整包解析限制。 */
XRT_API void xrtMultipartLimitsInit(
	xmultipartlimits* pLimits
)
{
	const xmultipartlimits Limits = {
		1024u,
		64u,
		1024u,
		64u * 1024u,
		SIZE_MAX,
		SIZE_MAX
	};

	if ( !xrtMemRangeValid(pLimits, sizeof(Limits)) ) {
		__xhttpErrorSetInvalidArgument();
		return;
	}
	memcpy(pLimits, &Limits, sizeof(Limits));
}



/* 校验并复制 MIME boundary。 */
XRT_API bool xrtMultipartBoundaryParse(
	xstrview Text,
	xmultipartboundary* pBoundary
)
{
	xmultipartboundary Boundary;
	size_t i;

	if ( !xrtMemRangeValid(pBoundary, sizeof(Boundary)) ||
		!__xhttpViewValid(Text) ||
		!xrtMemRangeValid(Text.Data, Text.Size) ||
		xrtMemRangesOverlap(
			pBoundary, sizeof(Boundary), Text.Data, Text.Size
		) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	memset(&Boundary, 0, sizeof(Boundary));
	memcpy(pBoundary, &Boundary, sizeof(Boundary));
	if ( (Text.Size == 0) ||
		(Text.Size > XMULTIPART_BOUNDARY_MAX) ||
		(Text.Data[Text.Size - 1u] == ' ') ) {
		__xhttpErrorSetValue();
		return false;
	}
	for ( i = 0; i < Text.Size; i++ ) {
		uint8 iByte = (uint8)Text.Data[i];

		if ( (iByte != (uint8)' ') &&
			!__xrtMultipartBoundaryByte(iByte) ) {
			__xhttpErrorSetValue();
			return false;
		}
		Boundary.Data[i] = (char)iByte;
	}
	Boundary.Size = Text.Size;
	memcpy(pBoundary, &Boundary, sizeof(Boundary));
	return true;
}



/* 从 multipart Content-Type 解码并复制 boundary 参数。 */
XRT_API bool xrtMultipartBoundaryFromContentType(
	xstrview ContentType,
	xmultipartboundary* pBoundary
)
{
	xmultipartboundary Boundary;
	xmediatype MediaType;
	xhttpparam Param;
	xhttpnext Next;
	size_t iSize;

	if ( !xrtMemRangeValid(pBoundary, sizeof(Boundary)) ||
		!__xhttpViewValid(ContentType) ||
		!xrtMemRangeValid(ContentType.Data, ContentType.Size) ||
		xrtMemRangesOverlap(
			pBoundary, sizeof(Boundary),
			ContentType.Data, ContentType.Size
		) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	memset(&Boundary, 0, sizeof(Boundary));
	memcpy(pBoundary, &Boundary, sizeof(Boundary));
	if ( !xrtHttpMediaTypeParse(ContentType, &MediaType) ||
		!xrtHttpTokenEqual(
			MediaType.Type, XRT_STR_LITERAL("multipart")
		) ) {
		__xhttpErrorSetValue();
		return false;
	}
	Next = xrtHttpMediaTypeParam(
		&MediaType, XRT_STR_LITERAL("boundary"), &Param
	);
	if ( (Next != XHTTP_NEXT_ITEM) ||
		!xrtHttpParamValueWrite(&Param, NULL, 0, &iSize) ||
		(iSize == 0) || (iSize > XMULTIPART_BOUNDARY_MAX) ) {
		__xhttpErrorSetValue();
		return false;
	}
	if ( !xrtHttpParamValueWrite(
		&Param, Boundary.Data,
		XMULTIPART_BOUNDARY_MAX, &iSize
	) ) {
		return false;
	}
	Boundary.Size = iSize;
	if ( !__xrtMultipartBoundaryValid(&Boundary) ) {
		__xhttpErrorSetValue();
		return false;
	}
	memcpy(pBoundary, &Boundary, sizeof(Boundary));
	return true;
}



/* 返回借用边界结构的字符串视图。 */
XRT_API xstrview xrtMultipartBoundaryView(
	const xmultipartboundary* pBoundary
)
{
	xmultipartboundary Boundary;

	if ( !__xrtMultipartBoundaryValid(pBoundary) ) {
		__xhttpErrorSetInvalidArgument();
		return (xstrview){ NULL, 0 };
	}
	memcpy(&Boundary, pBoundary, sizeof(Boundary));
	return (xstrview){
		(cstr)((const uint8*)pBoundary +
			offsetof(xmultipartboundary, Data)),
		Boundary.Size
	};
}



/* 读取完整 multipart 正文中的下一项。 */
XRT_API xhttpnext xrtMultipartNext(
	xbytesview Body,
	const xmultipartboundary* pBoundary,
	size_t* pOffset,
	xmultipartpart* pPart,
	xmultiparterrorinfo* pError
)
{
	xmultipartboundary Boundary;
	xrt_multipart_delimiter Delimiter;
	xmultipartpart Part;
	xmultiparterrorinfo* pFailure = pError;
	size_t iOffset;
	size_t iDash;
	size_t iHeaderStart;
	size_t iNextDash;

	if ( (pError != NULL) && (
		!xrtMemRangeValid(pError, sizeof(*pError)) ||
		xrtMemRangesOverlap(
			pError, sizeof(*pError), Body.Data, Body.Size
		) || xrtMemRangesOverlap(
			pError, sizeof(*pError),
			pBoundary, sizeof(Boundary)
		) || xrtMemRangesOverlap(
			pError, sizeof(*pError), pOffset, sizeof(iOffset)
		) || xrtMemRangesOverlap(
			pError, sizeof(*pError), pPart, sizeof(Part)
		)
	) ) {
		pFailure = NULL;
	}
	if ( !__xrtMultipartBytesValid(Body) ||
		!__xrtMultipartBoundaryValid(pBoundary) ||
		!xrtMemRangeValid(pOffset, sizeof(iOffset)) ||
		!xrtMemRangeValid(pPart, sizeof(Part)) ||
		(pFailure != pError) ||
		xrtMemRangesOverlap(
			pOffset, sizeof(iOffset), Body.Data, Body.Size
		) || xrtMemRangesOverlap(
			pOffset, sizeof(iOffset),
			pBoundary, sizeof(Boundary)
		) || xrtMemRangesOverlap(
			pPart, sizeof(Part), Body.Data, Body.Size
		) || xrtMemRangesOverlap(
			pPart, sizeof(Part),
			pBoundary, sizeof(Boundary)
		) || xrtMemRangesOverlap(
			pOffset, sizeof(iOffset), pPart, sizeof(Part)
		) ) {
		return __xrtMultipartFail(
			pFailure, XMULTIPART_ERROR_ARGUMENT,
			0, XERR_ARGUMENT
		);
	}
	memcpy(&Boundary, pBoundary, sizeof(Boundary));
	memcpy(&iOffset, pOffset, sizeof(iOffset));
	if ( iOffset > Body.Size ) {
		return __xrtMultipartFail(
			pError, XMULTIPART_ERROR_ARGUMENT,
			0, XERR_ARGUMENT
		);
	}
	memset(&Part, 0, sizeof(Part));
	memcpy(pPart, &Part, sizeof(Part));
	__xrtMultipartErrorClear(pError);
	if ( iOffset == Body.Size ) {
		if ( Body.Size == 0 ) {
			return __xrtMultipartFail(
				pError, XMULTIPART_ERROR_TRUNCATED,
				0, XERR_VALUE
			);
		}
		return XHTTP_NEXT_END;
	}

	if ( iOffset == 0 ) {
		Delimiter = __xrtMultipartFindFirst(
			Body, &Boundary, &iDash, &iHeaderStart
		);
		if ( Delimiter == XRT_MULTIPART_DELIMITER_INVALID ) {
			return __xrtMultipartFail(
				pError, XMULTIPART_ERROR_DELIMITER,
				Body.Size, XERR_VALUE
			);
		}
		if ( Delimiter == XRT_MULTIPART_DELIMITER_CLOSE ) {
			iOffset = Body.Size;
			memcpy(pOffset, &iOffset, sizeof(iOffset));
			return XHTTP_NEXT_END;
		}
	} else {
		iDash = iOffset;
		Delimiter = __xrtMultipartDelimiterAt(
			Body, &Boundary, iDash, true, &iHeaderStart
		);
		if ( Delimiter == XRT_MULTIPART_DELIMITER_INVALID ) {
			return __xrtMultipartFail(
				pError, XMULTIPART_ERROR_DELIMITER,
				iDash, XERR_VALUE
			);
		}
		if ( Delimiter == XRT_MULTIPART_DELIMITER_CLOSE ) {
			iOffset = Body.Size;
			memcpy(pOffset, &iOffset, sizeof(iOffset));
			return XHTTP_NEXT_END;
		}
	}

	if ( !__xrtMultipartPartParse(
		Body, &Boundary, iHeaderStart,
		&Part, &iNextDash, pError
	) ) {
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pPart, &Part, sizeof(Part));
	memcpy(pOffset, &iNextDash, sizeof(iNextDash));
	return XHTTP_NEXT_ITEM;
}



/* 从 Header 起点反向定位紧邻的 delimiter 行首。 */
static size_t __xrtMultipartDelimiterStart(
	xbytesview Body,
	size_t iHeader
)
{
	size_t i;

	if ( iHeader < 2u ) {
		return 0;
	}
	i = iHeader - 2u;
	while ( i >= 2u ) {
		if ( (Body.Data[i - 2u] == (uint8)'\r') &&
			(Body.Data[i - 1u] == (uint8)'\n') ) {
			return i;
		}
		i--;
	}
	return 0;
}



/* 按限制验证完整 multipart 正文，并按标志决定是否接受零 Part。 */
XRT_API bool xrtMultipartValidateCount(
	xbytesview Body,
	const xmultipartboundary* pBoundary,
	const xmultipartlimits* pInputLimits,
	uint32 Flags,
	size_t* pPartCount,
	xmultiparterrorinfo* pError
)
{
	xmultipartboundary Boundary;
	xmultipartlimits Limits;
	xmultipartpart Part;
	xmultiparterrorinfo* pFailure = pError;
	xhttpnext Next;
	size_t iOffset = 0;
	size_t iBefore;
	size_t iCount = 0;

	if ( ((Flags & ~((uint32)XMULTIPART_VALIDATE_ALLOW_EMPTY)) != 0u) ||
		(pPartCount == NULL) || ((pError != NULL) && (
		!xrtMemRangeValid(pError, sizeof(*pError)) ||
		xrtMemRangesOverlap(
			pError, sizeof(*pError), Body.Data, Body.Size
		) || xrtMemRangesOverlap(
			pError, sizeof(*pError),
			pBoundary, sizeof(Boundary)
		) || ((pInputLimits != NULL) && xrtMemRangesOverlap(
			pError, sizeof(*pError),
			pInputLimits, sizeof(Limits)
		))
	)) ) {
		pFailure = NULL;
	}
	if ( (pPartCount == NULL) ||
		!__xrtMultipartBytesValid(Body) ||
		!__xrtMultipartBoundaryValid(pBoundary) ||
		(pFailure != pError) ||
		!__xrtMultipartLimitsResolve(pInputLimits, &Limits) ) {
		return __xrtMultipartFailBool(
			pFailure, XMULTIPART_ERROR_ARGUMENT,
			0, XERR_ARGUMENT
		);
	}
	memcpy(&Boundary, pBoundary, sizeof(Boundary));
	*pPartCount = 0;
	__xrtMultipartErrorClear(pError);
	if ( Body.Size > Limits.MaxBodyBytes ) {
		return __xrtMultipartFailBool(
			pError, XMULTIPART_ERROR_BODY_BYTES_LIMIT,
			Limits.MaxBodyBytes, XERR_RANGE
		);
	}
	for ( ;; ) {
		iBefore = iOffset;
		Next = xrtMultipartNext(
			Body, &Boundary, &iOffset, &Part, pError
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			if ( iCount == 0 ) {
				xrt_multipart_delimiter Delimiter;
				size_t iDash = 0;
				size_t iAfter;

				if ( (Flags & XMULTIPART_VALIDATE_ALLOW_EMPTY) == 0u ) {
					return __xrtMultipartFailBool(
						pError, XMULTIPART_ERROR_DELIMITER,
						0, XERR_VALUE
					);
				}
				Delimiter = __xrtMultipartFindFirst(
					Body, &Boundary, &iDash, &iAfter
				);
				if ( (Delimiter != XRT_MULTIPART_DELIMITER_CLOSE) ||
					((iAfter - iDash) >
					 Limits.MaxDelimiterBytes) ) {
					return __xrtMultipartFailBool(
						pError,
						XMULTIPART_ERROR_DELIMITER_BYTES_LIMIT,
						iDash, XERR_RANGE
					);
				}
				return true;
			}
			{
				xrt_multipart_delimiter Delimiter;
				size_t iAfter;

				Delimiter = __xrtMultipartDelimiterAt(
					Body, &Boundary, iBefore, true, &iAfter
				);
				if ( (Delimiter !=
						XRT_MULTIPART_DELIMITER_CLOSE) ||
					((iAfter - iBefore) >
						Limits.MaxDelimiterBytes) ) {
					return __xrtMultipartFailBool(
						pError,
						XMULTIPART_ERROR_DELIMITER_BYTES_LIMIT,
						iBefore, XERR_RANGE
					);
				}
			}
			*pPartCount = iCount;
			return true;
		}
		{
			size_t iHeader = (size_t)(
				(const uint8*)Part.Headers.Data - Body.Data
			);
			size_t iDelimiter =
				__xrtMultipartDelimiterStart(Body, iHeader);

			if ( (iHeader - iDelimiter) >
				Limits.MaxDelimiterBytes ) {
				return __xrtMultipartFailBool(
					pError,
					XMULTIPART_ERROR_DELIMITER_BYTES_LIMIT,
					iDelimiter, XERR_RANGE
				);
			}
		}
		if ( iCount >= Limits.MaxParts ) {
			return __xrtMultipartFailBool(
				pError, XMULTIPART_ERROR_PARTS_LIMIT,
				(size_t)(
					(const uint8*)Part.Headers.Data -
					Body.Data
				), XERR_RANGE
			);
		}
		if ( Part.HeaderCount > Limits.MaxHeaders ) {
			return __xrtMultipartFailBool(
				pError, XMULTIPART_ERROR_HEADERS_LIMIT,
				(size_t)(
					(const uint8*)Part.Headers.Data -
					Body.Data
				), XERR_RANGE
			);
		}
		if ( Part.Headers.Size > Limits.MaxHeaderBytes ) {
			return __xrtMultipartFailBool(
				pError, XMULTIPART_ERROR_HEADER_BYTES_LIMIT,
				(size_t)(
					(const uint8*)Part.Headers.Data -
					Body.Data
				), XERR_RANGE
			);
		}
		if ( Part.Body.Size > Limits.MaxPartBytes ) {
			return __xrtMultipartFailBool(
				pError, XMULTIPART_ERROR_PART_BYTES_LIMIT,
				(size_t)(Part.Body.Data - Body.Data),
				XERR_RANGE
			);
		}
		iCount++;
	}
}



/* 按限制严格验证至少包含一个 Part 的完整 multipart 正文。 */
XRT_API bool xrtMultipartValidate(
	xbytesview Body,
	const xmultipartboundary* pBoundary,
	const xmultipartlimits* pLimits,
	xmultiparterrorinfo* pError
)
{
	size_t iPartCount;

	return xrtMultipartValidateCount(
		Body,
		pBoundary,
		pLimits,
		XMULTIPART_VALIDATE_NONE,
		&iPartCount,
		pError
	);
}



/* 解析完整 multipart 正文到调用方数组，并保证容量失败不产生部分结果。 */
XRT_API bool xrtMultipartParse(
	xbytesview Body,
	const xmultipartboundary* pBoundary,
	xmultipartpart* pParts,
	size_t iCapacity,
	size_t* pCount,
	const xmultipartlimits* pLimits,
	xmultiparterrorinfo* pError
)
{
	xmultipartboundary Boundary;
	xmultipartpart Part;
	xmultiparterrorinfo* pFailure = pError;
	xhttpnext Next;
	size_t iOffset = 0;
	size_t iRequired = 0;
	size_t iBytes;

	if ( (pError != NULL) && (
		!xrtMemRangeValid(pError, sizeof(*pError)) ||
		xrtMemRangesOverlap(
			pError, sizeof(*pError), Body.Data, Body.Size
		) || xrtMemRangesOverlap(
			pError, sizeof(*pError),
			pBoundary, sizeof(Boundary)
		) || xrtMemRangesOverlap(
			pError, sizeof(*pError), pCount, sizeof(iRequired)
		) || ((pLimits != NULL) && xrtMemRangesOverlap(
			pError, sizeof(*pError),
			pLimits, sizeof(*pLimits)
		))
	) ) {
		pFailure = NULL;
	}
	if ( iCapacity > (SIZE_MAX / sizeof(Part)) ) {
		return __xrtMultipartFailBool(
			pFailure, XMULTIPART_ERROR_ARGUMENT,
			0, XERR_ARGUMENT
		);
	}
	iBytes = iCapacity * sizeof(Part);
	if ( (pError != NULL) && (pParts != NULL) &&
		xrtMemRangesOverlap(
			pError, sizeof(*pError), pParts, iBytes
		) ) {
		pFailure = NULL;
	}
	if ( !xrtMemRangeValid(pCount, sizeof(iRequired)) ||
		((pParts == NULL) && (iCapacity != 0)) ||
		((pParts != NULL) && !xrtMemRangeValid(pParts, iBytes)) ||
		!__xrtMultipartBytesValid(Body) ||
		!__xrtMultipartBoundaryValid(pBoundary) ||
		(pFailure != pError) || xrtMemRangesOverlap(
		pCount, sizeof(*pCount), Body.Data, Body.Size
	) || xrtMemRangesOverlap(
		pCount, sizeof(*pCount),
		pBoundary, sizeof(Boundary)
	) || ((pLimits != NULL) && (
		xrtMemRangesOverlap(
			pCount, sizeof(*pCount),
			pLimits, sizeof(*pLimits)
		) || ((pError != NULL) && xrtMemRangesOverlap(
			pError, sizeof(*pError),
			pLimits, sizeof(*pLimits)
		))
	)) || ((pParts != NULL) && (
		xrtMemRangesOverlap(
			pParts, iBytes, Body.Data, Body.Size
		) || xrtMemRangesOverlap(
			pParts, iBytes, pBoundary, sizeof(*pBoundary)
		) || xrtMemRangesOverlap(
			pParts, iBytes, pCount, sizeof(*pCount)
		) || ((pLimits != NULL) && xrtMemRangesOverlap(
			pParts, iBytes, pLimits, sizeof(*pLimits)
		)) || ((pError != NULL) && xrtMemRangesOverlap(
			pParts, iBytes, pError, sizeof(*pError)
		))
	)) ) {
		return __xrtMultipartFailBool(
			pFailure, XMULTIPART_ERROR_ARGUMENT,
			0, XERR_ARGUMENT
		);
	}
	memcpy(&Boundary, pBoundary, sizeof(Boundary));
	memcpy(pCount, &iRequired, sizeof(iRequired));
	if ( !xrtMultipartValidate(
		Body, &Boundary, pLimits, pError
	) ) {
		return false;
	}
	for ( ;; ) {
		Next = xrtMultipartNext(
			Body, &Boundary, &iOffset, &Part, pError
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			break;
		}
		if ( iRequired == SIZE_MAX ) {
			return __xrtMultipartFailBool(
				pError, XMULTIPART_ERROR_PARTS_LIMIT,
				iOffset, XERR_RANGE
			);
		}
		iRequired++;
	}
	if ( (pParts == NULL) || (iCapacity < iRequired) ) {
		memcpy(pCount, &iRequired, sizeof(iRequired));
		if ( pParts == NULL ) {
			return true;
		}
		__xhttpErrorSetRange();
		return false;
	}

	iOffset = 0;
	iRequired = 0;
	for ( ;; ) {
		Next = xrtMultipartNext(
			Body, &Boundary, &iOffset,
			&Part, pError
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			memset(pParts, 0, iBytes);
			iRequired = 0;
			memcpy(pCount, &iRequired, sizeof(iRequired));
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			memcpy(pCount, &iRequired, sizeof(iRequired));
			return true;
		}
		memcpy(
			((uint8*)pParts) + (iRequired * sizeof(Part)),
			&Part,
			sizeof(Part)
		);
		iRequired++;
	}
}



/* 验证 RFC 7578 form-data Part 的必要结构。 */
XRT_API bool xrtMultipartFormPartValid(
	const xmultipartpart* pPart
)
{
	xmultipartpart Part;

	if ( !xrtMemRangeValid(pPart, sizeof(Part)) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Part, pPart, sizeof(Part));
	if ( ((Part.Flags & ~((uint32)(
			XMULTIPART_PART_DISPOSITION |
			XMULTIPART_PART_CONTENT_TYPE |
			XMULTIPART_PART_TRANSFER_ENCODING
		))) != 0) ||
		((Part.Flags & XMULTIPART_PART_DISPOSITION) == 0) ||
		!xrtHttpTokenEqual(
			Part.Disposition.Type,
			XRT_STR_LITERAL("form-data")
		) || ((Part.Disposition.Flags &
			XCONTENT_DISPOSITION_NAME) == 0) ) {
		__xhttpErrorSetValue();
		return false;
	}
	return true;
}



/* 读取 form-data 字段名。 */
XRT_API bool xrtMultipartPartNameWrite(
	const xmultipartpart* pPart,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xmultipartpart Part;
	size_t iSize = 0;
	bool bResult;

	if ( !xrtMemRangeValid(pPart, sizeof(Part)) ||
		!xrtMemRangeValid(pSize, sizeof(*pSize)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		!xrtMemRangeValid(pOutput, iCapacity) ||
		xrtMemRangesOverlap(
			pSize, sizeof(*pSize), pOutput, iCapacity
		) ||
		xrtMemRangesOverlap(
			pPart, sizeof(Part), pSize, sizeof(*pSize)
		) || ((pOutput != NULL) && xrtMemRangesOverlap(
			pPart, sizeof(Part), pOutput, iCapacity
		)) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Part, pPart, sizeof(Part));
	if ( !xrtMultipartFormPartValid(&Part) ) {
		return false;
	}
	bResult = xrtHttpParamValueWrite(
		&Part.Disposition.Name,
		pOutput, iCapacity, &iSize
	);
	memcpy(pSize, &iSize, sizeof(iSize));
	return bResult;
}



/* 读取上传文件名。 */
XRT_API bool xrtMultipartPartFileNameWrite(
	const xmultipartpart* pPart,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xmultipartpart Part;
	size_t iSize = 0;
	bool bResult;

	if ( !xrtMemRangeValid(pPart, sizeof(Part)) ||
		!xrtMemRangeValid(pSize, sizeof(*pSize)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		!xrtMemRangeValid(pOutput, iCapacity) ||
		xrtMemRangesOverlap(
			pSize, sizeof(*pSize), pOutput, iCapacity
		) ||
		xrtMemRangesOverlap(
			pPart, sizeof(Part), pSize, sizeof(*pSize)
		) || ((pOutput != NULL) && xrtMemRangesOverlap(
			pPart, sizeof(Part), pOutput, iCapacity
		)) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Part, pPart, sizeof(Part));
	if ( !xrtMultipartFormPartValid(&Part) ) {
		return false;
	}
	bResult = xrtHttpContentDispositionFileNameWrite(
		&Part.Disposition,
		pOutput, iCapacity, &iSize
	);
	memcpy(pSize, &iSize, sizeof(iSize));
	return bResult;
}

#endif
