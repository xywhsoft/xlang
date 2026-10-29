#include "../internal/xrt_mime.h"



#if defined(XHTTP_FEATURE_MIME)

/* 安全累加线缆长度。 */
bool __xrtMimeSizeAdd(size_t* pSize, size_t iAdd)
{
	if ( (pSize == NULL) || (*pSize > (SIZE_MAX - iAdd)) ) {
		__xhttpErrorSetSizeOverflow();
		return false;
	}
	*pSize += iAdd;
	return true;
}



/* 检查一个参数名称是否已经在当前项之前出现。 */
static bool __xrtMimeParamSeen(
	xstrview Parameters,
	size_t iBefore,
	xstrview Name
)
{
	xhttpparam Param;
	xhttpnext Next;
	size_t iOffset = 0;

	while ( iOffset < iBefore ) {
		Next = xrtHttpParamNext(
			Parameters, &iOffset, &Param
		);
		if ( Next != XHTTP_NEXT_ITEM ) {
			return false;
		}
		if ( xrtHttpTokenEqual(Param.Name, Name) ) {
			return true;
		}
	}
	return false;
}



/* 计算大小写不敏感参数名称的稳定哈希。 */
static uint64 __xrtMimeParamHash(xstrview Name)
{
	uint64 iHash = UINT64_C(1469598103934665603);
	size_t i;

	for ( i = 0; i < Name.Size; i++ ) {
		uint8 iByte = (uint8)Name.Data[i];

		if ( (iByte >= (uint8)'A') && (iByte <= (uint8)'Z') ) {
			iByte = (uint8)(iByte + ((uint8)'a' - (uint8)'A'));
		}
		iHash ^= iByte;
		iHash *= UINT64_C(1099511628211);
	}
	return iHash;
}



/* 验证并扫描媒体类型与 Content-Disposition 共用的严格参数列表。 */
bool __xrtMimeParametersInspect(
	xstrview Parameters,
	xrt_mime_param_visitor Visitor,
	void* pContext
)
{
	uint64 arrSeen[16] = { 0 };
	xhttpparam Param;
	xhttpnext Next;
	size_t iOffset = 0;
	size_t iBefore;

	if ( !__xhttpViewValid(Parameters) ||
		(Parameters.Size == 0) ) {
		return false;
	}
	for ( ;; ) {
		iBefore = iOffset;
		Next = xrtHttpParamNext(
			Parameters, &iOffset, &Param
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			return true;
		}
		if ( (Param.Flags & XHTTP_PARAM_HAS_VALUE) == 0 ) {
			__xhttpErrorSetValue();
			return false;
		}
		{
			uint64 iHash = __xrtMimeParamHash(Param.Name);
			size_t iWord = (size_t)((iHash >> 6u) & UINT64_C(15));
			uint64 iBit = UINT64_C(1) << (iHash & UINT64_C(63));

			if ( ((arrSeen[iWord] & iBit) != 0) &&
				__xrtMimeParamSeen(
				Parameters, iBefore, Param.Name
				) ) {
				__xhttpErrorSetValue();
				return false;
			}
			arrSeen[iWord] |= iBit;
		}
		if ( (Visitor != NULL) &&
			!Visitor(&Param, pContext) ) {
			return false;
		}
	}
}



/* 验证媒体类型与 Content-Disposition 共用的严格参数列表。 */
bool __xrtMimeParametersValid(xstrview Parameters)
{
	return __xrtMimeParametersInspect(Parameters, NULL, NULL);
}



/* 验证媒体类型结构可安全写回线缆。 */
bool __xrtHttpMediaTypeValid(const xmediatype* pType)
{
	if ( (pType == NULL) ||
		!xrtHttpTokenValid(pType->Type) ||
		!xrtHttpTokenValid(pType->Subtype) ||
		!__xhttpViewValid(pType->Parameters) ) {
		return false;
	}
	return (pType->Parameters.Size == 0) ||
		__xrtMimeParametersValid(pType->Parameters);
}



