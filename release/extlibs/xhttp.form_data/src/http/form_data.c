#include "../internal/xrt_form_data.h"



#if defined(XHTTP_FEATURE_FORM_DATA)

/* 发布 FormData 域错误。 */
bool __xrtFormDataFail(
	xerrkind Kind,
	xformdataerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Code = (int32)Code;
	Desc.Domain = "form.data";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetErrorTake(pError);
	}
	return false;
}



/* 验证借用文本视图是一段不会发生地址回绕的连续内存。 */
static bool __xrtFormDataViewValid(xstrview Text)
{
	return xrtMemRangeValid(Text.Data, Text.Size);
}



/* 返回条目元数据副本的真实分配长度。 */
static size_t __xrtFormDataEntryAllocation(
	const xrt_form_data_entry* pEntry
)
{
	size_t iSize = pEntry->NameSize + 1u;

	if ( (pEntry->Flags & XFORM_DATA_PART_FILENAME) != 0 ) {
		iSize += pEntry->FilenameSize + 1u;
	}
	if ( (pEntry->Flags & XFORM_DATA_PART_CONTENT_TYPE) != 0 ) {
		iSize += pEntry->ContentTypeSize + 1u;
	}
	return iSize;
}



/* 返回条目借用的名称。 */
xstrview __xrtFormDataEntryName(
	const xrt_form_data_entry* pEntry
)
{
	return (xstrview){
		pEntry->Metadata,
		pEntry->NameSize
	};
}



/* 返回条目借用的文件名。 */
xstrview __xrtFormDataEntryFilename(
	const xrt_form_data_entry* pEntry
)
{
	if ( (pEntry->Flags & XFORM_DATA_PART_FILENAME) == 0 ) {
		return (xstrview){ NULL, 0 };
	}
	return (xstrview){
		pEntry->Metadata + pEntry->NameSize + 1u,
		pEntry->FilenameSize
	};
}



/* 返回条目借用的媒体类型。 */
xstrview __xrtFormDataEntryContentType(
	const xrt_form_data_entry* pEntry
)
{
	size_t iOffset;

	if ( (pEntry->Flags & XFORM_DATA_PART_CONTENT_TYPE) == 0 ) {
		return (xstrview){ NULL, 0 };
	}
	iOffset = pEntry->NameSize + 1u;
	if ( (pEntry->Flags & XFORM_DATA_PART_FILENAME) != 0 ) {
		iOffset += pEntry->FilenameSize + 1u;
	}
	return (xstrview){
		pEntry->Metadata + iOffset,
		pEntry->ContentTypeSize
	};
}



/* 返回条目计入容器配额的逻辑元数据字节数。 */
static size_t __xrtFormDataEntryMetadata(
	const xrt_form_data_entry* pEntry
)
{
	return pEntry->NameSize +
		pEntry->FilenameSize +
		pEntry->ContentTypeSize;
}



/* 判断名称是否按字节完全相等。 */
static bool __xrtFormDataEntryNameEqual(
	const xrt_form_data_entry* pEntry,
	xstrview Name
)
{
	xstrview Stored = __xrtFormDataEntryName(pEntry);

	return (Stored.Size == Name.Size) &&
		((Name.Size == 0) ||
		 (memcmp(Stored.Data, Name.Data, Name.Size) == 0));
}



/* 销毁一个独立条目。 */
static void __xrtFormDataEntryDestroy(
	xrt_form_data_entry* pEntry
)
{
	if ( pEntry == NULL ) {
		return;
	}
	xrtHttpBodyDestroy(pEntry->Body);
	if ( pEntry->Metadata != NULL ) {
		memset(
			pEntry->Metadata,
			0,
			__xrtFormDataEntryAllocation(pEntry)
		);
		xrtFree(pEntry->Metadata);
	}
	memset(pEntry, 0, sizeof(*pEntry));
}



