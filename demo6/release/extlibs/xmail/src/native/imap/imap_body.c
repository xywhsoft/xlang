#include <xrt/imap_body.h>

#include "../internal/xrt_mail.h"



#if defined(XMAIL_FEATURE_IMAP_BODY)

/* 创建稳定的 BODYSTRUCTURE 协议错误。 */
static bool __xrtImapBodyError(cstr sMessage)
{
	__xrtMailError(XERR_PROTOCOL, XMAIL_ERROR_PROTOCOL, sMessage);
	return false;
}



/* 跳过 BODYSTRUCTURE 数据项之间的 SP。 */
static size_t __xrtImapBodySpace(xstrview Text, size_t iPosition)
{
	while ( (iPosition < Text.Size) && (Text.Data[iPosition] == ' ') ) {
		iPosition++;
	}
	return iPosition;
}



/* 判断数据值是否是可在线路内完整表示的 IMAP string。 */
static bool __xrtImapBodyString(const ximapdataview* pValue)
{
	if ( (pValue->Kind != XIMAP_DATA_ATOM) &&
		(pValue->Kind != XIMAP_DATA_QUOTED) ) {
		return false;
	}
	if ( (pValue->Kind == XIMAP_DATA_ATOM) &&
		(pValue->Source.Size != 0) &&
		((pValue->Source.Data[0] == '{') ||
		 ((pValue->Source.Size > 1u) &&
		  (pValue->Source.Data[0] == '~') &&
		  (pValue->Source.Data[1] == '{'))) ) {
		return false;
	}
	return true;
}



/* 判断数据值是否是可选 IMAP string。 */
static bool __xrtImapBodyNString(const ximapdataview* pValue)
{
	return (pValue->Kind == XIMAP_DATA_NIL) || __xrtImapBodyString(pValue);
}



/* 从游标读取一个必须存在的数据值。 */
static bool __xrtImapBodyRequired(
	ximapdatacursor* pCursor,
	ximapdataview* pValue
)
{
	xmailnext Next = xrtImapDataNext(pCursor, pValue);

	if ( Next == XMAIL_NEXT_ITEM ) {
		return true;
	}
	if ( Next == XMAIL_NEXT_END ) {
		return __xrtImapBodyError("incomplete IMAP BODYSTRUCTURE");
	}
	return false;
}



/* 校验 body-fld-param 的 NIL 或成对字符串列表。 */
static bool __xrtImapBodyParameters(const ximapdataview* pParameters)
{
	ximapdatacursor Cursor;
	ximapdataview Name;
	ximapdataview Value;
	xmailnext Next;
	size_t iCount = 0;

	if ( pParameters->Kind == XIMAP_DATA_NIL ) {
		return true;
	}
	if ( (pParameters->Kind != XIMAP_DATA_LIST) ||
		!xrtImapDataCursorInit(&Cursor, pParameters->Value) ) {
		return false;
	}
	for ( ;; ) {
		Next = xrtImapDataNext(&Cursor, &Name);
		if ( Next == XMAIL_NEXT_END ) {
			return iCount != 0;
		}
		if ( (Next != XMAIL_NEXT_ITEM) || !__xrtImapBodyString(&Name) ||
			!__xrtImapBodyRequired(&Cursor, &Value) ||
			!__xrtImapBodyString(&Value) ) {
			return false;
		}
		iCount++;
	}
}



/* 校验 body-fld-dsp 的 NIL 或 `(type parameters)` 结构。 */
static bool __xrtImapBodyDisposition(const ximapdataview* pDisposition)
{
	ximapdatacursor Cursor;
	ximapdataview Type;
	ximapdataview Parameters;
	ximapdataview Extra;

	if ( pDisposition->Kind == XIMAP_DATA_NIL ) {
		return true;
	}
	return (pDisposition->Kind == XIMAP_DATA_LIST) &&
		xrtImapDataCursorInit(&Cursor, pDisposition->Value) &&
		__xrtImapBodyRequired(&Cursor, &Type) &&
		__xrtImapBodyString(&Type) &&
		__xrtImapBodyRequired(&Cursor, &Parameters) &&
		__xrtImapBodyParameters(&Parameters) &&
		(xrtImapDataNext(&Cursor, &Extra) == XMAIL_NEXT_END);
}



