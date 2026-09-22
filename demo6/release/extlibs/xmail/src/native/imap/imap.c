#include "../internal/xrt_mail.h"



#if defined(XMAIL_FEATURE_IMAP)

/* 判断字节是否属于 IMAP atom-specials。 */
static bool __xrtImapAtomSpecial(unsigned char iByte)
{
	return (iByte == (unsigned char)'(') || (iByte == (unsigned char)')') ||
		(iByte == (unsigned char)'{') || (iByte == (unsigned char)' ') ||
		(iByte == (unsigned char)'%') || (iByte == (unsigned char)'*') ||
		(iByte == (unsigned char)'"') || (iByte == (unsigned char)'\\') ||
		(iByte == (unsigned char)']');
}



/* 判断 IMAP atom 的完整语法。 */
XRT_API bool xrtImapAtomValid(xstrview Atom)
{
	if ( !__xrtMailViewValid(Atom) || (Atom.Size == 0) ) {
		return false;
	}
	for ( size_t i = 0; i < Atom.Size; i++ ) {
		unsigned char iByte = (unsigned char)Atom.Data[i];

		if ( (iByte <= 31u) || (iByte == 127u) ||
			 __xrtImapAtomSpecial(iByte) ) {
			return false;
		}
	}
	return true;
}



/* 读取一个非零 32 位序号或星号。 */
static bool __xrtImapSequenceNumber(
	xstrview Set,
	size_t* pPosition
)
{
	size_t iPosition = *pPosition;
	uint64 iValue = 0;

	if ( (iPosition < Set.Size) && (Set.Data[iPosition] == '*') ) {
		*pPosition = iPosition + 1u;
		return true;
	}
	if ( (iPosition >= Set.Size) || (Set.Data[iPosition] < '1') ||
		(Set.Data[iPosition] > '9') ) {
		return false;
	}
	while ( (iPosition < Set.Size) && (Set.Data[iPosition] >= '0') &&
		(Set.Data[iPosition] <= '9') ) {
		iValue = (iValue * UINT64_C(10)) +
			(uint64)(Set.Data[iPosition] - '0');
		if ( iValue > UINT32_MAX ) {
			return false;
		}
		iPosition++;
	}
	*pPosition = iPosition;
	return true;
}



/* 验证 RFC sequence-set，并允许 SEARCHRES 的单独 `$`。 */
XRT_API bool xrtImapSequenceSetValid(xstrview Set)
{
	size_t iPosition = 0;

	if ( !__xrtMailViewValid(Set) || (Set.Size == 0) ) {
		return false;
	}
	if ( (Set.Size == 1u) && (Set.Data[0] == '$') ) {
		return true;
	}
	for ( ;; ) {
		if ( !__xrtImapSequenceNumber(Set, &iPosition) ) {
			return false;
		}
		if ( (iPosition < Set.Size) && (Set.Data[iPosition] == ':') ) {
			iPosition++;
			if ( !__xrtImapSequenceNumber(Set, &iPosition) ) {
				return false;
			}
		}
		if ( iPosition == Set.Size ) {
			return true;
		}
		if ( Set.Data[iPosition] != ',' ) {
			return false;
		}
		iPosition++;
		if ( iPosition == Set.Size ) {
			return false;
		}
	}
}



/* 跳过一段 SP。 */
static size_t __xrtImapSpace(xstrview Text, size_t iPosition)
{
	while ( (iPosition < Text.Size) && (Text.Data[iPosition] == ' ') ) {
		iPosition++;
	}
	return iPosition;
}



/* 读取一个由 SP 终止的 atom 视图。 */
static bool __xrtImapAtom(
	xstrview Text,
	size_t* pPosition,
	xstrview* pAtom
)
{
	size_t iPosition = *pPosition;
	size_t iStart = iPosition;

	while ( (iPosition < Text.Size) && (Text.Data[iPosition] != ' ') ) {
		iPosition++;
	}
	*pAtom = __xrtMailSlice(Text, iStart, iPosition - iStart);
	if ( !xrtImapAtomValid(*pAtom) ) {
		return false;
	}
	*pPosition = iPosition;
	return true;
}



