


// 创建内存管理器
XXAPI xfsmempool xrtFSMemPoolCreate(unsigned int iItemLength)
{
	xfsmempool mm = xrtMalloc(sizeof(xfsmempool_struct));
	if ( mm ) {
		xrtFSMemPoolInit(mm, iItemLength);
	}
	return mm;
}

// 销毁内存管理器
XXAPI void xrtFSMemPoolDestroy(xfsmempool objMM)
{
	if ( objMM ) {
		xrtFSMemPoolUnit(objMM);
		xrtFree(objMM);
	}
}

// 初始化内存管理器（对自维护结构体指针使用）
XXAPI void xrtFSMemPoolInit(xfsmempool objMM, unsigned int iItemLength)
{
	objMM->ItemLength = iItemLength;
	xrtBsmmInit(&objMM->arrMMU, sizeof(MMU_LLNode));
	objMM->arrMMU.PageMMU.AllocStep = 64;
	objMM->LL_Idle = NULL;
	objMM->LL_Full = NULL;
	objMM->LL_Null = NULL;
	objMM->LL_Free = NULL;
}

// 释放内存管理器（对自维护结构体指针使用）
XXAPI void xrtFSMemPoolUnit(xfsmempool objMM)
{
	for ( int i = 0; i < objMM->arrMMU.Count; i++ ) {
		MMU_LLNode* pNode = xrtBsmmGetPtr_Inline(&objMM->arrMMU, i);
		if ( pNode->objMMU ) {
			xrtMemUnitDestroy(pNode->objMMU);
		}
	}
	xrtBsmmUnit(&objMM->arrMMU);
	objMM->LL_Idle = NULL;
	objMM->LL_Full = NULL;
	objMM->LL_Null = NULL;
	objMM->LL_Free = NULL;
}

// 从内存管理器中申请一块内存
XXAPI ptr xrtFSMemPoolAlloc(xfsmempool objMM)
{
	xmemunit objMMU = NULL;
	if ( objMM->LL_Idle == NULL ) {
		// 如果没有空闲的内存管理单元，优先使用备用的全空单元，或创建一个新的单元
		if ( objMM->LL_Null ) {
			// 使用备用的全空内存管理单元
			objMMU = objMM->LL_Null->objMMU;
			objMM->LL_Idle = objMM->LL_Null;
			objMM->LL_Null = NULL;
		} else if ( objMM->LL_Free ) {
			// 创建新的内存管理单元，使用已释放的内存管理单元位置
			objMMU = xrtMemUnitCreate(objMM->ItemLength);
			if ( objMMU == NULL ) {
				return NULL;
			}
			// 恢复Flag，写入新申请的单元
			MMU_LLNode* pNode = objMM->LL_Free;
			objMMU->Flag = pNode->Flag;
			pNode->objMMU = objMMU;
			// 从 LL_Free 中移除
			if ( pNode->Next ) {
				pNode->Next->Prev = NULL;
			}
			objMM->LL_Free = pNode->Next;
			// 添加到 LL_Idle
			pNode->Prev = NULL;
			pNode->Next = NULL;
			objMM->LL_Idle = pNode;
		} else {
			// 创建新的内存管理单元，创建失败就报错处理
			objMMU = xrtMemUnitCreate(objMM->ItemLength);
			if ( objMMU == NULL ) {
				return NULL;
			}
			// 将创建好的内存管理单元添加到单元阵列管理器，添加失败就报错处理
			MMU_LLNode* pNode = xrtBsmmAlloc(&objMM->arrMMU);
			if ( pNode ) {
				pNode->objMMU = objMMU;
				pNode->Prev = NULL;
				pNode->Next = NULL;
				pNode->Flag = MMU_FLAG_USE | ((objMM->arrMMU.Count - 1) << 8);
				objMM->LL_Idle = pNode;
				// 标记内存管理器单元的 Flag
				objMMU->Flag = pNode->Flag;
			} else {
				xrtMemUnitDestroy(objMMU);
				xrtSetError("Fixed-Size Memory Pool : add memory unit failed.", FALSE);
				return NULL;
			}
		}
	} else {
		// 有空闲的内存管理单元，优先使用空闲的
		objMMU = objMM->LL_Idle->objMMU;
		// 如果空闲的内存管理单元即将满了，将它转移到满载单元链表
		if ( objMMU->Count >= 255 ) {
			MMU_LLNode* pNode = objMM->LL_Idle;
			// 从 LL_Idle 中移除
			if ( pNode->Next ) {
				pNode->Next->Prev = NULL;
			}
			objMM->LL_Idle = pNode->Next;
			// 添加到 LL_Full
			pNode->Prev = NULL;
			pNode->Next = objMM->LL_Full;
			if ( objMM->LL_Full ) {
				objMM->LL_Full->Prev = pNode;
			}
			objMM->LL_Full = pNode;
		}
	}
	// 从选定内存管理器单元中申请内存块
	return xrtMemUnitAlloc_Inline(objMMU);
}

