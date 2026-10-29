#include "../internal/xrt_mail.h"



#if defined(XMAIL_FEATURE_MAIL_ADDRESS)

typedef enum __xmailaddressdelimiter {
	__XMAIL_ADDRESS_END = 0,
	__XMAIL_ADDRESS_COMMA,
	__XMAIL_ADDRESS_COLON,
	__XMAIL_ADDRESS_SEMICOLON
} __xmailaddressdelimiter;



typedef enum __xmailaddressname {
	__XMAIL_ADDRESS_NAME_RAW = 0,
	__XMAIL_ADDRESS_NAME_QUOTED,
	__XMAIL_ADDRESS_NAME_WORD
} __xmailaddressname;



/* 发布稳定的地址语法错误。 */
static bool __xrtMailAddressError(cstr sMessage)
{
	__xrtMailError(XERR_PROTOCOL, XMAIL_ERROR_ADDRESS, sMessage);
	return false;
}



/* 消费一段合法折叠空白。 */
static bool __xrtMailAddressFws(
	xstrview Text,
	size_t* pPosition,
	bool* pConsumed
)
{
	size_t i = *pPosition;
	bool bConsumed = false;

	while ( i < Text.Size ) {
		if ( (Text.Data[i] == ' ') || (Text.Data[i] == '\t') ) {
			i++;
			bConsumed = true;
			continue;
		}
		if ( Text.Data[i] == '\n' ) {
			return __xrtMailAddressError("mail address contains a bare LF");
		}
		if ( Text.Data[i] != '\r' ) {
			break;
		}
		if ( ((i + 2u) >= Text.Size) || (Text.Data[i + 1u] != '\n') ||
			 ((Text.Data[i + 2u] != ' ') && (Text.Data[i + 2u] != '\t')) ) {
			return __xrtMailAddressError("mail address contains invalid folding");
		}
		i += 3u;
		bConsumed = true;
	}
	*pPosition = i;
	*pConsumed = bConsumed;
	return true;
}



/* 消费一个支持嵌套和 quoted-pair 的注释。 */
static bool __xrtMailAddressComment(xstrview Text, size_t* pPosition)
{
	size_t i = *pPosition;
	size_t iDepth = 0;

	if ( (i >= Text.Size) || (Text.Data[i] != '(') ) {
		return false;
	}
	while ( i < Text.Size ) {
		unsigned char iByte = (unsigned char)Text.Data[i++];

		if ( iByte == (unsigned char)'(' ) {
			if ( ++iDepth > XMAIL_ADDRESS_COMMENT_DEPTH ) {
				return __xrtMailAddressError(
					"mail address comment nesting is too deep"
				);
			}
			continue;
		}
		if ( iByte == (unsigned char)')' ) {
			if ( --iDepth == 0 ) {
				*pPosition = i;
				return true;
			}
			continue;
		}
		if ( iByte == (unsigned char)'\\' ) {
			if ( i >= Text.Size ) {
				return __xrtMailAddressError("mail address has a dangling escape");
			}
			iByte = (unsigned char)Text.Data[i++];
			if ( (iByte < 32u) || (iByte == 127u) ) {
				return __xrtMailAddressError("mail address has an invalid escape");
			}
			continue;
		}
		if ( iByte == (unsigned char)'\r' ) {
			size_t iFold = i - 1u;
			bool bConsumed;

			if ( !__xrtMailAddressFws(Text, &iFold, &bConsumed) ) {
				return false;
			}
			i = iFold;
			continue;
		}
		if ( (iByte == (unsigned char)'\n') || (iByte == 0) ||
			 ((iByte < 32u) && (iByte != (unsigned char)'\t')) ||
			 (iByte == 127u) ) {
			return __xrtMailAddressError("mail address comment has a control byte");
		}
	}
	return __xrtMailAddressError("mail address has an unterminated comment");
}



/* 跳过连续 CFWS。 */
static bool __xrtMailAddressCfws(xstrview Text, size_t* pPosition)
{
	size_t i = *pPosition;

	while ( i < Text.Size ) {
		bool bSpace;

		if ( !__xrtMailAddressFws(Text, &i, &bSpace) ) {
			return false;
		}
		if ( i < Text.Size && (Text.Data[i] == '(') ) {
			if ( !__xrtMailAddressComment(Text, &i) ) {
				return false;
			}
			continue;
		}
		if ( !bSpace ) {
			break;
		}
	}
	*pPosition = i;
	return true;
}



/* 返回一个 quoted-string 结束位置。 */
static bool __xrtMailAddressQuoted(
	xstrview Text,
	size_t iStart,
	bool bUtf8,
	size_t* pEnd
)
{
	for ( size_t i = iStart + 1u; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( iByte == (unsigned char)'"' ) {
			*pEnd = i + 1u;
			return true;
		}
		if ( iByte == (unsigned char)'\\' ) {
			if ( ++i >= Text.Size ) {
				break;
			}
			iByte = (unsigned char)Text.Data[i];
		}
		if ( (iByte == 0) || (iByte == 127u) ||
			 ((iByte < 32u) && (iByte != (unsigned char)' ') &&
			  (iByte != (unsigned char)'\t')) ||
			 (!bUtf8 && (iByte >= 128u)) ) {
			return __xrtMailAddressError("mail address has invalid quoted text");
		}
	}
	return __xrtMailAddressError("mail address has an unterminated quote");
}