/* 把状态 atom 映射为稳定枚举。 */
static ximapstatus __xrtImapStatus(xstrview Atom)
{
	if ( __xrtMailAsciiEqualI(Atom, XRT_STR_LITERAL("OK")) ) {
		return XIMAP_STATUS_OK;
	}
	if ( __xrtMailAsciiEqualI(Atom, XRT_STR_LITERAL("NO")) ) {
		return XIMAP_STATUS_NO;
	}
	if ( __xrtMailAsciiEqualI(Atom, XRT_STR_LITERAL("BAD")) ) {
		return XIMAP_STATUS_BAD;
	}
	if ( __xrtMailAsciiEqualI(Atom, XRT_STR_LITERAL("PREAUTH")) ) {
		return XIMAP_STATUS_PREAUTH;
	}
	if ( __xrtMailAsciiEqualI(Atom, XRT_STR_LITERAL("BYE")) ) {
		return XIMAP_STATUS_BYE;
	}
	return XIMAP_STATUS_NONE;
}



/* 验证响应文本不含线路控制字节。 */
static bool __xrtImapTextValid(xstrview Text)
{
	for ( size_t i = 0; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( (iByte == 0) || (iByte == (unsigned char)'\r') ||
			 (iByte == (unsigned char)'\n') ||
			 ((iByte < 32u) && (iByte != (unsigned char)'\t')) ) {
			return false;
		}
	}
	return true;
}



/* 解析 IMAP 响应行。 */
XRT_API bool xrtImapResponseParse(
	xstrview Line,
	ximapresponseview* pResponse
)
{
	ximapresponseview Response;
	xstrview Atom;
	size_t iPosition;

	if ( !__xrtMailViewValid(Line) ||
		 !xrtMemRangeValid(pResponse, sizeof(*pResponse)) ||
		 xrtMemRangesOverlap(pResponse, sizeof(*pResponse), Line.Data,
			Line.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtImapTextValid(Line) || (Line.Size == 0) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid IMAP response line"
		);
		return false;
	}
	Response.Source = Line;
	Response.Tag = __xrtMailView(NULL, 0);
	Response.Text = __xrtMailView(NULL, 0);
	Response.Status = XIMAP_STATUS_NONE;
	if ( Line.Data[0] == '+' ) {
		if ( (Line.Size > 1u) && (Line.Data[1] != ' ') ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_PROTOCOL,
				"invalid IMAP continuation response"
			);
			return false;
		}
		iPosition = __xrtImapSpace(Line, 1u);
		Response.Kind = XIMAP_RESPONSE_CONTINUATION;
		Response.Text = __xrtMailSlice(Line, iPosition, Line.Size - iPosition);
		*pResponse = Response;
		return true;
	}
	if ( Line.Data[0] == '*' ) {
		if ( (Line.Size < 3u) || (Line.Data[1] != ' ') ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_PROTOCOL,
				"invalid IMAP untagged response"
			);
			return false;
		}
		iPosition = 2u;
		if ( !__xrtImapAtom(Line, &iPosition, &Atom) ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_PROTOCOL,
				"invalid IMAP untagged response atom"
			);
			return false;
		}
		Response.Kind = XIMAP_RESPONSE_UNTAGGED;
		Response.Status = __xrtImapStatus(Atom);
		if ( Response.Status == XIMAP_STATUS_NONE ) {
			Response.Text = __xrtMailSlice(Line, 2u, Line.Size - 2u);
		} else {
			iPosition = __xrtImapSpace(Line, iPosition);
			Response.Text = __xrtMailSlice(
				Line,
				iPosition,
				Line.Size - iPosition
			);
		}
		*pResponse = Response;
		return true;
	}
	iPosition = 0;
	if ( !__xrtImapAtom(Line, &iPosition, &Response.Tag) ||
		 (Response.Tag.Data[0] == '+') || (iPosition == Line.Size) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid IMAP tagged response"
		);
		return false;
	}
	iPosition = __xrtImapSpace(Line, iPosition);
	if ( !__xrtImapAtom(Line, &iPosition, &Atom) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid IMAP tagged status"
		);
		return false;
	}
	Response.Status = __xrtImapStatus(Atom);
	if ( (Response.Status != XIMAP_STATUS_OK) &&
		 (Response.Status != XIMAP_STATUS_NO) &&
		 (Response.Status != XIMAP_STATUS_BAD) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid IMAP tagged status"
		);
		return false;
	}
	iPosition = __xrtImapSpace(Line, iPosition);
	Response.Kind = XIMAP_RESPONSE_TAGGED;
	Response.Text = __xrtMailSlice(Line, iPosition, Line.Size - iPosition);
	*pResponse = Response;
	return true;
}



