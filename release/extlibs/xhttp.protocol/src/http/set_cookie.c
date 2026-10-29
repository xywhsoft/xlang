#include "../internal/xrt_cookie.h"



#if defined(XHTTP_FEATURE_SET_COOKIE)

/* 裁剪视图两端的 WSP。 */
static xstrview __xrtSetCookieTrim(xstrview Text)
{
	while ( (Text.Size != 0) &&
		((Text.Data[0] == ' ') || (Text.Data[0] == '\t')) ) {
		Text.Data++;
		Text.Size--;
	}
	while ( (Text.Size != 0) &&
		((Text.Data[Text.Size - 1u] == ' ') ||
		 (Text.Data[Text.Size - 1u] == '\t')) ) {
		Text.Size--;
	}
	return Text;
}



/* 判断 Set-Cookie 接收算法禁止的控制字节。 */
static bool __xrtSetCookieHasCtl(xstrview Text)
{
	size_t i;

	for ( i = 0; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( (iByte <= 0x08u) ||
			((iByte >= 0x0Au) && (iByte <= 0x1Fu)) ||
			(iByte == 0x7Fu) ) {
			return true;
		}
	}
	return false;
}



/* 判断文本是否按 ASCII 规则等于一个编译期属性名。 */
static bool __xrtSetCookieName(xstrview Name, cstr sName, size_t iSize)
{
	return __xhttpCookieAsciiEqual(Name, (xstrview){ sName, iSize });
}



/* 扫描一个原始属性段，空段由调用方决定是否忽略。 */
static void __xrtSetCookieAttributeSegment(
	xstrview Text,
	size_t iOffset,
	xcookieattribute* pAttribute,
	size_t* pNext
)
{
	size_t iEnd = iOffset;
	size_t iEqual;
	xstrview Segment;

	while ( (iEnd < Text.Size) && (Text.Data[iEnd] != ';') ) {
		iEnd++;
	}
	Segment = __xrtSetCookieTrim((xstrview){
		Text.Data + iOffset, iEnd - iOffset
	});
	iEqual = 0;
	while ( (iEqual < Segment.Size) &&
		(Segment.Data[iEqual] != '=') ) {
		iEqual++;
	}
	memset(pAttribute, 0, sizeof(*pAttribute));
	if ( iEqual < Segment.Size ) {
		pAttribute->Flags = XCOOKIE_ATTRIBUTE_HAS_VALUE;
		pAttribute->Name = __xrtSetCookieTrim((xstrview){
			Segment.Data, iEqual
		});
		pAttribute->Value = __xrtSetCookieTrim((xstrview){
			Segment.Data + iEqual + 1u,
			Segment.Size - iEqual - 1u
		});
	} else {
		pAttribute->Name = Segment;
	}
	*pNext = (iEnd < Text.Size) ? (iEnd + 1u) : iEnd;
}



/* 扫描 Set-Cookie 原始属性区并保留未知属性。 */
XRT_API xcookieattributenext xrtSetCookieAttributeNext(
	xstrview Text,
	size_t* pOffset,
	xcookieattribute* pAttribute
)
{
	xcookieattribute Attribute;
	size_t iOffset;
	size_t iNext;

	if ( (pOffset == NULL) || (pAttribute == NULL) ||
		!__xhttpCookieViewValid(Text) ||
		xrtMemRangesOverlap(pOffset, sizeof(*pOffset), Text.Data, Text.Size) ||
		xrtMemRangesOverlap(
			pAttribute, sizeof(*pAttribute), Text.Data, Text.Size
		) ) {
		__xhttpErrorSetInvalidArgument();
		return XCOOKIE_ATTRIBUTE_ERROR;
	}
	iOffset = *pOffset;
	if ( iOffset > Text.Size ) {
		__xhttpErrorSetRange();
		return XCOOKIE_ATTRIBUTE_ERROR;
	}
	while ( iOffset < Text.Size ) {
		size_t iBegin = iOffset;

		__xrtSetCookieAttributeSegment(
			Text, iOffset, &Attribute, &iNext
		);
		if ( __xrtSetCookieHasCtl((xstrview){
			Text.Data + iBegin, iNext - iBegin
		}) ) {
			__xhttpErrorSetValue();
			return XCOOKIE_ATTRIBUTE_ERROR;
		}
		iOffset = iNext;
		if ( (Attribute.Name.Size != 0) ||
			((Attribute.Flags & XCOOKIE_ATTRIBUTE_HAS_VALUE) != 0) ) {
			*pAttribute = Attribute;
			*pOffset = iOffset;
			return XCOOKIE_ATTRIBUTE_ITEM;
		}
	}
	*pOffset = iOffset;
	return XCOOKIE_ATTRIBUTE_END;
}



/* 判断一个字节是否为 cookie-date 分隔符。 */
static bool __xrtCookieDateDelimiter(unsigned char iByte)
{
	return (iByte == 0x09u) ||
		((iByte >= 0x20u) && (iByte <= 0x2Fu)) ||
		((iByte >= 0x3Bu) && (iByte <= 0x40u)) ||
		((iByte >= 0x5Bu) && (iByte <= 0x60u)) ||
		((iByte >= 0x7Bu) && (iByte <= 0x7Eu));
}



/* 解析 token 开头受限位数的十进制数，并返回已消耗位数。 */
static bool __xrtCookieDateDigits(
	xstrview Token,
	size_t iMinimum,
	size_t iMaximum,
	int* pValue,
	size_t* pDigits
)
{
	size_t i = 0;
	int iValue = 0;

	while ( (i < Token.Size) && (i < iMaximum) &&
		(Token.Data[i] >= '0') && (Token.Data[i] <= '9') ) {
		iValue = (iValue * 10) + (Token.Data[i] - '0');
		i++;
	}
	if ( (i < iMinimum) ||
		((i < Token.Size) && (Token.Data[i] >= '0') &&
		 (Token.Data[i] <= '9')) ) {
		return false;
	}
	*pValue = iValue;
	*pDigits = i;
	return true;
}