/* 校验 body-fld-lang 的 NIL、单字符串或非空字符串列表。 */
static bool __xrtImapBodyLanguage(const ximapdataview* pLanguage)
{
	ximapdatacursor Cursor;
	ximapdataview Value;
	xmailnext Next;
	size_t iCount = 0;

	if ( (pLanguage->Kind == XIMAP_DATA_NIL) ||
		__xrtImapBodyString(pLanguage) ) {
		return true;
	}
	if ( (pLanguage->Kind != XIMAP_DATA_LIST) ||
		!xrtImapDataCursorInit(&Cursor, pLanguage->Value) ) {
		return false;
	}
	for ( ;; ) {
		Next = xrtImapDataNext(&Cursor, &Value);
		if ( Next == XMAIL_NEXT_END ) {
			return iCount != 0;
		}
		if ( (Next != XMAIL_NEXT_ITEM) || !__xrtImapBodyString(&Value) ) {
			return false;
		}
		iCount++;
	}
}



/* 递归校验服务器定义的 body-extension 值。 */
static bool __xrtImapBodyExtension(
	const ximapdataview* pValue,
	size_t iDepth
)
{
	ximapdatacursor Cursor;
	ximapdataview Child;
	xmailnext Next;
	size_t iCount = 0;

	if ( iDepth > XIMAP_BODY_DEPTH_MAX ) {
		return __xrtImapBodyError("IMAP body extension nesting is too deep");
	}
	if ( (pValue->Kind == XIMAP_DATA_NUMBER) ||
		__xrtImapBodyNString(pValue) ) {
		return true;
	}
	if ( (pValue->Kind != XIMAP_DATA_LIST) ||
		!xrtImapDataCursorInit(&Cursor, pValue->Value) ) {
		return false;
	}
	for ( ;; ) {
		Next = xrtImapDataNext(&Cursor, &Child);
		if ( Next == XMAIL_NEXT_END ) {
			return iCount != 0;
		}
		if ( (Next != XMAIL_NEXT_ITEM) ||
			!__xrtImapBodyExtension(&Child, iDepth + 1u) ) {
			return false;
		}
		iCount++;
	}
}



/* 前置声明递归 BODYSTRUCTURE 解析器。 */
static bool __xrtImapBodyParseList(
	const ximapdataview* pList,
	size_t iDepth,
	ximapbodyview* pBody
);



/* 校验未知扩展尾并保留其原始数据区。 */
static bool __xrtImapBodyExtensionTail(
	ximapdatacursor* pCursor,
	xstrview Text,
	xstrview* pExtensions
)
{
	ximapdataview Value;
	xmailnext Next;
	size_t iStart = __xrtImapBodySpace(Text, pCursor->Position);
	size_t iEnd = iStart;

	for ( ;; ) {
		Next = xrtImapDataNext(pCursor, &Value);
		if ( Next == XMAIL_NEXT_END ) {
			pExtensions->Data = Text.Data != NULL ? Text.Data + iStart : NULL;
			pExtensions->Size = iEnd - iStart;
			return true;
		}
		if ( (Next != XMAIL_NEXT_ITEM) ||
			!__xrtImapBodyExtension(&Value, 1u) ) {
			return false;
		}
		iEnd = (size_t)((Value.Source.Data + Value.Source.Size) - Text.Data);
	}
}



/* 按 RFC 顺序读取一个单部分的可选扩展字段。 */
static bool __xrtImapBodyOneExtensions(
	ximapdatacursor* pCursor,
	xstrview Text,
	ximapbodyview* pBody
)
{
	ximapdataview Value;
	xmailnext Next;

	Next = xrtImapDataNext(pCursor, &Value);
	if ( Next == XMAIL_NEXT_END ) {
		return true;
	}
	if ( (Next != XMAIL_NEXT_ITEM) || !__xrtImapBodyNString(&Value) ) {
		return __xrtImapBodyError("invalid IMAP body MD5 field");
	}
	pBody->Md5 = Value;

	Next = xrtImapDataNext(pCursor, &Value);
	if ( Next == XMAIL_NEXT_END ) {
		return true;
	}
	if ( (Next != XMAIL_NEXT_ITEM) || !__xrtImapBodyDisposition(&Value) ) {
		return __xrtImapBodyError("invalid IMAP body disposition field");
	}
	pBody->Disposition = Value;

	Next = xrtImapDataNext(pCursor, &Value);
	if ( Next == XMAIL_NEXT_END ) {
		return true;
	}
	if ( (Next != XMAIL_NEXT_ITEM) || !__xrtImapBodyLanguage(&Value) ) {
		return __xrtImapBodyError("invalid IMAP body language field");
	}
	pBody->Language = Value;

	Next = xrtImapDataNext(pCursor, &Value);
	if ( Next == XMAIL_NEXT_END ) {
		return true;
	}
	if ( (Next != XMAIL_NEXT_ITEM) || !__xrtImapBodyNString(&Value) ) {
		return __xrtImapBodyError("invalid IMAP body location field");
	}
	pBody->Location = Value;
	return __xrtImapBodyExtensionTail(pCursor, Text, &pBody->Extensions);
}