/* 解析行尾 IMAP literal 标记。 */
XRT_API xmailnext xrtImapLiteralParse(
	xstrview Line,
	ximapliteralview* pLiteral
)
{
	ximapliteralview Literal;
	size_t iOpen = XRT_NPOS;
	size_t iDigits;
	size_t iPosition;
	size_t iEnd;
	uint64 iValue = 0;
	bool bPlus = false;

	if ( !__xrtMailViewValid(Line) ||
		 !xrtMemRangeValid(pLiteral, sizeof(*pLiteral)) ||
		 xrtMemRangesOverlap(pLiteral, sizeof(*pLiteral), Line.Data, Line.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( (Line.Size < 3u) || (Line.Data[Line.Size - 1u] != '}') ) {
		return XMAIL_NEXT_END;
	}
	for ( size_t i = Line.Size - 1u; i > 0; i-- ) {
		if ( Line.Data[i - 1u] == '{' ) {
			iOpen = i - 1u;
			break;
		}
	}
	if ( iOpen == XRT_NPOS ) {
		return XMAIL_NEXT_END;
	}
	Literal.Binary = (iOpen != 0) && (Line.Data[iOpen - 1u] == '~');
	iPosition = iOpen + 1u;
	if ( (Line.Size >= 4u) && (Line.Data[Line.Size - 2u] == '+') ) {
		bPlus = true;
	}
	iDigits = iPosition;
	iEnd = Line.Size - 1u - (bPlus ? 1u : 0u);
	while ( iPosition < iEnd ) {
		unsigned char iByte = (unsigned char)Line.Data[iPosition++];

		if ( (iByte < (unsigned char)'0') || (iByte > (unsigned char)'9') ||
			 (iValue > ((UINT64_MAX -
			  (uint64)(iByte - (unsigned char)'0')) / UINT64_C(10))) ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_PROTOCOL,
				"invalid IMAP literal marker"
			);
			return XMAIL_NEXT_ERROR;
		}
		iValue = (iValue * UINT64_C(10)) +
			(uint64)(iByte - (unsigned char)'0');
	}
	if ( (iPosition == iDigits) || (iValue > (uint64)SIZE_MAX) ) {
		__xrtMailError(
			XERR_RANGE,
			XMAIL_ERROR_LIMIT,
			"IMAP literal size is invalid or too large"
		);
		return XMAIL_NEXT_ERROR;
	}
	Literal.Source = __xrtMailSlice(
		Line,
		Literal.Binary ? iOpen - 1u : iOpen,
		Line.Size - (Literal.Binary ? iOpen - 1u : iOpen)
	);
	Literal.Size = (size_t)iValue;
	Literal.NonSynchronizing = bPlus;
	*pLiteral = Literal;
	return XMAIL_NEXT_ITEM;
}



/* 解析状态文本开头的方括号响应码。 */
XRT_API xmailnext xrtImapCodeParse(
	xstrview Text,
	ximapcodeview* pCode
)
{
	ximapcodeview Code;
	size_t iClose = XRT_NPOS;
	size_t iNameEnd;
	size_t iArguments;
	size_t iArgumentsEnd;
	size_t iText;

	if ( !__xrtMailViewValid(Text) ||
		!xrtMemRangeValid(pCode, sizeof(*pCode)) ||
		xrtMemRangesOverlap(pCode, sizeof(*pCode), Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( (Text.Size == 0) || (Text.Data[0] != '[') ) {
		return XMAIL_NEXT_END;
	}
	if ( !__xrtImapTextValid(Text) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid IMAP response code text"
		);
		return XMAIL_NEXT_ERROR;
	}
	for ( size_t i = 1u; i < Text.Size; i++ ) {
		if ( Text.Data[i] == ']' ) {
			iClose = i;
			break;
		}
	}
	if ( iClose == XRT_NPOS ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"unterminated IMAP response code"
		);
		return XMAIL_NEXT_ERROR;
	}
	iNameEnd = 1u;
	while ( (iNameEnd < iClose) && (Text.Data[iNameEnd] != ' ') ) {
		iNameEnd++;
	}
	Code.Name = __xrtMailSlice(Text, 1u, iNameEnd - 1u);
	if ( !xrtImapAtomValid(Code.Name) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid IMAP response code name"
		);
		return XMAIL_NEXT_ERROR;
	}
	iArguments = __xrtImapSpace(Text, iNameEnd);
	iArgumentsEnd = iClose;
	while ( (iArgumentsEnd > iArguments) &&
		(Text.Data[iArgumentsEnd - 1u] == ' ') ) {
		iArgumentsEnd--;
	}
	iText = iClose + 1u;
	if ( iText < Text.Size ) {
		if ( Text.Data[iText] != ' ' ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_PROTOCOL,
				"invalid IMAP response code separator"
			);
			return XMAIL_NEXT_ERROR;
		}
		iText = __xrtImapSpace(Text, iText);
	}
	Code.Source = __xrtMailSlice(Text, 0, iClose + 1u);
	Code.Arguments = __xrtMailSlice(
		Text,
		iArguments,
		iArgumentsEnd - iArguments
	);
	Code.Text = __xrtMailSlice(Text, iText, Text.Size - iText);
	*pCode = Code;
	return XMAIL_NEXT_ITEM;
}



