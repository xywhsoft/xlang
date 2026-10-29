#include "../internal/xrt_http_body_transform.h"



#if defined(XHTTP_FEATURE_HTTP_BODY_TRANSFORM)

/* 输出块同时由 Reader 队列和已经交付的 Chunk 持有引用。 */
typedef struct xrt_http_body_transform_block {
	volatile int32 RefCount;
	struct xrt_http_body_transform_block* Next;
	size_t Size;
	uint8 Data[];
} xrt_http_body_transform_block;



/* 工厂保存来源引用、静态算法操作和逐字节复制的不可变配置。 */
typedef struct xrt_http_body_transform_factory {
	xhttpbody* Source;
	const xrt_http_body_transform_ops* Ops;
	size_t ReadSize;
	size_t QueueLimit;
	size_t ConfigSize;
	uint8 Config[];
} xrt_http_body_transform_factory;



/* 每个 Reader 独立推进来源、算法对象和待交付输出队列。 */
typedef struct xrt_http_body_transform_reader {
	xhttpbodyreader* Source;
	const xrt_http_body_transform_ops* Ops;
	ptr Codec;
	xrt_http_body_transform_block* Head;
	xrt_http_body_transform_block* Tail;
	size_t HeadOffset;
	size_t ReadSize;
	size_t QueueBytes;
	size_t QueueLimit;
	bool Finished;
} xrt_http_body_transform_reader;



/* 归还输出块引用，并在最后一个租约结束时释放内存。 */
static void __xrtHttpBodyTransformBlockRelease(
	xrt_http_body_transform_block* pBlock
)
{
	if ( (pBlock == NULL) ||
		(xrtRefRelease(&pBlock->RefCount) != 0) ) {
		return;
	}
	xrtFree(pBlock);
}



/* 销毁算法对象，并恢复进入清理前持有的当前错误。 */
static void __xrtHttpBodyTransformCodecDestroy(
	const xrt_http_body_transform_ops* pOps,
	ptr pCodec
)
{
	xerror* pPrevious;
	xerror* pDiscard;

	if ( (pOps == NULL) || (pOps->Destroy == NULL) ||
		(pCodec == NULL) ) {
		return;
	}
	pPrevious = __xhttpErrorSwapOwned(NULL);
	pOps->Destroy(pCodec);
	pDiscard = __xhttpErrorSwapOwned(pPrevious);
	xrtErrorFree(pDiscard);
}



/* Chunk 释放过程只归还对应输出块的一个引用。 */
static void __xrtHttpBodyTransformChunkRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pData;
	(void)iSize;
	__xrtHttpBodyTransformBlockRelease(
		(xrt_http_body_transform_block*)pContext
	);
}



/* 清空 Reader 仍持有的全部待交付输出。 */
static void __xrtHttpBodyTransformQueueClear(
	xrt_http_body_transform_reader* pReader
)
{
	xrt_http_body_transform_block* pBlock = pReader->Head;

	while ( pBlock != NULL ) {
		xrt_http_body_transform_block* pNext = pBlock->Next;

		__xrtHttpBodyTransformBlockRelease(pBlock);
		pBlock = pNext;
	}
	pReader->Head = NULL;
	pReader->Tail = NULL;
	pReader->HeadOffset = 0;
	pReader->QueueBytes = 0;
}



/* 把一个临时算法输出片段复制成独立拥有的队列块。 */
static bool __xrtHttpBodyTransformOutput(
	xbytesview Data,
	ptr pData
)
{
	xrt_http_body_transform_reader* pReader =
		(xrt_http_body_transform_reader*)pData;
	xrt_http_body_transform_block* pBlock;

	if ( Data.Size == 0 ) {
		return true;
	}
	if ( !xrtMemRangeValid(Data.Data, Data.Size) ) {
		__xhttpErrorSetInvalidState();
		return false;
	}
	if ( Data.Size > (SIZE_MAX - sizeof(*pBlock)) ) {
		__xhttpErrorSetSizeOverflow();
		return false;
	}
	if ( Data.Size > (SIZE_MAX - pReader->QueueBytes) ) {
		__xhttpErrorSetSizeOverflow();
		return false;
	}
	if ( pReader->QueueLimit != 0 ) {
		if ( (pReader->QueueBytes > pReader->QueueLimit) ||
			(Data.Size >
			 (pReader->QueueLimit - pReader->QueueBytes)) ) {
			__xhttpErrorSetRange();
			return false;
		}
	}
	pBlock = (xrt_http_body_transform_block*)xrtMalloc(
		sizeof(*pBlock) + Data.Size
	);
	if ( pBlock == NULL ) {
		return false;
	}
	pBlock->RefCount = 1;
	pBlock->Next = NULL;
	pBlock->Size = Data.Size;
	memcpy(pBlock->Data, Data.Data, Data.Size);
	if ( pReader->Tail == NULL ) {
		pReader->Head = pBlock;
	} else {
		pReader->Tail->Next = pBlock;
	}
	pReader->Tail = pBlock;
	pReader->QueueBytes += Data.Size;
	return true;
}



