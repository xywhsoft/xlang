


// 创建数组
XXAPI xarray xrtArrayCreate(uint32 iItemLength)
{
	xarray pArr = xrtMalloc(sizeof(xarray_struct));
	if ( pArr == NULL ) {
		return NULL;
	}
	xrtArrayInit(pArr, iItemLength);
	return pArr;
}

// 销毁数组
XXAPI void xrtArrayDestroy(xarray pArr)
{
	if ( pArr ) {
		xrtArrayUnit(pArr);
		xrtFree(pArr);
	}
}

// 初始化数组的数据结构 ( 用于内嵌数组的对象使用 )
XXAPI void xrtArrayInit(xarray pArr, uint32 iItemLength)
{
	pArr->Memory = NULL;
	pArr->ItemLength = iItemLength;
	pArr->Count = 0;
	pArr->AllocCount = 0;
	pArr->AllocStep = XARRAY_PREASSIGNSTEP;
}

// 释放数组的数据结构 ( 但不会释放数组结构体本身的内存，用于内嵌数组的对象使用 )
XXAPI void xrtArrayUnit(xarray pArr)
{
	if ( pArr->Memory ) { xrtFree(pArr->Memory); pArr->Memory = NULL; }
	pArr->Count = 0;
	pArr->AllocCount = 0;
}

// 分配内存
XXAPI bool xrtArrayAlloc(xarray pArr, uint32 iCount)
{
	if ( iCount > pArr->AllocCount ) {
		// 增量
		ptr pNew = xrtRealloc(pArr->Memory, iCount * pArr->ItemLength);
		if ( pNew ) {
			pArr->AllocCount = iCount;
			pArr->Memory = pNew;
			return TRUE;
		}
	} else if ( iCount < pArr->AllocCount ) {
		// 裁剪
		ptr pNew = xrtRealloc(pArr->Memory, iCount * pArr->ItemLength);
		if ( pNew ) {
			pArr->AllocCount = iCount;
			pArr->Memory = pNew;
			if ( pArr->Count > iCount ) {
				// 需要裁剪数据
				pArr->Count = iCount;
			}
			return TRUE;
		}
	} else if ( iCount == 0 ) {
		// 清空
		xrtArrayUnit(pArr);
		return TRUE;
	} else {
		// 不变
		return TRUE;
	}
	return FALSE;
}

// 中间插入成员
XXAPI uint32 xrtArrayInsert(xarray pArr, uint32 iPos, uint32 iCount)
{
	// 不能添加 0 个成员
	if ( iCount == 0 ) { iCount = 1; }
	// 分配内存
	if ( (pArr->Count + iCount) > pArr->AllocCount ) {
		if ( xrtArrayAlloc(pArr, pArr->Count + iCount + pArr->AllocStep) == FALSE ) {
			return 0;
		}
	}
	if ( iPos < pArr->Count ) {
		// 插入模式（需要移动内存）
		ptr dst = pArr->Memory + ((iPos + iCount) * pArr->ItemLength);
		ptr src = pArr->Memory + (iPos * pArr->ItemLength);
		memmove(dst, src, (pArr->Count - iPos) * pArr->ItemLength);
		pArr->Count += iCount;
		return iPos + 1;
	} else {
		// 追加模式
		pArr->Count += iCount;
		return pArr->Count - iCount + 1;
	}
}

// 末尾添加成员
XXAPI uint32 xrtArrayAppend(xarray pArr, uint32 iCount)
{
	return xrtArrayInsert(pArr, pArr->Count, iCount);
}

// 交换成员
XXAPI bool xrtArraySwap(xarray pArr, uint32 iPosA, uint32 iPosB)
{
	// 范围检查
	if ( iPosA == 0 ) { return FALSE; }
	if ( iPosA > pArr->Count ) { return FALSE; }
	if ( iPosB == 0 ) { return FALSE; }
	if ( iPosB > pArr->Count ) { return FALSE; }
	if ( iPosA == iPosB ) { return TRUE; }
	// 交换成员
	iPosA--;
	iPosB--;
	ptr pTemp = xrtMalloc(pArr->ItemLength);
	if ( pTemp == NULL ) {
		return FALSE;
	}
	memmove(pTemp, pArr->Memory + (iPosA * pArr->ItemLength), pArr->ItemLength);
	memmove(pArr->Memory + (iPosA * pArr->ItemLength), pArr->Memory + (iPosB * pArr->ItemLength), pArr->ItemLength);
	memmove(pArr->Memory + (iPosB * pArr->ItemLength), pTemp, pArr->ItemLength);
	xrtFree(pTemp);
	return TRUE;
}

// 删除成员
XXAPI bool xrtArrayRemove(xarray pArr, uint32 iPos, uint32 iCount)
{
	// 不能添加 0 个成员、不能删除不存在的成员（0号成员也不存在）
	if ( iCount == 0 ) { return FALSE; }
	if ( iPos == 0 ) { return FALSE; }
	if ( iPos > pArr->Count ) { return FALSE; }
	// 删除成员（数量不足时删除后面的所有成员）
	iPos--;
	if ( iPos + iCount < pArr->Count ) {
		// 中段删除
		ptr dst = pArr->Memory + (iPos * pArr->ItemLength);
		ptr src = pArr->Memory + ((iPos + iCount) * pArr->ItemLength);
		memmove(dst, src, (pArr->Count - (iPos + iCount)) * pArr->ItemLength);
		pArr->Count -= iCount;
	} else {
		// 末尾删除
		pArr->Count = iPos;
	}
	return TRUE;
}

// 获取成员指针
XXAPI ptr xrtArrayGet(xarray pArr, uint32 iPos)
{
	if ( iPos ) {
		iPos--;
		if ( iPos < pArr->Count ) {
			return &pArr->Memory[iPos * pArr->ItemLength];
		}
	}
	return NULL;
}
XXAPI ptr xrtArrayGet_Unsafe(xarray pArr, uint32 iPos)
{
	return &(pArr->Memory[(iPos - 1) * pArr->ItemLength]);
}

// 成员排序
XXAPI bool xrtArraySort(xarray pArr, ptr procCompar)
{
	if ( pArr ) {
		qsort(pArr->Memory, pArr->Count, pArr->ItemLength, procCompar);
		return TRUE;
	} else {
		return FALSE;
	}
}