/* 按 cookie-date 的 hms-time 前缀规则解析时间 token。 */
static bool __xrtCookieDateTime(
	xstrview Token,
	int* pHour,
	int* pMinute,
	int* pSecond
)
{
	size_t iPosition = 0;
	size_t iDigits;
	xstrview Rest;

	if ( !__xrtCookieDateDigits(
		Token, 1, 2, pHour, &iDigits
	) ) {
		return false;
	}
	iPosition += iDigits;
	if ( (iPosition >= Token.Size) ||
		(Token.Data[iPosition++] != ':') ) {
		return false;
	}
	Rest = (xstrview){
		Token.Data + iPosition, Token.Size - iPosition
	};
	if ( !__xrtCookieDateDigits(
		Rest, 1, 2, pMinute, &iDigits
	) ) {
		return false;
	}
	iPosition += iDigits;
	if ( (iPosition >= Token.Size) ||
		(Token.Data[iPosition++] != ':') ) {
		return false;
	}
	Rest = (xstrview){
		Token.Data + iPosition, Token.Size - iPosition
	};
	if ( !__xrtCookieDateDigits(
		Rest, 1, 2, pSecond, &iDigits
	) ) {
		return false;
	}
	iPosition += iDigits;
	return (iPosition == Token.Size) ||
		(Token.Data[iPosition] < '0') || (Token.Data[iPosition] > '9');
}



/* 从 token 的前三个字节识别英文月份。 */
static int __xrtCookieDateMonth(xstrview Token)
{
	static const char Months[][3] = {
		{ 'j', 'a', 'n' }, { 'f', 'e', 'b' }, { 'm', 'a', 'r' },
		{ 'a', 'p', 'r' }, { 'm', 'a', 'y' }, { 'j', 'u', 'n' },
		{ 'j', 'u', 'l' }, { 'a', 'u', 'g' }, { 's', 'e', 'p' },
		{ 'o', 'c', 't' }, { 'n', 'o', 'v' }, { 'd', 'e', 'c' }
	};
	int i;

	if ( Token.Size < 3 ) {
		return 0;
	}
	for ( i = 0; i < 12; i++ ) {
		if ( (__xhttpAsciiLower((unsigned char)Token.Data[0]) ==
			 (unsigned char)Months[i][0]) &&
			(__xhttpAsciiLower((unsigned char)Token.Data[1]) ==
			 (unsigned char)Months[i][1]) &&
			(__xhttpAsciiLower((unsigned char)Token.Data[2]) ==
			 (unsigned char)Months[i][2]) ) {
			return i + 1;
		}
	}
	return 0;
}



/* 无错误副作用地执行 RFC 10025 cookie-date 接收算法。 */
static bool __xrtCookieDateParseValue(xstrview Text, xtime* pTime)
{
	bool bTime = false;
	bool bDay = false;
	bool bMonth = false;
	bool bYear = false;
	int iHour = 0;
	int iMinute = 0;
	int iSecond = 0;
	int iDay = 0;
	int iMonth = 0;
	int iYear = 0;
	size_t iPosition = 0;
	xdatetime DateTime;

	while ( iPosition < Text.Size ) {
		size_t iBegin;
		size_t iDigits;
		int iValue;
		xstrview Token;

		while ( (iPosition < Text.Size) &&
			__xrtCookieDateDelimiter(
				(unsigned char)Text.Data[iPosition]
			) ) {
			iPosition++;
		}
		iBegin = iPosition;
		while ( (iPosition < Text.Size) &&
			!__xrtCookieDateDelimiter(
				(unsigned char)Text.Data[iPosition]
			) ) {
			iPosition++;
		}
		if ( iPosition == iBegin ) {
			continue;
		}
		Token = (xstrview){ Text.Data + iBegin, iPosition - iBegin };
		if ( !bTime && __xrtCookieDateTime(
			Token, &iHour, &iMinute, &iSecond
		) ) {
			bTime = true;
			continue;
		}
		if ( !bDay && __xrtCookieDateDigits(
			Token, 1, 2, &iValue, &iDigits
		) ) {
			iDay = iValue;
			bDay = true;
			continue;
		}
		if ( !bMonth && ((iValue = __xrtCookieDateMonth(Token)) != 0) ) {
			iMonth = iValue;
			bMonth = true;
			continue;
		}
		if ( !bYear && __xrtCookieDateDigits(
			Token, 2, 4, &iValue, &iDigits
		) ) {
			iYear = iValue;
			bYear = true;
		}
	}
	if ( !bTime || !bDay || !bMonth || !bYear ) {
		return false;
	}
	if ( (iYear >= 70) && (iYear <= 99) ) {
		iYear += 1900;
	} else if ( (iYear >= 0) && (iYear <= 69) ) {
		iYear += 2000;
	}
	if ( (iYear < 1601) || (iDay < 1) || (iDay > 31) ||
		(iHour > 23) || (iMinute > 59) || (iSecond > 59) ) {
		return false;
	}
	memset(&DateTime, 0, sizeof(DateTime));
	DateTime.Year = iYear;
	DateTime.Month = iMonth;
	DateTime.Day = iDay;
	DateTime.Hour = iHour;
	DateTime.Minute = iMinute;
	DateTime.Second = iSecond;
	return xrtTimeMake(&DateTime, pTime);
}



/* 按 RFC 10025 宽松 cookie-date 算法解析 UTC 时间。 */
XRT_API bool xrtCookieDateParse(xstrview Text, xtime* pTime)
{
	xtime iTime;

	if ( (pTime == NULL) || !__xhttpCookieViewValid(Text) ||
		xrtMemRangesOverlap(pTime, sizeof(*pTime), Text.Data, Text.Size) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtCookieDateParseValue(Text, &iTime) ) {
		__xhttpErrorSetValue();
		return false;
	}
	*pTime = iTime;
	return true;
}



