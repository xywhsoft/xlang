#include "../internal/xrt_http.h"



#if defined(XHTTP_FEATURE_MULTIPART_STREAM)

#define XRT_MULTIPART_READER_START     1u
#define XRT_MULTIPART_READER_HEADERS   2u
#define XRT_MULTIPART_READER_BODY      3u
#define XRT_MULTIPART_READER_EPILOGUE  4u
#define XRT_MULTIPART_READER_DONE      5u
#define XRT_MULTIPART_READER_ERROR     6u



/* 清空可选的流解析错误位置。 */
static void __xrtMultipartReaderErrorClear(
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



/* 发布流解析错误并把 Reader 固定在终止状态。 */
static xmultipartreadstatus __xrtMultipartReaderFail(
	xmultipartreader* pReader,
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

	if ( pReader != NULL ) {
		pReader->State = XRT_MULTIPART_READER_ERROR;
		pReader->Error = Code;
		pReader->ErrorOffset = iOffset;
	}
	if ( xrtMemRangeValid(pError, sizeof(Error)) ) {
		memcpy(pError, &Error, sizeof(Error));
	}
	if ( Kind == XERR_ARGUMENT ) {
		__xhttpErrorSetInvalidArgument();
	} else if ( Kind == XERR_RANGE ) {
		__xhttpErrorSetRange();
	} else {
		__xhttpErrorSetValue();
	}
	return XMULTIPART_READ_ERROR;
}



/* 安全累加 Reader 的线缆消费量。 */
static xmultipartreadstatus __xrtMultipartReaderReturn(
	xmultipartreader* pReader,
	size_t iConsumed,
	size_t* pConsumed,
	xmultiparterrorinfo* pError,
	xmultipartreadstatus Status
)
{
	if ( iConsumed >
		(pReader->Limits.MaxBodyBytes - pReader->WireBytes) ) {
		return __xrtMultipartReaderFail(
			pReader, pError,
			XMULTIPART_ERROR_BODY_BYTES_LIMIT,
			pReader->WireBytes, XERR_RANGE
		);
	}
	pReader->WireBytes += iConsumed;
	memcpy(pConsumed, &iConsumed, sizeof(iConsumed));
	return Status;
}



/* 验证 Reader 本体自然对齐并完整可访问。 */
static bool __xrtMultipartReaderPointerValid(
	const xmultipartreader* pReader
)
{
	size_t iAlignment;

	#if defined(_MSC_VER)
		iAlignment = __alignof(xmultipartreader);
	#elif defined(__GNUC__) || defined(__clang__) || defined(__TINYC__)
		iAlignment = __alignof__(xmultipartreader);
	#else
		iAlignment = _Alignof(xmultipartreader);
	#endif
	#if defined(__TINYC__) && defined(__i386__)
		if ( iAlignment > sizeof(void*) ) {
			iAlignment = sizeof(void*);
		}
	#endif
	return xrtMemRangeValid(pReader, sizeof(*pReader)) &&
		(((uintptr_t)pReader % iAlignment) == 0);
}



/* 验证公开 Reader 没有破坏状态、计数和限制不变量。 */
static bool __xrtMultipartReaderValid(
	const xmultipartreader* pReader
)
{
	if ( !__xrtMultipartReaderPointerValid(pReader) ||
		!__xrtMultipartBoundaryValid(&pReader->Boundary) ||
		(pReader->State < XRT_MULTIPART_READER_START) ||
		(pReader->State > XRT_MULTIPART_READER_ERROR) ||
		(pReader->Parts > pReader->Limits.MaxParts) ||
		(pReader->PartBytes > pReader->Limits.MaxPartBytes) ||
		(pReader->WireBytes > pReader->Limits.MaxBodyBytes) ||
		(pReader->BodyBytes > pReader->WireBytes) ) {
		return false;
	}
	if ( pReader->State == XRT_MULTIPART_READER_ERROR ) {
		return pReader->Error != XMULTIPART_ERROR_NONE;
	}
	return pReader->Error == XMULTIPART_ERROR_NONE;
}



/* 验证 ReaderRead 的输出描述符和借用输入互不覆盖。 */
static bool __xrtMultipartReaderArgumentsValid(
	xmultipartreader* pReader,
	xbytesview Input,
	size_t* pConsumed,
	xmultipartpart* pPart,
	xbytesview* pData,
	xmultiparterrorinfo* pError
)
{
	if ( !__xrtMultipartReaderPointerValid(pReader) ||
		!xrtMemRangeValid(pConsumed, sizeof(*pConsumed)) ||
		!xrtMemRangeValid(pPart, sizeof(*pPart)) ||
		!xrtMemRangeValid(pData, sizeof(*pData)) ||
		!__xrtMultipartBytesValid(Input) ||
		xrtMemRangesOverlap(
			pReader, sizeof(*pReader), Input.Data, Input.Size
		) || xrtMemRangesOverlap(
			pConsumed, sizeof(*pConsumed),
			Input.Data, Input.Size
		) || xrtMemRangesOverlap(
			pPart, sizeof(*pPart), Input.Data, Input.Size
		) || xrtMemRangesOverlap(
			pData, sizeof(*pData), Input.Data, Input.Size
		) || xrtMemRangesOverlap(
			pReader, sizeof(*pReader),
			pConsumed, sizeof(*pConsumed)
		) || xrtMemRangesOverlap(
			pReader, sizeof(*pReader),
			pPart, sizeof(*pPart)
		) || xrtMemRangesOverlap(
			pReader, sizeof(*pReader),
			pData, sizeof(*pData)
		) || xrtMemRangesOverlap(
			pConsumed, sizeof(*pConsumed),
			pPart, sizeof(*pPart)
		) || xrtMemRangesOverlap(
			pConsumed, sizeof(*pConsumed),
			pData, sizeof(*pData)
		) || xrtMemRangesOverlap(
			pPart, sizeof(*pPart),
			pData, sizeof(*pData)
		) ) {
		return false;
	}
	if ( (pError != NULL) && (
		!xrtMemRangeValid(pError, sizeof(*pError)) ||
		xrtMemRangesOverlap(
			pError, sizeof(*pError), Input.Data, Input.Size
		) || xrtMemRangesOverlap(
			pError, sizeof(*pError),
			pReader, sizeof(*pReader)
		) || xrtMemRangesOverlap(
			pError, sizeof(*pError),
			pConsumed, sizeof(*pConsumed)
		) || xrtMemRangesOverlap(
			pError, sizeof(*pError),
			pPart, sizeof(*pPart)
		) || xrtMemRangesOverlap(
			pError, sizeof(*pError),
			pData, sizeof(*pData)
		)
	) ) {
		return false;
	}
	return true;
}



/* 在 preamble 中查找第一条完整或待补全的 delimiter。 */
static xrt_multipart_delimiter __xrtMultipartReaderFirst(
	xbytesview Input,
	const xmultipartboundary* pBoundary,
	bool bEnd,
	size_t* pDash,
	size_t* pNext
)
{
	size_t i;

	for ( i = 0; i < Input.Size; i++ ) {
		xrt_multipart_delimiter Delimiter;
		bool bLineStart = (i == 0) ||
			((i >= 2u) &&
			 (Input.Data[i - 2u] == (uint8)'\r') &&
			 (Input.Data[i - 1u] == (uint8)'\n'));

		if ( !bLineStart ||
			(Input.Data[i] != (uint8)'-') ) {
			continue;
		}
		Delimiter = __xrtMultipartDelimiterAt(
			Input, pBoundary, i, bEnd, pNext
		);
		if ( Delimiter != XRT_MULTIPART_DELIMITER_INVALID ) {
			*pDash = i;
			return Delimiter;
		}
	}
	*pDash = Input.Size;
	return bEnd ?
		XRT_MULTIPART_DELIMITER_INVALID :
		XRT_MULTIPART_DELIMITER_MORE;
}



/* 在正文中查找下一条 delimiter，并保留可能被分块切开的前缀。 */
static xrt_multipart_delimiter __xrtMultipartReaderNext(
	xbytesview Input,
	const xmultipartboundary* pBoundary,
	bool bEnd,
	size_t* pBodyEnd,
	size_t* pDash,
	size_t* pNext
)
{
	size_t i;

	for ( i = 0; (Input.Size - i) >= 2u; i++ ) {
		xrt_multipart_delimiter Delimiter;

		if ( (Input.Data[i] != (uint8)'\r') ||
			(Input.Data[i + 1u] != (uint8)'\n') ) {
			continue;
		}
		Delimiter = __xrtMultipartDelimiterAt(
			Input, pBoundary, i + 2u, bEnd, pNext
		);
		if ( Delimiter == XRT_MULTIPART_DELIMITER_MORE ) {
			*pBodyEnd = i;
			*pDash = i + 2u;
			return Delimiter;
		}
		if ( Delimiter != XRT_MULTIPART_DELIMITER_INVALID ) {
			*pBodyEnd = i;
			*pDash = i + 2u;
			return Delimiter;
		}
	}
	*pBodyEnd = Input.Size;
	*pDash = Input.Size;
	return bEnd ?
		XRT_MULTIPART_DELIMITER_INVALID :
		XRT_MULTIPART_DELIMITER_MORE;
}



/* 发布正文数据并同步 Part 与总正文计数。 */
static xmultipartreadstatus __xrtMultipartReaderData(
	xmultipartreader* pReader,
	xbytesview Input,
	size_t iSize,
	size_t* pConsumed,
	xbytesview* pData,
	xmultiparterrorinfo* pError
)
{
	if ( iSize >
		(pReader->Limits.MaxPartBytes -
		 pReader->PartBytes) ) {
		return __xrtMultipartReaderFail(
			pReader, pError,
			XMULTIPART_ERROR_PART_BYTES_LIMIT,
			pReader->WireBytes, XERR_RANGE
		);
	}
	if ( pReader->BodyBytes > (SIZE_MAX - iSize) ) {
		return __xrtMultipartReaderFail(
			pReader, pError,
			XMULTIPART_ERROR_BODY_BYTES_LIMIT,
			pReader->WireBytes, XERR_RANGE
		);
	}
	if ( iSize >
		(pReader->Limits.MaxBodyBytes -
		 pReader->WireBytes) ) {
		return __xrtMultipartReaderFail(
			pReader, pError,
			XMULTIPART_ERROR_BODY_BYTES_LIMIT,
			pReader->WireBytes, XERR_RANGE
		);
	}
	{
		xbytesview Data = { Input.Data, iSize };

		memcpy(pData, &Data, sizeof(Data));
	}
	pReader->PartBytes += iSize;
	pReader->BodyBytes += iSize;
	pReader->WireBytes += iSize;
	memcpy(pConsumed, &iSize, sizeof(iSize));
	return XMULTIPART_READ_DATA;
}



/* 初始化无分配流 Reader。 */
XRT_API bool xrtMultipartReaderInit(
	xmultipartreader* pReader,
	const xmultipartboundary* pBoundary,
	const xmultipartlimits* pLimits
)
{
	xmultipartreader Reader;

	if ( !__xrtMultipartReaderPointerValid(pReader) ||
		!__xrtMultipartBoundaryValid(pBoundary) ||
		((pLimits != NULL) &&
		 !xrtMemRangeValid(pLimits, sizeof(*pLimits))) ||
		xrtMemRangesOverlap(
			pReader, sizeof(*pReader),
			pBoundary, sizeof(*pBoundary)
		) || ((pLimits != NULL) &&
		 xrtMemRangesOverlap(
			pReader, sizeof(*pReader),
			pLimits, sizeof(*pLimits)
		)) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	memset(&Reader, 0, sizeof(Reader));
	memcpy(&Reader.Boundary, pBoundary, sizeof(Reader.Boundary));
	xrtMultipartLimitsInit(&Reader.Limits);
	if ( pLimits != NULL ) {
		memcpy(&Reader.Limits, pLimits, sizeof(Reader.Limits));
	}
	Reader.State = XRT_MULTIPART_READER_START;
	memcpy(pReader, &Reader, sizeof(Reader));
	return true;
}



/* 重置流 Reader 的解析进度。 */
XRT_API void xrtMultipartReaderReset(
	xmultipartreader* pReader
)
{
	xmultipartboundary Boundary;
	xmultipartlimits Limits;

	if ( !__xrtMultipartReaderValid(pReader) ) {
		__xhttpErrorSetInvalidArgument();
		return;
	}
	memcpy(&Boundary, &pReader->Boundary, sizeof(Boundary));
	memcpy(&Limits, &pReader->Limits, sizeof(Limits));
	memset(pReader, 0, sizeof(*pReader));
	pReader->Boundary = Boundary;
	pReader->Limits = Limits;
	pReader->State = XRT_MULTIPART_READER_START;
}



/* 推进流式 multipart 状态机。 */
XRT_API xmultipartreadstatus xrtMultipartReaderRead(
	xmultipartreader* pReader,
	xbytesview Input,
	bool bEnd,
	size_t* pConsumed,
	xmultipartpart* pPart,
	xbytesview* pData,
	xmultiparterrorinfo* pError
)
{
	size_t iLocal = 0;

	if ( !__xrtMultipartReaderArgumentsValid(
		pReader, Input, pConsumed, pPart, pData, pError
	) ) {
		return __xrtMultipartReaderFail(
			NULL, NULL,
			XMULTIPART_ERROR_ARGUMENT,
			0, XERR_ARGUMENT
		);
	}
	memcpy(pConsumed, &iLocal, sizeof(iLocal));
	{
		const xmultipartpart Part = { 0 };
		const xbytesview Data = { NULL, 0 };

		memcpy(pPart, &Part, sizeof(Part));
		memcpy(pData, &Data, sizeof(Data));
	}
	__xrtMultipartReaderErrorClear(pError);
	if ( !__xrtMultipartReaderValid(pReader) ) {
		return __xrtMultipartReaderFail(
			pReader, pError,
			XMULTIPART_ERROR_ARGUMENT,
			pReader->WireBytes, XERR_ARGUMENT
		);
	}
	if ( pReader->State == XRT_MULTIPART_READER_ERROR ) {
		const xmultiparterrorinfo Error = {
			pReader->Error,
			pReader->ErrorOffset
		};

		if ( pError != NULL ) {
			memcpy(pError, &Error, sizeof(Error));
		}
		return XMULTIPART_READ_ERROR;
	}
	if ( pReader->State == XRT_MULTIPART_READER_DONE ) {
		return XMULTIPART_READ_DONE;
	}
	if ( Input.Size >
		(pReader->Limits.MaxBodyBytes -
		 pReader->WireBytes) ) {
		return __xrtMultipartReaderFail(
			pReader, pError,
			XMULTIPART_ERROR_BODY_BYTES_LIMIT,
			pReader->WireBytes, XERR_RANGE
		);
	}

	for ( ;; ) {
		xbytesview Remaining = {
			(Input.Data == NULL) ?
				NULL : Input.Data + iLocal,
			Input.Size - iLocal
		};

		if ( pReader->State == XRT_MULTIPART_READER_START ) {
			xrt_multipart_delimiter Delimiter;
			size_t iDash;
			size_t iNext;
			size_t iKeep =
				pReader->Boundary.Size + 8u;

			Delimiter = __xrtMultipartReaderFirst(
				Remaining, &pReader->Boundary,
				bEnd, &iDash, &iNext
			);
			if ( Delimiter ==
				XRT_MULTIPART_DELIMITER_INVALID ) {
				return __xrtMultipartReaderFail(
					pReader, pError,
					XMULTIPART_ERROR_TRUNCATED,
					pReader->WireBytes + iLocal,
					XERR_VALUE
				);
			}
			if ( Delimiter ==
				XRT_MULTIPART_DELIMITER_MORE ) {
				size_t iCandidate =
					Remaining.Size - iDash;

				if ( (iDash < Remaining.Size) &&
					(iCandidate >
					 pReader->Limits.MaxDelimiterBytes) ) {
					return __xrtMultipartReaderFail(
						pReader, pError,
						XMULTIPART_ERROR_DELIMITER_BYTES_LIMIT,
						pReader->WireBytes + iLocal + iDash,
						XERR_RANGE
					);
				}
				if ( iDash < Remaining.Size ) {
					iLocal += iDash;
				} else if ( Remaining.Size > iKeep ) {
					iLocal += Remaining.Size - iKeep;
				}
				return __xrtMultipartReaderReturn(
					pReader, iLocal, pConsumed,
					pError, XMULTIPART_READ_MORE
				);
			}
			if ( (iNext - iDash) >
				pReader->Limits.MaxDelimiterBytes ) {
				return __xrtMultipartReaderFail(
					pReader, pError,
					XMULTIPART_ERROR_DELIMITER_BYTES_LIMIT,
					pReader->WireBytes + iLocal + iDash,
					XERR_RANGE
				);
			}
			if ( Delimiter ==
				XRT_MULTIPART_DELIMITER_CLOSE ) {
				return __xrtMultipartReaderFail(
					pReader, pError,
					XMULTIPART_ERROR_DELIMITER,
					pReader->WireBytes + iLocal + iDash,
					XERR_VALUE
				);
			}
			iLocal += iNext;
			pReader->State =
				XRT_MULTIPART_READER_HEADERS;
			continue;
		}

		if ( pReader->State ==
			XRT_MULTIPART_READER_HEADERS ) {
			xmultiparterrorinfo HeaderError;
			xmultipartpart Part;
			size_t iBodyStart;

			memset(&HeaderError, 0, sizeof(HeaderError));
			memset(&Part, 0, sizeof(Part));
			if ( !__xrtMultipartHeaderBlock(
				Remaining, 0,
				&Part.Headers, &iBodyStart
			) ) {
				if ( (Remaining.Size >
					 pReader->Limits.MaxHeaderBytes) &&
					((Remaining.Size -
					  pReader->Limits.MaxHeaderBytes) >
					 4u) ) {
					return __xrtMultipartReaderFail(
						pReader, pError,
						XMULTIPART_ERROR_HEADER_BYTES_LIMIT,
						pReader->WireBytes + iLocal,
						XERR_RANGE
					);
				}
				if ( bEnd ) {
					return __xrtMultipartReaderFail(
						pReader, pError,
						XMULTIPART_ERROR_TRUNCATED,
						pReader->WireBytes + iLocal,
						XERR_VALUE
					);
				}
				return __xrtMultipartReaderReturn(
					pReader, iLocal, pConsumed,
					pError, XMULTIPART_READ_MORE
				);
			}
			if ( Part.Headers.Size >
				pReader->Limits.MaxHeaderBytes ) {
				return __xrtMultipartReaderFail(
					pReader, pError,
					XMULTIPART_ERROR_HEADER_BYTES_LIMIT,
					pReader->WireBytes + iLocal,
					XERR_RANGE
				);
			}
			Part.Body.Data =
				Remaining.Data + iBodyStart;
			Part.Body.Size = 0;
			if ( !__xrtMultipartPartHeaders(
				&Part, &HeaderError,
				pReader->WireBytes + iLocal
			) ) {
				pReader->State =
					XRT_MULTIPART_READER_ERROR;
				pReader->Error = HeaderError.Code;
				pReader->ErrorOffset = HeaderError.Offset;
				if ( pError != NULL ) {
					memcpy(pError, &HeaderError, sizeof(HeaderError));
				}
				return XMULTIPART_READ_ERROR;
			}
			if ( Part.HeaderCount >
				pReader->Limits.MaxHeaders ) {
				return __xrtMultipartReaderFail(
					pReader, pError,
					XMULTIPART_ERROR_HEADERS_LIMIT,
					pReader->WireBytes + iLocal,
					XERR_RANGE
				);
			}
			if ( pReader->Parts >=
				pReader->Limits.MaxParts ) {
				return __xrtMultipartReaderFail(
					pReader, pError,
					XMULTIPART_ERROR_PARTS_LIMIT,
					pReader->WireBytes + iLocal,
					XERR_RANGE
				);
			}
			if ( (iLocal >
				 (pReader->Limits.MaxBodyBytes -
				  pReader->WireBytes)) ||
				(iBodyStart >
				 (pReader->Limits.MaxBodyBytes -
				  pReader->WireBytes - iLocal)) ) {
				return __xrtMultipartReaderFail(
					pReader, pError,
					XMULTIPART_ERROR_BODY_BYTES_LIMIT,
					pReader->WireBytes + iLocal,
					XERR_RANGE
				);
			}
			iLocal += iBodyStart;
			pReader->Parts++;
			pReader->PartBytes = 0;
			pReader->State =
				XRT_MULTIPART_READER_BODY;
			memcpy(pPart, &Part, sizeof(Part));
			return __xrtMultipartReaderReturn(
				pReader, iLocal, pConsumed,
				pError, XMULTIPART_READ_PART
			);
		}

		if ( pReader->State ==
			XRT_MULTIPART_READER_BODY ) {
			xrt_multipart_delimiter Delimiter;
			size_t iBodyEnd;
			size_t iDash;
			size_t iNext;
			size_t iKeep =
				pReader->Boundary.Size + 8u;

			Delimiter = __xrtMultipartReaderNext(
				Remaining, &pReader->Boundary,
				bEnd, &iBodyEnd, &iDash, &iNext
			);
			if ( Delimiter ==
				XRT_MULTIPART_DELIMITER_INVALID ) {
				return __xrtMultipartReaderFail(
					pReader, pError,
					XMULTIPART_ERROR_TRUNCATED,
					pReader->WireBytes,
					XERR_VALUE
				);
			}
			if ( Delimiter ==
				XRT_MULTIPART_DELIMITER_MORE ) {
				if ( iDash < Remaining.Size ) {
					size_t iCandidate =
						Remaining.Size - iDash;

					if ( iCandidate >
						pReader->Limits.MaxDelimiterBytes ) {
						return __xrtMultipartReaderFail(
							pReader, pError,
							XMULTIPART_ERROR_DELIMITER_BYTES_LIMIT,
							pReader->WireBytes + iDash,
							XERR_RANGE
						);
					}
					if ( iBodyEnd != 0 ) {
						return __xrtMultipartReaderData(
							pReader, Remaining,
							iBodyEnd, pConsumed,
							pData, pError
						);
					}
					return XMULTIPART_READ_MORE;
				}
				if ( Remaining.Size > iKeep ) {
					return __xrtMultipartReaderData(
						pReader, Remaining,
						Remaining.Size - iKeep,
						pConsumed, pData, pError
					);
				}
				return XMULTIPART_READ_MORE;
			}
			if ( iBodyEnd != 0 ) {
				return __xrtMultipartReaderData(
					pReader, Remaining,
					iBodyEnd, pConsumed,
					pData, pError
				);
			}
			if ( (iNext - iDash) >
				pReader->Limits.MaxDelimiterBytes ) {
				return __xrtMultipartReaderFail(
					pReader, pError,
					XMULTIPART_ERROR_DELIMITER_BYTES_LIMIT,
					pReader->WireBytes + iDash,
					XERR_RANGE
				);
			}
			pReader->State = (Delimiter ==
				XRT_MULTIPART_DELIMITER_CLOSE) ?
				XRT_MULTIPART_READER_EPILOGUE :
				XRT_MULTIPART_READER_HEADERS;
			return __xrtMultipartReaderReturn(
				pReader, iNext, pConsumed,
				pError, XMULTIPART_READ_PART_END
			);
		}

		if ( pReader->State ==
			XRT_MULTIPART_READER_EPILOGUE ) {
			if ( !bEnd ) {
				return __xrtMultipartReaderReturn(
					pReader, Remaining.Size,
					pConsumed, pError,
					XMULTIPART_READ_MORE
				);
			}
			pReader->State = XRT_MULTIPART_READER_DONE;
			return __xrtMultipartReaderReturn(
				pReader, Remaining.Size,
				pConsumed, pError,
				XMULTIPART_READ_DONE
			);
		}

		return __xrtMultipartReaderFail(
			pReader, pError,
			XMULTIPART_ERROR_ARGUMENT,
			pReader->WireBytes + iLocal,
			XERR_ARGUMENT
		);
	}
}



/* 判断流 Reader 是否完成关闭 boundary 和 epilogue。 */
XRT_API bool xrtMultipartReaderDone(
	const xmultipartreader* pReader
)
{
	if ( !__xrtMultipartReaderValid(pReader) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	return pReader->State == XRT_MULTIPART_READER_DONE;
}

#endif
