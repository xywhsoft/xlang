#include <xrt/imap_data.h>

#include "../internal/xrt_mail.h"



#if defined(XMAIL_FEATURE_IMAP_DATA)

/* 创建稳定的 IMAP 数据语法错误。 */
static bool __xrtImapDataError(xerrkind Kind, cstr sMessage)
{
	__xrtMailError(Kind, XMAIL_ERROR_PROTOCOL, sMessage);
	return false;
}



/* 跳过 IMAP 数据项之间的 SP。 */
static size_t __xrtImapDataSpace(xstrview Text, size_t iPosition)
{
	while ( (iPosition < Text.Size) && (Text.Data[iPosition] == ' ') ) {
		iPosition++;
	}
	return iPosition;
}



/* 验证数据片段不含线路控制字节。 */
static bool __xrtImapDataTextValid(xstrview Text)
{
	if ( !__xrtMailViewValid(Text) ) {
		return false;
	}
	for ( size_t i = 0; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( (iByte == 0) || (iByte == (unsigned char)'\r') ||
			(iByte == (unsigned char)'\n') || (iByte < 32u) ||
			(iByte == 127u) ) {
			return false;
		}
	}
	return true;
}



/* 解析并保留 quoted string 的线路表示。 */
static bool __xrtImapDataQuoted(
	xstrview Text,
	size_t* pPosition,
	ximapdataview* pValue
)
{
	size_t iStart = *pPosition;
	size_t iPosition = iStart + 1u;

	while ( iPosition < Text.Size ) {
		unsigned char iByte = (unsigned char)Text.Data[iPosition];

		if ( iByte == (unsigned char)'"' ) {
			pValue->Source = __xrtMailSlice(
				Text,
				iStart,
				(iPosition - iStart) + 1u
			);
			pValue->Value = __xrtMailSlice(
				Text,
				iStart + 1u,
				iPosition - iStart - 1u
			);
			pValue->Kind = XIMAP_DATA_QUOTED;
			*pPosition = iPosition + 1u;
			return true;
		}
		if ( iByte == (unsigned char)'\\' ) {
			iPosition++;
			if ( (iPosition >= Text.Size) ||
				((Text.Data[iPosition] != '"') &&
				 (Text.Data[iPosition] != '\\')) ) {
				return __xrtImapDataError(
					XERR_PROTOCOL,
					"invalid IMAP quoted string escape"
				);
			}
		}
		iPosition++;
	}
	return __xrtImapDataError(
		XERR_PROTOCOL,
		"unterminated IMAP quoted string"
	);
}



/* 扫描一个完整、可嵌套但不跨 literal 的括号列表。 */
static bool __xrtImapDataList(
	xstrview Text,
	size_t* pPosition,
	ximapdataview* pValue
)
{
	size_t iStart = *pPosition;
	size_t iPosition = iStart + 1u;
	size_t iDepth = 1u;

	while ( iPosition < Text.Size ) {
		char iByte = Text.Data[iPosition];

		if ( iByte == '"' ) {
			ximapdataview Quoted;

			if ( !__xrtImapDataQuoted(Text, &iPosition, &Quoted) ) {
				return false;
			}
			continue;
		}
		if ( iByte == '(' ) {
			if ( iDepth == SIZE_MAX ) {
				return __xrtImapDataError(
					XERR_RANGE,
					"IMAP data nesting exceeds size_t"
				);
			}
			iDepth++;
		} else if ( iByte == ')' ) {
			iDepth--;
			if ( iDepth == 0 ) {
				pValue->Source = __xrtMailSlice(
					Text,
					iStart,
					(iPosition - iStart) + 1u
				);
				pValue->Value = __xrtMailSlice(
					Text,
					iStart + 1u,
					iPosition - iStart - 1u
				);
				pValue->Kind = XIMAP_DATA_LIST;
				*pPosition = iPosition + 1u;
				return true;
			}
		}
		iPosition++;
	}
	return __xrtImapDataError(
		XERR_PROTOCOL,
		"unterminated IMAP data list"
	);
}