/* 裁掉外层 CFWS，同时保留内部语法的原始切片。 */
static bool __xrtMailAddressTrim(xstrview Text, xstrview* pTrimmed)
{
	size_t i = 0;
	size_t iStart;
	size_t iEnd;
	bool bDomain = false;
	bool bEscape = false;

	if ( !__xrtMailAddressCfws(Text, &i) ) {
		return false;
	}
	iStart = i;
	iEnd = i;
	while ( i < Text.Size ) {
		size_t iSpace = i;
		unsigned char iByte;

		if ( bDomain ) {
			iByte = (unsigned char)Text.Data[i++];
			if ( bEscape ) {
				bEscape = false;
			} else if ( iByte == (unsigned char)'\\' ) {
				bEscape = true;
			} else if ( iByte == (unsigned char)']' ) {
				bDomain = false;
			}
			if ( (iByte == 0) || (iByte == 127u) || (iByte < 32u) ) {
				return __xrtMailAddressError(
					"mail domain literal contains a control byte"
				);
			}
			iEnd = i;
			continue;
		}

		if ( !__xrtMailAddressCfws(Text, &iSpace) ) {
			return false;
		}
		if ( iSpace != i ) {
			i = iSpace;
			continue;
		}
		if ( Text.Data[i] == '"' ) {
			if ( !__xrtMailAddressQuoted(Text, i, true, &i) ) {
				return false;
			}
		} else {
			iByte = (unsigned char)Text.Data[i++];

			if ( (iByte == 0) || (iByte == 127u) ||
				 ((iByte < 32u) && (iByte != (unsigned char)'\t')) ) {
				return __xrtMailAddressError("mail address contains a control byte");
			}
			if ( iByte == (unsigned char)'[' ) {
				bDomain = true;
			}
		}
		iEnd = i;
	}
	if ( bDomain || bEscape ) {
		return __xrtMailAddressError("mail address has an unterminated domain literal");
	}
	*pTrimmed = __xrtMailView(Text.Data + iStart, iEnd - iStart);
	return true;
}



/* 扫描当前列表项的顶层分隔符并验证括号结构。 */
static bool __xrtMailAddressDelimiter(
	xstrview Text,
	size_t iStart,
	size_t* pPosition,
	__xmailaddressdelimiter* pDelimiter
)
{
	size_t i = iStart;
	size_t iAngle = 0;
	bool bQuoted = false;
	bool bDomain = false;
	bool bEscape = false;

	while ( i < Text.Size ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( bEscape ) {
			bEscape = false;
			i++;
			continue;
		}
		if ( (bQuoted || bDomain) && (iByte == (unsigned char)'\\') ) {
			bEscape = true;
			i++;
			continue;
		}
		if ( bQuoted ) {
			if ( iByte == (unsigned char)'"' ) {
				bQuoted = false;
			}
			i++;
			continue;
		}
		if ( bDomain ) {
			if ( iByte == (unsigned char)']' ) {
				bDomain = false;
			}
			i++;
			continue;
		}
		if ( iByte == (unsigned char)'(' ) {
			if ( !__xrtMailAddressComment(Text, &i) ) {
				return false;
			}
			continue;
		}
		if ( iByte == (unsigned char)'"' ) {
			bQuoted = true;
			i++;
			continue;
		}
		if ( iByte == (unsigned char)'[' ) {
			bDomain = true;
			i++;
			continue;
		}
		if ( iByte == (unsigned char)'<' ) {
			if ( iAngle != 0 ) {
				return __xrtMailAddressError("mail address has nested angle brackets");
			}
			iAngle = 1u;
			i++;
			continue;
		}
		if ( iByte == (unsigned char)'>' ) {
			if ( iAngle == 0 ) {
				return __xrtMailAddressError("mail address has an unmatched angle bracket");
			}
			iAngle = 0;
			i++;
			continue;
		}
		if ( iAngle == 0 ) {
			if ( iByte == (unsigned char)',' ) {
				*pPosition = i;
				*pDelimiter = __XMAIL_ADDRESS_COMMA;
				return true;
			}
			if ( iByte == (unsigned char)':' ) {
				*pPosition = i;
				*pDelimiter = __XMAIL_ADDRESS_COLON;
				return true;
			}
			if ( iByte == (unsigned char)';' ) {
				*pPosition = i;
				*pDelimiter = __XMAIL_ADDRESS_SEMICOLON;
				return true;
			}
		}
		if ( iByte == (unsigned char)'\r' ) {
			bool bConsumed;

			if ( !__xrtMailAddressFws(Text, &i, &bConsumed) ) {
				return false;
			}
			continue;
		}
		if ( (iByte == (unsigned char)'\n') || (iByte == 0) ||
			 ((iByte < 32u) && (iByte != (unsigned char)' ') &&
			  (iByte != (unsigned char)'\t')) || (iByte == 127u) ) {
			return __xrtMailAddressError("mail address contains a control byte");
		}
		i++;
	}
	if ( bEscape || bQuoted || bDomain || (iAngle != 0) ) {
		return __xrtMailAddressError("mail address has an unterminated construct");
	}
	*pPosition = Text.Size;
	*pDelimiter = __XMAIL_ADDRESS_END;
	return true;
}



