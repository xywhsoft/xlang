#include "../internal/xrt_mail.h"



#if defined(XMAIL_FEATURE_MAIL_MULTIPART)

/* 发布 multipart 结构错误。 */
static xmailnext __xrtMailMultipartError(cstr sMessage)
{
	__xrtMailError(XERR_PROTOCOL, XMAIL_ERROR_MIME, sMessage);
	return XMAIL_NEXT_ERROR;
}



/* 判断位置是否位于 MIME 物理行首。 */
static bool __xrtMailMultipartLineStart(xstrview Body, size_t iPosition)
{
	return (iPosition == 0) ||
		((iPosition >= 2u) && (Body.Data[iPosition - 2u] == '\r') &&
		 (Body.Data[iPosition - 1u] == '\n'));
}



/* 验证一个候选位置并返回 delimiter 行后的第一个字节。 */
static bool __xrtMailMultipartDelimiterAt(
	xstrview Body,
	xstrview Boundary,
	size_t iPosition,
	size_t* pAfter,
	bool* pClose
)
{
	size_t i = iPosition;
	bool bClose = false;

	if ( !__xrtMailMultipartLineStart(Body, iPosition) ||
		 ((Body.Size - iPosition) < (Boundary.Size + 2u)) ||
		 (Body.Data[i++] != '-') || (Body.Data[i++] != '-') ||
		 (memcmp(Body.Data + i, Boundary.Data, Boundary.Size) != 0) ) {
		return false;
	}
	i += Boundary.Size;
	if ( ((i + 1u) < Body.Size) && (Body.Data[i] == '-') &&
		 (Body.Data[i + 1u] == '-') ) {
		bClose = true;
		i += 2u;
	}
	while ( (i < Body.Size) &&
		 ((Body.Data[i] == ' ') || (Body.Data[i] == '\t')) ) {
		i++;
	}
	if ( i == Body.Size ) {
		*pAfter = i;
		*pClose = bClose;
		return true;
	}
	if ( ((i + 1u) >= Body.Size) || (Body.Data[i] != '\r') ||
		 (Body.Data[i + 1u] != '\n') ) {
		return false;
	}
	*pAfter = i + 2u;
	*pClose = bClose;
	return true;
}



/* 从指定位置寻找下一条严格 boundary delimiter 行。 */
static bool __xrtMailMultipartDelimiter(
	xstrview Body,
	xstrview Boundary,
	size_t iStart,
	size_t* pPosition,
	size_t* pAfter,
	bool* pClose
)
{
	if ( (Boundary.Size + 2u) > Body.Size ) {
		return false;
	}
	for ( size_t i = iStart; (i + Boundary.Size + 2u) <= Body.Size; i++ ) {
		if ( (Body.Data[i] == '-') && (Body.Data[i + 1u] == '-') &&
			 __xrtMailMultipartDelimiterAt(
				Body,
				Boundary,
				i,
				pAfter,
				pClose
			) ) {
			*pPosition = i;
			return true;
		}
	}
	return false;
}



/* 初始化零分配 multipart 游标并定位首条分隔线。 */
XRT_API bool xrtMailMultipartCursorInit(
	xmailmultipartcursor* pCursor,
	xstrview Body,
	xstrview Boundary,
	size_t iMaxParts
)
{
	xmailmultipartcursor Cursor;
	size_t iBoundary;
	size_t iAfter;
	size_t iPreambleEnd;
	bool bClose;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		 !__xrtMailViewValid(Body) || !__xrtMailViewValid(Boundary) ||
		 xrtMemRangesOverlap(pCursor, sizeof(*pCursor), Body.Data, Body.Size) ||
		 xrtMemRangesOverlap(pCursor, sizeof(*pCursor),
			Boundary.Data, Boundary.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtMailBoundaryValid(Boundary) ) {
		__xrtMailError(XERR_PROTOCOL, XMAIL_ERROR_MIME,
			"multipart boundary is invalid");
		return false;
	}
	if ( !__xrtMailMultipartDelimiter(
		Body,
		Boundary,
		0,
		&iBoundary,
		&iAfter,
		&bClose
	) ) {
		__xrtMailError(XERR_PROTOCOL, XMAIL_ERROR_MIME,
			"multipart body has no opening boundary");
		return false;
	}
	iPreambleEnd = (iBoundary >= 2u) ? iBoundary - 2u : iBoundary;
	memset(&Cursor, 0, sizeof(Cursor));
	Cursor.Source = Body;
	Cursor.Boundary = Boundary;
	Cursor.Preamble = __xrtMailView(Body.Data, iPreambleEnd);
	Cursor.Position = iAfter;
	Cursor.MaxParts = iMaxParts == 0 ? XMAIL_MULTIPART_PARTS_DEFAULT : iMaxParts;
	Cursor.Closed = bClose;
	Cursor.Done = bClose;
	if ( bClose ) {
		Cursor.Epilogue = __xrtMailView(
			Body.Data + iAfter,
			Body.Size - iAfter
		);
	}
	*pCursor = Cursor;
	return true;
}