/* 解析 number 或非空原子值。 */
static bool __xrtImapDataAtom(
	xstrview Text,
	size_t* pPosition,
	ximapdataview* pValue
)
{
	size_t iStart = *pPosition;
	size_t iPosition = iStart;
	uint64 iNumber = 0;
	bool bNumber = true;
	bool bOverflow = false;

	while ( (iPosition < Text.Size) && (Text.Data[iPosition] != ' ') &&
		(Text.Data[iPosition] != '(') && (Text.Data[iPosition] != ')') ) {
		unsigned char iByte = (unsigned char)Text.Data[iPosition];

		if ( (iByte < (unsigned char)'0') ||
			(iByte > (unsigned char)'9') ) {
			bNumber = false;
		} else if ( bNumber && !bOverflow ) {
			uint64 iDigit = (uint64)(iByte - (unsigned char)'0');

			if ( iNumber > ((UINT64_MAX - iDigit) / UINT64_C(10)) ) {
				bOverflow = true;
			} else {
				iNumber = (iNumber * UINT64_C(10)) + iDigit;
			}
		}
		iPosition++;
	}
	if ( iPosition == iStart ) {
		return __xrtImapDataError(
			XERR_PROTOCOL,
			"invalid empty IMAP data value"
		);
	}
	pValue->Source = __xrtMailSlice(Text, iStart, iPosition - iStart);
	pValue->Value = pValue->Source;
	if ( bNumber ) {
		if ( bOverflow ) {
			return __xrtImapDataError(
				XERR_RANGE,
				"IMAP number exceeds uint64"
			);
		}
		pValue->Kind = XIMAP_DATA_NUMBER;
		pValue->Number = iNumber;
	} else if ( __xrtMailAsciiEqualI(
		pValue->Source,
		XRT_STR_LITERAL("NIL")
	) ) {
		pValue->Kind = XIMAP_DATA_NIL;
		pValue->Value = __xrtMailView(NULL, 0);
	} else {
		pValue->Kind = XIMAP_DATA_ATOM;
	}
	*pPosition = iPosition;
	return true;
}



/* 解析当前位置的一项数据值。 */
static bool __xrtImapDataValue(
	xstrview Text,
	size_t* pPosition,
	ximapdataview* pValue
)
{
	size_t iPosition = *pPosition;
	ximapliteralview Literal;
	xmailnext Next;

	memset(pValue, 0, sizeof(*pValue));
	if ( Text.Data[iPosition] == '"' ) {
		return __xrtImapDataQuoted(Text, pPosition, pValue);
	}
	if ( Text.Data[iPosition] == '(' ) {
		return __xrtImapDataList(Text, pPosition, pValue);
	}
	if ( (Text.Data[iPosition] == '{') ||
		((Text.Data[iPosition] == '~') &&
		 ((iPosition + 1u) < Text.Size) &&
		 (Text.Data[iPosition + 1u] == '{')) ) {
		xstrview Marker = __xrtMailSlice(
			Text,
			iPosition,
			Text.Size - iPosition
		);

		Next = xrtImapLiteralParse(Marker, &Literal);
		if ( Next != XMAIL_NEXT_ITEM ) {
			return Next == XMAIL_NEXT_ERROR ? false : __xrtImapDataError(
				XERR_PROTOCOL,
				"invalid IMAP literal data value"
			);
		}
		if ( (Literal.Source.Data != Marker.Data) ||
			(Literal.Source.Size != Marker.Size) ) {
			return __xrtImapDataError(
				XERR_PROTOCOL,
				"IMAP literal marker must occupy the line tail"
			);
		}
		pValue->Source = Literal.Source;
		pValue->Kind = XIMAP_DATA_LITERAL;
		pValue->LiteralSize = Literal.Size;
		pValue->LiteralNonSynchronizing = Literal.NonSynchronizing;
		pValue->LiteralBinary = Literal.Binary;
		*pPosition = Text.Size;
		return true;
	}
	return __xrtImapDataAtom(Text, pPosition, pValue);
}



