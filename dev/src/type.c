


#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>




#include "type.h"
#include "smm.h"
#include "pmm.h"



// 预创建值（null、true、false）
xValueStruct xValue_Null = {XLVT_VARIANT | XLVMT_NULL, 0};
xValueStruct xValue_True = {XLVT_VARIANT | XLVMT_BOOL | XLVT_BOOL_TRUE, 0};
xValueStruct xValue_False = {XLVT_VARIANT | XLVMT_BOOL | XLVT_BOOL_FALSE, 0};



// xValue 管理器单元对象数据结构
typedef struct {
	u8* arrValue;									// 值数组（一个管理器能管理 256 个对象）
	u8 arrFreeIdx[256];								// 管理器存储已释放内存位置的数组
	u32 iValueSize;									// 管理器内单个对象的大小
	u16 iValueCount;								// 管理器内值的数量
	u8 iFreeCount;									// 管理器中记录了多少个已释放的对象
	u8 iFreeOffset;									// 当前已释放内存位置偏移
	u32 iValueFlag;									// 用于自动设置变量的 Flag（由全局管理器下发）
	u32 noused;										// 预留未使用
} xValueManageUnitStruct, *xValueManageUnit;

// xValue 管理器对象数据结构
typedef struct {
	PmmStruct vm16;
	PmmStruct vm24;
	PmmStruct vm40;
} xValueManageStruct, *xValueManage;





// 创建 xValue 管理器单元
xValueManageUnit xVMU_Create(u32 iSize, u32 iFlag)
{
	xValueManageUnit objUnit = malloc(sizeof(xValueManageUnitStruct) + (256 * iSize));
	objUnit->iValueSize = iSize;
	objUnit->arrValue = (ptr)&objUnit[1];
	objUnit->iValueCount = 0;
	objUnit->iFreeCount = 0;
	objUnit->iFreeOffset = 0;
	objUnit->iValueFlag = iFlag;
	return objUnit;
}

// 销毁 xValue 管理器单元
void xVMU_Destroy(xValueManageUnit objUnit)
{
	if ( objUnit ) {
		free(objUnit);
	}
}

// 获取指定位置的 xValue
xValue xVMU_GetValue(xValueManageUnit objUnit, u8 idx)
{
	return (xValue)&objUnit->arrValue[objUnit->iValueSize * idx];
}

// 从 xValue 管理器单元中申请一个变量
xValue xVMU_Alloc(xValueManageUnit objUnit)
{
	xValue pVal;
	u8 idx;
	if ( objUnit->iFreeCount > 0 ) {
		// 优先使用已释放的区块
		idx = objUnit->arrFreeIdx[objUnit->iFreeOffset & 0xFF];
		objUnit->iFreeOffset++;
		objUnit->iFreeCount--;
		objUnit->iValueCount++;
	} else {
		// 使用新的区块
		idx = objUnit->iValueCount;
		objUnit->iValueCount++;
	}
	pVal = (xValue)&objUnit->arrValue[objUnit->iValueSize * idx];
	pVal->Flag = objUnit->iValueFlag | idx;
	return pVal;
}

// 将 xValue 管理器单元中的某个变量释放
void xVMU_Free(xValueManageUnit objUnit, xValue pVal)
{
	u8 idx = pVal->Flag & XVM_MASK_INDEX;
	pVal->Type = 0;
	pVal->Flag = 0;
	objUnit->arrFreeIdx[(objUnit->iFreeOffset + objUnit->iFreeCount) & 0xFF] = idx;
	objUnit->iFreeCount++;
	objUnit->iValueCount--;
}
void xVMU_Free24(xValueManageUnit objUnit, xValue24 pVal)
{
	u8 idx = pVal->Flag & XVM_MASK_INDEX;
	pVal->Type = 0;
	pVal->Flag = 0;
	if ( pVal->pVal.sValA ) {
		free(pVal->pVal.sValA);
	}
	objUnit->arrFreeIdx[(objUnit->iFreeOffset + objUnit->iFreeCount) & 0xFF] = idx;
	objUnit->iFreeCount++;
	objUnit->iValueCount--;
}





