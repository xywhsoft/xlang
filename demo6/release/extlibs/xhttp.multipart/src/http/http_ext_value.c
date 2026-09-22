#include "../internal/xrt_http.h"




#if defined(XHTTP_FEATURE_HTTP_EXT_VALUE)

/* RFC 8187 attr-char 不包含百分号，百分号只用于转义。 */
static bool __xrtHttpExtAttrByte(uint8 iByte)
{
	return ((iByte >= (uint8)'0') && (iByte <= (uint8)'9')) ||
		((iByte >= (uint8)'A') && (iByte <= (uint8)'Z')) ||
		((iByte >= (uint8)'a') && (iByte <= (uint8)'z')) ||
		(iByte == (uint8)'!') ||
		(iByte == (uint8)'#') ||
		(iByte == (uint8)'$') ||
		(iByte == (uint8)'&') ||
		(iByte == (uint8)'+') ||
		(iByte == (uint8)'-') ||
		(iByte == (uint8)'.') ||
		(iByte == (uint8)'^') ||
		(iByte == (uint8)'_') ||
		(iByte == (uint8)'`') ||
		(iByte == (uint8)'|') ||
		(iByte == (uint8)'~');
}



/* 把十六进制 ASCII 字节转换为数值。 */
static int __xrtHttpExtHex(uint8 iByte)
{
	if ( (iByte >= (uint8)'0') && (iByte <= (uint8)'9') ) {
		return (int)(iByte - (uint8)'0');
	}
	if ( (iByte >= (uint8)'A') && (iByte <= (uint8)'F') ) {
		return (int)(iByte - (uint8)'A') + 10;
	}
	if ( (iByte >= (uint8)'a') && (iByte <= (uint8)'f') ) {
		return (int)(iByte - (uint8)'a') + 10;
	}
	return -1;
}



/* 验证 RFC 8187 独立定义的 mime-charset 语法。 */
static bool __xrtHttpExtCharsetValid(xstrview Charset)
{
	if ( !xrtMemRangeValid(Charset.Data, Charset.Size) ||
		(Charset.Size == 0) ) {
		return false;
	}
	for ( size_t i = 0; i < Charset.Size; i++ ) {
		uint8 iByte = (uint8)Charset.Data[i];

		if ( ((iByte >= (uint8)'0') && (iByte <= (uint8)'9')) ||
			((iByte >= (uint8)'A') && (iByte <= (uint8)'Z')) ||
			((iByte >= (uint8)'a') && (iByte <= (uint8)'z')) ||
			(iByte == (uint8)'!') ||
			(iByte == (uint8)'#') ||
			(iByte == (uint8)'$') ||
			(iByte == (uint8)'%') ||
			(iByte == (uint8)'&') ||
			(iByte == (uint8)'+') ||
			(iByte == (uint8)'-') ||
			(iByte == (uint8)'^') ||
			(iByte == (uint8)'_') ||
			(iByte == (uint8)'`') ||
			(iByte == (uint8)'{') ||
			(iByte == (uint8)'}') ||
			(iByte == (uint8)'~') ) {
			continue;
		}
		return false;
	}
	return true;
}



/* 验证已经拆分的扩展值。 */
static bool __xrtHttpExtValueValid(const xhttpextvalue* pValue)
{
	if ( !__xrtHttpExtCharsetValid(pValue->Charset) ||
		!__xrtHttpLanguageTextValid(
			pValue->Language, false, true, NULL
		) ||
		!__xhttpViewValid(pValue->Encoded) ) {
		return false;
	}
	for ( size_t i = 0; i < pValue->Encoded.Size; i++ ) {
		uint8 iByte = (uint8)pValue->Encoded.Data[i];

		if ( __xrtHttpExtAttrByte(iByte) ) {
			continue;
		}
		if ( (iByte != (uint8)'%') ||
			(i + 2u >= pValue->Encoded.Size) ||
			(__xrtHttpExtHex(
				(uint8)pValue->Encoded.Data[i + 1u]
			) < 0) ||
			(__xrtHttpExtHex(
				(uint8)pValue->Encoded.Data[i + 2u]
			) < 0) ) {
			return false;
		}
		i += 2u;
	}
	return true;
}



