#include "xl_object_runtime.h"

#include "xlrt_generated.h"

typedef struct XL_ObjectCollectNode {
	XL_ObjectHeader* pHeader;
	xvalue value;
	uint32_t uIncomingCount;
	uint32_t uExternalCount;
	unsigned char bReachable;
	unsigned char bCollect;
	unsigned char bQueued;
} XL_ObjectCollectNode;

typedef struct XL_ObjectCollectVisitState {
	xvalue* arrSeen;
	size_t iSeenCount;
	size_t iSeenCap;
} XL_ObjectCollectVisitState;

#define XL_GENERATED_GENERATOR_MAGIC 0x584C4747u
#define XL_GENERATED_FUTURE_MAGIC    0x584C4654u

typedef struct XL_RuntimeGeneratorBox {
	uint32_t uMagic;
	XL_ObjectTraceStateFn pTraceState;
	void* pNext;
	void* pState;
	size_t iStateSize;
	int iValueKind;
	const char* sName;
} XL_RuntimeGeneratorBox;

typedef struct XL_RuntimeFutureBox {
	uint32_t uMagic;
	xfuture* handle;
} XL_RuntimeFutureBox;

typedef struct XL_ObjectFutureTraceCtx {
	uint32_t uMagic;
	xfuture* pFuture;
	const void* pState;
	XL_ObjectTraceStateFn pTraceState;
	struct XL_ObjectFutureTraceCtx* pPrev;
	struct XL_ObjectFutureTraceCtx* pNext;
} XL_ObjectFutureTraceCtx;

typedef struct XL_ObjectCollectAccumulateCtx {
	const XL_ObjectCollectNode* arrNodes;
	size_t iNodeCount;
	uint32_t* arrIncoming;
	XL_ObjectCollectVisitState* pSeen;
} XL_ObjectCollectAccumulateCtx;

typedef struct XL_ObjectCollectPushCtx {
	XL_ObjectCollectNode* arrNodes;
	size_t iNodeCount;
	size_t* arrStack;
	size_t* piStackCount;
	XL_ObjectCollectVisitState* pSeen;
} XL_ObjectCollectPushCtx;

static XL_ObjectHeader* g_pXlObjectRegistryHead = NULL;
static XL_ObjectFutureTraceCtx* g_pXlFutureTraceHead = NULL;
static XL_ObjectPinnedRef* g_pXlPinnedRootHead = NULL;
static char* g_sXlObjectMetaFieldNameScratch = NULL;
static size_t g_iXlObjectMetaFieldNameScratchCap = 0u;
static char* g_sXlObjectMetaMethodNameScratch = NULL;
static size_t g_iXlObjectMetaMethodNameScratchCap = 0u;
static xmutex g_pXlFutureTraceLock = NULL;
static xmutex g_pXlPinnedRootLock = NULL;

static xmutex __xlObjectFutureTraceLock(void)
{
	if ( g_pXlFutureTraceLock == NULL ) {
		g_pXlFutureTraceLock = xrtMutexCreate();
	}
	return g_pXlFutureTraceLock;
}

static xmutex __xlObjectPinnedRootLock(void)
{
	if ( g_pXlPinnedRootLock == NULL ) {
		g_pXlPinnedRootLock = xrtMutexCreate();
	}
	return g_pXlPinnedRootLock;
}

static size_t __xlObjectMetaPublicMethodNameLen(const char* sDispatchKey)
{
	const char* sArity;

	if ( sDispatchKey == NULL ) {
		return 0u;
	}
	sArity = strstr(sDispatchKey, "$arity");
	if ( sArity == NULL ) {
		return strlen(sDispatchKey);
	}
	return (size_t)(sArity - sDispatchKey);
}

static void __xlObjectFutureTraceInsert(XL_ObjectFutureTraceCtx* pCtx)
{
	xmutex pLock;

	if ( pCtx == NULL ) {
		return;
	}
	pLock = __xlObjectFutureTraceLock();
	if ( pLock == NULL ) {
		return;
	}
	xrtMutexLock(pLock);
	pCtx->pPrev = NULL;
	pCtx->pNext = g_pXlFutureTraceHead;
	if ( g_pXlFutureTraceHead != NULL ) {
		g_pXlFutureTraceHead->pPrev = pCtx;
	}
	g_pXlFutureTraceHead = pCtx;
	xrtMutexUnlock(pLock);
}

static void __xlObjectFutureTraceRemove(XL_ObjectFutureTraceCtx* pCtx)
{
	xmutex pLock;

	if ( pCtx == NULL ) {
		return;
	}
	pLock = __xlObjectFutureTraceLock();
	if ( pLock == NULL ) {
		return;
	}
	xrtMutexLock(pLock);
	if ( pCtx->pPrev != NULL ) {
		pCtx->pPrev->pNext = pCtx->pNext;
	} else if ( g_pXlFutureTraceHead == pCtx ) {
		g_pXlFutureTraceHead = pCtx->pNext;
	}
	if ( pCtx->pNext != NULL ) {
		pCtx->pNext->pPrev = pCtx->pPrev;
	}
	pCtx->pPrev = NULL;
	pCtx->pNext = NULL;
	xrtMutexUnlock(pLock);
}

static XL_ObjectFutureTraceCtx* __xlObjectFutureTraceCtx(const xfuture* pFuture)
{
	XL_ObjectFutureTraceCtx* pCtx;
	xmutex pLock;

	if ( pFuture == NULL ) {
		return NULL;
	}
	pLock = __xlObjectFutureTraceLock();
	if ( pLock == NULL ) {
		return NULL;
	}
	xrtMutexLock(pLock);
	for ( pCtx = g_pXlFutureTraceHead; pCtx != NULL; pCtx = pCtx->pNext ) {
		if ( (pCtx->uMagic == XL_GENERATED_FUTURE_MAGIC) && (pCtx->pFuture == pFuture) ) {
			xrtMutexUnlock(pLock);
			return pCtx;
		}
	}
	xrtMutexUnlock(pLock);
	return NULL;
}

static XL_ObjectHeader* __xlObjectHeader(void* pBlob)
{
	return (XL_ObjectHeader*)pBlob;
}

static const XL_ObjectHeader* __xlObjectHeaderConst(const void* pBlob)
{
	return (const XL_ObjectHeader*)pBlob;
}

static int __xlObjectHeaderAliveRaw(const XL_ObjectHeader* pHeader)
{
	return (pHeader != NULL) &&
		(pHeader->uMagic == XL_OBJECT_HEADER_MAGIC) &&
		((pHeader->uFlags & XL_OBJECT_FLAG_DESTROYED) == 0);
}

static int __xlObjectHeaderDestroyingRaw(const XL_ObjectHeader* pHeader)
{
	return (pHeader != NULL) &&
		(pHeader->uMagic == XL_OBJECT_HEADER_MAGIC) &&
		((pHeader->uFlags & XL_OBJECT_FLAG_DESTROYING) != 0);
}

static int __xlObjectHeaderDestroyedRaw(const XL_ObjectHeader* pHeader)
{
	return (pHeader != NULL) &&
		(pHeader->uMagic == XL_OBJECT_HEADER_MAGIC) &&
		((pHeader->uFlags & XL_OBJECT_FLAG_DESTROYED) != 0);
}

static size_t __xlObjectKeyLen(const char* sKey)
{
	return (sKey != NULL) ? strlen(sKey) : 0u;
}

static void* __xlObjectDispatchInType(const XL_ObjectTypeInfo* pTypeInfo, const char* sDispatchKey)
{
	size_t i;

	if ( (pTypeInfo == NULL) || (sDispatchKey == NULL) || (sDispatchKey[0] == '\0') ) {
		return NULL;
	}
	for ( i = 0; i < pTypeInfo->iMethodCount; ++i ) {
		const XL_ObjectMethodEntry* pEntry = &pTypeInfo->pMethods[i];

		if ( (pEntry->sDispatchKey != NULL) && (strcmp(pEntry->sDispatchKey, sDispatchKey) == 0) ) {
			return pEntry->pFunc;
		}
	}
	return NULL;
}

static void __xlObjectRegistryInsert(XL_ObjectHeader* pHeader)
{
	if ( pHeader == NULL ) {
		return;
	}
	if ( (g_pXlObjectRegistryHead == pHeader) || (pHeader->pTrackPrev != NULL) || (pHeader->pTrackNext != NULL) ) {
		return;
	}
	pHeader->pTrackPrev = NULL;
	pHeader->pTrackNext = g_pXlObjectRegistryHead;
	if ( g_pXlObjectRegistryHead != NULL ) {
		g_pXlObjectRegistryHead->pTrackPrev = pHeader;
	}
	g_pXlObjectRegistryHead = pHeader;
}