/* 验证显示名 phrase，允许 encoded-word、quoted-string、注释与 UTF-8。 */
static bool __xrtMailAddressPhrase(xstrview Name)
{
	size_t i = 0;

	while ( i < Name.Size ) {
		xmailwordview Word;
		xmailnext Next;

		if ( !__xrtMailAddressCfws(Name, &i) ) {
			return false;
		}
		if ( i == Name.Size ) {
			break;
		}
		if ( Name.Data[i] == '"' ) {
			if ( !__xrtMailAddressQuoted(Name, i, true, &i) ) {
				return false;
			}
			continue;
		}
		if ( (Name.Data[i] == '=') && ((i + 1u) < Name.Size) &&
			 (Name.Data[i + 1u] == '?') ) {
			Next = xrtMailWordParse(
				__xrtMailView(Name.Data + i, Name.Size - i),
				&Word
			);
			if ( Next != XMAIL_NEXT_ITEM ) {
				return false;
			}
			i += Word.Source.Size;
			continue;
		}
		{
			unsigned char iByte = (unsigned char)Name.Data[i];

			if ( (iByte < 128u) && !__xrtMailAtext(iByte) &&
				 (iByte != (unsigned char)'.') ) {
				return __xrtMailAddressError("mail display name has an invalid token");
			}
			i++;
		}
	}
	return true;
}



/* 验证 local-part。 */
static bool __xrtMailAddressLocal(xstrview Local, uint32 iFlags)
{
	bool bSmtpUtf8 = (iFlags & (uint32)XMAIL_ADDRESS_SMTPUTF8) != 0;
	bool bDot = true;

	if ( Local.Size == 0 ) {
		return __xrtMailAddressError("mail address has an empty local-part");
	}
	if ( Local.Data[0] == '"' ) {
		size_t iEnd;

		return __xrtMailAddressQuoted(Local, 0, bSmtpUtf8, &iEnd) &&
			(iEnd == Local.Size);
	}
	for ( size_t i = 0; i < Local.Size; i++ ) {
		unsigned char iByte = (unsigned char)Local.Data[i];

		if ( iByte == (unsigned char)'.' ) {
			if ( bDot ) {
				return __xrtMailAddressError("mail local-part has an empty atom");
			}
			bDot = true;
			continue;
		}
		if ( (iByte < 128u) ? !__xrtMailAtext(iByte) : !bSmtpUtf8 ) {
			return __xrtMailAddressError("mail local-part has an invalid byte");
		}
		bDot = false;
	}
	return !bDot || __xrtMailAddressError("mail local-part ends with a dot");
}



/* 验证 domain 或 domain-literal。 */
static bool __xrtMailAddressDomain(xstrview Domain, uint32 iFlags)
{
	bool bSmtpUtf8 = (iFlags & (uint32)XMAIL_ADDRESS_SMTPUTF8) != 0;
	bool bLabelStart = true;
	bool bLastHyphen = false;

	if ( Domain.Size == 0 ) {
		return __xrtMailAddressError("mail address has an empty domain");
	}
	if ( Domain.Data[0] == '[' ) {
		bool bEscape = false;

		if ( (Domain.Size < 3u) || (Domain.Data[Domain.Size - 1u] != ']') ) {
			return __xrtMailAddressError("mail address has an invalid domain literal");
		}
		for ( size_t i = 1u; (i + 1u) < Domain.Size; i++ ) {
			unsigned char iByte = (unsigned char)Domain.Data[i];

			if ( bEscape ) {
				bEscape = false;
			} else if ( iByte == (unsigned char)'\\' ) {
				bEscape = true;
				continue;
			} else if ( (iByte == (unsigned char)'[') ||
				 (iByte == (unsigned char)']') ) {
				return __xrtMailAddressError("mail domain literal has an invalid byte");
			}
			if ( (iByte < 33u) || (iByte > 126u) ) {
				return __xrtMailAddressError("mail domain literal has a control byte");
			}
		}
		return !bEscape || __xrtMailAddressError(
			"mail domain literal has a dangling escape"
		);
	}
	for ( size_t i = 0; i < Domain.Size; i++ ) {
		unsigned char iByte = (unsigned char)Domain.Data[i];

		if ( iByte == (unsigned char)'.' ) {
			if ( bLabelStart || bLastHyphen ) {
				return __xrtMailAddressError("mail domain has an invalid label");
			}
			bLabelStart = true;
			bLastHyphen = false;
			continue;
		}
		if ( iByte < 128u ) {
			bool bAlphaNumeric =
				((iByte >= (unsigned char)'A') && (iByte <= (unsigned char)'Z')) ||
				((iByte >= (unsigned char)'a') && (iByte <= (unsigned char)'z')) ||
				((iByte >= (unsigned char)'0') && (iByte <= (unsigned char)'9'));

			if ( !bAlphaNumeric && (iByte != (unsigned char)'-') ) {
				return __xrtMailAddressError("mail domain has an invalid byte");
			}
			if ( bLabelStart && (iByte == (unsigned char)'-') ) {
				return __xrtMailAddressError("mail domain label starts with a hyphen");
			}
			bLastHyphen = iByte == (unsigned char)'-';
		} else {
			if ( !bSmtpUtf8 ) {
				return __xrtMailAddressError("mail domain requires SMTPUTF8");
			}
			bLastHyphen = false;
		}
		bLabelStart = false;
	}
	return (!bLabelStart && !bLastHyphen) || __xrtMailAddressError(
		"mail domain ends with an invalid label"
	);
}