/* 解析数字开头的非标记响应文本。 */
XRT_API xmailnext xrtImapNumberParse(
	xstrview Text,
	ximapnumberview* pNumber
)
{
	ximapnumberview Number;
	size_t iPosition = 0;
	size_t iNameStart;
	size_t iNameEnd;
	uint64 iValue = 0;

	if ( !__xrtMailViewValid(Text) ||
		!xrtMemRangeValid(pNumber, sizeof(*pNumber)) ||
		xrtMemRangesOverlap(pNumber, sizeof(*pNumber), Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( (Text.Size == 0) || (Text.Data[0] < '0') ||
		(Text.Data[0] > '9') ) {
		return XMAIL_NEXT_END;
	}
	if ( !__xrtImapTextValid(Text) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid IMAP numeric response text"
		);
		return XMAIL_NEXT_ERROR;
	}
	while ( (iPosition < Text.Size) &&
		(Text.Data[iPosition] >= '0') && (Text.Data[iPosition] <= '9') ) {
		uint64 iDigit = (uint64)(Text.Data[iPosition] - '0');

		if ( iValue > ((UINT64_MAX - iDigit) / UINT64_C(10)) ) {
			__xrtMailError(
				XERR_RANGE,
				XMAIL_ERROR_LIMIT,
				"IMAP numeric response exceeds uint64"
			);
			return XMAIL_NEXT_ERROR;
		}
		iValue = (iValue * UINT64_C(10)) + iDigit;
		iPosition++;
	}
	if ( (iPosition == Text.Size) || (Text.Data[iPosition] != ' ') ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid IMAP numeric response separator"
		);
		return XMAIL_NEXT_ERROR;
	}
	iNameStart = __xrtImapSpace(Text, iPosition);
	iNameEnd = iNameStart;
	while ( (iNameEnd < Text.Size) && (Text.Data[iNameEnd] != ' ') ) {
		iNameEnd++;
	}
	Number.Name = __xrtMailSlice(Text, iNameStart, iNameEnd - iNameStart);
	if ( !xrtImapAtomValid(Number.Name) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid IMAP numeric response name"
		);
		return XMAIL_NEXT_ERROR;
	}
	iPosition = __xrtImapSpace(Text, iNameEnd);
	Number.Source = Text;
	Number.Text = __xrtMailSlice(Text, iPosition, Text.Size - iPosition);
	Number.Number = iValue;
	*pNumber = Number;
	return XMAIL_NEXT_ITEM;
}