/* 验证正文长度满足单 Part 上限。 */
static bool __xrtFormDataPartLengthValid(
	const xformdata* pForm,
	uint64 iLength
)
{
	if ( iLength == XHTTP_BODY_UNKNOWN ) {
		if ( (pForm->Config.MaxPartBytes != XHTTP_BODY_UNKNOWN) ||
			(pForm->Config.MaxBodyBytes != XHTTP_BODY_UNKNOWN) ) {
			return __xrtFormDataFail(
				XERR_RANGE,
				XFORM_DATA_ERROR_LIMIT,
				"part",
				"unknown body length cannot satisfy a finite FormData limit"
			);
		}
		return true;
	}
	if ( (pForm->Config.MaxPartBytes != XHTTP_BODY_UNKNOWN) &&
		(iLength > pForm->Config.MaxPartBytes) ) {
		return __xrtFormDataFail(
			XERR_RANGE,
			XFORM_DATA_ERROR_LIMIT,
			"part",
			"FormData Part body exceeds its configured limit"
		);
	}
	return true;
}



/* 验证加入一个条目后的容器级元数据和正文配额。 */
static bool __xrtFormDataTotalsValid(
	const xformdata* pForm,
	size_t iBaseMetadata,
	uint64 iBaseKnown,
	size_t iBaseUnknown,
	const xrt_form_data_entry* pEntry
)
{
	size_t iMetadata = __xrtFormDataEntryMetadata(pEntry);
	uint64 iLength = xrtHttpBodyLength(pEntry->Body);

	if ( (iMetadata > (pForm->Config.MaxMetadata - iBaseMetadata)) ) {
		return __xrtFormDataFail(
			XERR_RANGE,
			XFORM_DATA_ERROR_LIMIT,
			"metadata",
			"FormData metadata exceeds its configured limit"
		);
	}
	if ( iLength == XHTTP_BODY_UNKNOWN ) {
		iBaseUnknown++;
	} else if ( iLength >= (XHTTP_BODY_UNKNOWN - iBaseKnown) ) {
		return __xrtFormDataFail(
			XERR_RANGE,
			XFORM_DATA_ERROR_LIMIT,
			"body",
			"FormData body length overflowed"
		);
	} else {
		iBaseKnown += iLength;
	}
	if ( (pForm->Config.MaxBodyBytes != XHTTP_BODY_UNKNOWN) &&
		((iBaseUnknown != 0) ||
		 (iBaseKnown > pForm->Config.MaxBodyBytes)) ) {
		return __xrtFormDataFail(
			XERR_RANGE,
			XFORM_DATA_ERROR_LIMIT,
			"body",
			"FormData body exceeds its configured limit"
		);
	}
	return true;
}



