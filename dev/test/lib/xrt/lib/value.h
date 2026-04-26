


// 静态值 : empty、null、true、false
static xvalue_struct XVO_VALUE_EMPTY = {
	XVO_DT_EMPTY,
	0,
	TRUE,
	0,
	0,
	0
};
static xvalue_struct XVO_VALUE_NULL = {
	XVO_DT_NULL,
	0,
	TRUE,
	0,
	0,
	0
};
static xvalue_struct XVO_VALUE_TRUE = {
	XVO_DT_BOOL,
	0,
	TRUE,
	0,
	sizeof(bool),
	TRUE
};
static xvalue_struct XVO_VALUE_FALSE = {
	XVO_DT_BOOL,
	0,
	TRUE,
	0,
	sizeof(bool),
	FALSE
};



// 引用计数操作
XXAPI void xvoAddRef(xvalue pVal)
{
	if ( pVal ) {
		if ( pVal->RefCount >= 0x3FFFFFF ) {
			// 引用计数太多，就转为静态值
			pVal->IsStatic = 1;
		} else {
			pVal->RefCount++;
		}
	}
}
bool xvoListClear_FreeProc(int64 pKey, xvalue* ppVal, xlist pList)
{
	xvoUnref(*ppVal);
	return FALSE;
}
bool xvoCollClear_FreeProc(Coll_Key* pKey, xavltree pColl)
{
	xvoUnref(pKey->Value);
	return FALSE;
}
bool xvoTableClear_FreeProc(Dict_Key* pKey, xvalue* ppVal, xdict pTbl)
{
	xvoUnref(*ppVal);
	return FALSE;
}
XXAPI void xvoUnref(xvalue pVal)
{
	if ( pVal ) {
		if ( pVal->IsStatic == 0 ) {
			pVal->RefCount--;
			// 引用计数用完了就销毁对象
			if ( pVal->RefCount == 0 ) {
				// 释放值
				if ( pVal->Type == XVO_DT_TEXT ) {
					xrtFree(pVal->vText);
				} else if ( pVal->Type == XVO_DT_ARRAY ) {
					for ( int i = 1; i <= pVal->vArray->Count; i++ ) {
						xvalue pItem = xrtPtrArrayGet_Inline(pVal->vArray, i);
						xvoUnref(pItem);
					}
					xrtPtrArrayDestroy(pVal->vArray);
				} else if ( pVal->Type == XVO_DT_LIST ) {
					xrtListWalk(pVal->vList, (ptr)xvoListClear_FreeProc, pVal->vList);
					xrtListDestroy(pVal->vList);
				} else if ( pVal->Type == XVO_DT_COLL ) {
					xrtAVLTreeWalk(pVal->vColl, (ptr)xvoCollClear_FreeProc, pVal->vColl);
					xrtAVLTreeDestroy(pVal->vColl);
				} else if ( pVal->Type == XVO_DT_TABLE ) {
					xrtDictWalk(pVal->vTable, (ptr)xvoTableClear_FreeProc, pVal->vTable);
					xrtDictDestroy(pVal->vTable);
				} else if ( pVal->Type == XVO_DT_STRUCT ) {
					xrtFree(pVal->vStruct);
				} else if ( pVal->Type == XVO_DT_OBJECT ) {
					xrtFree(pVal->vObject);
				} else if ( pVal->Type == XVO_DT_CUSTOM ) {
				}
				// 释放变量本身
				xrtFree(pVal);
				#ifdef DEBUG_TRACE
					printf("free value : %x\n", pVal);
				#endif
			}
		}
	}
}



// 创建值
XXAPI xvalue xvoCreateNull()
{
	return &XVO_VALUE_NULL;
}
XXAPI xvalue xvoCreateBool(bool bVal)
{
	if ( bVal ) {
		return &XVO_VALUE_TRUE;
	} else {
		return &XVO_VALUE_FALSE;
	}
}
XXAPI xvalue xvoCreateInt(int64 iVal)
{
	xvalue pVal = xrtMalloc(sizeof(xvalue_struct));
	if ( pVal ) {
		pVal->Type = XVO_DT_INT;
		pVal->IsStatic = FALSE;
		pVal->RefCount = 1;
		pVal->Size = sizeof(int64);
		pVal->vInt = iVal;
	}
	return pVal;
}
XXAPI xvalue xvoCreateFloat(double fVal)
{
	xvalue pVal = xrtMalloc(sizeof(xvalue_struct));
	if ( pVal ) {
		pVal->Type = XVO_DT_FLOAT;
		pVal->IsStatic = FALSE;
		pVal->RefCount = 1;
		pVal->Size = sizeof(double);
		pVal->vFloat = fVal;
	}
	return pVal;
}
XXAPI xvalue xvoCreateText(ptr sVal, uint32 iSize, bool bColloc)
{
	if ( sVal == NULL ) {
		sVal = xCore.sNull;
		iSize = 0;
		bColloc = TRUE;
	} else if ( iSize == 0 ) {
		iSize = strlen(sVal);
		if ( iSize == 0 ) {
			sVal = xCore.sNull;
			bColloc = TRUE;
		}
	}
	xvalue pVal = xrtMalloc(sizeof(xvalue_struct));
	if ( pVal ) {
		pVal->Type = XVO_DT_TEXT;
		pVal->IsStatic = FALSE;
		pVal->RefCount = 1;
		pVal->Size = iSize;
		if ( bColloc ) {
			pVal->vText = sVal;
		} else {
			pVal->vText = xrtCopyStr(sVal, iSize);
			if ( pVal->vText == xCore.sNull ) {
				xrtFree(pVal);
				return NULL;
			}
		}
	}
	return pVal;
}
XXAPI xvalue xvoCreateTime(xtime tVal)
{
	xvalue pVal = xrtMalloc(sizeof(xvalue_struct));
	if ( pVal ) {
		pVal->Type = XVO_DT_TIME;
		pVal->IsStatic = FALSE;
		pVal->RefCount = 1;
		pVal->Size = sizeof(xtime);
		pVal->vTime = tVal;
	}
	return pVal;
}
XXAPI xvalue xvoCreateTimeSerial(int64 iYear, int iMonth, int iDay, int iHour, int iMinute, int iSecond)
{
	xvalue pVal = xrtMalloc(sizeof(xvalue_struct));
	if ( pVal ) {
		pVal->Type = XVO_DT_TIME;
		pVal->IsStatic = FALSE;
		pVal->RefCount = 1;
		pVal->Size = sizeof(xtime);
		pVal->vTime = xrtDateTimeSerial(iYear, iMonth, iDay, iHour, iMinute, iSecond);
	}
	return pVal;
}
XXAPI xvalue xvoCreatePoint(ptr point)
{
	xvalue pVal = xrtMalloc(sizeof(xvalue_struct));
	if ( pVal ) {
		pVal->Type = XVO_DT_POINT;
		pVal->IsStatic = FALSE;
		pVal->RefCount = 1;
		pVal->Size = sizeof(ptr);
		pVal->vPoint = point;
	}
	return pVal;
}
XXAPI xvalue xvoCreateFunc(xfunction pFunc)
{
	xvalue pVal = xrtMalloc(sizeof(xvalue_struct));
	if ( pVal ) {
		pVal->Type = XVO_DT_FUNC;
		pVal->IsStatic = FALSE;
		pVal->RefCount = 1;
		pVal->Size = sizeof(ptr);
		pVal->vFunc = pFunc;
	}
	return pVal;
}
XXAPI xvalue xvoCreateArray()
{
	xvalue pVal = xrtMalloc(sizeof(xvalue_struct));
	if ( pVal ) {
		xparray objArr = xrtPtrArrayCreate();
		if ( objArr == NULL ) {
			xrtFree(pVal);
			return NULL;
		}
		pVal->Type = XVO_DT_ARRAY;
		pVal->IsStatic = FALSE;
		pVal->RefCount = 1;
		pVal->Size = 0;
		pVal->vArray = objArr;
	}
	return pVal;
}
XXAPI xvalue xvoCreateList()
{
	xvalue pVal = xrtMalloc(sizeof(xvalue_struct));
	if ( pVal ) {
		xlist objList = xrtListCreate(sizeof(xvalue));
		if ( objList == NULL ) {
			xrtFree(pVal);
			return NULL;
		}
		pVal->Type = XVO_DT_LIST;
		pVal->IsStatic = FALSE;
		pVal->RefCount = 1;
		pVal->Size = 0;
		pVal->vList = objList;
	}
	return pVal;
}
XXAPI xvalue xvoCreateColl()
{
	xvalue pVal = xrtMalloc(sizeof(xvalue_struct));
	if ( pVal ) {
		int Coll_CompProc(Coll_Key* pNode, Coll_Key* pObjKey);	// 比较函数定义
		xavltree objColl = xrtAVLTreeCreate(sizeof(Coll_Key), (ptr)Coll_CompProc);
		if ( objColl == NULL ) {
			xrtFree(pVal);
			return NULL;
		}
		pVal->Type = XVO_DT_COLL;
		pVal->IsStatic = FALSE;
		pVal->RefCount = 1;
		pVal->Size = 0;
		pVal->vColl = objColl;
	}
	return pVal;
}
XXAPI xvalue xvoCreateTable()
{
	xvalue pVal = xrtMalloc(sizeof(xvalue_struct));
	if ( pVal ) {
		xdict objTbl = xrtDictCreate(sizeof(xvalue));
		if ( objTbl == NULL ) {
			xrtFree(pVal);
			return NULL;
		}
		pVal->Type = XVO_DT_TABLE;
		pVal->IsStatic = FALSE;
		pVal->RefCount = 1;
		pVal->Size = 0;
		pVal->vTable = objTbl;
	}
	return pVal;
}
XXAPI xvalue xvoCreateStruct(uint32 iSize)
{
	if ( iSize == 0 ) {
		return NULL;
	}
	xvalue pVal = xrtMalloc(sizeof(xvalue_struct));
	if ( pVal ) {
		ptr pStruct = xrtMalloc(iSize);
		if ( pStruct == NULL ) {
			xrtFree(pVal);
			return NULL;
		}
		pVal->Type = XVO_DT_STRUCT;
		pVal->IsStatic = FALSE;
		pVal->RefCount = 1;
		pVal->Size = iSize;
		pVal->vStruct = pStruct;
	}
	return pVal;
}
XXAPI xvalue xvoCreateObject(uint32 iSize)
{
	if ( iSize == 0 ) {
		return NULL;
	}
	xvalue pVal = xrtMalloc(sizeof(xvalue_struct));
	if ( pVal ) {
		ptr pStruct = xrtMalloc(iSize);
		if ( pStruct == NULL ) {
			xrtFree(pVal);
			return NULL;
		}
		pVal->Type = XVO_DT_OBJECT;
		pVal->IsStatic = FALSE;
		pVal->RefCount = 1;
		pVal->Size = iSize;
		pVal->vObject = pStruct;
	}
	return pVal;
}
XXAPI xvalue xvoCreateCustom(ptr pObj)
{
	xvalue pVal = xrtMalloc(sizeof(xvalue_struct));
	if ( pVal ) {
		pVal->Type = XVO_DT_CUSTOM;
		pVal->IsStatic = FALSE;
		pVal->RefCount = 1;
		pVal->Size = 0;
		pVal->vCustom = pObj;
	}
	return pVal;
}