// 将内存管理器申请的内存释放掉
static inline void MM256_LLNode_ClearCheck(xfsmempool objMM, MMU_LLNode* pNode, bool bLL_Full)
{
	// 如果这个内存管理单元已经清空
	if ( pNode->objMMU->Count == 0 ) {
		if ( objMM->LL_Null ) {
			// 有备用单元时，直接释放掉这个单元
			xrtMemUnitDestroy(pNode->objMMU);
			pNode->objMMU = NULL;
			// 从 LL_Idle 或 LL_Full 中移除
			if ( pNode->Prev ) {
				pNode->Prev->Next = pNode->Next;
			} else {
				if ( bLL_Full ) {
					objMM->LL_Full = pNode->Next;
				} else {
					objMM->LL_Idle = pNode->Next;
				}
			}
			if ( pNode->Next ) {
				pNode->Next->Prev = pNode->Prev;
			}
			// 添加到 LL_Free
			pNode->Prev = NULL;
			pNode->Next = objMM->LL_Free;
			if ( objMM->LL_Free ) {
				objMM->LL_Free->Prev = pNode;
			}
			objMM->LL_Free = pNode;
		} else {
			// 没有备用单元时，让这个单元备用，避免临界状态反复申请和释放内存管理单元，造成性能损失
			// 从 LL_Idle 或 LL_Full 中移除
			if ( pNode->Prev ) {
				pNode->Prev->Next = pNode->Next;
			} else {
				if ( bLL_Full ) {
					objMM->LL_Full = pNode->Next;
				} else {
					objMM->LL_Idle = pNode->Next;
				}
			}
			if ( pNode->Next ) {
				pNode->Next->Prev = pNode->Prev;
			}
			// 添加到 LL_Null
			objMM->LL_Null = pNode;
			pNode->Prev = NULL;
			pNode->Next = NULL;
		}
	}
}
static inline void MM256_LLNode_IdleCheck(xfsmempool objMM, MMU_LLNode* pNode)
{
	if ( pNode->objMMU->Count < 256 ) {
		// 从 LL_Full 中移除
		if ( pNode->Prev ) {
			pNode->Prev->Next = pNode->Next;
		} else {
			objMM->LL_Full = pNode->Next;
		}
		if ( pNode->Next ) {
			pNode->Next->Prev = pNode->Prev;
		}
		// 添加到 LL_Idle
		pNode->Prev = NULL;
		pNode->Next = objMM->LL_Idle;
		if ( objMM->LL_Idle ) {
			objMM->LL_Idle->Prev = pNode;
		}
		objMM->LL_Idle = pNode;
	}
}
XXAPI void xrtFSMemPoolFree(xfsmempool objMM, ptr p)
{
	MMU_ValuePtr v = p - sizeof(MMU_Value);
	if ( v->ItemFlag & MMU_FLAG_USE ) {
		int iMMU = (v->ItemFlag & MMU_FLAG_MASK) >> 8;
		uint8 idx = v->ItemFlag & 0xFF;
		// 获取对应的内存管理器单元链表结构
		MMU_LLNode* pNode = xrtBsmmGetPtr_Inline(&objMM->arrMMU, iMMU);
		if ( pNode->objMMU == NULL ) {
			xrtSetError("Fixed-Size Memory Pool : MMU cannot be null.", FALSE);
			return;
		}
		// 调用对应 MMU 的释放函数
		xrtMemUnitFreeIdx_Inline(pNode->objMMU, idx);
		v->ItemFlag = 0;
		// 如果是一个满载的内存管理器单元，将它放入空闲单元列表
		if ( pNode->objMMU->Count >= 255 ) {
			MM256_LLNode_IdleCheck(objMM, pNode);
		}
		// 如果这个内存管理单元已经清空，将他释放或变为备用单元
		MM256_LLNode_ClearCheck(objMM, pNode, 0);
	}
}

// 进行一轮GC，将未标记为使用中的内存全部回收
XXAPI void xrtFSMemPoolGC(xfsmempool objMM, bool bFreeMark)
{
	// 遍历所有 空闲的 和 满载的 内存管理单元，进行标记回收
	MMU_LLNode* pNode = objMM->LL_Idle;
	while ( pNode ) {
		xrtMemUnitGC(pNode->objMMU, bFreeMark);
		pNode = pNode->Next;
	}
	pNode = objMM->LL_Full;
	while ( pNode ) {
		xrtMemUnitGC(pNode->objMMU, bFreeMark);
		pNode = pNode->Next;
	}
	// 再次遍历所有 空闲的 和 满载的 内存管理单元，将他们归类到正确的分组
	pNode = objMM->LL_Idle;
	while ( pNode ) {
		MMU_LLNode* pNext = pNode->Next;
		MM256_LLNode_ClearCheck(objMM, pNode, 0);
		pNode = pNext;
	}
	pNode = objMM->LL_Full;
	while ( pNode ) {
		MMU_LLNode* pNext = pNode->Next;
		if ( pNode->objMMU->Count == 0 ) {
			MM256_LLNode_ClearCheck(objMM, pNode, -1);
		} else {
			MM256_LLNode_IdleCheck(objMM, pNode);
		}
		pNode = pNext;
	}
}