/* 拆分并验证完整 addr-spec。 */
static bool __xrtMailAddressSpec(
	xstrview Address,
	uint32 iFlags,
	xstrview* pAddress,
	xstrview* pLocal,
	xstrview* pDomain
)
{
	xstrview Trimmed;
	xstrview Local;
	xstrview Domain;
	size_t iAt = XRT_NPOS;
	size_t i = 0;
	bool bQuoted = false;
	bool bDomain = false;
	bool bEscape = false;

	if ( !__xrtMailAddressTrim(Address, &Trimmed) || (Trimmed.Size == 0) ) {
		return __xrtMailAddressError("mail address is empty");
	}
	while ( i < Trimmed.Size ) {
		unsigned char iByte = (unsigned char)Trimmed.Data[i];

		if ( bEscape ) {
			bEscape = false;
			i++;
			continue;
		}
		if ( (bQuoted || bDomain) && (iByte == (unsigned char)'\\') ) {
			bEscape = true;
			i++;
			continue;
		}
		if ( bQuoted ) {
			bQuoted = iByte != (unsigned char)'"';
			i++;
			continue;
		}
		if ( bDomain ) {
			bDomain = iByte != (unsigned char)']';
			i++;
			continue;
		}
		if ( iByte == (unsigned char)'(' ) {
			if ( !__xrtMailAddressComment(Trimmed, &i) ) {
				return false;
			}
			continue;
		}
		if ( iByte == (unsigned char)'"' ) {
			bQuoted = true;
		} else if ( iByte == (unsigned char)'[' ) {
			bDomain = true;
		} else if ( iByte == (unsigned char)'@' ) {
			if ( iAt != XRT_NPOS ) {
				return __xrtMailAddressError("mail address has multiple at signs");
			}
			iAt = i;
		}
		i++;
	}
	if ( bEscape || bQuoted || bDomain || (iAt == XRT_NPOS) ) {
		return __xrtMailAddressError("mail address has an invalid addr-spec");
	}
	if ( !__xrtMailAddressTrim(
		__xrtMailView(Trimmed.Data, iAt),
		&Local
	) || !__xrtMailAddressTrim(
		__xrtMailView(
			Trimmed.Data + iAt + 1u,
			Trimmed.Size - iAt - 1u
		),
		&Domain
	) || !__xrtMailAddressLocal(Local, iFlags) ||
		 !__xrtMailAddressDomain(Domain, iFlags) ) {
		return false;
	}
	*pAddress = Trimmed;
	*pLocal = Local;
	*pDomain = Domain;
	return true;
}



/* 从一个完整列表项提取 mailbox 视图。 */
static bool __xrtMailAddressMailbox(
	xstrview Segment,
	uint32 iFlags,
	xmailaddressview* pAddress
)
{
	xstrview Source;
	xstrview Name = { NULL, 0 };
	xstrview Address;
	xstrview Local;
	xstrview Domain;
	size_t iLeft = XRT_NPOS;
	size_t iRight = XRT_NPOS;
	size_t i = 0;
	bool bQuoted = false;
	bool bDomain = false;
	bool bEscape = false;
	xmailaddressview Result;

	if ( !__xrtMailAddressTrim(Segment, &Source) || (Source.Size == 0) ) {
		return __xrtMailAddressError("mail address list contains an empty item");
	}
	while ( i < Source.Size ) {
		unsigned char iByte = (unsigned char)Source.Data[i];

		if ( bEscape ) {
			bEscape = false;
			i++;
			continue;
		}
		if ( (bQuoted || bDomain) && (iByte == (unsigned char)'\\') ) {
			bEscape = true;
			i++;
			continue;
		}
		if ( bQuoted ) {
			bQuoted = iByte != (unsigned char)'"';
			i++;
			continue;
		}
		if ( bDomain ) {
			bDomain = iByte != (unsigned char)']';
			i++;
			continue;
		}
		if ( iByte == (unsigned char)'(' ) {
			if ( !__xrtMailAddressComment(Source, &i) ) {
				return false;
			}
			continue;
		}
		if ( iByte == (unsigned char)'"' ) {
			bQuoted = true;
		} else if ( iByte == (unsigned char)'[' ) {
			bDomain = true;
		} else if ( iByte == (unsigned char)'<' ) {
			if ( iLeft != XRT_NPOS ) {
				return __xrtMailAddressError("mailbox has multiple angle addresses");
			}
			iLeft = i;
		} else if ( iByte == (unsigned char)'>' ) {
			if ( (iLeft == XRT_NPOS) || (iRight != XRT_NPOS) ) {
				return __xrtMailAddressError("mailbox has an unmatched angle bracket");
			}
			iRight = i;
		}
		i++;
	}
	if ( bEscape || bQuoted || bDomain ||
		 ((iLeft == XRT_NPOS) != (iRight == XRT_NPOS)) ) {
		return __xrtMailAddressError("mailbox has an unterminated construct");
	}
	if ( iLeft != XRT_NPOS ) {
		xstrview Tail;

		if ( !__xrtMailAddressTrim(
			__xrtMailView(Source.Data, iLeft),
			&Name
		) || !__xrtMailAddressTrim(
			__xrtMailView(
				Source.Data + iLeft + 1u,
				iRight - iLeft - 1u
			),
			&Address
		) || !__xrtMailAddressTrim(
			__xrtMailView(
				Source.Data + iRight + 1u,
				Source.Size - iRight - 1u
			),
			&Tail
		) || (Tail.Size != 0) ) {
			return __xrtMailAddressError("mailbox has text after its angle address");
		}
		if ( (Name.Size != 0) && !__xrtMailAddressPhrase(Name) ) {
			return false;
		}
	} else {
		Address = Source;
	}
	if ( !__xrtMailAddressSpec(
		Address,
		iFlags,
		&Address,
		&Local,
		&Domain
	) ) {
		return false;
	}
	Result.Kind = XMAIL_ADDRESS_MAILBOX;
	Result.Source = Source;
	Result.Name = Name;
	Result.Address = Address;
	Result.Local = Local;
	Result.Domain = Domain;
	*pAddress = Result;
	return true;
}



