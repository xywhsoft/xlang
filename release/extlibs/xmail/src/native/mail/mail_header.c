#include "../internal/xrt_mail.h"



#if defined(XMAIL_FEATURE_MAIL_HEADER)

/* 判断字段名称是否符合 RFC 5322 ftext。 */
XRT_API bool xrtMailHeaderNameValid(xstrview Name)
{
	if ( !__xrtMailViewValid(Name) || (Name.Size == 0) ) {
		return false;
	}
	for ( size_t i = 0; i < Name.Size; i++ ) {
		unsigned char iByte = (unsigned char)Name.Data[i];

		if ( (iByte < 33u) || (iByte > 126u) || (iByte == (unsigned char)':') ) {
			return false;
		}
	}
	return true;
}



/* 校验字段值并可选返回展开后的精确长度。 */
static bool __xrtMailHeaderValueCheck(
	xstrview Value,
	size_t* pUnfoldedSize
)
{
	size_t iRequired = 0;

	if ( !__xrtMailViewValid(Value) ) {
		return false;
	}
	for ( size_t i = 0; i < Value.Size; i++ ) {
		unsigned char iByte = (unsigned char)Value.Data[i];

		if ( iByte == (unsigned char)'\r' ) {
			if ( ((i + 2u) >= Value.Size) ||
				 (Value.Data[i + 1u] != '\n') ||
				 ((Value.Data[i + 2u] != ' ') &&
				  (Value.Data[i + 2u] != '\t')) ) {
				return false;
			}
			i += 2u;
			while ( ((i + 1u) < Value.Size) &&
				 ((Value.Data[i + 1u] == ' ') ||
				  (Value.Data[i + 1u] == '\t')) ) {
				i++;
			}
			if ( iRequired == SIZE_MAX ) {
				return false;
			}
			iRequired++;
			continue;
		}
		if ( (iByte == (unsigned char)'\n') || (iByte == 0) ||
			 ((iByte < 32u) && (iByte != (unsigned char)'\t')) ||
			 (iByte == 127u) ) {
			return false;
		}
		if ( iRequired == SIZE_MAX ) {
			return false;
		}
		iRequired++;
	}
	if ( pUnfoldedSize != NULL ) {
		*pUnfoldedSize = iRequired;
	}
	return true;
}



/* 判断字段值是否安全且折叠语法完整。 */
XRT_API bool xrtMailHeaderValueValid(xstrview Value)
{
	return __xrtMailHeaderValueCheck(Value, NULL);
}



/* 初始化不分配内存的字段游标。 */
XRT_API bool xrtMailHeaderCursorInit(
	xmailheadercursor* pCursor,
	xstrview Block
)
{
	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		 !__xrtMailViewValid(Block) ||
		 xrtMemRangesOverlap(pCursor, sizeof(*pCursor), Block.Data, Block.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	pCursor->Block = Block;
	pCursor->Position = 0;
	pCursor->Done = false;
	return true;
}



/* 找到严格 CRLF 物理行的结束位置。 */
static bool __xrtMailHeaderLineEnd(
	xstrview Block,
	size_t iStart,
	size_t* pEnd,
	bool* pCrlf
)
{
	for ( size_t i = iStart; i < Block.Size; i++ ) {
		if ( Block.Data[i] == '\n' ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_LINE,
				"mail header contains a bare LF"
			);
			return false;
		}
		if ( Block.Data[i] != '\r' ) {
			continue;
		}
		if ( ((i + 1u) >= Block.Size) || (Block.Data[i + 1u] != '\n') ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_LINE,
				"mail header contains a bare CR"
			);
			return false;
		}
		*pEnd = i;
		*pCrlf = true;
		return true;
	}
	*pEnd = Block.Size;
	*pCrlf = false;
	return true;
}



/* 发布一个字段解析错误。 */
static xmailnext __xrtMailHeaderParseError(cstr sMessage)
{
	__xrtMailError(XERR_PROTOCOL, XMAIL_ERROR_HEADER, sMessage);
	return XMAIL_NEXT_ERROR;
}