/* 按 RFC 顺序读取 multipart 的可选扩展字段。 */
static bool __xrtImapBodyMultiExtensions(
	ximapdatacursor* pCursor,
	xstrview Text,
	ximapbodyview* pBody
)
{
	ximapdataview Value;
	xmailnext Next;

	Next = xrtImapDataNext(pCursor, &Value);
	if ( Next == XMAIL_NEXT_END ) {
		return true;
	}
	if ( (Next != XMAIL_NEXT_ITEM) || !__xrtImapBodyParameters(&Value) ) {
		return __xrtImapBodyError("invalid IMAP multipart parameters");
	}
	pBody->Parameters = Value;

	Next = xrtImapDataNext(pCursor, &Value);
	if ( Next == XMAIL_NEXT_END ) {
		return true;
	}
	if ( (Next != XMAIL_NEXT_ITEM) || !__xrtImapBodyDisposition(&Value) ) {
		return __xrtImapBodyError("invalid IMAP multipart disposition");
	}
	pBody->Disposition = Value;

	Next = xrtImapDataNext(pCursor, &Value);
	if ( Next == XMAIL_NEXT_END ) {
		return true;
	}
	if ( (Next != XMAIL_NEXT_ITEM) || !__xrtImapBodyLanguage(&Value) ) {
		return __xrtImapBodyError("invalid IMAP multipart language");
	}
	pBody->Language = Value;

	Next = xrtImapDataNext(pCursor, &Value);
	if ( Next == XMAIL_NEXT_END ) {
		return true;
	}
	if ( (Next != XMAIL_NEXT_ITEM) || !__xrtImapBodyNString(&Value) ) {
		return __xrtImapBodyError("invalid IMAP multipart location");
	}
	pBody->Location = Value;
	return __xrtImapBodyExtensionTail(pCursor, Text, &pBody->Extensions);
}



/* 解析普通、TEXT 或 MESSAGE/RFC822、MESSAGE/GLOBAL 单部分。 */
static bool __xrtImapBodyOnePart(
	ximapdatacursor* pCursor,
	const ximapdataview* pType,
	xstrview Text,
	size_t iDepth,
	ximapbodyview* pBody
)
{
	ximapdataview Value;
	ximapbodyview Nested;
	bool bText;
	bool bMessage;

	pBody->Type = *pType;
	if ( !__xrtImapBodyString(&pBody->Type) ||
		!__xrtImapBodyRequired(pCursor, &pBody->Subtype) ||
		!__xrtImapBodyString(&pBody->Subtype) ||
		!__xrtImapBodyRequired(pCursor, &pBody->Parameters) ||
		!__xrtImapBodyParameters(&pBody->Parameters) ||
		!__xrtImapBodyRequired(pCursor, &pBody->Id) ||
		!__xrtImapBodyNString(&pBody->Id) ||
		!__xrtImapBodyRequired(pCursor, &pBody->Description) ||
		!__xrtImapBodyNString(&pBody->Description) ||
		!__xrtImapBodyRequired(pCursor, &pBody->Encoding) ||
		!__xrtImapBodyString(&pBody->Encoding) ||
		!__xrtImapBodyRequired(pCursor, &Value) ||
		(Value.Kind != XIMAP_DATA_NUMBER) ) {
		return __xrtImapBodyError("invalid IMAP single-part body fields");
	}
	pBody->Octets = Value.Number;
	bText = __xrtMailAsciiEqualI(
		pBody->Type.Value,
		XRT_STR_LITERAL("TEXT")
	);
	bMessage = __xrtMailAsciiEqualI(
		pBody->Type.Value,
		XRT_STR_LITERAL("MESSAGE")
	) && (__xrtMailAsciiEqualI(
		pBody->Subtype.Value,
		XRT_STR_LITERAL("RFC822")
	) || __xrtMailAsciiEqualI(
		pBody->Subtype.Value,
		XRT_STR_LITERAL("GLOBAL")
	));
	if ( bText ) {
		pBody->Kind = XIMAP_BODY_TEXT;
		if ( !__xrtImapBodyRequired(pCursor, &Value) ||
			(Value.Kind != XIMAP_DATA_NUMBER) ) {
			return __xrtImapBodyError("invalid IMAP text body line count");
		}
		pBody->Lines = Value.Number;
	} else if ( bMessage ) {
		pBody->Kind = XIMAP_BODY_MESSAGE;
		if ( !__xrtImapBodyRequired(pCursor, &pBody->Envelope) ||
			(pBody->Envelope.Kind != XIMAP_DATA_LIST) ||
			!__xrtImapBodyRequired(pCursor, &pBody->Body) ||
			(pBody->Body.Kind != XIMAP_DATA_LIST) ||
			!__xrtImapBodyParseList(
				&pBody->Body,
				iDepth + 1u,
				&Nested
			) || !__xrtImapBodyRequired(pCursor, &Value) ||
			(Value.Kind != XIMAP_DATA_NUMBER) ) {
			return __xrtImapBodyError("invalid IMAP message body fields");
		}
		pBody->Lines = Value.Number;
	} else {
		pBody->Kind = XIMAP_BODY_BASIC;
	}
	return __xrtImapBodyOneExtensions(pCursor, Text, pBody);
}