// 创建 xValue 管理器
xValueManage xVM_Create()
{
	xValueManage xVM = malloc(sizeof(xValueManageStruct));
	xVM->vm16.Memory = NULL;
	xVM->vm16.Count = 0;
	xVM->vm16.AllocCount = 0;
	xVM->vm24.Memory = NULL;
	xVM->vm24.Count = 0;
	xVM->vm24.AllocCount = 0;
	xVM->vm40.Memory = NULL;
	xVM->vm40.Count = 0;
	xVM->vm40.AllocCount = 0;
}

// 销毁 xValue 管理器
void xVM_Destroy(xValueManage objVM)
{
	if ( objVM ) {
		for ( int i = 0; i < objVM->vm16.Count; i++ ) {
			xValueManageUnit objVMU = objVM->vm16.Memory[i];
			if ( objVMU ) {
				free(objVMU);
			}
		}
		for ( int i = 0; i < objVM->vm24.Count; i++ ) {
			xValueManageUnit objVMU = objVM->vm24.Memory[i];
			if ( objVMU ) {
				// xValue24 需要额外的内存释放
				for ( int count = 0, idx = 0; count < objVMU->iValueCount; count++, idx++ ) {
					xValue24 v = (xValue24)xVMU_GetValue(objVMU, idx);
					if ( v->Type == 0 ) {
						count--;
						continue;
					}
					if ( v->pVal.sValA ) {
						free(v->pVal.sValA);
					}
				}
				free(objVMU);
			}
		}
		for ( int i = 0; i < objVM->vm40.Count; i++ ) {
			xValueManageUnit objVMU = objVM->vm40.Memory[i];
			if ( objVMU ) {
				free(objVMU);
			}
		}
		PMM_ReInit(&objVM->vm16);
		PMM_ReInit(&objVM->vm24);
		PMM_ReInit(&objVM->vm40);
		free(objVM);
	}
}

// 从 xValue 管理器中申请一个 16 字节变量（失败返回 null）
xValue xVM_Alloc16(xValueManage objVM)
{
	xValueManageUnit objUnit = NULL;
	// 获取一个空闲的管理器单元
	for ( int i = 0; i < objVM->vm16.Count; i++ ) {
		xValueManageUnit objVMU = objVM->vm16.Memory[i];		// 直接从偏移取出指针
		if ( objVMU->iValueCount < 256 ) {
			objUnit = objVMU;
			break;
		}
	}
	// 如果没有空闲的管理器单元，则创建一个新的管理器单元
	if ( objUnit == NULL ) {
		objUnit = xVMU_Create(sizeof(xValueStruct16), XVM_TYPE_VAL16 | (objVM->vm16.Count << 8));
		if ( PMM_Append(&objVM->vm16, objUnit) == 0 ) {
			// 管理器单元无法获取和申请，报错
			printf("error : 变量内存空间申请失败！");
		}
	}
	// 从管理器单元中取值
	return xVMU_Alloc(objUnit);
}

// 从 xValue 管理器中申请一个 24 字节变量（失败返回 null）
xValue xVM_Alloc24(xValueManage objVM)
{
	xValueManageUnit objUnit = NULL;
	// 获取一个空闲的管理器单元
	for ( int i = 0; i < objVM->vm24.Count; i++ ) {
		xValueManageUnit objVMU = objVM->vm24.Memory[i];		// 直接从偏移取出指针
		if ( objVMU->iValueCount < 256 ) {
			objUnit = objVMU;
			break;
		}
	}
	// 如果没有空闲的管理器单元，则创建一个新的管理器单元
	if ( objUnit == NULL ) {
		objUnit = xVMU_Create(sizeof(xValueStruct24), XVM_TYPE_VAL24 | (objVM->vm24.Count << 8));
		if ( PMM_Append(&objVM->vm24, objUnit) == 0 ) {
			// 管理器单元无法获取和申请，报错
			printf("error : 变量内存空间申请失败！");
		}
	}
	// 从管理器单元中取值
	return xVMU_Alloc(objUnit);
}