/* 验证、复制元数据并保留正文引用，形成尚未提交的条目。 */
static bool __xrtFormDataEntryPrepare(
	const xformdata* pForm,
	xstrview Name,
	xhttpbody* pBody,
	const xstrview* pFilename,
	xstrview ContentType,
	xrt_form_data_entry* pEntry
)
{
	xmediatype MediaType;
	xstrview Filename;
	uint64 iLength;
	size_t iQuoted;
	size_t iMetadata;
	size_t iAllocation;
	size_t iOffset = 0;
	uint32 iFlags = XFORM_DATA_PART_NONE;
	bool bFilename = pFilename != NULL;

	memset(pEntry, 0, sizeof(*pEntry));
	memset(&Filename, 0, sizeof(Filename));
	if ( bFilename ) {
		if ( !xrtMemRangeValid(pFilename, sizeof(*pFilename)) ) {
			return __xrtFormDataFail(
				XERR_ARGUMENT,
				XFORM_DATA_ERROR_ARGUMENT,
				"part",
				"FormData filename descriptor is invalid"
			);
		}
		memcpy(&Filename, pFilename, sizeof(Filename));
	}
	if ( (pForm == NULL) || (pBody == NULL) ||
		!__xrtFormDataViewValid(Name) ||
		!__xrtFormDataViewValid(ContentType) ||
		(bFilename && !__xrtFormDataViewValid(Filename)) ) {
		return __xrtFormDataFail(
			XERR_ARGUMENT,
			XFORM_DATA_ERROR_ARGUMENT,
			"part",
			"FormData Part arguments are invalid"
		);
	}
	if ( (Name.Size > pForm->Config.MaxName) ||
		(bFilename &&
		 (Filename.Size > pForm->Config.MaxFilename)) ||
		(ContentType.Size > pForm->Config.MaxContentType) ) {
		return __xrtFormDataFail(
			XERR_RANGE,
			XFORM_DATA_ERROR_LIMIT,
			"part",
			"FormData Part metadata exceeds its configured limit"
		);
	}
	if ( !xrtHttpQuotedWrite(Name, NULL, 0, &iQuoted) ||
		(bFilename && !xrtHttpQuotedWrite(
			Filename, NULL, 0, &iQuoted
		)) ) {
		return __xrtFormDataFail(
			XERR_VALUE,
			XFORM_DATA_ERROR_VALUE,
			"part",
			"FormData name or filename is not a valid quoted value"
		);
	}
	if ( (ContentType.Size != 0) &&
		!xrtHttpMediaTypeParse(ContentType, &MediaType) ) {
		return __xrtFormDataFail(
			XERR_VALUE,
			XFORM_DATA_ERROR_VALUE,
			"part",
			"FormData Content-Type is invalid"
		);
	}
	iLength = xrtHttpBodyLength(pBody);
	if ( !__xrtFormDataPartLengthValid(pForm, iLength) ) {
		return false;
	}
	iMetadata = Name.Size;
	if ( bFilename &&
		(Filename.Size > (SIZE_MAX - iMetadata)) ) {
		return __xrtFormDataFail(
			XERR_RANGE, XFORM_DATA_ERROR_LIMIT,
			"part", "FormData metadata length overflowed"
		);
	}
	if ( bFilename ) {
		iMetadata += Filename.Size;
		iFlags |= XFORM_DATA_PART_FILENAME;
	}
	if ( ContentType.Size > (SIZE_MAX - iMetadata) ) {
		return __xrtFormDataFail(
			XERR_RANGE, XFORM_DATA_ERROR_LIMIT,
			"part", "FormData metadata length overflowed"
		);
	}
	iMetadata += ContentType.Size;
	if ( ContentType.Size != 0 ) {
		iFlags |= XFORM_DATA_PART_CONTENT_TYPE;
	}
	if ( iMetadata == SIZE_MAX ) {
		return __xrtFormDataFail(
			XERR_RANGE, XFORM_DATA_ERROR_LIMIT,
			"part", "FormData metadata allocation overflowed"
		);
	}
	iAllocation = iMetadata + 1u;
	if ( bFilename ) {
		if ( iAllocation == SIZE_MAX ) {
			return __xrtFormDataFail(
				XERR_RANGE, XFORM_DATA_ERROR_LIMIT,
				"part", "FormData metadata allocation overflowed"
			);
		}
		iAllocation++;
	}
	if ( ContentType.Size != 0 ) {
		if ( iAllocation == SIZE_MAX ) {
			return __xrtFormDataFail(
				XERR_RANGE, XFORM_DATA_ERROR_LIMIT,
				"part", "FormData metadata allocation overflowed"
			);
		}
		iAllocation++;
	}
	pEntry->Metadata = (str)xrtMalloc(iAllocation);
	if ( pEntry->Metadata == NULL ) {
		return false;
	}
	if ( Name.Size != 0 ) {
		memcpy(pEntry->Metadata, Name.Data, Name.Size);
	}
	pEntry->Metadata[Name.Size] = '\0';
	iOffset = Name.Size + 1u;
	if ( bFilename ) {
		if ( Filename.Size != 0 ) {
			memcpy(
				pEntry->Metadata + iOffset,
				Filename.Data,
				Filename.Size
			);
		}
		iOffset += Filename.Size;
		pEntry->Metadata[iOffset++] = '\0';
	}
	if ( ContentType.Size != 0 ) {
		memcpy(
			pEntry->Metadata + iOffset,
			ContentType.Data,
			ContentType.Size
		);
		iOffset += ContentType.Size;
		pEntry->Metadata[iOffset++] = '\0';
	}
	pEntry->Body = xrtHttpBodyRef(pBody);
	if ( pEntry->Body == NULL ) {
		memset(pEntry->Metadata, 0, iAllocation);
		xrtFree(pEntry->Metadata);
		memset(pEntry, 0, sizeof(*pEntry));
		return false;
	}
	pEntry->NameSize = Name.Size;
	pEntry->FilenameSize = bFilename ? Filename.Size : 0;
	pEntry->ContentTypeSize = ContentType.Size;
	pEntry->Flags = iFlags;
	return true;
}



