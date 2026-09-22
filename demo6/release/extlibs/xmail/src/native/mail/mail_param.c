#include "../internal/xrt_mail.h"



#if defined(XMAIL_FEATURE_MAIL_PARAM)

/* 发布 MIME 参数语法错误。 */
static bool __xrtMailParamError(cstr sMessage)
{
	__xrtMailError(XERR_PROTOCOL, XMAIL_ERROR_MIME, sMessage);
	return false;
}



/* 判断字节是否属于 MIME token。 */
static bool __xrtMailParamTokenByte(unsigned char iByte)
{
	if ( (iByte <= 32u) || (iByte >= 127u) ) {
		return false;
	}
	switch ( iByte ) {
		case '(':
		case ')':
		case '<':
		case '>':
		case '@':
		case ',':
		case ';':
		case ':':
		case '\\':
		case '"':
		case '/':
		case '[':
		case ']':
		case '?':
		case '=':
			return false;
		default:
			return true;
	}
}



/* 去掉字段值外侧的 SP/HTAB。 */
static xstrview __xrtMailParamTrim(xstrview Text)
{
	size_t iStart = 0;
	size_t iEnd = Text.Size;

	while ( (iStart < iEnd) &&
		 ((Text.Data[iStart] == ' ') || (Text.Data[iStart] == '\t')) ) {
		iStart++;
	}
	while ( (iEnd > iStart) &&
		 ((Text.Data[iEnd - 1u] == ' ') || (Text.Data[iEnd - 1u] == '\t')) ) {
		iEnd--;
	}
	return __xrtMailSlice(Text, iStart, iEnd - iStart);
}



/* 跳过已经展开字段中的 SP/HTAB。 */
static void __xrtMailParamWhitespace(xstrview Text, size_t* pPosition)
{
	while ( (*pPosition < Text.Size) &&
		 ((Text.Data[*pPosition] == ' ') || (Text.Data[*pPosition] == '\t')) ) {
		(*pPosition)++;
	}
}



/* 读取一个非空 MIME token。 */
static bool __xrtMailParamToken(
	xstrview Text,
	size_t* pPosition,
	xstrview* pToken
)
{
	size_t iStart = *pPosition;

	while ( (*pPosition < Text.Size) &&
		 __xrtMailParamTokenByte((unsigned char)Text.Data[*pPosition]) ) {
		(*pPosition)++;
	}
	if ( *pPosition == iStart ) {
		return false;
	}
	*pToken = __xrtMailView(Text.Data + iStart, *pPosition - iStart);
	return true;
}



/* 把十进制 section 后缀转换为 size_t。 */
static bool __xrtMailParamSection(
	xstrview Text,
	size_t iStart,
	size_t iEnd,
	size_t* pSection
)
{
	size_t iValue = 0;

	if ( iStart == iEnd ) {
		return false;
	}
	for ( size_t i = iStart; i < iEnd; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( (iByte < (unsigned char)'0') || (iByte > (unsigned char)'9') ||
			 (iValue > ((SIZE_MAX - (size_t)(iByte - (unsigned char)'0')) / 10u)) ) {
			return false;
		}
		iValue = (iValue * 10u) + (size_t)(iByte - (unsigned char)'0');
	}
	*pSection = iValue;
	return true;
}



/* 拆分 RFC 2231 的 name*、name*0 和 name*0* 后缀。 */
static bool __xrtMailParamName(
	xstrview RawName,
	xstrview* pName,
	size_t* pSection,
	bool* pExtended,
	bool* pContinued
)
{
	size_t iEnd = RawName.Size;
	size_t iStar = XRT_NPOS;
	bool bExtended = false;
	size_t iSection;

	if ( (iEnd > 0) && (RawName.Data[iEnd - 1u] == '*') ) {
		bExtended = true;
		iEnd--;
	}
	for ( size_t i = iEnd; i > 0; i-- ) {
		if ( RawName.Data[i - 1u] == '*' ) {
			iStar = i - 1u;
			break;
		}
	}
	if ( (iStar != XRT_NPOS) &&
		 __xrtMailParamSection(RawName, iStar + 1u, iEnd, &iSection) ) {
		if ( (iStar == 0) || (iSection >= XMAIL_PARAM_SECTIONS_MAX) ) {
			return false;
		}
		*pName = __xrtMailView(RawName.Data, iStar);
		*pSection = iSection;
		*pExtended = bExtended;
		*pContinued = true;
		return true;
	}
	if ( bExtended ) {
		if ( (iEnd == 0) || (RawName.Data[iEnd - 1u] == '*') ) {
			return false;
		}
		*pName = __xrtMailView(RawName.Data, iEnd);
		*pSection = XMAIL_PARAM_SECTION_NONE;
		*pExtended = true;
		*pContinued = false;
		return true;
	}
	*pName = RawName;
	*pSection = XMAIL_PARAM_SECTION_NONE;
	*pExtended = false;
	*pContinued = false;
	return true;
}