/* 判断一个输出区间是否覆盖任意借用视图。 */
static bool __xrtHttpExtOutputOverlap(
	const xstrview* pViews,
	size_t iCount,
	const void* pOutput,
	size_t iSize
)
{
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( xrtMemRangesOverlap(
			pViews[i].Data, pViews[i].Size, pOutput, iSize
		) ) {
			return true;
		}
	}
	return false;
}



/* 安全累加线路长度。 */
static bool __xrtHttpExtSizeAdd(size_t* pSize, size_t iAdd)
{
	if ( *pSize > (SIZE_MAX - iAdd) ) {
		__xhttpErrorSetSizeOverflow();
		return false;
	}
	*pSize += iAdd;
	return true;
}



/* 不发布错误地拆分 RFC 8187 扩展值，供可回退路径使用。 */
bool __xrtHttpExtValueSplit(
	xstrview Text,
	xhttpextvalue* pValue
)
{
	xhttpextvalue Value = { 0 };
	size_t iFirst = XRT_NPOS;
	size_t iSecond = XRT_NPOS;

	if ( !__xhttpViewValid(Text) ||
		!xrtMemRangeValid(pValue, sizeof(Value)) ||
		xrtMemRangesOverlap(
			pValue, sizeof(Value), Text.Data, Text.Size
		) ) {
		return false;
	}
	for ( size_t i = 0; i < Text.Size; i++ ) {
		if ( Text.Data[i] != '\'' ) {
			continue;
		}
		if ( iFirst == XRT_NPOS ) {
			iFirst = i;
		} else {
			iSecond = i;
			break;
		}
	}
	if ( (iFirst == XRT_NPOS) || (iFirst == 0) ||
		(iSecond == XRT_NPOS) ) {
		return false;
	}
	Value.Charset = (xstrview){ Text.Data, iFirst };
	Value.Language = (xstrview){
		Text.Data + iFirst + 1u, iSecond - iFirst - 1u
	};
	Value.Encoded = (xstrview){
		Text.Data + iSecond + 1u, Text.Size - iSecond - 1u
	};
	if ( !__xrtHttpExtValueValid(&Value) ) {
		return false;
	}
	memcpy(pValue, &Value, sizeof(Value));
	return true;
}