/* 提取完整非标记响应的名称后数据区。 */
static bool __xrtImapDataPayload(
	xstrview Text,
	xstrview Name,
	xstrview* pPayload
)
{
	if ( !__xrtImapDataTextValid(Text) || (Text.Size < Name.Size) ||
		!__xrtMailAsciiEqualI(
			__xrtMailSlice(Text, 0, Name.Size),
			Name
		) || ((Text.Size > Name.Size) &&
		(Text.Data[Name.Size] != ' ')) ) {
		return __xrtImapDataError(
			XERR_PROTOCOL,
			"unexpected IMAP data response name"
		);
	}
	*pPayload = __xrtMailSlice(
		Text,
		__xrtImapDataSpace(Text, Name.Size),
		Text.Size - __xrtImapDataSpace(Text, Name.Size)
	);
	return true;
}



/* 初始化通用 IMAP 数据游标。 */
XRT_API bool xrtImapDataCursorInit(
	ximapdatacursor* pCursor,
	xstrview Text
)
{
	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!__xrtImapDataTextValid(Text) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	pCursor->Text = Text;
	pCursor->Position = 0;
	pCursor->Done = false;
	return true;
}



/* 返回一层数据区的下一项。 */
XRT_API xmailnext xrtImapDataNext(
	ximapdatacursor* pCursor,
	ximapdataview* pValue
)
{
	size_t iPosition;
	ximapdataview Value;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!xrtMemRangeValid(pValue, sizeof(*pValue)) ||
		(pCursor == NULL) || !__xrtImapDataTextValid(pCursor->Text) ||
		(pCursor->Position > pCursor->Text.Size) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pValue, sizeof(*pValue)) ||
		xrtMemRangesOverlap(pValue, sizeof(*pValue), pCursor->Text.Data,
			pCursor->Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( pCursor->Done ) {
		return XMAIL_NEXT_END;
	}
	iPosition = __xrtImapDataSpace(pCursor->Text, pCursor->Position);
	if ( iPosition == pCursor->Text.Size ) {
		pCursor->Position = iPosition;
		pCursor->Done = true;
		return XMAIL_NEXT_END;
	}
	if ( pCursor->Text.Data[iPosition] == ')' ) {
		__xrtImapDataError(XERR_PROTOCOL, "unexpected IMAP list terminator");
		return XMAIL_NEXT_ERROR;
	}
	if ( !__xrtImapDataValue(pCursor->Text, &iPosition, &Value) ) {
		return XMAIL_NEXT_ERROR;
	}
	pCursor->Position = iPosition;
	*pValue = Value;
	return XMAIL_NEXT_ITEM;
}