// 从 xValue 管理器中申请一个 40 字节变量（失败返回 null）
xValue xVM_Alloc40(xValueManage objVM)
{
	xValueManageUnit objUnit = NULL;
	// 获取一个空闲的管理器单元
	for ( int i = 0; i < objVM->vm40.Count; i++ ) {
		xValueManageUnit objVMU = objVM->vm40.Memory[i];		// 直接从偏移取出指针
		if ( objVMU->iValueCount < 256 ) {
			objUnit = objVMU;
			break;
		}
	}
	// 如果没有空闲的管理器单元，则创建一个新的管理器单元
	if ( objUnit == NULL ) {
		objUnit = xVMU_Create(sizeof(xValueStruct40), XVM_TYPE_VAL40 | (objVM->vm40.Count << 8));
		if ( PMM_Append(&objVM->vm40, objUnit) == 0 ) {
			// 管理器单元无法获取和申请，报错
			printf("error : 变量内存空间申请失败！");
		}
	}
	// 从管理器单元中取值
	return xVMU_Alloc(objUnit);
}

// 从 xValue 管理器中申请一个变量
xValue xVM_Alloc(xValueManage objVM, u32 iType)
{
	int iSize = 16;
	int iMainType = iType & XLVT_MASK_MAINTYPE;
	if ( iMainType == XLVMT_NUMBER ) {
		int iSubType = iType & XLVT_MASK_SUBTYPE;
		if ( (iSubType == XLVT_I128) || (iSubType == XLVT_U128) || (iSubType == XLVT_F128) || (iSubType == XLVT_LONGCURR) || (iSubType == XLVT_COMPLEXF64) ) {
			iSize = 24;
		} else if ( iSubType == XLVT_COMPLEXF128 ) {
			iSize = 40;
		}
	} else if ( (iMainType == XLVMT_STRING) || (iMainType == XLVMT_BINARY) || (iMainType == XLVMT_POINT) || (iMainType == XLVMT_CUSTOM) ) {
		iSize = 24;
	} else if ( iMainType == XLVMT_BOOL ) {
		if ( iType & XLVT_MASK_BOOL ) {
			return &xValue_True;
		} else {
			return &xValue_False;
		}
	} else if ( iMainType == XLVMT_NULL ) {
		return &xValue_Null;
	}
	// 根据长度调用不同的申请函数
	xValue objVal = NULL;
	if ( iSize == 16 ) {
		objVal = xVM_Alloc16(objVM);
	} else if ( iSize == 24 ) {
		objVal = xVM_Alloc24(objVM);
	} else if ( iSize == 40 ) {
		objVal = xVM_Alloc40(objVM);
	}
	objVal->Type = iType;
	return objVal;
}