/* 验证一个 part 的字段块，并保持 header 游标为唯一语法实现。 */
static bool __xrtMailMultipartHeaders(xstrview Headers)
{
	xmailheadercursor Cursor;
	xmailheaderview Header;
	xmailnext Next;

	if ( Headers.Size == 0 ) {
		return true;
	}
	if ( !xrtMailHeaderCursorInit(&Cursor, Headers) ) {
		return false;
	}
	while ( (Next = xrtMailHeaderNext(&Cursor, &Header)) == XMAIL_NEXT_ITEM ) {
	}
	return Next == XMAIL_NEXT_END;
}



/* 把一段 MIME entity 拆成字段块和正文。 */
static bool __xrtMailMultipartPart(
	xstrview Source,
	xmailmultipartview* pPart
)
{
	xmailmultipartview Part;
	size_t iSeparator = XRT_NPOS;
	size_t iBodyStart;

	Part.Source = Source;
	if ( Source.Size == 0 ) {
		Part.Headers = __xrtMailView(Source.Data, 0);
		Part.Body = __xrtMailView(Source.Data, 0);
		*pPart = Part;
		return true;
	}
	if ( (Source.Size >= 2u) && (Source.Data[0] == '\r') &&
		 (Source.Data[1] == '\n') ) {
		iSeparator = 0;
		iBodyStart = 2u;
	} else {
		for ( size_t i = 0; (i + 3u) < Source.Size; i++ ) {
			if ( (Source.Data[i] == '\r') && (Source.Data[i + 1u] == '\n') &&
				 (Source.Data[i + 2u] == '\r') &&
				 (Source.Data[i + 3u] == '\n') ) {
				iSeparator = i;
				iBodyStart = i + 4u;
				break;
			}
		}
	}
	if ( iSeparator == XRT_NPOS ) {
		__xrtMailError(XERR_PROTOCOL, XMAIL_ERROR_MIME,
			"multipart part has no header/body separator");
		return false;
	}
	Part.Headers = __xrtMailView(Source.Data, iSeparator);
	Part.Body = __xrtMailView(
		Source.Data + iBodyStart,
		Source.Size - iBodyStart
	);
	if ( !__xrtMailMultipartHeaders(Part.Headers) ) {
		return false;
	}
	*pPart = Part;
	return true;
}