/* 解码 quoted string，或直接复制 atom 和 NIL。 */
XRT_API bool xrtImapStringWrite(
	const ximapdataview* pValue,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired = 0;
	size_t iOutput = 0;

	if ( !xrtMemRangeValid(pValue, sizeof(*pValue)) ||
		!xrtMemRangeValid(sOutput, iCapacity) ||
		!xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		(pValue == NULL) || !__xrtMailViewValid(pValue->Source) ||
		!__xrtMailViewValid(pValue->Value) ||
		xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize),
			pValue->Source.Data, pValue->Source.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( (pValue->Kind != XIMAP_DATA_ATOM) &&
		(pValue->Kind != XIMAP_DATA_QUOTED) &&
		(pValue->Kind != XIMAP_DATA_NIL) ) {
		return __xrtImapDataError(
			XERR_STATE,
			"IMAP data value is not a string"
		);
	}
	if ( pValue->Kind == XIMAP_DATA_QUOTED ) {
		for ( size_t i = 0; i < pValue->Value.Size; i++ ) {
			if ( pValue->Value.Data[i] == '\\' ) {
				i++;
				if ( (i >= pValue->Value.Size) ||
					((pValue->Value.Data[i] != '"') &&
					 (pValue->Value.Data[i] != '\\')) ) {
					return __xrtImapDataError(
						XERR_PROTOCOL,
						"invalid IMAP quoted string escape"
					);
				}
			}
			iRequired++;
		}
	} else if ( pValue->Kind == XIMAP_DATA_ATOM ) {
		iRequired = pValue->Value.Size;
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
	if ( xrtMemRangesOverlap(sOutput, iRequired + 1u,
		pValue->Source.Data, pValue->Source.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( pValue->Kind == XIMAP_DATA_QUOTED ) {
		for ( size_t i = 0; i < pValue->Value.Size; i++ ) {
			if ( pValue->Value.Data[i] == '\\' ) {
				i++;
			}
			sOutput[iOutput++] = pValue->Value.Data[i];
		}
	} else if ( iRequired != 0 ) {
		memcpy(sOutput, pValue->Value.Data, iRequired);
		iOutput = iRequired;
	}
	sOutput[iOutput] = 0;
	*pOutputSize = iOutput;
	return true;
}



/* 解析 LIST 的三项固定前缀，并保留后续扩展。 */
XRT_API bool xrtImapListParse(
	xstrview Text,
	ximaplistview* pList
)
{
	ximaplistview List;
	ximapdatacursor Cursor;
	ximapdataview Attributes;
	ximapdataview Delimiter;
	ximapdataview Mailbox;
	xstrview Payload;
	size_t iExtensions;

	if ( !xrtMemRangeValid(pList, sizeof(*pList)) ||
		xrtMemRangesOverlap(pList, sizeof(*pList), Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtImapDataPayload(Text, XRT_STR_LITERAL("LIST"), &Payload) ||
		!xrtImapDataCursorInit(&Cursor, Payload) ||
		(xrtImapDataNext(&Cursor, &Attributes) != XMAIL_NEXT_ITEM) ||
		(Attributes.Kind != XIMAP_DATA_LIST) ||
		(xrtImapDataNext(&Cursor, &Delimiter) != XMAIL_NEXT_ITEM) ||
		((Delimiter.Kind != XIMAP_DATA_QUOTED) &&
		 (Delimiter.Kind != XIMAP_DATA_NIL)) ||
		(xrtImapDataNext(&Cursor, &Mailbox) != XMAIL_NEXT_ITEM) ||
		((Mailbox.Kind != XIMAP_DATA_ATOM) &&
		 (Mailbox.Kind != XIMAP_DATA_QUOTED) &&
		 (Mailbox.Kind != XIMAP_DATA_LITERAL)) ) {
		return __xrtImapDataError(
			XERR_PROTOCOL,
			"invalid IMAP LIST response"
		);
	}
	iExtensions = __xrtImapDataSpace(Payload, Cursor.Position);
	List.Source = Text;
	List.Attributes = Attributes.Value;
	List.Delimiter = Delimiter;
	List.Mailbox = Mailbox;
	List.Extensions = __xrtMailSlice(
		Payload,
		iExtensions,
		Payload.Size - iExtensions
	);
	*pList = List;
	return true;
}



/* 初始化括号 flag 列表游标。 */
XRT_API bool xrtImapFlagCursorInit(
	ximapflagcursor* pCursor,
	xstrview Flags
)
{
	ximapdatacursor Outer;
	ximapdataview List;
	ximapdataview Extra;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), Flags.Data, Flags.Size) ||
		!xrtImapDataCursorInit(&Outer, Flags) ||
		(xrtImapDataNext(&Outer, &List) != XMAIL_NEXT_ITEM) ||
		(List.Kind != XIMAP_DATA_LIST) ||
		(xrtImapDataNext(&Outer, &Extra) != XMAIL_NEXT_END) ) {
		return __xrtImapDataError(
			XERR_PROTOCOL,
			"invalid IMAP flag list"
		);
	}
	return xrtImapDataCursorInit(&pCursor->Data, List.Value);
}



/* 返回 flag 列表中的下一项。 */
XRT_API xmailnext xrtImapFlagNext(
	ximapflagcursor* pCursor,
	xstrview* pFlag
)
{
	ximapdataview Value;
	xmailnext Next;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!xrtMemRangeValid(pFlag, sizeof(*pFlag)) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pFlag, sizeof(*pFlag)) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	Next = xrtImapDataNext(&pCursor->Data, &Value);
	if ( Next != XMAIL_NEXT_ITEM ) {
		return Next;
	}
	if ( Value.Kind != XIMAP_DATA_ATOM ) {
		__xrtImapDataError(XERR_PROTOCOL, "invalid IMAP flag value");
		return XMAIL_NEXT_ERROR;
	}
	*pFlag = Value.Source;
	return XMAIL_NEXT_ITEM;
}