static void __xlObjectRegistryRemove(XL_ObjectHeader* pHeader)
{
	if ( pHeader == NULL ) {
		return;
	}
	if ( pHeader->pTrackPrev != NULL ) {
		pHeader->pTrackPrev->pTrackNext = pHeader->pTrackNext;
	} else if ( g_pXlObjectRegistryHead == pHeader ) {
		g_pXlObjectRegistryHead = pHeader->pTrackNext;
	}
	if ( pHeader->pTrackNext != NULL ) {
		pHeader->pTrackNext->pTrackPrev = pHeader->pTrackPrev;
	}
	pHeader->pTrackPrev = NULL;
	pHeader->pTrackNext = NULL;
}

static xvalue __xlObjectHeaderOwner(const XL_ObjectHeader* pHeader)
{
	xvalue value;

	if ( pHeader == NULL ) {
		return NULL;
	}
	value = pHeader->pOwner;
	if ( (value == NULL) || (value->Type != XVO_DT_CLASS) ) {
		return NULL;
	}
	if ( xvoGetClass(value) != (void*)pHeader ) {
		return NULL;
	}
	return value;
}

static size_t __xlObjectRegistryCount(void)
{
	size_t iCount;
	const XL_ObjectHeader* pHeader;

	iCount = 0u;
	for ( pHeader = g_pXlObjectRegistryHead; pHeader != NULL; pHeader = pHeader->pTrackNext ) {
		if ( __xlObjectHeaderAliveRaw(pHeader) && (__xlObjectHeaderOwner(pHeader) != NULL) ) {
			++iCount;
		}
	}
	return iCount;
}

static int __xlObjectCollectNodeIndex(const XL_ObjectCollectNode* arrNodes, size_t iNodeCount, const XL_ObjectHeader* pHeader)
{
	size_t i;

	if ( (arrNodes == NULL) || (pHeader == NULL) ) {
		return -1;
	}
	for ( i = 0; i < iNodeCount; ++i ) {
		if ( arrNodes[i].pHeader == pHeader ) {
			return (int)i;
		}
	}
	return -1;
}

static int __xlObjectCollectTargetIndex(const XL_ObjectCollectNode* arrNodes, size_t iNodeCount, xvalue value)
{
	void* pBlob;
	const XL_ObjectHeader* pHeader;

	if ( (value == NULL) || (value->Type != XVO_DT_CLASS) ) {
		return -1;
	}
	pBlob = xvoGetClass(value);
	if ( pBlob == NULL ) {
		return -1;
	}
	pHeader = __xlObjectHeaderConst(pBlob);
	if ( !__xlObjectHeaderAliveRaw(pHeader) ) {
		return -1;
	}
	return __xlObjectCollectNodeIndex(arrNodes, iNodeCount, pHeader);
}

static int __xlObjectCollectSeenContains(const XL_ObjectCollectVisitState* pState, xvalue value)
{
	size_t i;

	if ( (pState == NULL) || (value == NULL) ) {
		return 0;
	}
	for ( i = 0u; i < pState->iSeenCount; ++i ) {
		if ( pState->arrSeen[i] == value ) {
			return 1;
		}
	}
	return 0;
}

static int __xlObjectCollectSeenPush(XL_ObjectCollectVisitState* pState, xvalue value)
{
	xvalue* arrNew;
	size_t iNewCap;

	if ( (pState == NULL) || (value == NULL) ) {
		return 0;
	}
	if ( __xlObjectCollectSeenContains(pState, value) ) {
		return 1;
	}
	if ( pState->iSeenCount >= pState->iSeenCap ) {
		iNewCap = (pState->iSeenCap == 0u) ? 8u : (pState->iSeenCap * 2u);
		arrNew = (xvalue*)realloc(pState->arrSeen, iNewCap * sizeof(*arrNew));
		if ( arrNew == NULL ) {
			return 0;
		}
		pState->arrSeen = arrNew;
		pState->iSeenCap = iNewCap;
	}
	pState->arrSeen[pState->iSeenCount++] = value;
	return 1;
}

static void __xlObjectCollectSeenFree(XL_ObjectCollectVisitState* pState)
{
	if ( pState == NULL ) {
		return;
	}
	free(pState->arrSeen);
	pState->arrSeen = NULL;
	pState->iSeenCount = 0u;
	pState->iSeenCap = 0u;
}

static const XL_RuntimeGeneratorBox* __xlRuntimeGeneratorBox(xvalue value)
{
	const XL_RuntimeGeneratorBox* pBox;

	if ( (value == NULL) || (value->Type != XVO_DT_CLASS) || (xvoGetClass(value) == NULL) ) {
		return NULL;
	}
	if ( value->Size < sizeof(XL_RuntimeGeneratorBox) ) {
		return NULL;
	}
	pBox = (const XL_RuntimeGeneratorBox*)xvoGetClass(value);
	if ( pBox->uMagic != XL_GENERATED_GENERATOR_MAGIC ) {
		return NULL;
	}
	return pBox;
}

static const XL_RuntimeFutureBox* __xlRuntimeFutureBox(xvalue value)
{
	const XL_RuntimeFutureBox* pBox;

	if ( (value == NULL) || (value->Type != XVO_DT_CLASS) || (xvoGetClass(value) == NULL) ) {
		return NULL;
	}
	if ( value->Size < sizeof(XL_RuntimeFutureBox) ) {
		return NULL;
	}
	pBox = (const XL_RuntimeFutureBox*)xvoGetClass(value);
	if ( pBox->uMagic != XL_GENERATED_FUTURE_MAGIC ) {
		return NULL;
	}
	return pBox;
}

static int __xlObjectCollectVisitAccumulate(xvalue value, void* pCtxVoid);
static int __xlObjectCollectVisitPush(xvalue value, void* pCtxVoid);

int xl_object_future_bind_trace(xfuture* pFuture, const void* pState, XL_ObjectTraceStateFn pTraceState)
{
	XL_ObjectFutureTraceCtx* pCtx;

	if ( (pFuture == NULL) || (pState == NULL) || (pTraceState == NULL) ) {
		return 0;
	}
	if ( __xlObjectFutureTraceCtx(pFuture) != NULL ) {
		return 1;
	}
	pCtx = (XL_ObjectFutureTraceCtx*)xrtMalloc(sizeof(*pCtx));
	if ( pCtx == NULL ) {
		return 0;
	}
	memset(pCtx, 0, sizeof(*pCtx));
	pCtx->uMagic = XL_GENERATED_FUTURE_MAGIC;
	pCtx->pFuture = pFuture;
	pCtx->pState = pState;
	pCtx->pTraceState = pTraceState;
	__xlObjectFutureTraceInsert(pCtx);
	return 1;
}

int xl_object_future_clear_trace(xfuture* pFuture)
{
	XL_ObjectFutureTraceCtx* pCtx;

	pCtx = __xlObjectFutureTraceCtx(pFuture);
	if ( pCtx == NULL ) {
		return 0;
	}
	__xlObjectFutureTraceRemove(pCtx);
	xrtFree(pCtx);
	return 1;
}

int xl_object_future_clear_promise_trace(xpromise* pPromise)
{
	xfuture* pFuture;

	if ( pPromise == NULL ) {
		return 0;
	}
	pFuture = xPromisePeekFuture(pPromise);
	while ( pFuture != NULL ) {
		if ( xl_object_future_clear_trace(pFuture) ) {
			return 1;
		}
		pFuture = xFuturePeekGroupSource(pFuture);
	}
	return 0;
}

int xl_object_future_trace_handle(const xfuture* pFuture, XL_ObjectGraphTraceVisitor pVisit, void* pCtx)
{
	const XL_ObjectFutureTraceCtx* pTraceCtx;

	if ( (pFuture == NULL) || (pVisit == NULL) ) {
		return 1;
	}
	if ( xFutureState((xfuture*)pFuture) != XFUTURE_PENDING ) {
		return 1;
	}
	pTraceCtx = __xlObjectFutureTraceCtx(pFuture);
	if ( (pTraceCtx != NULL) && (pTraceCtx->pTraceState != NULL) && (pTraceCtx->pState != NULL) ) {
		if ( !pTraceCtx->pTraceState(pTraceCtx->pState, pVisit, pCtx) ) {
			return 0;
		}
	}
	if ( xFuturePeekGroupSource((xfuture*)pFuture) != NULL ) {
		return xl_object_future_trace_handle(xFuturePeekGroupSource((xfuture*)pFuture), pVisit, pCtx);
	}
	return 1;
}

int xl_object_trace_blob(const void* pBlob, XL_ObjectGraphTraceVisitor pVisit, void* pCtx)
{
	const XL_ObjectHeader* pHeader = (const XL_ObjectHeader*)pBlob;

	if ( (pHeader == NULL) || (pVisit == NULL) ) {
		return 1;
	}
	if ( !xl_object_header_alive(pBlob) ) {
		return 1;
	}
	if ( pHeader->dynamicFields != NULL ) {
		return pVisit(pHeader->dynamicFields, pCtx);
	}
	return 1;
}