/* 初始化地址列表游标。 */
XRT_API bool xrtMailAddressCursorInit(
	xmailaddresscursor* pCursor,
	xstrview Text,
	uint32 iFlags
)
{
	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		 !__xrtMailViewValid(Text) ||
		 (iFlags & ~(uint32)XMAIL_ADDRESS_SMTPUTF8) ||
		 xrtMemRangesOverlap(pCursor, sizeof(*pCursor), Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtUtf8Valid(Text, NULL) ) {
		__xrtMailError(
			XERR_VALUE,
			XMAIL_ERROR_CHARSET,
			"mail address list is not valid UTF-8"
		);
		return false;
	}
	pCursor->Text = Text;
	pCursor->Position = 0;
	pCursor->Flags = iFlags;
	pCursor->InGroup = false;
	pCursor->Done = false;
	return true;
}



/* 处理 group 结束符并验证其后的顶层分隔。 */
static bool __xrtMailAddressGroupEnd(
	xmailaddresscursor* pCursor,
	size_t iPosition,
	xmailaddressview* pAddress
)
{
	xmailaddressview Result = { 0 };
	size_t iNext = iPosition + 1u;

	Result.Kind = XMAIL_ADDRESS_GROUP_END;
	Result.Source = __xrtMailView(pCursor->Text.Data + iPosition, 1u);
	if ( !__xrtMailAddressCfws(pCursor->Text, &iNext) ) {
		return false;
	}
	if ( iNext < pCursor->Text.Size ) {
		if ( pCursor->Text.Data[iNext] != ',' ) {
			return __xrtMailAddressError("mail group is not followed by a comma");
		}
		iNext++;
		if ( !__xrtMailAddressCfws(pCursor->Text, &iNext) ) {
			return false;
		}
		if ( iNext == pCursor->Text.Size ) {
			return __xrtMailAddressError("mail address list has a trailing comma");
		}
	}
	pCursor->Position = iNext;
	pCursor->InGroup = false;
	*pAddress = Result;
	return true;
}



/* 返回地址列表的下一个结构项。 */
XRT_API xmailnext xrtMailAddressNext(
	xmailaddresscursor* pCursor,
	xmailaddressview* pAddress
)
{
	xmailaddresscursor Cursor;
	xmailaddressview Result = { 0 };
	size_t iStart;
	size_t iDelimiter;
	__xmailaddressdelimiter Delimiter;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		 !xrtMemRangeValid(pAddress, sizeof(*pAddress)) ||
		 !__xrtMailViewValid(pCursor != NULL ? pCursor->Text :
			__xrtMailView(NULL, 0)) ||
		 (pCursor->Position > pCursor->Text.Size) ||
		 (pCursor->Flags & ~(uint32)XMAIL_ADDRESS_SMTPUTF8) ||
		 xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pAddress,
			sizeof(*pAddress)) ||
		 xrtMemRangesOverlap(pAddress, sizeof(*pAddress),
			pCursor->Text.Data, pCursor->Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( pCursor->Done ) {
		return XMAIL_NEXT_END;
	}
	Cursor = *pCursor;
	if ( !__xrtMailAddressCfws(Cursor.Text, &Cursor.Position) ) {
		return XMAIL_NEXT_ERROR;
	}
	if ( Cursor.Position == Cursor.Text.Size ) {
		if ( Cursor.InGroup ) {
			(void)__xrtMailAddressError("mail address group has no semicolon");
			return XMAIL_NEXT_ERROR;
		}
		Cursor.Done = true;
		*pCursor = Cursor;
		return XMAIL_NEXT_END;
	}
	if ( Cursor.InGroup && (Cursor.Text.Data[Cursor.Position] == ';') ) {
		if ( !__xrtMailAddressGroupEnd(
			&Cursor,
			Cursor.Position,
			&Result
		) ) {
			return XMAIL_NEXT_ERROR;
		}
		*pCursor = Cursor;
		*pAddress = Result;
		return XMAIL_NEXT_ITEM;
	}
	iStart = Cursor.Position;
	if ( !__xrtMailAddressDelimiter(
		Cursor.Text,
		iStart,
		&iDelimiter,
		&Delimiter
	) ) {
		return XMAIL_NEXT_ERROR;
	}
	if ( Delimiter == __XMAIL_ADDRESS_COLON ) {
		xstrview Name;

		if ( Cursor.InGroup ) {
			(void)__xrtMailAddressError("nested mail address groups are not allowed");
			return XMAIL_NEXT_ERROR;
		}
		if ( !__xrtMailAddressTrim(
			__xrtMailView(Cursor.Text.Data + iStart, iDelimiter - iStart),
			&Name
		) || (Name.Size == 0) || !__xrtMailAddressPhrase(Name) ) {
			(void)__xrtMailAddressError("mail address group has an invalid name");
			return XMAIL_NEXT_ERROR;
		}
		Result.Kind = XMAIL_ADDRESS_GROUP_BEGIN;
		Result.Source = __xrtMailView(
			Cursor.Text.Data + iStart,
			iDelimiter - iStart + 1u
		);
		Result.Name = Name;
		Cursor.Position = iDelimiter + 1u;
		Cursor.InGroup = true;
		*pCursor = Cursor;
		*pAddress = Result;
		return XMAIL_NEXT_ITEM;
	}
	if ( Delimiter == __XMAIL_ADDRESS_SEMICOLON ) {
		xstrview Item;

		if ( !Cursor.InGroup ) {
			(void)__xrtMailAddressError("mail address list has an unmatched semicolon");
			return XMAIL_NEXT_ERROR;
		}
		if ( !__xrtMailAddressTrim(
			__xrtMailView(Cursor.Text.Data + iStart, iDelimiter - iStart),
			&Item
		) ) {
			return XMAIL_NEXT_ERROR;
		}
		if ( Item.Size == 0 ) {
			if ( !__xrtMailAddressGroupEnd(
				&Cursor,
				iDelimiter,
				&Result
			) ) {
				return XMAIL_NEXT_ERROR;
			}
			*pCursor = Cursor;
			*pAddress = Result;
			return XMAIL_NEXT_ITEM;
		}
		if ( !__xrtMailAddressMailbox(Item, Cursor.Flags, &Result) ) {
			return XMAIL_NEXT_ERROR;
		}
		Cursor.Position = iDelimiter;
		*pCursor = Cursor;
		*pAddress = Result;
		return XMAIL_NEXT_ITEM;
	}
	if ( (Delimiter == __XMAIL_ADDRESS_END) && Cursor.InGroup ) {
		(void)__xrtMailAddressError("mail address group has no semicolon");
		return XMAIL_NEXT_ERROR;
	}
	if ( !__xrtMailAddressMailbox(
		__xrtMailView(Cursor.Text.Data + iStart, iDelimiter - iStart),
		Cursor.Flags,
		&Result
	) ) {
		return XMAIL_NEXT_ERROR;
	}
	if ( Delimiter == __XMAIL_ADDRESS_COMMA ) {
		Cursor.Position = iDelimiter + 1u;
		if ( !__xrtMailAddressCfws(Cursor.Text, &Cursor.Position) ) {
			return XMAIL_NEXT_ERROR;
		}
		if ( (Cursor.Position == Cursor.Text.Size) ||
			 (Cursor.Text.Data[Cursor.Position] == ',') ||
			 (Cursor.Text.Data[Cursor.Position] == ';') ) {
			(void)__xrtMailAddressError("mail address list has an empty item");
			return XMAIL_NEXT_ERROR;
		}
	} else {
		Cursor.Position = iDelimiter;
		Cursor.Done = true;
	}
	*pCursor = Cursor;
	*pAddress = Result;
	return XMAIL_NEXT_ITEM;
}