/* 解析 STATUS 的 mailbox 和属性列表。 */
XRT_API bool xrtImapStatusParse(
	xstrview Text,
	ximapmailboxstatusview* pStatus
)
{
	ximapmailboxstatusview Status;
	ximapdatacursor Cursor;
	ximapdataview Mailbox;
	ximapdataview Items;
	ximapdataview Extra;
	xstrview Payload;

	if ( !xrtMemRangeValid(pStatus, sizeof(*pStatus)) ||
		xrtMemRangesOverlap(pStatus, sizeof(*pStatus), Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtImapDataPayload(Text, XRT_STR_LITERAL("STATUS"), &Payload) ||
		!xrtImapDataCursorInit(&Cursor, Payload) ||
		(xrtImapDataNext(&Cursor, &Mailbox) != XMAIL_NEXT_ITEM) ||
		((Mailbox.Kind != XIMAP_DATA_ATOM) &&
		 (Mailbox.Kind != XIMAP_DATA_QUOTED) &&
		 (Mailbox.Kind != XIMAP_DATA_LITERAL)) ||
		(xrtImapDataNext(&Cursor, &Items) != XMAIL_NEXT_ITEM) ||
		(Items.Kind != XIMAP_DATA_LIST) ||
		(xrtImapDataNext(&Cursor, &Extra) != XMAIL_NEXT_END) ) {
		return __xrtImapDataError(
			XERR_PROTOCOL,
			"invalid IMAP STATUS response"
		);
	}
	Status.Source = Text;
	Status.Mailbox = Mailbox;
	Status.Items = Items.Value;
	*pStatus = Status;
	return true;
}



/* 初始化 STATUS 名称和值游标。 */
XRT_API bool xrtImapStatusCursorInit(
	ximapstatuscursor* pCursor,
	const ximapmailboxstatusview* pStatus
)
{
	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!xrtMemRangeValid(pStatus, sizeof(*pStatus)) ||
		(pStatus == NULL) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pStatus, sizeof(*pStatus)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return xrtImapDataCursorInit(&pCursor->Data, pStatus->Items);
}



/* 返回下一 STATUS 属性对。 */
XRT_API xmailnext xrtImapStatusNext(
	ximapstatuscursor* pCursor,
	ximapstatusitem* pItem
)
{
	ximapdataview Name;
	ximapdataview Value;
	xmailnext Next;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!xrtMemRangeValid(pItem, sizeof(*pItem)) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pItem, sizeof(*pItem)) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	Next = xrtImapDataNext(&pCursor->Data, &Name);
	if ( Next != XMAIL_NEXT_ITEM ) {
		return Next;
	}
	if ( (Name.Kind != XIMAP_DATA_ATOM) ||
		(xrtImapDataNext(&pCursor->Data, &Value) != XMAIL_NEXT_ITEM) ) {
		__xrtImapDataError(XERR_PROTOCOL, "invalid IMAP STATUS item");
		return XMAIL_NEXT_ERROR;
	}
	pItem->Name = Name.Source;
	pItem->Value = Value;
	return XMAIL_NEXT_ITEM;
}



