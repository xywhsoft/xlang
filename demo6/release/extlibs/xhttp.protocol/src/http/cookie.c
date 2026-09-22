#include "../internal/xrt_cookie.h"



#if defined(XHTTP_FEATURE_COOKIE)

/* 验证借用文本的空值一致性。 */
bool __xhttpCookieViewValid(xstrview Text)
{
	return xrtMemRangeValid(Text.Data, Text.Size);
}



/* 判断两个文本是否按 ASCII 规则忽略大小写相等。 */
bool __xhttpCookieAsciiEqual(xstrview Left, xstrview Right)
{
	size_t i;

	if ( Left.Size != Right.Size ) {
		return false;
	}
	for ( i = 0; i < Left.Size; i++ ) {
		if ( __xhttpAsciiLower((unsigned char)Left.Data[i]) !=
			__xhttpAsciiLower((unsigned char)Right.Data[i]) ) {
			return false;
		}
	}
	return true;
}



/* 判断一个字节是否属于 RFC 10025 cookie-octet。 */
static bool __xrtCookieOctet(unsigned char iByte)
{
	return (iByte == 0x21u) ||
		((iByte >= 0x23u) && (iByte <= 0x2Bu)) ||
		((iByte >= 0x2Du) && (iByte <= 0x3Au)) ||
		((iByte >= 0x3Cu) && (iByte <= 0x5Bu)) ||
		((iByte >= 0x5Du) && (iByte <= 0x7Eu));
}



/* 判断一个值是否符合 RFC 10025 严格 cookie-value 语法。 */
bool __xhttpCookieValueValid(xstrview Value)
{
	size_t iBegin = 0;
	size_t iEnd = Value.Size;
	size_t i;

	if ( !__xhttpCookieViewValid(Value) ) {
		return false;
	}
	if ( (Value.Size >= 2) && (Value.Data[0] == '"') &&
		(Value.Data[Value.Size - 1u] == '"') ) {
		iBegin = 1;
		iEnd--;
	} else if ( (Value.Size != 0) &&
		((Value.Data[0] == '"') ||
		 (Value.Data[Value.Size - 1u] == '"')) ) {
		return false;
	}
	for ( i = iBegin; i < iEnd; i++ ) {
		if ( !__xrtCookieOctet((unsigned char)Value.Data[i]) ) {
			return false;
		}
	}
	return true;
}



/* 判断一个值是否只包含 Set-Cookie 属性允许的 av-octet。 */
bool __xhttpCookieAttributeValueValid(xstrview Value)
{
	size_t i;

	if ( !__xhttpCookieViewValid(Value) ) {
		return false;
	}
	for ( i = 0; i < Value.Size; i++ ) {
		unsigned char iByte = (unsigned char)Value.Data[i];

		if ( (iByte < 0x20u) || (iByte > 0x7Eu) || (iByte == ';') ) {
			return false;
		}
	}
	return true;
}



/* 判断 Cookie pair 数组的元数据或借用数据是否与指定范围重叠。 */
bool __xhttpCookiePairsOverlap(
	const xcookiepair* pPairs,
	size_t iCount,
	const void* pData,
	size_t iSize
)
{
	xcookiepair Pair;
	size_t i;

	if ( (iSize == 0) || (pData == NULL) ) {
		return false;
	}
	if ( xrtMemRangesOverlap(
		pPairs, iCount * sizeof(*pPairs), pData, iSize
	) ) {
		return true;
	}
	for ( i = 0; i < iCount; i++ ) {
		memcpy(
			&Pair,
			(const uint8*)pPairs + (i * sizeof(Pair)),
			sizeof(Pair)
		);
		if ( xrtMemRangesOverlap(
			Pair.Name.Data, Pair.Name.Size, pData, iSize
		) || xrtMemRangesOverlap(
			Pair.Value.Data, Pair.Value.Size, pData, iSize
		) ) {
			return true;
		}
	}
	return false;
}



/* 裁剪一个范围两端的 HTTP 空白。 */
static xstrview __xrtCookieTrim(xstrview Text)
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