/* 按用户代理规则解析并饱和保存 Max-Age。 */
static bool __xrtSetCookieMaxAgeParse(xstrview Text, int64* pValue)
{
	bool bNegative = false;
	uint64 iValue = 0;
	size_t i = 0;

	if ( Text.Size == 0 ) {
		return false;
	}
	if ( Text.Data[0] == '-' ) {
		bNegative = true;
		i = 1;
	}
	if ( (i == Text.Size) || (Text.Data[i] < '0') ||
		(Text.Data[i] > '9') ) {
		return false;
	}
	for ( ; i < Text.Size; i++ ) {
		uint64 iDigit;

		if ( (Text.Data[i] < '0') || (Text.Data[i] > '9') ) {
			return false;
		}
		iDigit = (uint64)(Text.Data[i] - '0');
		if ( iValue > ((UINT64_MAX - iDigit) / UINT64_C(10)) ) {
			iValue = UINT64_MAX;
		} else if ( iValue != UINT64_MAX ) {
			iValue = (iValue * UINT64_C(10)) + iDigit;
		}
	}
	if ( bNegative ) {
		uint64 iLimit = (uint64)INT64_MAX + UINT64_C(1);

		*pValue = (iValue >= iLimit) ? INT64_MIN : -(int64)iValue;
	} else {
		*pValue = (iValue > (uint64)INT64_MAX) ?
			INT64_MAX : (int64)iValue;
	}
	return true;
}



/* 把 SameSite 属性值映射为稳定枚举。 */
static xcookiesamesite __xrtSetCookieSameSite(xstrview Value)
{
	if ( __xrtSetCookieName(Value, "Lax", 3) ) {
		return XCOOKIE_SAME_SITE_LAX;
	}
	if ( __xrtSetCookieName(Value, "Strict", 6) ) {
		return XCOOKIE_SAME_SITE_STRICT;
	}
	if ( __xrtSetCookieName(Value, "None", 4) ) {
		return XCOOKIE_SAME_SITE_NONE;
	}
	return XCOOKIE_SAME_SITE_DEFAULT;
}



/* 把 Priority 扩展属性值映射为稳定枚举。 */
static xcookiepriority __xrtSetCookiePriority(xstrview Value)
{
	if ( __xrtSetCookieName(Value, "Low", 3) ) {
		return XCOOKIE_PRIORITY_LOW;
	}
	if ( __xrtSetCookieName(Value, "Medium", 6) ) {
		return XCOOKIE_PRIORITY_MEDIUM;
	}
	if ( __xrtSetCookieName(Value, "High", 4) ) {
		return XCOOKIE_PRIORITY_HIGH;
	}
	return XCOOKIE_PRIORITY_UNSPECIFIED;
}



/* 按 RFC 10025 用户代理接收算法解析 Set-Cookie 字段值。 */
XRT_API bool xrtSetCookieParse(xstrview Text, xsetcookie* pCookie)
{
	xsetcookie Cookie;
	size_t iPairEnd = 0;
	size_t iEqual;
	xstrview Pair;

	if ( (pCookie == NULL) || !__xhttpCookieViewValid(Text) ||
		xrtMemRangesOverlap(
			pCookie, sizeof(*pCookie), Text.Data, Text.Size
		) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtSetCookieHasCtl(Text) ) {
		__xhttpErrorSetValue();
		return false;
	}
	memset(&Cookie, 0, sizeof(Cookie));
	while ( (iPairEnd < Text.Size) && (Text.Data[iPairEnd] != ';') ) {
		iPairEnd++;
	}
	Pair = __xrtSetCookieTrim((xstrview){ Text.Data, iPairEnd });
	iEqual = 0;
	while ( (iEqual < Pair.Size) && (Pair.Data[iEqual] != '=') ) {
		iEqual++;
	}
	if ( iEqual == Pair.Size ) {
		Cookie.Value = Pair;
	} else {
		Cookie.Name = __xrtSetCookieTrim((xstrview){
			Pair.Data, iEqual
		});
		Cookie.Value = __xrtSetCookieTrim((xstrview){
			Pair.Data + iEqual + 1u, Pair.Size - iEqual - 1u
		});
	}
	if ( Cookie.Name.Size >
		(SIZE_MAX - Cookie.Value.Size) ) {
		__xhttpErrorSetSizeOverflow();
		return false;
	}
	if ( (Cookie.Name.Size + Cookie.Value.Size) >
		XSET_COOKIE_MAX_PAIR_BYTES ) {
		__xhttpErrorSetRange();
		return false;
	}
	if ( iPairEnd < Text.Size ) {
		size_t iOffset = 0;
		xcookieattribute Attribute;

		Cookie.RawAttributes = (xstrview){
			Text.Data + iPairEnd + 1u, Text.Size - iPairEnd - 1u
		};
		while ( xrtSetCookieAttributeNext(
			Cookie.RawAttributes, &iOffset, &Attribute
		) == XCOOKIE_ATTRIBUTE_ITEM ) {
			if ( Attribute.Value.Size >
				XSET_COOKIE_MAX_ATTRIBUTE_VALUE ) {
				continue;
			}
			if ( __xrtSetCookieName(Attribute.Name, "Expires", 7) ) {
				xtime iExpires;

				if ( __xrtCookieDateParseValue(
					Attribute.Value, &iExpires
				) ) {
					Cookie.Expires = iExpires;
					Cookie.Flags |= XSET_COOKIE_HAS_EXPIRES;
				}
			} else if ( __xrtSetCookieName(
				Attribute.Name, "Max-Age", 7
			) ) {
				int64 iMaxAge;

				if ( __xrtSetCookieMaxAgeParse(
					Attribute.Value, &iMaxAge
				) ) {
					Cookie.MaxAge = iMaxAge;
					Cookie.Flags |= XSET_COOKIE_HAS_MAX_AGE;
				}
			} else if ( __xrtSetCookieName(
				Attribute.Name, "Domain", 6
			) ) {
				Cookie.Domain = Attribute.Value;
				if ( (Cookie.Domain.Size != 0) &&
					(Cookie.Domain.Data[0] == '.') ) {
					Cookie.Domain.Data++;
					Cookie.Domain.Size--;
				}
				Cookie.Flags |= XSET_COOKIE_HAS_DOMAIN;
			} else if ( __xrtSetCookieName(
				Attribute.Name, "Path", 4
			) ) {
				Cookie.Path = Attribute.Value;
				Cookie.Flags |= XSET_COOKIE_HAS_PATH;
			} else if ( __xrtSetCookieName(
				Attribute.Name, "Secure", 6
			) ) {
				Cookie.Flags |= XSET_COOKIE_SECURE;
			} else if ( __xrtSetCookieName(
				Attribute.Name, "HttpOnly", 8
			) ) {
				Cookie.Flags |= XSET_COOKIE_HTTP_ONLY;
			} else if ( __xrtSetCookieName(
				Attribute.Name, "SameSite", 8
			) ) {
				Cookie.SameSite = __xrtSetCookieSameSite(
					Attribute.Value
				);
				Cookie.Flags |= XSET_COOKIE_HAS_SAME_SITE;
			} else if ( __xrtSetCookieName(
				Attribute.Name, "Partitioned", 11
			) ) {
				Cookie.Flags |= XSET_COOKIE_PARTITIONED;
			} else if ( __xrtSetCookieName(
				Attribute.Name, "Priority", 8
			) ) {
				xcookiepriority Priority =
					__xrtSetCookiePriority(Attribute.Value);

				if ( Priority != XCOOKIE_PRIORITY_UNSPECIFIED ) {
					Cookie.Priority = Priority;
					Cookie.Flags |= XSET_COOKIE_HAS_PRIORITY;
				}
			}
		}
	}
	*pCookie = Cookie;
	return true;
}