/* 初始化 SEARCH 结果游标。 */
XRT_API bool xrtImapSearchCursorInit(
	ximapsearchcursor* pCursor,
	xstrview Text
)
{
	xstrview Payload;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), Text.Data, Text.Size) ||
		!__xrtImapDataPayload(Text, XRT_STR_LITERAL("SEARCH"), &Payload) ) {
		return false;
	}
	return xrtImapDataCursorInit(&pCursor->Data, Payload);
}



/* 返回 SEARCH ID 或 MODSEQ。 */
XRT_API xmailnext xrtImapSearchNext(
	ximapsearchcursor* pCursor,
	ximapsearchitem* pItem
)
{
	ximapdataview Value;
	xmailnext Next;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!xrtMemRangeValid(pItem, sizeof(*pItem)) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pItem, sizeof(*pItem)) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	Next = xrtImapDataNext(&pCursor->Data, &Value);
	if ( Next != XMAIL_NEXT_ITEM ) {
		return Next;
	}
	if ( Value.Kind == XIMAP_DATA_NUMBER ) {
		pItem->Kind = XIMAP_SEARCH_ID;
		pItem->Number = Value.Number;
		return XMAIL_NEXT_ITEM;
	}
	if ( Value.Kind == XIMAP_DATA_LIST ) {
		ximapdatacursor Cursor;
		ximapdataview Name;
		ximapdataview Number;
		ximapdataview Extra;

		if ( xrtImapDataCursorInit(&Cursor, Value.Value) &&
			(xrtImapDataNext(&Cursor, &Name) == XMAIL_NEXT_ITEM) &&
			(Name.Kind == XIMAP_DATA_ATOM) &&
			__xrtMailAsciiEqualI(Name.Source, XRT_STR_LITERAL("MODSEQ")) &&
			(xrtImapDataNext(&Cursor, &Number) == XMAIL_NEXT_ITEM) &&
			(Number.Kind == XIMAP_DATA_NUMBER) &&
			(xrtImapDataNext(&Cursor, &Extra) == XMAIL_NEXT_END) ) {
			pItem->Kind = XIMAP_SEARCH_MODSEQ;
			pItem->Number = Number.Number;
			return XMAIL_NEXT_ITEM;
		}
	}
	__xrtImapDataError(XERR_PROTOCOL, "invalid IMAP SEARCH result");
	return XMAIL_NEXT_ERROR;
}