/* 从队首发布一个不超过调用方上限的独立 Chunk 租约。 */
static xhttpbodystatus __xrtHttpBodyTransformPublish(
	xrt_http_body_transform_reader* pReader,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	xrt_http_body_transform_block* pBlock = pReader->Head;
	size_t iRemaining;
	size_t iSize;

	if ( pBlock == NULL ) {
		return XHTTP_BODY_EOF;
	}
	iRemaining = pBlock->Size - pReader->HeadOffset;
	iSize = iRemaining < iMaxBytes ? iRemaining : iMaxBytes;
	if ( xrtRefRetain(&pBlock->RefCount) < 0 ) {
		__xhttpErrorSetInvalidState();
		return XHTTP_BODY_ERROR;
	}
	pChunk->Data = pBlock->Data + pReader->HeadOffset;
	pChunk->Size = iSize;
	pChunk->Release = __xrtHttpBodyTransformChunkRelease;
	pChunk->Context = pBlock;
	pReader->HeadOffset += iSize;
	if ( pReader->HeadOffset == pBlock->Size ) {
		pReader->Head = pBlock->Next;
		if ( pReader->Head == NULL ) {
			pReader->Tail = NULL;
		}
		pReader->HeadOffset = 0;
		pReader->QueueBytes -= pBlock->Size;
		__xrtHttpBodyTransformBlockRelease(pBlock);
	}
	return XHTTP_BODY_DATA;
}



/* 同步推进来源，直到产生输出、需要等待、失败或完整结束。 */
static xhttpbodystatus __xrtHttpBodyTransformNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	xrt_http_body_transform_reader* pReader =
		(xrt_http_body_transform_reader*)pContext;

	if ( pReader->Head != NULL ) {
		return __xrtHttpBodyTransformPublish(
			pReader, iMaxBytes, pChunk
		);
	}
	if ( pReader->Finished ) {
		return XHTTP_BODY_EOF;
	}
	for ( ;; ) {
		xhttpbodychunk SourceChunk;
		xhttpbodystatus Status = xrtHttpBodyNext(
			pReader->Source,
			pReader->ReadSize,
			&SourceChunk
		);

		if ( Status == XHTTP_BODY_DATA ) {
			bool bWritten = pReader->Ops->Write(
				pReader->Codec,
				(xbytesview){
					SourceChunk.Data,
					SourceChunk.Size
				},
				false,
				__xrtHttpBodyTransformOutput,
				pReader
			);

			xrtHttpBodyChunkRelease(&SourceChunk);
			if ( !bWritten ) {
				return XHTTP_BODY_ERROR;
			}
			if ( pReader->Head != NULL ) {
				return __xrtHttpBodyTransformPublish(
					pReader, iMaxBytes, pChunk
				);
			}
			continue;
		}
		if ( Status == XHTTP_BODY_AGAIN ) {
			return XHTTP_BODY_AGAIN;
		}
		if ( Status == XHTTP_BODY_ERROR ) {
			return XHTTP_BODY_ERROR;
		}
		if ( !pReader->Ops->Write(
			pReader->Codec,
			(xbytesview){ NULL, 0 },
			true,
			__xrtHttpBodyTransformOutput,
			pReader
		) ) {
			return XHTTP_BODY_ERROR;
		}
		pReader->Finished = true;
		if ( pReader->Head != NULL ) {
			return __xrtHttpBodyTransformPublish(
				pReader, iMaxBytes, pChunk
			);
		}
		return XHTTP_BODY_EOF;
	}
}



#if defined(XHTTP_FEATURE_HTTP_BODY_ASYNC)

/* 把变换 Reader 的一次可读性等待直接委托给来源 Reader。 */
static xfuture* __xrtHttpBodyTransformWait(ptr pContext)
{
	xrt_http_body_transform_reader* pReader =
		(xrt_http_body_transform_reader*)pContext;

	return xrtHttpBodyReaderWait(pReader->Source);
}

#endif



/* 关闭一个变换 Reader，并回收尚未交付的输出。 */
static void __xrtHttpBodyTransformClose(ptr pContext)
{
	xrt_http_body_transform_reader* pReader =
		(xrt_http_body_transform_reader*)pContext;

	if ( pReader == NULL ) {
		return;
	}
	xrtHttpBodyReaderDestroy(pReader->Source);
	__xrtHttpBodyTransformCodecDestroy(
		pReader->Ops, pReader->Codec
	);
	__xrtHttpBodyTransformQueueClear(pReader);
	memset(pReader, 0, sizeof(*pReader));
	xrtFree(pReader);
}