/* 初始化 MIME 参数游标。 */
XRT_API bool xrtMailParamCursorInit(
	xmailparamcursor* pCursor,
	xstrview Parameters
)
{
	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		 !__xrtMailViewValid(Parameters) ||
		 xrtMemRangesOverlap(pCursor, sizeof(*pCursor),
			Parameters.Data, Parameters.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	pCursor->Text = Parameters;
	pCursor->Position = 0;
	pCursor->Done = false;
	return true;
}



/* 返回下一个 MIME 参数。 */
XRT_API xmailnext xrtMailParamNext(
	xmailparamcursor* pCursor,
	xmailparamview* pParameter
)
{
	xmailparamcursor Cursor;
	xmailparamview Parameter;
	size_t iStart;
	size_t iValueStart;
	size_t iValueEnd;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		 !xrtMemRangeValid(pParameter, sizeof(*pParameter)) ||
		 !__xrtMailViewValid(pCursor != NULL ? pCursor->Text :
			__xrtMailView(NULL, 0)) ||
		 (pCursor->Position > pCursor->Text.Size) ||
		 xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pParameter,
			sizeof(*pParameter)) ||
		 xrtMemRangesOverlap(pParameter, sizeof(*pParameter),
			pCursor->Text.Data, pCursor->Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( pCursor->Done ) {
		return XMAIL_NEXT_END;
	}
	Cursor = *pCursor;
	__xrtMailParamWhitespace(Cursor.Text, &Cursor.Position);
	if ( Cursor.Position == Cursor.Text.Size ) {
		Cursor.Done = true;
		*pCursor = Cursor;
		return XMAIL_NEXT_END;
	}
	iStart = Cursor.Position;
	if ( Cursor.Text.Data[Cursor.Position++] != ';' ) {
		__xrtMailParamError("MIME parameter requires a semicolon");
		return XMAIL_NEXT_ERROR;
	}
	__xrtMailParamWhitespace(Cursor.Text, &Cursor.Position);
	if ( !__xrtMailParamToken(
		Cursor.Text,
		&Cursor.Position,
		&Parameter.RawName
	) ) {
		__xrtMailParamError("MIME parameter has an invalid name");
		return XMAIL_NEXT_ERROR;
	}
	__xrtMailParamWhitespace(Cursor.Text, &Cursor.Position);
	if ( (Cursor.Position >= Cursor.Text.Size) ||
		 (Cursor.Text.Data[Cursor.Position++] != '=') ) {
		__xrtMailParamError("MIME parameter has no value");
		return XMAIL_NEXT_ERROR;
	}
	__xrtMailParamWhitespace(Cursor.Text, &Cursor.Position);
	iValueStart = Cursor.Position;
	Parameter.Quoted = (Cursor.Position < Cursor.Text.Size) &&
		(Cursor.Text.Data[Cursor.Position] == '"');
	if ( Parameter.Quoted ) {
		bool bEscape = false;

		Cursor.Position++;
		iValueStart = Cursor.Position;
		while ( Cursor.Position < Cursor.Text.Size ) {
			unsigned char iByte = (unsigned char)Cursor.Text.Data[Cursor.Position++];

			if ( bEscape ) {
				if ( (iByte == 0) || (iByte == (unsigned char)'\r') ||
					 (iByte == (unsigned char)'\n') ) {
					__xrtMailParamError("MIME parameter has an invalid quoted-pair");
					return XMAIL_NEXT_ERROR;
				}
				bEscape = false;
				continue;
			}
			if ( iByte == (unsigned char)'\\' ) {
				bEscape = true;
				continue;
			}
			if ( iByte == (unsigned char)'"' ) {
				iValueEnd = Cursor.Position - 1u;
				goto quoted_complete;
			}
			if ( (iByte == 0) || (iByte == (unsigned char)'\r') ||
				 (iByte == (unsigned char)'\n') ||
				 ((iByte < 32u) && (iByte != (unsigned char)'\t')) ||
				 (iByte == 127u) ) {
				__xrtMailParamError("MIME parameter has an invalid quoted value");
				return XMAIL_NEXT_ERROR;
			}
		}
		__xrtMailParamError("MIME parameter has an unterminated quoted value");
		return XMAIL_NEXT_ERROR;
	quoted_complete:
		Parameter.RawValue = __xrtMailView(
			Cursor.Text.Data + iValueStart - 1u,
			Cursor.Position - (iValueStart - 1u)
		);
		Parameter.Value = __xrtMailView(
			Cursor.Text.Data + iValueStart,
			iValueEnd - iValueStart
		);
	} else {
		if ( !__xrtMailParamToken(
			Cursor.Text,
			&Cursor.Position,
			&Parameter.Value
		) ) {
			__xrtMailParamError("MIME parameter has an invalid token value");
			return XMAIL_NEXT_ERROR;
		}
		Parameter.RawValue = Parameter.Value;
	}
	__xrtMailParamWhitespace(Cursor.Text, &Cursor.Position);
	if ( (Cursor.Position < Cursor.Text.Size) &&
		 (Cursor.Text.Data[Cursor.Position] != ';') ) {
		__xrtMailParamError("MIME parameter has trailing invalid bytes");
		return XMAIL_NEXT_ERROR;
	}
	if ( !__xrtMailParamName(
		Parameter.RawName,
		&Parameter.Name,
		&Parameter.Section,
		&Parameter.Extended,
		&Parameter.Continued
	) ) {
		__xrtMailParamError("MIME parameter has an invalid section suffix");
		return XMAIL_NEXT_ERROR;
	}
	Parameter.Source = __xrtMailView(
		Cursor.Text.Data + iStart,
		Cursor.Position - iStart
	);
	*pParameter = Parameter;
	*pCursor = Cursor;
	return XMAIL_NEXT_ITEM;
}



/* 验证参数块可以被游标完整消费。 */
static bool __xrtMailParamValidate(xstrview Parameters)
{
	xmailparamcursor Cursor;
	xmailparamview Parameter;
	xmailnext Next;

	if ( !xrtMailParamCursorInit(&Cursor, Parameters) ) {
		return false;
	}
	while ( (Next = xrtMailParamNext(&Cursor, &Parameter)) == XMAIL_NEXT_ITEM ) {
	}
	return Next == XMAIL_NEXT_END;
}



/* 解析并验证 Content-Type 字段值。 */
XRT_API bool xrtMailMediaTypeParse(
	xstrview Text,
	xmailmediatypeview* pMediaType
)
{
	xstrview Source;
	xmailmediatypeview Result;
	size_t iPosition = 0;

	if ( !__xrtMailViewValid(Text) ||
		 !xrtMemRangeValid(pMediaType, sizeof(*pMediaType)) ||
		 xrtMemRangesOverlap(pMediaType, sizeof(*pMediaType), Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	Source = __xrtMailParamTrim(Text);
	if ( !__xrtMailParamToken(Source, &iPosition, &Result.Type) ||
		 (iPosition >= Source.Size) || (Source.Data[iPosition++] != '/') ||
		 !__xrtMailParamToken(Source, &iPosition, &Result.Subtype) ) {
		return __xrtMailParamError("Content-Type requires type/subtype");
	}
	__xrtMailParamWhitespace(Source, &iPosition);
	Result.Source = Source;
	Result.Parameters = __xrtMailView(
		Source.Data + iPosition,
		Source.Size - iPosition
	);
	if ( !__xrtMailParamValidate(Result.Parameters) ) {
		return false;
	}
	*pMediaType = Result;
	return true;
}



/* 解析并验证 Content-Disposition 字段值。 */
XRT_API bool xrtMailDispositionParse(
	xstrview Text,
	xmaildispositionview* pDisposition
)
{
	xstrview Source;
	xmaildispositionview Result;
	size_t iPosition = 0;

	if ( !__xrtMailViewValid(Text) ||
		 !xrtMemRangeValid(pDisposition, sizeof(*pDisposition)) ||
		 xrtMemRangesOverlap(pDisposition, sizeof(*pDisposition),
			Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	Source = __xrtMailParamTrim(Text);
	if ( !__xrtMailParamToken(Source, &iPosition, &Result.Type) ) {
		return __xrtMailParamError("Content-Disposition requires a token");
	}
	__xrtMailParamWhitespace(Source, &iPosition);
	Result.Source = Source;
	Result.Parameters = __xrtMailView(
		Source.Data + iPosition,
		Source.Size - iPosition
	);
	if ( !__xrtMailParamValidate(Result.Parameters) ) {
		return false;
	}
	*pDisposition = Result;
	return true;
}



/* 定位扩展参数第一段的 charset'language' 前缀。 */
static bool __xrtMailParamExtendedPrefix(
	const xmailparamview* pParameter,
	xstrview* pEncoded,
	xstrview* pCharset,
	xstrview* pLanguage
)
{
	size_t iFirst = XRT_NPOS;
	size_t iSecond = XRT_NPOS;

	*pEncoded = pParameter->Value;
	*pCharset = __xrtMailView(NULL, 0);
	*pLanguage = __xrtMailView(NULL, 0);
	if ( !pParameter->Extended ||
		 (pParameter->Continued && (pParameter->Section != 0)) ) {
		return true;
	}
	if ( pParameter->Quoted ) {
		return __xrtMailParamError("extended MIME parameter cannot be quoted");
	}
	for ( size_t i = 0; i < pParameter->Value.Size; i++ ) {
		if ( pParameter->Value.Data[i] == '\'' ) {
			if ( iFirst == XRT_NPOS ) {
				iFirst = i;
			} else {
				iSecond = i;
				break;
			}
		}
	}
	if ( (iFirst == 0) || (iFirst == XRT_NPOS) || (iSecond == XRT_NPOS) ) {
		return __xrtMailParamError(
			"extended MIME parameter has no charset or language prefix"
		);
	}
	*pCharset = __xrtMailView(pParameter->Value.Data, iFirst);
	*pLanguage = __xrtMailView(
		pParameter->Value.Data + iFirst + 1u,
		iSecond - iFirst - 1u
	);
	*pEncoded = __xrtMailView(
		pParameter->Value.Data + iSecond + 1u,
		pParameter->Value.Size - iSecond - 1u
	);
	return true;
}



/* 计算并可选写入一个参数 section。 */
static bool __xrtMailParamDecodeBody(
	const xmailparamview* pParameter,
	xstrview Encoded,
	char* sOutput,
	size_t* pOutputSize
)
{
	size_t iOutput = 0;

	for ( size_t i = 0; i < Encoded.Size; i++ ) {
		unsigned char iByte = (unsigned char)Encoded.Data[i];

		if ( pParameter->Extended && (iByte == (unsigned char)'%') ) {
			int iHigh;
			int iLow;

			if ( (i + 2u) >= Encoded.Size ||
				 ((iHigh = __xrtMailHexValue(
					(unsigned char)Encoded.Data[i + 1u]
				)) < 0) ||
				 ((iLow = __xrtMailHexValue(
					(unsigned char)Encoded.Data[i + 2u]
				)) < 0) ) {
				return __xrtMailParamError(
					"extended MIME parameter has invalid percent encoding"
				);
			}
			iByte = (unsigned char)((iHigh << 4) | iLow);
			i += 2u;
		} else if ( !pParameter->Extended && pParameter->Quoted &&
			 (iByte == (unsigned char)'\\') ) {
			if ( ++i >= Encoded.Size ) {
				return __xrtMailParamError(
					"MIME parameter has a dangling quoted-pair"
				);
			}
			iByte = (unsigned char)Encoded.Data[i];
		}
		if ( (iByte == 0) || (iByte == 127u) ||
			 ((iByte < 32u) && (iByte != (unsigned char)'\t')) ) {
			return __xrtMailParamError(
				"decoded MIME parameter contains a control byte"
			);
		}
		if ( sOutput != NULL ) {
			sOutput[iOutput] = (char)iByte;
		}
		iOutput++;
	}
	*pOutputSize = iOutput;
	return true;
}



/* 解码单个 MIME 参数 section。 */
XRT_API bool xrtMailParamDecodeWrite(
	const xmailparamview* pParameter,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	xstrview* pCharset,
	xstrview* pLanguage
)
{
	xstrview Encoded;
	xstrview Charset;
	xstrview Language;
	size_t iRequired;

	if ( !xrtMemRangeValid(pParameter, sizeof(*pParameter)) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 !xrtMemRangeValid(pCharset, pCharset != NULL ? sizeof(*pCharset) : 0) ||
		 !xrtMemRangeValid(pLanguage, pLanguage != NULL ? sizeof(*pLanguage) : 0) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailViewValid(pParameter->Source) ||
		 !__xrtMailViewValid(pParameter->RawName) ||
		 !__xrtMailViewValid(pParameter->Name) ||
		 !__xrtMailViewValid(pParameter->RawValue) ||
		 !__xrtMailViewValid(pParameter->Value) ||
		 (pParameter->Name.Size == 0) ||
		 (pParameter->Continued !=
		  (pParameter->Section != XMAIL_PARAM_SECTION_NONE)) ||
		 xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			pParameter->Value.Data,
			pParameter->Value.Size
		) || ((pCharset != NULL) &&
		  xrtMemRangesOverlap(pCharset, sizeof(*pCharset),
			pParameter->Value.Data, pParameter->Value.Size)) ||
		 ((pLanguage != NULL) &&
		  xrtMemRangesOverlap(pLanguage, sizeof(*pLanguage),
			pParameter->Value.Data, pParameter->Value.Size)) ||
		 ((pCharset != NULL) && (pLanguage != NULL) &&
		  xrtMemRangesOverlap(pCharset, sizeof(*pCharset),
			pLanguage, sizeof(*pLanguage))) ||
		 ((pCharset != NULL) && xrtMemRangesOverlap(
			pCharset, sizeof(*pCharset), pOutputSize, sizeof(*pOutputSize))) ||
		 ((pLanguage != NULL) && xrtMemRangesOverlap(
			pLanguage, sizeof(*pLanguage), pOutputSize, sizeof(*pOutputSize))) ||
		 ((sOutput != NULL) &&
		  (xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize),
			sOutput, iCapacity) ||
		   ((pCharset != NULL) && xrtMemRangesOverlap(
			pCharset, sizeof(*pCharset), sOutput, iCapacity)) ||
		   ((pLanguage != NULL) && xrtMemRangesOverlap(
			pLanguage, sizeof(*pLanguage), sOutput, iCapacity)))) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailParamExtendedPrefix(
		pParameter,
		&Encoded,
		&Charset,
		&Language
	) || !__xrtMailParamDecodeBody(
		pParameter,
		Encoded,
		NULL,
		&iRequired
	) ) {
		return false;
	}
	if ( sOutput != NULL ) {
		if ( iCapacity <= iRequired ) {
			*pOutputSize = iRequired;
			__xrtMailSetRange();
			return false;
		}
		if ( xrtMemRangesOverlap(
			sOutput,
			iRequired + 1u,
			pParameter->Value.Data,
			pParameter->Value.Size
		) && (sOutput != pParameter->Value.Data) ) {
			__xrtMailSetInvalidArgument();
			return false;
		}
		if ( !__xrtMailParamDecodeBody(
			pParameter,
			Encoded,
			sOutput,
			&iRequired
		) ) {
			return false;
		}
		sOutput[iRequired] = 0;
	}
	*pOutputSize = iRequired;
	if ( pCharset != NULL ) {
		*pCharset = Charset;
	}
	if ( pLanguage != NULL ) {
		*pLanguage = Language;
	}
	return true;
}



/* 查找一种参数表示并校验重复和 section 连续性。 */
static xmailnext __xrtMailParamSelect(
	xstrview Parameters,
	xstrview Name,
	xmailparamview* pSingle,
	bool* pContinued,
	size_t* pSections
)
{
	xmailparamcursor Cursor;
	xmailparamview Parameter;
	xmailparamview Plain;
	xmailparamview Extended;
	xmailnext Next;
	size_t iContinuedCount = 0;
	size_t iMaxSection = 0;
	bool bPlain = false;
	bool bExtended = false;

	if ( !xrtMailParamCursorInit(&Cursor, Parameters) ) {
		return XMAIL_NEXT_ERROR;
	}
	while ( (Next = xrtMailParamNext(&Cursor, &Parameter)) == XMAIL_NEXT_ITEM ) {
		if ( !__xrtMailAsciiEqualI(Parameter.Name, Name) ) {
			continue;
		}
		if ( Parameter.Continued ) {
			iContinuedCount++;
			if ( Parameter.Section > iMaxSection ) {
				iMaxSection = Parameter.Section;
			}
		} else if ( Parameter.Extended ) {
			if ( bExtended ) {
				__xrtMailParamError("MIME parameter has duplicate extended values");
				return XMAIL_NEXT_ERROR;
			}
			Extended = Parameter;
			bExtended = true;
		} else {
			if ( bPlain ) {
				__xrtMailParamError("MIME parameter has duplicate values");
				return XMAIL_NEXT_ERROR;
			}
			Plain = Parameter;
			bPlain = true;
		}
	}
	if ( Next == XMAIL_NEXT_ERROR ) {
		return XMAIL_NEXT_ERROR;
	}
	if ( iContinuedCount != 0 ) {
		if ( (iMaxSection + 1u) != iContinuedCount ) {
			__xrtMailParamError("MIME parameter sections are missing or duplicated");
			return XMAIL_NEXT_ERROR;
		}
		*pContinued = true;
		*pSections = iContinuedCount;
		return XMAIL_NEXT_ITEM;
	}
	if ( bExtended || bPlain ) {
		*pSingle = bExtended ? Extended : Plain;
		*pContinued = false;
		*pSections = 1u;
		return XMAIL_NEXT_ITEM;
	}
	return XMAIL_NEXT_END;
}



/* 在连续表示中查找唯一 section。 */
static bool __xrtMailParamFindSection(
	xstrview Parameters,
	xstrview Name,
	size_t iSection,
	xmailparamview* pParameter
)
{
	xmailparamcursor Cursor;
	xmailparamview Parameter;
	xmailnext Next;
	bool bFound = false;

	if ( !xrtMailParamCursorInit(&Cursor, Parameters) ) {
		return false;
	}
	while ( (Next = xrtMailParamNext(&Cursor, &Parameter)) == XMAIL_NEXT_ITEM ) {
		if ( Parameter.Continued && (Parameter.Section == iSection) &&
			 __xrtMailAsciiEqualI(Parameter.Name, Name) ) {
			if ( bFound ) {
				return __xrtMailParamError("MIME parameter section is duplicated");
			}
			*pParameter = Parameter;
			bFound = true;
		}
	}
	if ( Next == XMAIL_NEXT_ERROR ) {
		return false;
	}
	return bFound || __xrtMailParamError("MIME parameter section is missing");
}



/* 查找、合并并解码 MIME 参数。 */
XRT_API xmailnext xrtMailParamFindWrite(
	xstrview Parameters,
	xstrview Name,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	xmailparaminfo* pInfo
)
{
	xmailparamview Parameter;
	xmailparaminfo Info;
	xmailnext Next;
	size_t iSections;
	size_t iRequired = 0;
	bool bContinued;

	if ( !__xrtMailViewValid(Parameters) || !__xrtMailViewValid(Name) ||
		 (Name.Size == 0) || !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 !xrtMemRangeValid(pInfo, pInfo != NULL ? sizeof(*pInfo) : 0) ||
		 xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize),
			Parameters.Data, Parameters.Size) ||
		 xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize),
			Name.Data, Name.Size) ||
		 ((pInfo != NULL) &&
		  (xrtMemRangesOverlap(pInfo, sizeof(*pInfo),
			Parameters.Data, Parameters.Size) ||
		   xrtMemRangesOverlap(pInfo, sizeof(*pInfo), Name.Data, Name.Size) ||
		   xrtMemRangesOverlap(pInfo, sizeof(*pInfo),
			pOutputSize, sizeof(*pOutputSize)))) ||
		 ((sOutput != NULL) && xrtMemRangesOverlap(
			sOutput,
			iCapacity,
			Parameters.Data,
			Parameters.Size
		)) || ((sOutput != NULL) &&
		  (xrtMemRangesOverlap(sOutput, iCapacity, Name.Data, Name.Size) ||
		   xrtMemRangesOverlap(sOutput, iCapacity,
			pOutputSize, sizeof(*pOutputSize)) ||
		   ((pInfo != NULL) && xrtMemRangesOverlap(
			sOutput, iCapacity, pInfo, sizeof(*pInfo))))) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	for ( size_t i = 0; i < Name.Size; i++ ) {
		if ( !__xrtMailParamTokenByte((unsigned char)Name.Data[i]) ||
			 (Name.Data[i] == '*') ) {
			__xrtMailSetInvalidArgument();
			return XMAIL_NEXT_ERROR;
		}
	}
	Next = __xrtMailParamSelect(
		Parameters,
		Name,
		&Parameter,
		&bContinued,
		&iSections
	);
	if ( Next != XMAIL_NEXT_ITEM ) {
		return Next;
	}
	memset(&Info, 0, sizeof(Info));
	Info.Sections = iSections;
	Info.Continued = bContinued;
	for ( size_t i = 0; i < iSections; i++ ) {
		xstrview Charset;
		xstrview Language;
		size_t iPartSize;

		if ( bContinued && !__xrtMailParamFindSection(
			Parameters,
			Name,
			i,
			&Parameter
		) ) {
			return XMAIL_NEXT_ERROR;
		}
		if ( !xrtMailParamDecodeWrite(
			&Parameter,
			NULL,
			0,
			&iPartSize,
			&Charset,
			&Language
		) || !__xrtMailSizeAdd(iRequired, iPartSize, &iRequired) ) {
			return XMAIL_NEXT_ERROR;
		}
		if ( i == 0 ) {
			Info.Charset = Charset;
			Info.Language = Language;
		}
		Info.Extended = Info.Extended || Parameter.Extended;
	}
	if ( sOutput != NULL ) {
		size_t iOutput = 0;

		if ( iCapacity <= iRequired ) {
			*pOutputSize = iRequired;
			__xrtMailSetRange();
			return XMAIL_NEXT_ERROR;
		}
		for ( size_t i = 0; i < iSections; i++ ) {
			size_t iPartSize;

			if ( bContinued && !__xrtMailParamFindSection(
				Parameters,
				Name,
				i,
				&Parameter
			) ) {
				return XMAIL_NEXT_ERROR;
			}
			if ( !xrtMailParamDecodeWrite(
				&Parameter,
				sOutput + iOutput,
				iCapacity - iOutput,
				&iPartSize,
				NULL,
				NULL
			) ) {
				return XMAIL_NEXT_ERROR;
			}
			iOutput += iPartSize;
		}
		sOutput[iRequired] = 0;
	}
	*pOutputSize = iRequired;
	if ( pInfo != NULL ) {
		*pInfo = Info;
	}
	return XMAIL_NEXT_ITEM;
}



