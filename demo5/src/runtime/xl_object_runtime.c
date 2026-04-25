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

static XL_ObjectHeader* g_pXlObjectRegistryHead = NULL;

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