/* 重新计算修改后的逻辑配额统计。 */
static void __xrtFormDataRecount(xformdata* pForm)
{
	size_t i;

	pForm->Metadata = 0;
	pForm->KnownBodyBytes = 0;
	pForm->UnknownBodies = 0;
	for ( i = 0; i < pForm->Count; i++ ) {
		uint64 iLength = xrtHttpBodyLength(
			pForm->Entries[i].Body
		);

		pForm->Metadata += __xrtFormDataEntryMetadata(
			&pForm->Entries[i]
		);
		if ( iLength == XHTTP_BODY_UNKNOWN ) {
			pForm->UnknownBodies++;
		} else {
			pForm->KnownBodyBytes += iLength;
		}
	}
}



/* 验证配置中的容量关系和正文上限哨兵。 */
static bool __xrtFormDataConfigValid(
	const xformdataconfig* pConfig
)
{
	if ( pConfig->InitialParts > pConfig->MaxParts ) {
		return __xrtFormDataFail(
			XERR_ARGUMENT,
			XFORM_DATA_ERROR_ARGUMENT,
			"config",
			"FormData configuration limits are inconsistent"
		);
	}
	return true;
}



/* 初始化适合 HTTP 表单的容量和限制。 */
XRT_API void xrtFormDataConfigInit(xformdataconfig* pConfig)
{
	xformdataconfig Config;

	if ( pConfig == NULL ) {
		return;
	}
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		(void)__xrtFormDataFail(
			XERR_ARGUMENT,
			XFORM_DATA_ERROR_ARGUMENT,
			"config",
			"FormData configuration output range is invalid"
		);
		return;
	}
	memset(&Config, 0, sizeof(Config));
	Config.InitialParts = 8u;
	Config.MaxParts = 1024u;
	Config.MaxName = 4096u;
	Config.MaxFilename = 16384u;
	Config.MaxContentType = 4096u;
	Config.MaxMetadata = 1024u * 1024u;
	Config.MaxPartBytes = XHTTP_BODY_UNKNOWN;
	Config.MaxBodyBytes = XHTTP_BODY_UNKNOWN;
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 创建拥有型空容器。 */
XRT_API xformdata* xrtFormDataCreate(
	const xformdataconfig* pConfig
)
{
	xformdataconfig Config;
	xformdata* pForm;

	xrtFormDataConfigInit(&Config);
	if ( pConfig != NULL ) {
		if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
			(void)__xrtFormDataFail(
				XERR_ARGUMENT,
				XFORM_DATA_ERROR_ARGUMENT,
				"config",
				"FormData configuration input range is invalid"
			);
			return NULL;
		}
		memcpy(&Config, pConfig, sizeof(Config));
	}
	if ( !__xrtFormDataConfigValid(&Config) ) {
		return NULL;
	}
	pForm = (xformdata*)xrtCalloc(1, sizeof(*pForm));
	if ( pForm == NULL ) {
		return NULL;
	}
	pForm->Config = Config;
	if ( (Config.InitialParts != 0) &&
		!xrtFormDataReserve(pForm, Config.InitialParts) ) {
		xrtFormDataDestroy(pForm);
		return NULL;
	}
	return pForm;
}



/* 清空全部 Part 并保留条目数组。 */
XRT_API void xrtFormDataClear(xformdata* pForm)
{
	size_t i;

	if ( pForm == NULL ) {
		return;
	}
	for ( i = 0; i < pForm->Count; i++ ) {
		__xrtFormDataEntryDestroy(&pForm->Entries[i]);
	}
	pForm->Count = 0;
	pForm->Metadata = 0;
	pForm->KnownBodyBytes = 0;
	pForm->UnknownBodies = 0;
}



/* 销毁容器。 */
XRT_API void xrtFormDataDestroy(xformdata* pForm)
{
	if ( pForm == NULL ) {
		return;
	}
	xrtFormDataClear(pForm);
	if ( pForm->Entries != NULL ) {
		memset(
			pForm->Entries,
			0,
			pForm->Capacity * sizeof(*pForm->Entries)
		);
		xrtFree(pForm->Entries);
	}
	memset(pForm, 0, sizeof(*pForm));
	xrtFree(pForm);
}



