#include "../internal/xrt_mail.h"



#if defined(XMAIL_FEATURE_MAIL_WIRE)

/* 从增量输入探测一条严格 CRLF 线路。 */
XRT_API xmailnext xrtMailLineRead(
	xstrview Data,
	size_t iMaxLine,
	xstrview* pLine,
	size_t* pConsumed
)
{
	if ( !__xrtMailViewValid(Data) ||
		 !xrtMemRangeValid(pLine, sizeof(*pLine)) ||
		 !xrtMemRangeValid(pConsumed, sizeof(*pConsumed)) ||
		 xrtMemRangesOverlap(pLine, sizeof(*pLine), pConsumed,
			sizeof(*pConsumed)) ||
		 xrtMemRangesOverlap(pLine, sizeof(*pLine), Data.Data, Data.Size) ||
		 xrtMemRangesOverlap(pConsumed, sizeof(*pConsumed), Data.Data, Data.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( iMaxLine == 0 ) {
		iMaxLine = XMAIL_WIRE_LINE_DEFAULT;
	}
	for ( size_t i = 0; i < Data.Size; i++ ) {
		if ( Data.Data[i] == '\n' ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_LINE,
				"mail wire contains a bare LF"
			);
			return XMAIL_NEXT_ERROR;
		}
		if ( Data.Data[i] != '\r' ) {
			continue;
		}
		if ( (i + 1u) == Data.Size ) {
			if ( i > iMaxLine ) {
				__xrtMailError(
					XERR_RANGE,
					XMAIL_ERROR_LIMIT,
					"mail wire line exceeds the byte limit"
				);
				return XMAIL_NEXT_ERROR;
			}
			return XMAIL_NEXT_END;
		}
		if ( Data.Data[i + 1u] != '\n' ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_LINE,
				"mail wire contains a bare CR"
			);
			return XMAIL_NEXT_ERROR;
		}
		if ( i > iMaxLine ) {
			__xrtMailError(
				XERR_RANGE,
				XMAIL_ERROR_LIMIT,
				"mail wire line exceeds the byte limit"
			);
			return XMAIL_NEXT_ERROR;
		}
		*pLine = __xrtMailSlice(Data, 0, i);
		*pConsumed = i + 2u;
		return XMAIL_NEXT_ITEM;
	}
	if ( Data.Size > iMaxLine ) {
		__xrtMailError(
			XERR_RANGE,
			XMAIL_ERROR_LIMIT,
			"mail wire line exceeds the byte limit"
		);
		return XMAIL_NEXT_ERROR;
	}
	return XMAIL_NEXT_END;
}



/* 去除一条线路的 dot transparency 前缀。 */
XRT_API xmailnext xrtMailDotLine(xstrview Line, xstrview* pData)
{
	if ( !__xrtMailViewValid(Line) ||
		 !xrtMemRangeValid(pData, sizeof(*pData)) ||
		 xrtMemRangesOverlap(pData, sizeof(*pData), Line.Data, Line.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( (Line.Size == 1u) && (Line.Data[0] == '.') ) {
		return XMAIL_NEXT_END;
	}
	*pData = (Line.Size != 0) && (Line.Data[0] == '.') ?
		__xrtMailSlice(Line, 1u, Line.Size - 1u) : Line;
	return XMAIL_NEXT_ITEM;
}



/* 将一个非空片段交给增量 sink，并保留原始回调错误。 */
static bool __xrtMailDotEmit(
	xmailwriteproc pWrite,
	ptr pUserData,
	const void* pData,
	size_t iSize
)
{
	xbytesview Data;

	if ( iSize == 0 ) {
		return true;
	}
	Data.Data = (const unsigned char*)pData;
	Data.Size = iSize;
	if ( pWrite(Data, pUserData) ) {
		return true;
	}
	if ( xrtGetError() == NULL ) {
		__xrtMailError(
			XERR_IO,
			XMAIL_ERROR_CALLBACK,
			"mail dot writer sink failed"
		);
	}
	return false;
}



/* 在发布任何输出前校验当前片段的 CRLF 结构。 */
static bool __xrtMailDotWriterValidate(
	const xmaildotwriter* pWriter,
	xbytesview Data
)
{
	size_t iPosition = 0;

	if ( pWriter->PendingCr ) {
		if ( Data.Size == 0 ) {
			return true;
		}
		if ( Data.Data[0] != (unsigned char)'\n' ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_LINE,
				"mail dot stream contains a bare CR"
			);
			return false;
		}
		iPosition = 1u;
	}
	while ( iPosition < Data.Size ) {
		unsigned char iByte = Data.Data[iPosition];

		if ( iByte == (unsigned char)'\n' ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_LINE,
				"mail dot stream contains a bare LF"
			);
			return false;
		}
		if ( iByte == (unsigned char)'\r' ) {
			if ( (iPosition + 1u) == Data.Size ) {
				return true;
			}
			if ( Data.Data[iPosition + 1u] != (unsigned char)'\n' ) {
				__xrtMailError(
					XERR_PROTOCOL,
					XMAIL_ERROR_LINE,
					"mail dot stream contains a bare CR"
				);
				return false;
			}
			iPosition += 2u;
			continue;
		}
		iPosition++;
	}
	return true;
}