// 确定类型的值申请
xValue xVM_NewI64(xValueManage objVM, i64 iVal)
{
	xValue16 objVal = (xValue16)xVM_Alloc16(objVM);
	objVal->Type = XLVT_VARIANT | XLVMT_NUMBER | XLVT_NUM_INT | XLVT_I64;
	objVal->iVal64 = iVal;
	return (xValue)objVal;
}
xValue xVM_NewU64(xValueManage objVM, u64 iVal)
{
	xValue16 objVal = (xValue16)xVM_Alloc16(objVM);
	objVal->Type = XLVT_VARIANT | XLVMT_NUMBER | XLVT_NUM_INT | XLVT_U64;
	objVal->uVal64 = iVal;
	return (xValue)objVal;
}
xValue xVM_NewF64(xValueManage objVM, f64 fVal)
{
	xValue16 objVal = (xValue16)xVM_Alloc16(objVM);
	objVal->Type = XLVT_VARIANT | XLVMT_NUMBER | XLVT_NUM_FLOAT | XLVT_F64;
	objVal->fVal64 = fVal;
	return (xValue)objVal;
}
xValue xVM_NewCurr(xValueManage objVM, curr fVal)
{
	xValue16 objVal = (xValue16)xVM_Alloc16(objVM);
	objVal->Type = XLVT_VARIANT | XLVMT_NUMBER | XLVT_NUM_CURR | XLVT_CURR;
	objVal->currVal64 = fVal;
	return (xValue)objVal;
}
xValue xVM_NewStrA(xValueManage objVM, astr sVal, u32 Flag)
{
	xValue24 objVal = (xValue24)xVM_Alloc24(objVM);
	objVal->Type = XLVT_VARIANT | XLVMT_STRING | XLVT_ASTR;
	objVal->pVal.Length = strlen(sVal);
	objVal->pVal.Flag = Flag;
	objVal->pVal.sValA = sVal;
	return (xValue)objVal;
}

// 释放值内存
void xVM_Free(xValueManage objVM, xValue pVal)
{
	if ( (pVal->Flag & XVM_MASK_TYPE) == XVM_TYPE_VAL16 ) {
		u32 idx = (pVal->Flag & XVM_MASK_MUID) >> 8;
		if ( idx < objVM->vm16.Count ) {
			xValueManageUnit objVMU = objVM->vm16.Memory[idx];
			xVMU_Free(objVMU, pVal);
		} else {
			// 释放变量指定了不合法的管理器单元编号
			printf("error : 释放变量指定了不合法的管理器单元编号！");
		}
	} else if ( (pVal->Flag & XVM_MASK_TYPE) == XVM_TYPE_VAL24 ) {
		u32 idx = (pVal->Flag & XVM_MASK_MUID) >> 8;
		if ( idx < objVM->vm24.Count ) {
			xValueManageUnit objVMU = objVM->vm24.Memory[idx];
			xVMU_Free24(objVMU, (xValue24)pVal);
		} else {
			// 释放变量指定了不合法的管理器单元编号
			printf("error : 释放变量指定了不合法的管理器单元编号！");
		}
	} else if ( (pVal->Flag & XVM_MASK_TYPE) == XVM_TYPE_VAL40 ) {
		u32 idx = (pVal->Flag & XVM_MASK_MUID) >> 8;
		if ( idx < objVM->vm40.Count ) {
			xValueManageUnit objVMU = objVM->vm40.Memory[idx];
			xVMU_Free(objVMU, pVal);
		} else {
			// 释放变量指定了不合法的管理器单元编号
			printf("error : 释放变量指定了不合法的管理器单元编号！");
		}
	} else {
		// 未知的变量内存管理器分类
		printf("error : 未知的变量内存管理器分类！");
	}
}