/* 预留条目数组容量。 */
XRT_API bool xrtFormDataReserve(xformdata* pForm, size_t iParts)
{
	xrt_form_data_entry* pEntries;
	size_t iCapacity;

	if ( pForm == NULL ) {
		return __xrtFormDataFail(
			XERR_ARGUMENT, XFORM_DATA_ERROR_ARGUMENT,
			"reserve", "FormData container is null"
		);
	}
	if ( iParts <= pForm->Capacity ) {
		return true;
	}
	if ( (iParts > pForm->Config.MaxParts) ||
		(iParts > (SIZE_MAX / sizeof(*pEntries))) ) {
		return __xrtFormDataFail(
			XERR_RANGE, XFORM_DATA_ERROR_LIMIT,
			"reserve", "FormData Part capacity exceeds its limit"
		);
	}
	iCapacity = pForm->Capacity != 0 ?
		pForm->Capacity : pForm->Config.InitialParts;
	if ( iCapacity == 0 ) {
		iCapacity = 8u;
	}
	if ( iCapacity > pForm->Config.MaxParts ) {
		iCapacity = pForm->Config.MaxParts;
	}
	while ( iCapacity < iParts ) {
		size_t iNext = iCapacity > (SIZE_MAX / 2u) ?
			iParts : iCapacity * 2u;

		if ( iNext > pForm->Config.MaxParts ) {
			iNext = pForm->Config.MaxParts;
		}
		if ( iNext <= iCapacity ) {
			iCapacity = iParts;
			break;
		}
		iCapacity = iNext;
	}
	pEntries = (xrt_form_data_entry*)xrtRealloc(
		pForm->Entries,
		iCapacity * sizeof(*pEntries)
	);
	if ( pEntries == NULL ) {
		return false;
	}
	memset(
		pEntries + pForm->Capacity,
		0,
		(iCapacity - pForm->Capacity) * sizeof(*pEntries)
	);
	pForm->Entries = pEntries;
	pForm->Capacity = iCapacity;
	return true;
}



/* 返回 Part 数量。 */
XRT_API size_t xrtFormDataCount(const xformdata* pForm)
{
	return pForm != NULL ? pForm->Count : 0;
}



/* 返回逻辑元数据字节数。 */
XRT_API size_t xrtFormDataMetadata(const xformdata* pForm)
{
	return pForm != NULL ? pForm->Metadata : 0;
}



/* 提交一个已经完整准备的追加条目。 */
static bool __xrtFormDataAppendEntry(
	xformdata* pForm,
	xrt_form_data_entry* pEntry
)
{
	uint64 iLength;

	if ( pForm->Count >= pForm->Config.MaxParts ) {
		return __xrtFormDataFail(
			XERR_RANGE, XFORM_DATA_ERROR_LIMIT,
			"append", "FormData Part count exceeds its limit"
		);
	}
	if ( !__xrtFormDataTotalsValid(
		pForm,
		pForm->Metadata,
		pForm->KnownBodyBytes,
		pForm->UnknownBodies,
		pEntry
	) || !xrtFormDataReserve(
		pForm, pForm->Count + 1u
	) ) {
		return false;
	}
	pForm->Entries[pForm->Count++] = *pEntry;
	pForm->Metadata += __xrtFormDataEntryMetadata(pEntry);
	iLength = xrtHttpBodyLength(pEntry->Body);
	if ( iLength == XHTTP_BODY_UNKNOWN ) {
		pForm->UnknownBodies++;
	} else {
		pForm->KnownBodyBytes += iLength;
	}
	memset(pEntry, 0, sizeof(*pEntry));
	return true;
}



/* 追加一个正文 Part。 */
XRT_API bool xrtFormDataAppendBody(
	xformdata* pForm,
	xstrview Name,
	xhttpbody* pBody,
	const xstrview* pFilename,
	xstrview ContentType
)
{
	xrt_form_data_entry Entry;

	if ( !__xrtFormDataEntryPrepare(
		pForm, Name, pBody, pFilename, ContentType, &Entry
	) ) {
		return false;
	}
	if ( !__xrtFormDataAppendEntry(pForm, &Entry) ) {
		__xrtFormDataEntryDestroy(&Entry);
		return false;
	}
	return true;
}



/* 复制文本值并追加普通字段。 */
XRT_API bool xrtFormDataAppendText(
	xformdata* pForm,
	xstrview Name,
	xstrview Value
)
{
	return xrtFormDataAppendBytes(
		pForm,
		Name,
		(xbytesview){
			(const uint8*)Value.Data,
			Value.Size
		},
		NULL,
		(xstrview){ NULL, 0 }
	);
}