/* 创建独立的合并 MIME 参数。 */
XRT_API str xrtMailParamFind(
	xstrview Parameters,
	xstrview Name,
	size_t* pOutputSize,
	xmailparaminfo* pInfo
)
{
	xmailparaminfo Info;
	xmailnext Next;
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMemRangeValid(pInfo, pInfo != NULL ? sizeof(*pInfo) : 0) ) {
		__xrtMailSetInvalidArgument();
		return NULL;
	}
	Next = xrtMailParamFindWrite(
		Parameters,
		Name,
		NULL,
		0,
		&iRequired,
		&Info
	);
	if ( Next != XMAIL_NEXT_ITEM ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( xrtMailParamFindWrite(
		Parameters,
		Name,
		sOutput,
		iRequired + 1u,
		&iRequired,
		&Info
	) != XMAIL_NEXT_ITEM ) {
		xrtFree(sOutput);
		return NULL;
	}
	if ( pOutputSize != NULL ) {
		*pOutputSize = iRequired;
	}
	if ( pInfo != NULL ) {
		*pInfo = Info;
	}
	return sOutput;
}



/* 判断字节能否直接出现在 RFC 2231 扩展参数值中。 */
static bool __xrtMailParamAttributeByte(unsigned char iByte)
{
	if ( ((iByte >= (unsigned char)'a') && (iByte <= (unsigned char)'z')) ||
		 ((iByte >= (unsigned char)'A') && (iByte <= (unsigned char)'Z')) ||
		 ((iByte >= (unsigned char)'0') && (iByte <= (unsigned char)'9')) ) {
		return true;
	}
	switch ( iByte ) {
		case '!':
		case '#':
		case '$':
		case '&':
		case '+':
		case '-':
		case '.':
		case '^':
		case '_':
		case '`':
		case '|':
		case '~':
			return true;
		default:
			return false;
	}
}