/* 判断 Domain 是否符合服务器生成侧的 ASCII 子域语法。 */
static bool __xrtSetCookieDomainValid(xstrview Domain)
{
	size_t iLabel = 0;
	size_t i;

	if ( !__xhttpCookieViewValid(Domain) || (Domain.Size == 0) ||
		(Domain.Size > 253) ) {
		return false;
	}
	for ( i = 0; i < Domain.Size; i++ ) {
		unsigned char iByte = (unsigned char)Domain.Data[i];
		bool bAlpha = ((iByte >= 'A') && (iByte <= 'Z')) ||
			((iByte >= 'a') && (iByte <= 'z'));
		bool bDigit = (iByte >= '0') && (iByte <= '9');

		if ( iByte == '.' ) {
			if ( (iLabel == 0) || (iLabel > 63) ||
				(Domain.Data[i - 1u] == '-') ) {
				return false;
			}
			iLabel = 0;
			continue;
		}
		if ( !bAlpha && !bDigit && (iByte != '-') ) {
			return false;
		}
		if ( (iLabel == 0) && (iByte == '-') ) {
			return false;
		}
		iLabel++;
	}
	return (iLabel != 0) && (iLabel <= 63) &&
		(Domain.Data[Domain.Size - 1u] != '-');
}



/* 判断 Max-Age 是否符合严格服务器生成语法。 */
static bool __xrtSetCookieStrictMaxAge(xstrview Value)
{
	size_t i;

	if ( (Value.Size == 0) || (Value.Data[0] < '1') ||
		(Value.Data[0] > '9') ) {
		return false;
	}
	for ( i = 1; i < Value.Size; i++ ) {
		if ( (Value.Data[i] < '0') || (Value.Data[i] > '9') ) {
			return false;
		}
	}
	return true;
}



/* 判断当前属性名是否已经在前面的属性段出现。 */
static bool __xrtSetCookieDuplicateBefore(
	xstrview Text,
	size_t iBegin,
	size_t iCurrent,
	xstrview Name
)
{
	size_t iPosition = iBegin;

	while ( iPosition < iCurrent ) {
		xcookieattribute Previous;
		size_t iNext;

		__xrtSetCookieAttributeSegment(
			Text, iPosition, &Previous, &iNext
		);
		if ( __xhttpCookieAsciiEqual(Previous.Name, Name) ) {
			return true;
		}
		iPosition = iNext;
	}
	return false;
}



/* 判断名称是否以一个 ASCII 大小写不敏感前缀开始。 */
static bool __xrtSetCookieStartsWith(
	xstrview Text,
	cstr sPrefix,
	size_t iPrefix
)
{
	return (Text.Size >= iPrefix) && __xhttpCookieAsciiEqual(
		(xstrview){ Text.Data, iPrefix },
		(xstrview){ sPrefix, iPrefix }
	);
}



/* 统一验证文本与结构化生成路径共享的部署安全约束。 */
static bool __xrtSetCookieSecurityValid(const xsetcookie* pCookie)
{
	if ( ((pCookie->Flags & XSET_COOKIE_HAS_SAME_SITE) != 0) &&
		(pCookie->SameSite == XCOOKIE_SAME_SITE_NONE) &&
		((pCookie->Flags & XSET_COOKIE_SECURE) == 0) ) {
		return false;
	}
	if ( ((pCookie->Flags & XSET_COOKIE_PARTITIONED) != 0) &&
		((pCookie->Flags & XSET_COOKIE_SECURE) == 0) ) {
		return false;
	}
	if ( __xrtSetCookieStartsWith(
		pCookie->Name, "__Secure-", 9
	) && ((pCookie->Flags & XSET_COOKIE_SECURE) == 0) ) {
		return false;
	}
	if ( __xrtSetCookieStartsWith(pCookie->Name, "__Host-", 7) &&
		(((pCookie->Flags & XSET_COOKIE_SECURE) == 0) ||
		 ((pCookie->Flags & XSET_COOKIE_HAS_DOMAIN) != 0) ||
		 ((pCookie->Flags & XSET_COOKIE_HAS_PATH) == 0) ||
		 (pCookie->Path.Size != 1) || (pCookie->Path.Data[0] != '/')) ) {
		return false;
	}
	return true;
}



