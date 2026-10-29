#include "../internal/xrt_query.h"



#if defined(XHTTP_FEATURE_QUERY)

/* 验证借用视图的空值一致性。 */
bool __xrtQueryViewValid(xstrview Text)
{
	if ( !__xrtRangeValid(Text.Data, Text.Size) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 验证 Query 项数组的视图、标志和值存在状态。 */
bool __xrtQueryPairsValid(const xquerypair* pPairs, size_t iCount)
{
	size_t i;

	if ( (pPairs == NULL) && (iCount != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iCount > (SIZE_MAX / sizeof(*pPairs)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		const xquerypair* pPair = &pPairs[i];

		if ( !__xrtQueryViewValid(pPair->Key) ||
			!__xrtQueryViewValid(pPair->Value) ) {
			return false;
		}
		if ( ((pPair->Flags & ~XQUERY_HAS_VALUE) != 0) ||
			(((pPair->Flags & XQUERY_HAS_VALUE) == 0) &&
			 ((pPair->Key.Size == 0) || (pPair->Value.Data != NULL) ||
			  (pPair->Value.Size != 0))) ) {
			__xrtErrorSetValue();
			return false;
		}
	}
	return true;
}



/* 检查一个借用视图是否包含会改变查询项结构的分隔符。 */
static bool __xrtQueryHasByte(xstrview Text, char iValue)
{
	size_t i;

	for ( i = 0; i < Text.Size; i++ ) {
		if ( Text.Data[i] == iValue ) {
			return true;
		}
	}
	return false;
}



/* 验证全部原始查询项并计算精确写出长度。 */
static bool __xrtQueryMeasure(
	const xquerypair* pPairs,
	size_t iCount,
	size_t* pRequired
)
{
	size_t iRequired = 0;
	size_t i;

	if ( !__xrtQueryPairsValid(pPairs, iCount) ) {
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		const xquerypair* pPair = &pPairs[i];
		size_t iAdd;

		if ( __xrtQueryHasByte(pPair->Key, '&') ||
			__xrtQueryHasByte(pPair->Key, '=') ||
			__xrtQueryHasByte(pPair->Value, '&') ) {
			__xrtErrorSetValue();
			return false;
		}
		iAdd = pPair->Key.Size;
		if ( (pPair->Flags & XQUERY_HAS_VALUE) != 0 ) {
			if ( iAdd == SIZE_MAX ) {
				__xrtErrorSetSizeOverflow();
				return false;
			}
			iAdd++;
			if ( iAdd > (SIZE_MAX - pPair->Value.Size) ) {
				__xrtErrorSetSizeOverflow();
				return false;
			}
			iAdd += pPair->Value.Size;
		}
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



/* 判断输出元数据是否会覆盖查询项数组或任一借用文本。 */
bool __xrtQueryMetadataOverlap(
	const xquerypair* pPairs,
	size_t iCount,
	const void* pMetadata,
	size_t iMetadataSize
)
{
	size_t i;

	if ( __xrtRangesOverlap(
		pMetadata, iMetadataSize, pPairs, iCount * sizeof(*pPairs)
	) ) {
		return true;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( __xrtRangesOverlap(
			pMetadata, iMetadataSize,
			pPairs[i].Key.Data, pPairs[i].Key.Size
		) || __xrtRangesOverlap(
			pMetadata, iMetadataSize,
			pPairs[i].Value.Data, pPairs[i].Value.Size
		) ) {
			return true;
		}
	}
	return false;
}



/* 判断写出范围是否覆盖查询项数组或任一借用文本。 */
static bool __xrtQueryOutputOverlap(
	const xquerypair* pPairs,
	size_t iCount,
	const void* pOutput,
	size_t iOutputSize
)
{
	return __xrtQueryMetadataOverlap(
		pPairs, iCount, pOutput, iOutputSize
	);
}



/* 扫描下一个非空原始查询项，并允许调用方控制前导问号语义。 */
xquerynext __xrtQueryNext(
	xstrview Query,
	size_t* pOffset,
	xquerypair* pPair,
	bool bQuestion
)
{
	size_t iStart;
	size_t iEnd;
	size_t iEqual;
	size_t i;

	if ( !__xrtQueryViewValid(Query) || (pOffset == NULL) ||
		(pPair == NULL) || (*pOffset > Query.Size) ||
		__xrtRangesOverlap(
			pOffset, sizeof(*pOffset), Query.Data, Query.Size
		) || __xrtRangesOverlap(
			pPair, sizeof(*pPair), Query.Data, Query.Size
		) || __xrtRangesOverlap(
			pOffset, sizeof(*pOffset), pPair, sizeof(*pPair)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XQUERY_NEXT_ERROR;
	}
	memset(pPair, 0, sizeof(*pPair));
	iStart = *pOffset;
	if ( bQuestion && (iStart == 0) && (iStart < Query.Size) &&
		(Query.Data[iStart] == '?') ) {
		iStart++;
	}
	while ( (iStart < Query.Size) && (Query.Data[iStart] == '&') ) {
		iStart++;
	}
	if ( iStart == Query.Size ) {
		*pOffset = iStart;
		return XQUERY_NEXT_END;
	}
	iEnd = iStart;
	iEqual = XRT_NPOS;
	while ( (iEnd < Query.Size) && (Query.Data[iEnd] != '&') ) {
		if ( (Query.Data[iEnd] == '=') && (iEqual == XRT_NPOS) ) {
			iEqual = iEnd;
		}
		iEnd++;
	}
	if ( iEqual == XRT_NPOS ) {
		pPair->Key = (xstrview){ Query.Data + iStart, iEnd - iStart };
	} else {
		pPair->Flags = XQUERY_HAS_VALUE;
		pPair->Key = (xstrview){ Query.Data + iStart, iEqual - iStart };
		pPair->Value = (xstrview){
			Query.Data + iEqual + 1u, iEnd - iEqual - 1u
		};
	}
	i = iEnd;
	if ( i < Query.Size ) {
		i++;
	}
	*pOffset = i;
	return XQUERY_NEXT_ITEM;
}



/* 读取下一个非空原始查询项，并识别可选前导问号。 */
XRT_API xquerynext xrtQueryNext(
	xstrview Query,
	size_t* pOffset,
	xquerypair* pPair
)
{
	return __xrtQueryNext(Query, pOffset, pPair, true);
}



/* 统计全部非空查询段。 */
XRT_API bool xrtQueryCount(xstrview Query, size_t* pCount)
{
	xquerypair Pair;
	xquerynext Next;
	size_t iOffset = 0;
	size_t iCount = 0;

	if ( (pCount == NULL) || __xrtRangesOverlap(
		pCount, sizeof(*pCount), Query.Data, Query.Size
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( ;; ) {
		Next = xrtQueryNext(Query, &iOffset, &Pair);
		if ( Next == XQUERY_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XQUERY_NEXT_END ) {
			*pCount = iCount;
			return true;
		}
		if ( iCount == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iCount++;
	}
}



/* 按调用方明确给出的上限验证每个原始查询项。 */
XRT_API bool xrtQueryValidate(
	xstrview Query,
	const xquerylimits* pLimits,
	size_t* pCount
)
{
	xquerypair Pair;
	xquerynext Next;
	size_t iOffset = 0;
	size_t iCount = 0;

	if ( ((pCount != NULL) && __xrtRangesOverlap(
		pCount, sizeof(*pCount), Query.Data, Query.Size
	)) || ((pCount != NULL) && (pLimits != NULL) && __xrtRangesOverlap(
		pCount, sizeof(*pCount), pLimits, sizeof(*pLimits)
	)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( ;; ) {
		Next = xrtQueryNext(Query, &iOffset, &Pair);
		if ( Next == XQUERY_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XQUERY_NEXT_END ) {
			if ( pCount != NULL ) {
				*pCount = iCount;
			}
			return true;
		}
		if ( (pLimits != NULL) &&
			(((pLimits->MaxPairs != 0) &&
			  (iCount >= pLimits->MaxPairs)) ||
			 ((pLimits->MaxKey != 0) &&
			  (Pair.Key.Size > pLimits->MaxKey)) ||
			 ((pLimits->MaxValue != 0) &&
			  (Pair.Value.Size > pLimits->MaxValue))) ) {
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



/* 从指定偏移查找下一个同名原始查询项。 */
XRT_API xquerynext xrtQueryFind(
	xstrview Query,
	xstrview Key,
	size_t* pOffset,
	xquerypair* pPair
)
{
	xquerynext Next;

	if ( !__xrtQueryViewValid(Key) || (pOffset == NULL) ||
		(pPair == NULL) || __xrtRangesOverlap(
			pOffset, sizeof(*pOffset), Key.Data, Key.Size
		) || __xrtRangesOverlap(
			pPair, sizeof(*pPair), Key.Data, Key.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return XQUERY_NEXT_ERROR;
	}
	for ( ;; ) {
		Next = xrtQueryNext(Query, pOffset, pPair);
		if ( Next != XQUERY_NEXT_ITEM ) {
			return Next;
		}
		if ( (pPair->Key.Size == Key.Size) &&
			((Key.Size == 0) ||
			 (memcmp(pPair->Key.Data, Key.Data, Key.Size) == 0)) ) {
			return XQUERY_NEXT_ITEM;
		}
	}
}



/* 原子写出已经编码好的原始查询项。 */
XRT_API bool xrtQueryRawWrite(
	const xquerypair* pPairs,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	uint8* pWrite = (uint8*)pOutput;
	size_t iRequired;
	size_t iOffset = 0;
	size_t i;

	if ( (pSize == NULL) || ((pOutput == NULL) && (iCapacity != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtQueryMeasure(pPairs, iCount, &iRequired) ) {
		return false;
	}
	if ( __xrtQueryMetadataOverlap(
		pPairs, iCount, pSize, sizeof(*pSize)
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
	if ( __xrtQueryOutputOverlap(
		pPairs, iCount, pOutput, iRequired
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( i != 0 ) {
			pWrite[iOffset++] = (uint8)'&';
		}
		if ( pPairs[i].Key.Size != 0 ) {
			memcpy(
				pWrite + iOffset,
				pPairs[i].Key.Data,
				pPairs[i].Key.Size
			);
			iOffset += pPairs[i].Key.Size;
		}
		if ( (pPairs[i].Flags & XQUERY_HAS_VALUE) != 0 ) {
			pWrite[iOffset++] = (uint8)'=';
			if ( pPairs[i].Value.Size != 0 ) {
				memcpy(
					pWrite + iOffset,
					pPairs[i].Value.Data,
					pPairs[i].Value.Size
				);
				iOffset += pPairs[i].Value.Size;
			}
		}
	}
	*pSize = iRequired;
	return true;
}



/* 分配并构建零结尾原始查询。 */
XRT_API str xrtQueryRawBuild(
	const xquerypair* pPairs,
	size_t iCount,
	size_t* pSize
)
{
	str sOutput;
	size_t iRequired;

	if ( (pSize != NULL) && (pPairs != NULL) &&
		(iCount <= (SIZE_MAX / sizeof(*pPairs))) &&
		__xrtQueryMetadataOverlap(
			pPairs, iCount, pSize, sizeof(*pSize)
		) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtQueryRawWrite(pPairs, iCount, NULL, 0, &iRequired) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtQueryRawWrite(
		pPairs, iCount, sOutput, iRequired, &iRequired
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