static int __xlObjectCollectAccumulateValue(const XL_ObjectCollectNode* arrNodes, size_t iNodeCount, xvalue value, uint32_t* arrIncoming, XL_ObjectCollectVisitState* pState)
{
	uint32_t i;
	int iTargetIndex;

	if ( (arrNodes == NULL) || (arrIncoming == NULL) || (value == NULL) ) {
		return 1;
	}

	iTargetIndex = __xlObjectCollectTargetIndex(arrNodes, iNodeCount, value);
	if ( iTargetIndex >= 0 ) {
		arrIncoming[iTargetIndex] += 1u;
		return 1;
	}

	if ( value->Type == XVO_DT_CLASS ) {
		const XL_RuntimeGeneratorBox* pGeneratorBox = __xlRuntimeGeneratorBox(value);
		const XL_RuntimeFutureBox* pFutureBox = __xlRuntimeFutureBox(value);

		if ( pGeneratorBox != NULL ) {
			XL_ObjectCollectAccumulateCtx ctx;

			if ( !__xlObjectCollectSeenPush(pState, value) ) {
				return 0;
			}
			if ( (pGeneratorBox->pTraceState == NULL) || (pGeneratorBox->pState == NULL) ) {
				return 1;
			}
			ctx.arrNodes = arrNodes;
			ctx.iNodeCount = iNodeCount;
			ctx.arrIncoming = arrIncoming;
			ctx.pSeen = pState;
			return pGeneratorBox->pTraceState(pGeneratorBox->pState, __xlObjectCollectVisitAccumulate, &ctx);
		}
		if ( pFutureBox != NULL ) {
			if ( !__xlObjectCollectSeenPush(pState, value) ) {
				return 0;
			}
			return xl_object_future_trace_handle(pFutureBox->handle, __xlObjectCollectVisitAccumulate, &(XL_ObjectCollectAccumulateCtx) {
				arrNodes,
				iNodeCount,
				arrIncoming,
				pState
			});
		}
	}

	switch ( value->Type ) {
	case XVO_DT_FUNC:
		if ( !__xlObjectCollectSeenPush(pState, value) ) {
			return 0;
		}
		return __xlObjectCollectAccumulateValue(arrNodes, iNodeCount, xvoGetFuncEnv(value), arrIncoming, pState);
	case XVO_DT_ARRAY:
		if ( !__xlObjectCollectSeenPush(pState, value) ) {
			return 0;
		}
		for ( i = 0u; i < xvoArrayItemCount(value); ++i ) {
			if ( !__xlObjectCollectAccumulateValue(arrNodes, iNodeCount, xvoArrayGetValue(value, i), arrIncoming, pState) ) {
				return 0;
			}
		}
		return 1;
	case XVO_DT_LIST:
		if ( !__xlObjectCollectSeenPush(pState, value) ) {
			return 0;
		}
		for ( i = 0u; i < xvoListItemCount(value); ++i ) {
			if ( !__xlObjectCollectAccumulateValue(arrNodes, iNodeCount, xvoListGetValue(value, (int64)i), arrIncoming, pState) ) {
				return 0;
			}
		}
		return 1;
	case XVO_DT_COLL:
		{
			xavltree pColl = xvoGetColl(value);
			Coll_Key* pKey;

			if ( !__xlObjectCollectSeenPush(pState, value) ) {
				return 0;
			}
			if ( pColl == NULL ) {
				return 1;
			}
			xrtAVLTB_IterBegin((xavltbase)pColl);
			for ( pKey = (Coll_Key*)xrtAVLTB_IterNext((xavltbase)pColl); pKey != NULL; pKey = (Coll_Key*)xrtAVLTB_IterNext((xavltbase)pColl) ) {
				if ( !__xlObjectCollectAccumulateValue(arrNodes, iNodeCount, pKey->Value, arrIncoming, pState) ) {
					xrtAVLTB_IterEnd((xavltbase)pColl);
					return 0;
				}
			}
			xrtAVLTB_IterEnd((xavltbase)pColl);
			return 1;
		}
	case XVO_DT_TABLE:
		{
			xdict pTable = xvoGetTable(value);
			Dict_Key* pKey;

			if ( !__xlObjectCollectSeenPush(pState, value) ) {
				return 0;
			}
			if ( pTable == NULL ) {
				return 1;
			}
			xrtAVLTB_IterBegin((xavltbase)pTable);
			for ( pKey = (Dict_Key*)xrtAVLTB_IterNext((xavltbase)pTable); pKey != NULL; pKey = (Dict_Key*)xrtAVLTB_IterNext((xavltbase)pTable) ) {
				if ( !__xlObjectCollectAccumulateValue(arrNodes, iNodeCount, *(xvalue*)(&pKey[1]), arrIncoming, pState) ) {
					xrtAVLTB_IterEnd((xavltbase)pTable);
					return 0;
				}
			}
			xrtAVLTB_IterEnd((xavltbase)pTable);
			return 1;
		}
	default:
		return 1;
	}
}

static int __xlObjectCollectVisitAccumulate(xvalue value, void* pCtxVoid)
{
	const XL_ObjectCollectAccumulateCtx* pCtx = (const XL_ObjectCollectAccumulateCtx*)pCtxVoid;

	if ( pCtx == NULL ) {
		return 0;
	}
	return __xlObjectCollectAccumulateValue(
		pCtx->arrNodes,
		pCtx->iNodeCount,
		value,
		pCtx->arrIncoming,
		pCtx->pSeen
	);
}

static int __xlObjectCollectAccumulateEdges(const XL_ObjectCollectNode* arrNodes, size_t iNodeCount, const XL_ObjectHeader* pHeader, uint32_t* arrIncoming)
{
	XL_ObjectCollectVisitState state;
	int iRc;

	memset(&state, 0, sizeof(state));
	if ( (arrNodes == NULL) || (arrIncoming == NULL) || (pHeader == NULL) || (pHeader->dynamicFields == NULL) ) {
		return 1;
	}
	iRc = __xlObjectCollectAccumulateValue(arrNodes, iNodeCount, pHeader->dynamicFields, arrIncoming, &state);
	__xlObjectCollectSeenFree(&state);
	return iRc;
}

static int __xlObjectCollectPushValue(XL_ObjectCollectNode* arrNodes, size_t iNodeCount, xvalue value, size_t* arrStack, size_t* piStackCount, XL_ObjectCollectVisitState* pState)
{
	uint32_t i;
	int iTargetIndex;

	if ( (arrNodes == NULL) || (arrStack == NULL) || (piStackCount == NULL) || (value == NULL) ) {
		return 1;
	}

	iTargetIndex = __xlObjectCollectTargetIndex(arrNodes, iNodeCount, value);
	if ( iTargetIndex >= 0 ) {
		if ( !arrNodes[iTargetIndex].bReachable && !arrNodes[iTargetIndex].bQueued ) {
			arrNodes[iTargetIndex].bQueued = 1u;
			arrStack[*piStackCount] = (size_t)iTargetIndex;
			*piStackCount += 1u;
		}
		return 1;
	}

	if ( value->Type == XVO_DT_CLASS ) {
		const XL_RuntimeGeneratorBox* pGeneratorBox = __xlRuntimeGeneratorBox(value);
		const XL_RuntimeFutureBox* pFutureBox = __xlRuntimeFutureBox(value);

		if ( pGeneratorBox != NULL ) {
			XL_ObjectCollectPushCtx ctx;

			if ( !__xlObjectCollectSeenPush(pState, value) ) {
				return 0;
			}
			if ( (pGeneratorBox->pTraceState == NULL) || (pGeneratorBox->pState == NULL) ) {
				return 1;
			}
			ctx.arrNodes = arrNodes;
			ctx.iNodeCount = iNodeCount;
			ctx.arrStack = arrStack;
			ctx.piStackCount = piStackCount;
			ctx.pSeen = pState;
			return pGeneratorBox->pTraceState(pGeneratorBox->pState, __xlObjectCollectVisitPush, &ctx);
		}
		if ( pFutureBox != NULL ) {
			if ( !__xlObjectCollectSeenPush(pState, value) ) {
				return 0;
			}
			return xl_object_future_trace_handle(pFutureBox->handle, __xlObjectCollectVisitPush, &(XL_ObjectCollectPushCtx) {
				arrNodes,
				iNodeCount,
				arrStack,
				piStackCount,
				pState
			});
		}
	}

	switch ( value->Type ) {
	case XVO_DT_FUNC:
		if ( !__xlObjectCollectSeenPush(pState, value) ) {
			return 0;
		}
		return __xlObjectCollectPushValue(arrNodes, iNodeCount, xvoGetFuncEnv(value), arrStack, piStackCount, pState);
	case XVO_DT_ARRAY:
		if ( !__xlObjectCollectSeenPush(pState, value) ) {
			return 0;
		}
		for ( i = 0u; i < xvoArrayItemCount(value); ++i ) {
			if ( !__xlObjectCollectPushValue(arrNodes, iNodeCount, xvoArrayGetValue(value, i), arrStack, piStackCount, pState) ) {
				return 0;
			}
		}
		return 1;
	case XVO_DT_LIST:
		if ( !__xlObjectCollectSeenPush(pState, value) ) {
			return 0;
		}
		for ( i = 0u; i < xvoListItemCount(value); ++i ) {
			if ( !__xlObjectCollectPushValue(arrNodes, iNodeCount, xvoListGetValue(value, (int64)i), arrStack, piStackCount, pState) ) {
				return 0;
			}
		}
		return 1;
	case XVO_DT_COLL:
		{
			xavltree pColl = xvoGetColl(value);
			Coll_Key* pKey;

			if ( !__xlObjectCollectSeenPush(pState, value) ) {
				return 0;
			}
			if ( pColl == NULL ) {
				return 1;
			}
			xrtAVLTB_IterBegin((xavltbase)pColl);
			for ( pKey = (Coll_Key*)xrtAVLTB_IterNext((xavltbase)pColl); pKey != NULL; pKey = (Coll_Key*)xrtAVLTB_IterNext((xavltbase)pColl) ) {
				if ( !__xlObjectCollectPushValue(arrNodes, iNodeCount, pKey->Value, arrStack, piStackCount, pState) ) {
					xrtAVLTB_IterEnd((xavltbase)pColl);
					return 0;
				}
			}
			xrtAVLTB_IterEnd((xavltbase)pColl);
			return 1;
		}
	case XVO_DT_TABLE:
		{
			xdict pTable = xvoGetTable(value);
			Dict_Key* pKey;

			if ( !__xlObjectCollectSeenPush(pState, value) ) {
				return 0;
			}
			if ( pTable == NULL ) {
				return 1;
			}
			xrtAVLTB_IterBegin((xavltbase)pTable);
			for ( pKey = (Dict_Key*)xrtAVLTB_IterNext((xavltbase)pTable); pKey != NULL; pKey = (Dict_Key*)xrtAVLTB_IterNext((xavltbase)pTable) ) {
				if ( !__xlObjectCollectPushValue(arrNodes, iNodeCount, *(xvalue*)(&pKey[1]), arrStack, piStackCount, pState) ) {
					xrtAVLTB_IterEnd((xavltbase)pTable);
					return 0;
				}
			}
			xrtAVLTB_IterEnd((xavltbase)pTable);
			return 1;
		}
	default:
		return 1;
	}
}