/* 初始化简单 atom 游标。 */
XRT_API bool xrtImapAtomCursorInit(
	ximapatomcursor* pCursor,
	xstrview Text
)
{
	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		 !__xrtMailViewValid(Text) ||
		 xrtMemRangesOverlap(pCursor, sizeof(*pCursor), Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	pCursor->Text = Text;
	pCursor->Position = 0;
	pCursor->Done = false;
	return true;
}



/* 返回空白分隔的下一 atom。 */
XRT_API xmailnext xrtImapAtomNext(
	ximapatomcursor* pCursor,
	xstrview* pAtom
)
{
	size_t iPosition;
	xstrview Atom;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		 !xrtMemRangeValid(pAtom, sizeof(*pAtom)) ||
		 (pCursor == NULL) || !__xrtMailViewValid(pCursor->Text) ||
		 (pCursor->Position > pCursor->Text.Size) ||
		 xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pAtom, sizeof(*pAtom)) ||
		 xrtMemRangesOverlap(pAtom, sizeof(*pAtom), pCursor->Text.Data,
			pCursor->Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( pCursor->Done ) {
		return XMAIL_NEXT_END;
	}
	iPosition = __xrtImapSpace(pCursor->Text, pCursor->Position);
	if ( iPosition == pCursor->Text.Size ) {
		pCursor->Position = iPosition;
		pCursor->Done = true;
		return XMAIL_NEXT_END;
	}
	if ( !__xrtImapAtom(pCursor->Text, &iPosition, &Atom) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid IMAP atom list"
		);
		return XMAIL_NEXT_ERROR;
	}
	pCursor->Position = iPosition;
	*pAtom = Atom;
	return XMAIL_NEXT_ITEM;
}