/* 复制二进制值并追加字段。 */
XRT_API bool xrtFormDataAppendBytes(
	xformdata* pForm,
	xstrview Name,
	xbytesview Data,
	const xstrview* pFilename,
	xstrview ContentType
)
{
	xhttpbody* pBody = xrtHttpBodyCopy(Data);
	bool bResult;

	if ( pBody == NULL ) {
		return false;
	}
	bResult = xrtFormDataAppendBody(
		pForm, Name, pBody, pFilename, ContentType
	);
	xrtHttpBodyDestroy(pBody);
	return bResult;
}



/* 在首个同名位置提交替换条目并删除后续同名项。 */
static bool __xrtFormDataSetEntry(
	xformdata* pForm,
	xrt_form_data_entry* pEntry
)
{
	xstrview Name = __xrtFormDataEntryName(pEntry);
	size_t iRemovedMetadata = 0;
	uint64 iRemovedKnown = 0;
	size_t iRemovedUnknown = 0;
	size_t iFirst = SIZE_MAX;
	size_t i;

	for ( i = 0; i < pForm->Count; i++ ) {
		if ( __xrtFormDataEntryNameEqual(
			&pForm->Entries[i], Name
		) ) {
			uint64 iLength = xrtHttpBodyLength(
				pForm->Entries[i].Body
			);

			if ( iFirst == SIZE_MAX ) {
				iFirst = i;
			}
			iRemovedMetadata += __xrtFormDataEntryMetadata(
				&pForm->Entries[i]
			);
			if ( iLength == XHTTP_BODY_UNKNOWN ) {
				iRemovedUnknown++;
			} else {
				iRemovedKnown += iLength;
			}
		}
	}
	if ( !__xrtFormDataTotalsValid(
		pForm,
		pForm->Metadata - iRemovedMetadata,
		pForm->KnownBodyBytes - iRemovedKnown,
		pForm->UnknownBodies - iRemovedUnknown,
		pEntry
	) ) {
		return false;
	}
	if ( iFirst == SIZE_MAX ) {
		return __xrtFormDataAppendEntry(pForm, pEntry);
	}
	__xrtFormDataEntryDestroy(&pForm->Entries[iFirst]);
	pForm->Entries[iFirst] = *pEntry;
	memset(pEntry, 0, sizeof(*pEntry));
	{
		size_t iWrite = iFirst + 1u;

		for ( i = iFirst + 1u; i < pForm->Count; i++ ) {
			if ( __xrtFormDataEntryNameEqual(
				&pForm->Entries[i], Name
			) ) {
				__xrtFormDataEntryDestroy(&pForm->Entries[i]);
				continue;
			}
			if ( iWrite != i ) {
				pForm->Entries[iWrite] = pForm->Entries[i];
				memset(
					&pForm->Entries[i],
					0,
					sizeof(pForm->Entries[i])
				);
			}
			iWrite++;
		}
		pForm->Count = iWrite;
	}
	__xrtFormDataRecount(pForm);
	return true;
}



/* 原子替换全部同名正文 Part。 */
XRT_API bool xrtFormDataSetBody(
	xformdata* pForm,
	xstrview Name,
	xhttpbody* pBody,
	const xstrview* pFilename,
	xstrview ContentType
)
{
	xrt_form_data_entry Entry;

	if ( !__xrtFormDataEntryPrepare(
		pForm, Name, pBody, pFilename, ContentType, &Entry
	) ) {
		return false;
	}
	if ( !__xrtFormDataSetEntry(pForm, &Entry) ) {
		__xrtFormDataEntryDestroy(&Entry);
		return false;
	}
	return true;
}



/* 复制文本值并原子替换同名项。 */
XRT_API bool xrtFormDataSetText(
	xformdata* pForm,
	xstrview Name,
	xstrview Value
)
{
	return xrtFormDataSetBytes(
		pForm,
		Name,
		(xbytesview){
			(const uint8*)Value.Data,
			Value.Size
		},
		NULL,
		(xstrview){ NULL, 0 }
	);
}