/* 严格解析 RFC 8187 扩展值并原子发布借用视图。 */
XRT_API bool xrtHttpExtValueParse(
	xstrview Text,
	xhttpextvalue* pValue
)
{
	xhttpextvalue Empty = { 0 };

	if ( !__xhttpViewValid(Text) ||
		!xrtMemRangeValid(pValue, sizeof(Empty)) ||
		xrtMemRangesOverlap(
			pValue, sizeof(Empty), Text.Data, Text.Size
		) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	memcpy(pValue, &Empty, sizeof(Empty));
	if ( !__xrtHttpExtValueSplit(Text, pValue) ) {
		__xhttpErrorSetValue();
		return false;
	}
	return true;
}



/* 严格解码 RFC 8187 扩展值。 */
XRT_API bool xrtHttpExtValueRead(
	const xhttpextvalue* pInput,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpextvalue Value;
	xstrview Views[3];

	if ( !xrtMemRangeValid(pInput, sizeof(Value)) ||
		!xrtMemRangeValid(pSize, sizeof(size_t)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		xrtMemRangesOverlap(
			pInput, sizeof(Value), pSize, sizeof(size_t)
		) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Value, pInput, sizeof(Value));
	if ( !__xrtHttpExtValueValid(&Value) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	Views[0] = Value.Charset;
	Views[1] = Value.Language;
	Views[2] = Value.Encoded;
	if ( __xrtHttpExtOutputOverlap(
		Views, 3u, pSize, sizeof(size_t)
	) || ((pOutput != NULL) && (xrtMemRangesOverlap(
		pInput, sizeof(Value), pOutput, iCapacity
	) || __xrtHttpExtOutputOverlap(
		Views, 3u, pOutput, iCapacity
	))) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	return xrtPercentDecode(
		Value.Encoded, pOutput, iCapacity, pSize
	);
}



/* 写出 RFC 8187 扩展值。 */
XRT_API bool xrtHttpExtValueWrite(
	xstrview Charset,
	xstrview Language,
	xbytesview Value,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	static const char Hex[] = "0123456789ABCDEF";
	static const xstrview Utf8 = XRT_STR_INIT("UTF-8");
	uint8* pWrite = (uint8*)pOutput;
	xstrview Views[2];
	size_t iEncoded = 0;
	size_t iRequired = 0;
	size_t iOffset = 0;

	if ( Charset.Size == 0 ) {
		Charset = Utf8;
	}
	if ( !xrtMemRangeValid(pSize, sizeof(size_t)) ||
		!xrtMemRangeValid(pOutput, iCapacity) ||
		!__xrtHttpExtCharsetValid(Charset) ||
		!__xrtHttpLanguageTextValid(
			Language, false, true, NULL
		) ||
		!xrtMemRangeValid(Value.Data, Value.Size) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	for ( size_t i = 0; i < Value.Size; i++ ) {
		size_t iAdd = __xrtHttpExtAttrByte(Value.Data[i]) ? 1u : 3u;

		if ( iEncoded > (SIZE_MAX - iAdd) ) {
			__xhttpErrorSetSizeOverflow();
			return false;
		}
		iEncoded += iAdd;
	}
	if ( !__xrtHttpExtSizeAdd(&iRequired, Charset.Size) ||
		!__xrtHttpExtSizeAdd(&iRequired, 1u) ||
		!__xrtHttpExtSizeAdd(&iRequired, Language.Size) ||
		!__xrtHttpExtSizeAdd(&iRequired, 1u) ||
		!__xrtHttpExtSizeAdd(&iRequired, iEncoded) ) {
		return false;
	}
	Views[0] = Charset;
	Views[1] = Language;
	if ( __xrtHttpExtOutputOverlap(
		Views, 2u, pSize, sizeof(size_t)
	) || xrtMemRangesOverlap(
		Value.Data, Value.Size, pSize, sizeof(size_t)
	) || ((pOutput != NULL) && xrtMemRangesOverlap(
		pOutput, iCapacity, pSize, sizeof(size_t)
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
	if ( __xrtHttpExtOutputOverlap(
		Views, 2u, pOutput, iRequired
		) || xrtMemRangesOverlap(
		Value.Data, Value.Size, pOutput, iRequired
		) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	memcpy(pWrite + iOffset, Charset.Data, Charset.Size);
	iOffset += Charset.Size;
	pWrite[iOffset++] = (uint8)'\'';
	memcpy(pWrite + iOffset, Language.Data, Language.Size);
	iOffset += Language.Size;
	pWrite[iOffset++] = (uint8)'\'';
	for ( size_t i = 0; i < Value.Size; i++ ) {
		uint8 iByte = Value.Data[i];

		if ( __xrtHttpExtAttrByte(iByte) ) {
			pWrite[iOffset++] = iByte;
		} else {
			pWrite[iOffset++] = (uint8)'%';
			pWrite[iOffset++] = (uint8)Hex[iByte >> 4u];
			pWrite[iOffset++] = (uint8)Hex[iByte & UINT8_C(0x0F)];
		}
	}
	memcpy(pSize, &iOffset, sizeof(iOffset));
	return true;
}



/* 构建零结尾 RFC 8187 扩展值。 */
XRT_API str xrtHttpExtValueBuild(
	xstrview Charset,
	xstrview Language,
	xbytesview Value,
	size_t* pSize
)
{
	str sOutput;
	size_t iRequired;

	if ( ((pSize != NULL) && !xrtMemRangeValid(
		pSize, sizeof(iRequired)
	)) || ((pSize != NULL) && (xrtMemRangesOverlap(
		pSize, sizeof(iRequired), Charset.Data, Charset.Size
	) || xrtMemRangesOverlap(
		pSize, sizeof(iRequired), Language.Data, Language.Size
	) || xrtMemRangesOverlap(
		pSize, sizeof(iRequired), Value.Data, Value.Size
	))) ) {
		__xhttpErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtHttpExtValueWrite(
		Charset, Language, Value, NULL, 0, &iRequired
	) || (iRequired == SIZE_MAX) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtHttpExtValueWrite(
		Charset, Language, Value, sOutput, iRequired, &iRequired
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

