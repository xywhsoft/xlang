


#include <complex.h>
#include <stdio.h>
#include <stdlib.h>



#include "type.h"



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
	printf("alloc value : %d\n", idx);
	pVal = (xValue)&objUnit->arrValue[objUnit->iValueSize * idx];
	pVal->Flag = objUnit->iValueFlag | idx;
	return pVal;
}

// 将 xValue 管理器单元中的某个变量释放
int xVMU_Free(xValueManageUnit objUnit, xValue pVal)
{
	u8 idx = pVal->Flag & XVM_MASK_INDEX;
	printf("free value : %d\n", idx);
	pVal->Type = 0;
	pVal->Flag = 0;
	objUnit->arrFreeIdx[(objUnit->iFreeOffset + objUnit->iFreeCount) & 0xFF] = idx;
	objUnit->iFreeCount++;
	objUnit->iValueCount--;
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
	printf("%x\t", v->Flag);
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
	printf("id\tmt\tst\t\tnt\tFlag\ttype\tval\n");
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
	printf("pos\tidx\tmt\tst\t\tnt\tFlag\ttype\tval\n");
	for ( int i = 0; i < objVMU->iFreeCount; i++ ) {
		int idx = objVMU->arrFreeIdx[(objVMU->iFreeOffset + i) & 0xFF];
		printf("%d\t%d\t", i, idx);
		xValue v = xVMU_GetValue(objVMU, idx);
		xVMU_PrintValue(v);
	}
}



int main(int argc, char** argv)
{
	printf("start ...\n\n");
	
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
	
	printf("\nstop !\n");
	return 0;
}


