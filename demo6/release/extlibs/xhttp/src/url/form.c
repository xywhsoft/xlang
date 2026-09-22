#include "../internal/xrt_form.h"



#if defined(XHTTP_FEATURE_FORM_URLENCODED)

static const char __xrtFormSafe[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789*-._";



/* 初始化 WHATWG form-urlencoded 的完整 ASCII 安全字符位图。 */
bool __xrtFormSafeMap(xpercentmap* pSafe)
{
	return xrtPercentMapInit(
		pSafe,
		(xstrview){ __xrtFormSafe, sizeof(__xrtFormSafe) - 1u },
		false
	);
}



/* 验证借用二进制视图的空值一致性。 */
static bool __xrtFormViewValid(xbytesview Data)
{
	if ( (Data.Data == NULL) && (Data.Size != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 验证全部字段并计算序列化后的精确字节数。 */
static bool __xrtFormMeasure(
	const xformfield* pFields,
	size_t iCount,
	const xpercentmap* pSafe,
	size_t* pRequired
)
{
	size_t iRequired = 0;
	size_t i;

	if ( (pFields == NULL) && (iCount != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iCount > (SIZE_MAX / sizeof(*pFields)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		size_t iName;
		size_t iValue;
		size_t iAdd;

		if ( !__xrtFormViewValid(pFields[i].Name) ||
			!__xrtFormViewValid(pFields[i].Value) ) {
			return false;
		}
		if ( !xrtPercentMeasure(
			pFields[i].Name.Data, pFields[i].Name.Size,
			pSafe, true, &iName
		) || !xrtPercentMeasure(
			pFields[i].Value.Data, pFields[i].Value.Size,
			pSafe, true, &iValue
		) ) {
			return false;
		}
		if ( iName > (SIZE_MAX - 1u) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iAdd = iName + 1u;
		if ( iAdd > (SIZE_MAX - iValue) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iAdd += iValue;
		if ( i != 0 ) {
			if ( iAdd == SIZE_MAX ) {
				__xrtErrorSetSizeOverflow();
				return false;
			}
			iAdd++;
		}
		if ( iRequired > (SIZE_MAX - iAdd) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iRequired += iAdd;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pRequired = iRequired;
	return true;
}



/* 判断元数据范围是否覆盖字段数组或任一借用字节。 */
static bool __xrtFormMetadataOverlap(
	const xformfield* pFields,
	size_t iCount,
	const void* pMetadata,
	size_t iMetadataSize
)
{
	size_t i;

	if ( __xrtRangesOverlap(
		pMetadata, iMetadataSize,
		pFields, iCount * sizeof(*pFields)
	) ) {
		return true;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( __xrtRangesOverlap(
			pMetadata, iMetadataSize,
			pFields[i].Name.Data, pFields[i].Name.Size
		) || __xrtRangesOverlap(
			pMetadata, iMetadataSize,
			pFields[i].Value.Data, pFields[i].Value.Size
		) ) {
			return true;
		}
	}
	return false;
}



/* 预检全部字段、percent 转义、解码长度和显式资源限额。 */
static bool __xrtFormPreflight(
	xstrview Text,
	const xformlimits* pLimits,
	size_t* pCount
)
{
	xquerypair Pair;
	xquerynext Next;
	size_t iOffset = 0;
	size_t iCount = 0;
	size_t iBytes = 0;

	for ( ;; ) {
		size_t iName;
		size_t iValue;

		Next = __xrtQueryNext(Text, &iOffset, &Pair, false);
		if ( Next == XQUERY_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XQUERY_NEXT_END ) {
			*pCount = iCount;
			return true;
		}
		if ( !xrtPercentDecodeMeasure(
			Pair.Key, true, &iName
		) || !xrtPercentDecodeMeasure(
			Pair.Value, true, &iValue
		) ) {
			return false;
		}
		if ( iBytes > (SIZE_MAX - iName) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iBytes += iName;
		if ( iBytes > (SIZE_MAX - iValue) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iBytes += iValue;
		if ( (pLimits != NULL) &&
			(((pLimits->MaxFields != 0) &&
			  (iCount >= pLimits->MaxFields)) ||
			 ((pLimits->MaxName != 0) &&
			  (iName > pLimits->MaxName)) ||
			 ((pLimits->MaxValue != 0) &&
			  (iValue > pLimits->MaxValue)) ||
			 ((pLimits->MaxBytes != 0) &&
			  (iBytes > pLimits->MaxBytes))) ) {
			__xrtErrorSetRange();
			return false;
		}
		if ( iCount == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iCount++;
	}
}



/* 按 WHATWG form-urlencoded 字节规则编码。 */
XRT_API bool xrtFormEncode(
	const void* pData,
	size_t iSize,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	xpercentmap Safe;
	size_t iRequired;
	size_t iWriteSize;

	if ( !__xrtRangeValid(pData, iSize) ||
		 !__xrtRangeValid(sOutput, iCapacity) ||
		 !__xrtRangeValid(pOutputSize, sizeof(*pOutputSize)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtFormSafeMap(&Safe) ||
		 !xrtPercentMeasure(
			pData, iSize, &Safe, true, &iRequired
		 ) ) {
		return false;
	}
	iWriteSize = iRequired + 1u;
	if ( __xrtRangesOverlap(
		pOutputSize, sizeof(*pOutputSize), pData, iSize
	) || ((sOutput != NULL) && __xrtRangesOverlap(
		pOutputSize, sizeof(*pOutputSize), sOutput, iCapacity
	)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( sOutput == NULL ) {
		*pOutputSize = iRequired;
		return true;
	}
	if ( iCapacity < iWriteSize ) {
		*pOutputSize = iRequired;
		__xrtErrorSetRange();
		return false;
	}
	if ( __xrtRangesOverlap(
		sOutput, iWriteSize, pData, iSize
	) && ((const void*)sOutput != pData) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	xrtPercentEncodeMeasured(
		pData, iSize, &Safe, true,
		sOutput, iRequired, true
	);
	*pOutputSize = iRequired;
	return true;
}



/* 严格解码 form-urlencoded 字节并把加号转换为空格。 */
XRT_API bool xrtFormDecode(
	xstrview Text,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;

	if ( !__xrtRangeValid(Text.Data, Text.Size) ||
		 !__xrtRangeValid(pOutput, iCapacity) ||
		 !__xrtRangeValid(pOutputSize, sizeof(*pOutputSize)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtPercentDecodeMeasure(
		Text, true, &iRequired
	) ) {
		return false;
	}
	if ( __xrtRangesOverlap(
		pOutputSize, sizeof(*pOutputSize), Text.Data, Text.Size
	) || ((pOutput != NULL) && __xrtRangesOverlap(
		pOutputSize, sizeof(*pOutputSize), pOutput, iCapacity
	)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pOutput == NULL ) {
		*pOutputSize = iRequired;
		return true;
	}
	if ( iCapacity < iRequired ) {
		*pOutputSize = iRequired;
		__xrtErrorSetRange();
		return false;
	}
	if ( __xrtRangesOverlap(
		pOutput, iRequired, Text.Data, Text.Size
	) && (pOutput != (const void*)Text.Data) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iRequired != 0 ) {
		(void)xrtPercentDecodeMeasured(
			Text, true, pOutput
		);
	}
	*pOutputSize = iRequired;
	return true;
}



/* 分配并返回零结尾 form-urlencoded 编码文本。 */
XRT_API str xrtFormEncodeNew(
	const void* pData,
	size_t iSize,
	size_t* pOutputSize
)
{
	str sOutput;
	size_t iOutputSize;

	if ( (pOutputSize != NULL) && __xrtRangesOverlap(
		pOutputSize, sizeof(*pOutputSize), pData, iSize
	) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtFormEncode(
		pData, iSize, NULL, 0, &iOutputSize
	) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iOutputSize + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtFormEncode(
		pData, iSize, sOutput, iOutputSize + 1u, &iOutputSize
	) ) {
		xrtFree(sOutput);
		return NULL;
	}
	if ( pOutputSize != NULL ) {
		*pOutputSize = iOutputSize;
	}
	return sOutput;
}



/* 分配解码字节并保留一个不计入长度的零哨兵。 */
XRT_API bytes xrtFormDecodeNew(
	xstrview Text,
	size_t* pOutputSize
)
{
	bytes pOutput;
	size_t iOutputSize;

	if ( (pOutputSize == NULL) || __xrtRangesOverlap(
		pOutputSize, sizeof(*pOutputSize), Text.Data, Text.Size
	) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtFormDecode(Text, NULL, 0, &iOutputSize) ) {
		return NULL;
	}
	pOutput = (bytes)xrtMalloc(iOutputSize + 1u);
	if ( pOutput == NULL ) {
		return NULL;
	}
	if ( !xrtFormDecode(
		Text, pOutput, iOutputSize, &iOutputSize
	) ) {
		xrtFree(pOutput);
		return NULL;
	}
	pOutput[iOutputSize] = 0;
	*pOutputSize = iOutputSize;
	return pOutput;
}



/* 预检后在原始表单缓冲中原地解码全部字段。 */
XRT_API bool xrtFormParse(
	void* pData,
	size_t iSize,
	xformfield* pFields,
	size_t iCapacity,
	size_t* pCount,
	const xformlimits* pLimits
)
{
	xstrview Text = { (cstr)pData, iSize };
	xquerypair Pair;
	size_t iRequired;
	size_t iOffset;
	size_t iField;

	if ( ((pData == NULL) && (iSize != 0)) || (pCount == NULL) ||
		((pFields == NULL) && (iCapacity != 0)) ||
		(iCapacity > (SIZE_MAX / sizeof(*pFields))) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtRangesOverlap(
		pCount, sizeof(*pCount), pData, iSize
	) || __xrtRangesOverlap(
		pCount, sizeof(*pCount), pFields, iCapacity * sizeof(*pFields)
	) || ((pLimits != NULL) && __xrtRangesOverlap(
		pCount, sizeof(*pCount), pLimits, sizeof(*pLimits)
	)) || ((pFields != NULL) && __xrtRangesOverlap(
		pFields, iCapacity * sizeof(*pFields), pData, iSize
	)) || ((pFields != NULL) && (pLimits != NULL) && __xrtRangesOverlap(
		pFields, iCapacity * sizeof(*pFields), pLimits, sizeof(*pLimits)
	)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtFormPreflight(Text, pLimits, &iRequired) ) {
		return false;
	}
	if ( pFields == NULL ) {
		*pCount = iRequired;
		return true;
	}
	if ( iCapacity < iRequired ) {
		*pCount = iRequired;
		__xrtErrorSetRange();
		return false;
	}

	iOffset = 0;
	iField = 0;
	while ( __xrtQueryNext(
		Text, &iOffset, &Pair, false
	) == XQUERY_NEXT_ITEM ) {
		size_t iName;
		size_t iValue;
		uint8* pName = (uint8*)Pair.Key.Data;
		uint8* pValue;

		pValue = (Pair.Flags & XQUERY_HAS_VALUE) != 0 ?
			(uint8*)Pair.Value.Data : pName + Pair.Key.Size;
		iName = xrtPercentDecodeMeasured(Pair.Key, true, pName);
		iValue = xrtPercentDecodeMeasured(Pair.Value, true, pValue);
		pFields[iField].Name = (xbytesview){ pName, iName };
		pFields[iField].Value = (xbytesview){ pValue, iValue };
		iField++;
	}
	*pCount = iRequired;
	return true;
}



/* 在已经预检的表单字段上无分配比较解码名称。 */
static bool __xrtFormDecodedEqual(
	xstrview Text,
	xbytesview Data
)
{
	size_t iOffset = 0;
	size_t iData = 0;
	uint8 iValue;

	while ( xrtPercentNext(
		Text, true, &iOffset, &iValue
	) == XPERCENT_NEXT_BYTE ) {
		if ( (iData == Data.Size) ||
			 (Data.Data[iData] != iValue) ) {
			return false;
		}
		iData++;
	}
	return iData == Data.Size;
}



/* 严格验证整个表单，并查找下一个解码后名称匹配的字段。 */
XRT_API xformfind xrtFormFind(
	xstrview Text,
	xbytesview Name,
	size_t* pOffset,
	void* pValue,
	size_t iCapacity,
	size_t* pSize
)
{
	xquerypair Pair;
	xquerynext Next;
	size_t iCount;
	size_t iCursor;

	if ( !__xrtFormViewValid(Name) ||
		((Text.Data == NULL) && (Text.Size != 0)) ||
		(pOffset == NULL) || (pSize == NULL) ||
		((pValue == NULL) && (iCapacity != 0)) ||
		((pOffset != NULL) && (*pOffset > Text.Size)) ) {
		__xrtErrorSetInvalidArgument();
		return XFORM_FIND_ERROR;
	}
	if ( __xrtRangesOverlap(
		pOffset, sizeof(*pOffset), pSize, sizeof(*pSize)
	) || __xrtRangesOverlap(
		pOffset, sizeof(*pOffset), Text.Data, Text.Size
	) || __xrtRangesOverlap(
		pSize, sizeof(*pSize), Text.Data, Text.Size
	) || __xrtRangesOverlap(
		pOffset, sizeof(*pOffset), Name.Data, Name.Size
	) || __xrtRangesOverlap(
		pSize, sizeof(*pSize), Name.Data, Name.Size
	) ) {
		__xrtErrorSetInvalidArgument();
		return XFORM_FIND_ERROR;
	}
	if ( (pValue != NULL) && (__xrtRangesOverlap(
		pValue, iCapacity, Text.Data, Text.Size
	) || __xrtRangesOverlap(
		pValue, iCapacity, Name.Data, Name.Size
	) || __xrtRangesOverlap(
		pValue, iCapacity, pOffset, sizeof(*pOffset)
	) || __xrtRangesOverlap(
		pValue, iCapacity, pSize, sizeof(*pSize)
	)) ) {
		__xrtErrorSetInvalidArgument();
		return XFORM_FIND_ERROR;
	}
	if ( !__xrtFormPreflight(Text, NULL, &iCount) ) {
		return XFORM_FIND_ERROR;
	}
	(void)iCount;

	iCursor = *pOffset;
	for ( ;; ) {
		size_t iRequired;

		Next = __xrtQueryNext(Text, &iCursor, &Pair, false);
		if ( Next == XQUERY_NEXT_ERROR ) {
			return XFORM_FIND_ERROR;
		}
		if ( Next == XQUERY_NEXT_END ) {
			*pOffset = iCursor;
			return XFORM_FIND_END;
		}
		if ( !__xrtFormDecodedEqual(Pair.Key, Name) ) {
			continue;
		}
		if ( !xrtPercentDecodeMeasure(
			Pair.Value, true, &iRequired
		) ) {
			return XFORM_FIND_ERROR;
		}
		if ( pValue == NULL ) {
			*pSize = iRequired;
			*pOffset = iCursor;
			return XFORM_FIND_FOUND;
		}
		if ( iCapacity < iRequired ) {
			*pSize = iRequired;
			__xrtErrorSetRange();
			return XFORM_FIND_ERROR;
		}
		if ( iRequired != 0 ) {
			(void)xrtPercentDecodeMeasured(
				Pair.Value, true, pValue
			);
		}
		*pSize = iRequired;
		*pOffset = iCursor;
		return XFORM_FIND_FOUND;
	}
}



/* 原子写出 form-urlencoded 字段列表。 */
XRT_API bool xrtFormWrite(
	const xformfield* pFields,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xpercentmap Safe;
	char* sWrite = (char*)pOutput;
	size_t iRequired;
	size_t iOffset = 0;
	size_t i;

	if ( (pSize == NULL) || ((pOutput == NULL) && (iCapacity != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtFormSafeMap(&Safe) ) {
		return false;
	}
	if ( !__xrtFormMeasure(pFields, iCount, &Safe, &iRequired) ) {
		return false;
	}
	if ( __xrtFormMetadataOverlap(
		pFields, iCount, pSize, sizeof(*pSize)
	) || ((pOutput != NULL) && __xrtRangesOverlap(
		pSize, sizeof(*pSize), pOutput, iRequired
	)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pOutput == NULL ) {
		*pSize = iRequired;
		return true;
	}
	if ( iCapacity < iRequired ) {
		*pSize = iRequired;
		__xrtErrorSetRange();
		return false;
	}
	if ( __xrtFormMetadataOverlap(
		pFields, iCount, pOutput, iRequired
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( i != 0 ) {
			sWrite[iOffset++] = '&';
		}
		iOffset += xrtPercentWriteMeasured(
			pFields[i].Name.Data, pFields[i].Name.Size,
			&Safe, true, sWrite + iOffset
		);
		sWrite[iOffset++] = '=';
		iOffset += xrtPercentWriteMeasured(
			pFields[i].Value.Data, pFields[i].Value.Size,
			&Safe, true, sWrite + iOffset
		);
	}
	*pSize = iRequired;
	return true;
}



/* 分配并构建零结尾 form-urlencoded 字段列表。 */
XRT_API str xrtFormBuild(
	const xformfield* pFields,
	size_t iCount,
	size_t* pSize
)
{
	str sOutput;
	size_t iRequired;

	if ( (pSize != NULL) && (pFields != NULL) &&
		(iCount <= (SIZE_MAX / sizeof(*pFields))) &&
		__xrtFormMetadataOverlap(
			pFields, iCount, pSize, sizeof(*pSize)
		) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtFormWrite(pFields, iCount, NULL, 0, &iRequired) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtFormWrite(
		pFields, iCount, sOutput, iRequired, &iRequired
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
