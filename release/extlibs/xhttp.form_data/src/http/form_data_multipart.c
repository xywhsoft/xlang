#include "../internal/xrt_form_data.h"




#if defined(XHTTP_FEATURE_FORM_DATA_MULTIPART)

/* 查询一个条目的标准 form-data Part 头长度。 */
static bool __xrtFormDataHeadSize(
	const xrt_form_data_entry* pEntry,
	const xmultipartboundary* pBoundary,
	size_t* pSize
)
{
	xstrview Filename = __xrtFormDataEntryFilename(pEntry);

	return xrtMultipartFormHeadWrite(
		pBoundary,
		__xrtFormDataEntryName(pEntry),
		(pEntry->Flags & XFORM_DATA_PART_FILENAME) != 0 ?
			&Filename : NULL,
		__xrtFormDataEntryContentType(pEntry),
		NULL,
		0,
		pSize
	);
}



/* 计算片段数组和全部协议元数据所需的单块临时空间。 */
static bool __xrtFormDataMultipartMeasure(
	const xformdata* pForm,
	const xmultipartboundary* pBoundary,
	size_t* pPieceCount,
	size_t* pScratch,
	size_t* pAllocation
)
{
	size_t iPieces;
	size_t iScratch = 0;
	size_t i;

	if ( (pForm == NULL) || (pBoundary == NULL) ) {
		(void)__xrtFormDataFail(
			XERR_ARGUMENT,
			XFORM_DATA_ERROR_ARGUMENT,
			"encode",
			"FormData encoding arguments are invalid"
		);
		return false;
	}
	if ( pForm->Count > ((SIZE_MAX - 1u) / 3u) ) {
		(void)__xrtFormDataFail(
			XERR_RANGE,
			XFORM_DATA_ERROR_LIMIT,
			"encode",
			"FormData piece count overflowed"
		);
		return false;
	}
	iPieces = pForm->Count * 3u + 1u;
	for ( i = 0; i < pForm->Count; i++ ) {
		size_t iHead;

		if ( !__xrtFormDataHeadSize(
			&pForm->Entries[i], pBoundary, &iHead
		) ) {
			return false;
		}
		if ( iHead > (SIZE_MAX - iScratch) ) {
			(void)__xrtFormDataFail(
				XERR_RANGE,
				XFORM_DATA_ERROR_LIMIT,
				"encode",
				"FormData metadata length overflowed"
			);
			return false;
		}
		iScratch += iHead;
	}
	{
		size_t iClose;

		if ( !xrtMultipartCloseWrite(
			pBoundary, NULL, 0, &iClose
		) ) {
			return false;
		}
		if ( iClose > (SIZE_MAX - iScratch) ) {
			(void)__xrtFormDataFail(
				XERR_RANGE,
				XFORM_DATA_ERROR_LIMIT,
				"encode",
				"FormData closing boundary length overflowed"
			);
			return false;
		}
		iScratch += iClose;
	}
	if ( iPieces > (SIZE_MAX / sizeof(xhttpbodypiece)) ) {
		(void)__xrtFormDataFail(
			XERR_RANGE,
			XFORM_DATA_ERROR_LIMIT,
			"encode",
			"FormData piece allocation overflowed"
		);
		return false;
	}
	*pAllocation = iPieces * sizeof(xhttpbodypiece);
	if ( iScratch > (SIZE_MAX - *pAllocation) ) {
		(void)__xrtFormDataFail(
			XERR_RANGE,
			XFORM_DATA_ERROR_LIMIT,
			"encode",
			"FormData encoding allocation overflowed"
		);
		return false;
	}
	*pAllocation += iScratch;
	*pPieceCount = iPieces;
	*pScratch = iScratch;
	return true;
}



/* 写出全部 Part 头和关闭边界，并构造对应组合片段。 */
static bool __xrtFormDataMultipartFill(
	const xformdata* pForm,
	const xmultipartboundary* pBoundary,
	xhttpbodypiece* pPieces,
	uint8* pScratch,
	size_t iScratch
)
{
	static const uint8 PartEnd[] = "\r\n";
	size_t iPiece = 0;
	size_t iOffset = 0;
	size_t i;

	for ( i = 0; i < pForm->Count; i++ ) {
		const xrt_form_data_entry* pEntry = &pForm->Entries[i];
		xstrview Filename = __xrtFormDataEntryFilename(pEntry);
		size_t iHead;

		if ( !xrtMultipartFormHeadWrite(
			pBoundary,
			__xrtFormDataEntryName(pEntry),
			(pEntry->Flags & XFORM_DATA_PART_FILENAME) != 0 ?
				&Filename : NULL,
			__xrtFormDataEntryContentType(pEntry),
			pScratch + iOffset,
			iScratch - iOffset,
			&iHead
		) ) {
			return false;
		}
		pPieces[iPiece++] = xrtHttpBodyPieceBytes(
			(xbytesview){ pScratch + iOffset, iHead }
		);
		iOffset += iHead;
		pPieces[iPiece++] = xrtHttpBodyPieceBody(pEntry->Body);
		pPieces[iPiece++] = xrtHttpBodyPieceBytes(
			(xbytesview){ PartEnd, 2u }
		);
	}
	{
		size_t iClose;

		if ( !xrtMultipartCloseWrite(
			pBoundary,
			pScratch + iOffset,
			iScratch - iOffset,
			&iClose
		) ) {
			return false;
		}
		pPieces[iPiece] = xrtHttpBodyPieceBytes(
			(xbytesview){ pScratch + iOffset, iClose }
		);
		iOffset += iClose;
	}
	if ( iOffset != iScratch ) {
		(void)__xrtFormDataFail(
			XERR_INTERNAL,
			XFORM_DATA_ERROR_MULTIPART,
			"encode",
			"multipart writer changed its measured FormData size"
		);
		return false;
	}
	return true;
}



/* 创建由协议元数据和原始 Part 正文交错组成的流式正文。 */
XRT_API xhttpbody* xrtFormDataBody(
	const xformdata* pForm,
	const xmultipartboundary* pBoundary
)
{
	xhttpbodypiece* pPieces;
	uint8* pScratch;
	xhttpbody* pBody;
	ptr pMemory;
	size_t iPieceCount;
	size_t iScratch;
	size_t iAllocation;

	if ( !__xrtFormDataMultipartMeasure(
		pForm,
		pBoundary,
		&iPieceCount,
		&iScratch,
		&iAllocation
	) ) {
		return NULL;
	}
	pMemory = xrtMalloc(iAllocation);
	if ( pMemory == NULL ) {
		return NULL;
	}
	pPieces = (xhttpbodypiece*)pMemory;
	pScratch = (uint8*)(pPieces + iPieceCount);
	memset(pPieces, 0, iPieceCount * sizeof(*pPieces));
	if ( !__xrtFormDataMultipartFill(
		pForm,
		pBoundary,
		pPieces,
		pScratch,
		iScratch
	) ) {
		memset(pMemory, 0, iAllocation);
		xrtFree(pMemory);
		return NULL;
	}
	pBody = xrtHttpBodyCompose(pPieces, iPieceCount);
	memset(pMemory, 0, iAllocation);
	xrtFree(pMemory);
	return pBody;
}

#endif