// 读取值
XXAPI bool xvoGetBool(xvalue pVal)
{
	if ( (pVal == NULL) || (pVal->Type == XVO_DT_EMPTY) ) {
		return FALSE;
	} else if ( pVal->Type == XVO_DT_NULL ) {
		return FALSE;
	} else if ( pVal->Type == XVO_DT_BOOL ) {
		return pVal->vBool;
	} else if ( pVal->Type == XVO_DT_INT ) {
		return pVal->vInt != 0;
	} else if ( pVal->Type == XVO_DT_FLOAT ) {
		return pVal->vFloat != 0.0;
	} else {
		return TRUE;
	}
}
XXAPI int64 xvoGetInt(xvalue pVal)
{
	if ( (pVal == NULL) || (pVal->Type == XVO_DT_EMPTY) ) {
		return 0;
	} else if ( pVal->Type == XVO_DT_BOOL ) {
		return pVal->vBool ? 1 : 0;
	} else if ( pVal->Type == XVO_DT_INT ) {
		return pVal->vInt;
	} else if ( pVal->Type == XVO_DT_FLOAT ) {
		return pVal->vFloat;
	} else if ( pVal->Type == XVO_DT_TEXT ) {
		return xrtStrToI64(pVal->vText);
	} else {
		return 0;
	}
}
XXAPI double xvoGetFloat(xvalue pVal)
{
	if ( (pVal == NULL) || (pVal->Type == XVO_DT_EMPTY) ) {
		return 0.0;
	} else if ( pVal->Type == XVO_DT_BOOL ) {
		return pVal->vBool ? 1.0 : 0.0;
	} else if ( pVal->Type == XVO_DT_INT ) {
		return pVal->vInt;
	} else if ( pVal->Type == XVO_DT_FLOAT ) {
		return pVal->vFloat;
	} else if ( pVal->Type == XVO_DT_TEXT ) {
		return xrtStrToNum(pVal->vText);
	} else {
		return 0.0;
	}
}
XXAPI str xvoGetText(xvalue pVal)
{
	if ( (pVal == NULL) || (pVal->Type == XVO_DT_EMPTY) ) {
		return xCore.sNull;
	} else if ( pVal->Type == XVO_DT_TEXT ) {
		return pVal->vText;
	} else if ( pVal->Type == XVO_DT_BOOL ) {
		return (pVal->vBool ? "true" : "false");
	} else if ( pVal->Type == XVO_DT_INT ) {
		str sRet = xrtTempMemory(24);
		xrtI64ToStr(pVal->vInt, sRet);
		return sRet;
	} else if ( pVal->Type == XVO_DT_FLOAT ) {
		str sRet = xrtTempMemory(32);
		xrtNumToStr(pVal->vFloat, sRet);
		return sRet;
	} else if ( pVal->Type == XVO_DT_TIME ) {
		str sRet = xrtTempMemory(24);
		int64 iYear;
		int iMonth, iDay, iHour, iMinute, iSecond;
		xrtDecodeSerial(pVal->vTime, &iYear, &iMonth, &iDay, &iHour, &iMinute, &iSecond, NULL, NULL);
		sprintf(sRet, "%d-%02d-%02d %02d:%02d:%02d", iYear, iMonth, iDay, iHour, iMinute, iSecond);
		return sRet;
	} else if ( pVal->Type == XVO_DT_POINT ) {
		str sRet = xrtTempMemory(32);
		sprintf(sRet, "[point:%x]", pVal->vPoint);
		return sRet;
	} else if ( pVal->Type == XVO_DT_FUNC ) {
		str sRet = xrtTempMemory(32);
		sprintf(sRet, "[function:%x]", pVal->vFunc);
		return sRet;
	} else if ( pVal->Type == XVO_DT_ARRAY ) {
		str sRet = xrtTempMemory(32);
		sprintf(sRet, "[array:%x]", pVal->vArray);
		return sRet;
	} else if ( pVal->Type == XVO_DT_LIST ) {
		str sRet = xrtTempMemory(32);
		sprintf(sRet, "[list:%x]", pVal->vList);
		return sRet;
	} else if ( pVal->Type == XVO_DT_COLL ) {
		str sRet = xrtTempMemory(32);
		sprintf(sRet, "[coll:%x]", pVal->vColl);
		return sRet;
	} else if ( pVal->Type == XVO_DT_TABLE ) {
		str sRet = xrtTempMemory(32);
		sprintf(sRet, "[table:%x]", pVal->vTable);
		return sRet;
	} else if ( pVal->Type == XVO_DT_STRUCT ) {
		str sRet = xrtTempMemory(32);
		sprintf(sRet, "[struct:%x]", pVal->vStruct);
		return sRet;
	} else if ( pVal->Type == XVO_DT_OBJECT ) {
		str sRet = xrtTempMemory(32);
		sprintf(sRet, "[object:%x]", pVal->vObject);
		return sRet;
	} else if ( pVal->Type == XVO_DT_CUSTOM ) {
		str sRet = xrtTempMemory(32);
		sprintf(sRet, "[custom:%x]", pVal->vCustom);
		return sRet;
	} else {
		return xCore.sNull;
	}
}
XXAPI xtime xvoGetTime(xvalue pVal)
{
	if ( (pVal == NULL) || (pVal->Type == XVO_DT_EMPTY) ) {
		return 0;
	} else if ( pVal->Type == XVO_DT_TIME ) {
		return pVal->vTime;
	} else if ( pVal->Type == XVO_DT_TEXT ) {
		return 0;			// 未来应该支持字符串转换为 xtime
	} else {
		return 0;
	}
}
XXAPI ptr xvoGetPoint(xvalue pVal)
{
	if ( (pVal == NULL) || (pVal->Type == XVO_DT_EMPTY) ) {
		return NULL;
	} else if ( pVal->Type == XVO_DT_POINT ) {
		return pVal->vPoint;
	} else {
		return NULL;
	}
}
XXAPI xfunction xvoGetFunc(xvalue pVal)
{
	if ( (pVal == NULL) || (pVal->Type == XVO_DT_EMPTY) ) {
		return NULL;
	} else if ( pVal->Type == XVO_DT_FUNC ) {
		return pVal->vFunc;
	} else {
		return NULL;
	}
}
XXAPI xparray xvoGetArray(xvalue pVal)
{
	if ( (pVal == NULL) || (pVal->Type == XVO_DT_EMPTY) ) {
		return NULL;
	} else if ( pVal->Type == XVO_DT_ARRAY ) {
		return pVal->vArray;
	} else {
		return NULL;
	}
}
XXAPI xlist xvoGetList(xvalue pVal)
{
	if ( (pVal == NULL) || (pVal->Type == XVO_DT_EMPTY) ) {
		return NULL;
	} else if ( pVal->Type == XVO_DT_LIST ) {
		return pVal->vList;
	} else {
		return NULL;
	}
}
XXAPI xavltree xvoGetColl(xvalue pVal)
{
	if ( (pVal == NULL) || (pVal->Type == XVO_DT_EMPTY) ) {
		return NULL;
	} else if ( pVal->Type == XVO_DT_COLL ) {
		return pVal->vColl;
	} else {
		return NULL;
	}
}
XXAPI xdict xvoGetTable(xvalue pVal)
{
	if ( (pVal == NULL) || (pVal->Type == XVO_DT_EMPTY) ) {
		return NULL;
	} else if ( pVal->Type == XVO_DT_TABLE ) {
		return pVal->vTable;
	} else {
		return NULL;
	}
}
XXAPI ptr xvoGetStruct(xvalue pVal)
{
	if ( (pVal == NULL) || (pVal->Type == XVO_DT_EMPTY) ) {
		return NULL;
	} else if ( pVal->Type == XVO_DT_STRUCT ) {
		return pVal->vStruct;
	} else {
		return NULL;
	}
}
XXAPI ptr xvoGetObject(xvalue pVal)
{
	if ( (pVal == NULL) || (pVal->Type == XVO_DT_EMPTY) ) {
		return NULL;
	} else if ( pVal->Type == XVO_DT_OBJECT ) {
		return pVal->vObject;
	} else {
		return NULL;
	}
}
XXAPI ptr xvoGetCustom(xvalue pVal)
{
	if ( (pVal == NULL) || (pVal->Type == XVO_DT_EMPTY) ) {
		return NULL;
	} else if ( pVal->Type == XVO_DT_CUSTOM ) {
		return pVal->vCustom;
	} else {
		return NULL;
	}
}