static int __xlObjectCollectVisitPush(xvalue value, void* pCtxVoid)
{
	XL_ObjectCollectPushCtx* pCtx = (XL_ObjectCollectPushCtx*)pCtxVoid;

	if ( pCtx == NULL ) {
		return 0;
	}
	return __xlObjectCollectPushValue(
		pCtx->arrNodes,
		pCtx->iNodeCount,
		value,
		pCtx->arrStack,
		pCtx->piStackCount,
		pCtx->pSeen
	);
}

static int __xlObjectCollectPushEdges(XL_ObjectCollectNode* arrNodes, size_t iNodeCount, const XL_ObjectHeader* pHeader, size_t* arrStack, size_t* piStackCount)
{
	XL_ObjectCollectVisitState state;
	int iRc;

	memset(&state, 0, sizeof(state));
	if ( (arrNodes == NULL) || (arrStack == NULL) || (piStackCount == NULL) || (pHeader == NULL) || (pHeader->dynamicFields == NULL) ) {
		return 1;
	}
	iRc = __xlObjectCollectPushValue(arrNodes, iNodeCount, pHeader->dynamicFields, arrStack, piStackCount, &state);
	__xlObjectCollectSeenFree(&state);
	return iRc;
}

static int __xlObjectCollectPushFutureRoots(XL_ObjectCollectNode* arrNodes, size_t iNodeCount, size_t* arrStack, size_t* piStackCount)
{
	XL_ObjectCollectVisitState state;
	XL_ObjectCollectPushCtx ctx;
	xmutex pLock;
	XL_ObjectFutureTraceCtx* pTraceCtx;

	memset(&state, 0, sizeof(state));
	ctx.arrNodes = arrNodes;
	ctx.iNodeCount = iNodeCount;
	ctx.arrStack = arrStack;
	ctx.piStackCount = piStackCount;
	ctx.pSeen = &state;
	pLock = __xlObjectFutureTraceLock();
	if ( pLock == NULL ) {
		return 1;
	}
	xrtMutexLock(pLock);
	for ( pTraceCtx = g_pXlFutureTraceHead; pTraceCtx != NULL; pTraceCtx = pTraceCtx->pNext ) {
		if ( (pTraceCtx->pFuture == NULL) || (xFutureState(pTraceCtx->pFuture) != XFUTURE_PENDING) ) {
			continue;
		}
		if ( (pTraceCtx->pTraceState != NULL) &&
			(pTraceCtx->pState != NULL) &&
			!pTraceCtx->pTraceState(pTraceCtx->pState, __xlObjectCollectVisitPush, &ctx) ) {
			xrtMutexUnlock(pLock);
			__xlObjectCollectSeenFree(&state);
			return 0;
		}
	}
	xrtMutexUnlock(pLock);
	__xlObjectCollectSeenFree(&state);
	return 1;
}

static int __xlObjectCollectPushPinnedRoots(XL_ObjectCollectNode* arrNodes, size_t iNodeCount, size_t* arrStack, size_t* piStackCount)
{
	XL_ObjectCollectVisitState state;
	xmutex pLock;
	XL_ObjectPinnedRef* pPinnedRef;

	memset(&state, 0, sizeof(state));
	pLock = __xlObjectPinnedRootLock();
	if ( pLock == NULL ) {
		return 1;
	}
	xrtMutexLock(pLock);
	for ( pPinnedRef = g_pXlPinnedRootHead; pPinnedRef != NULL; pPinnedRef = pPinnedRef->pNext ) {
		if ( (pPinnedRef->value != NULL) &&
			!__xlObjectCollectPushValue(arrNodes, iNodeCount, pPinnedRef->value, arrStack, piStackCount, &state) ) {
			xrtMutexUnlock(pLock);
			__xlObjectCollectSeenFree(&state);
			return 0;
		}
	}
	xrtMutexUnlock(pLock);
	__xlObjectCollectSeenFree(&state);
	return 1;
}

static void __xlObjectWeakDetach(XL_ObjectWeakRef* pWeakRef)
{
	XL_ObjectHeader* pHeader;

	if ( pWeakRef == NULL ) {
		return;
	}

	pHeader = (pWeakRef->pBlob != NULL) ? __xlObjectHeader(pWeakRef->pBlob) : NULL;
	if ( pWeakRef->pPrev != NULL ) {
		pWeakRef->pPrev->pNext = pWeakRef->pNext;
	} else if ( (pHeader != NULL) && (pHeader->uMagic == XL_OBJECT_HEADER_MAGIC) && (pHeader->pWeakRefs == pWeakRef) ) {
		pHeader->pWeakRefs = pWeakRef->pNext;
	}
	if ( pWeakRef->pNext != NULL ) {
		pWeakRef->pNext->pPrev = pWeakRef->pPrev;
	}
	pWeakRef->pPrev = NULL;
	pWeakRef->pNext = NULL;
	pWeakRef->pValue = NULL;
	pWeakRef->pBlob = NULL;
}

static void __xlObjectWeakClearAll(XL_ObjectHeader* pHeader)
{
	XL_ObjectWeakRef* pWeakRef;
	XL_ObjectWeakRef* pNext;

	if ( pHeader == NULL ) {
		return;
	}

	pWeakRef = pHeader->pWeakRefs;
	pHeader->pWeakRefs = NULL;
	while ( pWeakRef != NULL ) {
		pNext = pWeakRef->pNext;
		pWeakRef->pPrev = NULL;
		pWeakRef->pNext = NULL;
		pWeakRef->pValue = NULL;
		pWeakRef->pBlob = NULL;
		pWeakRef = pNext;
	}
}

static XL_ObjectHeader* __xlObjectPrepareHeader(void* pBlob, int bRevive)
{
	XL_ObjectHeader* pHeader;

	if ( pBlob == NULL ) {
		return NULL;
	}

	pHeader = __xlObjectHeader(pBlob);
	if ( pHeader->uMagic != XL_OBJECT_HEADER_MAGIC ) {
		memset(pHeader, 0, sizeof(*pHeader));
		pHeader->uMagic = XL_OBJECT_HEADER_MAGIC;
	} else if ( __xlObjectHeaderDestroyedRaw(pHeader) ) {
		if ( !bRevive ) {
			return NULL;
		}
		memset(pHeader, 0, sizeof(*pHeader));
		pHeader->uMagic = XL_OBJECT_HEADER_MAGIC;
	}
	pHeader->uFlags &= ~(XL_OBJECT_FLAG_DESTROYING | XL_OBJECT_FLAG_DESTROYED);
	return pHeader;
}

