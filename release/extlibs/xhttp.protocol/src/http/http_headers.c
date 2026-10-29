#include "../internal/xhttp_internal.h"

#include "../../include/xrt/http_headers.h"



#if defined(XHTTP_FEATURE_HTTP_HEADERS)

/* 动态 Header 以连续字段数组和连续字符串区兼顾遍历速度与内存密度。 */
struct xhttpheaders {
	xhttpheadersconfig Config;
	xhttpfield* Fields;
	char* Storage;
	size_t Count;
	size_t FieldCapacity;
	size_t StorageUsed;
	size_t StorageCapacity;
	size_t Bytes;
	size_t StoredBytes;
};



/* 判断输出是否覆盖容器本体、字段容量或字符串容量。 */
bool __xrtHttpHeadersOwnedOverlap(
	const xhttpheaders* pHeaders,
	const void* pMemory,
	size_t iSize
)
{
	size_t iFieldBytes;

	if ( pHeaders == NULL ) {
		return false;
	}
	if ( !xrtMemRangeValid(pHeaders, sizeof(*pHeaders)) ||
		(pHeaders->FieldCapacity >
		 (SIZE_MAX / sizeof(*pHeaders->Fields))) ) {
		return true;
	}
	iFieldBytes = pHeaders->FieldCapacity *
		sizeof(*pHeaders->Fields);
	return xrtMemRangesOverlap(
		pHeaders, sizeof(*pHeaders), pMemory, iSize
	) || xrtMemRangesOverlap(
		pHeaders->Fields, iFieldBytes, pMemory, iSize
	) || xrtMemRangesOverlap(
		pHeaders->Storage,
		pHeaders->StorageCapacity,
		pMemory,
		iSize
	);
}



/* 安全计算两个 size_t 的和。 */
static bool __xrtHttpHeadersAddSize(
	size_t iLeft,
	size_t iRight,
	size_t* pResult
)
{
	if ( (pResult == NULL) || (iLeft > (SIZE_MAX - iRight)) ) {
		__xhttpErrorSetSizeOverflow();
		return false;
	}
	*pResult = iLeft + iRight;
	return true;
}