/* 验证并拆分一个 addr-spec。 */
XRT_API bool xrtMailAddressValid(
	xstrview Address,
	uint32 iFlags,
	xstrview* pLocal,
	xstrview* pDomain
)
{
	xstrview Trimmed;
	xstrview Local;
	xstrview Domain;

	if ( !__xrtMailViewValid(Address) ||
		 !xrtMemRangeValid(pLocal, pLocal != NULL ? sizeof(*pLocal) : 0) ||
		 !xrtMemRangeValid(pDomain, pDomain != NULL ? sizeof(*pDomain) : 0) ||
		 (iFlags & ~(uint32)XMAIL_ADDRESS_SMTPUTF8) ||
		 ((pLocal != NULL) && xrtMemRangesOverlap(
			pLocal, sizeof(*pLocal), Address.Data, Address.Size
		 )) || ((pDomain != NULL) && xrtMemRangesOverlap(
			pDomain, sizeof(*pDomain), Address.Data, Address.Size
		 )) || ((pLocal != NULL) && (pDomain != NULL) &&
			xrtMemRangesOverlap(pLocal, sizeof(*pLocal),
				pDomain, sizeof(*pDomain))) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtUtf8Valid(Address, NULL) ) {
		__xrtMailError(
			XERR_VALUE,
			XMAIL_ERROR_CHARSET,
			"mail address is not valid UTF-8"
		);
		return false;
	}
	if ( !__xrtMailAddressSpec(
		Address,
		iFlags,
		&Trimmed,
		&Local,
		&Domain
	) ) {
		return false;
	}
	if ( pLocal != NULL ) {
		*pLocal = Local;
	}
	if ( pDomain != NULL ) {
		*pDomain = Domain;
	}
	return true;
}



