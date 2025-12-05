


// 创建内存池
XXAPI xmempool xrtMemPoolCreate(int iCustom)
{
	xmempool objMP = xrtMalloc(sizeof(xmempool_struct));
	if ( objMP ) {
		xrtMemPoolInit(objMP, iCustom);
	}
	return objMP;
}

// 销毁内存池
XXAPI void xrtMemPoolDestroy(xmempool objMP)
{
	if ( objMP ) {
		xrtMemPoolUnit(objMP);
		xrtFree(objMP);
	}
}

// 初始化内存池（对自维护结构体指针使用，和 MP256_Create 功能类似）
void MP256_SetFSB(FSB_Item* FSB, int idx, uint32 iSizeMin, uint32 iSizeMax, FSB_Item* left, FSB_Item* right)
{
	FSB[idx].MinLength = iSizeMin;
	FSB[idx].MaxLength = iSizeMax;
	FSB[idx].LL_Idle = NULL;
	FSB[idx].LL_Full = NULL;
	FSB[idx].LL_Null = NULL;
	FSB[idx].LL_Free = NULL;
	FSB[idx].left = left;
	FSB[idx].right = right;
}
XXAPI void xrtMemPoolInit(xmempool objMP, int iCustom)
{
	xrtBsmmInit(&objMP->arrMMU, sizeof(MMU_LLNode));
	xrtBsmmInit(&objMP->BigMM, sizeof(MP_BigInfoLL));
	objMP->LL_BigFree = NULL;
	if ( iCustom == 1 ) {
		// 添加默认的区块区间 (4层树，针对小内存的方案)
		//
		// 二叉树视图 (根据建树顺序插入避免旋转产生额外开销)：
		//								○
		//								160
		//				○								○
		//				64								320
		//		○				○				○				○
		//		32				96				224				448
		//	○		○		○		○		○		○		○		○
		//	16		48		80		128		192		256		384		512
		//
		objMP->FSB_Memory = xrtMalloc(sizeof(FSB_Item) * 15);
		MP256_SetFSB(objMP->FSB_Memory, 0,	1,		16, NULL, NULL);
		MP256_SetFSB(objMP->FSB_Memory, 1,	17,		32, &objMP->FSB_Memory[0], &objMP->FSB_Memory[2]);
		MP256_SetFSB(objMP->FSB_Memory, 2,	33,		48, NULL, NULL);
		MP256_SetFSB(objMP->FSB_Memory, 3,	49,		64, &objMP->FSB_Memory[1], &objMP->FSB_Memory[5]);
		MP256_SetFSB(objMP->FSB_Memory, 4,	65,		80, NULL, NULL);
		MP256_SetFSB(objMP->FSB_Memory, 5,	81,		96, &objMP->FSB_Memory[4], &objMP->FSB_Memory[6]);
		MP256_SetFSB(objMP->FSB_Memory, 6,	97,		128, NULL, NULL);
		MP256_SetFSB(objMP->FSB_Memory, 7,	129,	160, &objMP->FSB_Memory[3], &objMP->FSB_Memory[11]);
		MP256_SetFSB(objMP->FSB_Memory, 8,	161,	192, NULL, NULL);
		MP256_SetFSB(objMP->FSB_Memory, 9,	193,	224, &objMP->FSB_Memory[8], &objMP->FSB_Memory[10]);
		MP256_SetFSB(objMP->FSB_Memory, 10,	225,	256, NULL, NULL);
		MP256_SetFSB(objMP->FSB_Memory, 11,	257,	320, &objMP->FSB_Memory[9], &objMP->FSB_Memory[13]);
		MP256_SetFSB(objMP->FSB_Memory, 12,	321,	384, NULL, NULL);
		MP256_SetFSB(objMP->FSB_Memory, 13,	385,	448, &objMP->FSB_Memory[12], &objMP->FSB_Memory[14]);
		MP256_SetFSB(objMP->FSB_Memory, 14,	449,	512, NULL, NULL);
		objMP->FSB_RootNode = &objMP->FSB_Memory[7];
	} else if ( iCustom == 2 ) {
		// 添加默认的区块区间 (5层树，针对大内存的方案)
		//
		// 二叉树视图 (根据建树顺序插入避免旋转产生额外开销)：
		//																○
		//																640
		//								○																○
		//								160																2304
		//				○								○								○								○
		//				64								320								1280							3328
		//		○				○				○				○				○				○				○				○
		//		32				96				224				448				896				1792			2816			3840
		//	○		○		○		○		○		○		○		○		○		○		○		○		○		○		○		○
		//	16		48		80		128		192		256		384		512		768		1024	1536	2048	2560	3072	3584	4096
		//
		objMP->FSB_Memory = xrtMalloc(sizeof(FSB_Item) * 31);
		MP256_SetFSB(objMP->FSB_Memory, 0,	1,		16, NULL, NULL);
		MP256_SetFSB(objMP->FSB_Memory, 1,	17,		32, &objMP->FSB_Memory[0], &objMP->FSB_Memory[2]);
		MP256_SetFSB(objMP->FSB_Memory, 2,	33,		48, NULL, NULL);
		MP256_SetFSB(objMP->FSB_Memory, 3,	49,		64, &objMP->FSB_Memory[1], &objMP->FSB_Memory[5]);
		MP256_SetFSB(objMP->FSB_Memory, 4,	65,		80, NULL, NULL);
		MP256_SetFSB(objMP->FSB_Memory, 5,	81,		96, &objMP->FSB_Memory[4], &objMP->FSB_Memory[6]);
		MP256_SetFSB(objMP->FSB_Memory, 6,	97,		128, NULL, NULL);
		MP256_SetFSB(objMP->FSB_Memory, 7,	129,	160, &objMP->FSB_Memory[3], &objMP->FSB_Memory[11]);
		MP256_SetFSB(objMP->FSB_Memory, 8,	161,	192, NULL, NULL);
		MP256_SetFSB(objMP->FSB_Memory, 9,	193,	224, &objMP->FSB_Memory[8], &objMP->FSB_Memory[10]);
		MP256_SetFSB(objMP->FSB_Memory, 10,	225,	256, NULL, NULL);
		MP256_SetFSB(objMP->FSB_Memory, 11,	257,	320, &objMP->FSB_Memory[9], &objMP->FSB_Memory[13]);
		MP256_SetFSB(objMP->FSB_Memory, 12,	321,	384, NULL, NULL);
		MP256_SetFSB(objMP->FSB_Memory, 13,	385,	448, &objMP->FSB_Memory[12], &objMP->FSB_Memory[14]);
		MP256_SetFSB(objMP->FSB_Memory, 14,	449,	512, NULL, NULL);
		MP256_SetFSB(objMP->FSB_Memory, 15,	513,	640, &objMP->FSB_Memory[7], &objMP->FSB_Memory[23]);
		MP256_SetFSB(objMP->FSB_Memory, 16,	641,	768, NULL, NULL);
		MP256_SetFSB(objMP->FSB_Memory, 17,	769,	896, &objMP->FSB_Memory[16], &objMP->FSB_Memory[18]);
		MP256_SetFSB(objMP->FSB_Memory, 18,	897,	1024, NULL, NULL);
		MP256_SetFSB(objMP->FSB_Memory, 19,	1025,	1280, &objMP->FSB_Memory[17], &objMP->FSB_Memory[21]);
		MP256_SetFSB(objMP->FSB_Memory, 20,	1281,	1536, NULL, NULL);
		MP256_SetFSB(objMP->FSB_Memory, 21,	1537,	1792, &objMP->FSB_Memory[20], &objMP->FSB_Memory[22]);
		MP256_SetFSB(objMP->FSB_Memory, 22,	1793,	2048, NULL, NULL);
		MP256_SetFSB(objMP->FSB_Memory, 23,	2049,	2304, &objMP->FSB_Memory[19], &objMP->FSB_Memory[27]);
		MP256_SetFSB(objMP->FSB_Memory, 24,	2305,	2560, NULL, NULL);
		MP256_SetFSB(objMP->FSB_Memory, 25,	2561,	2816, &objMP->FSB_Memory[24], &objMP->FSB_Memory[26]);
		MP256_SetFSB(objMP->FSB_Memory, 26,	2817,	3072, NULL, NULL);
		MP256_SetFSB(objMP->FSB_Memory, 27,	3073,	3328, &objMP->FSB_Memory[25], &objMP->FSB_Memory[29]);
		MP256_SetFSB(objMP->FSB_Memory, 28,	3329,	3584, NULL, NULL);
		MP256_SetFSB(objMP->FSB_Memory, 29,	3585,	3840, &objMP->FSB_Memory[28], &objMP->FSB_Memory[30]);
		MP256_SetFSB(objMP->FSB_Memory, 30,	3841,	4096, NULL, NULL);
		objMP->FSB_RootNode = &objMP->FSB_Memory[15];
	}
}