void xVMU_PrintValue(xValue v)
{
	int mt = xVar_MainType(v);
	int st = xVar_SubType(v);
	int nt = xVar_NumType(v);
	printf("%d\t", mt);
	if ( st < 0x10000000 ) {
		printf("%x\t\t", st);
	} else {
		printf("%x\t", st);
	}
	if ( mt == XLVMT_NUMBER ) {
		printf("%x\t", nt);
	} else {
		printf("-\t");
	}
	if ( v->Flag < 0x10000000 ) {
		printf("%x\t\t", v->Flag);
	} else {
		printf("%x\t", v->Flag);
	}
	if ( mt == XLVMT_NULL ) {
		printf("null\n");
	} else if ( mt == XLVMT_BOOL ) {
		if ( v->Type & XLVT_MASK_BOOL ) {
			printf("bool\ttrue\n");
		} else {
			printf("bool\tfalse\n");
		}
	} else if ( mt == XLVMT_NUMBER ) {
		if ( nt == XLVT_NUM_INT ) {
			printf("int\t%d\n", ((xValue16)v)->iVal64);
		} else if ( nt == XLVT_NUM_FLOAT ) {
			printf("float\t%f\n", ((xValue16)v)->fVal64);
		} else if ( nt == XLVT_NUM_CURR ) {
			printf("curr\t%d\n", ((xValue16)v)->iVal64);
		} else if ( nt == XLVT_NUM_COMPLEX ) {
			printf("complex\n");
		}
	} else if ( mt == XLVMT_STRING ) {
		printf("string\t%s\n", ((xValue24)v)->pVal.sValA);
	} else if ( mt == XLVMT_BINARY ) {
		printf("binary\t\n");
	} else if ( mt == XLVMT_TIME ) {
		printf("time\t%f\n", ((xValue16)v)->tVal);
	} else if ( mt == XLVMT_COLLECT ) {
		printf("collect\n");
	} else if ( mt == XLVMT_ARRAY ) {
		printf("array\n");
	} else if ( mt == XLVMT_STRUCT ) {
		printf("struct\n");
	} else if ( mt == XLVMT_TABLE ) {
		printf("table\n");
	} else if ( mt == XLVMT_OBJECT ) {
		printf("object\n");
	} else if ( mt == XLVMT_FUNCTION ) {
		printf("funct\n");
	} else if ( mt == XLVMT_POINT ) {
		printf("point\t%d\n", ((xValue24)v)->pVal.ptrVal);
	} else if ( mt == XLVMT_CARRAY ) {
		printf("carray\n");
	} else if ( mt == XLVMT_CUSTOM ) {
		printf("custom\n");
	} else if ( mt == XLVMT_EMPTY ) {
		printf("empty\n");
	} else {
		printf("error : Unknown datatype\n");
	}
}
void xVMU_DebugPrint(xValueManageUnit objVMU)
{
	printf("objVMU Point : %d\narrValue Point : %d\nValueSize : %d\niValueFlag : %d\n\n", objVMU, objVMU->arrValue, objVMU->iValueSize, objVMU->iValueFlag);
	printf("ValueCount : %d , List :\n\n", objVMU->iValueCount);
	printf("id\tmt\tst\t\tnt\tflag\t\ttype\tval\n");
	for ( int i = 0, idx = 0; i < objVMU->iValueCount; i++, idx++ ) {
		xValue v = xVMU_GetValue(objVMU, idx);
		if ( v->Type == 0 ) {
			i--;
			continue;
		}
		printf("%d\t", idx);
		xVMU_PrintValue(v);
	}
	printf("\nFreeCount : %d ( FreeOffset : %d ), List :\n\n", objVMU->iFreeCount, objVMU->iFreeOffset);
	printf("pos\tidx\tmt\tst\t\tnt\tflag\t\ttype\tval\n");
	for ( int i = 0; i < objVMU->iFreeCount; i++ ) {
		int idx = objVMU->arrFreeIdx[(objVMU->iFreeOffset + i) & 0xFF];
		printf("%d\t%d\t", i, idx);
		xValue v = xVMU_GetValue(objVMU, idx);
		xVMU_PrintValue(v);
	}
}