/* 复制二进制值并原子替换同名项。 */
XRT_API bool xrtFormDataSetBytes(
	xformdata* pForm,
	xstrview Name,
	xbytesview Data,
	const xstrview* pFilename,
	xstrview ContentType
)
{
	xhttpbody* pBody = xrtHttpBodyCopy(Data);
	bool bResult;

	if ( pBody == NULL ) {
		return false;
	}
	bResult = xrtFormDataSetBody(
		pForm, Name, pBody, pFilename, ContentType
	);
	xrtHttpBodyDestroy(pBody);
	return bResult;
}



/* 删除全部同名 Part。 */
XRT_API size_t xrtFormDataRemove(
	xformdata* pForm,
	xstrview Name
)
{
	size_t iWrite = 0;
	size_t iRemoved = 0;
	size_t i;

	if ( (pForm == NULL) || !__xrtFormDataViewValid(Name) ) {
		(void)__xrtFormDataFail(
			XERR_ARGUMENT, XFORM_DATA_ERROR_ARGUMENT,
			"remove", "FormData remove arguments are invalid"
		);
		return 0;
	}
	for ( i = 0; i < pForm->Count; i++ ) {
		if ( __xrtFormDataEntryNameEqual(
			&pForm->Entries[i], Name
		) ) {
			__xrtFormDataEntryDestroy(&pForm->Entries[i]);
			iRemoved++;
			continue;
		}
		if ( iWrite != i ) {
			pForm->Entries[iWrite] = pForm->Entries[i];
			memset(
				&pForm->Entries[i],
				0,
				sizeof(pForm->Entries[i])
			);
		}
		iWrite++;
	}
	pForm->Count = iWrite;
	if ( iRemoved != 0 ) {
		__xrtFormDataRecount(pForm);
	}
	return iRemoved;
}



/* 返回同名 Part 数量。 */
XRT_API size_t xrtFormDataCountName(
	const xformdata* pForm,
	xstrview Name
)
{
	size_t iCount = 0;
	size_t i;

	if ( (pForm == NULL) || !__xrtFormDataViewValid(Name) ) {
		(void)__xrtFormDataFail(
			XERR_ARGUMENT, XFORM_DATA_ERROR_ARGUMENT,
			"count", "FormData count arguments are invalid"
		);
		return 0;
	}
	for ( i = 0; i < pForm->Count; i++ ) {
		if ( __xrtFormDataEntryNameEqual(
			&pForm->Entries[i], Name
		) ) {
			iCount++;
		}
	}
	return iCount;
}



/* 判断是否存在同名 Part。 */
XRT_API bool xrtFormDataHas(
	const xformdata* pForm,
	xstrview Name
)
{
	return xrtFormDataCountName(pForm, Name) != 0;
}



/* 验证输出结构不会覆盖容器拥有的内部存储。 */
bool __xrtFormDataOutputValid(
	const xformdata* pForm,
	const void* pOutput,
	size_t iOutput
)
{
	size_t i;

	if ( !xrtMemRangeValid(pOutput, iOutput) ||
		xrtMemRangesOverlap(
			pOutput, iOutput, pForm, sizeof(*pForm)
		) || xrtMemRangesOverlap(
		pOutput, iOutput,
		pForm->Entries,
		pForm->Capacity * sizeof(*pForm->Entries)
	) ) {
		return false;
	}
	for ( i = 0; i < pForm->Count; i++ ) {
		if ( xrtMemRangesOverlap(
			pOutput,
			iOutput,
			pForm->Entries[i].Metadata,
			__xrtFormDataEntryAllocation(&pForm->Entries[i])
		) ) {
			return false;
		}
	}
	return true;
}



/* 复制指定位置的借用 Part 视图。 */
XRT_API bool xrtFormDataAt(
	const xformdata* pForm,
	size_t iIndex,
	xformdatapart* pPart
)
{
	const xrt_form_data_entry* pEntry;
	xformdatapart Part;

	if ( (pForm == NULL) || (pPart == NULL) ||
		!__xrtFormDataOutputValid(
			pForm, pPart, sizeof(*pPart)
		) ) {
		return __xrtFormDataFail(
			XERR_ARGUMENT, XFORM_DATA_ERROR_ARGUMENT,
			"at", "FormData output overlaps its container"
		);
	}
	if ( iIndex >= pForm->Count ) {
		return __xrtFormDataFail(
			XERR_RANGE, XFORM_DATA_ERROR_LIMIT,
			"at", "FormData index is out of range"
		);
	}
	pEntry = &pForm->Entries[iIndex];
	memset(&Part, 0, sizeof(Part));
	Part.Name = __xrtFormDataEntryName(pEntry);
	Part.Filename = __xrtFormDataEntryFilename(pEntry);
	Part.ContentType = __xrtFormDataEntryContentType(pEntry);
	Part.Body = pEntry->Body;
	Part.Length = xrtHttpBodyLength(pEntry->Body);
	Part.Flags = pEntry->Flags;
	memcpy(pPart, &Part, sizeof(Part));
	return true;
}