/* 初始化增量 dot writer。 */
XRT_API bool xrtMailDotWriterInit(xmaildotwriter* pWriter)
{
	if ( !xrtMemRangeValid(pWriter, sizeof(*pWriter)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	memset(pWriter, 0, sizeof(*pWriter));
	pWriter->LineStart = true;
	return true;
}



/* 写出一个增量 dot-transparent 片段。 */
XRT_API bool xrtMailDotWriterWrite(
	xmaildotwriter* pWriter,
	xbytesview Data,
	xmailwriteproc pWrite,
	ptr pUserData
)
{
	xmaildotwriter Writer;
	size_t iPosition = 0;
	size_t iStart = 0;

	if ( !xrtMemRangeValid(pWriter, sizeof(*pWriter)) ||
		 !xrtMemRangeValid(Data.Data, Data.Size) || (pWrite == NULL) ||
		 pWriter->Finished ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailDotWriterValidate(pWriter, Data) ) {
		return false;
	}
	Writer = *pWriter;
	if ( Writer.PendingCr && (Data.Size != 0) ) {
		if ( !__xrtMailDotEmit(pWrite, pUserData, "\r\n", 2u) ) {
			pWriter->Finished = true;
			return false;
		}
		Writer.PendingCr = false;
		Writer.LineStart = true;
		iPosition = 1u;
		iStart = 1u;
	}
	while ( iPosition < Data.Size ) {
		unsigned char iByte = Data.Data[iPosition];

		if ( Writer.LineStart && (iByte == (unsigned char)'.') ) {
			if ( !__xrtMailDotEmit(
				pWrite,
				pUserData,
				Data.Data + iStart,
				iPosition - iStart
			) || !__xrtMailDotEmit(pWrite, pUserData, ".", 1u) ) {
				pWriter->Finished = true;
				return false;
			}
			iStart = iPosition;
		}
		if ( iByte == (unsigned char)'\r' ) {
			if ( (iPosition + 1u) == Data.Size ) {
				if ( !__xrtMailDotEmit(
					pWrite,
					pUserData,
					Data.Data + iStart,
					iPosition - iStart
				) ) {
					pWriter->Finished = true;
					return false;
				}
				Writer.PendingCr = true;
				iStart = Data.Size;
				break;
			}
			iPosition++;
			Writer.LineStart = true;
		} else {
			Writer.LineStart = false;
		}
		iPosition++;
	}
	if ( !__xrtMailDotEmit(
		pWrite,
		pUserData,
		Data.Data != NULL ? Data.Data + iStart : NULL,
		Data.Size - iStart
	) ) {
		pWriter->Finished = true;
		return false;
	}
	*pWriter = Writer;
	return true;
}



/* 完成增量 dot stream。 */
XRT_API bool xrtMailDotWriterFinish(
	xmaildotwriter* pWriter,
	xmailwriteproc pWrite,
	ptr pUserData
)
{
	cstr sTerminator;
	size_t iSize;

	if ( !xrtMemRangeValid(pWriter, sizeof(*pWriter)) ||
		 (pWrite == NULL) || pWriter->Finished ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( pWriter->PendingCr ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_LINE,
			"mail dot stream ends with a bare CR"
		);
		return false;
	}
	sTerminator = pWriter->LineStart ? ".\r\n" : "\r\n.\r\n";
	iSize = pWriter->LineStart ? 3u : 5u;
	if ( !__xrtMailDotEmit(pWrite, pUserData, sTerminator, iSize) ) {
		pWriter->Finished = true;
		return false;
	}
	pWriter->Finished = true;
	return true;
}



/* 验证 dot 编码输入并计算额外前导点数量。 */
static bool __xrtMailDotMeasure(
	xstrview Data,
	bool Terminate,
	size_t* pRequired
)
{
	size_t iRequired = Data.Size;
	bool bLineStart = true;
	bool bEndsCrlf = false;

	for ( size_t i = 0; i < Data.Size; i++ ) {
		if ( bLineStart && (Data.Data[i] == '.') ) {
			if ( !__xrtMailSizeAdd(iRequired, 1u, &iRequired) ) {
				return false;
			}
		}
		bLineStart = false;
		if ( Data.Data[i] == '\n' ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_LINE,
				"mail dot input contains a bare LF"
			);
			return false;
		}
		if ( Data.Data[i] != '\r' ) {
			continue;
		}
		if ( ((i + 1u) >= Data.Size) || (Data.Data[i + 1u] != '\n') ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_LINE,
				"mail dot input contains a bare CR"
			);
			return false;
		}
		i++;
		bLineStart = true;
		bEndsCrlf = (i + 1u) == Data.Size;
	}
	if ( Terminate ) {
		if ( (Data.Size != 0) && !bEndsCrlf &&
			 !__xrtMailSizeAdd(iRequired, 2u, &iRequired) ) {
			return false;
		}
		if ( !__xrtMailSizeAdd(iRequired, 3u, &iRequired) ) {
			return false;
		}
	}
	*pRequired = iRequired;
	return true;
}