// 释放内存池（对自维护结构体指针使用，和 MP256_Destroy 功能类似）
XXAPI void xrtMemPoolUnit(xmempool objMP)
{
	// 循环释放所有 MMU
	for ( int i = 0; i < objMP->arrMMU.Count; i++ ) {
		MMU_LLNode* pNode = xrtBsmmGetPtr_Inline(&objMP->arrMMU, i);
		if ( pNode->objMMU ) {
			xrtMemUnitDestroy(pNode->objMMU);
		}
	}
	xrtBsmmUnit(&objMP->arrMMU);
	if ( objMP->FSB_Memory ) {
		xrtFree(objMP->FSB_Memory);
		objMP->FSB_Memory = NULL;
	}
	// 循环释放所有大内存块
	for ( int i = 0; i < objMP->BigMM.Count; i++ ) {
		MP_BigInfoLL* pInfo = xrtBsmmGetPtr_Inline(&objMP->BigMM, i);
		if ( pInfo->Ptr ) {
			xrtFree(pInfo->Ptr);
		}
	}
	xrtBsmmUnit(&objMP->BigMM);
	objMP->LL_BigFree = NULL;
}

// 从内存池中申请一块内存
XXAPI void* xrtMemPoolAlloc(xmempool objMP, uint32 iSize)
{
	if ( iSize == 0 ) { return NULL; }
	// 查找符合条件的 FSB 信息
	FSB_Item* objFSB = objMP->FSB_RootNode;
	while ( objFSB ) {
		if ( iSize < objFSB->MinLength ) {
			objFSB = objFSB->left;
		} else if ( iSize > objFSB->MaxLength ) {
			objFSB = objFSB->right;
		} else {
			break;
		}
	}
	if ( objFSB ) {
		// 选定了 FSB，根据 FSB 区块信息通过 MMU256 分配内存
		xmemunit objMMU = NULL;
		if ( objFSB->LL_Idle == NULL ) {
			// 如果没有空闲的内存管理单元，优先使用备用的全空单元，或创建一个新的单元
			if ( objFSB->LL_Null ) {
				// 使用备用的全空内存管理单元
				objMMU = objFSB->LL_Null->objMMU;
				objFSB->LL_Idle = objFSB->LL_Null;
				objFSB->LL_Null = NULL;
			} else if ( objFSB->LL_Free ) {
				// 创建新的内存管理单元，使用已释放的内存管理单元位置
				objMMU = xrtMemUnitCreate(objFSB->MaxLength);
				if ( objMMU == NULL ) {
					return NULL;
				}
				// 恢复Flag，写入新申请的单元
				MMU_LLNode* pNode = objFSB->LL_Free;
				objMMU->Flag = pNode->Flag;
				pNode->objMMU = objMMU;
				// 从 LL_Free 中移除
				if ( pNode->Next ) {
					pNode->Next->Prev = NULL;
				}
				objFSB->LL_Free = pNode->Next;
				// 添加到 LL_Idle
				pNode->Prev = NULL;
				pNode->Next = NULL;
				objFSB->LL_Idle = pNode;
			} else {
				// 创建新的内存管理单元，创建失败就报错处理
				objMMU = xrtMemUnitCreate(objFSB->MaxLength);
				if ( objMMU == NULL ) {
					return NULL;
				}
				// 将创建好的内存管理单元添加到单元阵列管理器，添加失败就报错处理
				MMU_LLNode* pNode = xrtBsmmAlloc(&objMP->arrMMU);
				if ( pNode ) {
					pNode->objMMU = objMMU;
					pNode->Prev = NULL;
					pNode->Next = NULL;
					pNode->Flag = MMU_FLAG_USE | ((objMP->arrMMU.Count - 1) << 8);
					objFSB->LL_Idle = pNode;
					// 标记内存管理器单元的 Flag
					objMMU->Flag = pNode->Flag;
				} else {
					xrtMemUnitDestroy(objMMU);
					xrtSetError("Memory Pool : add memory unit failed.", FALSE);
					return NULL;
				}
			}
		} else {
			// 有空闲的内存管理单元，优先使用空闲的
			objMMU = objFSB->LL_Idle->objMMU;
			// 如果空闲的内存管理单元即将满了，将它转移到满载单元链表
			if ( objMMU->Count >= 255 ) {
				MMU_LLNode* pNode = objFSB->LL_Idle;
				// 从 LL_Idle 中移除
				if ( pNode->Next ) {
					pNode->Next->Prev = NULL;
				}
				objFSB->LL_Idle = pNode->Next;
				// 添加到 LL_Full
				pNode->Prev = NULL;
				pNode->Next = objFSB->LL_Full;
				if ( objFSB->LL_Full ) {
					objFSB->LL_Full->Prev = pNode;
				}
				objFSB->LL_Full = pNode;
			}
		}
		return xrtMemUnitAlloc_Inline(objMMU);
	} else {
		// 无法选定 FSB，使用 malloc 申请内存
		MP_MemHead* pHead = xrtMalloc(sizeof(MP_MemHead) + iSize);
		if ( pHead ) {
			if ( objMP->LL_BigFree ) {
				// 优先复用已释放的 BigMM 元素
				MP_BigInfoLL* pInfo = objMP->LL_BigFree;
				objMP->LL_BigFree = pInfo->Next;
				pHead->Flag = MMU_FLAG_EXT;
				pInfo->Size = iSize;
				pInfo->Ptr = pHead;
				return &pHead[1];
			} else {
				// 没有已释放的 BigMM 元素，就申请一个新的
				MP_BigInfoLL* pInfo = xrtBsmmAlloc(&objMP->BigMM);
				if ( pInfo ) {
					pHead->Index = objMP->BigMM.Count;
					pHead->Flag = MMU_FLAG_EXT;
					pInfo->Size = iSize;
					pInfo->Ptr = pHead;
					return &pHead[1];
				}
			}
			xrtFree(pHead);
		}
		return NULL;
	}
}

