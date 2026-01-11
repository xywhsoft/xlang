


// 哈希表比较函数（内部函数）
int List_CompProc(int64* pNode, int64* pObjKey)
{
	if ( *pNode == *pObjKey ) {
		return 0;
	} else if ( *pNode > *pObjKey ) {
		return -1;
	} else {
		return 1;
	}
}



// 创建列表
XXAPI xlist xrtListCreate(uint32 iItemLength)
{
	xlist objList = xrtMalloc(sizeof(xlist_struct));
	if ( objList ) {
		xrtListInit(objList, iItemLength);
	}
	return objList;
}

// 销毁列表
XXAPI void xrtListDestroy(xlist objList)
{
	if ( objList ) {
		xrtListUnit(objList);
		xrtFree(objList);
	}
}

// 初始化列表（对自维护结构体指针使用）
XXAPI void xrtListInit(xlist objList, uint32 iItemLength)
{
	xrtAVLTreeInit(&objList->AVLT, iItemLength + sizeof(int64), (ptr)List_CompProc);
}

// 释放列表（对自维护结构体指针使用）
XXAPI void xrtListUnit(xlist objList)
{
	xrtAVLTreeUnit(&objList->AVLT);
}

// 设置值
XXAPI ptr xrtListSet(xlist objList, int64 iKey, bool* bNewRet)
{
	bool bNew;
	int64* pNode = xrtAVLTreeInsert(&objList->AVLT, &iKey, &bNew);
	if ( pNode == NULL ) {
		return NULL;
	}
	if ( bNewRet ) {
		*bNewRet = bNew;
	}
	if ( bNew ) {
		*pNode = iKey;
	}
	return &pNode[1];
}

// 设置值 - 当值为 ptr 时直接修改指针内容
XXAPI bool xrtListSetPtr(xlist objList, int64 iKey, ptr pVal, ptr* ppOldVal)
{
	bool bNew;
	int64* pNode = xrtAVLTreeInsert(&objList->AVLT, &iKey, &bNew);
	if ( pNode == NULL ) {
		return FALSE;
	}
	if ( bNew ) {
		*pNode = iKey;
	}
	// 获取单指针数据结构
	struct {
		ptr val;
	} *pData = (ptr)&pNode[1];
	// 传回旧值
	if ( ppOldVal ) {
		if ( bNew ) {
			*ppOldVal = NULL;
		} else {
			*ppOldVal = pData->val;
		}
	}
	// 修改为新值
	pData->val = pVal;
	return TRUE;
}

// 获取值
XXAPI ptr xrtListGet(xlist objList, int64 iKey)
{
	int64* pNode = xrtAVLTreeSearch(&objList->AVLT, &iKey);
	if ( pNode ) {
		return &pNode[1];
	}
	return NULL;
}

// 获取值 - 当值为 ptr 时直接获取指针内容
XXAPI ptr xrtListGetPtr(xlist objList, int64 iKey)
{
	int64* pNode = xrtAVLTreeSearch(&objList->AVLT, &iKey);
	if ( pNode ) {
		// 指针大小可能为 4 或 8 字节，使用 memcpy 确保跨平台兼容性
		ptr result;
		memcpy(&result, &pNode[1], sizeof(ptr));
		return result;
	}
	return NULL;
}

// 删除值
XXAPI bool xrtListRemove(xlist objList, int64 iKey)
{
	return xrtAVLTreeRemove(&objList->AVLT, &iKey);
}

// 删除值，当值为 ptr 时返回 ptr
XXAPI ptr xrtListRemovePtr(xlist objList, int64 iKey)
{
	xavltnode pDelNode = xrtAVLTB_Remove((xavltbase)&objList->AVLT, objList->AVLT.CompProc, &iKey);
	if ( pDelNode ) {
		int64* pKeyPtr = (int64*)xrtAVLTreeGetNodeData(pDelNode);  // List 使用 int64 作为键
		ptr* pData = (ptr*)&pKeyPtr[1];
		ptr result = pData[0];  // 先保存返回值
		if ( objList->AVLT.FreeProc ) {
			objList->AVLT.FreeProc(&objList->AVLT, &pDelNode[1]);
		}
		xrtFSMemPoolFree(&objList->AVLT.objMM, pDelNode);
		return result;  // 返回保存的值
	} else {
		return NULL;
	}
}

// 判断值是否存在
XXAPI bool xrtListExists(xlist objList, int64 iKey)
{
	int64* pNode = xrtAVLTreeSearch(&objList->AVLT, &iKey);
	if ( pNode ) {
		return TRUE;
	}
	return FALSE;
}

// 获取表内元素数量
XXAPI uint32 xrtListCount(xlist objList)
{
	return objList->AVLT.Count;
}

// 遍历表元素
int List_WalkRecuProc(xavltnode root, List_EachProc procEach, ptr pArg)
{
	if ( root ) {
		// 递归左子树
		if ( root->left != NULL ) {
			if ( List_WalkRecuProc(root->left, procEach, pArg) ) {
				return -1;
			}
		}
		// 调用回调函数
		if ( procEach ) {
			if ( procEach(((int64*)&root[1])[0], ((ptr)root) + sizeof(xavltnode_struct) + sizeof(int64), pArg) ) {
				return -1;
			}
		}
		// 递归右子树
		if ( root->right != NULL ) {
			if ( List_WalkRecuProc(root->right, procEach, pArg) ) {
				return -1;
			}
		}
	}
	return 0;
}
XXAPI void xrtListWalk(xlist objList, List_EachProc procEach, ptr pArg)
{
	List_WalkRecuProc(objList->AVLT.RootNode, procEach, pArg);
}