static xvalue __xlObjectEnsureDynamicFields(void* pBlob, int bRevive)
{
	XL_ObjectHeader* pHeader;

	pHeader = __xlObjectPrepareHeader(pBlob, bRevive);
	if ( pHeader == NULL ) {
		return NULL;
	}
	if ( pHeader->dynamicFields == NULL ) {
		pHeader->dynamicFields = xvoCreateTable();
	}
	return pHeader->dynamicFields;
}

int xl_object_header_ready(const void* pBlob)
{
	const XL_ObjectHeader* pHeader;

	if ( pBlob == NULL ) {
		return 0;
	}
	pHeader = __xlObjectHeaderConst(pBlob);
	return __xlObjectHeaderAliveRaw(pHeader) ? 1 : 0;
}

int xl_object_header_alive(const void* pBlob)
{
	const XL_ObjectHeader* pHeader;

	if ( pBlob == NULL ) {
		return 0;
	}
	pHeader = __xlObjectHeaderConst(pBlob);
	return __xlObjectHeaderAliveRaw(pHeader) ? 1 : 0;
}

int xl_object_header_destroying(const void* pBlob)
{
	const XL_ObjectHeader* pHeader;

	if ( pBlob == NULL ) {
		return 0;
	}
	pHeader = __xlObjectHeaderConst(pBlob);
	return __xlObjectHeaderDestroyingRaw(pHeader) ? 1 : 0;
}

int xl_object_header_destroyed(const void* pBlob)
{
	const XL_ObjectHeader* pHeader;

	if ( pBlob == NULL ) {
		return 0;
	}
	pHeader = __xlObjectHeaderConst(pBlob);
	return __xlObjectHeaderDestroyedRaw(pHeader) ? 1 : 0;
}

void xl_object_header_init(void* pBlob)
{
	(void)__xlObjectEnsureDynamicFields(pBlob, 1);
}

void xl_object_header_bind(void* pBlob, const XL_ObjectTypeInfo* pTypeInfo)
{
	XL_ObjectHeader* pHeader;

	if ( pBlob == NULL ) {
		return;
	}

	pHeader = __xlObjectPrepareHeader(pBlob, 1);
	if ( pHeader == NULL ) {
		return;
	}
	pHeader->pTypeInfo = pTypeInfo;
}

void xl_object_header_clone(void* pDstBlob, const void* pSrcBlob, const XL_ObjectTypeInfo* pFallbackTypeInfo)
{
	XL_ObjectHeader* pDst;
	const XL_ObjectHeader* pSrc;

	if ( pDstBlob == NULL ) {
		return;
	}

	pDst = __xlObjectHeader(pDstBlob);
	memset(pDst, 0, sizeof(*pDst));

	pSrc = __xlObjectHeaderConst(pSrcBlob);
	if ( __xlObjectHeaderAliveRaw(pSrc) ) {
		pDst->uMagic = XL_OBJECT_HEADER_MAGIC;
		pDst->uFlags = pSrc->uFlags & ~(XL_OBJECT_FLAG_DESTROYING | XL_OBJECT_FLAG_DESTROYED);
		pDst->pTypeInfo = (pSrc->pTypeInfo != NULL) ? pSrc->pTypeInfo : pFallbackTypeInfo;
		pDst->pNativePayload = pSrc->pNativePayload;
		pDst->dynamicFields = (pSrc->dynamicFields != NULL) ? xvoCopy(pSrc->dynamicFields) : NULL;
		return;
	}

	if ( pFallbackTypeInfo != NULL ) {
		pDst->uMagic = XL_OBJECT_HEADER_MAGIC;
		pDst->pTypeInfo = pFallbackTypeInfo;
	}
}

void xl_object_header_dispose(void* pBlob)
{
	XL_ObjectHeader* pHeader;

	if ( pBlob == NULL ) {
		return;
	}

	pHeader = __xlObjectHeader(pBlob);
	if ( pHeader->uMagic != XL_OBJECT_HEADER_MAGIC ) {
		memset(pHeader, 0, sizeof(*pHeader));
		return;
	}
	if ( (pHeader->uFlags & XL_OBJECT_FLAG_DESTROYED) != 0 ) {
		return;
	}

	pHeader->uFlags |= XL_OBJECT_FLAG_DESTROYING;
	__xlObjectWeakClearAll(pHeader);
	__xlObjectRegistryRemove(pHeader);
	if ( pHeader->dynamicFields != NULL ) {
		xvoUnref(pHeader->dynamicFields);
		pHeader->dynamicFields = NULL;
	}
	pHeader->pTypeInfo = NULL;
	pHeader->pNativePayload = NULL;
	pHeader->pOwner = NULL;
	pHeader->uFlags &= ~XL_OBJECT_FLAG_DESTROYING;
	pHeader->uFlags |= XL_OBJECT_FLAG_DESTROYED;
}

const XL_ObjectTypeInfo* xl_object_type(const void* pBlob)
{
	const XL_ObjectHeader* pHeader;

	if ( pBlob == NULL ) {
		return NULL;
	}

	pHeader = __xlObjectHeaderConst(pBlob);
	if ( !__xlObjectHeaderAliveRaw(pHeader) ) {
		return NULL;
	}
	return pHeader->pTypeInfo;
}

void* xl_object_payload(const void* pBlob)
{
	const XL_ObjectHeader* pHeader;

	if ( pBlob == NULL ) {
		return NULL;
	}

	pHeader = __xlObjectHeaderConst(pBlob);
	if ( !__xlObjectHeaderAliveRaw(pHeader) ) {
		return NULL;
	}
	return pHeader->pNativePayload;
}

int xl_object_payload_set(void* pBlob, void* pPayload)
{
	XL_ObjectHeader* pHeader;

	pHeader = __xlObjectPrepareHeader(pBlob, 1);
	if ( pHeader == NULL ) {
		return 0;
	}
	pHeader->pNativePayload = pPayload;
	return 1;
}

void* xl_object_payload_take(void* pBlob)
{
	XL_ObjectHeader* pHeader;
	void* pPayload;

	if ( pBlob == NULL ) {
		return NULL;
	}
	pHeader = __xlObjectHeader(pBlob);
	if ( !__xlObjectHeaderAliveRaw(pHeader) ) {
		return NULL;
	}
	pPayload = pHeader->pNativePayload;
	pHeader->pNativePayload = NULL;
	return pPayload;
}

void* xl_object_dispatch(void* pBlob, const char* sDispatchKey)
{
	const XL_ObjectTypeInfo* pTypeInfo;
	const XL_ObjectHeader* pHeader;

	if ( (pBlob == NULL) || (sDispatchKey == NULL) ) {
		return NULL;
	}

	pHeader = __xlObjectHeaderConst(pBlob);
	if ( __xlObjectHeaderDestroyingRaw(pHeader) || __xlObjectHeaderDestroyedRaw(pHeader) ) {
		return NULL;
	}

	pTypeInfo = xl_object_type(pBlob);
	while ( pTypeInfo != NULL ) {
		void* pFunc = __xlObjectDispatchInType(pTypeInfo, sDispatchKey);

		if ( pFunc != NULL ) {
			return pFunc;
		}
		pTypeInfo = pTypeInfo->pBaseType;
	}
	return NULL;
}

int xl_object_delete_value(xvalue value)
{
	void* pBlob;
	const XL_ObjectTypeInfo* pTypeInfo;

	if ( value == NULL ) {
		return 1;
	}
	if ( value->Type != XVO_DT_CLASS ) {
		return 1;
	}

	pBlob = xvoGetClass(value);
	if ( pBlob == NULL ) {
		return 1;
	}
	if ( xl_object_header_destroying(pBlob) ) {
		return 1;
	}
	pTypeInfo = xl_object_type(pBlob);
	if ( pTypeInfo == NULL ) {
		return 1;
	}
	if ( value->IsStatic == FALSE && value->RefCount > 1u ) {
		xlrt_native_error_raise("ValueError", "delete requires unique object reference", 0, NULL);
		return 0;
	}
	if ( !xl_object_header_destroyed(pBlob) && (pTypeInfo->pDestroyFunc != NULL) ) {
		pTypeInfo->pDestroyFunc(pBlob);
	} else if ( !xl_object_header_destroyed(pBlob) ) {
		xl_object_header_dispose(pBlob);
	}
	memset(pBlob, 0, value->Size);
	return 1;
}