/* 返回配置允许的最大物理字符串区，额外字节用于字段结尾的零字符。 */
static bool __xrtHttpHeadersStorageLimit(
	const xhttpheadersconfig* pConfig,
	size_t* pLimit
)
{
	size_t iEnds;

	if ( (pConfig == NULL) || (pLimit == NULL) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	if ( pConfig->MaxFields > (SIZE_MAX / 2u) ) {
		__xhttpErrorSetSizeOverflow();
		return false;
	}
	iEnds = pConfig->MaxFields * 2u;
	return __xrtHttpHeadersAddSize(pConfig->MaxBytes, iEnds, pLimit);
}



/* 验证 Header 容器的逻辑容量和物理字符串区上限。 */
XRT_API bool xrtHttpHeadersConfigValid(
	const xhttpheadersconfig* pConfig
)
{
	size_t iLimit;

	if ( pConfig == NULL ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	if ( (pConfig->InitialFields > pConfig->MaxFields) ||
		(pConfig->InitialBytes > pConfig->MaxBytes) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	return __xrtHttpHeadersStorageLimit(pConfig, &iLimit);
}



/* 解析可选配置并验证初始容量、逻辑限额和物理上限。 */
static bool __xrtHttpHeadersConfigResolve(
	const xhttpheadersconfig* pInput,
	xhttpheadersconfig* pConfig
)
{
	xrtHttpHeadersConfigInit(pConfig);
	if ( pInput != NULL ) {
		if ( !xrtMemRangeValid(pInput, sizeof(*pInput)) ) {
			__xhttpErrorSetInvalidArgument();
			return false;
		}
		memcpy(pConfig, pInput, sizeof(*pConfig));
	}
	return xrtHttpHeadersConfigValid(pConfig);
}



/* 验证公开输出不会覆盖容器对象及其拥有的连续存储。 */
static bool __xrtHttpHeadersOutputValid(
	const xhttpheaders* pHeaders,
	const void* pOutput,
	size_t iSize
)
{
	if ( !xrtMemRangeValid(pOutput, iSize) ) {
		return false;
	}
	if ( pHeaders == NULL ) {
		return true;
	}
	return !xrtMemRangesOverlap(
		pOutput, iSize, pHeaders, sizeof(*pHeaders)
	) && !xrtMemRangesOverlap(
		pOutput,
		iSize,
		pHeaders->Fields,
		pHeaders->FieldCapacity * sizeof(*pHeaders->Fields)
	) && !xrtMemRangesOverlap(
		pOutput,
		iSize,
		pHeaders->Storage,
		pHeaders->StorageCapacity
	);
}



/* 验证字段语法和容器配置允许的单字段长度。 */
static bool __xrtHttpHeadersFieldValid(
	const xhttpheaders* pHeaders,
	xstrview Name,
	xstrview Value,
	size_t* pBytes
)
{
	if ( (pHeaders == NULL) || (pBytes == NULL) ||
		!__xhttpViewValid(Name) || !__xhttpViewValid(Value) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtHttpTokenValid(Name) ||
		!xrtHttpFieldValueValid(Value) ) {
		__xhttpErrorSetValue();
		return false;
	}
	if ( (Name.Size > pHeaders->Config.MaxName) ||
		(Value.Size > pHeaders->Config.MaxValue) ) {
		__xhttpErrorSetRange();
		return false;
	}
	if ( !__xrtHttpHeadersAddSize(Name.Size, Value.Size, pBytes) ) {
		return false;
	}
	if ( *pBytes > pHeaders->Config.MaxBytes ) {
		__xhttpErrorSetRange();
		return false;
	}
	return true;
}



/* 验证只读查找和删除入口使用的字段名。 */
static bool __xrtHttpHeadersLookupValid(
	const xhttpheaders* pHeaders,
	xstrview Name
)
{
	if ( pHeaders == NULL ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	return __xhttpLookupNameValid(Name);
}



/* 判断调用方输入是否借用了当前字符串区。 */
static bool __xrtHttpHeadersAliases(
	const xhttpheaders* pHeaders,
	xstrview Text
)
{
	uintptr_t iStorage;
	uintptr_t iText;

	if ( (pHeaders->Storage == NULL) ||
		(Text.Data == NULL) || (Text.Size == 0) ) {
		return false;
	}
	iStorage = (uintptr_t)(const void*)pHeaders->Storage;
	iText = (uintptr_t)(const void*)Text.Data;
	if ( (iStorage > (UINTPTR_MAX - pHeaders->StorageUsed)) ||
		(iText > (UINTPTR_MAX - Text.Size)) ) {
		return true;
	}
	return (iText < (iStorage + pHeaders->StorageUsed)) &&
		(iStorage < (iText + Text.Size));
}



/* 在扩容或压缩前复制借用当前容器的调用方输入。 */
static bool __xrtHttpHeadersStabilize(
	const xhttpheaders* pHeaders,
	xstrview* pName,
	xstrview* pValue,
	char** ppStorage
)
{
	size_t iBytes;
	char* pStorage;

	*ppStorage = NULL;
	if ( !__xrtHttpHeadersAliases(pHeaders, *pName) &&
		!__xrtHttpHeadersAliases(pHeaders, *pValue) ) {
		return true;
	}
	if ( !__xrtHttpHeadersAddSize(
		pName->Size, pValue->Size, &iBytes
	) ) {
		return false;
	}
	pStorage = (char*)xrtMalloc(iBytes);
	if ( pStorage == NULL ) {
		return false;
	}
	memmove(pStorage, pName->Data, pName->Size);
	if ( pValue->Size != 0 ) {
		memmove(pStorage + pName->Size, pValue->Data, pValue->Size);
	}
	pName->Data = pStorage;
	pValue->Data = pStorage + pName->Size;
	*ppStorage = pStorage;
	return true;
}



/* 按倍增策略预留字段数组，同时不超过配置上限。 */
static bool __xrtHttpHeadersReserveFields(
	xhttpheaders* pHeaders,
	size_t iRequired
)
{
	xhttpfield* pFields;
	size_t iCapacity;

	if ( iRequired <= pHeaders->FieldCapacity ) {
		return true;
	}
	if ( iRequired > pHeaders->Config.MaxFields ) {
		__xhttpErrorSetRange();
		return false;
	}
	if ( iRequired > (SIZE_MAX / sizeof(*pFields)) ) {
		__xhttpErrorSetSizeOverflow();
		return false;
	}
	iCapacity = pHeaders->FieldCapacity;
	if ( iCapacity == 0 ) {
		iCapacity = pHeaders->Config.InitialFields;
	}
	if ( iCapacity == 0 ) {
		iCapacity = 8;
	}
	while ( iCapacity < iRequired ) {
		size_t iNext = (iCapacity > (SIZE_MAX / 2u)) ?
			iRequired : (iCapacity * 2u);

		if ( iNext > pHeaders->Config.MaxFields ) {
			iNext = pHeaders->Config.MaxFields;
		}
		if ( iNext <= iCapacity ) {
			iCapacity = iRequired;
			break;
		}
		iCapacity = iNext;
	}
	pFields = (xhttpfield*)xrtRealloc(
		pHeaders->Fields, iCapacity * sizeof(*pFields)
	);
	if ( pFields == NULL ) {
		return false;
	}
	pHeaders->Fields = pFields;
	pHeaders->FieldCapacity = iCapacity;
	return true;
}



/* 移动字符串区并同步修正全部公开字段视图。 */
static bool __xrtHttpHeadersReserveStorage(
	xhttpheaders* pHeaders,
	size_t iRequired
)
{
	size_t iLimit;
	size_t iCapacity;
	size_t i;
	char* pStorage;

	if ( iRequired <= pHeaders->StorageCapacity ) {
		return true;
	}
	if ( !__xrtHttpHeadersStorageLimit(
		&pHeaders->Config, &iLimit
	) ) {
		return false;
	}
	if ( iRequired > iLimit ) {
		__xhttpErrorSetRange();
		return false;
	}
	iCapacity = pHeaders->StorageCapacity;
	if ( iCapacity == 0 ) {
		if ( !__xrtHttpHeadersAddSize(
			pHeaders->Config.InitialBytes,
			pHeaders->Config.InitialFields * 2u,
			&iCapacity
		) ) {
			return false;
		}
	}
	if ( iCapacity == 0 ) {
		iCapacity = 512;
	}
	while ( iCapacity < iRequired ) {
		size_t iNext = (iCapacity > (SIZE_MAX / 2u)) ?
			iRequired : (iCapacity * 2u);

		if ( iNext > iLimit ) {
			iNext = iLimit;
		}
		if ( iNext <= iCapacity ) {
			iCapacity = iRequired;
			break;
		}
		iCapacity = iNext;
	}
	pStorage = (char*)xrtMalloc(iCapacity);
	if ( pStorage == NULL ) {
		return false;
	}
	if ( pHeaders->StorageUsed != 0 ) {
		memcpy(pStorage, pHeaders->Storage, pHeaders->StorageUsed);
	}
	for ( i = 0; i < pHeaders->Count; i++ ) {
		size_t iName = (size_t)(
			pHeaders->Fields[i].Name.Data - pHeaders->Storage
		);
		size_t iValue = (size_t)(
			pHeaders->Fields[i].Value.Data - pHeaders->Storage
		);

		pHeaders->Fields[i].Name.Data = pStorage + iName;
		pHeaders->Fields[i].Value.Data = pStorage + iValue;
	}
	xrtFree(pHeaders->Storage);
	pHeaders->Storage = pStorage;
	pHeaders->StorageCapacity = iCapacity;
	return true;
}



/* 按增长或收缩策略计算满足有效内容的物理字符串容量。 */
static bool __xrtHttpHeadersCapacity(
	xhttpheaders* pHeaders,
	size_t iRequired,
	bool bShrink,
	size_t* pCapacity
)
{
	size_t iLimit;
	size_t iCapacity;

	if ( !__xrtHttpHeadersStorageLimit(
		&pHeaders->Config, &iLimit
	) ) {
		return false;
	}
	if ( iRequired > iLimit ) {
		__xhttpErrorSetRange();
		return false;
	}
	iCapacity = bShrink ? 0 : pHeaders->StorageCapacity;
	if ( iCapacity == 0 ) {
		if ( !__xrtHttpHeadersAddSize(
			pHeaders->Config.InitialBytes,
			pHeaders->Config.InitialFields * 2u,
			&iCapacity
		) ) {
			return false;
		}
	}
	if ( iCapacity == 0 ) {
		iCapacity = 512;
	}
	if ( iCapacity > iLimit ) {
		iCapacity = iLimit;
	}
	while ( iCapacity < iRequired ) {
		size_t iNext = (iCapacity > (SIZE_MAX / 2u)) ?
			iRequired : (iCapacity * 2u);

		if ( iNext > iLimit ) {
			iNext = iLimit;
		}
		if ( iNext <= iCapacity ) {
			iCapacity = iRequired;
			break;
		}
		iCapacity = iNext;
	}
	*pCapacity = iCapacity;
	return true;
}



/* 把有效字符串重排到新存储，并按增长或收缩策略一次性提交。 */
static bool __xrtHttpHeadersRepack(
	xhttpheaders* pHeaders,
	size_t iRequired,
	bool bShrink
)
{
	size_t iCapacity;
	size_t iPosition = 0;
	size_t i;
	char* pStorage;

	if ( (iRequired < pHeaders->StoredBytes) ||
		!__xrtHttpHeadersCapacity(
			pHeaders, iRequired, bShrink, &iCapacity
		) ) {
		if ( iRequired < pHeaders->StoredBytes ) {
			__xhttpErrorSetRange();
		}
		return false;
	}
	pStorage = (char*)xrtMalloc(iCapacity);
	if ( pStorage == NULL ) {
		return false;
	}
	for ( i = 0; i < pHeaders->Count; i++ ) {
		xhttpfield* pField = &pHeaders->Fields[i];

		memcpy(
			pStorage + iPosition,
			pField->Name.Data,
			pField->Name.Size + 1u
		);
		pField->Name.Data = pStorage + iPosition;
		iPosition += pField->Name.Size + 1u;
		memcpy(
			pStorage + iPosition,
			pField->Value.Data,
			pField->Value.Size + 1u
		);
		pField->Value.Data = pStorage + iPosition;
		iPosition += pField->Value.Size + 1u;
	}
	xrtFree(pHeaders->Storage);
	pHeaders->Storage = pStorage;
	pHeaders->StorageUsed = iPosition;
	pHeaders->StorageCapacity = iCapacity;
	return true;
}



/* 在物理空间不足时直接构造 Set 的最终字符串区，避免旧值占用事务空间。 */
static bool __xrtHttpHeadersSetRepack(
	xhttpheaders* pHeaders,
	xstrview Name,
	xstrview Value,
	size_t iFirst,
	size_t iNewCount,
	size_t iNewBytes,
	size_t iNewStored
)
{
	size_t iCapacity;
	size_t iPosition = 0;
	size_t iRead;
	size_t iWrite = 0;
	char* pStorage;

	if ( !__xrtHttpHeadersCapacity(
		pHeaders, iNewStored, false, &iCapacity
	) ) {
		return false;
	}
	pStorage = (char*)xrtMalloc(iCapacity);
	if ( pStorage == NULL ) {
		return false;
	}
	for ( iRead = 0; iRead < pHeaders->Count; iRead++ ) {
		xhttpfield Source = pHeaders->Fields[iRead];
		xhttpfield Field;

		if ( iRead == iFirst ) {
			Field.Name.Data = pStorage + iPosition;
			Field.Name.Size = Name.Size;
			memcpy(pStorage + iPosition, Name.Data, Name.Size);
			iPosition += Name.Size;
			pStorage[iPosition++] = '\0';
			Field.Value.Data = pStorage + iPosition;
			Field.Value.Size = Value.Size;
			if ( Value.Size != 0 ) {
				memcpy(pStorage + iPosition, Value.Data, Value.Size);
			}
			iPosition += Value.Size;
			pStorage[iPosition++] = '\0';
			pHeaders->Fields[iWrite++] = Field;
		}
		if ( xrtHttpFieldNameEqual(Source.Name, Name) ) {
			continue;
		}
		Field.Name.Data = pStorage + iPosition;
		Field.Name.Size = Source.Name.Size;
		memcpy(
			pStorage + iPosition,
			Source.Name.Data,
			Source.Name.Size + 1u
		);
		iPosition += Source.Name.Size + 1u;
		Field.Value.Data = pStorage + iPosition;
		Field.Value.Size = Source.Value.Size;
		memcpy(
			pStorage + iPosition,
			Source.Value.Data,
			Source.Value.Size + 1u
		);
		iPosition += Source.Value.Size + 1u;
		pHeaders->Fields[iWrite++] = Field;
	}
	xrtFree(pHeaders->Storage);
	pHeaders->Storage = pStorage;
	pHeaders->StorageUsed = iPosition;
	pHeaders->StorageCapacity = iCapacity;
	pHeaders->Count = iNewCount;
	pHeaders->Bytes = iNewBytes;
	pHeaders->StoredBytes = iNewStored;
	return true;
}



/* 把一份已经验证且稳定的字段复制到字符串区末尾。 */
static bool __xrtHttpHeadersStore(
	xhttpheaders* pHeaders,
	xstrview Name,
	xstrview Value,
	xhttpfield* pField
)
{
	size_t iFieldBytes;
	size_t iRequired;

	if ( !__xrtHttpHeadersAddSize(Name.Size, Value.Size, &iFieldBytes) ||
		!__xrtHttpHeadersAddSize(iFieldBytes, 2u, &iFieldBytes) ) {
		return false;
	}
	if ( (pHeaders->StorageCapacity < pHeaders->StorageUsed) ||
		(iFieldBytes >
		 (pHeaders->StorageCapacity - pHeaders->StorageUsed)) ) {
		if ( !__xrtHttpHeadersAddSize(
			pHeaders->StoredBytes, iFieldBytes, &iRequired
		) ) {
			return false;
		}
		if ( pHeaders->StorageUsed != pHeaders->StoredBytes ) {
			if ( !__xrtHttpHeadersRepack(
				pHeaders, iRequired, false
			) ) {
				return false;
			}
		} else if ( !__xrtHttpHeadersReserveStorage(
			pHeaders, iRequired
		) ) {
			return false;
		}
	}
	pField->Name.Data = pHeaders->Storage + pHeaders->StorageUsed;
	pField->Name.Size = Name.Size;
	memcpy(pHeaders->Storage + pHeaders->StorageUsed, Name.Data, Name.Size);
	pHeaders->StorageUsed += Name.Size;
	pHeaders->Storage[pHeaders->StorageUsed++] = '\0';
	pField->Value.Data = pHeaders->Storage + pHeaders->StorageUsed;
	pField->Value.Size = Value.Size;
	if ( Value.Size != 0 ) {
		memcpy(
			pHeaders->Storage + pHeaders->StorageUsed,
			Value.Data,
			Value.Size
		);
	}
	pHeaders->StorageUsed += Value.Size;
	pHeaders->Storage[pHeaders->StorageUsed++] = '\0';
	return true;
}



/* 把严格 CRLF 字段块解析到工作容器，调用方负责事务边界。 */
static bool __xrtHttpHeadersParseInto(
	xhttpheaders* pHeaders,
	xstrview Block,
	size_t* pErrorOffset
)
{
	size_t iOffset = 0;

	if ( pErrorOffset != NULL ) {
		*pErrorOffset = 0;
	}
	if ( (pHeaders == NULL) || !__xhttpViewValid(Block) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	while ( iOffset < Block.Size ) {
		size_t iLineStart = iOffset;
		size_t iLineEnd = iOffset;
		xhttpfield Field;

		while ( (iLineEnd < Block.Size) &&
			(Block.Data[iLineEnd] != '\r') &&
			(Block.Data[iLineEnd] != '\n') ) {
			iLineEnd++;
		}
		if ( iLineEnd < Block.Size ) {
			if ( (Block.Data[iLineEnd] != '\r') ||
				((iLineEnd + 1u) >= Block.Size) ||
				(Block.Data[iLineEnd + 1u] != '\n') ) {
				if ( pErrorOffset != NULL ) {
					*pErrorOffset = iLineEnd;
				}
				__xhttpErrorSetValue();
				return false;
			}
			iOffset = iLineEnd + 2u;
		} else {
			iOffset = iLineEnd;
		}
		if ( iLineEnd == iLineStart ) {
			if ( iOffset != Block.Size ) {
				if ( pErrorOffset != NULL ) {
					*pErrorOffset = iOffset;
				}
				__xhttpErrorSetValue();
				return false;
			}
			break;
		}
		if ( !xrtHttpFieldParse(
			(xstrview){
				Block.Data + iLineStart,
				iLineEnd - iLineStart
			},
			&Field
		) ) {
			if ( pErrorOffset != NULL ) {
				*pErrorOffset = iLineStart;
			}
			__xhttpErrorSetValue();
			return false;
		}
		if ( !xrtHttpHeadersAdd(
			pHeaders, Field.Name, Field.Value
		) ) {
			if ( pErrorOffset != NULL ) {
				*pErrorOffset = iLineStart;
			}
			return false;
		}
	}
	return true;
}



/* 初始化动态 Header 的默认容量与安全上限。 */
XRT_API void xrtHttpHeadersConfigInit(xhttpheadersconfig* pConfig)
{
	xhttpheadersconfig Config;

	if ( pConfig == NULL ) {
		__xhttpErrorSetInvalidArgument();
		return;
	}
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xhttpErrorSetInvalidArgument();
		return;
	}
	Config.InitialFields = 8;
	Config.InitialBytes = 512;
	Config.MaxFields = 1024;
	Config.MaxName = 1024u * 1024u;
	Config.MaxValue = 1024u * 1024u;
	Config.MaxBytes = 1024u * 1024u;
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 创建拥有字段内容的动态 Header 容器。 */
XRT_API xhttpheaders* xrtHttpHeadersCreate(
	const xhttpheadersconfig* pConfig
)
{
	xhttpheadersconfig Config;
	xhttpheaders* pHeaders;

	if ( !__xrtHttpHeadersConfigResolve(pConfig, &Config) ) {
		return NULL;
	}
	pHeaders = (xhttpheaders*)xrtCalloc(1, sizeof(*pHeaders));
	if ( pHeaders == NULL ) {
		return NULL;
	}
	pHeaders->Config = Config;
	if ( !xrtHttpHeadersReserve(
		pHeaders, Config.InitialFields, Config.InitialBytes
	) ) {
		xrtHttpHeadersDestroy(pHeaders);
		return NULL;
	}
	return pHeaders;
}



/* 释放动态 Header 的字段、字符串区和对象本身。 */
XRT_API void xrtHttpHeadersDestroy(xhttpheaders* pHeaders)
{
	if ( pHeaders == NULL ) {
		return;
	}
	xrtFree(pHeaders->Fields);
	xrtFree(pHeaders->Storage);
	memset(pHeaders, 0, sizeof(*pHeaders));
	xrtFree(pHeaders);
}



/* 清空逻辑内容并保留容量供下一条消息复用。 */
XRT_API void xrtHttpHeadersClear(xhttpheaders* pHeaders)
{
	if ( pHeaders == NULL ) {
		__xhttpErrorSetInvalidArgument();
		return;
	}
	pHeaders->Count = 0;
	pHeaders->StorageUsed = 0;
	pHeaders->Bytes = 0;
	pHeaders->StoredBytes = 0;
}



/* 预留字段数组与有效内容对应的字符串区。 */
XRT_API bool xrtHttpHeadersReserve(
	xhttpheaders* pHeaders,
	size_t iFields,
	size_t iBytes
)
{
	size_t iEnds;
	size_t iStorage;

	if ( pHeaders == NULL ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	if ( (iFields > pHeaders->Config.MaxFields) ||
		(iBytes > pHeaders->Config.MaxBytes) ||
		(iFields > (SIZE_MAX / 2u)) ) {
		__xhttpErrorSetRange();
		return false;
	}
	iEnds = iFields * 2u;
	if ( !__xrtHttpHeadersAddSize(iBytes, iEnds, &iStorage) ||
		!__xrtHttpHeadersReserveFields(pHeaders, iFields) ||
		!__xrtHttpHeadersReserveStorage(pHeaders, iStorage) ) {
		return false;
	}
	return true;
}



/* 把所有有效字符串复制到紧凑的新存储。 */
XRT_API bool xrtHttpHeadersCompact(xhttpheaders* pHeaders)
{
	if ( pHeaders == NULL ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	if ( pHeaders->StorageUsed == pHeaders->StoredBytes ) {
		return true;
	}
	if ( pHeaders->StoredBytes == 0 ) {
		pHeaders->StorageUsed = 0;
		return true;
	}
	return __xrtHttpHeadersRepack(
		pHeaders, pHeaders->StoredBytes, true
	);
}



/* 返回容器中的字段数量。 */
XRT_API size_t xrtHttpHeadersCount(const xhttpheaders* pHeaders)
{
	if ( pHeaders == NULL ) {
		__xhttpErrorSetInvalidArgument();
		return 0;
	}
	return pHeaders->Count;
}



/* 返回全部有效名称和值的总字节数。 */
XRT_API size_t xrtHttpHeadersBytes(const xhttpheaders* pHeaders)
{
	if ( pHeaders == NULL ) {
		__xhttpErrorSetInvalidArgument();
		return 0;
	}
	return pHeaders->Bytes;
}



/* 返回便于直接遍历和协议写出的连续字段数组。 */
XRT_API const xhttpfield* xrtHttpHeadersData(
	const xhttpheaders* pHeaders
)
{
	if ( pHeaders == NULL ) {
		__xhttpErrorSetInvalidArgument();
		return NULL;
	}
	return (pHeaders->Count == 0) ? NULL : pHeaders->Fields;
}



/* 返回指定位置的借用字段。 */
XRT_API const xhttpfield* xrtHttpHeadersAt(
	const xhttpheaders* pHeaders,
	size_t iIndex
)
{
	if ( pHeaders == NULL ) {
		__xhttpErrorSetInvalidArgument();
		return NULL;
	}
	if ( iIndex >= pHeaders->Count ) {
		return NULL;
	}
	return &pHeaders->Fields[iIndex];
}



/* 追加允许重名且拥有内容副本的新字段。 */
XRT_API bool xrtHttpHeadersAdd(
	xhttpheaders* pHeaders,
	xstrview Name,
	xstrview Value
)
{
	xhttpfield Field;
	size_t iFieldBytes;
	size_t iNewBytes;
	xstrview StableName = Name;
	xstrview StableValue = Value;
	char* pStable;

	if ( !__xrtHttpHeadersFieldValid(
		pHeaders, Name, Value, &iFieldBytes
	) ) {
		return false;
	}
	if ( pHeaders->Count >= pHeaders->Config.MaxFields ) {
		__xhttpErrorSetRange();
		return false;
	}
	if ( !__xrtHttpHeadersAddSize(
		pHeaders->Bytes, iFieldBytes, &iNewBytes
	) ) {
		return false;
	}
	if ( iNewBytes > pHeaders->Config.MaxBytes ) {
		__xhttpErrorSetRange();
		return false;
	}
	if ( !__xrtHttpHeadersStabilize(
		pHeaders, &StableName, &StableValue, &pStable
	) ) {
		return false;
	}
	if ( !__xrtHttpHeadersReserveFields(
		pHeaders, pHeaders->Count + 1u
	) || !__xrtHttpHeadersStore(
		pHeaders, StableName, StableValue, &Field
	) ) {
		xrtFree(pStable);
		return false;
	}
	pHeaders->Fields[pHeaders->Count++] = Field;
	pHeaders->Bytes = iNewBytes;
	pHeaders->StoredBytes += iFieldBytes + 2u;
	xrtFree(pStable);
	return true;
}



/* 替换首个同名位置并折叠其余同名字段。 */
XRT_API bool xrtHttpHeadersSet(
	xhttpheaders* pHeaders,
	xstrview Name,
	xstrview Value
)
{
	xhttpfield Field;
	size_t iAddedBytes;
	size_t iRemovedBytes = 0;
	size_t iRemovedStorage = 0;
	size_t iMatches = 0;
	size_t iFirst = XRT_NPOS;
	size_t iNewBytes;
	size_t iNewCount;
	size_t iNewStored;
	size_t iFieldStored;
	size_t i;
	xstrview StableName = Name;
	xstrview StableValue = Value;
	char* pStable;

	if ( !__xrtHttpHeadersFieldValid(
		pHeaders, Name, Value, &iAddedBytes
	) || !__xrtHttpHeadersStabilize(
		pHeaders, &StableName, &StableValue, &pStable
	) ) {
		return false;
	}
	for ( i = 0; i < pHeaders->Count; i++ ) {
		const xhttpfield* pField = &pHeaders->Fields[i];

		if ( !xrtHttpFieldNameEqual(pField->Name, StableName) ) {
			continue;
		}
		if ( iFirst == XRT_NPOS ) {
			iFirst = i;
		}
		iMatches++;
		iRemovedBytes += pField->Name.Size + pField->Value.Size;
		iRemovedStorage += pField->Name.Size + pField->Value.Size + 2u;
	}
	iNewBytes = pHeaders->Bytes - iRemovedBytes;
	if ( !__xrtHttpHeadersAddSize(
		iNewBytes, iAddedBytes, &iNewBytes
	) ) {
		xrtFree(pStable);
		return false;
	}
	if ( iNewBytes > pHeaders->Config.MaxBytes ) {
		xrtFree(pStable);
		__xhttpErrorSetRange();
		return false;
	}
	iNewCount = pHeaders->Count - iMatches + 1u;
	if ( !__xrtHttpHeadersAddSize(
		iAddedBytes, 2u, &iFieldStored
	) || !__xrtHttpHeadersAddSize(
		pHeaders->StoredBytes - iRemovedStorage,
		iFieldStored,
		&iNewStored
	) ) {
		xrtFree(pStable);
		return false;
	}
	if ( (iMatches != 0) &&
		((pHeaders->StorageCapacity < pHeaders->StorageUsed) ||
		 (iFieldStored >
		  (pHeaders->StorageCapacity - pHeaders->StorageUsed))) ) {
		bool bResult = __xrtHttpHeadersSetRepack(
			pHeaders,
			StableName,
			StableValue,
			iFirst,
			iNewCount,
			iNewBytes,
			iNewStored
		);

		xrtFree(pStable);
		return bResult;
	}
	if ( iNewCount > pHeaders->Config.MaxFields ) {
		xrtFree(pStable);
		__xhttpErrorSetRange();
		return false;
	}
	if ( !__xrtHttpHeadersReserveFields(pHeaders, iNewCount) ||
		!__xrtHttpHeadersStore(
			pHeaders, StableName, StableValue, &Field
		) ) {
		xrtFree(pStable);
		return false;
	}
	if ( iMatches == 0 ) {
		pHeaders->Fields[pHeaders->Count++] = Field;
	} else {
		size_t iRead;
		size_t iWrite = iFirst + 1u;

		pHeaders->Fields[iFirst] = Field;
		for ( iRead = iFirst + 1u; iRead < pHeaders->Count; iRead++ ) {
			if ( xrtHttpFieldNameEqual(
				pHeaders->Fields[iRead].Name, StableName
			) ) {
				continue;
			}
			if ( iWrite != iRead ) {
				pHeaders->Fields[iWrite] = pHeaders->Fields[iRead];
			}
			iWrite++;
		}
		pHeaders->Count = iWrite;
	}
	pHeaders->Bytes = iNewBytes;
	pHeaders->StoredBytes = iNewStored;
	xrtFree(pStable);
	return true;
}



/* 删除全部同名字段并压紧字段数组。 */
XRT_API size_t xrtHttpHeadersRemove(
	xhttpheaders* pHeaders,
	xstrview Name
)
{
	size_t iRead;
	size_t iWrite = 0;
	size_t iRemoved = 0;

	if ( !__xrtHttpHeadersLookupValid(pHeaders, Name) ) {
		return 0;
	}
	for ( iRead = 0; iRead < pHeaders->Count; iRead++ ) {
		const xhttpfield* pField = &pHeaders->Fields[iRead];

		if ( xrtHttpFieldNameEqual(pField->Name, Name) ) {
			pHeaders->Bytes -= pField->Name.Size + pField->Value.Size;
			pHeaders->StoredBytes -=
				pField->Name.Size + pField->Value.Size + 2u;
			iRemoved++;
			continue;
		}
		if ( iWrite != iRead ) {
			pHeaders->Fields[iWrite] = pHeaders->Fields[iRead];
		}
		iWrite++;
	}
	pHeaders->Count = iWrite;
	if ( pHeaders->StoredBytes == 0 ) {
		pHeaders->StorageUsed = 0;
	}
	return iRemoved;
}



/* 判断容器是否包含指定字段名。 */
XRT_API bool xrtHttpHeadersHas(
	const xhttpheaders* pHeaders,
	xstrview Name
)
{
	return xrtHttpHeadersGet(pHeaders, Name) != NULL;
}



/* 统计容器中的同名字段。 */
XRT_API size_t xrtHttpHeadersCountName(
	const xhttpheaders* pHeaders,
	xstrview Name
)
{
	if ( !__xrtHttpHeadersLookupValid(pHeaders, Name) ) {
		return 0;
	}
	return xrtHttpFieldCount(pHeaders->Fields, pHeaders->Count, Name);
}



/* 返回第一个同名借用字段。 */
XRT_API const xhttpfield* xrtHttpHeadersGet(
	const xhttpheaders* pHeaders,
	xstrview Name
)
{
	return xrtHttpHeadersGetNth(pHeaders, Name, 0);
}



/* 返回唯一同名借用字段，并把重复字段作为值错误报告。 */
XRT_API xhttpnext xrtHttpHeadersGetUnique(
	const xhttpheaders* pHeaders,
	xstrview Name,
	const xhttpfield** ppField
)
{
	const xhttpfield* pField = NULL;
	xhttpnext Result;

	if ( (ppField == NULL) || !__xrtHttpHeadersOutputValid(
		pHeaders, ppField, sizeof(*ppField)
	) ) {
		__xhttpErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(ppField, &pField, sizeof(pField));
	if ( !__xrtHttpHeadersLookupValid(pHeaders, Name) ) {
		return XHTTP_NEXT_ERROR;
	}
	Result = xrtHttpFieldGetUnique(
		pHeaders->Fields,
		pHeaders->Count,
		Name,
		&pField
	);
	memcpy(ppField, &pField, sizeof(pField));
	return Result;
}



/* 返回第 N 个同名借用字段。 */
XRT_API const xhttpfield* xrtHttpHeadersGetNth(
	const xhttpheaders* pHeaders,
	xstrview Name,
	size_t iIndex
)
{
	size_t iPosition = 0;

	if ( !__xrtHttpHeadersLookupValid(pHeaders, Name) ) {
		return NULL;
	}
	while ( iPosition < pHeaders->Count ) {
		iPosition = xrtHttpFieldFind(
			pHeaders->Fields, pHeaders->Count, Name, iPosition
		);
		if ( iPosition == XRT_NPOS ) {
			return NULL;
		}
		if ( iIndex == 0 ) {
			return &pHeaders->Fields[iPosition];
		}
		iIndex--;
		iPosition++;
	}
	return NULL;
}



/* 复制全部同名借用值，并始终返回完整匹配数量。 */
XRT_API size_t xrtHttpHeadersGetAll(
	const xhttpheaders* pHeaders,
	xstrview Name,
	xstrview* pValues,
	size_t iCapacity
)
{
	size_t iOutputSize;
	size_t iFound = 0;
	size_t i;

	if ( iCapacity > (SIZE_MAX / sizeof(*pValues)) ) {
		__xhttpErrorSetSizeOverflow();
		return 0;
	}
	iOutputSize = iCapacity * sizeof(*pValues);
	if ( !__xrtHttpHeadersLookupValid(pHeaders, Name) ) {
		return 0;
	}
	if ( !__xrtHttpHeadersOutputValid(
		pHeaders, pValues, iOutputSize
	) ) {
		__xhttpErrorSetInvalidArgument();
		return 0;
	}
	for ( i = 0; i < pHeaders->Count; i++ ) {
		if ( !xrtHttpFieldNameEqual(
			pHeaders->Fields[i].Name, Name
		) ) {
			continue;
		}
		if ( iFound < iCapacity ) {
			memcpy(
				(uint8*)(void*)pValues +
					(iFound * sizeof(*pValues)),
				&pHeaders->Fields[i].Value,
				sizeof(*pValues)
			);
		}
		iFound++;
	}
	return iFound;
}



/* 深复制 Header 容器及其配置。 */
XRT_API xhttpheaders* xrtHttpHeadersClone(
	const xhttpheaders* pHeaders
)
{
	xhttpheaders* pClone;
	size_t i;

	if ( pHeaders == NULL ) {
		__xhttpErrorSetInvalidArgument();
		return NULL;
	}
	pClone = xrtHttpHeadersCreate(&pHeaders->Config);
	if ( pClone == NULL ) {
		return NULL;
	}
	if ( !xrtHttpHeadersReserve(
		pClone, pHeaders->Count, pHeaders->Bytes
	) ) {
		xrtHttpHeadersDestroy(pClone);
		return NULL;
	}
	for ( i = 0; i < pHeaders->Count; i++ ) {
		if ( !xrtHttpHeadersAdd(
			pClone,
			pHeaders->Fields[i].Name,
			pHeaders->Fields[i].Value
		) ) {
			xrtHttpHeadersDestroy(pClone);
			return NULL;
		}
	}
	return pClone;
}



/* 不分配地交换两个完整容器；容器地址不变，拥有的状态随之交换。 */
XRT_API bool xrtHttpHeadersSwap(
	xhttpheaders* pLeft,
	xhttpheaders* pRight
)
{
	xhttpheaders Swap;

	if ( (pLeft == NULL) || (pRight == NULL) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	if ( pLeft == pRight ) {
		return true;
	}
	Swap = *pLeft;
	*pLeft = *pRight;
	*pRight = Swap;
	return true;
}



/* 在独立副本中解析字段块，成功后一次性交换整个容器状态。 */
XRT_API bool xrtHttpHeadersAddBlock(
	xhttpheaders* pHeaders,
	xstrview Block,
	size_t* pErrorOffset
)
{
	xhttpheaders* pWork;
	size_t iErrorOffset = 0;

	if ( (pHeaders == NULL) || !__xhttpViewValid(Block) ||
		((pErrorOffset != NULL) &&
		 (!__xrtHttpHeadersOutputValid(
			pHeaders, pErrorOffset, sizeof(*pErrorOffset)
		 ) || xrtMemRangesOverlap(
			pErrorOffset, sizeof(*pErrorOffset),
			Block.Data, Block.Size
		 ))) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	if ( pErrorOffset != NULL ) {
		memcpy(pErrorOffset, &iErrorOffset, sizeof(iErrorOffset));
	}
	pWork = xrtHttpHeadersClone(pHeaders);
	if ( pWork == NULL ) {
		return false;
	}
	if ( !__xrtHttpHeadersParseInto(
		pWork, Block, &iErrorOffset
	) ) {
		if ( pErrorOffset != NULL ) {
			memcpy(
				pErrorOffset,
				&iErrorOffset,
				sizeof(iErrorOffset)
			);
		}
		xrtHttpHeadersDestroy(pWork);
		return false;
	}
	(void)xrtHttpHeadersSwap(pHeaders, pWork);
	xrtHttpHeadersDestroy(pWork);
	return true;
}



/* 创建容器并把完整字段块直接解析到该容器。 */
XRT_API xhttpheaders* xrtHttpHeadersParse(
	xstrview Block,
	const xhttpheadersconfig* pConfig,
	size_t* pErrorOffset
)
{
	xhttpheaders* pHeaders;
	size_t iErrorOffset = 0;

	if ( !__xhttpViewValid(Block) ||
		((pErrorOffset != NULL) &&
		 (!xrtMemRangeValid(
			pErrorOffset, sizeof(*pErrorOffset)
		 ) || xrtMemRangesOverlap(
			pErrorOffset, sizeof(*pErrorOffset),
			Block.Data, Block.Size
		 ) || ((pConfig != NULL) && xrtMemRangesOverlap(
			pErrorOffset, sizeof(*pErrorOffset),
			pConfig, sizeof(*pConfig)
		 ))) ) ) {
		__xhttpErrorSetInvalidArgument();
		return NULL;
	}
	if ( pErrorOffset != NULL ) {
		memcpy(pErrorOffset, &iErrorOffset, sizeof(iErrorOffset));
	}
	pHeaders = xrtHttpHeadersCreate(pConfig);
	if ( pHeaders == NULL ) {
		return NULL;
	}
	if ( !__xrtHttpHeadersParseInto(
		pHeaders, Block, &iErrorOffset
	) ) {
		if ( pErrorOffset != NULL ) {
			memcpy(
				pErrorOffset,
				&iErrorOffset,
				sizeof(iErrorOffset)
			);
		}
		xrtHttpHeadersDestroy(pHeaders);
		return NULL;
	}
	return pHeaders;
}



/* 写出字段行和最终空行，不附加零字符。 */
XRT_API bool xrtHttpHeadersWrite(
	const xhttpheaders* pHeaders,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	bool bResult;
	size_t iRequired;
	size_t iWritten;

	if ( (pHeaders == NULL) || (pSize == NULL) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		!__xrtHttpHeadersOutputValid(
			pHeaders, pSize, sizeof(*pSize)
		) ||
		xrtMemRangesOverlap(
			pHeaders, sizeof(*pHeaders),
			pSize, sizeof(*pSize)
		) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtHttpFieldBlockWrite(
		pHeaders->Fields, pHeaders->Count,
		NULL, 0, &iRequired
	) ) {
		return false;
	}
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	if ( !xrtMemRangeValid(pOutput, iCapacity) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	if ( xrtMemRangesOverlap(
		pHeaders, sizeof(*pHeaders),
		pOutput, iRequired
	) || xrtMemRangesOverlap(
		pOutput, iRequired,
		pSize, sizeof(*pSize)
	) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		__xhttpErrorSetRange();
		return false;
	}
	iWritten = 0;
	bResult = xrtHttpFieldBlockWrite(
		pHeaders->Fields, pHeaders->Count,
		pOutput, iCapacity, &iWritten
	);
	if ( bResult ) {
		memcpy(pSize, &iWritten, sizeof(iWritten));
	}
	return bResult;
}



/* 分配并构建带最终空行和额外零字符的字段块。 */
XRT_API str xrtHttpHeadersBuild(
	const xhttpheaders* pHeaders,
	size_t* pSize
)
{
	size_t iSize;
	str sOutput;

	if ( pSize != NULL ) {
		if ( !__xrtHttpHeadersOutputValid(
			pHeaders, pSize, sizeof(*pSize)
		) ) {
			__xhttpErrorSetInvalidArgument();
			return NULL;
		}
		iSize = 0;
		memcpy(pSize, &iSize, sizeof(iSize));
	}
	if ( !xrtHttpHeadersWrite(
		pHeaders, NULL, 0, &iSize
	) ) {
		return NULL;
	}
	if ( iSize == SIZE_MAX ) {
		__xhttpErrorSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iSize + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtHttpHeadersWrite(
		pHeaders, sOutput, iSize, &iSize
	) ) {
		xrtFree(sOutput);
		return NULL;
	}
	sOutput[iSize] = '\0';
	if ( pSize != NULL ) {
		memcpy(pSize, &iSize, sizeof(iSize));
	}
	return sOutput;
}

#endif