/* 验证参数名和值，选择编码并精确计量。 */
static bool __xrtMailParamWriteMeasure(
	xstrview Name,
	xstrview Value,
	xmailparamencoding Encoding,
	xmailparamencoding* pSelected,
	size_t* pValueSize,
	size_t* pRequired
)
{
	bool bAscii = true;
	bool bToken = Value.Size != 0;
	size_t iValueSize = 0;
	size_t iRequired;

	if ( !__xrtMailViewValid(Name) || !__xrtMailViewValid(Value) ||
		 (Name.Size == 0) ||
		 (Encoding < XMAIL_PARAM_ENCODING_AUTO) ||
		 (Encoding > XMAIL_PARAM_ENCODING_UTF8) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	for ( size_t i = 0; i < Name.Size; i++ ) {
		if ( !__xrtMailParamTokenByte((unsigned char)Name.Data[i]) ||
			 (Name.Data[i] == '*') ) {
			__xrtMailSetInvalidArgument();
			return false;
		}
	}
	for ( size_t i = 0; i < Value.Size; i++ ) {
		unsigned char iByte = (unsigned char)Value.Data[i];

		if ( (iByte == 0) || (iByte == 127u) ||
			 ((iByte < 32u) && (iByte != (unsigned char)'\t')) ) {
			__xrtMailSetInvalidArgument();
			return false;
		}
		bAscii = bAscii && (iByte < 128u);
		bToken = bToken && __xrtMailParamTokenByte(iByte);
	}
	if ( !bAscii && !xrtUtf8Valid(Value, NULL) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( Encoding == XMAIL_PARAM_ENCODING_AUTO ) {
		Encoding = bToken ? XMAIL_PARAM_ENCODING_TOKEN :
			(bAscii ? XMAIL_PARAM_ENCODING_QUOTED :
			 XMAIL_PARAM_ENCODING_UTF8);
	} else if ( ((Encoding == XMAIL_PARAM_ENCODING_TOKEN) && !bToken) ||
		 ((Encoding == XMAIL_PARAM_ENCODING_QUOTED) && !bAscii) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}

	if ( Encoding == XMAIL_PARAM_ENCODING_TOKEN ) {
		iValueSize = Value.Size;
	} else if ( Encoding == XMAIL_PARAM_ENCODING_QUOTED ) {
		iValueSize = 2u;
		for ( size_t i = 0; i < Value.Size; i++ ) {
			if ( ((Value.Data[i] == '\"') || (Value.Data[i] == '\\')) &&
				 !__xrtMailSizeAdd(iValueSize, 1u, &iValueSize) ) {
				return false;
			}
			if ( !__xrtMailSizeAdd(iValueSize, 1u, &iValueSize) ) {
				return false;
			}
		}
	} else {
		iValueSize = 8u;
		for ( size_t i = 0; i < Value.Size; i++ ) {
			size_t iByteSize = __xrtMailParamAttributeByte(
				(unsigned char)Value.Data[i]
			) ? 1u : 3u;

			if ( !__xrtMailSizeAdd(iValueSize, iByteSize, &iValueSize) ) {
				return false;
			}
		}
	}
	if ( !__xrtMailSizeAdd(2u, Name.Size, &iRequired) ||
		 !__xrtMailSizeAdd(iRequired, 1u, &iRequired) ||
		 !__xrtMailSizeAdd(iRequired, iValueSize, &iRequired) ) {
		return false;
	}
	*pSelected = Encoding;
	*pValueSize = iValueSize -
		(Encoding == XMAIL_PARAM_ENCODING_QUOTED ? 2u :
		 (Encoding == XMAIL_PARAM_ENCODING_UTF8 ? 7u : 0u));
	*pRequired = iRequired;
	return true;
}



/* 返回连续段编号的十进制位数。 */
static size_t __xrtMailParamDigits(size_t iValue)
{
	size_t iDigits = 1u;

	while ( iValue >= 10u ) {
		iValue /= 10u;
		iDigits++;
	}
	return iDigits;
}



/* 返回一个源字节编码后的参数值长度。 */
static size_t __xrtMailParamEncodedByte(
	xmailparamencoding Encoding,
	unsigned char iByte
)
{
	if ( Encoding == XMAIL_PARAM_ENCODING_QUOTED ) {
		return ((iByte == (unsigned char)'\"') ||
			(iByte == (unsigned char)'\\')) ? 2u : 1u;
	}
	if ( Encoding == XMAIL_PARAM_ENCODING_UTF8 ) {
		return __xrtMailParamAttributeByte(iByte) ? 1u : 3u;
	}
	return 1u;
}



/* 返回一个源字符占用的字节数和编码后长度。 */
static void __xrtMailParamEncodedUnit(
	xstrview Value,
	xmailparamencoding Encoding,
	size_t iPosition,
	size_t* pSourceSize,
	size_t* pEncodedSize
)
{
	unsigned char iByte = (unsigned char)Value.Data[iPosition];
	size_t iSourceSize = 1u;
	size_t iEncodedSize = __xrtMailParamEncodedByte(Encoding, iByte);

	if ( (Encoding == XMAIL_PARAM_ENCODING_UTF8) && (iByte >= 128u) ) {
		iSourceSize = (iByte < 224u) ? 2u : ((iByte < 240u) ? 3u : 4u);
		iEncodedSize = iSourceSize * 3u;
	}
	*pSourceSize = iSourceSize;
	*pEncodedSize = iEncodedSize;
}



/* 在不拆开转义单元或 UTF-8 字符的前提下取得下一个连续段。 */
static void __xrtMailParamChunk(
	xstrview Value,
	xmailparamencoding Encoding,
	size_t iPosition,
	size_t* pEnd,
	size_t* pEncodedSize
)
{
	size_t iSize = 0;
	size_t iEnd = iPosition;

	while ( iEnd < Value.Size ) {
		size_t iSourceSize;
		size_t iUnitSize;

		__xrtMailParamEncodedUnit(
			Value,
			Encoding,
			iEnd,
			&iSourceSize,
			&iUnitSize
		);
		if ( (iSize != 0) && ((iSize + iUnitSize) > XMAIL_PARAM_SECTION_SIZE) ) {
			break;
		}
		iSize += iUnitSize;
		iEnd += iSourceSize;
	}
	*pEnd = iEnd;
	*pEncodedSize = iSize;
}



/* 精确计量自动连续分段后的参数文本。 */
static bool __xrtMailParamSectionsMeasure(
	xstrview Name,
	xstrview Value,
	xmailparamencoding Encoding,
	size_t iValueSize,
	size_t iSingleSize,
	size_t* pRequired,
	size_t* pSections
)
{
	size_t iRequired = 0;
	size_t iPosition = 0;
	size_t iSection = 0;

	if ( iValueSize <= XMAIL_PARAM_SECTION_SIZE ) {
		*pRequired = iSingleSize;
		*pSections = 1u;
		return true;
	}
	while ( iPosition < Value.Size ) {
		size_t iEnd;
		size_t iEncodedSize;
		size_t iPartSize;

		if ( iSection >= XMAIL_PARAM_SECTIONS_MAX ) {
			__xrtMailError(
				XERR_RANGE,
				XMAIL_ERROR_LIMIT,
				"MIME parameter requires too many continuation sections"
			);
			return false;
		}
		__xrtMailParamChunk(Value, Encoding, iPosition, &iEnd, &iEncodedSize);
		iPartSize = 2u;
		if ( !__xrtMailSizeAdd(iPartSize, Name.Size, &iPartSize) ||
			 !__xrtMailSizeAdd(iPartSize, 1u, &iPartSize) ||
			 !__xrtMailSizeAdd(iPartSize, __xrtMailParamDigits(iSection),
				&iPartSize) ||
			 !__xrtMailSizeAdd(iPartSize,
				Encoding == XMAIL_PARAM_ENCODING_UTF8 ? 1u : 0u,
				&iPartSize) ||
			 !__xrtMailSizeAdd(iPartSize, 1u, &iPartSize) ||
			 !__xrtMailSizeAdd(iPartSize,
				Encoding == XMAIL_PARAM_ENCODING_QUOTED ? 2u :
				((Encoding == XMAIL_PARAM_ENCODING_UTF8) && (iSection == 0) ?
				 7u : 0u),
				&iPartSize) ||
			 !__xrtMailSizeAdd(iPartSize, iEncodedSize, &iPartSize) ||
			 !__xrtMailSizeAdd(iRequired, iPartSize, &iRequired) ) {
			return false;
		}
		iPosition = iEnd;
		iSection++;
	}
	*pRequired = iRequired;
	*pSections = iSection;
	return true;
}



/* 写出一个十进制连续段编号。 */
static size_t __xrtMailParamWriteDigits(char* sOutput, size_t iValue)
{
	char arrDigits[32];
	size_t iDigits = 0;

	do {
		arrDigits[iDigits++] = (char)('0' + (iValue % 10u));
		iValue /= 10u;
	} while ( iValue != 0 );
	for ( size_t i = 0; i < iDigits; i++ ) {
		sOutput[i] = arrDigits[iDigits - i - 1u];
	}
	return iDigits;
}



/* 写出一段 token、quoted-string 或扩展参数值。 */
static size_t __xrtMailParamWriteValue(
	char* sOutput,
	xstrview Value,
	xmailparamencoding Encoding,
	bool bFirst,
	bool bQuoted
)
{
	static const char arrHex[] = "0123456789ABCDEF";
	size_t iOutput = 0;

	if ( bQuoted ) {
		sOutput[iOutput++] = '\"';
	} else if ( (Encoding == XMAIL_PARAM_ENCODING_UTF8) && bFirst ) {
		memcpy(sOutput + iOutput, "UTF-8''", 7u);
		iOutput += 7u;
	}
	for ( size_t i = 0; i < Value.Size; i++ ) {
		unsigned char iByte = (unsigned char)Value.Data[i];

		if ( (Encoding == XMAIL_PARAM_ENCODING_QUOTED) &&
			 ((iByte == (unsigned char)'\"') ||
			  (iByte == (unsigned char)'\\')) ) {
			sOutput[iOutput++] = '\\';
			sOutput[iOutput++] = (char)iByte;
		} else if ( (Encoding == XMAIL_PARAM_ENCODING_UTF8) &&
			 !__xrtMailParamAttributeByte(iByte) ) {
			sOutput[iOutput++] = '%';
			sOutput[iOutput++] = arrHex[iByte >> 4];
			sOutput[iOutput++] = arrHex[iByte & 15u];
		} else {
			sOutput[iOutput++] = (char)iByte;
		}
	}
	if ( bQuoted ) {
		sOutput[iOutput++] = '\"';
	}
	return iOutput;
}



/* 写出包含分号前缀的 MIME 参数，长值自动使用连续段。 */
XRT_API bool xrtMailParamWrite(
	xstrview Name,
	xstrview Value,
	xmailparamencoding Encoding,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	xmailparamencoding Selected;
	size_t iValueSize;
	size_t iRequired;
	size_t iSections;
	size_t iOutput = 0;

	if ( !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			Name.Data,
			Name.Size
		 ) || xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			Value.Data,
			Value.Size
		 ) || ((sOutput != NULL) && xrtMemRangesOverlap(
			sOutput,
			iCapacity,
			pOutputSize,
			sizeof(*pOutputSize)
		 )) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailParamWriteMeasure(
		Name,
		Value,
		Encoding,
		&Selected,
		&iValueSize,
		&iRequired
	) || !__xrtMailParamSectionsMeasure(
		Name,
		Value,
		Selected,
		iValueSize,
		iRequired,
		&iRequired,
		&iSections
	) ) {
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
	if ( xrtMemRangesOverlap(
		sOutput,
		iRequired + 1u,
		Name.Data,
		Name.Size
	) || xrtMemRangesOverlap(
		sOutput,
		iRequired + 1u,
		Value.Data,
		Value.Size
	) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}

	if ( iSections == 1u ) {
		sOutput[iOutput++] = ';';
		sOutput[iOutput++] = ' ';
		memcpy(sOutput + iOutput, Name.Data, Name.Size);
		iOutput += Name.Size;
		if ( Selected == XMAIL_PARAM_ENCODING_UTF8 ) {
			sOutput[iOutput++] = '*';
		}
		sOutput[iOutput++] = '=';
		iOutput += __xrtMailParamWriteValue(
			sOutput + iOutput,
			Value,
			Selected,
			true,
			Selected == XMAIL_PARAM_ENCODING_QUOTED
		);
	} else {
		size_t iPosition = 0;

		for ( size_t iSection = 0; iSection < iSections; iSection++ ) {
			size_t iEnd;
			size_t iEncodedSize;

			__xrtMailParamChunk(Value, Selected, iPosition,
				&iEnd, &iEncodedSize);
			(void)iEncodedSize;
			sOutput[iOutput++] = ';';
			sOutput[iOutput++] = ' ';
			memcpy(sOutput + iOutput, Name.Data, Name.Size);
			iOutput += Name.Size;
			sOutput[iOutput++] = '*';
			iOutput += __xrtMailParamWriteDigits(
				sOutput + iOutput,
				iSection
			);
			if ( Selected == XMAIL_PARAM_ENCODING_UTF8 ) {
				sOutput[iOutput++] = '*';
			}
			sOutput[iOutput++] = '=';
			iOutput += __xrtMailParamWriteValue(
				sOutput + iOutput,
				__xrtMailView(Value.Data + iPosition, iEnd - iPosition),
				Selected,
				iSection == 0,
				Selected == XMAIL_PARAM_ENCODING_QUOTED
			);
			iPosition = iEnd;
		}
	}
	sOutput[iOutput] = 0;
	*pOutputSize = iRequired;
	return true;
}



/* 创建独立的单个 MIME 参数。 */
XRT_API str xrtMailParam(
	xstrview Name,
	xstrview Value,
	xmailparamencoding Encoding,
	size_t* pOutputSize
)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) ) {
		__xrtMailSetInvalidArgument();
		return NULL;
	}
	if ( (pOutputSize != NULL) && (xrtMemRangesOverlap(
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
	if ( !xrtMailParamWrite(
		Name,
		Value,
		Encoding,
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
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtMailParamWrite(
		Name,
		Value,
		Encoding,
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