/* 严格校验一个已拆分的服务器生成属性。 */
static bool __xrtSetCookieStrictAttribute(
	xcookieattribute Attribute,
	xstrview Segment
)
{
	bool bValue = (Attribute.Flags & XCOOKIE_ATTRIBUTE_HAS_VALUE) != 0;

	if ( !__xhttpCookieAttributeValueValid(Segment) ||
		(Segment.Size == 0) ) {
		return false;
	}
	if ( __xrtSetCookieName(Attribute.Name, "Expires", 7) ) {
		xtime iTime;

		if ( !bValue || (Attribute.Value.Size != 29) ||
			!xrtTimeParseHTTPDate(Attribute.Value, &iTime) ) {
			return false;
		}
		return true;
	}
	if ( __xrtSetCookieName(Attribute.Name, "Max-Age", 7) ) {
		return bValue && __xrtSetCookieStrictMaxAge(Attribute.Value);
	}
	if ( __xrtSetCookieName(Attribute.Name, "Domain", 6) ) {
		return bValue && __xrtSetCookieDomainValid(Attribute.Value);
	}
	if ( __xrtSetCookieName(Attribute.Name, "Path", 4) ) {
		return bValue &&
			__xhttpCookieAttributeValueValid(Attribute.Value);
	}
	if ( __xrtSetCookieName(Attribute.Name, "Secure", 6) ||
		__xrtSetCookieName(Attribute.Name, "HttpOnly", 8) ) {
		return !bValue;
	}
	if ( __xrtSetCookieName(Attribute.Name, "SameSite", 8) ) {
		return bValue &&
			(__xrtSetCookieSameSite(Attribute.Value) !=
			 XCOOKIE_SAME_SITE_DEFAULT);
	}
	if ( __xrtSetCookieName(Attribute.Name, "Partitioned", 11) ) {
		return !bValue;
	}
	if ( __xrtSetCookieName(Attribute.Name, "Priority", 8) ) {
		return bValue &&
			(__xrtSetCookiePriority(Attribute.Value) !=
			 XCOOKIE_PRIORITY_UNSPECIFIED);
	}
	return true;
}



/* 按 RFC 10025 服务器生成语法严格校验 Set-Cookie 字段值。 */
XRT_API bool xrtSetCookieValidate(xstrview Text)
{
	xsetcookie Cookie;
	xstrview Whole;
	xstrview Pair;
	size_t iPairEnd = 0;
	size_t iEqual;
	size_t iPosition;

	if ( !__xhttpCookieViewValid(Text) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtSetCookieHasCtl(Text) ) {
		__xhttpErrorSetValue();
		return false;
	}
	Whole = __xrtSetCookieTrim(Text);
	while ( (iPairEnd < Whole.Size) &&
		(Whole.Data[iPairEnd] != ';') ) {
		iPairEnd++;
	}
	Pair = __xrtSetCookieTrim((xstrview){ Whole.Data, iPairEnd });
	iEqual = 0;
	while ( (iEqual < Pair.Size) && (Pair.Data[iEqual] != '=') ) {
		iEqual++;
	}
	if ( (iEqual == 0) || (iEqual == Pair.Size) ||
		!xrtHttpTokenValid(__xrtSetCookieTrim((xstrview){
			Pair.Data, iEqual
		})) || !__xhttpCookieValueValid(__xrtSetCookieTrim((xstrview){
			Pair.Data + iEqual + 1u, Pair.Size - iEqual - 1u
		})) ) {
		__xhttpErrorSetValue();
		return false;
	}
	iPosition = (iPairEnd < Whole.Size) ? (iPairEnd + 1u) : Whole.Size;
	while ( iPosition < Whole.Size ) {
		xcookieattribute Attribute;
		xstrview Segment;
		size_t iSegment = iPosition;
		size_t iEnd = iPosition;
		size_t iNext;

		while ( (iEnd < Whole.Size) && (Whole.Data[iEnd] != ';') ) {
			iEnd++;
		}
		__xrtSetCookieAttributeSegment(
			Whole, iPosition, &Attribute, &iNext
		);
		Segment = __xrtSetCookieTrim((xstrview){
			Whole.Data + iPosition, iEnd - iPosition
		});
		if ( (Segment.Size == 0) ||
			__xrtSetCookieDuplicateBefore(
				Whole, iPairEnd + 1u, iSegment, Attribute.Name
			) || !__xrtSetCookieStrictAttribute(
				Attribute, Segment
			) ) {
			__xhttpErrorSetValue();
			return false;
		}
		iPosition = iNext;
	}
	if ( (Whole.Size != 0) && (Whole.Data[Whole.Size - 1u] == ';') ) {
		__xhttpErrorSetValue();
		return false;
	}
	if ( !xrtSetCookieParse(Whole, &Cookie) ||
		!__xrtSetCookieSecurityValid(&Cookie) ) {
		__xhttpErrorSetValue();
		return false;
	}
	return true;
}



/* 判断扩展属性名称是否与结构化标准属性冲突。 */
static bool __xrtSetCookieKnownAttribute(xstrview Name)
{
	return __xrtSetCookieName(Name, "Domain", 6) ||
		__xrtSetCookieName(Name, "Path", 4) ||
		__xrtSetCookieName(Name, "Expires", 7) ||
		__xrtSetCookieName(Name, "Max-Age", 7) ||
		__xrtSetCookieName(Name, "SameSite", 8) ||
		__xrtSetCookieName(Name, "Secure", 6) ||
		__xrtSetCookieName(Name, "HttpOnly", 8) ||
		__xrtSetCookieName(Name, "Partitioned", 11) ||
		__xrtSetCookieName(Name, "Priority", 8);
}



