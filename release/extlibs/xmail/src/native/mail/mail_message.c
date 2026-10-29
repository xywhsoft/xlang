#include "../internal/xrt_mail.h"



#if defined(XMAIL_FEATURE_MAIL_MESSAGE)

/* 检查消息视图的公开字段是否都具有合法内存范围。 */
static bool __xrtMailMessageViewValid(const xmailmessageview* pMessage)
{
	return xrtMemRangeValid(pMessage, pMessage != NULL ? sizeof(*pMessage) : 0) &&
		(pMessage != NULL) &&
		__xrtMailViewValid(pMessage->Source) &&
		__xrtMailViewValid(pMessage->Headers) &&
		__xrtMailViewValid(pMessage->Body);
}



/* 查找严格 CRLF 空行，并拒绝字段区中的裸换行。 */
static bool __xrtMailMessageSplit(
	xstrview Source,
	size_t iMaxHeaderBytes,
	size_t* pHeaderSize,
	size_t* pBodyStart
)
{
	size_t iLineStart = 0;

	while ( iLineStart < Source.Size ) {
		size_t iLineEnd = iLineStart;

		while ( (iLineEnd < Source.Size) &&
			(Source.Data[iLineEnd] != '\r') &&
			(Source.Data[iLineEnd] != '\n') ) {
			iLineEnd++;
		}
		if ( (iLineEnd >= Source.Size) ||
			(Source.Data[iLineEnd] != '\r') ||
			((iLineEnd + 1u) >= Source.Size) ||
			(Source.Data[iLineEnd + 1u] != '\n') ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_LINE,
				"mail message headers require a CRLF terminator"
			);
			return false;
		}
		if ( iLineEnd > iMaxHeaderBytes ) {
			__xrtMailError(
				XERR_RANGE,
				XMAIL_ERROR_LIMIT,
				"mail message headers exceed the byte limit"
			);
			return false;
		}
		if ( iLineEnd == iLineStart ) {
			*pHeaderSize = iLineStart;
			*pBodyStart = iLineEnd + 2u;
			return true;
		}
		iLineStart = iLineEnd + 2u;
	}
	__xrtMailError(
		XERR_PROTOCOL,
		XMAIL_ERROR_HEADER,
		"mail message has no header separator"
	);
	return false;
}