/* 返回下一个借用字段。 */
XRT_API xmailnext xrtMailHeaderNext(
	xmailheadercursor* pCursor,
	xmailheaderview* pHeader
)
{
	xstrview Block;
	size_t iStart;
	size_t iFirstEnd;
	size_t iValueStart;
	size_t iValueEnd;
	size_t iNext;
	size_t iColon = XRT_NPOS;
	bool bCrlf;
	xmailheaderview Result;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		 !xrtMemRangeValid(pHeader, sizeof(*pHeader)) ||
		 !__xrtMailViewValid(pCursor != NULL ? pCursor->Block :
			 __xrtMailView(NULL, 0)) ||
		 (pCursor->Position > pCursor->Block.Size) ||
		 xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pHeader,
			sizeof(*pHeader)) ||
		 xrtMemRangesOverlap(pHeader, sizeof(*pHeader),
			pCursor->Block.Data, pCursor->Block.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( pCursor->Done ) {
		return XMAIL_NEXT_END;
	}
	Block = pCursor->Block;
	iStart = pCursor->Position;
	if ( iStart == Block.Size ) {
		pCursor->Done = true;
		return XMAIL_NEXT_END;
	}
	if ( !__xrtMailHeaderLineEnd(Block, iStart, &iFirstEnd, &bCrlf) ) {
		return XMAIL_NEXT_ERROR;
	}
	if ( iFirstEnd == iStart ) {
		pCursor->Position = bCrlf ? iFirstEnd + 2u : iFirstEnd;
		pCursor->Done = true;
		return XMAIL_NEXT_END;
	}
	if ( (iFirstEnd - iStart) > XMAIL_HEADER_LINE_HARD ) {
		return __xrtMailHeaderParseError("mail header line exceeds 998 bytes");
	}
	if ( (Block.Data[iStart] == ' ') || (Block.Data[iStart] == '\t') ) {
		return __xrtMailHeaderParseError("mail header starts with a continuation line");
	}
	for ( size_t i = iStart; i < iFirstEnd; i++ ) {
		if ( Block.Data[i] == ':' ) {
			iColon = i;
			break;
		}
	}
	if ( iColon == XRT_NPOS ) {
		return __xrtMailHeaderParseError("mail header field has no colon");
	}
	Result.Name = __xrtMailView(Block.Data + iStart, iColon - iStart);
	if ( !xrtMailHeaderNameValid(Result.Name) ) {
		return __xrtMailHeaderParseError("invalid mail header field name");
	}
	iValueStart = iColon + 1u;
	while ( (iValueStart < iFirstEnd) &&
		 ((Block.Data[iValueStart] == ' ') ||
		  (Block.Data[iValueStart] == '\t')) ) {
		iValueStart++;
	}
	iValueEnd = iFirstEnd;
	iNext = bCrlf ? iFirstEnd + 2u : iFirstEnd;
	while ( bCrlf && (iNext < Block.Size) &&
		 ((Block.Data[iNext] == ' ') || (Block.Data[iNext] == '\t')) ) {
		size_t iEnd;
		bool bNextCrlf;

		if ( !__xrtMailHeaderLineEnd(Block, iNext, &iEnd, &bNextCrlf) ) {
			return XMAIL_NEXT_ERROR;
		}
		if ( (iEnd - iNext) > XMAIL_HEADER_LINE_HARD ) {
			return __xrtMailHeaderParseError("mail header line exceeds 998 bytes");
		}
		iValueEnd = iEnd;
		iNext = bNextCrlf ? iEnd + 2u : iEnd;
		bCrlf = bNextCrlf;
	}
	while ( (iValueEnd > iValueStart) &&
		 ((Block.Data[iValueEnd - 1u] == ' ') ||
		  (Block.Data[iValueEnd - 1u] == '\t')) ) {
		iValueEnd--;
	}
	Result.Value = __xrtMailView(
		Block.Data + iValueStart,
		iValueEnd - iValueStart
	);
	if ( !xrtMailHeaderValueValid(Result.Value) ) {
		return __xrtMailHeaderParseError("invalid mail header field value");
	}
	*pHeader = Result;
	pCursor->Position = iNext;
	return XMAIL_NEXT_ITEM;
}



/* 在已验证字段值上写出展开结果。 */
static void __xrtMailHeaderUnfoldBody(
	xstrview Value,
	char* sOutput,
	size_t iOutputSize
)
{
	size_t iOutput = 0;

	for ( size_t i = 0; i < Value.Size; i++ ) {
		if ( Value.Data[i] != '\r' ) {
			sOutput[iOutput++] = Value.Data[i];
			continue;
		}
		i += 2u;
		while ( ((i + 1u) < Value.Size) &&
			 ((Value.Data[i + 1u] == ' ') ||
			  (Value.Data[i + 1u] == '\t')) ) {
			i++;
		}
		sOutput[iOutput++] = ' ';
	}
	sOutput[iOutputSize] = 0;
}