/* 选择显示名的最小合法表示。 */
static bool __xrtMailAddressNameMode(
	xstrview Name,
	__xmailaddressname* pMode,
	size_t* pQuotedSize
)
{
	bool bQuoted = false;
	bool bWord = false;
	size_t iQuoted = 2u;

	if ( !xrtUtf8Valid(Name, NULL) ) {
		__xrtMailError(
			XERR_VALUE,
			XMAIL_ERROR_CHARSET,
			"mail display name is not valid UTF-8"
		);
		return false;
	}
	for ( size_t i = 0; i < Name.Size; i++ ) {
		unsigned char iByte = (unsigned char)Name.Data[i];

		if ( (iByte == 0) || (iByte == 127u) || (iByte < 32u) ) {
			return __xrtMailAddressError("mail display name has a control byte");
		}
		if ( (iByte >= 128u) || ((iByte == (unsigned char)'=') &&
			 ((i + 1u) < Name.Size) && (Name.Data[i + 1u] == '?')) ) {
			bWord = true;
		}
		if ( (iByte < 128u) && (iByte != (unsigned char)' ') &&
			 !__xrtMailAtext(iByte) && (iByte != (unsigned char)'.') ) {
			bQuoted = true;
		}
		if ( (iByte == (unsigned char)'"') ||
			 (iByte == (unsigned char)'\\') ) {
			if ( !__xrtMailSizeAdd(iQuoted, 1u, &iQuoted) ) {
				return false;
			}
		}
		if ( !__xrtMailSizeAdd(iQuoted, 1u, &iQuoted) ) {
			return false;
		}
	}
	if ( (Name.Size != 0) && ((Name.Data[0] == ' ') ||
		 (Name.Data[Name.Size - 1u] == ' ')) ) {
		bQuoted = true;
	}
	*pMode = bWord ? __XMAIL_ADDRESS_NAME_WORD :
		(bQuoted ? __XMAIL_ADDRESS_NAME_QUOTED : __XMAIL_ADDRESS_NAME_RAW);
	*pQuotedSize = iQuoted;
	return true;
}