/* 为一次读取创建独立来源 Reader、算法对象和轻量状态。 */
static bool __xrtHttpBodyTransformOpen(
	ptr pFactory,
	xhttpbodyreaderops* pReaderOps,
	ptr* ppReader
)
{
	xrt_http_body_transform_factory* pBody =
		(xrt_http_body_transform_factory*)pFactory;
	xrt_http_body_transform_reader* pReader;

	pReader = (xrt_http_body_transform_reader*)xrtCalloc(
		1, sizeof(*pReader)
	);
	if ( pReader == NULL ) {
		return false;
	}
	pReader->Ops = pBody->Ops;
	pReader->Codec = pBody->Ops->Create(pBody->Config);
	if ( pReader->Codec == NULL ) {
		xrtFree(pReader);
		return false;
	}
	pReader->Source = xrtHttpBodyOpen(pBody->Source);
	if ( pReader->Source == NULL ) {
		__xrtHttpBodyTransformCodecDestroy(
			pReader->Ops, pReader->Codec
		);
		xrtFree(pReader);
		return false;
	}
	pReader->ReadSize = pBody->ReadSize;
	pReader->QueueLimit = pBody->QueueLimit;
	memset(pReaderOps, 0, sizeof(*pReaderOps));
	pReaderOps->Next = __xrtHttpBodyTransformNext;
	pReaderOps->Close = __xrtHttpBodyTransformClose;
#if defined(XHTTP_FEATURE_HTTP_BODY_ASYNC)
	pReaderOps->Wait = __xrtHttpBodyTransformWait;
#endif
	*ppReader = pReader;
	return true;
}



/* 释放变换正文工厂持有的来源引用和配置副本。 */
static void __xrtHttpBodyTransformDestroy(ptr pFactory)
{
	xrt_http_body_transform_factory* pBody =
		(xrt_http_body_transform_factory*)pFactory;
	size_t iFactorySize;

	if ( pBody == NULL ) {
		return;
	}
	iFactorySize = sizeof(*pBody) + pBody->ConfigSize;
	xrtHttpBodyDestroy(pBody->Source);
	memset(pBody, 0, iFactorySize);
	xrtFree(pBody);
}



/* 验证内部变换器参数不会形成空算法或越界配置。 */
static bool __xrtHttpBodyTransformValid(
	xhttpbody* pSource,
	const xrt_http_body_transform_ops* pOps,
	const void* pConfig,
	size_t iConfigSize,
	size_t iReadSize
)
{
	if ( (pSource == NULL) || (pOps == NULL) ||
		(pOps->Create == NULL) || (pOps->Write == NULL) ||
		(pOps->Destroy == NULL) ||
		(iConfigSize == 0) || (iReadSize == 0) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtMemRangeValid(pConfig, iConfigSize) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	if ( iConfigSize > (SIZE_MAX - sizeof(
		xrt_http_body_transform_factory
	)) ) {
		__xhttpErrorSetSizeOverflow();
		return false;
	}
	return true;
}



/* 创建持有来源引用和配置副本的通用流式变换正文。 */
xhttpbody* __xrtHttpBodyTransformCreate(
	xhttpbody* pSource,
	const xrt_http_body_transform_ops* pOps,
	const void* pConfig,
	size_t iConfigSize,
	size_t iReadSize,
	size_t iQueueLimit
)
{
	static const xhttpbodyops BodyOps = {
		__xrtHttpBodyTransformOpen,
		__xrtHttpBodyTransformDestroy
	};
	xrt_http_body_transform_factory* pFactory;
	xhttpbody* pBody;
	uint32 iFlags;

	if ( !__xrtHttpBodyTransformValid(
		pSource,
		pOps,
		pConfig,
		iConfigSize,
		iReadSize
	) ) {
		return NULL;
	}
	pFactory = (xrt_http_body_transform_factory*)xrtMalloc(
		sizeof(*pFactory) + iConfigSize
	);
	if ( pFactory == NULL ) {
		return NULL;
	}
	pFactory->Source = NULL;
	pFactory->Ops = pOps;
	pFactory->ReadSize = iReadSize;
	pFactory->QueueLimit = iQueueLimit;
	pFactory->ConfigSize = iConfigSize;
	memcpy(pFactory->Config, pConfig, iConfigSize);
	pFactory->Source = xrtHttpBodyRef(pSource);
	if ( pFactory->Source == NULL ) {
		memset(
			pFactory,
			0,
			sizeof(*pFactory) + iConfigSize
		);
		xrtFree(pFactory);
		return NULL;
	}
	iFlags = xrtHttpBodyReplayable(pSource) ?
		XHTTP_BODY_REPLAYABLE : XHTTP_BODY_NONE;
	pBody = xrtHttpBodyCreate(
		&BodyOps,
		pFactory,
		XHTTP_BODY_UNKNOWN,
		iFlags
	);
	if ( pBody == NULL ) {
		__xrtHttpBodyTransformDestroy(pFactory);
	}
	return pBody;
}

#endif
