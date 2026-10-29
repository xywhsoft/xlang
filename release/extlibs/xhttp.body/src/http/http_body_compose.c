#include "../internal/xrt_http_body.h"
#include "../../include/xrt/http_body_compose.h"



#if defined(XHTTP_FEATURE_HTTP_BODY_COMPOSE)

/* 工厂片段借用同一工厂尾部的字节副本，或持有一个子正文引用。 */
typedef struct xrt_http_body_compose_piece {
	xhttpbodypiecekind Kind;
	xbytesview Bytes;
	xhttpbody* Body;
} xrt_http_body_compose_piece;



/* 工厂由正文对象与尚未释放的字节 Chunk 共同持有。 */
typedef struct xrt_http_body_compose_factory {
	volatile int32 RefCount;
	size_t AllocationSize;
	size_t Count;
	xrt_http_body_compose_piece Pieces[];
} xrt_http_body_compose_factory;



/* Reader 只打开当前子正文，避免长序列同时占用来源资源。 */
typedef struct xrt_http_body_compose_reader {
	xrt_http_body_compose_factory* Factory;
	xhttpbodyreader* Child;
	size_t Index;
	size_t Offset;
} xrt_http_body_compose_reader;



/* 释放工厂最后一个引用及其持有的全部子正文。 */
static void __xrtHttpBodyComposeFactoryRelease(
	xrt_http_body_compose_factory* pFactory
)
{
	size_t i;
	size_t iAllocation;

	if ( (pFactory == NULL) ||
		(xrtRefRelease(&pFactory->RefCount) != 0) ) {
		return;
	}
	iAllocation = pFactory->AllocationSize;
	for ( i = 0; i < pFactory->Count; i++ ) {
		if ( pFactory->Pieces[i].Kind ==
			XHTTP_BODY_PIECE_BODY ) {
			xrtHttpBodyDestroy(pFactory->Pieces[i].Body);
		}
	}
	memset(pFactory, 0, iAllocation);
	xrtFree(pFactory);
}



/* 字节 Chunk 释放时只归还保护其数据副本的工厂引用。 */
static void __xrtHttpBodyComposeChunkRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pData;
	(void)iSize;
	__xrtHttpBodyComposeFactoryRelease(
		(xrt_http_body_compose_factory*)pContext
	);
}



/* 从当前字节片段发布一段不超过调用上限的独立租约。 */
static xhttpbodystatus __xrtHttpBodyComposeBytesNext(
	xrt_http_body_compose_reader* pReader,
	const xrt_http_body_compose_piece* pPiece,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	size_t iRemaining = pPiece->Bytes.Size - pReader->Offset;
	size_t iSize = iRemaining < iMaxBytes ?
		iRemaining : iMaxBytes;

	if ( xrtRefRetain(&pReader->Factory->RefCount) < 0 ) {
		__xhttpErrorSetInvalidState();
		return XHTTP_BODY_ERROR;
	}
	pChunk->Data = pPiece->Bytes.Data + pReader->Offset;
	pChunk->Size = iSize;
	pChunk->Release = __xrtHttpBodyComposeChunkRelease;
	pChunk->Context = pReader->Factory;
	pReader->Offset += iSize;
	if ( pReader->Offset == pPiece->Bytes.Size ) {
		pReader->Index++;
		pReader->Offset = 0;
	}
	return XHTTP_BODY_DATA;
}



/* 延迟打开当前子正文，使组合序列始终只占用一个活动来源。 */
static bool __xrtHttpBodyComposeOpenChild(
	xrt_http_body_compose_reader* pReader,
	xhttpbody* pBody
)
{
	if ( pReader->Child != NULL ) {
		return true;
	}
	pReader->Child = xrtHttpBodyOpen(pBody);
	return pReader->Child != NULL;
}



/* 顺序推进字节与子正文，直到发布数据、等待、失败或结束。 */
static xhttpbodystatus __xrtHttpBodyComposeNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	xrt_http_body_compose_reader* pReader =
		(xrt_http_body_compose_reader*)pContext;

	while ( pReader->Index < pReader->Factory->Count ) {
		const xrt_http_body_compose_piece* pPiece =
			&pReader->Factory->Pieces[pReader->Index];

		if ( pPiece->Kind == XHTTP_BODY_PIECE_BYTES ) {
			if ( pReader->Offset == pPiece->Bytes.Size ) {
				pReader->Index++;
				pReader->Offset = 0;
				continue;
			}
			return __xrtHttpBodyComposeBytesNext(
				pReader, pPiece, iMaxBytes, pChunk
			);
		}
		if ( !__xrtHttpBodyComposeOpenChild(
			pReader, pPiece->Body
		) ) {
			return XHTTP_BODY_ERROR;
		}
		{
			xhttpbodystatus Status = xrtHttpBodyNext(
				pReader->Child, iMaxBytes, pChunk
			);

			if ( Status != XHTTP_BODY_EOF ) {
				return Status;
			}
		}
		xrtHttpBodyReaderDestroy(pReader->Child);
		pReader->Child = NULL;
		pReader->Index++;
	}
	return XHTTP_BODY_EOF;
}