// Array 读数据
XXAPI xvalue xvoArrayGetValue(xvalue pArr, uint32 index)
{
	if ( pArr == NULL ) {
		return &XVO_VALUE_EMPTY;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return &XVO_VALUE_EMPTY;
	}
	xvalue pVal = xrtPtrArrayGet(pArr->vArray, index + 1);
	if ( pVal ) {
		return pVal;
	} else {
		return &XVO_VALUE_EMPTY;
	}
}



// Array 追加数据
XXAPI bool xvoArrayAppendValue(xvalue pArr, xvalue pVal, bool bColloc)
{
	if ( (pArr || pVal) == 0 ) {
		return FALSE;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	uint32 index = xrtPtrArrayAppend(pArr->vArray, pVal);
	if ( index == 0 ) {
		return FALSE;
	}
	if ( (bColloc == FALSE) && (pVal->IsStatic == FALSE) ) {
		xvoAddRef_Inline(pVal);
	}
	return TRUE;
}



// Array 插入操作
XXAPI bool xvoArrayInsertValue(xvalue pArr, uint32 index, xvalue pVal, bool bColloc)
{
	if ( (pArr || pVal) == 0 ) {
		return FALSE;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	uint32 idx = xrtPtrArrayInsert(pArr->vArray, index, pVal);
	if ( idx == 0 ) {
		return FALSE;
	}
	if ( (bColloc == FALSE) && (pVal->IsStatic == FALSE) ) {
		xvoAddRef_Inline(pVal);
	}
	return TRUE;
}



// Array 修改操作
XXAPI bool xvoArraySetValue(xvalue pArr, uint32 index, xvalue pVal, bool bColloc)
{
	if ( (pArr || pVal) == 0 ) {
		return FALSE;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	xvalue pOldVal = xrtPtrArrayGet(pArr->vArray, index + 1);
	if ( pOldVal == NULL ) {
		return FALSE;
	}
	xvoUnref(pOldVal);
	xrtPtrArraySet_Inline(pArr->vArray, index + 1, pVal);
	if ( (bColloc == FALSE) && (pVal->IsStatic == FALSE) ) {
		xvoAddRef_Inline(pVal);
	}
	return TRUE;
}



// Array 合并
XXAPI bool xvoArrayMerge(xvalue pArr1, xvalue pArr2)
{
	if ( (pArr1 || pArr2) == 0 ) {
		return FALSE;
	}
	if ( pArr1->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	if ( pArr2->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	for ( int i = 1; i <= pArr2->vArray->Count; i++ ) {
		xvalue pVal = xrtPtrArrayGet_Inline(pArr2->vArray, i);
		xvoAddRef_Inline(pVal);
		xrtPtrArrayAppend(pArr1->vArray, pVal);
	}
	return TRUE;
}



// Array 操作
XXAPI bool xvoArraySwap(xvalue pArr, uint32 index1, uint32 index2)
{
	if ( pArr == NULL ) {
		return FALSE;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	return xrtPtrArraySwap(pArr->vArray, index1, index2);
}
XXAPI bool xvoArrayRemove(xvalue pArr, uint32 index, uint32 count)
{
	if ( pArr == NULL ) {
		return FALSE;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	return xrtPtrArrayRemove(pArr->vArray, index, count);
}
XXAPI uint32 xvoArrayItemCount(xvalue pArr)
{
	if ( pArr == NULL ) {
		return 0;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return 0;
	}
	return pArr->vArray->Count;
}
XXAPI bool xvoArrayClear(xvalue pArr)
{
	if ( pArr == NULL ) {
		return FALSE;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	for ( int i = 1; i <= pArr->vArray->Count; i++ ) {
		xvalue pVal = xrtPtrArrayGet_Inline(pArr->vArray, i);
		xvoUnref(pVal);
	}
	xrtPtrArrayClear(pArr->vArray);
	return TRUE;
}
XXAPI bool xvoArrayAlloc(xvalue pArr, uint32 count)
{
	if ( pArr == NULL ) {
		return FALSE;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	return xrtPtrArrayMalloc(pArr->vArray, count);
}
XXAPI bool xvoArraySort(xvalue pArr, ptr proc)
{
	if ( pArr == NULL ) {
		return FALSE;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	return xrtPtrArraySort(pArr->vArray, proc);
}



// List 读数据
XXAPI xvalue xvoListGetValue(xvalue pList, int64 index)
{
	if ( pList == NULL ) {
		return &XVO_VALUE_EMPTY;
	}
	if ( pList->Type != XVO_DT_LIST ) {
		return &XVO_VALUE_EMPTY;
	}
	xvalue pVal = xrtListGetPtr(pList->vList, index);
	if ( pVal ) {
		return pVal;
	} else {
		return &XVO_VALUE_EMPTY;
	}
}



// List 写数据
XXAPI bool xvoListSetValue(xvalue pList, int64 index, xvalue pVal, bool bColloc)
{
	if ( (pList || pVal) == 0 ) {
		return FALSE;
	}
	if ( pList->Type != XVO_DT_LIST ) {
		return FALSE;
	}
	xvalue pOldVal = NULL;
	bool bRet = xrtListSetPtr(pList->vList, index, pVal, (ptr*)&pOldVal);
	if ( bRet == FALSE ) {
		return FALSE;
	}
	if ( pOldVal ) {
		xvoUnref(pOldVal);
	}
	if ( (bColloc == FALSE) && (pVal->IsStatic == FALSE) ) {
		xvoAddRef_Inline(pVal);
	}
	return TRUE;
}



// List 合并
bool xvoListMerge_RefProc(int64 iKey, xvalue* ppVal, xlist objList)
{
	bool bNew = FALSE;
	xvalue* ppOldVal = xrtListSet(objList, iKey, &bNew);
	if ( ppOldVal ) {
		// 只转移之前没有的值
		if ( bNew ) {
			xvoAddRef_Inline(*ppVal);
			ppOldVal[0] = *ppVal;
		}
	}
	return FALSE;
}
bool xvoListMerge_RefProc_ReWrite(int64 iKey, xvalue* ppVal, xlist objList)
{
	xvalue pOldVal = NULL;
	int iRet = xrtListSetPtr(objList, iKey, *ppVal, (ptr*)&pOldVal);
	if ( iRet ) {
		xvoAddRef_Inline(*ppVal);
		// 释放旧值
		if ( pOldVal ) {
			xvoUnref(pOldVal);
		}
	}
	return FALSE;
}
XXAPI bool xvoListMerge(xvalue pList1, xvalue pList2, bool bReWrite)
{
	if ( (pList1 || pList2) == 0 ) {
		return FALSE;
	}
	if ( pList1->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	if ( pList2->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	if ( bReWrite ) {
		xrtListWalk(pList2->vList, (ptr)xvoListMerge_RefProc_ReWrite, pList1->vList);
	} else {
		xrtListWalk(pList2->vList, (ptr)xvoListMerge_RefProc, pList1->vList);
	}
	return TRUE;
}



// List 操作
XXAPI bool xvoListExists(xvalue pList, int64 index)
{
	if ( pList == NULL ) {
		return FALSE;
	}
	if ( pList->Type != XVO_DT_LIST ) {
		return FALSE;
	}
	return xrtListExists(pList->vList, index);
}
XXAPI bool xvoListRemove(xvalue pList, int64 index)
{
	if ( pList == NULL ) {
		return FALSE;
	}
	if ( pList->Type != XVO_DT_LIST ) {
		return FALSE;
	}
	xvalue pOldVal = xrtListRemovePtr(pList->vList, index);
	if ( pOldVal ) {
		xvoUnref(pOldVal);
		return TRUE;
	} else {
		return FALSE;
	}
}
XXAPI uint32 xvoListItemCount(xvalue pList)
{
	if ( pList == NULL ) {
		return 0;
	}
	if ( pList->Type != XVO_DT_LIST ) {
		return 0;
	}
	return xrtListCount(pList->vList);
}
XXAPI bool xvoListClear(xvalue pList)
{
	if ( pList == NULL ) {
		return FALSE;
	}
	if ( pList->Type != XVO_DT_LIST ) {
		return FALSE;
	}
	xrtListWalk(pList->vList, (ptr)xvoListClear_FreeProc, pList);
	xrtListClear(pList->vList);
	return TRUE;
}
XXAPI bool xvoListSetParent(xvalue pList, xvalue pParentList)
{
	if ( (pList || pParentList) == 0 ) {
		return FALSE;
	}
	if ( pList->Type != XVO_DT_TABLE ) {
		return FALSE;
	}
	if ( pParentList->Type != XVO_DT_TABLE ) {
		return FALSE;
	}
	pList->vList->AVLT.Parent = &pParentList->vList->AVLT;
	return TRUE;
}



// 集合功能实现 - 值对比函数
int Coll_CompProc(Coll_Key* pNode, Coll_Key* pObjKey)
{
	if ( pNode->Hash == pObjKey->Hash ) {
		if ( pNode->Value->Type == XVO_DT_NULL ) {
			return 0;
		} else if ( pNode->Value->Type == XVO_DT_BOOL ) {
			return 0;
		} else if ( pNode->Value->Type == XVO_DT_TEXT ) {
			if ( pNode->Value->Size == pObjKey->Value->Size ) {
				return strcmp(pNode->Value->vText, pObjKey->Value->vText);
			} else {
				if ( pNode->Value->Size > pObjKey->Value->Size ) {
					return -1;
				} else {
					return 1;
				}
			}
		} else {
			if ( pNode->Value->vInt > pObjKey->Value->vInt ) {
				return -1;
			} else {
				return 1;
			}
		}
	} else if ( pNode->Hash > pObjKey->Hash ) {
		return -1;
	} else {
		return 1;
	}
}



// Coll 写数据
XXAPI bool xvoCollSetValue(xvalue pColl, xvalue pVal, bool bColloc)
{
	if ( (pColl || pVal) == 0 ) {
		return FALSE;
	}
	if ( pColl->Type != XVO_DT_COLL ) {
		return FALSE;
	}
	Coll_Key objKey;
	MAKE_COLL_KEY(objKey, pVal);
	return xvoCollSetValueWithKey(pColl->vColl, &objKey, bColloc);
}



// Coll 获取差集 [ pSelf 集合相对 pColl 集合没有的元素 ]
struct CollProcParam {
	xvalue pColl;
	xvalue pRetVal;
};
bool xvoCollDifference_EachProc(Coll_Key* pKey, struct CollProcParam* param)
{
	Coll_Key* pNode = xrtAVLTreeSearch(param->pColl->vColl, pKey);
	if ( pNode == NULL ) {
		xvoCollSetValueWithKey(param->pRetVal->vColl, pKey, FALSE);
	}
	return FALSE;
}
XXAPI xvalue xvoCollDifference(xvalue pSelf, xvalue pColl)
{
	if ( (pSelf || pColl) == 0 ) {
		return &XVO_VALUE_EMPTY;
	}
	if ( pSelf->Type != XVO_DT_COLL ) {
		return &XVO_VALUE_EMPTY;
	}
	if ( pColl->Type != XVO_DT_COLL ) {
		return &XVO_VALUE_EMPTY;
	}
	xvalue pRetVal = xvoCreateColl();
	struct CollProcParam param = { pColl, pRetVal };
	xrtAVLTreeWalk(pSelf->vColl, (ptr)xvoCollDifference_EachProc, &param);
	return pRetVal;
}



// Coll 获取对称差集 [ 两个集合中不重复的元素 ]
XXAPI xvalue xvoCollSymmetricDifference(xvalue pSelf, xvalue pColl)
{
	if ( (pSelf || pColl) == 0 ) {
		return &XVO_VALUE_EMPTY;
	}
	if ( pSelf->Type != XVO_DT_COLL ) {
		return &XVO_VALUE_EMPTY;
	}
	if ( pColl->Type != XVO_DT_COLL ) {
		return &XVO_VALUE_EMPTY;
	}
	xvalue pRetVal = xvoCreateColl();
	struct CollProcParam param = { pColl, pRetVal };
	xrtAVLTreeWalk(pSelf->vColl, (ptr)xvoCollDifference_EachProc, &param);
	param.pColl = pSelf;
	xrtAVLTreeWalk(pColl->vColl, (ptr)xvoCollDifference_EachProc, &param);
	return pRetVal;
}



// Coll 获取交集 [ pSelf 集合相对 pColl 集合存在的元素 ]
bool xvoCollIntersection_EachProc(Coll_Key* pKey, struct CollProcParam* param)
{
	Coll_Key* pNode = xrtAVLTreeSearch(param->pColl->vColl, pKey);
	if ( pNode ) {
		xvoCollSetValueWithKey(param->pRetVal->vColl, pKey, FALSE);
	}
	return FALSE;
}
XXAPI xvalue xvoCollIntersection(xvalue pSelf, xvalue pColl)
{
	if ( (pSelf || pColl) == 0 ) {
		return &XVO_VALUE_EMPTY;
	}
	if ( pSelf->Type != XVO_DT_COLL ) {
		return &XVO_VALUE_EMPTY;
	}
	if ( pColl->Type != XVO_DT_COLL ) {
		return &XVO_VALUE_EMPTY;
	}
	xvalue pRetVal = xvoCreateColl();
	struct CollProcParam param = { pColl, pRetVal };
	xrtAVLTreeWalk(pSelf->vColl, (ptr)xvoCollIntersection_EachProc, &param);
	return pRetVal;
}



// Coll 获取并集 [ 合并两个集合，返回和一个新的集合 ]
bool xvoCollUnion_EachProc(Coll_Key* pKey, xavltree pColl)
{
	xvoCollSetValueWithKey(pColl, pKey, FALSE);
	return FALSE;
}
XXAPI xvalue xvoCollUnion(xvalue pSelf, xvalue pColl)
{
	if ( (pSelf || pColl) == 0 ) {
		return &XVO_VALUE_EMPTY;
	}
	if ( pSelf->Type != XVO_DT_COLL ) {
		return &XVO_VALUE_EMPTY;
	}
	if ( pColl->Type != XVO_DT_COLL ) {
		return &XVO_VALUE_EMPTY;
	}
	xvalue pRetVal = xvoCreateColl();
	xrtAVLTreeWalk(pSelf->vColl, (ptr)xvoCollUnion_EachProc, pRetVal->vColl);
	xrtAVLTreeWalk(pColl->vColl, (ptr)xvoCollUnion_EachProc, pRetVal->vColl);
	return pRetVal;
}



// Coll 合并集合 [ 将 pColl 中的元素并入 pSelf ]
XXAPI bool xvoCollMerge(xvalue pSelf, xvalue pColl)
{
	if ( (pSelf || pColl) == 0 ) {
		return FALSE;
	}
	if ( pSelf->Type != XVO_DT_COLL ) {
		return FALSE;
	}
	if ( pColl->Type != XVO_DT_COLL ) {
		return FALSE;
	}
	xrtAVLTreeWalk(pColl->vColl, (ptr)xvoCollUnion_EachProc, pSelf->vColl);
	return TRUE;
}



// Coll 操作
XXAPI bool xvoCollExists(xvalue pColl, xvalue pVal)
{
	if ( (pColl || pVal) == 0 ) {
		return FALSE;
	}
	if ( pColl->Type != XVO_DT_COLL ) {
		return FALSE;
	}
	Coll_Key objKey;
	MAKE_COLL_KEY(objKey, pVal);
	Coll_Key* pNode = xrtAVLTreeSearch(pColl->vColl, &objKey);
	if ( pNode ) {
		return TRUE;
	}
	return FALSE;
}
XXAPI bool xvoCollRemove(xvalue pColl, xvalue pVal)
{
	if ( (pColl || pVal) == 0 ) {
		return FALSE;
	}
	if ( pColl->Type != XVO_DT_COLL ) {
		return FALSE;
	}
	Coll_Key objKey;
	MAKE_COLL_KEY(objKey, pVal);
	xavltnode pDelNode = xrtAVLTB_Remove((xavltbase)pColl->vColl, pColl->vColl->CompProc, &objKey);
	if ( pDelNode ) {
		Coll_Key* pKeyPtr = xrtAVLTreeGetNodeData(pDelNode);
		xvoUnref(pKeyPtr->Value);
		return TRUE;
	}
	return FALSE;
}
XXAPI uint32 xvoCollItemCount(xvalue pColl)
{
	if ( pColl == NULL ) {
		return 0;
	}
	if ( pColl->Type != XVO_DT_COLL ) {
		return 0;
	}
	return pColl->vColl->Count;
}
XXAPI bool xvoCollClear(xvalue pColl)
{
	if ( pColl == NULL ) {
		return FALSE;
	}
	if ( pColl->Type != XVO_DT_COLL ) {
		return FALSE;
	}
	xrtAVLTreeWalk(pColl->vColl, (ptr)xvoCollClear_FreeProc, pColl);
	xrtAVLTreeClear(pColl->vColl);
	return TRUE;
}
XXAPI bool xvoCollSetParent(xvalue pColl, xvalue pParentColl)
{
	if ( (pColl || pParentColl) == 0 ) {
		return FALSE;
	}
	if ( pColl->Type != XVO_DT_TABLE ) {
		return FALSE;
	}
	if ( pParentColl->Type != XVO_DT_TABLE ) {
		return FALSE;
	}
	pColl->vColl->Parent = pParentColl->vColl;
	return TRUE;
}



// Table 读数据
XXAPI xvalue xvoTableGetValue(xvalue pTbl, str key, uint32 kl)
{
	if ( pTbl == NULL ) {
		return &XVO_VALUE_EMPTY;
	}
	if ( pTbl->Type != XVO_DT_TABLE ) {
		return &XVO_VALUE_EMPTY;
	}
	if ( key == NULL ) {
		key = xCore.sNull;
		kl = 0;
	} else if ( kl == 0 ) {
		kl = strlen(key);
	}
	xvalue pVal = xrtDictGetPtr(pTbl->vTable, key, kl);
	if ( pVal ) {
		return pVal;
	} else {
		return &XVO_VALUE_EMPTY;
	}
}



// Table 写数据
XXAPI bool xvoTableSetValue(xvalue pTbl, str key, uint32 kl, xvalue pVal, bool bColloc)
{
	if ( (pTbl || pVal) == 0 ) {
		return FALSE;
	}
	if ( pTbl->Type != XVO_DT_TABLE ) {
		return FALSE;
	}
	if ( key == NULL ) {
		key = xCore.sNull;
		kl = 0;
	} else if ( kl == 0 ) {
		kl = strlen(key);
	}
	xvalue pOldVal = NULL;
	int iRet = xrtDictSetPtr(pTbl->vTable, key, kl, pVal, (ptr*)&pOldVal);
	if ( iRet == FALSE ) {
		return FALSE;
	}
	if ( pOldVal ) {
		xvoUnref(pOldVal);
	}
	if ( (bColloc == FALSE) && (pVal->IsStatic == FALSE) ) {
		xvoAddRef_Inline(pVal);
	}
	return TRUE;
}



// Table 合并
bool xvoTableMerge_RefProc(Dict_Key* pKey, xvalue* ppVal, xdict objTbl)
{
	bool bNew;
	xvalue* ppOldVal = xrtDictSetWithKey(objTbl, pKey, &bNew);
	if ( ppOldVal ) {
		// 只转移之前没有的值
		if ( bNew ) {
			xvoAddRef_Inline(*ppVal);
			ppOldVal[0] = *ppVal;
		}
	}
	return FALSE;
}
bool xvoTableMerge_RefProc_ReWrite(Dict_Key* pKey, xvalue* ppVal, xdict objTbl)
{
	bool bNew = FALSE;
	xvalue* ppOldVal = xrtDictSetWithKey(objTbl, pKey, &bNew);
	if ( ppOldVal ) {
		// 释放旧值
		if ( bNew == FALSE ) {
			xvoUnref(*ppOldVal);
		}
		xvoAddRef_Inline(*ppVal);
		ppOldVal[0] = *ppVal;
	}
	return FALSE;
}
XXAPI bool xvoTableMerge(xvalue pTbl1, xvalue pTbl2, bool bReWrite)
{
	if ( (pTbl1 || pTbl2) == 0 ) {
		return FALSE;
	}
	if ( pTbl1->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	if ( pTbl2->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	if ( bReWrite ) {
		xrtDictWalk(pTbl2->vTable, (ptr)xvoTableMerge_RefProc_ReWrite, pTbl1->vTable);
	} else {
		xrtDictWalk(pTbl2->vTable, (ptr)xvoTableMerge_RefProc, pTbl1->vTable);
	}
	return TRUE;
}



// Table 操作
XXAPI bool xvoTableExists(xvalue pTbl, str key, uint32 kl)
{
	if ( pTbl == NULL ) {
		return FALSE;
	}
	if ( pTbl->Type != XVO_DT_TABLE ) {
		return FALSE;
	}
	return xrtDictExists(pTbl->vTable, key, kl);
}
XXAPI bool xvoTableRemove(xvalue pTbl, str key, uint32 kl)
{
	if ( pTbl == NULL ) {
		return FALSE;
	}
	if ( pTbl->Type != XVO_DT_TABLE ) {
		return FALSE;
	}
	xvalue pOldVal = xrtDictRemovePtr(pTbl->vTable, key, kl);
	if ( pOldVal ) {
		xvoUnref(pOldVal);
		return TRUE;
	} else {
		return FALSE;
	}
}
XXAPI uint32 xvoTableItemCount(xvalue pTbl)
{
	if ( pTbl == NULL ) {
		return 0;
	}
	if ( pTbl->Type != XVO_DT_TABLE ) {
		return 0;
	}
	return xrtDictCount(pTbl->vTable);
}
XXAPI bool xvoTableClear(xvalue pTbl)
{
	if ( pTbl == NULL ) {
		return FALSE;
	}
	if ( pTbl->Type != XVO_DT_TABLE ) {
		return FALSE;
	}
	xrtDictWalk(pTbl->vTable, (ptr)xvoTableClear_FreeProc, pTbl);
	xrtDictClear(pTbl->vTable);
	return TRUE;
}
XXAPI bool xvoTableSetParent(xvalue pTbl, xvalue pParentTable)
{
	if ( (pTbl || pParentTable) == 0 ) {
		return FALSE;
	}
	if ( pTbl->Type != XVO_DT_TABLE ) {
		return FALSE;
	}
	if ( pParentTable->Type != XVO_DT_TABLE ) {
		return FALSE;
	}
	pTbl->vTable->AVLT.Parent = &pParentTable->vTable->AVLT;
	return TRUE;
}



// 类型操作
XXAPI bool xvoIsNull(xvalue pVal)
{
	if ( pVal == NULL ) {
		return TRUE;
	} else if ( pVal->Type == XVO_DT_NULL ) {
		return TRUE;
	} else if ( pVal->Type == XVO_DT_EMPTY ) {
		return TRUE;
	} else {
		return FALSE;
	}
}
XXAPI int xvoType(xvalue pVal)
{
	if ( pVal == NULL ) {
		return XVO_DT_EMPTY;
	} else {
		return pVal->Type;
	}
}



// 获取数据长度
XXAPI uint32 xvoGetSize(xvalue pVal)
{
	if ( pVal == NULL ) {
		return 0;
	} else {
		return pVal->Size;
	}
}



// 浅拷贝
bool xvoCopy_ListProc(int64 iKey, xvalue* ppVal, xlist objList)
{
	if ( (ppVal[0]->Type >= XVO_DT_ARRAY) ) {
		// 复杂数据类型 - 直接引用
		xvoAddRef_Inline(ppVal[0]);
		xrtListSetPtr(objList, iKey, ppVal[0], NULL);
	} else {
		// 基础数据类型 - 创建新值
		xvalue pItemCopy = xvoCopy(ppVal[0]);
		xrtListSetPtr(objList, iKey, pItemCopy, NULL);
	}
	return FALSE;
}
bool xvoCopy_CollProc(Coll_Key* pKey, xavltree objColl)
{
	if ( (pKey->Value->Type >= XVO_DT_ARRAY) ) {
		// 复杂数据类型 - 直接引用
		xvoCollSetValueWithKey(objColl, pKey, FALSE);
	} else {
		// 基础数据类型 - 创建新值
		xvalue pItemCopy = xvoCopy(pKey->Value);
		Coll_Key k = { pKey->Hash, pItemCopy };
		xvoCollSetValueWithKey(objColl, &k, TRUE);
	}
	return FALSE;
}
bool xvoCopy_TableProc(Dict_Key* pKey, xvalue* ppVal, xdict objTbl)
{
	if ( (ppVal[0]->Type >= XVO_DT_ARRAY) ) {
		// 复杂数据类型 - 直接引用
		xvoAddRef_Inline(ppVal[0]);
		xvalue* ppNTV = xrtDictSetWithKey(objTbl, pKey, NULL);
		if ( ppNTV ) {
			ppNTV[0] = ppVal[0];
		}
	} else {
		// 基础数据类型 - 创建新值
		xvalue pItemCopy = xvoCopy(ppVal[0]);
		xvalue* ppNTV = xrtDictSetWithKey(objTbl, pKey, NULL);
		if ( ppNTV ) {
			ppNTV[0] = pItemCopy;
		}
	}
	return FALSE;
}
XXAPI xvalue xvoCopy(xvalue pVal)
{
	if ( (pVal == NULL) || (pVal->Type == XVO_DT_EMPTY) ) {
		return &XVO_VALUE_EMPTY;
	} else if ( pVal->Type == XVO_DT_NULL ) {
		return &XVO_VALUE_NULL;
	} else if ( pVal->Type == XVO_DT_BOOL ) {
		if ( pVal->vBool ) {
			return &XVO_VALUE_TRUE;
		} else {
			return &XVO_VALUE_FALSE;
		}
	} else if ( pVal->Type == XVO_DT_TEXT ) {
		return xvoCreateText(pVal->vText, pVal->Size, FALSE);
	} else if ( pVal->Type == XVO_DT_ARRAY ) {
		xvalue arrRet = xvoCreateArray();
		for ( int i = 1; i <= pVal->vArray->Count; i++ ) {
			xvalue pItem = xrtPtrArrayGet_Inline(pVal->vArray, i);
			if ( (pItem->Type >= XVO_DT_ARRAY) ) {
				// 复杂数据类型 - 直接引用
				xvoAddRef_Inline(pItem);
				xrtPtrArrayAppend(arrRet->vArray, pItem);
			} else {
				// 基础数据类型 - 创建新值
				xvalue pItemCopy = xvoCopy(pItem);
				xrtPtrArrayAppend(arrRet->vArray, pItem);
			}
		}
		return arrRet;
	} else if ( pVal->Type == XVO_DT_LIST ) {
		xvalue lstRet = xvoCreateList();
		xrtListWalk(pVal->vList, (ptr)xvoCopy_ListProc, lstRet->vList);
		return lstRet;
	} else if ( pVal->Type == XVO_DT_COLL ) {
		xvalue setRet = xvoCreateColl();
		xrtAVLTreeWalk(pVal->vColl, (ptr)xvoCopy_CollProc, setRet->vColl);
		return setRet;
	} else if ( pVal->Type == XVO_DT_TABLE ) {
		xvalue tblRet = xvoCreateTable();
		xrtDictWalk(pVal->vTable, (ptr)xvoCopy_TableProc, tblRet->vTable);
		return tblRet;
	} else if ( pVal->Type == XVO_DT_STRUCT ) {
		return NULL;
	} else if ( pVal->Type == XVO_DT_OBJECT ) {
		return NULL;
	} else if ( pVal->Type == XVO_DT_CUSTOM ) {
		return NULL;
	} else {
		// 其他类型直接 Copy 64 位数据
		xvalue varRet = xrtMalloc(sizeof(xvalue_struct));
		varRet->Type = pVal->Type;
		varRet->Reserve = 0;
		varRet->IsStatic = 0;
		varRet->RefCount = 1;
		varRet->Size = pVal->Size;
		varRet->vInt = pVal->vInt;
		return varRet;
	}
}



// 深拷贝
bool xvoDeepCopy_ListProc(int64 iKey, xvalue* ppVal, xlist objList)
{
	xvalue pItemCopy = xvoDeepCopy(ppVal[0]);
	xrtListSetPtr(objList, iKey, pItemCopy, NULL);
	return FALSE;
}
bool xvoDeepCopy_CollProc(Coll_Key* pKey, xavltree objColl)
{
	xvalue pItemCopy = xvoDeepCopy(pKey->Value);
	Coll_Key k = { pKey->Hash, pItemCopy };
	xvoCollSetValueWithKey(objColl, &k, TRUE);
	return FALSE;
}
bool xvoDeepCopy_TableProc(Dict_Key* pKey, xvalue* ppVal, xdict objTbl)
{
	xvalue pItemCopy = xvoDeepCopy(ppVal[0]);
	xvalue* ppNTV = xrtDictSetWithKey(objTbl, pKey, NULL);
	if ( ppNTV ) {
		ppNTV[0] = pItemCopy;
	}
	return FALSE;
}
XXAPI xvalue xvoDeepCopy(xvalue pVal)
{
	if ( (pVal == NULL) || (pVal->Type == XVO_DT_EMPTY) ) {
		return &XVO_VALUE_EMPTY;
	} else if ( pVal->Type == XVO_DT_NULL ) {
		return &XVO_VALUE_NULL;
	} else if ( pVal->Type == XVO_DT_BOOL ) {
		if ( pVal->vBool ) {
			return &XVO_VALUE_TRUE;
		} else {
			return &XVO_VALUE_FALSE;
		}
	} else if ( pVal->Type == XVO_DT_TEXT ) {
		return xvoCreateText(pVal->vText, pVal->Size, FALSE);
	} else if ( pVal->Type == XVO_DT_ARRAY ) {
		xvalue arrRet = xvoCreateArray();
		for ( int i = 1; i <= pVal->vArray->Count; i++ ) {
			xvalue pItem = xrtPtrArrayGet_Inline(pVal->vArray, i);
			xvalue pItemCopy = xvoDeepCopy(pItem);
			xrtPtrArrayAppend(arrRet->vArray, pItem);
		}
		return arrRet;
	} else if ( pVal->Type == XVO_DT_LIST ) {
		xvalue lstRet = xvoCreateList();
		xrtListWalk(pVal->vList, (ptr)xvoDeepCopy_ListProc, lstRet->vList);
		return lstRet;
	} else if ( pVal->Type == XVO_DT_COLL ) {
		xvalue setRet = xvoCreateColl();
		xrtAVLTreeWalk(pVal->vColl, (ptr)xvoDeepCopy_CollProc, setRet->vColl);
		return setRet;
	} else if ( pVal->Type == XVO_DT_TABLE ) {
		xvalue tblRet = xvoCreateTable();
		xrtDictWalk(pVal->vTable, (ptr)xvoDeepCopy_TableProc, tblRet->vTable);
		return tblRet;
	} else if ( pVal->Type == XVO_DT_STRUCT ) {
		return NULL;
	} else if ( pVal->Type == XVO_DT_OBJECT ) {
		return NULL;
	} else if ( pVal->Type == XVO_DT_CUSTOM ) {
		return NULL;
	} else {
		// 其他类型直接 Copy 64 位数据
		xvalue varRet = xrtMalloc(sizeof(xvalue_struct));
		varRet->Type = pVal->Type;
		varRet->Reserve = 0;
		varRet->IsStatic = 0;
		varRet->RefCount = 1;
		varRet->Size = pVal->Size;
		varRet->vInt = pVal->vInt;
		return varRet;
	}
}



// 输出 value 的结构和值
bool xvoPrintValue_TableItemProc(Dict_Key* pKey, xvalue* ppVal, int iLevel)
{
	xvoPrintValue(*ppVal, iLevel, 2, 0, pKey->Key);
	return FALSE;
}
bool xvoPrintValue_ListItemProc(int64 iKey, xvalue* ppVal, int iLevel)
{
	xvoPrintValue(*ppVal, iLevel, 1, iKey, NULL);
	return FALSE;
}
bool xvoPrintValue_CollItemProc(Coll_Key* pKey, int iLevel)
{
	xvoPrintValue(pKey->Value, iLevel, 0, 0, NULL);
	return FALSE;
}
XXAPI void xvoPrintValue(xvalue objVal, int iLevel, int iMode, int64 iKey, str sKey)
{
	for ( int i = 0; i < iLevel; i++ ) {
		printf("    ");
	}
	if ( iMode == 1 ) {
		// 输出数组元素
		if ( (objVal == NULL) || (objVal->Type == XVO_DT_EMPTY) ) {
			printf("(empty) %lld = (empty)\n", iKey);
		} else if ( objVal->Type == XVO_DT_NULL ) {
			printf("(null ) [%x] %lld = (null)\n", objVal, iKey);
		} else if ( objVal->Type == XVO_DT_BOOL ) {
			printf("(bool ) [%x] %lld = (%s)\n", objVal, iKey, xvoGetText(objVal));
		} else if ( objVal->Type == XVO_DT_INT ) {
			printf("( int ) [%x] %lld = %lld\n", objVal, iKey, xvoGetInt(objVal));
		} else if ( objVal->Type == XVO_DT_FLOAT ) {
			printf("(float) [%x] %lld = %lf\n", objVal, iKey, xvoGetFloat(objVal));
		} else if ( objVal->Type == XVO_DT_TEXT ) {
			printf("(text ) [%x] %lld = \"%s\"\n", objVal, iKey, xvoGetText(objVal));
		} else if ( objVal->Type == XVO_DT_TIME ) {
			printf("(time ) [%x] %lld = < %s >\n", objVal, iKey, xvoGetText(objVal));
		} else if ( objVal->Type == XVO_DT_POINT ) {
			printf("(point) [%x] %lld = 0x%x\n", objVal, iKey, xvoGetPoint(objVal));
		} else if ( objVal->Type == XVO_DT_FUNC ) {
			printf("(func ) [%x] %lld = address:0x%x\n", objVal, iKey, xvoGetFunc(objVal));
		} else if ( objVal->Type == XVO_DT_ARRAY ) {
			printf("(array) [%x] %lld = (array), count : %d\n", objVal, iKey, xvoArrayItemCount(objVal));
		} else if ( objVal->Type == XVO_DT_LIST ) {
			printf("(list ) [%x] %lld = (list), count : %d\n", objVal, iKey, xvoListItemCount(objVal));
		} else if ( objVal->Type == XVO_DT_TABLE ) {
			printf("(table) [%x] %lld = (table), count : %d\n", objVal, iKey, xvoTableItemCount(objVal));
		} else if ( objVal->Type == XVO_DT_COLL ) {
			printf("(coll ) [%x] %lld = (coll), count : %d\n", objVal, iKey, xvoCollItemCount(objVal));
		} else if ( objVal->Type == XVO_DT_STRUCT ) {
			printf("(struc) [%x] %lld = (struct), size : %d\n", objVal, iKey, objVal->Size);
		} else {
			printf("Unknown data type\n");
		}
	} else if ( iMode == 2 ) {
		// 输出表元素
		if ( (objVal == NULL) || (objVal->Type == XVO_DT_EMPTY) ) {
			printf("(empty) \"%s\" = (empty)\n", sKey);
		} else if ( objVal->Type == XVO_DT_NULL ) {
			printf("(null ) [%x] \"%s\" = (null)\n", objVal, sKey);
		} else if ( objVal->Type == XVO_DT_BOOL ) {
			printf("(bool ) [%x] \"%s\" = (%s)\n", objVal, sKey, xvoGetText(objVal));
		} else if ( objVal->Type == XVO_DT_INT ) {
			printf("( int ) [%x] \"%s\" = %lld\n", objVal, sKey, xvoGetInt(objVal));
		} else if ( objVal->Type == XVO_DT_FLOAT ) {
			printf("(float) [%x] \"%s\" = %lf\n", objVal, sKey, xvoGetFloat(objVal));
		} else if ( objVal->Type == XVO_DT_TEXT ) {
			printf("(text ) [%x] \"%s\" = \"%s\"\n", objVal, sKey, xvoGetText(objVal));
		} else if ( objVal->Type == XVO_DT_TIME ) {
			printf("(time ) [%x] \"%s\" = < %s >\n", objVal, sKey, xvoGetText(objVal));
		} else if ( objVal->Type == XVO_DT_POINT ) {
			printf("(point) [%x] \"%s\" = 0x%x\n", objVal, sKey, xvoGetPoint(objVal));
		} else if ( objVal->Type == XVO_DT_FUNC ) {
			printf("(func ) [%x] \"%s\" = address:0x%x\n", objVal, sKey, xvoGetFunc(objVal));
		} else if ( objVal->Type == XVO_DT_ARRAY ) {
			printf("(array) [%x] \"%s\" = (array), count : %d\n", objVal, sKey, xvoArrayItemCount(objVal));
		} else if ( objVal->Type == XVO_DT_LIST ) {
			printf("(list ) [%x] \"%s\" = (list), count : %d\n", objVal, sKey, xvoListItemCount(objVal));
		} else if ( objVal->Type == XVO_DT_TABLE ) {
			printf("(table) [%x] \"%s\" = (table), count : %d\n", objVal, sKey, xvoTableItemCount(objVal));
		} else if ( objVal->Type == XVO_DT_COLL ) {
			printf("(coll ) [%x] \"%s\" = (coll), count : %d\n", objVal, sKey, xvoCollItemCount(objVal));
		} else if ( objVal->Type == XVO_DT_STRUCT ) {
			printf("(struc) [%x] \"%s\" = (struct), size : %d\n", objVal, sKey, objVal->Size);
		} else {
			printf("Unknown data type\n");
		}
	} else {
		// 输出元素
		if ( (objVal == NULL) || (objVal->Type == XVO_DT_EMPTY) ) {
			printf("(empty)\n");
		} else if ( objVal->Type == XVO_DT_NULL ) {
			printf("(null ) [%x] (null)\n", objVal);
		} else if ( objVal->Type == XVO_DT_BOOL ) {
			printf("(bool ) [%x] (%s)\n", objVal, xvoGetText(objVal));
		} else if ( objVal->Type == XVO_DT_INT ) {
			printf("( int ) [%x] %lld\n", objVal, xvoGetInt(objVal));
		} else if ( objVal->Type == XVO_DT_FLOAT ) {
			printf("(float) [%x] %lf\n", objVal, xvoGetFloat(objVal));
		} else if ( objVal->Type == XVO_DT_TEXT ) {
			printf("(text ) [%x] \"%s\"\n", objVal, xvoGetText(objVal));
		} else if ( objVal->Type == XVO_DT_TIME ) {
			printf("(time ) [%x] < %s >\n", objVal, xvoGetText(objVal));
		} else if ( objVal->Type == XVO_DT_POINT ) {
			printf("(point) [%x] 0x%x\n", objVal, xvoGetPoint(objVal));
		} else if ( objVal->Type == XVO_DT_FUNC ) {
			printf("(func ) [%x] address:0x%x\n", objVal, xvoGetFunc(objVal));
		} else if ( objVal->Type == XVO_DT_ARRAY ) {
			printf("(array) [%x] (array), count : %d\n", objVal, xvoArrayItemCount(objVal));
		} else if ( objVal->Type == XVO_DT_LIST ) {
			printf("(list ) [%x] (list), count : %d\n", objVal, xvoListItemCount(objVal));
		} else if ( objVal->Type == XVO_DT_TABLE ) {
			printf("(table) [%x] (table), count : %d\n", objVal, xvoTableItemCount(objVal));
		} else if ( objVal->Type == XVO_DT_COLL ) {
			printf("(coll ) [%x] (coll), count : %d\n", objVal, xvoCollItemCount(objVal));
		} else if ( objVal->Type == XVO_DT_STRUCT ) {
			printf("(struc) [%x] (struct), size : %d\n", objVal, objVal->Size);
		} else {
			printf("Unknown data type : %d\n", objVal->Type);
		}
	}
	if ( objVal ) {
		if ( objVal->Type == XVO_DT_ARRAY ) {
			for ( int64 i = 0; i < objVal->vArray->Count; i++ ) {
				xvalue objItem = xvoArrayGetValue(objVal, i);
				xvoPrintValue(objItem, iLevel + 1, 1, i, NULL);
			}
		} else if ( objVal->Type == XVO_DT_LIST ) {
			xrtListWalk(objVal->vList, (ptr)xvoPrintValue_ListItemProc, (ptr)(intptr_t)(iLevel+1));
		} else if ( objVal->Type == XVO_DT_TABLE ) {
			xrtDictWalk(objVal->vTable, (ptr)xvoPrintValue_TableItemProc, (ptr)(intptr_t)(iLevel+1));
		} else if ( objVal->Type == XVO_DT_COLL ) {
			xrtAVLTreeWalk(objVal->vColl, (ptr)xvoPrintValue_CollItemProc, (ptr)(intptr_t)(iLevel+1));
		}
	}
}