/* 执行已经测量过的 dot 编码。 */
static void __xrtMailDotBody(
	xstrview Data,
	bool Terminate,
	bytes pOutput,
	size_t iRequired
)
{
	size_t iOutput = 0;
	bool bLineStart = true;
	bool bEndsCrlf = false;

	for ( size_t i = 0; i < Data.Size; i++ ) {
		if ( bLineStart && (Data.Data[i] == '.') ) {
			pOutput[iOutput++] = (uint8)'.';
		}
		pOutput[iOutput++] = (uint8)Data.Data[i];
		bLineStart = false;
		if ( Data.Data[i] == '\r' ) {
			pOutput[iOutput++] = (uint8)Data.Data[++i];
			bLineStart = true;
			bEndsCrlf = (i + 1u) == Data.Size;
		}
	}
	if ( Terminate && (Data.Size != 0) && !bEndsCrlf ) {
		pOutput[iOutput++] = (uint8)'\r';
		pOutput[iOutput++] = (uint8)'\n';
	}
	if ( Terminate ) {
		pOutput[iOutput++] = (uint8)'.';
		pOutput[iOutput++] = (uint8)'\r';
		pOutput[iOutput++] = (uint8)'\n';
	}
	(void)iRequired;
}



/* 写出 dot-transparent 数据。 */
XRT_API bool xrtMailDotWrite(
	xstrview Data,
	bool Terminate,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;

	if ( !__xrtMailViewValid(Data) ||
		 !xrtMemRangeValid(pOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), Data.Data,
			Data.Size) ||
		 ((pOutput != NULL) && xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			pOutput,
			iCapacity
		 )) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailDotMeasure(Data, Terminate, &iRequired) ) {
		return false;
	}
	if ( pOutput == NULL ) {
		*pOutputSize = iRequired;
		return true;
	}
	if ( iCapacity < iRequired ) {
		*pOutputSize = iRequired;
		__xrtMailSetRange();
		return false;
	}
	if ( xrtMemRangesOverlap(pOutput, iRequired, Data.Data, Data.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	__xrtMailDotBody(Data, Terminate, (bytes)pOutput, iRequired);
	*pOutputSize = iRequired;
	return true;
}



/* 分配并写出 dot-transparent 数据。 */
XRT_API bytes xrtMailDot(
	xstrview Data,
	bool Terminate,
	size_t* pOutputSize
)
{
	size_t iRequired;
	bytes pOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailDotWrite(Data, Terminate, NULL, 0, &iRequired) ) {
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtMailSetSizeOverflow();
		return NULL;
	}
	pOutput = (bytes)xrtMalloc(iRequired + 1u);
	if ( pOutput == NULL ) {
		return NULL;
	}
	if ( !xrtMailDotWrite(
		Data,
		Terminate,
		pOutput,
		iRequired,
		&iRequired
	) ) {
		xrtFree(pOutput);
		return NULL;
	}
	pOutput[iRequired] = 0;
	if ( pOutputSize != NULL ) {
		*pOutputSize = iRequired;
	}
	return pOutput;
}