/* 解析 ESEARCH 的可选 correlator 和 UID 指示器。 */
XRT_API bool xrtImapESearchParse(
	xstrview Text,
	ximapesearchview* pSearch
)
{
	ximapesearchview Search;
	ximapdatacursor Cursor;
	ximapdataview Value;
	xstrview Payload;
	size_t iPosition;
	xmailnext Next;

	if ( !xrtMemRangeValid(pSearch, sizeof(*pSearch)) ||
		xrtMemRangesOverlap(pSearch, sizeof(*pSearch), Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtImapDataPayload(Text, XRT_STR_LITERAL("ESEARCH"), &Payload) ||
		!xrtImapDataCursorInit(&Cursor, Payload) ) {
		return false;
	}
	memset(&Search, 0, sizeof(Search));
	Search.Source = Text;
	iPosition = Cursor.Position;
	Next = xrtImapDataNext(&Cursor, &Value);
	if ( Next == XMAIL_NEXT_ERROR ) {
		return false;
	}
	if ( (Next == XMAIL_NEXT_ITEM) && (Value.Kind == XIMAP_DATA_LIST) ) {
		Search.Correlator = Value.Value;
	} else {
		Cursor.Position = iPosition;
		Cursor.Done = false;
	}
	iPosition = Cursor.Position;
	Next = xrtImapDataNext(&Cursor, &Value);
	if ( Next == XMAIL_NEXT_ERROR ) {
		return false;
	}
	if ( (Next == XMAIL_NEXT_ITEM) && (Value.Kind == XIMAP_DATA_ATOM) &&
		__xrtMailAsciiEqualI(Value.Source, XRT_STR_LITERAL("UID")) ) {
		Search.Uid = true;
	} else {
		Cursor.Position = iPosition;
		Cursor.Done = false;
	}
	iPosition = __xrtImapDataSpace(Payload, Cursor.Position);
	Search.Items = __xrtMailSlice(
		Payload,
		iPosition,
		Payload.Size - iPosition
	);
	*pSearch = Search;
	return true;
}



/* 初始化 ESEARCH 名称和值游标。 */
XRT_API bool xrtImapESearchCursorInit(
	ximapesearchcursor* pCursor,
	const ximapesearchview* pSearch
)
{
	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!xrtMemRangeValid(pSearch, sizeof(*pSearch)) ||
		(pSearch == NULL) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pSearch,
			sizeof(*pSearch)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return xrtImapDataCursorInit(&pCursor->Data, pSearch->Items);
}



/* 返回下一 ESEARCH 返回数据对。 */
XRT_API xmailnext xrtImapESearchNext(
	ximapesearchcursor* pCursor,
	ximapesearchitem* pItem
)
{
	ximapdataview Name;
	ximapdataview Value;
	xmailnext Next;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!xrtMemRangeValid(pItem, sizeof(*pItem)) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pItem, sizeof(*pItem)) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	Next = xrtImapDataNext(&pCursor->Data, &Name);
	if ( Next != XMAIL_NEXT_ITEM ) {
		return Next;
	}
	if ( (Name.Kind != XIMAP_DATA_ATOM) ||
		(xrtImapDataNext(&pCursor->Data, &Value) != XMAIL_NEXT_ITEM) ) {
		__xrtImapDataError(XERR_PROTOCOL, "invalid IMAP ESEARCH item");
		return XMAIL_NEXT_ERROR;
	}
	pItem->Name = Name.Source;
	pItem->Value = Value;
	return XMAIL_NEXT_ITEM;
}



