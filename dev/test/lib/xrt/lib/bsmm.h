


// 创建数据块结构内存管理器
XXAPI xbsmm xrtBsmmCreate(uint32 iItemLength)
{
	xbsmm objBSMM = xrtMalloc(sizeof(xbsmm_struct));
	if ( objBSMM ) {
		xrtBsmmInit(objBSMM, iItemLength);
	}
	return objBSMM;
}

// 销毁数据块结构内存管理器
XXAPI void xrtBsmmDestroy(xbsmm objBSMM)
{
	if ( objBSMM ) {
		xrtBsmmUnit(objBSMM);
		xrtFree(objBSMM);
	}
}

// 初始化数据块结构内存管理器（对自维护结构体指针使用，和 BSMM_Create 功能类似）
XXAPI void xrtBsmmInit(xbsmm objBSMM, uint32 iItemLength)
{
	objBSMM->ItemLength = iItemLength;
	objBSMM->Count = 0;
	xrtPtrArrayInit(&objBSMM->PageMMU);
	objBSMM->LL_Free = NULL;
}

// 释放数据块结构内存管理器（对自维护结构体指针使用，和 BSMM_Destroy 功能类似）
XXAPI void xrtBsmmUnit(xbsmm objBSMM)
{
	objBSMM->Count = 0;
	// 循环释放 PageMMU 中的内存页
	for ( int i = 0; i < objBSMM->PageMMU.Count; i++ ) {
		xrtFree(objBSMM->PageMMU.Memory[i]);
	}
	xrtPtrArrayUnit(&objBSMM->PageMMU);
	// 循环释放空闲内存块链表
	MemPtr_LLNode* pNode = objBSMM->LL_Free;
	while ( pNode ) {
		MemPtr_LLNode* pNext = pNode->Next;
		xrtFree(pNode);
		pNode = pNext;
	}
	objBSMM->LL_Free = NULL;
}

// 申请结构体内存
XXAPI ptr xrtBsmmAlloc(xbsmm objBSMM)
{
	if ( objBSMM->LL_Free ) {
		// 有空闲内存块先用空闲的
		ptr Ptr = objBSMM->LL_Free->Ptr;
		MemPtr_LLNode* pNext = objBSMM->LL_Free->Next;
		xrtFree(objBSMM->LL_Free);
		objBSMM->LL_Free = pNext;
		return Ptr;
	} else {
		// 需要申请新的内存块
		if ( objBSMM->Count >= (objBSMM->PageMMU.Count * 256) ) {
			ptr pBlock = xrtMalloc(objBSMM->ItemLength * 256);
			if ( pBlock == NULL ) {
				return NULL;
			}
			// 向
			uint iIdx = xrtPtrArrayAppend(&objBSMM->PageMMU, pBlock);
			if ( iIdx == 0 ) {
				xrtFree(pBlock);
				return NULL;
			}
		}
		// 从内存块中分配值
		uint32 iBlock = objBSMM->Count >> 8;
		uint32 iPos = objBSMM->Count & 0xFF;
		str pBlock = xrtPtrArrayGet_Inline(&objBSMM->PageMMU, iBlock + 1);
		objBSMM->Count++;
		return &pBlock[iPos * objBSMM->ItemLength];
	}
}

// 释放结构体内存
XXAPI void xrtBsmmFree(xbsmm objBSMM, ptr p)
{
	MemPtr_LLNode* pNode = xrtMalloc(sizeof(MemPtr_LLNode));
	pNode->Ptr = p;
	pNode->Next = objBSMM->LL_Free;
	objBSMM->LL_Free = pNode;
}