void xl_object_destroy_if_last(xvalue value)
{
	void* pBlob;
	const XL_ObjectTypeInfo* pTypeInfo;

	if ( value == NULL ) {
		return;
	}

	if ( value->Type == XVO_DT_CLASS ) {
		pBlob = xvoGetClass(value);
		if ( (pBlob != NULL) && xl_object_header_destroying(pBlob) ) {
			return;
		}
		pTypeInfo = xl_object_type(pBlob);
		if ( (pBlob != NULL) &&
			(pTypeInfo != NULL) &&
			(pTypeInfo->pDestroyFunc != NULL) &&
			(value->IsStatic == FALSE) &&
			(value->RefCount <= 1u) &&
			!xl_object_header_destroyed(pBlob) ) {
			pTypeInfo->pDestroyFunc(pBlob);
		} else if ( (pBlob != NULL) &&
			(value->IsStatic == FALSE) &&
			(value->RefCount <= 1u) &&
			!xl_object_header_destroyed(pBlob) ) {
			xl_object_header_dispose(pBlob);
		}
	}
}

xvalue xl_object_bind_value(xvalue value)
{
	void* pBlob;
	XL_ObjectHeader* pHeader;

	if ( (value == NULL) || (value->Type != XVO_DT_CLASS) ) {
		return value;
	}
	pBlob = xvoGetClass(value);
	if ( pBlob == NULL ) {
		return value;
	}
	pHeader = __xlObjectHeader(pBlob);
	if ( !__xlObjectHeaderAliveRaw(pHeader) ) {
		return value;
	}
	pHeader->pOwner = value;
	__xlObjectRegistryInsert(pHeader);
	return value;
}

xvalue xl_object_retain(xvalue value)
{
	(void)xl_object_bind_value(value);
	if ( (value != NULL) && (value->IsStatic == FALSE) ) {
		xvoAddRef_Inline(value);
	}
	return value;
}

uint32_t xl_object_refcount(xvalue value)
{
	(void)xl_object_bind_value(value);
	if ( value == NULL ) {
		return 0u;
	}
	return value->RefCount;
}

void xl_object_release(xvalue value)
{
	(void)xl_object_bind_value(value);
	if ( value == NULL ) {
		return;
	}
	xl_object_destroy_if_last(value);
	xvoUnref(value);
}

XL_ObjectWeakRef* xl_object_weak_new(xvalue value)
{
	XL_ObjectWeakRef* pWeakRef;
	XL_ObjectHeader* pHeader;
	void* pBlob;

	if ( (value == NULL) || (value->Type != XVO_DT_CLASS) ) {
		return NULL;
	}
	(void)xl_object_bind_value(value);

	pBlob = xvoGetClass(value);
	if ( pBlob == NULL ) {
		return NULL;
	}
	pHeader = __xlObjectHeader(pBlob);
	if ( !__xlObjectHeaderAliveRaw(pHeader) ) {
		return NULL;
	}

	pWeakRef = (XL_ObjectWeakRef*)calloc(1u, sizeof(*pWeakRef));
	if ( pWeakRef == NULL ) {
		return NULL;
	}
	pWeakRef->pValue = value;
	pWeakRef->pBlob = pBlob;
	pWeakRef->pNext = pHeader->pWeakRefs;
	if ( pHeader->pWeakRefs != NULL ) {
		pHeader->pWeakRefs->pPrev = pWeakRef;
	}
	pHeader->pWeakRefs = pWeakRef;
	return pWeakRef;
}

void xl_object_weak_delete(XL_ObjectWeakRef* pWeakRef)
{
	if ( pWeakRef == NULL ) {
		return;
	}
	__xlObjectWeakDetach(pWeakRef);
	free(pWeakRef);
}

int xl_object_weak_alive(const XL_ObjectWeakRef* pWeakRef)
{
	if ( (pWeakRef == NULL) || (pWeakRef->pValue == NULL) || (pWeakRef->pBlob == NULL) ) {
		return 0;
	}
	return xl_object_header_alive(pWeakRef->pBlob);
}

xvalue xl_object_weak_lock(XL_ObjectWeakRef* pWeakRef)
{
	if ( !xl_object_weak_alive(pWeakRef) ) {
		return NULL;
	}
	return xl_object_retain(pWeakRef->pValue);
}

XL_ObjectPinnedRef* xl_object_pin_value(xvalue value)
{
	XL_ObjectPinnedRef* pPinnedRef;
	xmutex pLock;

	if ( value == NULL ) {
		return NULL;
	}
	pPinnedRef = (XL_ObjectPinnedRef*)calloc(1u, sizeof(*pPinnedRef));
	if ( pPinnedRef == NULL ) {
		return NULL;
	}
	pPinnedRef->value = xl_object_retain(value);
	if ( pPinnedRef->value == NULL ) {
		free(pPinnedRef);
		return NULL;
	}
	pLock = __xlObjectPinnedRootLock();
	if ( pLock == NULL ) {
		xl_object_release(pPinnedRef->value);
		free(pPinnedRef);
		return NULL;
	}
	xrtMutexLock(pLock);
	pPinnedRef->pPrev = NULL;
	pPinnedRef->pNext = g_pXlPinnedRootHead;
	if ( g_pXlPinnedRootHead != NULL ) {
		g_pXlPinnedRootHead->pPrev = pPinnedRef;
	}
	g_pXlPinnedRootHead = pPinnedRef;
	xrtMutexUnlock(pLock);
	return pPinnedRef;
}

void xl_object_unpin_value(XL_ObjectPinnedRef* pPinnedRef)
{
	xvalue value;
	xmutex pLock;

	if ( pPinnedRef == NULL ) {
		return;
	}
	pLock = __xlObjectPinnedRootLock();
	if ( pLock != NULL ) {
		xrtMutexLock(pLock);
		if ( pPinnedRef->pPrev != NULL ) {
			pPinnedRef->pPrev->pNext = pPinnedRef->pNext;
		} else if ( g_pXlPinnedRootHead == pPinnedRef ) {
			g_pXlPinnedRootHead = pPinnedRef->pNext;
		}
		if ( pPinnedRef->pNext != NULL ) {
			pPinnedRef->pNext->pPrev = pPinnedRef->pPrev;
		}
		xrtMutexUnlock(pLock);
	}
	value = pPinnedRef->value;
	pPinnedRef->pPrev = NULL;
	pPinnedRef->pNext = NULL;
	pPinnedRef->value = NULL;
	if ( value != NULL ) {
		xl_object_release(value);
	}
	free(pPinnedRef);
}

xvalue xl_object_dynamic_fields(void* pBlob)
{
	const XL_ObjectHeader* pHeader;

	if ( pBlob == NULL ) {
		return NULL;
	}
	pHeader = __xlObjectHeaderConst(pBlob);
	if ( __xlObjectHeaderDestroyedRaw(pHeader) ) {
		return NULL;
	}
	return __xlObjectEnsureDynamicFields(pBlob, 0);
}

int xl_object_length(void* pBlob, int iDeclaredFieldCount)
{
	XL_ObjectHeader* pHeader;
	int iDynamicCount;

	if ( iDeclaredFieldCount < 0 ) {
		iDeclaredFieldCount = 0;
	}
	if ( pBlob == NULL ) {
		return iDeclaredFieldCount;
	}

	pHeader = __xlObjectHeader(pBlob);
	if ( __xlObjectHeaderDestroyedRaw(pHeader) ) {
		return 0;
	}
	iDynamicCount = 0;
	if ( __xlObjectHeaderAliveRaw(pHeader) && (pHeader->dynamicFields != NULL) ) {
		iDynamicCount = xl_core_len_dict(pHeader->dynamicFields);
	}
	return iDeclaredFieldCount + iDynamicCount;
}

int xl_object_exists(void* pBlob, const char* sKey)
{
	const XL_ObjectHeader* pHeader;

	if ( pBlob == NULL ) {
		return 0;
	}

	pHeader = __xlObjectHeaderConst(pBlob);
	if ( !__xlObjectHeaderAliveRaw(pHeader) || (pHeader->dynamicFields == NULL) ) {
		return 0;
	}
	return xl_dict_has(pHeader->dynamicFields, sKey);
}

xvalue xl_object_get(void* pBlob, const char* sKey, xvalue defaultValue, int bHasDefault)
{
	const XL_ObjectHeader* pHeader;
	xvalue value;

	if ( pBlob == NULL ) {
		return bHasDefault ? defaultValue : xvoCreateNull();
	}

	pHeader = __xlObjectHeaderConst(pBlob);
	if ( !__xlObjectHeaderAliveRaw(pHeader) || (pHeader->dynamicFields == NULL) ) {
		return bHasDefault ? defaultValue : xvoCreateNull();
	}

	value = xvoTableGetValue(
		pHeader->dynamicFields,
		(str)((sKey != NULL) ? sKey : ""),
		(uint32)__xlObjectKeyLen(sKey)
	);
	if ( (value == NULL) || (xvoType(value) == XVO_DT_NULL) ) {
		return bHasDefault ? defaultValue : xvoCreateNull();
	}
	return value;
}