/* 无错误副作用地解析一个 Cookie pair。 */
static bool __xrtCookiePairParse(
	xstrview Text,
	size_t iOffset,
	xcookiepair* pPair,
	size_t* pNext
)
{
	size_t iBegin = iOffset;
	size_t iEnd;
	size_t iEqual;
	xstrview Name;
	xstrview Value;

	while ( (iBegin < Text.Size) &&
		((Text.Data[iBegin] == ' ') || (Text.Data[iBegin] == '\t')) ) {
		iBegin++;
	}
	if ( iBegin == Text.Size ) {
		return false;
	}
	iEnd = iBegin;
	while ( (iEnd < Text.Size) && (Text.Data[iEnd] != ';') ) {
		iEnd++;
	}
	if ( iEnd < Text.Size ) {
		size_t iRemain = iEnd + 1u;

		while ( (iRemain < Text.Size) &&
			((Text.Data[iRemain] == ' ') ||
			 (Text.Data[iRemain] == '\t')) ) {
			iRemain++;
		}
		if ( iRemain == Text.Size ) {
			return false;
		}
	}
	iEqual = iBegin;
	while ( (iEqual < iEnd) && (Text.Data[iEqual] != '=') ) {
		iEqual++;
	}
	if ( iEqual == iEnd ) {
		return false;
	}
	Name = __xrtCookieTrim((xstrview){
		Text.Data + iBegin, iEqual - iBegin
	});
	Value = __xrtCookieTrim((xstrview){
		Text.Data + iEqual + 1u, iEnd - iEqual - 1u
	});
	if ( !xrtHttpTokenValid(Name) || !__xhttpCookieValueValid(Value) ) {
		return false;
	}
	pPair->Name = Name;
	pPair->Value = Value;
	*pNext = (iEnd < Text.Size) ? (iEnd + 1u) : iEnd;
	return true;
}



/* 逐项扫描完整 Cookie 字段值。 */
XRT_API xcookienext xrtCookieNext(
	xstrview Text,
	size_t* pOffset,
	xcookiepair* pPair
)
{
	xcookiepair Pair;
	size_t iOffset;
	size_t iNext;

	if ( !__xhttpCookieViewValid(Text) ||
		!xrtMemRangeValid(pOffset, sizeof(iOffset)) ||
		!xrtMemRangeValid(pPair, sizeof(Pair)) ||
		xrtMemRangesOverlap(pOffset, sizeof(*pOffset), Text.Data, Text.Size) ||
		xrtMemRangesOverlap(pPair, sizeof(*pPair), Text.Data, Text.Size) ||
		xrtMemRangesOverlap(
			pOffset, sizeof(iOffset), pPair, sizeof(Pair)
		) ) {
		__xhttpErrorSetInvalidArgument();
		return XCOOKIE_NEXT_ERROR;
	}
	memcpy(&iOffset, pOffset, sizeof(iOffset));
	if ( iOffset > Text.Size ) {
		__xhttpErrorSetRange();
		return XCOOKIE_NEXT_ERROR;
	}
	if ( iOffset == Text.Size ) {
		return XCOOKIE_NEXT_END;
	}
	if ( !__xrtCookiePairParse(Text, iOffset, &Pair, &iNext) ) {
		__xhttpErrorSetValue();
		return XCOOKIE_NEXT_ERROR;
	}
	memcpy(pPair, &Pair, sizeof(Pair));
	memcpy(pOffset, &iNext, sizeof(iNext));
	return XCOOKIE_NEXT_ITEM;
}