/* 验证结构化 Set-Cookie 构建输入的完整状态。 */
static bool __xrtSetCookieBuildValid(const xsetcookie* pCookie)
{
	const uint32 iKnown = XSET_COOKIE_HAS_DOMAIN |
		XSET_COOKIE_HAS_PATH | XSET_COOKIE_HAS_EXPIRES |
		XSET_COOKIE_HAS_MAX_AGE | XSET_COOKIE_HAS_SAME_SITE |
		XSET_COOKIE_SECURE | XSET_COOKIE_HTTP_ONLY |
		XSET_COOKIE_PARTITIONED | XSET_COOKIE_HAS_PRIORITY;
	size_t i;

	if ( (pCookie == NULL) || ((pCookie->Flags & ~iKnown) != 0) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	if ( !__xhttpCookieViewValid(pCookie->Name) ||
		!__xhttpCookieViewValid(pCookie->Value) ||
		!__xhttpCookieViewValid(pCookie->Domain) ||
		!__xhttpCookieViewValid(pCookie->Path) ||
		!__xhttpCookieViewValid(pCookie->RawAttributes) ||
		((pCookie->Extensions == NULL) &&
		 (pCookie->ExtensionCount != 0)) ||
		(pCookie->ExtensionCount >
		 (SIZE_MAX / sizeof(*pCookie->Extensions))) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtHttpTokenValid(pCookie->Name) ||
		!__xhttpCookieValueValid(pCookie->Value) ) {
		__xhttpErrorSetValue();
		return false;
	}
	if ( pCookie->Name.Size > (SIZE_MAX - pCookie->Value.Size) ) {
		__xhttpErrorSetSizeOverflow();
		return false;
	}
	if ( (pCookie->Name.Size + pCookie->Value.Size) >
		XSET_COOKIE_MAX_PAIR_BYTES ) {
		__xhttpErrorSetRange();
		return false;
	}
	if ( pCookie->RawAttributes.Size != 0 ) {
		__xhttpErrorSetInvalidState();
		return false;
	}
	if ( (((pCookie->Flags & XSET_COOKIE_HAS_DOMAIN) != 0) !=
		 (pCookie->Domain.Size != 0)) ||
		(((pCookie->Flags & XSET_COOKIE_HAS_PATH) == 0) &&
		 (pCookie->Path.Size != 0)) ||
		(((pCookie->Flags & XSET_COOKIE_HAS_EXPIRES) == 0) &&
		 (pCookie->Expires != 0)) ||
		(((pCookie->Flags & XSET_COOKIE_HAS_MAX_AGE) == 0) &&
		 (pCookie->MaxAge != 0)) ||
		(((pCookie->Flags & XSET_COOKIE_HAS_SAME_SITE) == 0) &&
		 (pCookie->SameSite != XCOOKIE_SAME_SITE_DEFAULT)) ||
		(((pCookie->Flags & XSET_COOKIE_HAS_PRIORITY) == 0) &&
		 (pCookie->Priority != XCOOKIE_PRIORITY_UNSPECIFIED)) ) {
		__xhttpErrorSetInvalidState();
		return false;
	}
	if ( ((pCookie->Flags & XSET_COOKIE_HAS_DOMAIN) != 0) &&
		!__xrtSetCookieDomainValid(pCookie->Domain) ) {
		__xhttpErrorSetValue();
		return false;
	}
	if ( ((pCookie->Flags & XSET_COOKIE_HAS_PATH) != 0) &&
		!__xhttpCookieAttributeValueValid(pCookie->Path) ) {
		__xhttpErrorSetValue();
		return false;
	}
	if ( ((pCookie->Flags & XSET_COOKIE_HAS_SAME_SITE) != 0) &&
		((pCookie->SameSite < XCOOKIE_SAME_SITE_LAX) ||
		 (pCookie->SameSite > XCOOKIE_SAME_SITE_NONE)) ) {
		__xhttpErrorSetValue();
		return false;
	}
	if ( ((pCookie->Flags & XSET_COOKIE_HAS_PRIORITY) != 0) &&
		((pCookie->Priority < XCOOKIE_PRIORITY_LOW) ||
		 (pCookie->Priority > XCOOKIE_PRIORITY_HIGH)) ) {
		__xhttpErrorSetValue();
		return false;
	}
	if ( !__xrtSetCookieSecurityValid(pCookie) ) {
		__xhttpErrorSetValue();
		return false;
	}
	for ( i = 0; i < pCookie->ExtensionCount; i++ ) {
		const xcookieattribute* pAttribute = &pCookie->Extensions[i];
		size_t j;

		if ( ((pAttribute->Flags & ~XCOOKIE_ATTRIBUTE_HAS_VALUE) != 0) ||
			!__xhttpCookieViewValid(pAttribute->Name) ||
			!__xhttpCookieViewValid(pAttribute->Value) ||
			(((pAttribute->Flags & XCOOKIE_ATTRIBUTE_HAS_VALUE) == 0) &&
			 (pAttribute->Value.Size != 0)) ) {
			__xhttpErrorSetInvalidArgument();
			return false;
		}
		if ( !xrtHttpTokenValid(pAttribute->Name) ||
			__xrtSetCookieKnownAttribute(pAttribute->Name) ||
			!__xhttpCookieAttributeValueValid(pAttribute->Value) ) {
			__xhttpErrorSetValue();
			return false;
		}
		for ( j = 0; j < i; j++ ) {
			if ( __xhttpCookieAsciiEqual(
				pCookie->Extensions[j].Name, pAttribute->Name
			) ) {
				__xhttpErrorSetValue();
				return false;
			}
		}
	}
	return true;
}



/* 安全累加 Set-Cookie 输出长度。 */
static bool __xrtSetCookieAdd(size_t* pSize, size_t iAdd)
{
	if ( *pSize > (SIZE_MAX - iAdd) ) {
		__xhttpErrorSetSizeOverflow();
		return false;
	}
	*pSize += iAdd;
	return true;
}



/* 返回有符号十进制整数的输出长度。 */
static size_t __xrtSetCookieIntegerSize(int64 iValue)
{
	uint64 iMagnitude;
	size_t iSize = iValue < 0 ? 1u : 0u;

	if ( iValue < 0 ) {
		iMagnitude = (uint64)(-(iValue + 1)) + UINT64_C(1);
	} else {
		iMagnitude = (uint64)iValue;
	}
	do {
		iSize++;
		iMagnitude /= UINT64_C(10);
	} while ( iMagnitude != 0 );
	return iSize;
}



/* 计算已经通过状态校验的 Set-Cookie 输出长度。 */
static bool __xrtSetCookieMeasure(
	const xsetcookie* pCookie,
	size_t* pRequired
)
{
	size_t iSize = pCookie->Name.Size + 1u + pCookie->Value.Size;
	size_t i;

	if ( ((pCookie->Flags & XSET_COOKIE_HAS_DOMAIN) != 0) &&
		!__xrtSetCookieAdd(&iSize, 9u + pCookie->Domain.Size) ) {
		return false;
	}
	if ( ((pCookie->Flags & XSET_COOKIE_HAS_PATH) != 0) &&
		!__xrtSetCookieAdd(&iSize, 7u + pCookie->Path.Size) ) {
		return false;
	}
	if ( ((pCookie->Flags & XSET_COOKIE_HAS_EXPIRES) != 0) &&
		!__xrtSetCookieAdd(&iSize, 39u) ) {
		return false;
	}
	if ( ((pCookie->Flags & XSET_COOKIE_HAS_MAX_AGE) != 0) &&
		!__xrtSetCookieAdd(
			&iSize, 10u + __xrtSetCookieIntegerSize(pCookie->MaxAge)
		) ) {
		return false;
	}
	if ( ((pCookie->Flags & XSET_COOKIE_HAS_SAME_SITE) != 0) &&
		!__xrtSetCookieAdd(&iSize, 11u +
			(pCookie->SameSite == XCOOKIE_SAME_SITE_LAX ? 3u :
			 pCookie->SameSite == XCOOKIE_SAME_SITE_NONE ? 4u : 6u)
		) ) {
		return false;
	}
	if ( ((pCookie->Flags & XSET_COOKIE_SECURE) != 0) &&
		!__xrtSetCookieAdd(&iSize, 8u) ) {
		return false;
	}
	if ( ((pCookie->Flags & XSET_COOKIE_HTTP_ONLY) != 0) &&
		!__xrtSetCookieAdd(&iSize, 10u) ) {
		return false;
	}
	if ( ((pCookie->Flags & XSET_COOKIE_PARTITIONED) != 0) &&
		!__xrtSetCookieAdd(&iSize, 13u) ) {
		return false;
	}
	if ( ((pCookie->Flags & XSET_COOKIE_HAS_PRIORITY) != 0) &&
		!__xrtSetCookieAdd(&iSize, 11u +
			(pCookie->Priority == XCOOKIE_PRIORITY_LOW ? 3u :
			 pCookie->Priority == XCOOKIE_PRIORITY_HIGH ? 4u : 6u)
		) ) {
		return false;
	}
	for ( i = 0; i < pCookie->ExtensionCount; i++ ) {
		size_t iAdd = 2u + pCookie->Extensions[i].Name.Size;

		if ( (pCookie->Extensions[i].Flags &
			XCOOKIE_ATTRIBUTE_HAS_VALUE) != 0 ) {
			if ( iAdd == SIZE_MAX ) {
				__xhttpErrorSetSizeOverflow();
				return false;
			}
			iAdd++;
			if ( iAdd > (SIZE_MAX - pCookie->Extensions[i].Value.Size) ) {
				__xhttpErrorSetSizeOverflow();
				return false;
			}
			iAdd += pCookie->Extensions[i].Value.Size;
		}
		if ( !__xrtSetCookieAdd(&iSize, iAdd) ) {
			return false;
		}
	}
	*pRequired = iSize;
	return true;
}



/* 判断结构化 Set-Cookie 的任一元数据范围是否与目标重叠。 */
static bool __xrtSetCookieOverlap(
	const xsetcookie* pCookie,
	const void* pData,
	size_t iSize
)
{
	size_t i;

	if ( (pData == NULL) || (iSize == 0) ) {
		return false;
	}
	if ( xrtMemRangesOverlap(pCookie, sizeof(*pCookie), pData, iSize) ||
		xrtMemRangesOverlap(
			pCookie->Name.Data, pCookie->Name.Size, pData, iSize
		) || xrtMemRangesOverlap(
			pCookie->Value.Data, pCookie->Value.Size, pData, iSize
		) || xrtMemRangesOverlap(
			pCookie->Domain.Data, pCookie->Domain.Size, pData, iSize
		) || xrtMemRangesOverlap(
			pCookie->Path.Data, pCookie->Path.Size, pData, iSize
		) || xrtMemRangesOverlap(
			pCookie->Extensions,
			pCookie->ExtensionCount * sizeof(*pCookie->Extensions),
			pData, iSize
		) ) {
		return true;
	}
	for ( i = 0; i < pCookie->ExtensionCount; i++ ) {
		if ( xrtMemRangesOverlap(
			pCookie->Extensions[i].Name.Data,
			pCookie->Extensions[i].Name.Size, pData, iSize
		) || xrtMemRangesOverlap(
			pCookie->Extensions[i].Value.Data,
			pCookie->Extensions[i].Value.Size, pData, iSize
		) ) {
			return true;
		}
	}
	return false;
}



/* 向已经确认容量的输出追加固定字节。 */
static void __xrtSetCookieAppend(
	uint8* pOutput,
	size_t* pOffset,
	const void* pData,
	size_t iSize
)
{
	if ( iSize != 0 ) {
		memcpy(pOutput + *pOffset, pData, iSize);
		*pOffset += iSize;
	}
}



/* 向已经确认容量的输出写入有符号十进制整数。 */
static void __xrtSetCookieIntegerWrite(
	uint8* pOutput,
	size_t* pOffset,
	int64 iValue
)
{
	uint64 iMagnitude;
	size_t iDigits;
	size_t iBegin = *pOffset;

	if ( iValue < 0 ) {
		pOutput[(*pOffset)++] = (uint8)'-';
		iMagnitude = (uint64)(-(iValue + 1)) + UINT64_C(1);
		iBegin++;
	} else {
		iMagnitude = (uint64)iValue;
	}
	iDigits = 1;
	while ( iMagnitude >= UINT64_C(10) ) {
		iMagnitude /= UINT64_C(10);
		iDigits++;
	}
	*pOffset = iBegin + iDigits;
	iMagnitude = iValue < 0 ?
		((uint64)(-(iValue + 1)) + UINT64_C(1)) : (uint64)iValue;
	while ( iDigits != 0 ) {
		pOutput[iBegin + iDigits - 1u] =
			(uint8)('0' + (iMagnitude % UINT64_C(10)));
		iMagnitude /= UINT64_C(10);
		iDigits--;
	}
}



/* 从结构化数据写出 Set-Cookie 字段值。 */
XRT_API bool xrtSetCookieWrite(
	const xsetcookie* pCookie,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	uint8* pWrite = (uint8*)pOutput;
	size_t iRequired;
	size_t iOffset = 0;
	size_t i;
	char Date[30];

	if ( (pSize == NULL) || ((pOutput == NULL) && (iCapacity != 0)) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtSetCookieBuildValid(pCookie) ||
		!__xrtSetCookieMeasure(pCookie, &iRequired) ) {
		return false;
	}
	if ( ((pCookie->Flags & XSET_COOKIE_HAS_EXPIRES) != 0) &&
		(xrtTimeWriteHTTPDate(
			Date, sizeof(Date), pCookie->Expires
		) != 29) ) {
		return false;
	}
	if ( __xrtSetCookieOverlap(pCookie, pSize, sizeof(*pSize)) ||
		((pOutput != NULL) && xrtMemRangesOverlap(
			pSize, sizeof(*pSize), pOutput, iRequired
		)) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	if ( pOutput == NULL ) {
		*pSize = iRequired;
		return true;
	}
	if ( iCapacity < iRequired ) {
		*pSize = iRequired;
		__xhttpErrorSetRange();
		return false;
	}
	if ( __xrtSetCookieOverlap(pCookie, pOutput, iRequired) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	__xrtSetCookieAppend(pWrite, &iOffset,
		pCookie->Name.Data, pCookie->Name.Size);
	pWrite[iOffset++] = (uint8)'=';
	__xrtSetCookieAppend(pWrite, &iOffset,
		pCookie->Value.Data, pCookie->Value.Size);
	if ( (pCookie->Flags & XSET_COOKIE_HAS_DOMAIN) != 0 ) {
		__xrtSetCookieAppend(pWrite, &iOffset, "; Domain=", 9);
		__xrtSetCookieAppend(pWrite, &iOffset,
			pCookie->Domain.Data, pCookie->Domain.Size);
	}
	if ( (pCookie->Flags & XSET_COOKIE_HAS_PATH) != 0 ) {
		__xrtSetCookieAppend(pWrite, &iOffset, "; Path=", 7);
		__xrtSetCookieAppend(pWrite, &iOffset,
			pCookie->Path.Data, pCookie->Path.Size);
	}
	if ( (pCookie->Flags & XSET_COOKIE_HAS_EXPIRES) != 0 ) {
		__xrtSetCookieAppend(pWrite, &iOffset, "; Expires=", 10);
		__xrtSetCookieAppend(pWrite, &iOffset, Date, 29);
	}
	if ( (pCookie->Flags & XSET_COOKIE_HAS_MAX_AGE) != 0 ) {
		__xrtSetCookieAppend(pWrite, &iOffset, "; Max-Age=", 10);
		__xrtSetCookieIntegerWrite(pWrite, &iOffset, pCookie->MaxAge);
	}
	if ( (pCookie->Flags & XSET_COOKIE_HAS_SAME_SITE) != 0 ) {
		__xrtSetCookieAppend(pWrite, &iOffset, "; SameSite=", 11);
		if ( pCookie->SameSite == XCOOKIE_SAME_SITE_LAX ) {
			__xrtSetCookieAppend(pWrite, &iOffset, "Lax", 3);
		} else if ( pCookie->SameSite == XCOOKIE_SAME_SITE_NONE ) {
			__xrtSetCookieAppend(pWrite, &iOffset, "None", 4);
		} else {
			__xrtSetCookieAppend(pWrite, &iOffset, "Strict", 6);
		}
	}
	if ( (pCookie->Flags & XSET_COOKIE_SECURE) != 0 ) {
		__xrtSetCookieAppend(pWrite, &iOffset, "; Secure", 8);
	}
	if ( (pCookie->Flags & XSET_COOKIE_HTTP_ONLY) != 0 ) {
		__xrtSetCookieAppend(pWrite, &iOffset, "; HttpOnly", 10);
	}
	if ( (pCookie->Flags & XSET_COOKIE_PARTITIONED) != 0 ) {
		__xrtSetCookieAppend(pWrite, &iOffset, "; Partitioned", 13);
	}
	if ( (pCookie->Flags & XSET_COOKIE_HAS_PRIORITY) != 0 ) {
		__xrtSetCookieAppend(pWrite, &iOffset, "; Priority=", 11);
		if ( pCookie->Priority == XCOOKIE_PRIORITY_LOW ) {
			__xrtSetCookieAppend(pWrite, &iOffset, "Low", 3);
		} else if ( pCookie->Priority == XCOOKIE_PRIORITY_HIGH ) {
			__xrtSetCookieAppend(pWrite, &iOffset, "High", 4);
		} else {
			__xrtSetCookieAppend(pWrite, &iOffset, "Medium", 6);
		}
	}
	for ( i = 0; i < pCookie->ExtensionCount; i++ ) {
		__xrtSetCookieAppend(pWrite, &iOffset, "; ", 2);
		__xrtSetCookieAppend(pWrite, &iOffset,
			pCookie->Extensions[i].Name.Data,
			pCookie->Extensions[i].Name.Size);
		if ( (pCookie->Extensions[i].Flags &
			XCOOKIE_ATTRIBUTE_HAS_VALUE) != 0 ) {
			pWrite[iOffset++] = (uint8)'=';
			__xrtSetCookieAppend(pWrite, &iOffset,
				pCookie->Extensions[i].Value.Data,
				pCookie->Extensions[i].Value.Size);
		}
	}
	if ( iOffset != iRequired ) {
		__xhttpErrorSetInternal();
		return false;
	}
	*pSize = iRequired;
	return true;
}



/* 分配并构建零结尾 Set-Cookie 字段值。 */
XRT_API str xrtSetCookieBuild(
	const xsetcookie* pCookie,
	size_t* pSize
)
{
	str sOutput;
	size_t iRequired;

	if ( (pCookie != NULL) && !__xrtSetCookieBuildValid(pCookie) ) {
		return NULL;
	}
	if ( (pSize != NULL) && (pCookie != NULL) &&
		__xrtSetCookieOverlap(pCookie, pSize, sizeof(*pSize)) ) {
		__xhttpErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtSetCookieWrite(pCookie, NULL, 0, &iRequired) ||
		(iRequired == SIZE_MAX) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtSetCookieWrite(
		pCookie, sOutput, iRequired, &iRequired
	) ) {
		xrtFree(sOutput);
		return NULL;
	}
	sOutput[iRequired] = '\0';
	if ( pSize != NULL ) {
		*pSize = iRequired;
	}
	return sOutput;
}

#endif