// 将内存池申请的内存释放掉
static inline void MP256_LLNode_ClearCheck(FSB_Item* objFSB, MMU_LLNode* pNode, int bLL_Full)
{
	// 如果这个内存管理单元已经清空
	if ( pNode->objMMU->Count == 0 ) {
		if ( objFSB->LL_Null ) {
			// 有备用单元时，直接释放掉这个单元
			xrtMemUnitDestroy(pNode->objMMU);
			pNode->objMMU = NULL;
			// 从 LL_Idle 或 LL_Full 中移除
			if ( pNode->Prev ) {
				pNode->Prev->Next = pNode->Next;
			} else {
				if ( bLL_Full ) {
					objFSB->LL_Full = pNode->Next;
				} else {
					objFSB->LL_Idle = pNode->Next;
				}
			}
			if ( pNode->Next ) {
				pNode->Next->Prev = pNode->Prev;
			}
			// 添加到 LL_Free
			pNode->Prev = NULL;
			pNode->Next = objFSB->LL_Free;
			if ( objFSB->LL_Free ) {
				objFSB->LL_Free->Prev = pNode;
			}
			objFSB->LL_Free = pNode;
		} else {
			// 没有备用单元时，让这个单元备用，避免临界状态反复申请和释放内存管理单元，造成性能损失
			// 从 LL_Idle 或 LL_Full 中移除
			if ( pNode->Prev ) {
				pNode->Prev->Next = pNode->Next;
			} else {
				if ( bLL_Full ) {
					objFSB->LL_Full = pNode->Next;
				} else {
					objFSB->LL_Idle = pNode->Next;
				}
			}
			if ( pNode->Next ) {
				pNode->Next->Prev = pNode->Prev;
			}
			// 添加到 LL_Null
			objFSB->LL_Null = pNode;
			pNode->Prev = NULL;
			pNode->Next = NULL;
		}
	}
}
static inline void MP256_LLNode_IdleCheck(FSB_Item* objFSB, MMU_LLNode* pNode)
{
	if ( pNode->objMMU->Count < 256 ) {
		// 从 LL_Full 中移除
		if ( pNode->Prev ) {
			pNode->Prev->Next = pNode->Next;
		} else {
			objFSB->LL_Full = pNode->Next;
		}
		if ( pNode->Next ) {
			pNode->Next->Prev = pNode->Prev;
		}
		// 添加到 LL_Idle
		pNode->Prev = NULL;
		pNode->Next = objFSB->LL_Idle;
		if ( objFSB->LL_Idle ) {
			objFSB->LL_Idle->Prev = pNode;
		}
		objFSB->LL_Idle = pNode;
	}
}
XXAPI void xrtMemPoolFree(xmempool objMP, void* ptr)
{
	MMU_ValuePtr v = ptr - sizeof(MMU_Value);
	if ( (v->ItemFlag & MMU_FLAG_MASK) == MMU_FLAG_MASK ) {
		// 大内存释放
		MP_MemHead* pHead = ptr - sizeof(MP_MemHead);
		MP_BigInfoLL* pInfo = xrtBsmmGetPtr_Inline(&objMP->BigMM, pHead->Index);
		xrtFree(pInfo->Ptr);
		pHead->Flag = 0;
		pInfo->Ptr = NULL;
		pInfo->Next = objMP->LL_BigFree;
		objMP->LL_BigFree = pInfo;
	} else {
		// FSB内存释放
		if ( v->ItemFlag & MMU_FLAG_USE ) {
			int iMMU = (v->ItemFlag & MMU_FLAG_MASK) >> 8;
			unsigned char idx = v->ItemFlag & 0xFF;
			// 获取对应的内存管理器单元链表结构
			MMU_LLNode* pNode = xrtBsmmGetPtr_Inline(&objMP->arrMMU, iMMU);
			if ( pNode->objMMU == NULL ) {
				xrtSetError("Memory Pool : MMU cannot be null.", FALSE);
				return;
			}
			// 查找符合条件的 FSB 信息
			FSB_Item* objFSB = objMP->FSB_RootNode;
			uint32 iMaxSize = pNode->objMMU->ItemLength - sizeof(MMU_Value);
			while ( objFSB ) {
				if ( iMaxSize < objFSB->MinLength ) {
					objFSB = objFSB->left;
				} else if ( iMaxSize > objFSB->MaxLength ) {
					objFSB = objFSB->right;
				} else {
					break;
				}
			}
			if ( objFSB == NULL ) {
				xrtSetError("Memory Pool : find FSB error.", FALSE);
				return;
			}
			// 调用对应 MMU 的释放函数
			xrtMemUnitFreeIdx_Inline(pNode->objMMU, idx);
			v->ItemFlag = 0;
			// 如果是一个满载的内存管理器单元，将它放入空闲单元列表
			if ( pNode->objMMU->Count >= 255 ) {
				MP256_LLNode_IdleCheck(objFSB, pNode);
			}
			// 如果这个内存管理单元已经清空，将他释放或变为备用单元
			MP256_LLNode_ClearCheck(objFSB, pNode, 0);
		}
	}
}

