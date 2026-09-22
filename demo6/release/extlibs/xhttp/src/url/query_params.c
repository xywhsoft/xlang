#include "../internal/xrt_query_params.h"



#if defined(XHTTP_FEATURE_QUERY_PARAMS)

/* QueryParams 使用连续 pair 数组和连续字节区兼顾遍历速度与内存密度。 */
struct xqueryparams {
	xqueryparamsconfig Config;
	xquerypair* Pairs;
	bytes Storage;
	size_t Count;
	size_t PairCapacity;
	size_t StorageUsed;
	size_t StorageCapacity;
	size_t Bytes;
	size_t StoredBytes;
};



/* 安全计算两个 size_t 的和。 */
static bool __xrtQueryParamsAddSize(
	size_t iLeft,
	size_t iRight,
	size_t* pResult
)
{
	if ( (pResult == NULL) || (iLeft > (SIZE_MAX - iRight)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pResult = iLeft + iRight;
	return true;
}



/* 解析默认配置并验证全部容量、限额和标志。 */
static bool __xrtQueryParamsConfigResolve(
	const xqueryparamsconfig* pInput,
	xqueryparamsconfig* pConfig
)
{
	xrtQueryParamsConfigInit(pConfig);
	if ( pInput != NULL ) {
		*pConfig = *pInput;
	}
	if ( (pConfig->InitialPairs > pConfig->MaxPairs) ||
		(pConfig->InitialBytes > pConfig->MaxBytes) ||
		(pConfig->MaxPairs > (SIZE_MAX / sizeof(xquerypair))) ||
		((pConfig->Flags & ~XQUERY_PARAMS_LENIENT_PERCENT) != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 验证一个 pair 能够无损表示并满足单项限额。 */
static bool __xrtQueryParamsPairValid(
	const xqueryparams* pParams,
	xquerypair Pair,
	size_t* pBytes
)
{
	if ( (pParams == NULL) || (pBytes == NULL) ||
		!__xrtQueryViewValid(Pair.Key) ||
		!__xrtQueryViewValid(Pair.Value) ||
		((Pair.Flags & ~XQUERY_HAS_VALUE) != 0) ||
		(((Pair.Flags & XQUERY_HAS_VALUE) == 0) &&
		 ((Pair.Value.Size != 0) || (Pair.Key.Size == 0))) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (Pair.Key.Size > pParams->Config.MaxName) ||
		(Pair.Value.Size > pParams->Config.MaxValue) ) {
		__xrtErrorSetRange();
		return false;
	}
	if ( !__xrtQueryParamsAddSize(
		Pair.Key.Size, Pair.Value.Size, pBytes
	) ) {
		return false;
	}
	if ( *pBytes > pParams->Config.MaxBytes ) {
		__xrtErrorSetRange();
		return false;
	}
	return true;
}



/* 验证名称可用于区分大小写的查找和删除。 */
static bool __xrtQueryParamsNameValid(
	const xqueryparams* pParams,
	xstrview Name
)
{
	if ( (pParams == NULL) || !__xrtQueryViewValid(Name) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 比较两个已解码名称是否按字节完全相同。 */
static bool __xrtQueryParamsNameEqual(
	xstrview Left,
	xstrview Right
)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 判断调用方视图是否借用了当前有效或废弃字符串区。 */
static bool __xrtQueryParamsAliases(
	const xqueryparams* pParams,
	xstrview Text
)
{
	return (pParams->Storage != NULL) &&
		(Text.Data != NULL) && (Text.Size != 0) &&
		__xrtRangesOverlap(
			pParams->Storage, pParams->StorageUsed,
			Text.Data, Text.Size
		);
}



/* 在扩容或紧凑化前复制借用当前容器的调用方 pair。 */
static bool __xrtQueryParamsStabilize(
	const xqueryparams* pParams,
	xquerypair* pPair,
	bytes* ppStorage
)
{
	size_t iBytes;
	bytes pStorage;

	*ppStorage = NULL;
	if ( !__xrtQueryParamsAliases(pParams, pPair->Key) &&
		!__xrtQueryParamsAliases(pParams, pPair->Value) ) {
		return true;
	}
	if ( !__xrtQueryParamsAddSize(
		pPair->Key.Size, pPair->Value.Size, &iBytes
	) ) {
		return false;
	}
	pStorage = (bytes)xrtMalloc(iBytes != 0 ? iBytes : 1u);
	if ( pStorage == NULL ) {
		return false;
	}
	if ( pPair->Key.Size != 0 ) {
		memmove(pStorage, pPair->Key.Data, pPair->Key.Size);
	}
	if ( pPair->Value.Size != 0 ) {
		memmove(
			pStorage + pPair->Key.Size,
			pPair->Value.Data,
			pPair->Value.Size
		);
	}
	pPair->Key.Data = (cstr)pStorage;
	pPair->Value.Data = (cstr)(pStorage + pPair->Key.Size);
	*ppStorage = pStorage;
	return true;
}



/* 按倍增策略预留 pair 数组，同时不超过配置硬上限。 */
static bool __xrtQueryParamsReservePairs(
	xqueryparams* pParams,
	size_t iRequired
)
{
	xquerypair* pPairs;
	size_t iCapacity;

	if ( iRequired <= pParams->PairCapacity ) {
		return true;
	}
	if ( (iRequired > pParams->Config.MaxPairs) ||
		(iRequired > (SIZE_MAX / sizeof(*pPairs))) ) {
		__xrtErrorSetRange();
		return false;
	}
	iCapacity = pParams->PairCapacity != 0 ?
		pParams->PairCapacity : pParams->Config.InitialPairs;
	if ( iCapacity == 0 ) {
		iCapacity = 1;
	}
	while ( iCapacity < iRequired ) {
		size_t iNext = iCapacity > (SIZE_MAX / 2u) ?
			iRequired : iCapacity * 2u;

		if ( iNext > pParams->Config.MaxPairs ) {
			iNext = pParams->Config.MaxPairs;
		}
		if ( iNext <= iCapacity ) {
			iCapacity = iRequired;
			break;
		}
		iCapacity = iNext;
	}
	pPairs = (xquerypair*)xrtRealloc(
		pParams->Pairs, iCapacity * sizeof(*pPairs)
	);
	if ( pPairs == NULL ) {
		return false;
	}
	pParams->Pairs = pPairs;
	pParams->PairCapacity = iCapacity;
	return true;
}



/* 扩大连续字符串区并修正所有非空借用视图。 */
static bool __xrtQueryParamsReserveStorage(
	xqueryparams* pParams,
	size_t iRequired
)
{
	bytes pStorage;
	size_t iCapacity;
	size_t i;

	if ( iRequired <= pParams->StorageCapacity ) {
		return true;
	}
	if ( iRequired > pParams->Config.MaxBytes ) {
		__xrtErrorSetRange();
		return false;
	}
	iCapacity = pParams->StorageCapacity != 0 ?
		pParams->StorageCapacity : pParams->Config.InitialBytes;
	if ( iCapacity == 0 ) {
		iCapacity = 1;
	}
	while ( iCapacity < iRequired ) {
		size_t iNext = iCapacity > (SIZE_MAX / 2u) ?
			iRequired : iCapacity * 2u;

		if ( iNext > pParams->Config.MaxBytes ) {
			iNext = pParams->Config.MaxBytes;
		}
		if ( iNext <= iCapacity ) {
			iCapacity = iRequired;
			break;
		}
		iCapacity = iNext;
	}
	pStorage = (bytes)xrtMalloc(iCapacity);
	if ( pStorage == NULL ) {
		return false;
	}
	if ( pParams->StorageUsed != 0 ) {
		memcpy(pStorage, pParams->Storage, pParams->StorageUsed);
	}
	for ( i = 0; i < pParams->Count; i++ ) {
		if ( pParams->Pairs[i].Key.Size != 0 ) {
			pParams->Pairs[i].Key.Data = (cstr)(
				pStorage + (pParams->Pairs[i].Key.Data -
				(cstr)pParams->Storage)
			);
		}
		if ( pParams->Pairs[i].Value.Size != 0 ) {
			pParams->Pairs[i].Value.Data = (cstr)(
				pStorage + (pParams->Pairs[i].Value.Data -
				(cstr)pParams->Storage)
			);
		}
	}
	xrtFree(pParams->Storage);
	pParams->Storage = pStorage;
	pParams->StorageCapacity = iCapacity;
	return true;
}



/* 为一次逻辑增长消除废弃字节并预留两类连续存储。 */
static bool __xrtQueryParamsPrepare(
	xqueryparams* pParams,
	size_t iPairs,
	size_t iBytes,
	size_t iAppendBytes
)
{
	size_t iPhysical;

	if ( (iPairs > pParams->Config.MaxPairs) ||
		(iBytes > pParams->Config.MaxBytes) ) {
		__xrtErrorSetRange();
		return false;
	}
	if ( iAppendBytes >
		(pParams->Config.MaxBytes - pParams->StorageUsed) ) {
		if ( !xrtQueryParamsCompact(pParams) ) {
			return false;
		}
	}
	if ( !__xrtQueryParamsAddSize(
		pParams->StorageUsed, iAppendBytes, &iPhysical
	) ) {
		return false;
	}
	return __xrtQueryParamsReservePairs(pParams, iPairs) &&
		__xrtQueryParamsReserveStorage(pParams, iPhysical);
}



/* 把一个已经验证的 pair 复制到字符串区尾部。 */
static xquerypair __xrtQueryParamsStore(
	xqueryparams* pParams,
	xquerypair Pair
)
{
	xquerypair Stored = Pair;

	if ( Pair.Key.Size != 0 ) {
		Stored.Key.Data = (cstr)(
			pParams->Storage + pParams->StorageUsed
		);
		memcpy(
			pParams->Storage + pParams->StorageUsed,
			Pair.Key.Data,
			Pair.Key.Size
		);
		pParams->StorageUsed += Pair.Key.Size;
	} else {
		Stored.Key.Data = NULL;
	}
	if ( Pair.Value.Size != 0 ) {
		Stored.Value.Data = (cstr)(
			pParams->Storage + pParams->StorageUsed
		);
		memcpy(
			pParams->Storage + pParams->StorageUsed,
			Pair.Value.Data,
			Pair.Value.Size
		);
		pParams->StorageUsed += Pair.Value.Size;
	} else {
		Stored.Value.Data = NULL;
	}
	return Stored;
}



/* 把工作容器的全部资产原子提交到目标容器。 */
static void __xrtQueryParamsCommit(
	xqueryparams* pParams,
	xqueryparams* pWork
)
{
	xrtFree(pParams->Pairs);
	xrtFree(pParams->Storage);
	pParams->Pairs = pWork->Pairs;
	pParams->Storage = pWork->Storage;
	pParams->Count = pWork->Count;
	pParams->PairCapacity = pWork->PairCapacity;
	pParams->StorageUsed = pWork->StorageUsed;
	pParams->StorageCapacity = pWork->StorageCapacity;
	pParams->Bytes = pWork->Bytes;
	pParams->StoredBytes = pWork->StoredBytes;
	pWork->Pairs = NULL;
	pWork->Storage = NULL;
	pWork->Count = 0;
	pWork->PairCapacity = 0;
	pWork->StorageUsed = 0;
	pWork->StorageCapacity = 0;
	pWork->Bytes = 0;
	pWork->StoredBytes = 0;
}



/* 返回十六进制字符值，无效字符返回负数。 */
static int __xrtQueryParamsHex(uint8 Character)
{
	if ( (Character >= (uint8)'0') &&
		(Character <= (uint8)'9') ) {
		return (int)(Character - (uint8)'0');
	}
	if ( (Character >= (uint8)'A') &&
		(Character <= (uint8)'F') ) {
		return (int)(Character - (uint8)'A') + 10;
	}
	if ( (Character >= (uint8)'a') &&
		(Character <= (uint8)'f') ) {
		return (int)(Character - (uint8)'a') + 10;
	}
	return -1;
}



/* 宽松解码 form-urlencoded 字节并保留无效百分号。 */
static void __xrtQueryParamsLooseDecode(
	xstrview Text,
	bytes pOutput
)
{
	size_t iOutput = 0;
	size_t i;

	for ( i = 0; i < Text.Size; i++ ) {
		int iHigh;
		int iLow;

		if ( Text.Data[i] == '+' ) {
			pOutput[iOutput++] = (uint8)' ';
			continue;
		}
		iHigh = ((Text.Size - i) >= 3u) ?
			__xrtQueryParamsHex((uint8)Text.Data[i + 1u]) : -1;
		iLow = ((Text.Size - i) >= 3u) ?
			__xrtQueryParamsHex((uint8)Text.Data[i + 2u]) : -1;
		if ( (Text.Data[i] == '%') &&
			(iHigh >= 0) && (iLow >= 0) ) {
			pOutput[iOutput++] = (uint8)(
				((uint32)iHigh << 4u) | (uint32)iLow
			);
			i += 2u;
		} else {
			pOutput[iOutput++] = (uint8)Text.Data[i];
		}
	}
}



/* 测量一段严格或宽松 form-urlencoded 文本。 */
static bool __xrtQueryParamsDecodedSize(
	xstrview Text,
	bool bLenient,
	size_t* pSize,
	size_t* pError
)
{
	size_t iSize = 0;
	size_t i;

	for ( i = 0; i < Text.Size; i++ ) {
		if ( Text.Data[i] == '%' ) {
			bool bValid = ((Text.Size - i) >= 3u) &&
				(__xrtQueryParamsHex((uint8)Text.Data[i + 1u]) >= 0) &&
				(__xrtQueryParamsHex((uint8)Text.Data[i + 2u]) >= 0);

			if ( bValid ) {
				i += 2u;
			} else if ( !bLenient ) {
				*pError = i;
				__xrtErrorSetValue();
				return false;
			}
		}
		iSize++;
	}
	*pSize = iSize;
	return true;
}



/* 解码一段已经测量过的 form-urlencoded 文本。 */
static void __xrtQueryParamsDecode(
	xstrview Text,
	bool bLenient,
	bytes pOutput,
	size_t iSize
)
{
	if ( iSize != 0 ) {
		if ( bLenient ) {
			__xrtQueryParamsLooseDecode(Text, pOutput);
		} else {
			(void)xrtPercentDecodeMeasured(Text, true, pOutput);
		}
	}
}



/* 把已经测量的原始 pair 直接解码到工作容器的最终连续存储。 */
static bool __xrtQueryParamsAppendDecoded(
	xqueryparams* pParams,
	xquerypair Raw,
	bool bLenient,
	size_t iName,
	size_t iValue
)
{
	xquerypair Pair = Raw;
	size_t iPairBytes;
	size_t iPairs;
	size_t iBytes;

	Pair.Key.Size = iName;
	Pair.Value.Size = iValue;
	if ( !__xrtQueryParamsPairValid(
		pParams, Pair, &iPairBytes
	) || !__xrtQueryParamsAddSize(
		pParams->Count, 1u, &iPairs
	) || !__xrtQueryParamsAddSize(
		pParams->Bytes, iPairBytes, &iBytes
	) || !__xrtQueryParamsPrepare(
		pParams, iPairs, iBytes, iPairBytes
	) ) {
		return false;
	}
	if ( iName != 0 ) {
		Pair.Key.Data = (cstr)(
			pParams->Storage + pParams->StorageUsed
		);
		__xrtQueryParamsDecode(
			Raw.Key, bLenient,
			pParams->Storage + pParams->StorageUsed,
			iName
		);
		pParams->StorageUsed += iName;
	} else {
		Pair.Key.Data = NULL;
	}
	if ( iValue != 0 ) {
		Pair.Value.Data = (cstr)(
			pParams->Storage + pParams->StorageUsed
		);
		__xrtQueryParamsDecode(
			Raw.Value, bLenient,
			pParams->Storage + pParams->StorageUsed,
			iValue
		);
		pParams->StorageUsed += iValue;
	} else {
		Pair.Value.Data = NULL;
	}
	pParams->Pairs[pParams->Count] = Pair;
	pParams->Count = iPairs;
	pParams->Bytes = iBytes;
	pParams->StoredBytes += iPairBytes;
	return true;
}



/* 解析一段文本并把已解码 pair 追加到工作容器。 */
static bool __xrtQueryParamsParseWork(
	xqueryparams* pWork,
	xstrview Text,
	size_t* pErrorOffset
)
{
	bool bLenient =
		(pWork->Config.Flags & XQUERY_PARAMS_LENIENT_PERCENT) != 0;
	size_t iOffset = 0;
	xquerypair Raw;
	xquerynext Next;

	for ( ;; ) {
		size_t iName;
		size_t iValue;
		size_t iDecodeError = 0;
		size_t iSegment;

		iSegment = iOffset;
		Next = xrtQueryNext(Text, &iOffset, &Raw);
		if ( Next == XQUERY_NEXT_END ) {
			*pErrorOffset = Text.Size;
			return true;
		}
		if ( Next == XQUERY_NEXT_ERROR ) {
			*pErrorOffset = iSegment;
			return false;
		}
		if ( !__xrtQueryParamsDecodedSize(
			Raw.Key, bLenient, &iName, &iDecodeError
		) ) {
			*pErrorOffset = (size_t)(Raw.Key.Data - Text.Data) +
				iDecodeError;
			return false;
		}
		if ( !__xrtQueryParamsDecodedSize(
			Raw.Value, bLenient, &iValue, &iDecodeError
		) ) {
			*pErrorOffset = (size_t)(Raw.Value.Data - Text.Data) +
				iDecodeError;
			return false;
		}
		if ( !__xrtQueryParamsAppendDecoded(
			pWork, Raw, bLenient, iName, iValue
		) ) {
			*pErrorOffset = iSegment;
			return false;
		}
	}
}



/* 按 unsigned 字节顺序比较两个名称。 */
static int __xrtQueryParamsCompare(
	xstrview Left,
	xstrview Right
)
{
	size_t iMin = Left.Size < Right.Size ? Left.Size : Right.Size;
	int iCompare = iMin != 0 ?
		memcmp(Left.Data, Right.Data, iMin) : 0;

	if ( iCompare != 0 ) {
		return iCompare;
	}
	if ( Left.Size < Right.Size ) {
		return -1;
	}
	if ( Left.Size > Right.Size ) {
		return 1;
	}
	return 0;
}



/* 测量容器的 form-urlencoded 线缆长度。 */
static bool __xrtQueryParamsMeasure(
	const xqueryparams* pParams,
	const xpercentmap* pSafe,
	size_t* pRequired
)
{
	size_t iRequired = 0;
	size_t i;

	for ( i = 0; i < pParams->Count; i++ ) {
		size_t iName;
		size_t iValue = 0;
		size_t iAdd;

		if ( !xrtPercentMeasure(
			pParams->Pairs[i].Key.Data,
			pParams->Pairs[i].Key.Size,
			pSafe, true, &iName
		) ) {
			return false;
		}
		if ( (pParams->Pairs[i].Flags &
			XQUERY_HAS_VALUE) != 0 ) {
			if ( !xrtPercentMeasure(
				pParams->Pairs[i].Value.Data,
				pParams->Pairs[i].Value.Size,
				pSafe, true, &iValue
			) || !__xrtQueryParamsAddSize(
				iValue, 1u, &iValue
			) ) {
				return false;
			}
		}
		if ( !__xrtQueryParamsAddSize(iName, iValue, &iAdd) ||
			((i != 0) && !__xrtQueryParamsAddSize(
				iAdd, 1u, &iAdd
			)) || !__xrtQueryParamsAddSize(
				iRequired, iAdd, &iRequired
			) ) {
			return false;
		}
	}
	*pRequired = iRequired;
	return true;
}



/* 初始化拥有型 QueryParams 的默认容量和安全限额。 */
XRT_API void xrtQueryParamsConfigInit(xqueryparamsconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	pConfig->InitialPairs = 8;
	pConfig->InitialBytes = 256;
	pConfig->MaxPairs = 4096;
	pConfig->MaxName = 64u * 1024u;
	pConfig->MaxValue = 1024u * 1024u;
	pConfig->MaxBytes = 4u * 1024u * 1024u;
	pConfig->Flags = 0;
}



/* 创建拥有型 QueryParams 容器。 */
XRT_API xqueryparams* xrtQueryParamsCreate(
	const xqueryparamsconfig* pConfig
)
{
	xqueryparamsconfig Config;
	xqueryparams* pParams;

	if ( !__xrtQueryParamsConfigResolve(pConfig, &Config) ) {
		return NULL;
	}
	pParams = (xqueryparams*)xrtCalloc(1, sizeof(*pParams));
	if ( pParams == NULL ) {
		return NULL;
	}
	pParams->Config = Config;
	if ( !xrtQueryParamsReserve(
		pParams, Config.InitialPairs, Config.InitialBytes
	) ) {
		xrtQueryParamsDestroy(pParams);
		return NULL;
	}
	return pParams;
}



/* 解析 form-urlencoded 文本并创建拥有型容器。 */
XRT_API xqueryparams* xrtQueryParamsParse(
	xstrview Text,
	const xqueryparamsconfig* pConfig,
	size_t* pErrorOffset
)
{
	xqueryparams* pParams;
	size_t iError;

	if ( !__xrtQueryViewValid(Text) ||
		((pErrorOffset != NULL) && __xrtRangesOverlap(
			pErrorOffset, sizeof(*pErrorOffset),
			Text.Data, Text.Size
		)) ) {
		return NULL;
	}
	pParams = xrtQueryParamsCreate(pConfig);
	if ( pParams == NULL ) {
		return NULL;
	}
	if ( !__xrtQueryParamsParseWork(pParams, Text, &iError) ) {
		if ( pErrorOffset != NULL ) {
			*pErrorOffset = iError;
		}
		xrtQueryParamsDestroy(pParams);
		return NULL;
	}
	if ( pErrorOffset != NULL ) {
		*pErrorOffset = Text.Size;
	}
	return pParams;
}



/* 深复制一个 QueryParams 容器。 */
XRT_API xqueryparams* xrtQueryParamsClone(
	const xqueryparams* pParams
)
{
	xqueryparams* pClone;
	size_t i;

	if ( pParams == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pClone = xrtQueryParamsCreate(&pParams->Config);
	if ( pClone == NULL ) {
		return NULL;
	}
	if ( !xrtQueryParamsReserve(
		pClone, pParams->Count, pParams->Bytes
	) ) {
		xrtQueryParamsDestroy(pClone);
		return NULL;
	}
	for ( i = 0; i < pParams->Count; i++ ) {
		if ( !xrtQueryParamsAppendPair(
			pClone, pParams->Pairs[i]
		) ) {
			xrtQueryParamsDestroy(pClone);
			return NULL;
		}
	}
	return pClone;
}



/* 销毁 QueryParams 的全部资产。 */
XRT_API void xrtQueryParamsDestroy(xqueryparams* pParams)
{
	if ( pParams == NULL ) {
		return;
	}
	xrtFree(pParams->Pairs);
	xrtFree(pParams->Storage);
	memset(pParams, 0, sizeof(*pParams));
	xrtFree(pParams);
}



/* 清空逻辑内容并保留容量。 */
XRT_API void xrtQueryParamsClear(xqueryparams* pParams)
{
	if ( pParams == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	pParams->Count = 0;
	pParams->StorageUsed = 0;
	pParams->Bytes = 0;
	pParams->StoredBytes = 0;
}



/* 预留指定逻辑 pair 与有效字节容量。 */
XRT_API bool xrtQueryParamsReserve(
	xqueryparams* pParams,
	size_t iPairs,
	size_t iBytes
)
{
	size_t iAdditional;
	size_t iPhysical;

	if ( pParams == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (iPairs > pParams->Config.MaxPairs) ||
		(iBytes > pParams->Config.MaxBytes) ) {
		__xrtErrorSetRange();
		return false;
	}
	iAdditional = iBytes > pParams->Bytes ?
		iBytes - pParams->Bytes : 0;
	if ( iAdditional >
		(pParams->Config.MaxBytes - pParams->StorageUsed) ) {
		if ( !xrtQueryParamsCompact(pParams) ) {
			return false;
		}
	}
	if ( !__xrtQueryParamsAddSize(
		pParams->StorageUsed, iAdditional, &iPhysical
	) ) {
		return false;
	}
	return __xrtQueryParamsReservePairs(pParams, iPairs) &&
		__xrtQueryParamsReserveStorage(pParams, iPhysical);
}



/* 返回当前 pair 数量。 */
XRT_API size_t xrtQueryParamsCount(const xqueryparams* pParams)
{
	return pParams != NULL ? pParams->Count : 0;
}



/* 返回当前有效名称和值字节数。 */
XRT_API size_t xrtQueryParamsBytes(const xqueryparams* pParams)
{
	return pParams != NULL ? pParams->Bytes : 0;
}



/* 追加一个拥有型 pair 并保留值存在状态。 */
XRT_API bool xrtQueryParamsAppendPair(
	xqueryparams* pParams,
	xquerypair Pair
)
{
	bytes pStable;
	size_t iPairBytes;
	size_t iPairs;
	size_t iBytes;

	if ( !__xrtQueryParamsPairValid(
		pParams, Pair, &iPairBytes
	) || !__xrtQueryParamsAddSize(
		pParams->Count, 1u, &iPairs
	) || !__xrtQueryParamsAddSize(
		pParams->Bytes, iPairBytes, &iBytes
	) || !__xrtQueryParamsStabilize(
		pParams, &Pair, &pStable
	) ) {
		return false;
	}
	if ( !__xrtQueryParamsPrepare(
		pParams, iPairs, iBytes, iPairBytes
	) ) {
		xrtFree(pStable);
		return false;
	}
	pParams->Pairs[pParams->Count] =
		__xrtQueryParamsStore(pParams, Pair);
	pParams->Count = iPairs;
	pParams->Bytes = iBytes;
	pParams->StoredBytes += iPairBytes;
	xrtFree(pStable);
	return true;
}



/* 追加一个始终带值的常用 pair。 */
XRT_API bool xrtQueryParamsAppend(
	xqueryparams* pParams,
	xstrview Name,
	xstrview Value
)
{
	xquerypair Pair = { XQUERY_HAS_VALUE, Name, Value };

	return xrtQueryParamsAppendPair(pParams, Pair);
}



/* 替换全部同名 pair，并把新值放在首个同名位置。 */
XRT_API bool xrtQueryParamsSetPair(
	xqueryparams* pParams,
	xquerypair Pair
)
{
	bytes pStable;
	xquerypair Stored;
	size_t iPairBytes;
	size_t iRemovedBytes = 0;
	size_t iMatches = 0;
	size_t iFirst = 0;
	size_t iPairs;
	size_t iBytes;
	size_t iRead;
	size_t iWrite;

	if ( !__xrtQueryParamsPairValid(
		pParams, Pair, &iPairBytes
	) ) {
		return false;
	}
	for ( iRead = 0; iRead < pParams->Count; iRead++ ) {
		if ( __xrtQueryParamsNameEqual(
			pParams->Pairs[iRead].Key, Pair.Key
		) ) {
			if ( iMatches == 0 ) {
				iFirst = iRead;
			}
			iMatches++;
			iRemovedBytes += pParams->Pairs[iRead].Key.Size +
				pParams->Pairs[iRead].Value.Size;
		}
	}
	if ( iMatches == 0 ) {
		return xrtQueryParamsAppendPair(pParams, Pair);
	}
	iPairs = pParams->Count - iMatches + 1u;
	iBytes = pParams->Bytes - iRemovedBytes;
	if ( !__xrtQueryParamsAddSize(
		iBytes, iPairBytes, &iBytes
	) || !__xrtQueryParamsStabilize(
		pParams, &Pair, &pStable
	) ) {
		return false;
	}
	if ( !__xrtQueryParamsPrepare(
		pParams, iPairs, iBytes, iPairBytes
	) ) {
		xrtFree(pStable);
		return false;
	}
	Stored = __xrtQueryParamsStore(pParams, Pair);
	pParams->Pairs[iFirst] = Stored;
	iWrite = iFirst + 1u;
	for ( iRead = iFirst + 1u;
		iRead < pParams->Count; iRead++ ) {
		if ( !__xrtQueryParamsNameEqual(
			pParams->Pairs[iRead].Key, Pair.Key
		) ) {
			pParams->Pairs[iWrite++] = pParams->Pairs[iRead];
		}
	}
	pParams->Count = iPairs;
	pParams->Bytes = iBytes;
	pParams->StoredBytes =
		pParams->StoredBytes - iRemovedBytes + iPairBytes;
	xrtFree(pStable);
	return true;
}



/* 用一个始终带值的常用 pair 替换同名项。 */
XRT_API bool xrtQueryParamsSet(
	xqueryparams* pParams,
	xstrview Name,
	xstrview Value
)
{
	xquerypair Pair = { XQUERY_HAS_VALUE, Name, Value };

	return xrtQueryParamsSetPair(pParams, Pair);
}



/* 删除全部同名 pair 并保留其他项顺序。 */
XRT_API size_t xrtQueryParamsRemove(
	xqueryparams* pParams,
	xstrview Name
)
{
	size_t iRemoved = 0;
	size_t iRemovedBytes = 0;
	size_t iWrite = 0;
	size_t iRead;

	if ( !__xrtQueryParamsNameValid(pParams, Name) ) {
		return 0;
	}
	for ( iRead = 0; iRead < pParams->Count; iRead++ ) {
		if ( __xrtQueryParamsNameEqual(
			pParams->Pairs[iRead].Key, Name
		) ) {
			iRemoved++;
			iRemovedBytes += pParams->Pairs[iRead].Key.Size +
				pParams->Pairs[iRead].Value.Size;
			continue;
		}
		if ( iWrite != iRead ) {
			pParams->Pairs[iWrite] = pParams->Pairs[iRead];
		}
		iWrite++;
	}
	pParams->Count = iWrite;
	pParams->Bytes -= iRemovedBytes;
	pParams->StoredBytes -= iRemovedBytes;
	return iRemoved;
}



/* 统计全部同名 pair。 */
XRT_API size_t xrtQueryParamsCountName(
	const xqueryparams* pParams,
	xstrview Name
)
{
	size_t iCount = 0;
	size_t i;

	if ( !__xrtQueryParamsNameValid(pParams, Name) ) {
		return 0;
	}
	for ( i = 0; i < pParams->Count; i++ ) {
		if ( __xrtQueryParamsNameEqual(
			pParams->Pairs[i].Key, Name
		) ) {
			iCount++;
		}
	}
	return iCount;
}



/* 判断是否存在同名 pair。 */
XRT_API bool xrtQueryParamsHas(
	const xqueryparams* pParams,
	xstrview Name
)
{
	return xrtQueryParamsCountName(pParams, Name) != 0;
}



/* 复制指定位置的借用 pair。 */
XRT_API bool xrtQueryParamsAt(
	const xqueryparams* pParams,
	size_t iIndex,
	xquerypair* pPair
)
{
	if ( (pParams == NULL) || (pPair == NULL) ||
		__xrtRangesOverlap(
			pPair, sizeof(*pPair), pParams, sizeof(*pParams)
		) || __xrtRangesOverlap(
			pPair, sizeof(*pPair),
			pParams->Pairs,
			pParams->Count * sizeof(*pParams->Pairs)
		) || __xrtRangesOverlap(
			pPair, sizeof(*pPair),
			pParams->Storage, pParams->StorageUsed
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iIndex >= pParams->Count ) {
		return false;
	}
	*pPair = pParams->Pairs[iIndex];
	return true;
}



/* 复制首个同名 pair。 */
XRT_API bool xrtQueryParamsGet(
	const xqueryparams* pParams,
	xstrview Name,
	xquerypair* pPair
)
{
	size_t iIndex = 0;
	xquerynext Next = xrtQueryParamsFind(
		pParams, Name, &iIndex, pPair
	);

	if ( Next == XQUERY_NEXT_END ) {
		memset(pPair, 0, sizeof(*pPair));
	}
	return Next == XQUERY_NEXT_ITEM;
}



/* 从 Index 开始查找下一个同名 pair。 */
XRT_API xquerynext xrtQueryParamsFind(
	const xqueryparams* pParams,
	xstrview Name,
	size_t* pIndex,
	xquerypair* pPair
)
{
	size_t i;

	if ( !__xrtQueryParamsNameValid(pParams, Name) ||
		(pIndex == NULL) || (pPair == NULL) ||
		((pIndex != NULL) && (*pIndex > pParams->Count)) ||
		__xrtRangesOverlap(
			pIndex, sizeof(*pIndex), pPair, sizeof(*pPair)
		) || __xrtRangesOverlap(
			pIndex, sizeof(*pIndex), pParams, sizeof(*pParams)
		) || __xrtRangesOverlap(
			pPair, sizeof(*pPair), pParams, sizeof(*pParams)
		) || __xrtRangesOverlap(
			pIndex, sizeof(*pIndex),
			pParams->Pairs,
			pParams->Count * sizeof(*pParams->Pairs)
		) || __xrtRangesOverlap(
			pPair, sizeof(*pPair),
			pParams->Pairs,
			pParams->Count * sizeof(*pParams->Pairs)
		) || __xrtRangesOverlap(
			pIndex, sizeof(*pIndex),
			pParams->Storage, pParams->StorageUsed
		) || __xrtRangesOverlap(
			pPair, sizeof(*pPair),
			pParams->Storage, pParams->StorageUsed
		) ) {
		__xrtErrorSetInvalidArgument();
		return XQUERY_NEXT_ERROR;
	}
	for ( i = *pIndex; i < pParams->Count; i++ ) {
		if ( __xrtQueryParamsNameEqual(
			pParams->Pairs[i].Key, Name
		) ) {
			*pPair = pParams->Pairs[i];
			*pIndex = i + 1u;
			return XQUERY_NEXT_ITEM;
		}
	}
	*pIndex = pParams->Count;
	return XQUERY_NEXT_END;
}



/* 原子解析并追加一段 form-urlencoded 文本。 */
XRT_API bool xrtQueryParamsParseAppend(
	xqueryparams* pParams,
	xstrview Text,
	size_t* pErrorOffset
)
{
	xqueryparams* pWork;
	size_t iError;

	if ( (pParams == NULL) ||
		!__xrtQueryViewValid(Text) ||
		((pErrorOffset != NULL) && __xrtRangesOverlap(
			pErrorOffset, sizeof(*pErrorOffset),
			Text.Data, Text.Size
		)) || ((pErrorOffset != NULL) && __xrtRangesOverlap(
			pErrorOffset, sizeof(*pErrorOffset),
			pParams, sizeof(*pParams)
		)) || ((pErrorOffset != NULL) && __xrtRangesOverlap(
			pErrorOffset, sizeof(*pErrorOffset),
			pParams->Pairs,
			pParams->Count * sizeof(*pParams->Pairs)
		)) || ((pErrorOffset != NULL) && __xrtRangesOverlap(
			pErrorOffset, sizeof(*pErrorOffset),
			pParams->Storage, pParams->StorageUsed
		)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pWork = xrtQueryParamsClone(pParams);
	if ( pWork == NULL ) {
		return false;
	}
	if ( !__xrtQueryParamsParseWork(pWork, Text, &iError) ) {
		if ( pErrorOffset != NULL ) {
			*pErrorOffset = iError;
		}
		xrtQueryParamsDestroy(pWork);
		return false;
	}
	__xrtQueryParamsCommit(pParams, pWork);
	xrtQueryParamsDestroy(pWork);
	if ( pErrorOffset != NULL ) {
		*pErrorOffset = Text.Size;
	}
	return true;
}



/* 按名称执行稳定自底向上归并排序。 */
XRT_API bool xrtQueryParamsSort(xqueryparams* pParams)
{
	xquerypair* pTemporary;
	xquerypair* pSource;
	xquerypair* pTarget;
	size_t iWidth;

	if ( pParams == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pParams->Count < 2u ) {
		return true;
	}
	pTemporary = (xquerypair*)xrtMalloc(
		pParams->Count * sizeof(*pTemporary)
	);
	if ( pTemporary == NULL ) {
		return false;
	}
	pSource = pParams->Pairs;
	pTarget = pTemporary;
	for ( iWidth = 1u; iWidth < pParams->Count; ) {
		size_t iStep = iWidth > (SIZE_MAX / 2u) ?
			pParams->Count : iWidth * 2u;
		size_t iBase;

		for ( iBase = 0; iBase < pParams->Count; ) {
			size_t iLeft = iBase;
			size_t iLeftEnd = iBase + iWidth;
			size_t iRight;
			size_t iRightEnd;
			size_t iOutput = iBase;

			if ( iLeftEnd > pParams->Count ) {
				iLeftEnd = pParams->Count;
			}
			iRight = iLeftEnd;
			iRightEnd = iBase > (SIZE_MAX - iStep) ?
				pParams->Count : iBase + iStep;
			if ( iRightEnd > pParams->Count ) {
				iRightEnd = pParams->Count;
			}
			while ( (iLeft < iLeftEnd) &&
				(iRight < iRightEnd) ) {
				if ( __xrtQueryParamsCompare(
					pSource[iLeft].Key,
					pSource[iRight].Key
				) <= 0 ) {
					pTarget[iOutput++] = pSource[iLeft++];
				} else {
					pTarget[iOutput++] = pSource[iRight++];
				}
			}
			while ( iLeft < iLeftEnd ) {
				pTarget[iOutput++] = pSource[iLeft++];
			}
			while ( iRight < iRightEnd ) {
				pTarget[iOutput++] = pSource[iRight++];
			}
			if ( iStep > (pParams->Count - iBase) ) {
				break;
			}
			iBase += iStep;
		}
		{
			xquerypair* pSwap = pSource;
			pSource = pTarget;
			pTarget = pSwap;
		}
		if ( iWidth > (pParams->Count / 2u) ) {
			break;
		}
		iWidth *= 2u;
	}
	if ( pSource != pParams->Pairs ) {
		memcpy(
			pParams->Pairs, pSource,
			pParams->Count * sizeof(*pSource)
		);
	}
	xrtFree(pTemporary);
	return true;
}



/* 把全部有效名称和值重新打包为紧凑字符串区。 */
XRT_API bool xrtQueryParamsCompact(xqueryparams* pParams)
{
	bytes pStorage;
	size_t iUsed = 0;
	size_t i;

	if ( pParams == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pParams->StoredBytes == 0 ) {
		xrtFree(pParams->Storage);
		pParams->Storage = NULL;
		pParams->StorageUsed = 0;
		pParams->StorageCapacity = 0;
		return true;
	}
	if ( (pParams->StorageUsed == pParams->StoredBytes) &&
		(pParams->StorageCapacity == pParams->StoredBytes) ) {
		return true;
	}
	pStorage = (bytes)xrtMalloc(pParams->StoredBytes);
	if ( pStorage == NULL ) {
		return false;
	}
	for ( i = 0; i < pParams->Count; i++ ) {
		if ( pParams->Pairs[i].Key.Size != 0 ) {
			memcpy(
				pStorage + iUsed,
				pParams->Pairs[i].Key.Data,
				pParams->Pairs[i].Key.Size
			);
			pParams->Pairs[i].Key.Data = (cstr)(pStorage + iUsed);
			iUsed += pParams->Pairs[i].Key.Size;
		} else {
			pParams->Pairs[i].Key.Data = NULL;
		}
		if ( pParams->Pairs[i].Value.Size != 0 ) {
			memcpy(
				pStorage + iUsed,
				pParams->Pairs[i].Value.Data,
				pParams->Pairs[i].Value.Size
			);
			pParams->Pairs[i].Value.Data =
				(cstr)(pStorage + iUsed);
			iUsed += pParams->Pairs[i].Value.Size;
		} else {
			pParams->Pairs[i].Value.Data = NULL;
		}
	}
	xrtFree(pParams->Storage);
	pParams->Storage = pStorage;
	pParams->StorageUsed = iUsed;
	pParams->StorageCapacity = iUsed;
	return true;
}



/* 写出 form-urlencoded 文本并保持全部失败路径原子。 */
XRT_API bool xrtQueryParamsWrite(
	const xqueryparams* pParams,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xpercentmap Safe;
	bytes pBytes = (bytes)pOutput;
	size_t iRequired;
	size_t iOutput = 0;
	size_t i;

	if ( (pParams == NULL) || (pSize == NULL) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		__xrtRangesOverlap(
			pSize, sizeof(*pSize), pParams, sizeof(*pParams)
		) || __xrtRangesOverlap(
			pSize, sizeof(*pSize),
			pParams->Pairs,
			pParams->Count * sizeof(*pParams->Pairs)
		) || __xrtRangesOverlap(
			pSize, sizeof(*pSize),
			pParams->Storage, pParams->StorageUsed
		) || ((pOutput != NULL) && (
			__xrtRangesOverlap(
				pOutput, iCapacity, pParams, sizeof(*pParams)
			) || __xrtRangesOverlap(
				pOutput, iCapacity,
				pParams->Pairs,
				pParams->Count * sizeof(*pParams->Pairs)
			) || __xrtRangesOverlap(
				pOutput, iCapacity,
				pParams->Storage, pParams->StorageUsed
			) || __xrtRangesOverlap(
				pOutput, iCapacity, pSize, sizeof(*pSize)
			)
		)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtFormSafeMap(&Safe) ) {
		return false;
	}
	if ( !__xrtQueryParamsMeasure(
		pParams, &Safe, &iRequired
	) ) {
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
	for ( i = 0; i < pParams->Count; i++ ) {
		size_t iWritten;

		if ( i != 0 ) {
			pBytes[iOutput++] = (uint8)'&';
		}
		iWritten = xrtPercentWriteMeasured(
			pParams->Pairs[i].Key.Data,
			pParams->Pairs[i].Key.Size,
			&Safe, true,
			(char*)(pBytes + iOutput)
		);
		iOutput += iWritten;
		if ( (pParams->Pairs[i].Flags &
			XQUERY_HAS_VALUE) != 0 ) {
			pBytes[iOutput++] = (uint8)'=';
			iWritten = xrtPercentWriteMeasured(
				pParams->Pairs[i].Value.Data,
				pParams->Pairs[i].Value.Size,
				&Safe, true,
				(char*)(pBytes + iOutput)
			);
			iOutput += iWritten;
		}
	}
	*pSize = iOutput;
	return true;
}



/* 分配并构建零结尾 form-urlencoded 文本。 */
XRT_API str xrtQueryParamsBuild(
	const xqueryparams* pParams,
	size_t* pSize
)
{
	str sText;
	size_t iRequired;
	size_t iWritten;

	if ( (pParams == NULL) || ((pSize != NULL) && (
		__xrtRangesOverlap(
			pSize, sizeof(*pSize), pParams, sizeof(*pParams)
		) || __xrtRangesOverlap(
			pSize, sizeof(*pSize),
			pParams->Pairs,
			pParams->Count * sizeof(*pParams->Pairs)
		) || __xrtRangesOverlap(
			pSize, sizeof(*pSize),
			pParams->Storage, pParams->StorageUsed
		)
	)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtQueryParamsWrite(
		pParams, NULL, 0, &iRequired
	) ) {
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	sText = (str)xrtMalloc(iRequired + 1u);
	if ( sText == NULL ) {
		return NULL;
	}
	if ( !xrtQueryParamsWrite(
		pParams, sText, iRequired, &iWritten
	) ) {
		xrtFree(sText);
		return NULL;
	}
	sText[iWritten] = '\0';
	if ( pSize != NULL ) {
		*pSize = iWritten;
	}
	return sText;
}

#endif