/* 解析一个或多个子部分、subtype 与 multipart 扩展字段。 */
static bool __xrtImapBodyMultipart(
	ximapdatacursor* pCursor,
	const ximapdataview* pFirst,
	xstrview Text,
	size_t iDepth,
	ximapbodyview* pBody
)
{
	ximapdataview Value = *pFirst;
	ximapbodyview Child;
	const char* sChildren = pFirst->Source.Data;
	const char* sChildrenEnd = sChildren;

	pBody->Kind = XIMAP_BODY_MULTIPART;
	for ( ;; ) {
		if ( (Value.Kind != XIMAP_DATA_LIST) ||
			!__xrtImapBodyParseList(&Value, iDepth + 1u, &Child) ) {
			return __xrtImapBodyError("invalid IMAP multipart child");
		}
		if ( pBody->ChildCount == SIZE_MAX ) {
			return __xrtImapBodyError("too many IMAP multipart children");
		}
		pBody->ChildCount++;
		sChildrenEnd = Value.Source.Data + Value.Source.Size;
		if ( !__xrtImapBodyRequired(pCursor, &Value) ) {
			return false;
		}
		if ( Value.Kind != XIMAP_DATA_LIST ) {
			break;
		}
	}
	pBody->Children.Data = sChildren;
	pBody->Children.Size = (size_t)(sChildrenEnd - sChildren);
	pBody->Subtype = Value;
	if ( !__xrtImapBodyString(&pBody->Subtype) ) {
		return __xrtImapBodyError("invalid IMAP multipart subtype");
	}
	return __xrtImapBodyMultiExtensions(pCursor, Text, pBody);
}



/* 解析一个已由通用数据层定界的括号 BODYSTRUCTURE 值。 */
static bool __xrtImapBodyParseList(
	const ximapdataview* pList,
	size_t iDepth,
	ximapbodyview* pBody
)
{
	ximapbodyview Body;
	ximapdatacursor Cursor;
	ximapdataview First;

	if ( iDepth > XIMAP_BODY_DEPTH_MAX ) {
		return __xrtImapBodyError("IMAP BODYSTRUCTURE nesting is too deep");
	}
	if ( (pList->Kind != XIMAP_DATA_LIST) ||
		!xrtImapDataCursorInit(&Cursor, pList->Value) ||
		!__xrtImapBodyRequired(&Cursor, &First) ) {
		return __xrtImapBodyError("invalid IMAP BODYSTRUCTURE list");
	}
	memset(&Body, 0, sizeof(Body));
	Body.Source = pList->Source;
	if ( First.Kind == XIMAP_DATA_LIST ) {
		if ( !__xrtImapBodyMultipart(
			&Cursor,
			&First,
			pList->Value,
			iDepth,
			&Body
		) ) {
			return false;
		}
	} else if ( !__xrtImapBodyOnePart(
		&Cursor,
		&First,
		pList->Value,
		iDepth,
		&Body
	) ) {
		return false;
	}
	*pBody = Body;
	return true;
}