/* 复制首个同名 Part。 */
XRT_API bool xrtFormDataGet(
	const xformdata* pForm,
	xstrview Name,
	xformdatapart* pPart
)
{
	size_t iIndex = 0;

	return xrtFormDataFind(
		pForm, Name, &iIndex, pPart
	) == XHTTP_NEXT_ITEM;
}



/* 从 Index 开始查找下一个同名项。 */
XRT_API xhttpnext xrtFormDataFind(
	const xformdata* pForm,
	xstrview Name,
	size_t* pIndex,
	xformdatapart* pPart
)
{
	xformdatapart Empty;
	size_t iIndex;
	size_t i;

	if ( (pForm == NULL) || !__xrtFormDataViewValid(Name) ||
		(pIndex == NULL) || (pPart == NULL) ||
		xrtMemRangesOverlap(
			pIndex, sizeof(*pIndex),
			pPart, sizeof(*pPart)
		) || !__xrtFormDataOutputValid(
			pForm, pIndex, sizeof(*pIndex)
		) || !__xrtFormDataOutputValid(
			pForm, pPart, sizeof(*pPart)
		) ) {
		(void)__xrtFormDataFail(
			XERR_ARGUMENT, XFORM_DATA_ERROR_ARGUMENT,
			"find", "FormData find arguments are invalid"
		);
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&iIndex, pIndex, sizeof(iIndex));
	if ( iIndex > pForm->Count ) {
		(void)__xrtFormDataFail(
			XERR_RANGE, XFORM_DATA_ERROR_LIMIT,
			"find", "FormData find index is out of range"
		);
		return XHTTP_NEXT_ERROR;
	}
	for ( i = iIndex; i < pForm->Count; i++ ) {
		if ( __xrtFormDataEntryNameEqual(
			&pForm->Entries[i], Name
		) ) {
			if ( !xrtFormDataAt(pForm, i, pPart) ) {
				return XHTTP_NEXT_ERROR;
			}
			iIndex = i + 1u;
			memcpy(pIndex, &iIndex, sizeof(iIndex));
			return XHTTP_NEXT_ITEM;
		}
	}
	iIndex = pForm->Count;
	memset(&Empty, 0, sizeof(Empty));
	memcpy(pIndex, &iIndex, sizeof(iIndex));
	memcpy(pPart, &Empty, sizeof(Empty));
	return XHTTP_NEXT_END;
}



/* 深复制元数据并保留正文引用。 */
XRT_API xformdata* xrtFormDataClone(const xformdata* pForm)
{
	xformdata* pClone;
	size_t i;

	if ( pForm == NULL ) {
		(void)__xrtFormDataFail(
			XERR_ARGUMENT, XFORM_DATA_ERROR_ARGUMENT,
			"clone", "FormData container is null"
		);
		return NULL;
	}
	pClone = xrtFormDataCreate(&pForm->Config);
	if ( pClone == NULL ) {
		return NULL;
	}
	if ( !xrtFormDataReserve(pClone, pForm->Count) ) {
		xrtFormDataDestroy(pClone);
		return NULL;
	}
	for ( i = 0; i < pForm->Count; i++ ) {
		const xrt_form_data_entry* pEntry = &pForm->Entries[i];
		xstrview Filename = __xrtFormDataEntryFilename(pEntry);

		if ( !xrtFormDataAppendBody(
			pClone,
			__xrtFormDataEntryName(pEntry),
			pEntry->Body,
			(pEntry->Flags & XFORM_DATA_PART_FILENAME) != 0 ?
				&Filename : NULL,
			__xrtFormDataEntryContentType(pEntry)
		) ) {
			xrtFormDataDestroy(pClone);
			return NULL;
		}
	}
	return pClone;
}

#endif