/* 写出规范 mailbox 文本。 */
XRT_API bool xrtMailAddressWrite(
	xstrview Name,
	xstrview Address,
	xmailwordencoding Encoding,
	uint32 iFlags,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	xstrview Trimmed;
	xstrview Local;
	xstrview Domain;
	__xmailaddressname NameMode;
	size_t iQuotedSize;
	size_t iNameSize = 0;
	size_t iAddressSize;
	size_t iRequired;

	if ( !__xrtMailViewValid(Name) || !__xrtMailViewValid(Address) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 ((Encoding != XMAIL_WORD_BASE64) && (Encoding != XMAIL_WORD_Q)) ||
		 (iFlags & ~(uint32)XMAIL_ADDRESS_SMTPUTF8) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtUtf8Valid(Address, NULL) || !__xrtMailAddressSpec(
		Address,
		iFlags,
		&Trimmed,
		&Local,
		&Domain
	) || !__xrtMailAddressNameMode(Name, &NameMode, &iQuotedSize) ) {
		return false;
	}
	if ( Name.Size != 0 ) {
		if ( NameMode == __XMAIL_ADDRESS_NAME_WORD ) {
			if ( !xrtMailWordEncodeWrite(
				Name,
				Encoding,
				NULL,
				0,
				&iNameSize
			) ) {
				return false;
			}
		} else {
			iNameSize = NameMode == __XMAIL_ADDRESS_NAME_QUOTED ?
				iQuotedSize : Name.Size;
		}
	}
	if ( !__xrtMailSizeAdd(Local.Size, 1u, &iAddressSize) ||
		 !__xrtMailSizeAdd(iAddressSize, Domain.Size, &iAddressSize) ) {
		return false;
	}
	iRequired = iAddressSize;
	if ( Name.Size != 0 ) {
		if ( !__xrtMailSizeAdd(iNameSize, 2u, &iRequired) ||
			 !__xrtMailSizeAdd(iRequired, iAddressSize, &iRequired) ||
			 !__xrtMailSizeAdd(iRequired, 1u, &iRequired) ) {
			return false;
		}
	}
	if ( xrtMemRangesOverlap(
		pOutputSize, sizeof(*pOutputSize), Name.Data, Name.Size
	) || xrtMemRangesOverlap(
		pOutputSize, sizeof(*pOutputSize), Address.Data, Address.Size
	) || ((sOutput != NULL) && xrtMemRangesOverlap(
		pOutputSize, sizeof(*pOutputSize), sOutput, iCapacity
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
	if ( xrtMemRangesOverlap(
		sOutput, iRequired + 1u, Name.Data, Name.Size
	) || xrtMemRangesOverlap(
		sOutput, iRequired + 1u, Address.Data, Address.Size
	) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	{
		size_t iOutput = 0;

		if ( Name.Size != 0 ) {
			if ( NameMode == __XMAIL_ADDRESS_NAME_WORD ) {
				(void)xrtMailWordEncodeWrite(
					Name,
					Encoding,
					sOutput,
					iNameSize + 1u,
					&iOutput
				);
			} else if ( NameMode == __XMAIL_ADDRESS_NAME_QUOTED ) {
				sOutput[iOutput++] = '"';
				for ( size_t i = 0; i < Name.Size; i++ ) {
					if ( (Name.Data[i] == '"') || (Name.Data[i] == '\\') ) {
						sOutput[iOutput++] = '\\';
					}
					sOutput[iOutput++] = Name.Data[i];
				}
				sOutput[iOutput++] = '"';
			} else {
				memcpy(sOutput, Name.Data, Name.Size);
				iOutput = Name.Size;
			}
			sOutput[iOutput++] = ' ';
			sOutput[iOutput++] = '<';
		}
		memcpy(sOutput + iOutput, Local.Data, Local.Size);
		iOutput += Local.Size;
		sOutput[iOutput++] = '@';
		memcpy(sOutput + iOutput, Domain.Data, Domain.Size);
		iOutput += Domain.Size;
		if ( Name.Size != 0 ) {
			sOutput[iOutput++] = '>';
		}
		sOutput[iOutput] = 0;
	}
	*pOutputSize = iRequired;
	return true;
}



/* 创建独立的规范 mailbox 文本。 */
XRT_API str xrtMailAddress(
	xstrview Name,
	xstrview Address,
	xmailwordencoding Encoding,
	uint32 iFlags,
	size_t* pOutputSize
)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailAddressWrite(
		Name,
		Address,
		Encoding,
		iFlags,
		NULL,
		0,
		&iRequired
	) ) {
		return NULL;
	}
	if ( (pOutputSize != NULL) && (xrtMemRangesOverlap(
		pOutputSize, sizeof(*pOutputSize), Name.Data, Name.Size
	) || xrtMemRangesOverlap(
		pOutputSize, sizeof(*pOutputSize), Address.Data, Address.Size
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
	if ( !xrtMailAddressWrite(
		Name,
		Address,
		Encoding,
		iFlags,
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



/* 验证地址数组、输出范围和所有借用视图都彼此安全。 */
static bool __xrtMailAddressListRanges(
	const xmailaddress* pAddresses,
	size_t iCount,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iArraySize;

	if ( (iCount > (SIZE_MAX / sizeof(*pAddresses))) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	iArraySize = iCount * sizeof(*pAddresses);
	if ( !xrtMemRangeValid(pAddresses, iArraySize) ||
		 xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			pAddresses,
			iArraySize
		 ) || ((sOutput != NULL) && (xrtMemRangesOverlap(
			sOutput,
			iCapacity,
			pAddresses,
			iArraySize
		 ) || xrtMemRangesOverlap(
			sOutput,
			iCapacity,
			pOutputSize,
			sizeof(*pOutputSize)
		 ))) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return true;
}



/* 计量全部 mailbox，并在写入前完成视图与别名检查。 */
static bool __xrtMailAddressListMeasure(
	const xmailaddress* pAddresses,
	size_t iCount,
	xmailwordencoding Encoding,
	uint32 iFlags,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	size_t* pRequired
)
{
	size_t iRequired = 0;

	if ( !__xrtMailAddressListRanges(
		pAddresses,
		iCount,
		sOutput,
		iCapacity,
		pOutputSize
	) ) {
		return false;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		size_t iAddressSize;

		if ( !__xrtMailViewValid(pAddresses[i].Name) ||
			 !__xrtMailViewValid(pAddresses[i].Address) ||
			 xrtMemRangesOverlap(
				pOutputSize,
				sizeof(*pOutputSize),
				pAddresses[i].Name.Data,
				pAddresses[i].Name.Size
			 ) || xrtMemRangesOverlap(
				pOutputSize,
				sizeof(*pOutputSize),
				pAddresses[i].Address.Data,
				pAddresses[i].Address.Size
			 ) || ((sOutput != NULL) && (xrtMemRangesOverlap(
				sOutput,
				iCapacity,
				pAddresses[i].Name.Data,
				pAddresses[i].Name.Size
			 ) || xrtMemRangesOverlap(
				sOutput,
				iCapacity,
				pAddresses[i].Address.Data,
				pAddresses[i].Address.Size
			 ))) ) {
			__xrtMailSetInvalidArgument();
			return false;
		}
		if ( !xrtMailAddressWrite(
			pAddresses[i].Name,
			pAddresses[i].Address,
			Encoding,
			iFlags,
			NULL,
			0,
			&iAddressSize
		) || ((i != 0) && !__xrtMailSizeAdd(
			iRequired,
			2u,
			&iRequired
		)) || !__xrtMailSizeAdd(
			iRequired,
			iAddressSize,
			&iRequired
		) ) {
			return false;
		}
	}
	*pRequired = iRequired;
	return true;
}



/* 写出逗号分隔的规范 mailbox 列表。 */
XRT_API bool xrtMailAddressListWrite(
	const xmailaddress* pAddresses,
	size_t iCount,
	xmailwordencoding Encoding,
	uint32 iFlags,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;
	size_t iOutput = 0;

	if ( !__xrtMailAddressListMeasure(
		pAddresses,
		iCount,
		Encoding,
		iFlags,
		sOutput,
		iCapacity,
		pOutputSize,
		&iRequired
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
	for ( size_t i = 0; i < iCount; i++ ) {
		size_t iAddressSize;

		if ( i != 0 ) {
			sOutput[iOutput++] = ',';
			sOutput[iOutput++] = ' ';
		}
		(void)xrtMailAddressWrite(
			pAddresses[i].Name,
			pAddresses[i].Address,
			Encoding,
			iFlags,
			sOutput + iOutput,
			iCapacity - iOutput,
			&iAddressSize
		);
		iOutput += iAddressSize;
	}
	sOutput[iOutput] = 0;
	*pOutputSize = iRequired;
	return true;
}



/* 创建独立的规范 mailbox 列表。 */
XRT_API str xrtMailAddressList(
	const xmailaddress* pAddresses,
	size_t iCount,
	xmailwordencoding Encoding,
	uint32 iFlags,
	size_t* pOutputSize
)
{
	size_t iRequired = 0;
	size_t* pCheckedSize;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) ) {
		__xrtMailSetInvalidArgument();
		return NULL;
	}
	pCheckedSize = pOutputSize != NULL ? pOutputSize : &iRequired;
	if ( !__xrtMailAddressListMeasure(
		pAddresses,
		iCount,
		Encoding,
		iFlags,
		NULL,
		0,
		pCheckedSize,
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
	if ( !xrtMailAddressListWrite(
		pAddresses,
		iCount,
		Encoding,
		iFlags,
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