/* 解析并递归校验完整 BODYSTRUCTURE。 */
XRT_API bool xrtImapBodyParse(
	xstrview Text,
	ximapbodyview* pBody
)
{
	ximapdatacursor Cursor;
	ximapdataview List;
	ximapdataview Extra;
	ximapbodyview Body;

	if ( !xrtMemRangeValid(pBody, sizeof(*pBody)) ||
		!xrtMemRangeValid(Text.Data, Text.Size) ||
		xrtMemRangesOverlap(pBody, sizeof(*pBody), Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtImapDataCursorInit(&Cursor, Text) ||
		(xrtImapDataNext(&Cursor, &List) != XMAIL_NEXT_ITEM) ||
		(List.Kind != XIMAP_DATA_LIST) ||
		(xrtImapDataNext(&Cursor, &Extra) != XMAIL_NEXT_END) ||
		!__xrtImapBodyParseList(&List, 0, &Body) ) {
		return __xrtImapBodyError("invalid IMAP BODYSTRUCTURE");
	}
	*pBody = Body;
	return true;
}



/* 初始化 multipart 直接子部分游标。 */
XRT_API bool xrtImapBodyChildCursorInit(
	ximapbodycursor* pCursor,
	const ximapbodyview* pBody
)
{
	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!xrtMemRangeValid(pBody, sizeof(*pBody)) || (pBody == NULL) ||
		(pBody->Kind != XIMAP_BODY_MULTIPART) ||
		(pBody->ChildCount == 0) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pBody,
			sizeof(*pBody)) ||
		!xrtImapDataCursorInit(&pCursor->Data, pBody->Children) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	pCursor->Remaining = pBody->ChildCount;
	return true;
}



/* 返回下一个 multipart 直接子部分。 */
XRT_API xmailnext xrtImapBodyChildNext(
	ximapbodycursor* pCursor,
	ximapbodyview* pBody
)
{
	ximapdataview Value;
	ximapbodyview Body;
	xmailnext Next;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!xrtMemRangeValid(pBody, sizeof(*pBody)) || (pCursor == NULL) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pBody,
			sizeof(*pBody)) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( pCursor->Remaining == 0 ) {
		Next = xrtImapDataNext(&pCursor->Data, &Value);
		if ( Next == XMAIL_NEXT_END ) {
			return XMAIL_NEXT_END;
		}
		__xrtImapBodyError("unexpected IMAP multipart child data");
		return XMAIL_NEXT_ERROR;
	}
	Next = xrtImapDataNext(&pCursor->Data, &Value);
	if ( (Next != XMAIL_NEXT_ITEM) || (Value.Kind != XIMAP_DATA_LIST) ||
		!__xrtImapBodyParseList(&Value, 0, &Body) ) {
		__xrtImapBodyError("invalid IMAP multipart child cursor");
		return XMAIL_NEXT_ERROR;
	}
	pCursor->Remaining--;
	*pBody = Body;
	return XMAIL_NEXT_ITEM;
}



/* 初始化成对参数游标。 */
XRT_API bool xrtImapBodyParamCursorInit(
	ximapbodyparamcursor* pCursor,
	const ximapdataview* pParameters
)
{
	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!xrtMemRangeValid(pParameters, sizeof(*pParameters)) ||
		(pParameters == NULL) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pParameters,
			sizeof(*pParameters)) ||
		!__xrtImapBodyParameters(pParameters) ||
		!xrtImapDataCursorInit(&pCursor->Data, pParameters->Value) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return true;
}



/* 返回下一参数名和值。 */
XRT_API xmailnext xrtImapBodyParamNext(
	ximapbodyparamcursor* pCursor,
	ximapbodyparam* pParameter
)
{
	ximapbodyparam Parameter;
	xmailnext Next;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!xrtMemRangeValid(pParameter, sizeof(*pParameter)) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pParameter,
			sizeof(*pParameter)) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	Next = xrtImapDataNext(&pCursor->Data, &Parameter.Name);
	if ( Next != XMAIL_NEXT_ITEM ) {
		return Next;
	}
	if ( !__xrtImapBodyString(&Parameter.Name) ||
		!__xrtImapBodyRequired(&pCursor->Data, &Parameter.Value) ||
		!__xrtImapBodyString(&Parameter.Value) ) {
		__xrtImapBodyError("invalid IMAP body parameter pair");
		return XMAIL_NEXT_ERROR;
	}
	*pParameter = Parameter;
	return XMAIL_NEXT_ITEM;
}

#endif