// 进行一轮GC，将 标记 或 未标记 的内存全部回收
void MP256_GC_RecuFSB(FSB_Item* objFSB, bool bFreeMark)
{
	// 遍历所有 空闲的 和 满载的 内存管理单元，进行标记回收
	MMU_LLNode* pNode = objFSB->LL_Idle;
	while ( pNode ) {
		xrtMemUnitGC(pNode->objMMU, bFreeMark);
		pNode = pNode->Next;
	}
	pNode = objFSB->LL_Full;
	while ( pNode ) {
		xrtMemUnitGC(pNode->objMMU, bFreeMark);
		pNode = pNode->Next;
	}
	// 再次遍历所有 空闲的 和 满载的 内存管理单元，将他们归类到正确的分组
	pNode = objFSB->LL_Idle;
	while ( pNode ) {
		MMU_LLNode* pNext = pNode->Next;
		MP256_LLNode_ClearCheck(objFSB, pNode, 0);
		pNode = pNext;
	}
	pNode = objFSB->LL_Full;
	while ( pNode ) {
		MMU_LLNode* pNext = pNode->Next;
		if ( pNode->objMMU->Count == 0 ) {
			MP256_LLNode_ClearCheck(objFSB, pNode, -1);
		} else {
			MP256_LLNode_IdleCheck(objFSB, pNode);
		}
		pNode = pNext;
	}
	// 递归调用左子树
	if ( objFSB->left ) {
		MP256_GC_RecuFSB(objFSB-> left, bFreeMark);
	}
	// 递归调用右子树
	if ( objFSB->right ) {
		MP256_GC_RecuFSB(objFSB-> right, bFreeMark);
	}
}
XXAPI void xrtMemPoolGC(xmempool objMP, bool bFreeMark)
{
	// 递归回收 FSB 标记的内存
	MP256_GC_RecuFSB(objMP->FSB_RootNode, bFreeMark);
	// 循环大内存列表进行回收
	if ( bFreeMark ) {
		// 被标记的内存将被回收
		for ( int i = 0; i < objMP->BigMM.Count; i++ ) {
			MP_BigInfoLL* pInfo = xrtBsmmGetPtr_Inline(&objMP->BigMM, i);
			MP_MemHead* pHead = pInfo->Ptr;
			if ( pHead->Flag & MMU_FLAG_USE ) {
				if ( pHead->Flag & MMU_FLAG_GC ) {
					xrtFree(pInfo->Ptr);
					pHead->Flag = 0;
					pInfo->Ptr = NULL;
					pInfo->Next = objMP->LL_BigFree;
					objMP->LL_BigFree = pInfo;
				}
			}
		}
	} else {
		// 未被标记的内存将被回收
		for ( int i = 0; i < objMP->BigMM.Count; i++ ) {
			MP_BigInfoLL* pInfo = xrtBsmmGetPtr_Inline(&objMP->BigMM, i);
			MP_MemHead* pHead = pInfo->Ptr;
			if ( pHead->Flag & MMU_FLAG_USE ) {
				if ( pHead->Flag & MMU_FLAG_GC ) {
					pHead->Flag &= ~MMU_FLAG_GC;
				} else {
					xrtFree(pInfo->Ptr);
					pHead->Flag = 0;
					pInfo->Ptr = NULL;
					pInfo->Next = objMP->LL_BigFree;
					objMP->LL_BigFree = pInfo;
				}
			}
		}
	}
}