/* 完整扫描 Cookie 字段并应用显式资源限额。 */
XRT_API bool xrtCookieValidate(
	xstrview Text,
	const xcookielimits* pLimits,
	size_t* pCount
)
{
	xcookielimits Limits;
	xcookiepair Pair;
	size_t iOffset = 0;
	size_t iCount = 0;
	bool bLimits = pLimits != NULL;

	if ( !__xhttpCookieViewValid(Text) ||
		(bLimits && !xrtMemRangeValid(pLimits, sizeof(Limits))) ||
		((pCount != NULL) && (!xrtMemRangeValid(
			pCount, sizeof(iCount)
		) || xrtMemRangesOverlap(
			pCount, sizeof(*pCount), Text.Data, Text.Size
		) || (bLimits && xrtMemRangesOverlap(
			pCount, sizeof(iCount), pLimits, sizeof(Limits)
		)))) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	if ( bLimits ) {
		memcpy(&Limits, pLimits, sizeof(Limits));
	} else {
		memset(&Limits, 0, sizeof(Limits));
	}
	if ( (Limits.MaxBytes != 0) &&
		(Text.Size > Limits.MaxBytes) ) {
		__xhttpErrorSetRange();
		return false;
	}
	while ( iOffset < Text.Size ) {
		size_t iNext;

		if ( !__xrtCookiePairParse(
			Text, iOffset, &Pair, &iNext
		) ) {
			__xhttpErrorSetValue();
			return false;
		}
		iOffset = iNext;
		if ( ((Limits.MaxPairs != 0) &&
			  (iCount >= Limits.MaxPairs)) ||
			 ((Limits.MaxName != 0) &&
			  (Pair.Name.Size > Limits.MaxName)) ||
			 ((Limits.MaxValue != 0) &&
			  (Pair.Value.Size > Limits.MaxValue)) ) {
			__xhttpErrorSetRange();
			return false;
		}
		if ( iCount == SIZE_MAX ) {
			__xhttpErrorSetSizeOverflow();
			return false;
		}
		iCount++;
	}
	if ( pCount != NULL ) {
		memcpy(pCount, &iCount, sizeof(iCount));
	}
	return true;
}



/* 完整校验后查找下一个同名 Cookie。 */
XRT_API xcookienext xrtCookieFind(
	xstrview Text,
	xstrview Name,
	size_t* pOffset,
	xcookiepair* pPair
)
{
	xcookiepair Pair;
	size_t iOffset;

	if ( !__xhttpCookieViewValid(Text) ||
		!__xhttpCookieViewValid(Name) ||
		!xrtMemRangeValid(pOffset, sizeof(iOffset)) ||
		!xrtMemRangeValid(pPair, sizeof(Pair)) ||
		xrtMemRangesOverlap(pOffset, sizeof(*pOffset), Text.Data, Text.Size) ||
		xrtMemRangesOverlap(pPair, sizeof(*pPair), Text.Data, Text.Size) ||
		xrtMemRangesOverlap(pOffset, sizeof(iOffset), Name.Data, Name.Size) ||
		xrtMemRangesOverlap(pPair, sizeof(Pair), Name.Data, Name.Size) ||
		xrtMemRangesOverlap(
			pOffset, sizeof(iOffset), pPair, sizeof(Pair)
		) ) {
		__xhttpErrorSetInvalidArgument();
		return XCOOKIE_NEXT_ERROR;
	}
	if ( !xrtHttpTokenValid(Name) ) {
		__xhttpErrorSetValue();
		return XCOOKIE_NEXT_ERROR;
	}
	memcpy(&iOffset, pOffset, sizeof(iOffset));
	if ( iOffset > Text.Size ) {
		__xhttpErrorSetRange();
		return XCOOKIE_NEXT_ERROR;
	}
	if ( !xrtCookieValidate(Text, NULL, NULL) ) {
		return XCOOKIE_NEXT_ERROR;
	}
	while ( iOffset < Text.Size ) {
		if ( xrtCookieNext(Text, &iOffset, &Pair) != XCOOKIE_NEXT_ITEM ) {
			return XCOOKIE_NEXT_ERROR;
		}
		if ( (Pair.Name.Size == Name.Size) &&
			(memcmp(Pair.Name.Data, Name.Data, Name.Size) == 0) ) {
			memcpy(pPair, &Pair, sizeof(Pair));
			memcpy(pOffset, &iOffset, sizeof(iOffset));
			return XCOOKIE_NEXT_ITEM;
		}
	}
	memcpy(pOffset, &iOffset, sizeof(iOffset));
	return XCOOKIE_NEXT_END;
}



/* 完整预检后把借用 Cookie pair 写入调用方数组。 */
XRT_API bool xrtCookieParse(
	xstrview Text,
	xcookiepair* pPairs,
	size_t iCapacity,
	size_t* pCount,
	const xcookielimits* pLimits
)
{
	xcookiepair Pair;
	size_t iRequired;
	size_t iOffset = 0;
	size_t iIndex = 0;

	if ( !xrtMemRangeValid(pCount, sizeof(iRequired)) ||
		((pPairs == NULL) && (iCapacity != 0)) ||
		!__xhttpCookieViewValid(Text) ||
		(iCapacity > (SIZE_MAX / sizeof(*pPairs))) ||
		!xrtMemRangeValid(
			pPairs, iCapacity * sizeof(*pPairs)
		) || ((pLimits != NULL) && !xrtMemRangeValid(
			pLimits, sizeof(*pLimits)
		)) ||
		xrtMemRangesOverlap(pCount, sizeof(*pCount), Text.Data, Text.Size) ||
		xrtMemRangesOverlap(
			pPairs, iCapacity * sizeof(*pPairs), Text.Data, Text.Size
		) || xrtMemRangesOverlap(
			pCount, sizeof(*pCount), pPairs,
			iCapacity * sizeof(*pPairs)
		) || ((pLimits != NULL) && (xrtMemRangesOverlap(
			pCount, sizeof(*pCount), pLimits, sizeof(*pLimits)
		) || xrtMemRangesOverlap(
			pPairs, iCapacity * sizeof(*pPairs),
			pLimits, sizeof(*pLimits)
		))) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtCookieValidate(Text, pLimits, &iRequired) ) {
		return false;
	}
	if ( pPairs == NULL ) {
		memcpy(pCount, &iRequired, sizeof(iRequired));
		return true;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pCount, &iRequired, sizeof(iRequired));
		__xhttpErrorSetRange();
		return false;
	}
	while ( iOffset < Text.Size ) {
		size_t iNext;

		if ( !__xrtCookiePairParse(
			Text, iOffset, &Pair, &iNext
		) ) {
			__xhttpErrorSetInternal();
			return false;
		}
		iOffset = iNext;
		memcpy(
			(uint8*)pPairs + (iIndex * sizeof(Pair)),
			&Pair,
			sizeof(Pair)
		);
		iIndex++;
	}
	memcpy(pCount, &iRequired, sizeof(iRequired));
	return true;
}