/* 严格解析消息字段块与正文。 */
XRT_API bool xrtMailMessageParse(
	xstrview Source,
	size_t iMaxHeaderBytes,
	size_t iMaxHeaders,
	xmailmessageview* pMessage
)
{
	xmailmessageview Result;
	xmailheadercursor Cursor;
	xmailheaderview Header;
	xmailnext Next;
	size_t iHeaderSize;
	size_t iBodyStart;
	size_t iHeaderCount = 0;

	if ( !__xrtMailViewValid(Source) ||
		 !xrtMemRangeValid(pMessage, sizeof(*pMessage)) ||
		 xrtMemRangesOverlap(pMessage, sizeof(*pMessage), Source.Data, Source.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( iMaxHeaderBytes == 0 ) {
		iMaxHeaderBytes = XMAIL_MESSAGE_HEADER_BYTES_DEFAULT;
	}
	if ( iMaxHeaders == 0 ) {
		iMaxHeaders = XMAIL_MESSAGE_HEADERS_DEFAULT;
	}
	if ( !__xrtMailMessageSplit(
		Source,
		iMaxHeaderBytes,
		&iHeaderSize,
		&iBodyStart
	) ) {
		return false;
	}
	Result.Source = Source;
	Result.Headers = __xrtMailSlice(Source, 0, iHeaderSize);
	Result.Body = __xrtMailSlice(Source, iBodyStart, Source.Size - iBodyStart);
	if ( !xrtMailHeaderCursorInit(&Cursor, Result.Headers) ) {
		return false;
	}
	while ( (Next = xrtMailHeaderNext(&Cursor, &Header)) == XMAIL_NEXT_ITEM ) {
		if ( iHeaderCount == iMaxHeaders ) {
			__xrtMailError(
				XERR_RANGE,
				XMAIL_ERROR_LIMIT,
				"mail message exceeds the header count limit"
			);
			return false;
		}
		iHeaderCount++;
	}
	if ( Next == XMAIL_NEXT_ERROR ) {
		return false;
	}
	Result.HeaderCount = iHeaderCount;
	*pMessage = Result;
	return true;
}



/* 查找指定序号的消息字段。 */
XRT_API xmailnext xrtMailMessageHeader(
	const xmailmessageview* pMessage,
	xstrview Name,
	size_t iOccurrence,
	xmailheaderview* pHeader
)
{
	xmailheadercursor Cursor;
	xmailheaderview Header;
	xmailnext Next;
	size_t iFound = 0;

	if ( !__xrtMailMessageViewValid(pMessage) ||
		 !__xrtMailViewValid(Name) ||
		 !xrtMemRangeValid(pHeader, sizeof(*pHeader)) ||
		 !xrtMailHeaderNameValid(Name) ||
		 xrtMemRangesOverlap(pHeader, sizeof(*pHeader),
			pMessage->Source.Data, pMessage->Source.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( !xrtMailHeaderCursorInit(&Cursor, pMessage->Headers) ) {
		return XMAIL_NEXT_ERROR;
	}
	while ( (Next = xrtMailHeaderNext(&Cursor, &Header)) == XMAIL_NEXT_ITEM ) {
		if ( !__xrtMailAsciiEqualI(Header.Name, Name) ) {
			continue;
		}
		if ( iFound == iOccurrence ) {
			*pHeader = Header;
			return XMAIL_NEXT_ITEM;
		}
		iFound++;
	}
	return Next;
}



/* 跳过字段值首尾允许出现的空白和折叠空白。 */
static bool __xrtMailTransferSkipFws(xstrview Value, size_t* pPosition)
{
	size_t iPosition = *pPosition;

	while ( iPosition < Value.Size ) {
		if ( (Value.Data[iPosition] == ' ') ||
			 (Value.Data[iPosition] == '\t') ) {
			iPosition++;
			continue;
		}
		if ( Value.Data[iPosition] != '\r' ) {
			break;
		}
		if ( ((iPosition + 2u) >= Value.Size) ||
			 (Value.Data[iPosition + 1u] != '\n') ||
			 ((Value.Data[iPosition + 2u] != ' ') &&
			  (Value.Data[iPosition + 2u] != '\t')) ) {
			return false;
		}
		iPosition += 3u;
		while ( (iPosition < Value.Size) &&
			((Value.Data[iPosition] == ' ') ||
			 (Value.Data[iPosition] == '\t')) ) {
			iPosition++;
		}
	}
	*pPosition = iPosition;
	return true;
}



/* 解析正文传输编码 token。 */
XRT_API xmailtransfer xrtMailTransferParse(xstrview Value)
{
	xstrview Token;
	size_t iPosition = 0;
	size_t iStart;

	if ( !__xrtMailViewValid(Value) ||
		 !__xrtMailTransferSkipFws(Value, &iPosition) ) {
		return XMAIL_TRANSFER_UNKNOWN;
	}
	iStart = iPosition;
	while ( (iPosition < Value.Size) &&
		 (Value.Data[iPosition] != ' ') &&
		 (Value.Data[iPosition] != '\t') &&
		 (Value.Data[iPosition] != '\r') ) {
		if ( Value.Data[iPosition] == '\n' ) {
			return XMAIL_TRANSFER_UNKNOWN;
		}
		iPosition++;
	}
	Token = __xrtMailSlice(Value, iStart, iPosition - iStart);
	if ( !__xrtMailTransferSkipFws(Value, &iPosition) ||
		 (iPosition != Value.Size) || (Token.Size == 0) ) {
		return XMAIL_TRANSFER_UNKNOWN;
	}
	if ( __xrtMailAsciiEqualI(Token, XRT_STR_LITERAL("7bit")) ) {
		return XMAIL_TRANSFER_7BIT;
	}
	if ( __xrtMailAsciiEqualI(Token, XRT_STR_LITERAL("8bit")) ) {
		return XMAIL_TRANSFER_8BIT;
	}
	if ( __xrtMailAsciiEqualI(Token, XRT_STR_LITERAL("binary")) ) {
		return XMAIL_TRANSFER_BINARY;
	}
	if ( __xrtMailAsciiEqualI(
		Token,
		XRT_STR_LITERAL("quoted-printable")
	) ) {
		return XMAIL_TRANSFER_QUOTED_PRINTABLE;
	}
	if ( __xrtMailAsciiEqualI(Token, XRT_STR_LITERAL("base64")) ) {
		return XMAIL_TRANSFER_BASE64;
	}
	return XMAIL_TRANSFER_UNKNOWN;
}



/* 读取并验证消息的唯一传输编码字段。 */
XRT_API bool xrtMailMessageTransfer(
	const xmailmessageview* pMessage,
	xmailtransfer* pTransfer
)
{
	xmailheaderview Header;
	xmailnext Next;
	xmailtransfer Transfer;

	if ( !__xrtMailMessageViewValid(pMessage) ||
		 !xrtMemRangeValid(pTransfer, sizeof(*pTransfer)) ||
		 xrtMemRangesOverlap(pTransfer, sizeof(*pTransfer),
			pMessage->Source.Data, pMessage->Source.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	Next = xrtMailMessageHeader(
		pMessage,
		XRT_STR_LITERAL("Content-Transfer-Encoding"),
		0,
		&Header
	);
	if ( Next == XMAIL_NEXT_ERROR ) {
		return false;
	}
	if ( Next == XMAIL_NEXT_END ) {
		*pTransfer = XMAIL_TRANSFER_7BIT;
		return true;
	}
	if ( xrtMailMessageHeader(
		pMessage,
		XRT_STR_LITERAL("Content-Transfer-Encoding"),
		1,
		&Header
	) != XMAIL_NEXT_END ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_ENCODING,
			"mail message has duplicate transfer encoding fields"
		);
		return false;
	}
	Next = xrtMailMessageHeader(
		pMessage,
		XRT_STR_LITERAL("Content-Transfer-Encoding"),
		0,
		&Header
	);
	if ( Next != XMAIL_NEXT_ITEM ) {
		return false;
	}
	Transfer = xrtMailTransferParse(Header.Value);
	if ( Transfer == XMAIL_TRANSFER_UNKNOWN ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_ENCODING,
			"mail message uses an unsupported transfer encoding"
		);
		return false;
	}
	*pTransfer = Transfer;
	return true;
}



/* 复制无需转换的正文，保持查询和容量失败的事务语义。 */
static bool __xrtMailMessageBodyCopy(
	xstrview Body,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	if ( pOutput == NULL ) {
		*pOutputSize = Body.Size;
		return true;
	}
	if ( iCapacity < Body.Size ) {
		*pOutputSize = Body.Size;
		__xrtMailSetRange();
		return false;
	}
	if ( xrtMemRangesOverlap(pOutput, Body.Size, Body.Data, Body.Size) &&
		 (pOutput != (const void*)Body.Data) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	memmove(pOutput, Body.Data, Body.Size);
	*pOutputSize = Body.Size;
	return true;
}



/* 按调用方选择的传输编码解码正文。 */
XRT_API bool xrtMailMessageBodyWrite(
	const xmailmessageview* pMessage,
	xmailtransfer Transfer,
	uint32 iFlags,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	if ( !__xrtMailMessageViewValid(pMessage) ||
		 !xrtMemRangeValid(pOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize),
			pMessage->Source.Data, pMessage->Source.Size) ||
		 ((pOutput != NULL) && xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			pOutput,
			iCapacity
		 )) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	switch ( Transfer ) {
		case XMAIL_TRANSFER_7BIT:
		case XMAIL_TRANSFER_8BIT:
		case XMAIL_TRANSFER_BINARY:
			return __xrtMailMessageBodyCopy(
				pMessage->Body,
				pOutput,
				iCapacity,
				pOutputSize
			);
		case XMAIL_TRANSFER_QUOTED_PRINTABLE:
			return xrtMailQpDecodeWrite(
				pMessage->Body,
				iFlags,
				pOutput,
				iCapacity,
				pOutputSize
			);
		case XMAIL_TRANSFER_BASE64:
			if ( iFlags != 0 ) {
				__xrtMailSetInvalidArgument();
				return false;
			}
			return xrtMailBase64DecodeWrite(
				pMessage->Body,
				pOutput,
				iCapacity,
				pOutputSize
			);
		default:
			__xrtMailError(
				XERR_ARGUMENT,
				XMAIL_ERROR_ENCODING,
				"unknown mail transfer encoding"
			);
			return false;
	}
}



/* 分配并解码消息正文。 */
XRT_API bytes xrtMailMessageBody(
	const xmailmessageview* pMessage,
	xmailtransfer Transfer,
	uint32 iFlags,
	size_t* pOutputSize
)
{
	size_t iRequired;
	bytes pOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailMessageBodyWrite(
		pMessage,
		Transfer,
		iFlags,
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
	if ( !xrtMailMessageBodyWrite(
		pMessage,
		Transfer,
		iFlags,
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