int xl_object_set(void* pBlob, const char* sKey, xvalue value)
{
	xvalue fields;

	fields = __xlObjectEnsureDynamicFields(pBlob, 0);
	if ( fields == NULL ) {
		return 0;
	}
	if ( value == NULL ) {
		value = xvoCreateNull();
	} else {
		(void)xl_object_bind_value(value);
	}
	{
		int ok = xvoTableSetValue(
			fields,
			(str)((sKey != NULL) ? sKey : ""),
			(uint32)__xlObjectKeyLen(sKey),
			value,
			FALSE
		) ? 1 : 0;
		return ok;
	}
}

int xl_object_remove(void* pBlob, const char* sKey)
{
	XL_ObjectHeader* pHeader;

	if ( pBlob == NULL ) {
		return 0;
	}

	pHeader = __xlObjectHeader(pBlob);
	if ( !__xlObjectHeaderAliveRaw(pHeader) || (pHeader->dynamicFields == NULL) ) {
		return 0;
	}
	return xl_dict_remove(pHeader->dynamicFields, sKey);
}

void xl_object_clear(void* pBlob)
{
	XL_ObjectHeader* pHeader;

	if ( pBlob == NULL ) {
		return;
	}

	pHeader = __xlObjectHeader(pBlob);
	if ( !__xlObjectHeaderAliveRaw(pHeader) || (pHeader->dynamicFields == NULL) ) {
		return;
	}
	xl_dict_clear(pHeader->dynamicFields);
}

xvalue xl_object_keys(void* pBlob)
{
	const XL_ObjectHeader* pHeader;

	if ( pBlob == NULL ) {
		return xvoCreateArray();
	}

	pHeader = __xlObjectHeaderConst(pBlob);
	if ( !__xlObjectHeaderAliveRaw(pHeader) || (pHeader->dynamicFields == NULL) ) {
		return xvoCreateArray();
	}
	return xl_iter_keys(pHeader->dynamicFields);
}

xvalue xl_object_values(void* pBlob)
{
	const XL_ObjectHeader* pHeader;

	if ( pBlob == NULL ) {
		return xvoCreateArray();
	}

	pHeader = __xlObjectHeaderConst(pBlob);
	if ( !__xlObjectHeaderAliveRaw(pHeader) || (pHeader->dynamicFields == NULL) ) {
		return xvoCreateArray();
	}
	return xl_iter_values(pHeader->dynamicFields);
}

xvalue xl_object_items(void* pBlob)
{
	const XL_ObjectHeader* pHeader;

	if ( pBlob == NULL ) {
		return xvoCreateArray();
	}

	pHeader = __xlObjectHeaderConst(pBlob);
	if ( !__xlObjectHeaderAliveRaw(pHeader) || (pHeader->dynamicFields == NULL) ) {
		return xvoCreateArray();
	}
	return xl_iter_items(pHeader->dynamicFields);
}

static const XL_ObjectTypeInfo* __xlObjectMetaTypeInfo(xvalue value, void** ppBlob)
{
	void* pBlob;
	const XL_ObjectHeader* pHeader;

	if ( ppBlob != NULL ) {
		*ppBlob = NULL;
	}
	if ( (value == NULL) || (value->Type != XVO_DT_CLASS) ) {
		return NULL;
	}
	pBlob = xvoGetClass(value);
	if ( pBlob == NULL ) {
		return NULL;
	}
	pHeader = __xlObjectHeaderConst(pBlob);
	if ( !__xlObjectHeaderAliveRaw(pHeader) || (pHeader->pTypeInfo == NULL) ) {
		return NULL;
	}
	if ( ppBlob != NULL ) {
		*ppBlob = pBlob;
	}
	return pHeader->pTypeInfo;
}

static int __xlObjectMetaBuiltInMethod(const char* sKey)
{
	if ( sKey == NULL ) {
		return 0;
	}
	return
		(strcmp(sKey, "set") == 0) ||
		(strcmp(sKey, "get") == 0) ||
		(strcmp(sKey, "exists") == 0) ||
		(strcmp(sKey, "remove") == 0) ||
		(strcmp(sKey, "clear") == 0) ||
		(strcmp(sKey, "keys") == 0) ||
		(strcmp(sKey, "values") == 0) ||
		(strcmp(sKey, "items") == 0) ||
		(strcmp(sKey, "clone") == 0);
}

int xl_object_meta_has_field(xvalue value, const char* sKey)
{
	void* pBlob;
	const XL_ObjectTypeInfo* pTypeInfo;

	if ( sKey == NULL ) {
		return 0;
	}
	pTypeInfo = __xlObjectMetaTypeInfo(value, &pBlob);
	if ( (pTypeInfo == NULL) || (pTypeInfo->pVisibleExists == NULL) ) {
		return 0;
	}
	return pTypeInfo->pVisibleExists(pBlob, sKey);
}

int xl_object_meta_has_method(xvalue value, const char* sKey)
{
	const XL_ObjectTypeInfo* pTypeInfo;

	if ( sKey == NULL ) {
		return 0;
	}
	pTypeInfo = __xlObjectMetaTypeInfo(value, NULL);
	if ( (pTypeInfo != NULL) && __xlObjectMetaBuiltInMethod(sKey) ) {
		return 1;
	}
	while ( pTypeInfo != NULL ) {
		size_t i;

		for ( i = 0; i < pTypeInfo->iMethodCount; ++i ) {
			const XL_ObjectMethodEntry* pEntry = &pTypeInfo->pMethods[i];
			const char* sDispatchKey;
			const char* sArity;

			if ( pEntry == NULL ) {
				continue;
			}
			sDispatchKey = pEntry->sDispatchKey;
			if ( (sDispatchKey == NULL) || (sDispatchKey[0] == '\0') ) {
				continue;
			}
			sArity = strstr(sDispatchKey, "$arity");
			if ( sArity == NULL ) {
				if ( strcmp(sDispatchKey, sKey) == 0 ) {
					return 1;
				}
				continue;
			}
			if ( ((size_t)(sArity - sDispatchKey) == strlen(sKey)) &&
				(strncmp(sDispatchKey, sKey, (size_t)(sArity - sDispatchKey)) == 0) ) {
				return 1;
			}
		}
		pTypeInfo = pTypeInfo->pBaseType;
	}
	return 0;
}

xvalue xl_object_meta_field_names(xvalue value)
{
	void* pBlob;
	const XL_ObjectTypeInfo* pTypeInfo;

	pTypeInfo = __xlObjectMetaTypeInfo(value, &pBlob);
	if ( (pTypeInfo == NULL) || (pTypeInfo->pVisibleKeys == NULL) ) {
		return xvoCreateArray();
	}
	return pTypeInfo->pVisibleKeys(pBlob);
}

int xl_object_meta_field_count(xvalue value)
{
	xvalue names = xl_object_meta_field_names(value);
	int iCount = 0;

	if ( names == NULL ) {
		return 0;
	}
	iCount = (int)xvoArrayItemCount(names);
	xvoUnref(names);
	return iCount;
}

const char* xl_object_meta_field_name_at(xvalue value, int index, const char* sDefaultValue)
{
	xvalue names = NULL;
	xvalue item = NULL;
	const char* sText = NULL;
	size_t iLen;
	char* sNewBuf;

	if ( sDefaultValue == NULL ) {
		sDefaultValue = "";
	}
	if ( index < 0 ) {
		return sDefaultValue;
	}
	names = xl_object_meta_field_names(value);
	if ( names == NULL ) {
		return sDefaultValue;
	}
	if ( (uint32)index >= xvoArrayItemCount(names) ) {
		xvoUnref(names);
		return sDefaultValue;
	}
	item = xvoArrayGetValue(names, (uint32)index);
	if ( (item == NULL) || !xvoIsText(item) ) {
		xvoUnref(names);
		return sDefaultValue;
	}
	sText = (const char*)xvoGetText(item);
	if ( sText == NULL ) {
		xvoUnref(names);
		return sDefaultValue;
	}
	iLen = strlen(sText);
	if ( g_iXlObjectMetaFieldNameScratchCap < (iLen + 1u) ) {
		sNewBuf = (char*)realloc(g_sXlObjectMetaFieldNameScratch, iLen + 1u);
		if ( sNewBuf == NULL ) {
			xvoUnref(names);
			return sDefaultValue;
		}
		g_sXlObjectMetaFieldNameScratch = sNewBuf;
		g_iXlObjectMetaFieldNameScratchCap = iLen + 1u;
	}
	memcpy(g_sXlObjectMetaFieldNameScratch, sText, iLen + 1u);
	xvoUnref(names);
	return g_sXlObjectMetaFieldNameScratch;
}