/* 返回下一 multipart part。 */
XRT_API xmailnext xrtMailMultipartNext(
	xmailmultipartcursor* pCursor,
	xmailmultipartview* pPart
)
{
	xmailmultipartcursor Cursor;
	xmailmultipartview Part;
	size_t iBoundary;
	size_t iAfter;
	size_t iSourceEnd;
	bool bClose;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		 !xrtMemRangeValid(pPart, sizeof(*pPart)) ||
		 !__xrtMailViewValid(pCursor != NULL ? pCursor->Source :
			__xrtMailView(NULL, 0)) ||
		 !__xrtMailViewValid(pCursor != NULL ? pCursor->Boundary :
			__xrtMailView(NULL, 0)) ||
		 (pCursor->Position > pCursor->Source.Size) ||
		 xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pPart, sizeof(*pPart)) ||
		 xrtMemRangesOverlap(pPart, sizeof(*pPart),
			pCursor->Source.Data, pCursor->Source.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( pCursor->Done ) {
		return XMAIL_NEXT_END;
	}
	Cursor = *pCursor;
	if ( (Cursor.MaxParts != SIZE_MAX) && (Cursor.Parts >= Cursor.MaxParts) ) {
		__xrtMailError(XERR_RANGE, XMAIL_ERROR_LIMIT,
			"multipart part limit exceeded");
		return XMAIL_NEXT_ERROR;
	}
	if ( !__xrtMailMultipartDelimiter(
		Cursor.Source,
		Cursor.Boundary,
		Cursor.Position,
		&iBoundary,
		&iAfter,
		&bClose
	) ) {
		return __xrtMailMultipartError(
			"multipart body has no closing boundary"
		);
	}
	if ( (iBoundary < 2u) || (iBoundary < Cursor.Position) ) {
		return __xrtMailMultipartError("multipart boundary position is invalid");
	}
	iSourceEnd = iBoundary - 2u;
	if ( iSourceEnd < Cursor.Position ) {
		return __xrtMailMultipartError("multipart part framing is invalid");
	}
	if ( !__xrtMailMultipartPart(
		__xrtMailView(
			Cursor.Source.Data + Cursor.Position,
			iSourceEnd - Cursor.Position
		),
		&Part
	) ) {
		return XMAIL_NEXT_ERROR;
	}
	Cursor.Parts++;
	Cursor.Position = iAfter;
	Cursor.Closed = bClose;
	Cursor.Done = bClose;
	if ( bClose ) {
		Cursor.Epilogue = __xrtMailView(
			Cursor.Source.Data + iAfter,
			Cursor.Source.Size - iAfter
		);
	}
	*pPart = Part;
	*pCursor = Cursor;
	return XMAIL_NEXT_ITEM;
}



/* 写入 multipart 分隔片段。 */
XRT_API bool xrtMailMultipartMarkWrite(
	xstrview Boundary,
	xmailmultipartmark Mark,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iPrefix;
	size_t iSuffix;
	size_t iRequired;

	if ( !__xrtMailViewValid(Boundary) || !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 ((Mark != XMAIL_MULTIPART_FIRST) &&
		  (Mark != XMAIL_MULTIPART_NEXT) &&
		  (Mark != XMAIL_MULTIPART_CLOSE)) ||
		 xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize),
			Boundary.Data, Boundary.Size) ||
		 ((sOutput != NULL) &&
		  (xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), sOutput, iCapacity) ||
		   xrtMemRangesOverlap(sOutput, iCapacity, Boundary.Data, Boundary.Size))) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtMailBoundaryValid(Boundary) ) {
		__xrtMailError(XERR_PROTOCOL, XMAIL_ERROR_MIME,
			"multipart boundary is invalid");
		return false;
	}
	iPrefix = Mark == XMAIL_MULTIPART_FIRST ? 2u : 4u;
	iSuffix = Mark == XMAIL_MULTIPART_CLOSE ? 4u : 2u;
	if ( !__xrtMailSizeAdd(iPrefix, Boundary.Size, &iRequired) ||
		 !__xrtMailSizeAdd(iRequired, iSuffix, &iRequired) ) {
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
	if ( Mark == XMAIL_MULTIPART_FIRST ) {
		memcpy(sOutput, "--", 2u);
	} else {
		memcpy(sOutput, "\r\n--", 4u);
	}
	memcpy(sOutput + iPrefix, Boundary.Data, Boundary.Size);
	if ( Mark == XMAIL_MULTIPART_CLOSE ) {
		memcpy(sOutput + iPrefix + Boundary.Size, "--\r\n", 4u);
	} else {
		memcpy(sOutput + iPrefix + Boundary.Size, "\r\n", 2u);
	}
	sOutput[iRequired] = 0;
	*pOutputSize = iRequired;
	return true;
}

#endif