/* 展开字段值中的规范折叠。 */
XRT_API bool xrtMailHeaderUnfoldWrite(
	xstrview Value,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;

	if ( !__xrtMailViewValid(Value) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailHeaderValueCheck(Value, &iRequired) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_HEADER,
			"invalid folded mail header value"
		);
		return false;
	}
	if ( xrtMemRangesOverlap(
		pOutputSize,
		sizeof(*pOutputSize),
		Value.Data,
		Value.Size
	) || ((sOutput != NULL) && xrtMemRangesOverlap(
		pOutputSize,
		sizeof(*pOutputSize),
		sOutput,
		iCapacity
	)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( sOutput == NULL ) {
		*pOutputSize = iRequired;
		return true;
	}
	if ( iCapacity <= iRequired ) {
		*pOutputSize = iRequired;
		__xrtMailSetRange();
		return false;
	}
	if ( xrtMemRangesOverlap(sOutput, iRequired + 1u, Value.Data, Value.Size) &&
		 ((const void*)sOutput != (const void*)Value.Data) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	__xrtMailHeaderUnfoldBody(Value, sOutput, iRequired);
	*pOutputSize = iRequired;
	return true;
}



/* 创建独立的展开字段值。 */
XRT_API str xrtMailHeaderUnfold(xstrview Value, size_t* pOutputSize)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailHeaderUnfoldWrite(
		Value,
		NULL,
		0,
		&iRequired
	) ) {
		return NULL;
	}
	if ( (pOutputSize != NULL) && xrtMemRangesOverlap(
		pOutputSize,
		sizeof(*pOutputSize),
		Value.Data,
		Value.Size
	) ) {
		__xrtMailSetInvalidArgument();
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtMailSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	__xrtMailHeaderUnfoldBody(Value, sOutput, iRequired);
	if ( pOutputSize != NULL ) {
		*pOutputSize = iRequired;
	}
	return sOutput;
}



/* 跳过字段值中的普通空白或折叠空白。 */
static size_t __xrtMailHeaderSkipSpace(xstrview Value, size_t iPosition)
{
	while ( iPosition < Value.Size ) {
		if ( (Value.Data[iPosition] == ' ') ||
			 (Value.Data[iPosition] == '\t') ) {
			iPosition++;
			continue;
		}
		if ( Value.Data[iPosition] == '\r' ) {
			iPosition += 2u;
			while ( (iPosition < Value.Size) &&
				 ((Value.Data[iPosition] == ' ') ||
				  (Value.Data[iPosition] == '\t')) ) {
				iPosition++;
			}
			continue;
		}
		break;
	}
	return iPosition;
}



/* 返回当前字段单词的结束位置。 */
static size_t __xrtMailHeaderWordEnd(xstrview Value, size_t iPosition)
{
	while ( iPosition < Value.Size ) {
		char iByte = Value.Data[iPosition];

		if ( (iByte == ' ') || (iByte == '\t') || (iByte == '\r') ) {
			break;
		}
		iPosition++;
	}
	return iPosition;
}



/* 计算或写出一个完成折叠的字段行。 */
static bool __xrtMailHeaderBody(
	xstrview Name,
	xstrview Value,
	size_t iLineSize,
	char* sOutput,
	size_t* pOutputSize
)
{
	size_t iOutput = 0;
	size_t iColumn = Name.Size + 1u;
	size_t iPosition = 0;

	if ( sOutput != NULL ) {
		memcpy(sOutput, Name.Data, Name.Size);
		sOutput[Name.Size] = ':';
	}
	iOutput = Name.Size + 1u;
	while ( true ) {
		size_t iEnd;
		size_t iWordSize;
		size_t iSeparator;

		iPosition = __xrtMailHeaderSkipSpace(Value, iPosition);
		if ( iPosition == Value.Size ) {
			break;
		}
		iEnd = __xrtMailHeaderWordEnd(Value, iPosition);
		iWordSize = iEnd - iPosition;
		if ( iWordSize > (XMAIL_HEADER_LINE_HARD - 1u) ) {
			__xrtMailError(
				XERR_RANGE,
				XMAIL_ERROR_LINE,
				"mail header word exceeds the hard line limit"
			);
			return false;
		}
		iSeparator = 1u;
		if ( ((iColumn + iSeparator + iWordSize) > iLineSize) &&
			 (iColumn != 1u) ) {
			if ( !__xrtMailSizeAdd(iOutput, 3u, &iOutput) ) {
				return false;
			}
			if ( sOutput != NULL ) {
				sOutput[iOutput - 3u] = '\r';
				sOutput[iOutput - 2u] = '\n';
				sOutput[iOutput - 1u] = ' ';
			}
			iColumn = 1u;
			iSeparator = 0;
		}
		if ( (iColumn + iSeparator + iWordSize) > XMAIL_HEADER_LINE_HARD ) {
			__xrtMailError(
				XERR_RANGE,
				XMAIL_ERROR_LINE,
				"mail header line exceeds 998 bytes"
			);
			return false;
		}
		if ( !__xrtMailSizeAdd(iOutput, iSeparator, &iOutput) ||
			 !__xrtMailSizeAdd(iOutput, iWordSize, &iOutput) ) {
			return false;
		}
		if ( sOutput != NULL ) {
			if ( iSeparator != 0 ) {
				sOutput[iOutput - iWordSize - 1u] = ' ';
			}
			memcpy(sOutput + iOutput - iWordSize, Value.Data + iPosition,
				iWordSize);
		}
		iColumn += iSeparator + iWordSize;
		iPosition = iEnd;
	}
	if ( !__xrtMailSizeAdd(iOutput, 2u, &iOutput) ) {
		return false;
	}
	if ( sOutput != NULL ) {
		sOutput[iOutput - 2u] = '\r';
		sOutput[iOutput - 1u] = '\n';
		sOutput[iOutput] = 0;
	}
	*pOutputSize = iOutput;
	return true;
}



/* 写出一个完成折叠的字段行。 */
XRT_API bool xrtMailHeaderWrite(
	xstrview Name,
	xstrview Value,
	size_t iLineSize,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;

	if ( !__xrtMailViewValid(Name) || !__xrtMailViewValid(Value) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtMailHeaderNameValid(Name) ) {
		__xrtMailError(
			XERR_ARGUMENT,
			XMAIL_ERROR_HEADER,
			"invalid mail header field name"
		);
		return false;
	}
	if ( !__xrtMailHeaderValueCheck(Value, NULL) ) {
		__xrtMailError(
			XERR_ARGUMENT,
			XMAIL_ERROR_HEADER,
			"invalid mail header field value"
		);
		return false;
	}
	if ( iLineSize == 0 ) {
		iLineSize = XMAIL_HEADER_LINE_DEFAULT;
	}
	if ( (iLineSize < 4u) || (iLineSize > XMAIL_HEADER_LINE_HARD) ||
		 (Name.Size > (XMAIL_HEADER_LINE_HARD - 1u)) ) {
		__xrtMailError(
			XERR_RANGE,
			XMAIL_ERROR_CONFIG,
			"invalid mail header line size"
		);
		return false;
	}
	if ( !__xrtMailHeaderBody(Name, Value, iLineSize, NULL, &iRequired) ) {
		return false;
	}
	if ( xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), Name.Data, Name.Size) ||
		 xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), Value.Data, Value.Size) ||
		 ((sOutput != NULL) && xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			sOutput,
			iCapacity
		 )) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( sOutput == NULL ) {
		*pOutputSize = iRequired;
		return true;
	}
	if ( iCapacity <= iRequired ) {
		*pOutputSize = iRequired;
		__xrtMailSetRange();
		return false;
	}
	if ( xrtMemRangesOverlap(sOutput, iRequired + 1u, Name.Data, Name.Size) ||
		 xrtMemRangesOverlap(sOutput, iRequired + 1u, Value.Data, Value.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return __xrtMailHeaderBody(
		Name,
		Value,
		iLineSize,
		sOutput,
		pOutputSize
	);
}



/* 创建一个完成折叠的独立字段行。 */
XRT_API str xrtMailHeader(
	xstrview Name,
	xstrview Value,
	size_t iLineSize,
	size_t* pOutputSize
)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailHeaderWrite(
		Name,
		Value,
		iLineSize,
		NULL,
		0,
		&iRequired
	) ) {
		return NULL;
	}
	if ( (pOutputSize != NULL) &&
		 (xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			Name.Data,
			Name.Size
		 ) || xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			Value.Data,
			Value.Size
		 )) ) {
		__xrtMailSetInvalidArgument();
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtMailSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtMailHeaderWrite(
		Name,
		Value,
		iLineSize,
		sOutput,
		iRequired + 1u,
		&iRequired
	) ) {
		xrtFree(sOutput);
		return NULL;
	}
	if ( pOutputSize != NULL ) {
		*pOutputSize = iRequired;
	}
	return sOutput;
}

#endif