#if defined(XHTTP_FEATURE_HTTP_BODY_ASYNC)

/* 把当前子正文的可读性等待透明转交给调用方。 */
static xfuture* __xrtHttpBodyComposeWait(ptr pContext)
{
	xrt_http_body_compose_reader* pReader =
		(xrt_http_body_compose_reader*)pContext;

	if ( (pReader == NULL) || (pReader->Child == NULL) ) {
		__xhttpErrorSetInvalidState();
		return NULL;
	}
	return xrtHttpBodyReaderWait(pReader->Child);
}

#endif



/* 关闭组合 Reader 及仍然活动的一个子正文 Reader。 */
static void __xrtHttpBodyComposeClose(ptr pContext)
{
	xrt_http_body_compose_reader* pReader =
		(xrt_http_body_compose_reader*)pContext;

	if ( pReader == NULL ) {
		return;
	}
	xrtHttpBodyReaderDestroy(pReader->Child);
	__xrtHttpBodyComposeFactoryRelease(pReader->Factory);
	memset(pReader, 0, sizeof(*pReader));
	xrtFree(pReader);
}



/* 为一次组合读取创建轻量状态，不提前打开任何子正文。 */
static bool __xrtHttpBodyComposeOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	xrt_http_body_compose_factory* pBody =
		(xrt_http_body_compose_factory*)pFactory;
	xrt_http_body_compose_reader* pReader;

	pReader = (xrt_http_body_compose_reader*)xrtCalloc(
		1, sizeof(*pReader)
	);
	if ( pReader == NULL ) {
		return false;
	}
	if ( xrtRefRetain(&pBody->RefCount) < 0 ) {
		xrtFree(pReader);
		__xhttpErrorSetInvalidState();
		return false;
	}
	pReader->Factory = pBody;
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = __xrtHttpBodyComposeNext;
	pOps->Close = __xrtHttpBodyComposeClose;
#if defined(XHTTP_FEATURE_HTTP_BODY_ASYNC)
	pOps->Wait = __xrtHttpBodyComposeWait;
#endif
	*ppReader = pReader;
	return true;
}



/* 正文释放时归还工厂自身的初始引用。 */
static void __xrtHttpBodyComposeDestroy(ptr pFactory)
{
	__xrtHttpBodyComposeFactoryRelease(
		(xrt_http_body_compose_factory*)pFactory
	);
}



/* 验证片段并计算工厂分配、已知总长度与公开能力。 */
static bool __xrtHttpBodyComposeMeasure(
	const xhttpbodypiece* pPieces,
	size_t iCount,
	size_t* pAllocation,
	size_t* pBytes,
	uint64* pLength,
	uint32* pFlags
)
{
	size_t iPiecesSize;
	size_t iBytes = 0;
	uint64 iLength = 0;
	uint32 iFlags = XHTTP_BODY_REPLAYABLE;
	size_t i;

	if ( (pPieces == NULL) ||
		(iCount > ( (SIZE_MAX - sizeof(
			xrt_http_body_compose_factory
		)) / sizeof(xrt_http_body_compose_piece))) ) {
		if ( pPieces == NULL ) {
			__xhttpErrorSetInvalidArgument();
		} else {
			__xhttpErrorSetSizeOverflow();
		}
		return false;
	}
	iPiecesSize = iCount * sizeof(xrt_http_body_compose_piece);
	if ( !xrtMemRangeValid(pPieces, iPiecesSize) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		uint64 iPieceLength;

		if ( pPieces[i].Kind == XHTTP_BODY_PIECE_BYTES ) {
			if ( !xrtMemRangeValid(
				pPieces[i].Bytes.Data,
				pPieces[i].Bytes.Size
			) || ( pPieces[i].Body != NULL ) ) {
				__xhttpErrorSetInvalidArgument();
				return false;
			}
#if SIZE_MAX > UINT64_MAX
			if ( pPieces[i].Bytes.Size > (size_t)UINT64_MAX ) {
				__xhttpErrorSetRange();
				return false;
			}
#endif
			if ( pPieces[i].Bytes.Size > (SIZE_MAX - iBytes) ) {
				__xhttpErrorSetSizeOverflow();
				return false;
			}
			iBytes += pPieces[i].Bytes.Size;
			iPieceLength = (uint64)pPieces[i].Bytes.Size;
		} else if ( pPieces[i].Kind == XHTTP_BODY_PIECE_BODY ) {
			if ( (pPieces[i].Body == NULL) ||
				(pPieces[i].Bytes.Data != NULL) ||
				(pPieces[i].Bytes.Size != 0) ) {
				__xhttpErrorSetInvalidArgument();
				return false;
			}
			iPieceLength = xrtHttpBodyLength(pPieces[i].Body);
			if ( !xrtHttpBodyReplayable(pPieces[i].Body) ) {
				iFlags = XHTTP_BODY_NONE;
			}
		} else {
			__xhttpErrorSetInvalidArgument();
			return false;
		}
		if ( (iLength == XHTTP_BODY_UNKNOWN) ||
			(iPieceLength == XHTTP_BODY_UNKNOWN) ) {
			iLength = XHTTP_BODY_UNKNOWN;
		} else if ( iPieceLength >=
			(XHTTP_BODY_UNKNOWN - iLength) ) {
			__xhttpErrorSetSizeOverflow();
			return false;
		} else {
			iLength += iPieceLength;
		}
	}
	if ( iBytes > (SIZE_MAX - sizeof(
		xrt_http_body_compose_factory
	) - iPiecesSize) ) {
		__xhttpErrorSetSizeOverflow();
		return false;
	}
	*pAllocation = sizeof(xrt_http_body_compose_factory) +
		iPiecesSize + iBytes;
	*pBytes = iBytes;
	*pLength = iLength;
	*pFlags = iFlags;
	return true;
}