/* 查找常用 IMAP capability 的稳定标记。 */
XRT_API uint64 xrtImapCapability(xstrview Capability)
{
	#define __XRT_IMAP_CAPABILITY(Name, Value) \
		{ Name, sizeof(Name) - 1u, Value }

	static const struct {
		cstr Name;
		size_t Size;
		uint64 Value;
	} arrCapabilities[] = {
		__XRT_IMAP_CAPABILITY("IMAP4REV1", XIMAP_CAP_IMAP4REV1),
		__XRT_IMAP_CAPABILITY("IMAP4REV2", XIMAP_CAP_IMAP4REV2),
		__XRT_IMAP_CAPABILITY("STARTTLS", XIMAP_CAP_STARTTLS),
		__XRT_IMAP_CAPABILITY("AUTH=PLAIN", XIMAP_CAP_AUTH_PLAIN),
		__XRT_IMAP_CAPABILITY("AUTH=XOAUTH2", XIMAP_CAP_AUTH_XOAUTH2),
		__XRT_IMAP_CAPABILITY("IDLE", XIMAP_CAP_IDLE),
		__XRT_IMAP_CAPABILITY("UIDPLUS", XIMAP_CAP_UIDPLUS),
		__XRT_IMAP_CAPABILITY("MOVE", XIMAP_CAP_MOVE),
		__XRT_IMAP_CAPABILITY("NAMESPACE", XIMAP_CAP_NAMESPACE),
		__XRT_IMAP_CAPABILITY("ENABLE", XIMAP_CAP_ENABLE),
		__XRT_IMAP_CAPABILITY("UTF8=ACCEPT", XIMAP_CAP_UTF8_ACCEPT),
		__XRT_IMAP_CAPABILITY("CONDSTORE", XIMAP_CAP_CONDSTORE),
		__XRT_IMAP_CAPABILITY("QRESYNC", XIMAP_CAP_QRESYNC),
		__XRT_IMAP_CAPABILITY("LITERAL+", XIMAP_CAP_LITERAL_PLUS),
		__XRT_IMAP_CAPABILITY("SASL-IR", XIMAP_CAP_SASL_IR),
		__XRT_IMAP_CAPABILITY("BINARY", XIMAP_CAP_BINARY),
		__XRT_IMAP_CAPABILITY("LOGINDISABLED", XIMAP_CAP_LOGIN_DISABLED),
		__XRT_IMAP_CAPABILITY(
			"AUTH=OAUTHBEARER",
			XIMAP_CAP_AUTH_OAUTHBEARER
		),
		__XRT_IMAP_CAPABILITY("UNSELECT", XIMAP_CAP_UNSELECT),
		__XRT_IMAP_CAPABILITY("ESEARCH", XIMAP_CAP_ESEARCH),
		__XRT_IMAP_CAPABILITY("LIST-EXTENDED", XIMAP_CAP_LIST_EXTENDED),
		__XRT_IMAP_CAPABILITY("SPECIAL-USE", XIMAP_CAP_SPECIAL_USE),
		__XRT_IMAP_CAPABILITY("SORT", XIMAP_CAP_SORT),
		__XRT_IMAP_CAPABILITY(
			"THREAD=REFERENCES",
			XIMAP_CAP_THREAD_REFERENCES
		),
		__XRT_IMAP_CAPABILITY("QUOTA", XIMAP_CAP_QUOTA),
		__XRT_IMAP_CAPABILITY("ACL", XIMAP_CAP_ACL),
		__XRT_IMAP_CAPABILITY("METADATA", XIMAP_CAP_METADATA),
		__XRT_IMAP_CAPABILITY("NOTIFY", XIMAP_CAP_NOTIFY),
		__XRT_IMAP_CAPABILITY(
			"COMPRESS=DEFLATE",
			XIMAP_CAP_COMPRESS_DEFLATE
		),
		__XRT_IMAP_CAPABILITY("APPENDLIMIT", XIMAP_CAP_APPENDLIMIT),
		__XRT_IMAP_CAPABILITY("LITERAL-", XIMAP_CAP_LITERAL_MINUS)
	};

	if ( !__xrtMailViewValid(Capability) ) {
		return 0;
	}
	if ( (Capability.Size > sizeof("APPENDLIMIT=") - 1u) &&
		__xrtMailAsciiEqualI(
			__xrtMailSlice(
				Capability,
				0,
				sizeof("APPENDLIMIT=") - 1u
			),
			XRT_STR_LITERAL("APPENDLIMIT=")
		) ) {
		return XIMAP_CAP_APPENDLIMIT;
	}
	for ( size_t i = 0; i < sizeof(arrCapabilities) /
		sizeof(arrCapabilities[0]); i++ ) {
		if ( __xrtMailAsciiEqualI(
			Capability,
			__xrtMailView(arrCapabilities[i].Name, arrCapabilities[i].Size)
		) ) {
			return arrCapabilities[i].Value;
		}
	}
	return 0;

	#undef __XRT_IMAP_CAPABILITY
}



/* 计算 quoted string 大小并验证输入。 */
static bool __xrtImapQuoteMeasure(xstrview Text, size_t* pRequired)
{
	size_t iRequired = 2u;

	for ( size_t i = 0; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( (iByte < 32u) || (iByte == 127u) ) {
			__xrtMailError(
				XERR_ARGUMENT,
				XMAIL_ERROR_PROTOCOL,
				"IMAP quoted string requires a literal for control data"
			);
			return false;
		}
		if ( !__xrtMailSizeAdd(iRequired,
			((iByte == (unsigned char)'"') ||
			 (iByte == (unsigned char)'\\')) ? 2u : 1u,
			&iRequired) ) {
			return false;
		}
	}
	*pRequired = iRequired;
	return true;
}



