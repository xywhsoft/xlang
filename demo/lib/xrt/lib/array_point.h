

// 创建指针内存管理器
XXAPI xparray xrtPtrArrayCreate()
{
	xparray ObjPtr = xrtMalloc(sizeof(xparray_struct));
	if ( ObjPtr ) {
		xrtPtrArrayInit(ObjPtr);
	}
	return ObjPtr;
}

// 销毁指针内存管理器
XXAPI void xrtPtrArrayDestroy(xparray pObject)
{
	if ( pObject ) {
		xrtPtrArrayUnit(pObject);
		xrtFree(pObject);
	}
}

// 初始化内存管理器（对自维护结构体指针使用）
XXAPI void xrtPtrArrayInit(xparray pObject)
{
	pObject->Memory = NULL;
	pObject->Count = 0;
	pObject->AllocCount = 0;
	pObject->AllocStep = XPARRAY_PREASSIGNSTEP;
}

// 释放内存管理器（对自维护结构体指针使用）
XXAPI void xrtPtrArrayUnit(xparray pObject)
{
	if ( pObject->Memory ) { xrtFree(pObject->Memory); pObject->Memory = NULL; }
	pObject->Count = 0;
	pObject->AllocCount = 0;
}

// 分配内存
XXAPI bool xrtPtrArrayMalloc(xparray pObject, uint32 iCount)
{
	if ( iCount > pObject->AllocCount ) {
		// 增量
		ptr* pNew = xrtRealloc(pObject->Memory, iCount * sizeof(ptr));
		if ( pNew ) {
			pObject->AllocCount = iCount;
			pObject->Memory = pNew;
			return TRUE;
		}
	} else if ( iCount < pObject->AllocCount ) {
		// 裁剪
		ptr* pNew = xrtRealloc(pObject->Memory, iCount * sizeof(ptr));
		if ( pNew ) {
			pObject->AllocCount = iCount;
			pObject->Memory = pNew;
			if ( pObject->Count > iCount ) {
				// 需要裁剪数据
				pObject->Count = iCount;
			}
			return TRUE;
		}
	} else if ( iCount = 0 ) {
		// 清空
		xrtPtrArrayUnit(pObject);
		return TRUE;
	} else {
		// 不变
		return TRUE;
	}
	return FALSE;
}

// 中间插入成员(0为头部插入，pObject->Count为末尾插入)
XXAPI uint32 xrtPtrArrayInsert(xparray pObject, uint32 iPos, ptr pVal)
{
	// 分配内存
	if ( pObject->Count >= pObject->AllocCount ) {
		if ( xrtPtrArrayMalloc(pObject, pObject->Count + pObject->AllocStep) == 0 ) {
			return 0;
		}
	}
	if ( iPos < pObject->Count ) {
		// 插入模式（需要移动内存）
		ptr* src = &(pObject->Memory[iPos]);
		memmove(src + 1, src, (pObject->Count - iPos) * sizeof(ptr));
		pObject->Memory[iPos] = pVal;
		pObject->Count++;
		return iPos + 1;
	} else {
		// 追加模式
		pObject->Memory[pObject->Count] = pVal;
		pObject->Count++;
		return pObject->Count;
	}
}

// 末尾添加成员
XXAPI uint32 xrtPtrArrayAppend(xparray pObject, ptr pVal)
{
	return xrtPtrArrayInsert(pObject, pObject->Count, pVal);
}

// 添加成员，自动查找空隙（替换为 NULL 的值）
XXAPI uint32 xrtPtrArrayAddAlt(xparray pObject, ptr pVal)
{
	for ( int i = 0; i < pObject->Count; i++ ) {
		if ( pObject->Memory[i] == NULL ) {
			pObject->Memory[i] = pVal;
			return i + 1;
		}
	}
	return xrtPtrArrayInsert(pObject, pObject->Count, pVal);
}

// 交换成员
XXAPI bool xrtPtrArraySwap(xparray pObject, uint32 iPosA, uint32 iPosB)
{
	// 范围检查
	if ( iPosA == 0 ) { return FALSE; }
	if ( iPosA > pObject->Count ) { return FALSE; }
	if ( iPosB == 0 ) { return FALSE; }
	if ( iPosB > pObject->Count ) { return FALSE; }
	if ( iPosA == iPosB ) { return TRUE; }
	// 交换成员
	iPosA--;
	iPosB--;
	ptr pA = pObject->Memory[iPosB];
	pObject->Memory[iPosB] = pObject->Memory[iPosA];
	pObject->Memory[iPosA] = pA;
	return TRUE;
}

// 删除成员
XXAPI bool xrtPtrArrayRemove(xparray pObject, uint32 iPos, uint32 iCount)
{
	// 不能添加 0 个成员、不能删除不存在的成员（0号成员也不存在）
	if ( iCount == 0 ) { return FALSE; }
	if ( iPos == 0 ) { return FALSE; }
	if ( iPos > pObject->Count ) { return FALSE; }
	// 删除成员（数量不足时删除后面的所有成员）
	iPos--;
	if ( iPos + iCount < pObject->Count ) {
		// 中段删除
		ptr* dst = &(pObject->Memory[iPos]);
		memmove(dst, dst + iCount, (pObject->Count - (iPos + iCount)) * sizeof(ptr));
		pObject->Count -= iCount;
	} else {
		// 末尾删除
		pObject->Count = iPos;
	}
	return TRUE;
}

// 获取成员指针
XXAPI ptr xrtPtrArrayGet(xparray pObject, uint32 iPos)
{
	if ( iPos ) {
		iPos--;
		if ( iPos < pObject->Count ) {
			return pObject->Memory[iPos];
		}
	}
	return NULL;
}
XXAPI ptr xrtPtrArrayGet_Unsafe(xparray pObject, uint32 iPos)
{
	return pObject->Memory[iPos - 1];
}

// 设置成员指针
XXAPI bool xrtPtrArraySet(xparray pObject, uint32 iPos, ptr pVal)
{
	if ( iPos ) {
		iPos--;
		if ( iPos < pObject->Count ) {
			pObject->Memory[iPos] = pVal;
			return TRUE;
		}
	}
	return FALSE;
}
XXAPI void xrtPtrArraySet_Unsafe(xparray pObject, uint32 iPos, ptr pVal)
{
	pObject->Memory[iPos - 1] = pVal;
}

// 成员排序
XXAPI bool xrtPtrArraySort(xparray pObject, ptr procCompar)
{
	if ( pObject ) {
		qsort(pObject->Memory, pObject->Count, sizeof(ptr), procCompar);
		return TRUE;
	} else {
		return FALSE;
	}
}


