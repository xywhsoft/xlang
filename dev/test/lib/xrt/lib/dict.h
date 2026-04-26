


// 哈希值计算函数（内部函数）
#if defined(__x86_64__) || defined(_M_X64)
	#define Dict_EvalHash(obj, k, l) obj.Key = k; obj.KeyLen = l; obj.Hash = xrtHash64(k, l)
#elif defined(__i386__) || defined(_M_IX86)
	#define Dict_EvalHash(obj, k, l) obj.Key = k; obj.KeyLen = l; obj.Hash = xrtHash32(k, l)
#endif

// 哈希表比较函数（内部函数）
int Dict_CompProc(Dict_Key* pNode, Dict_Key* pObjKey)
{
	if ( pNode->Hash == pObjKey->Hash ) {
		if ( pNode->KeyLen == pObjKey->KeyLen ) {
			return memcmp(pNode->Key, pObjKey->Key, pObjKey->KeyLen);
		} else {
			return pNode->KeyLen - pObjKey->KeyLen;
		}
	} else if ( pNode->Hash > pObjKey->Hash ) {
		return 1;
	} else {
		return -1;
	}
}



// 创建哈希表
XXAPI xdict xrtDictCreate(uint32 iItemLength)
{
	xdict objHT = xrtMalloc(sizeof(xdict_struct));
	if ( objHT ) {
		xrtDictInit(objHT, iItemLength);
	}
	return objHT;
}

// 销毁哈希表
XXAPI void xrtDictDestroy(xdict objHT)
{
	if ( objHT ) {
		xrtDictUnit(objHT);
		xrtFree(objHT);
	}
}

// 初始化哈希表（对自维护结构体指针使用，和 AVLHT32_Create 功能类似）
void AVLHT32_FreeProc(xdict objTree, Dict_Key* pNode)
{
	if ( objTree->MP ) {
		xrtMemPoolFree(objTree->MP, pNode->Key);
	} else {
		xrtFree(pNode->Key);
	}
}
XXAPI void xrtDictInit(xdict objHT, uint32 iItemLength)
{
	xrtAVLTreeInit(&objHT->AVLT, iItemLength + sizeof(Dict_Key), (ptr)Dict_CompProc);
	objHT->AVLT.FreeProc = (ptr)AVLHT32_FreeProc;
	objHT->MP = NULL;
}

// 释放哈希表（对自维护结构体指针使用，和 AVLHT32_Destroy 功能类似）
XXAPI void xrtDictUnit(xdict objHT)
{
	xrtAVLTreeUnit(&objHT->AVLT);
}

// 设置值
XXAPI ptr xrtDictSet(xdict objHT, ptr sKey, uint32 iKeyLen, bool* bNewRet)
{
	Dict_Key objKey;
	Dict_EvalHash(objKey, sKey, iKeyLen);
	return xrtDictSetWithKey(objHT, &objKey, bNewRet);
}

// 设置值 - 当值为 ptr 时直接修改指针内容
XXAPI bool xrtDictSetPtr(xdict objHT, ptr sKey, uint32 iKeyLen, ptr pVal, ptr* ppOldVal)
{
	Dict_Key objKey;
	Dict_EvalHash(objKey, sKey, iKeyLen);
	bool bNew;
	ptr* ppVal = xrtDictSetWithKey(objHT, &objKey, &bNew);
	if ( ppVal ) {
		// 传回旧值
		if ( ppOldVal ) {
			if ( bNew ) {
				*ppOldVal = NULL;
			} else {
				*ppOldVal = ppVal[0];
			}
		}
		// 修改为新值
		ppVal[0] = pVal;
		return TRUE;
	} else {
		if ( ppOldVal ) {
			*ppOldVal = NULL;
		}
		return FALSE;
	}
}

// 获取值
XXAPI ptr xrtDictGet(xdict objHT, ptr sKey, uint32 iKeyLen)
{
	Dict_Key objKey;
	Dict_EvalHash(objKey, sKey, iKeyLen);
	return xrtDictGetWithKey(objHT, &objKey);
}

// 获取值 - 当值为 ptr 时直接获取指针内容
XXAPI ptr xrtDictGetPtr(xdict objHT, ptr sKey, uint32 iKeyLen)
{
	Dict_Key objKey;
	Dict_EvalHash(objKey, sKey, iKeyLen);
	struct {
		ptr val;
	} *pData = xrtDictGetWithKey(objHT, &objKey);
	if ( pData ) {
		return pData->val;
	}
	return NULL;
}

// 删除值
XXAPI bool xrtDictRemove(xdict objHT, ptr sKey, uint32 iKeyLen)
{
	Dict_Key objKey;
	Dict_EvalHash(objKey, sKey, iKeyLen);
	return xrtAVLTreeRemove(&objHT->AVLT, &objKey);
}

// 删除值，当值为 ptr 时返回 ptr
XXAPI ptr xrtDictRemovePtr(xdict objHT, ptr sKey, uint32 iKeyLen)
{
	Dict_Key objKey;
	Dict_EvalHash(objKey, sKey, iKeyLen);
	xavltnode pDelNode = xrtAVLTB_Remove((xavltbase)&objHT->AVLT, objHT->AVLT.CompProc, &objKey);
	if ( pDelNode ) {
		Dict_Key* pKeyPtr = xrtAVLTreeGetNodeData(pDelNode);
		ptr* pData = (ptr*)&pKeyPtr[1];
		if ( objHT->AVLT.FreeProc ) {
			objHT->AVLT.FreeProc(&objHT->AVLT, &pDelNode[1]);
		}
		xrtFSMemPoolFree(&objHT->AVLT.objMM, pDelNode);
		return pData[0];
	} else {
		return NULL;
	}
}

// 判断值是否存在
XXAPI bool xrtDictExists(xdict objHT, ptr sKey, uint32 iKeyLen)
{
	Dict_Key objKey;
	Dict_EvalHash(objKey, sKey, iKeyLen);
	Dict_Key* pNode = xrtAVLTreeSearch(&objHT->AVLT, &objKey);
	if ( pNode ) {
		return TRUE;
	}
	return FALSE;
}

// 获取表内元素数量
XXAPI uint32 xrtDictCount(xdict objHT)
{
	return objHT->AVLT.Count;
}

// 遍历表元素
int AVLHT32_WalkRecuProc(xavltnode root, Dict_EachProc procEach, ptr pArg)
{
	if ( root ) {
		// 递归左子树
		if ( root->left != NULL ) {
			if ( AVLHT32_WalkRecuProc(root->left, procEach, pArg) ) {
				return -1;
			}
		}
		// 调用回调函数
		if ( procEach ) {
			if ( procEach(xrtAVLTreeGetNodeData(root), ((ptr)root) + sizeof(xavltnode_struct) + sizeof(Dict_Key), pArg) ) {
				return -1;
			}
		}
		// 递归右子树
		if ( root->right != NULL ) {
			if ( AVLHT32_WalkRecuProc(root->right, procEach, pArg) ) {
				return -1;
			}
		}
	}
	return 0;
}
XXAPI void xrtDictWalk(xdict objHT, Dict_EachProc procEach, ptr pArg)
{
	AVLHT32_WalkRecuProc(objHT->AVLT.RootNode, procEach, pArg);
}