/* 验证全部待写 pair 并计算规范 Cookie 字段值长度。 */
static bool __xrtCookieMeasure(
	const xcookiepair* pPairs,
	size_t iCount,
	size_t* pRequired
)
{
	xcookiepair Pair;
	size_t iSize = 0;
	size_t i;

	if ( ((pPairs == NULL) && (iCount != 0)) ||
		(iCount > (SIZE_MAX / sizeof(*pPairs))) ||
		!xrtMemRangeValid(
			pPairs, iCount * sizeof(*pPairs)
		) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		size_t iAdd;

		memcpy(
			&Pair,
			(const uint8*)pPairs + (i * sizeof(Pair)),
			sizeof(Pair)
		);
		if ( !xrtHttpTokenValid(Pair.Name) ||
			!__xhttpCookieValueValid(Pair.Value) ) {
			__xhttpErrorSetValue();
			return false;
		}
		if ( Pair.Name.Size > (SIZE_MAX - 1u) ) {
			__xhttpErrorSetSizeOverflow();
			return false;
		}
		iAdd = Pair.Name.Size + 1u;
		if ( iAdd > (SIZE_MAX - Pair.Value.Size) ) {
			__xhttpErrorSetSizeOverflow();
			return false;
		}
		iAdd += Pair.Value.Size;
		if ( i != 0 ) {
			if ( iAdd > (SIZE_MAX - 2u) ) {
				__xhttpErrorSetSizeOverflow();
				return false;
			}
			iAdd += 2u;
		}
		if ( iSize > (SIZE_MAX - iAdd) ) {
			__xhttpErrorSetSizeOverflow();
			return false;
		}
		iSize += iAdd;
	}
	*pRequired = iSize;
	return true;
}



/* 写出不含字段名和零结尾的规范 Cookie 字段值。 */
XRT_API bool xrtCookieWrite(
	const xcookiepair* pPairs,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	uint8* pWrite = (uint8*)pOutput;
	xcookiepair Pair;
	size_t iRequired;
	size_t iOffset = 0;
	size_t i;

	if ( !xrtMemRangeValid(pSize, sizeof(iRequired)) ||
		((pOutput == NULL) && (iCapacity != 0)) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtCookieMeasure(pPairs, iCount, &iRequired) ) {
		return false;
	}
	if ( __xhttpCookiePairsOverlap(
		pPairs, iCount, pSize, sizeof(*pSize)
	) || ((pOutput != NULL) && xrtMemRangesOverlap(
		pSize, sizeof(*pSize), pOutput, iRequired
	)) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		__xhttpErrorSetRange();
		return false;
	}
	if ( __xhttpCookiePairsOverlap(pPairs, iCount, pOutput, iRequired) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		memcpy(
			&Pair,
			(const uint8*)pPairs + (i * sizeof(Pair)),
			sizeof(Pair)
		);
		if ( i != 0 ) {
			pWrite[iOffset++] = (uint8)';';
			pWrite[iOffset++] = (uint8)' ';
		}
		memcpy(pWrite + iOffset, Pair.Name.Data, Pair.Name.Size);
		iOffset += Pair.Name.Size;
		pWrite[iOffset++] = (uint8)'=';
		if ( Pair.Value.Size != 0 ) {
			memcpy(
				pWrite + iOffset,
				Pair.Value.Data,
				Pair.Value.Size
			);
			iOffset += Pair.Value.Size;
		}
	}
	memcpy(pSize, &iRequired, sizeof(iRequired));
	return true;
}



/* 分配并构建零结尾 Cookie 字段值。 */
XRT_API str xrtCookieBuild(
	const xcookiepair* pPairs,
	size_t iCount,
	size_t* pSize
)
{
	str sOutput;
	size_t iRequired;

	if ( ((pSize != NULL) && !xrtMemRangeValid(
		pSize, sizeof(iRequired)
	)) || ((pSize != NULL) && (pPairs != NULL) &&
		(iCount <= (SIZE_MAX / sizeof(*pPairs))) &&
		__xhttpCookiePairsOverlap(
			pPairs, iCount, pSize, sizeof(*pSize)
		)) ) {
		__xhttpErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtCookieWrite(pPairs, iCount, NULL, 0, &iRequired) ||
		(iRequired == SIZE_MAX) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtCookieWrite(
		pPairs, iCount, sOutput, iRequired, &iRequired
	) ) {
		xrtFree(sOutput);
		return NULL;
	}
	sOutput[iRequired] = '\0';
	if ( pSize != NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
	}
	return sOutput;
}

#endif