/* 计算或写出完整 dot-transparent 行块。 */
static bool __xrtMailDotDecodeBody(
	xstrview Data,
	bool RequireTerminator,
	bytes pOutput,
	size_t* pOutputSize
)
{
	size_t iPosition = 0;
	size_t iOutput = 0;
	bool bTerminated = false;

	while ( iPosition < Data.Size ) {
		xstrview Remaining = __xrtMailSlice(
			Data,
			iPosition,
			Data.Size - iPosition
		);
		xstrview Line;
		xstrview Plain;
		size_t iConsumed;
		xmailnext Next = xrtMailLineRead(
			Remaining,
			SIZE_MAX,
			&Line,
			&iConsumed
		);

		if ( Next != XMAIL_NEXT_ITEM ) {
			if ( Next == XMAIL_NEXT_END ) {
				__xrtMailError(
					XERR_PROTOCOL,
					XMAIL_ERROR_LINE,
					"mail dot block ends with an incomplete line"
				);
			}
			return false;
		}
		Next = xrtMailDotLine(Line, &Plain);
		if ( Next == XMAIL_NEXT_ERROR ) {
			return false;
		}
		iPosition += iConsumed;
		if ( Next == XMAIL_NEXT_END ) {
			if ( iPosition != Data.Size ) {
				__xrtMailError(
					XERR_PROTOCOL,
					XMAIL_ERROR_LINE,
					"mail dot block contains data after the terminator"
				);
				return false;
			}
			bTerminated = true;
			break;
		}
		if ( !__xrtMailSizeAdd(iOutput, Plain.Size, &iOutput) ||
			 !__xrtMailSizeAdd(iOutput, 2u, &iOutput) ) {
			return false;
		}
		if ( pOutput != NULL ) {
			memcpy(pOutput + iOutput - Plain.Size - 2u, Plain.Data, Plain.Size);
			pOutput[iOutput - 2u] = (uint8)'\r';
			pOutput[iOutput - 1u] = (uint8)'\n';
		}
	}
	if ( RequireTerminator && !bTerminated ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_LINE,
			"mail dot block has no terminator"
		);
		return false;
	}
	*pOutputSize = iOutput;
	return true;
}



/* 解码完整 dot-transparent 行块。 */
XRT_API bool xrtMailDotDecodeWrite(
	xstrview Data,
	bool RequireTerminator,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;

	if ( !__xrtMailViewValid(Data) ||
		 !xrtMemRangeValid(pOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), Data.Data,
			Data.Size) ||
		 ((pOutput != NULL) && xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			pOutput,
			iCapacity
		 )) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailDotDecodeBody(
		Data,
		RequireTerminator,
		NULL,
		&iRequired
	) ) {
		return false;
	}
	if ( pOutput == NULL ) {
		*pOutputSize = iRequired;
		return true;
	}
	if ( iCapacity < iRequired ) {
		*pOutputSize = iRequired;
		__xrtMailSetRange();
		return false;
	}
	if ( xrtMemRangesOverlap(pOutput, iRequired, Data.Data, Data.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return __xrtMailDotDecodeBody(
		Data,
		RequireTerminator,
		(bytes)pOutput,
		pOutputSize
	);
}



/* 分配并解码完整 dot-transparent 行块。 */
XRT_API bytes xrtMailDotDecode(
	xstrview Data,
	bool RequireTerminator,
	size_t* pOutputSize
)
{
	size_t iRequired;
	bytes pOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailDotDecodeWrite(
		Data,
		RequireTerminator,
		NULL,
		0,
		&iRequired
	) ) {
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtMailSetSizeOverflow();
		return NULL;
	}
	pOutput = (bytes)xrtMalloc(iRequired + 1u);
	if ( pOutput == NULL ) {
		return NULL;
	}
	if ( !xrtMailDotDecodeWrite(
		Data,
		RequireTerminator,
		pOutput,
		iRequired,
		&iRequired
	) ) {
		xrtFree(pOutput);
		return NULL;
	}
	pOutput[iRequired] = 0;
	if ( pOutputSize != NULL ) {
		*pOutputSize = iRequired;
	}
	return pOutput;
}

#endif