/* 解析数字开头的 FETCH 响应前缀。 */
XRT_API bool xrtImapFetchParse(
	xstrview Text,
	ximapfetchview* pFetch
)
{
	ximapnumberview Number;
	ximapfetchview Fetch;

	if ( !xrtMemRangeValid(pFetch, sizeof(*pFetch)) ||
		xrtMemRangesOverlap(pFetch, sizeof(*pFetch), Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( (xrtImapNumberParse(Text, &Number) != XMAIL_NEXT_ITEM) ||
		!__xrtMailAsciiEqualI(Number.Name, XRT_STR_LITERAL("FETCH")) ||
		(Number.Number == 0) || (Number.Number > UINT32_MAX) ||
		(Number.Text.Size == 0) || (Number.Text.Data[0] != '(') ) {
		return __xrtImapDataError(
			XERR_PROTOCOL,
			"invalid IMAP FETCH response"
		);
	}
	Fetch.Source = Text;
	Fetch.Items = Number.Text;
	Fetch.Sequence = Number.Number;
	*pFetch = Fetch;
	return true;
}



/* 初始化 FETCH 属性游标。 */
XRT_API bool xrtImapFetchCursorInit(
	ximapfetchcursor* pCursor,
	const ximapfetchview* pFetch
)
{
	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!xrtMemRangeValid(pFetch, sizeof(*pFetch)) ||
		(pFetch == NULL) || !__xrtImapDataTextValid(pFetch->Items) ||
		(pFetch->Items.Size == 0) || (pFetch->Items.Data[0] != '(') ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pFetch,
			sizeof(*pFetch)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	pCursor->Text = pFetch->Items;
	pCursor->Position = 1u;
	pCursor->NeedMore = false;
	pCursor->Done = false;
	return true;
}



/* 在 literal 正文消费完成后接入后续 FETCH 行片段。 */
XRT_API bool xrtImapFetchCursorContinue(
	ximapfetchcursor* pCursor,
	xstrview Text
)
{
	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		(pCursor == NULL) || !pCursor->NeedMore || pCursor->Done ||
		!__xrtImapDataTextValid(Text) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	pCursor->Text = Text;
	pCursor->Position = 0;
	pCursor->NeedMore = false;
	return true;
}



/* 读取一个允许 section 内含空格的 FETCH 属性名。 */
static bool __xrtImapFetchAttribute(
	xstrview Text,
	size_t* pPosition,
	xstrview* pAttribute
)
{
	size_t iPosition = *pPosition;
	size_t iStart = iPosition;
	size_t iBrackets = 0;

	while ( iPosition < Text.Size ) {
		char iByte = Text.Data[iPosition];

		if ( iByte == '[' ) {
			iBrackets++;
		} else if ( iByte == ']' ) {
			if ( iBrackets == 0 ) {
				return __xrtImapDataError(
					XERR_PROTOCOL,
					"invalid IMAP FETCH section"
				);
			}
			iBrackets--;
		} else if ( (iBrackets == 0) &&
			((iByte == ' ') || (iByte == ')')) ) {
			break;
		}
		iPosition++;
	}
	if ( (iPosition == iStart) || (iBrackets != 0) ||
		(iPosition == Text.Size) || (Text.Data[iPosition] != ' ') ) {
		return __xrtImapDataError(
			XERR_PROTOCOL,
			"invalid IMAP FETCH attribute"
		);
	}
	*pAttribute = __xrtMailSlice(Text, iStart, iPosition - iStart);
	*pPosition = __xrtImapDataSpace(Text, iPosition);
	return true;
}



/* 返回下一 FETCH 属性和值。 */
XRT_API xmailnext xrtImapFetchNext(
	ximapfetchcursor* pCursor,
	ximapfetchitem* pItem
)
{
	ximapfetchitem Item;
	ximapdataview Value;
	size_t iPosition;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!xrtMemRangeValid(pItem, sizeof(*pItem)) ||
		(pCursor == NULL) || !__xrtImapDataTextValid(pCursor->Text) ||
		(pCursor->Position > pCursor->Text.Size) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pItem, sizeof(*pItem)) ||
		xrtMemRangesOverlap(pItem, sizeof(*pItem), pCursor->Text.Data,
			pCursor->Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( pCursor->Done || pCursor->NeedMore ) {
		return XMAIL_NEXT_END;
	}
	iPosition = __xrtImapDataSpace(pCursor->Text, pCursor->Position);
	if ( iPosition == pCursor->Text.Size ) {
		__xrtImapDataError(XERR_PROTOCOL, "unterminated IMAP FETCH response");
		return XMAIL_NEXT_ERROR;
	}
	if ( pCursor->Text.Data[iPosition] == ')' ) {
		iPosition = __xrtImapDataSpace(pCursor->Text, iPosition + 1u);
		if ( iPosition != pCursor->Text.Size ) {
			__xrtImapDataError(
				XERR_PROTOCOL,
				"trailing data after IMAP FETCH response"
			);
			return XMAIL_NEXT_ERROR;
		}
		pCursor->Position = iPosition;
		pCursor->Done = true;
		return XMAIL_NEXT_END;
	}
	memset(&Item, 0, sizeof(Item));
	if ( !__xrtImapFetchAttribute(
		pCursor->Text,
		&iPosition,
		&Item.Attribute
	) || (iPosition == pCursor->Text.Size) ||
		!__xrtImapDataValue(pCursor->Text, &iPosition, &Value) ) {
		return XMAIL_NEXT_ERROR;
	}
	Item.Value = Value;
	pCursor->Position = iPosition;
	if ( Value.Kind == XIMAP_DATA_LITERAL ) {
		pCursor->NeedMore = true;
	}
	*pItem = Item;
	return XMAIL_NEXT_ITEM;
}

#endif