/* 复制片段描述和字节，并为每一个子正文取得独立引用。 */
static bool __xrtHttpBodyComposeFill(
	xrt_http_body_compose_factory* pFactory,
	const xhttpbodypiece* pPieces,
	size_t iCount
)
{
	uint8* pBytes = (uint8*)(pFactory->Pieces + iCount);
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		xrt_http_body_compose_piece* pTarget =
			&pFactory->Pieces[i];

		pTarget->Kind = pPieces[i].Kind;
		if ( pTarget->Kind == XHTTP_BODY_PIECE_BYTES ) {
			pTarget->Bytes.Data = pBytes;
			pTarget->Bytes.Size = pPieces[i].Bytes.Size;
			if ( pPieces[i].Bytes.Size != 0 ) {
				memcpy(
					pBytes,
					pPieces[i].Bytes.Data,
					pPieces[i].Bytes.Size
				);
				pBytes += pPieces[i].Bytes.Size;
			}
		} else {
			pTarget->Body = xrtHttpBodyRef(pPieces[i].Body);
			if ( pTarget->Body == NULL ) {
				pFactory->Count = i;
				return false;
			}
		}
		pFactory->Count = i + 1;
	}
	return true;
}



/* 构造一个创建组合正文时立即复制的字节片段描述。 */
XRT_API xhttpbodypiece xrtHttpBodyPieceBytes(xbytesview Data)
{
	xhttpbodypiece Piece;

	memset(&Piece, 0, sizeof(Piece));
	Piece.Kind = XHTTP_BODY_PIECE_BYTES;
	Piece.Bytes = Data;
	return Piece;
}



/* 构造一个创建组合正文时增加引用的子正文片段描述。 */
XRT_API xhttpbodypiece xrtHttpBodyPieceBody(xhttpbody* pBody)
{
	xhttpbodypiece Piece;

	memset(&Piece, 0, sizeof(Piece));
	Piece.Kind = XHTTP_BODY_PIECE_BODY;
	Piece.Body = pBody;
	return Piece;
}



/* 复制字节并保留子正文引用，创建按片段顺序读取的正文。 */
XRT_API xhttpbody* xrtHttpBodyCompose(
	const xhttpbodypiece* pPieces,
	size_t iCount
)
{
	static const xhttpbodyops Ops = {
		__xrtHttpBodyComposeOpen,
		__xrtHttpBodyComposeDestroy
	};
	xrt_http_body_compose_factory* pFactory;
	xhttpbody* pBody;
	size_t iAllocation;
	size_t iBytes;
	uint64 iLength;
	uint32 iFlags;

	if ( iCount == 0 ) {
		return xrtHttpBodyEmpty();
	}
	if ( !__xrtHttpBodyComposeMeasure(
		pPieces,
		iCount,
		&iAllocation,
		&iBytes,
		&iLength,
		&iFlags
	) ) {
		return NULL;
	}
	(void)iBytes;
	if ( iCount == 1 ) {
		if ( pPieces[0].Kind == XHTTP_BODY_PIECE_BYTES ) {
			return xrtHttpBodyCopy(pPieces[0].Bytes);
		}
		return xrtHttpBodyRef(pPieces[0].Body);
	}
	pFactory = (xrt_http_body_compose_factory*)xrtCalloc(
		1, iAllocation
	);
	if ( pFactory == NULL ) {
		return NULL;
	}
	pFactory->RefCount = 1;
	pFactory->AllocationSize = iAllocation;
	if ( !__xrtHttpBodyComposeFill(
		pFactory, pPieces, iCount
	) ) {
		__xrtHttpBodyComposeFactoryRelease(pFactory);
		return NULL;
	}
	pBody = xrtHttpBodyCreate(
		&Ops, pFactory, iLength, iFlags
	);
	if ( pBody == NULL ) {
		__xrtHttpBodyComposeFactoryRelease(pFactory);
	}
	return pBody;
}

#endif