/* 判断输出区是否覆盖任一借用视图。 */
bool __xrtMimeOutputOverlap(
	const xstrview* pViews,
	size_t iCount,
	const void* pOutput,
	size_t iSize
)
{
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		if ( xrtMemRangesOverlap(
			pViews[i].Data, pViews[i].Size, pOutput, iSize
		) ) {
			return true;
		}
	}
	return false;
}



/* 为同构 Build 函数分配带零结尾的结果。 */
str __xrtMimeBuild(
	const void* pValue,
	bool (*Write)(const void*, void*, size_t, size_t*),
	size_t* pSize
)
{
	str sOutput;
	size_t iRequired;

	if ( !Write(pValue, NULL, 0, &iRequired) ||
		(iRequired == SIZE_MAX) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !Write(pValue, sOutput, iRequired, &iRequired) ) {
		xrtFree(sOutput);
		return NULL;
	}
	sOutput[iRequired] = '\0';
	if ( pSize != NULL ) {
		*pSize = iRequired;
	}
	return sOutput;
}



/* 严格解析媒体类型并报告参数或值错误。 */
XRT_API bool xrtHttpMediaTypeParse(
	xstrview Text,
	xmediatype* pType
)
{
	xmediatype Type;
	xstrview Main;
	xstrview Parameters = { NULL, 0 };
	cstr sSlash;
	cstr sSemi;

	if ( (pType == NULL) || !__xhttpViewValid(Text) ||
		xrtMemRangesOverlap(
			pType, sizeof(*pType), Text.Data, Text.Size
		) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	memset(pType, 0, sizeof(*pType));
	Text = xrtHttpOwsTrim(Text);
	if ( Text.Size == 0 ) {
		__xhttpErrorSetValue();
		return false;
	}
	sSemi = (cstr)memchr(Text.Data, ';', Text.Size);
	if ( sSemi == NULL ) {
		Main = Text;
	} else {
		Main = (xstrview){
			Text.Data, (size_t)(sSemi - Text.Data)
		};
		Parameters = xrtHttpOwsTrim((xstrview){
			sSemi + 1,
			Text.Size - (size_t)(sSemi + 1 - Text.Data)
		});
	}
	if ( (sSemi != NULL) && (Parameters.Size == 0) ) {
		__xhttpErrorSetValue();
		return false;
	}
	Main = xrtHttpOwsTrim(Main);
	sSlash = (cstr)memchr(Main.Data, '/', Main.Size);
	if ( (sSlash == NULL) ||
		(memchr(
			sSlash + 1, '/',
			Main.Size - (size_t)(sSlash + 1 - Main.Data)
		) != NULL) ) {
		__xhttpErrorSetValue();
		return false;
	}
	Type.Type = (xstrview){
		Main.Data, (size_t)(sSlash - Main.Data)
	};
	Type.Subtype = (xstrview){
		sSlash + 1,
		Main.Size - (size_t)(sSlash + 1 - Main.Data)
	};
	Type.Parameters = Parameters;
	if ( !__xrtHttpMediaTypeValid(&Type) ) {
		__xhttpErrorSetValue();
		return false;
	}
	*pType = Type;
	return true;
}



/* 按 MIME Sniff 算法静默解析 essence，参数尾不参与成败判定。 */
bool __xrtMimeSniffTypeParse(
	xstrview Text,
	xmediatype* pType
)
{
	xmediatype Type = { 0 };
	xstrview Main;
	cstr sSemi;
	cstr sSlash;

	if ( (pType == NULL) || !__xhttpViewValid(Text) ||
		xrtMemRangesOverlap(
			pType, sizeof(*pType), Text.Data, Text.Size
		) ) {
		return false;
	}
	memset(pType, 0, sizeof(*pType));
	Text = xrtHttpOwsTrim(Text);
	if ( Text.Size == 0 ) {
		return false;
	}

	/* MIME Sniff 只要求 type/subtype 合法，之后的参数可被逐项忽略。 */
	sSemi = (cstr)memchr(Text.Data, ';', Text.Size);
	Main = (sSemi == NULL) ? Text : (xstrview){
		Text.Data, (size_t)(sSemi - Text.Data)
	};
	sSlash = (cstr)memchr(Main.Data, '/', Main.Size);
	if ( sSlash == NULL ) {
		return false;
	}
	Type.Type = (xstrview){
		Main.Data, (size_t)(sSlash - Main.Data)
	};
	Type.Subtype = xrtHttpOwsTrim((xstrview){
		sSlash + 1,
		Main.Size - (size_t)(sSlash + 1 - Main.Data)
	});
	if ( !xrtHttpTokenValid(Type.Type) ||
		!xrtHttpTokenValid(Type.Subtype) ) {
		return false;
	}
	*pType = Type;
	return true;
}



/* 比较媒体类型主值，不比较参数。 */
XRT_API bool xrtHttpMediaTypeEqual(
	const xmediatype* pType,
	xstrview Type,
	xstrview Subtype
)
{
	if ( !__xrtHttpMediaTypeValid(pType) ||
		!xrtHttpTokenValid(Type) ||
		!xrtHttpTokenValid(Subtype) ) {
		return false;
	}
	return xrtHttpTokenEqual(pType->Type, Type) &&
		xrtHttpTokenEqual(pType->Subtype, Subtype);
}



/* 返回结构化语法后缀。 */
XRT_API xstrview xrtHttpMediaTypeSuffix(
	const xmediatype* pType
)
{
	size_t i;

	if ( (pType == NULL) ||
		!__xhttpViewValid(pType->Subtype) ) {
		__xhttpErrorSetInvalidArgument();
		return (xstrview){ NULL, 0 };
	}
	for ( i = pType->Subtype.Size; i != 0; i-- ) {
		if ( pType->Subtype.Data[i - 1u] == '+' ) {
			if ( (i == 1u) || (i == pType->Subtype.Size) ) {
				return (xstrview){ NULL, 0 };
			}
			return (xstrview){
				pType->Subtype.Data + i,
				pType->Subtype.Size - i
			};
		}
	}
	return (xstrview){ NULL, 0 };
}



/* 按 ASCII 大小写不敏感规则比较媒体类型 token 和静态文本。 */
static bool __xrtMimeTokenIs(
	xstrview Token,
	cstr sText,
	size_t iSize
)
{
	return xrtHttpTokenEqual(
		Token, (xstrview){ sText, iSize }
	);
}



/* 判断 application 子类型是否属于适合通用压缩的已知格式。 */
static bool __xrtMimeApplicationCompressible(
	xstrview Subtype
)
{
	static const struct {
		cstr Text;
		uint8 Size;
	} Types[] = {
		{ "json", 4 },
		{ "xml", 3 },
		{ "javascript", 10 },
		{ "x-javascript", 12 },
		{ "ecmascript", 10 },
		{ "wasm", 4 },
		{ "sql", 3 },
		{ "rtf", 3 },
		{ "toml", 4 },
		{ "yaml", 4 },
		{ "x-yaml", 6 },
		{ "graphql", 7 },
		{ "font-sfnt", 9 },
		{ "vnd.ms-fontobject", 17 }
	};
	size_t i;

	for ( i = 0; i < (sizeof(Types) / sizeof(Types[0])); i++ ) {
		if ( __xrtMimeTokenIs(
			Subtype, Types[i].Text, Types[i].Size
		) ) {
			return true;
		}
	}
	return false;
}



/* 判断媒体类型是否适合 gzip 或 deflate 等通用内容编码。 */
XRT_API bool xrtHttpMediaTypeCompressible(
	const xmediatype* pType
)
{
	xstrview Suffix;

	if ( !__xrtHttpMediaTypeValid(pType) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtMimeTokenIs(pType->Type, "text", 4) ) {
		return true;
	}
	Suffix = xrtHttpMediaTypeSuffix(pType);
	if ( __xrtMimeTokenIs(Suffix, "json", 4) ||
		__xrtMimeTokenIs(Suffix, "xml", 3) ) {
		return true;
	}
	if ( __xrtMimeTokenIs(pType->Type, "application", 11) ) {
		return __xrtMimeApplicationCompressible(pType->Subtype);
	}
	if ( !__xrtMimeTokenIs(pType->Type, "font", 4) ) {
		return false;
	}
	return __xrtMimeTokenIs(pType->Subtype, "ttf", 3) ||
		__xrtMimeTokenIs(pType->Subtype, "otf", 3) ||
		__xrtMimeTokenIs(pType->Subtype, "sfnt", 4);
}



/* 解析 Content-Type 并判断是否适合通用内容编码。 */
XRT_API bool xrtHttpContentTypeCompressible(
	xstrview Text
)
{
	xmediatype Type;

	if ( !xrtHttpMediaTypeParse(Text, &Type) ) {
		return false;
	}
	return xrtHttpMediaTypeCompressible(&Type);
}



/* 查找媒体类型参数。 */
XRT_API xhttpnext xrtHttpMediaTypeParam(
	const xmediatype* pType,
	xstrview Name,
	xhttpparam* pParam
)
{
	if ( !__xrtHttpMediaTypeValid(pType) ||
		(pParam == NULL) ) {
		__xhttpErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( pType->Parameters.Size == 0 ) {
		memset(pParam, 0, sizeof(*pParam));
		return XHTTP_NEXT_END;
	}
	return xrtHttpParamFind(pType->Parameters, Name, pParam);
}



/* 写出媒体类型。 */
XRT_API bool xrtHttpMediaTypeWrite(
	const xmediatype* pType,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	uint8* pWrite = (uint8*)pOutput;
	xstrview Views[3];
	size_t iRequired;
	size_t iOffset = 0;

	if ( (pSize == NULL) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		!__xrtHttpMediaTypeValid(pType) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	Views[0] = pType->Type;
	Views[1] = pType->Subtype;
	Views[2] = pType->Parameters;
	iRequired = 0;
	if ( !__xrtMimeSizeAdd(&iRequired, pType->Type.Size) ||
		!__xrtMimeSizeAdd(&iRequired, 1u) ||
		!__xrtMimeSizeAdd(&iRequired, pType->Subtype.Size) ) {
		return false;
	}
	if ( pType->Parameters.Size != 0 ) {
		if ( !__xrtMimeSizeAdd(&iRequired, 2u) ||
			!__xrtMimeSizeAdd(
				&iRequired, pType->Parameters.Size
			) ) {
			return false;
		}
	}
	if ( __xrtMimeOutputOverlap(
		Views, 3, pSize, sizeof(*pSize)
	) || xrtMemRangesOverlap(
		pType, sizeof(*pType), pSize, sizeof(*pSize)
	) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	if ( pOutput == NULL ) {
		*pSize = iRequired;
		return true;
	}
	if ( __xrtMimeOutputOverlap(
		Views, 3, pOutput, iRequired
	) || xrtMemRangesOverlap(
		pType, sizeof(*pType), pOutput, iRequired
	) || xrtMemRangesOverlap(
		pSize, sizeof(*pSize), pOutput, iRequired
	) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	if ( iCapacity < iRequired ) {
		*pSize = iRequired;
		__xhttpErrorSetRange();
		return false;
	}
	memcpy(pWrite + iOffset, pType->Type.Data, pType->Type.Size);
	iOffset += pType->Type.Size;
	pWrite[iOffset++] = (uint8)'/';
	memcpy(pWrite + iOffset, pType->Subtype.Data, pType->Subtype.Size);
	iOffset += pType->Subtype.Size;
	if ( pType->Parameters.Size != 0 ) {
		pWrite[iOffset++] = (uint8)';';
		pWrite[iOffset++] = (uint8)' ';
		memcpy(
			pWrite + iOffset,
			pType->Parameters.Data,
			pType->Parameters.Size
		);
		iOffset += pType->Parameters.Size;
	}
	*pSize = iOffset;
	return true;
}



/* 适配媒体类型 Writer 到共享 Build 函数。 */
static bool __xrtHttpMediaTypeWriteAdapter(
	const void* pValue,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return xrtHttpMediaTypeWrite(
		(const xmediatype*)pValue,
		pOutput,
		iCapacity,
		pSize
	);
}



/* 构建零结尾媒体类型。 */
XRT_API str xrtHttpMediaTypeBuild(
	const xmediatype* pType,
	size_t* pSize
)
{
	return __xrtMimeBuild(
		pType, __xrtHttpMediaTypeWriteAdapter, pSize
	);
}



#endif