void xVMU_Test()
{
	xValueManageUnit objVMU = xVMU_Create(sizeof(xValueStruct16), 0);
	
	// null
	xValue v0 = xVMU_Alloc(objVMU);
	v0->Type = XLVT_VARIANT | XLVMT_NULL;
	
	// true
	xValue v1 = xVMU_Alloc(objVMU);
	v1->Type = XLVT_VARIANT | XLVMT_BOOL | XLVT_BOOL_TRUE;
	
	// false
	xValue v2 = xVMU_Alloc(objVMU);
	v2->Type = XLVT_VARIANT | XLVMT_BOOL | XLVT_BOOL_FALSE;
	
	// int
	xValue16 v3 = (xValue16)xVMU_Alloc(objVMU);
	v3->Type = XLVT_VARIANT | XLVMT_NUMBER | XLVT_NUM_INT | XLVT_I64;
	v3->iVal64 = 12345678;
	
	// double
	xValue16 v4 = (xValue16)xVMU_Alloc(objVMU);
	v4->Type = XLVT_VARIANT | XLVMT_NUMBER | XLVT_NUM_FLOAT | XLVT_F64;
	v4->fVal64 = 3.141592654;
	
	// curr
	xValue16 v5 = (xValue16)xVMU_Alloc(objVMU);
	v5->Type = XLVT_VARIANT | XLVMT_NUMBER | XLVT_NUM_CURR | XLVT_CURR;
	v5->iVal64 = 99990000;
	
	// time
	xValue16 v6 = (xValue16)xVMU_Alloc(objVMU);
	v6->Type = XLVT_VARIANT | XLVMT_TIME;
	v6->fVal64 = 3.141592654;
	
	// null
	xValue v7 = xVMU_Alloc(objVMU);
	v7->Type = XLVT_VARIANT | XLVMT_NULL;
	
	// true
	xValue v8 = xVMU_Alloc(objVMU);
	v8->Type = XLVT_VARIANT | XLVMT_BOOL | XLVT_BOOL_TRUE;
	
	// false
	xValue v9 = xVMU_Alloc(objVMU);
	v9->Type = XLVT_VARIANT | XLVMT_BOOL | XLVT_BOOL_FALSE;
	
	// int
	xValue16 v10 = (xValue16)xVMU_Alloc(objVMU);
	v10->Type = XLVT_VARIANT | XLVMT_NUMBER | XLVT_NUM_INT | XLVT_I64;
	v10->iVal64 = 87654321;
	
	// double
	xValue16 v11 = (xValue16)xVMU_Alloc(objVMU);
	v11->Type = XLVT_VARIANT | XLVMT_NUMBER | XLVT_NUM_FLOAT | XLVT_F64;
	v11->fVal64 = 3.141592654;
	
	// curr
	xValue16 v12 = (xValue16)xVMU_Alloc(objVMU);
	v12->Type = XLVT_VARIANT | XLVMT_NUMBER | XLVT_NUM_CURR | XLVT_CURR;
	v12->iVal64 = 61734523;
	
	xVMU_Free(objVMU, (xValue)v3);
	xVMU_Free(objVMU, (xValue)v11);
	xVMU_Free(objVMU, (xValue)v8);
	
	// time
	xValue16 v13 = (xValue16)xVMU_Alloc(objVMU);
	v13->Type = XLVT_VARIANT | XLVMT_TIME;
	v13->fVal64 = 3.141592654;
	
	xVMU_Free(objVMU, (xValue)v0);
	
	// false
	xValue v14 = xVMU_Alloc(objVMU);
	v14->Type = XLVT_VARIANT | XLVMT_BOOL | XLVT_BOOL_FALSE;
	
	// int
	xValue16 v15 = (xValue16)xVMU_Alloc(objVMU);
	v15->Type = XLVT_VARIANT | XLVMT_NUMBER | XLVT_NUM_INT | XLVT_I64;
	v15->iVal64 = 87654321;
	
	// int
	xValue16 v16 = (xValue16)xVMU_Alloc(objVMU);
	v16->Type = XLVT_VARIANT | XLVMT_NUMBER | XLVT_NUM_INT | XLVT_I64;
	v16->iVal64 = 87654321;
	
	// int
	xValue16 v17 = (xValue16)xVMU_Alloc(objVMU);
	v17->Type = XLVT_VARIANT | XLVMT_NUMBER | XLVT_NUM_INT | XLVT_I64;
	v17->iVal64 = 87654321;
	
	xVMU_DebugPrint(objVMU);
	
	xVMU_Destroy(objVMU);
}