xvalue xl_object_meta_method_names(xvalue value)
{
	void* pBlob;
	const XL_ObjectTypeInfo* pTypeInfo;
	xvalue result;
	const XL_ObjectTypeInfo* pCursor;

	pTypeInfo = __xlObjectMetaTypeInfo(value, &pBlob);
	(void)pBlob;
	result = xvoCreateArray();
	if ( result == NULL ) {
		return NULL;
	}
	for ( pCursor = pTypeInfo; pCursor != NULL; pCursor = pCursor->pBaseType ) {
		size_t i;
		for ( i = 0u; i < pCursor->iMethodCount; ++i ) {
			const XL_ObjectMethodEntry* pEntry = &pCursor->pMethods[i];
			uint32 j;
			int bDuplicate = 0;
			xvalue item;
			size_t iNameLen;
			if ( (pEntry == NULL) || (pEntry->sDispatchKey == NULL) || (pEntry->sDispatchKey[0] == '\0') ) {
				continue;
			}
			iNameLen = __xlObjectMetaPublicMethodNameLen(pEntry->sDispatchKey);
			if ( iNameLen == 0u ) {
				continue;
			}
			for ( j = 0u; j < xvoArrayItemCount(result); ++j ) {
				xvalue existing = xvoArrayGetValue(result, j);
				const char* sExisting = (existing != NULL && xvoIsText(existing)) ? (const char*)xvoGetText(existing) : NULL;
				size_t iExistingLen = (existing != NULL && xvoIsText(existing)) ? (size_t)xvoGetSize(existing) : 0u;
				if ( (sExisting != NULL) &&
				     (iExistingLen == iNameLen) &&
				     (strncmp(sExisting, pEntry->sDispatchKey, iNameLen) == 0) ) {
					bDuplicate = 1;
					break;
				}
			}
			if ( bDuplicate ) {
				continue;
			}
			item = xvoCreateText((ptr)pEntry->sDispatchKey, (uint32)iNameLen, FALSE);
			if ( (item == NULL) || !xvoArrayAppendValue(result, item, TRUE) ) {
				if ( item != NULL ) {
					xvoUnref(item);
				}
				xvoUnref(result);
				return NULL;
			}
		}
	}
	return result;
}

int xl_object_meta_method_count(xvalue value)
{
	xvalue names = xl_object_meta_method_names(value);
	int iCount = 0;

	if ( names == NULL ) {
		return 0;
	}
	iCount = (int)xvoArrayItemCount(names);
	xvoUnref(names);
	return iCount;
}

const char* xl_object_meta_method_name_at(xvalue value, int index, const char* sDefaultValue)
{
	xvalue names = NULL;
	xvalue item = NULL;
	const char* sText = NULL;
	size_t iLen;
	char* sNewBuf;

	if ( sDefaultValue == NULL ) {
		sDefaultValue = "";
	}
	if ( index < 0 ) {
		return sDefaultValue;
	}
	names = xl_object_meta_method_names(value);
	if ( names == NULL ) {
		return sDefaultValue;
	}
	if ( (uint32)index >= xvoArrayItemCount(names) ) {
		xvoUnref(names);
		return sDefaultValue;
	}
	item = xvoArrayGetValue(names, (uint32)index);
	if ( (item == NULL) || !xvoIsText(item) ) {
		xvoUnref(names);
		return sDefaultValue;
	}
	sText = (const char*)xvoGetText(item);
	if ( sText == NULL ) {
		xvoUnref(names);
		return sDefaultValue;
	}
	iLen = (size_t)xvoGetSize(item);
	if ( g_iXlObjectMetaMethodNameScratchCap < (iLen + 1u) ) {
		sNewBuf = (char*)realloc(g_sXlObjectMetaMethodNameScratch, iLen + 1u);
		if ( sNewBuf == NULL ) {
			xvoUnref(names);
			return sDefaultValue;
		}
		g_sXlObjectMetaMethodNameScratch = sNewBuf;
		g_iXlObjectMetaMethodNameScratchCap = iLen + 1u;
	}
	memcpy(g_sXlObjectMetaMethodNameScratch, sText, iLen);
	g_sXlObjectMetaMethodNameScratch[iLen] = '\0';
	xvoUnref(names);
	return g_sXlObjectMetaMethodNameScratch;
}

int xl_object_collect_cycles(void)
{
	size_t iNodeCount;
	XL_ObjectCollectNode* arrNodes;
	uint32_t* arrIncoming;
	size_t* arrStack;
	size_t i;
	size_t iStackCount;
	int iCollectedCount;
	int bCollectReady;

	iNodeCount = __xlObjectRegistryCount();
	if ( iNodeCount == 0u ) {
		return 0;
	}

	arrNodes = (XL_ObjectCollectNode*)calloc(iNodeCount, sizeof(*arrNodes));
	arrIncoming = (uint32_t*)calloc(iNodeCount, sizeof(*arrIncoming));
	arrStack = (size_t*)calloc(iNodeCount, sizeof(*arrStack));
	if ( (arrNodes == NULL) || (arrIncoming == NULL) || (arrStack == NULL) ) {
		free(arrNodes);
		free(arrIncoming);
		free(arrStack);
		return 0;
	}

	i = 0u;
	{
		XL_ObjectHeader* pHeader;

		for ( pHeader = g_pXlObjectRegistryHead; (pHeader != NULL) && (i < iNodeCount); pHeader = pHeader->pTrackNext ) {
			xvalue value = __xlObjectHeaderOwner(pHeader);

			if ( !__xlObjectHeaderAliveRaw(pHeader) || (value == NULL) ) {
				continue;
			}
			arrNodes[i].pHeader = pHeader;
			arrNodes[i].value = value;
			++i;
		}
	}
	iNodeCount = i;
	if ( iNodeCount == 0u ) {
		free(arrNodes);
		free(arrIncoming);
		free(arrStack);
		return 0;
	}

	bCollectReady = 1;
	for ( i = 0u; i < iNodeCount; ++i ) {
		if ( !__xlObjectCollectAccumulateEdges(arrNodes, iNodeCount, arrNodes[i].pHeader, arrIncoming) ) {
			bCollectReady = 0;
			break;
		}
	}
	if ( !bCollectReady ) {
		free(arrNodes);
		free(arrIncoming);
		free(arrStack);
		return 0;
	}
	for ( i = 0u; i < iNodeCount; ++i ) {
		uint32_t uRefCount = xl_object_refcount(arrNodes[i].value);

		arrNodes[i].uIncomingCount = arrIncoming[i];
		arrNodes[i].uExternalCount = (uRefCount > arrIncoming[i]) ? (uRefCount - arrIncoming[i]) : 0u;
	}

	iStackCount = 0u;
	for ( i = 0u; i < iNodeCount; ++i ) {
		if ( arrNodes[i].uExternalCount > 0u ) {
			arrNodes[i].bQueued = 1u;
			arrStack[iStackCount++] = i;
		}
	}
	if ( !__xlObjectCollectPushFutureRoots(arrNodes, iNodeCount, arrStack, &iStackCount) ) {
		free(arrNodes);
		free(arrIncoming);
		free(arrStack);
		return 0;
	}
	if ( !__xlObjectCollectPushPinnedRoots(arrNodes, iNodeCount, arrStack, &iStackCount) ) {
		free(arrNodes);
		free(arrIncoming);
		free(arrStack);
		return 0;
	}
	while ( iStackCount > 0u ) {
		size_t iNodeIndex = arrStack[--iStackCount];

		arrNodes[iNodeIndex].bQueued = 0u;
		if ( arrNodes[iNodeIndex].bReachable ) {
			continue;
		}
		arrNodes[iNodeIndex].bReachable = 1u;
		if ( !__xlObjectCollectPushEdges(arrNodes, iNodeCount, arrNodes[iNodeIndex].pHeader, arrStack, &iStackCount) ) {
			bCollectReady = 0;
			break;
		}
	}
	if ( !bCollectReady ) {
		free(arrNodes);
		free(arrIncoming);
		free(arrStack);
		return 0;
	}

	iCollectedCount = 0;
	for ( i = 0u; i < iNodeCount; ++i ) {
		if ( !arrNodes[i].bReachable ) {
			arrNodes[i].bCollect = 1u;
			(void)xl_object_retain(arrNodes[i].value);
			++iCollectedCount;
		}
	}
	for ( i = 0u; i < iNodeCount; ++i ) {
		if ( !arrNodes[i].bCollect ) {
			continue;
		}
		if ( !xl_object_header_destroyed(arrNodes[i].pHeader) ) {
			const XL_ObjectTypeInfo* pTypeInfo = xl_object_type(arrNodes[i].pHeader);

			if ( (pTypeInfo != NULL) && (pTypeInfo->pDestroyFunc != NULL) ) {
				pTypeInfo->pDestroyFunc(arrNodes[i].pHeader);
			} else {
				xl_object_header_dispose(arrNodes[i].pHeader);
			}
		}
		memset(arrNodes[i].pHeader, 0, arrNodes[i].value->Size);
	}
	for ( i = 0u; i < iNodeCount; ++i ) {
		if ( arrNodes[i].bCollect ) {
			xl_object_release(arrNodes[i].value);
		}
	}

	free(arrNodes);
	free(arrIncoming);
	free(arrStack);
	return iCollectedCount;
}