/* 写出 IMAP quoted string。 */
XRT_API bool xrtImapQuoteWrite(
	xstrview Text,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;
	size_t iOutput = 0;

	if ( !__xrtMailViewValid(Text) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtImapQuoteMeasure(Text, &iRequired) ) {
		return false;
	}
	if ( xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), Text.Data,
			Text.Size) ||
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
	if ( xrtMemRangesOverlap(sOutput, iRequired + 1u, Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	sOutput[iOutput++] = '"';
	for ( size_t i = 0; i < Text.Size; i++ ) {
		char iByte = Text.Data[i];

		if ( (iByte == '"') || (iByte == '\\') ) {
			sOutput[iOutput++] = '\\';
		}
		sOutput[iOutput++] = iByte;
	}
	sOutput[iOutput++] = '"';
	sOutput[iOutput] = 0;
	*pOutputSize = iOutput;
	return true;
}



/* 分配并写出 IMAP quoted string。 */
XRT_API str xrtImapQuote(xstrview Text, size_t* pOutputSize)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtImapQuoteWrite(Text, NULL, 0, &iRequired) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtImapQuoteWrite(
		Text,
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



/* 验证命令参数只含单行可发送字节。 */
static bool __xrtImapArgumentsValid(xstrview Arguments)
{
	for ( size_t i = 0; i < Arguments.Size; i++ ) {
		unsigned char iByte = (unsigned char)Arguments.Data[i];

		if ( (iByte < 32u) || (iByte == 127u) ) {
			return false;
		}
	}
	return true;
}



/* 写出安全 IMAP 命令行。 */
XRT_API bool xrtImapCommandWrite(
	xstrview Tag,
	xstrview Command,
	xstrview Arguments,
	size_t iMaxLine,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;
	size_t iSeparator = Arguments.Size != 0 ? 1u : 0;

	if ( !__xrtMailViewValid(Tag) || !__xrtMailViewValid(Command) ||
		 !__xrtMailViewValid(Arguments) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtImapAtomValid(Tag) || (Tag.Data[0] == '+') ||
		 !xrtImapAtomValid(Command) ||
		 !__xrtImapArgumentsValid(Arguments) ) {
		__xrtMailError(
			XERR_ARGUMENT,
			XMAIL_ERROR_PROTOCOL,
			"invalid IMAP command"
		);
		return false;
	}
	if ( iMaxLine == 0 ) {
		iMaxLine = XIMAP_COMMAND_LINE_DEFAULT;
	}
	if ( !__xrtMailSizeAdd(Tag.Size, 1u, &iRequired) ||
		 !__xrtMailSizeAdd(iRequired, Command.Size, &iRequired) ||
		 !__xrtMailSizeAdd(iRequired, iSeparator, &iRequired) ||
		 !__xrtMailSizeAdd(iRequired, Arguments.Size, &iRequired) ||
		 !__xrtMailSizeAdd(iRequired, 2u, &iRequired) ) {
		return false;
	}
	if ( iRequired > iMaxLine ) {
		__xrtMailError(
			XERR_RANGE,
			XMAIL_ERROR_LIMIT,
			"IMAP command exceeds the line limit"
		);
		return false;
	}
	if ( xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), Tag.Data,
			Tag.Size) ||
		 xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), Command.Data,
			Command.Size) ||
		 xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), Arguments.Data,
			Arguments.Size) ||
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
	if ( xrtMemRangesOverlap(sOutput, iRequired + 1u, Tag.Data, Tag.Size) ||
		 xrtMemRangesOverlap(sOutput, iRequired + 1u, Command.Data,
			Command.Size) ||
		 xrtMemRangesOverlap(sOutput, iRequired + 1u, Arguments.Data,
			Arguments.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	memcpy(sOutput, Tag.Data, Tag.Size);
	sOutput[Tag.Size] = ' ';
	memcpy(sOutput + Tag.Size + 1u, Command.Data, Command.Size);
	if ( iSeparator != 0 ) {
		sOutput[Tag.Size + 1u + Command.Size] = ' ';
		memcpy(
			sOutput + Tag.Size + Command.Size + 2u,
			Arguments.Data,
			Arguments.Size
		);
	}
	sOutput[iRequired - 2u] = '\r';
	sOutput[iRequired - 1u] = '\n';
	sOutput[iRequired] = 0;
	*pOutputSize = iRequired;
	return true;
}



/* 分配并写出 IMAP 命令行。 */
XRT_API str xrtImapCommand(
	xstrview Tag,
	xstrview Command,
	xstrview Arguments,
	size_t iMaxLine,
	size_t* pOutputSize
)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtImapCommandWrite(
		Tag,
		Command,
		Arguments,
		iMaxLine,
		NULL,
		0,
		&iRequired
	) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtImapCommandWrite(
		Tag,
		Command,
		Arguments,
		iMaxLine,
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