void xVM_DebugPrint(xValueManage objVM)
{
	printf("Value Manage 16 Memory : %d, Count : %d, AllocCount : %d\n\n", objVM->vm16.Memory, objVM->vm16.Count, objVM->vm16.AllocCount);
	for ( int i = 0; i < objVM->vm16.Count; i++ ) {
		xValueManageUnit objVMU = objVM->vm16.Memory[i];
		xVMU_DebugPrint(objVMU);
	}
	printf("Value Manage 24 Memory : %d, Count : %d, AllocCount : %d\n\n", objVM->vm24.Memory, objVM->vm24.Count, objVM->vm24.AllocCount);
	for ( int i = 0; i < objVM->vm24.Count; i++ ) {
		xValueManageUnit objVMU = objVM->vm24.Memory[i];
		xVMU_DebugPrint(objVMU);
	}
	printf("Value Manage 40 Memory : %d, Count : %d, AllocCount : %d\n\n", objVM->vm40.Memory, objVM->vm40.Count, objVM->vm40.AllocCount);
	for ( int i = 0; i < objVM->vm40.Count; i++ ) {
		xValueManageUnit objVMU = objVM->vm40.Memory[i];
		xVMU_DebugPrint(objVMU);
	}
}

char* xCore_FormatA(char* sFormat, ...)
{
	if ( sFormat == NULL ) { return ""; }
	va_list ip;
	va_start(ip, sFormat);
	int iSize = vsnprintf(NULL, 0, sFormat, ip);
	va_end(ip);
	if ( iSize >= 0 ) {
		char* sRet = malloc(iSize + 1);
		va_start(ip, sFormat);
		iSize = vsnprintf(sRet, iSize + 1, sFormat, ip);
		va_end(ip);
		sRet[iSize] = 0;
		//xCore.iRetSize = iSize;
		return sRet;
	} else {
		return "";
	}
}

void xVM_Test()
{
	xValueManage objVM = xVM_Create();
	
	/*
	xValue16 v[300];
	for ( int i = 0; i < 300; i++ ) {
		v[i] = (xValue16)xVM_Alloc(objVM, XLVT_VARIANT | XLVMT_NUMBER | XLVT_NUM_INT | XLVT_I64);
		v[i]->iVal64 = i;
	}
	for ( int i = 0; i < 300; i++ ) {
		if ( i % 10 == 0 ) {
			xVM_Free(objVM, (xValue)v[i]);
		}
	}
	for ( int i = 0; i < 32; i++ ) {
		xVM_NewI64(objVM, 12340000 + i);
	}
	for ( int i = 0; i < 300; i++ ) {
		if ( i % 10 == 0 ) {
			xVM_Free(objVM, (xValue)v[i]);
		}
	}
	for ( int i = 0; i < 30; i++ ) {
		xVM_NewF64(objVM, i + 0.1234);
	}
	*/
	
	xValue24 v[300];
	for ( int i = 0; i < 300; i++ ) {
		v[i] = (xValue24)xVM_Alloc(objVM, XLVT_VARIANT | XLVMT_STRING | XLVT_ASTR);
		v[i]->pVal.sValA = xCore_FormatA("Value - %d", i);
	}
	for ( int i = 0; i < 300; i++ ) {
		if ( i % 10 == 0 ) {
			xVM_Free(objVM, (xValue)v[i]);
		}
	}
	for ( int i = 0; i < 32; i++ ) {
		xVM_NewStrA(objVM, xCore_FormatA("NewValue - %d", i), 0);
	}
	
	xVM_DebugPrint(objVM);
	
	xVM_Destroy(objVM);
}





int main(int argc, char** argv)
{
	printf("start ...\n\n");
	
	//xVMU_Test();
	xVM_Test();
	
	printf("\nstop !\n");
	return 0;
}


