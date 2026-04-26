


#include <stdlib.h>
#include <string.h>



#include "smm.h"



// 创建结构化内存管理器
XXAPI SmmObject SMM_Create(unsigned int iItemLenght, unsigned int PreassignStep)
{
	if ( iItemLenght == 0 ) { return 0; }
	if ( PreassignStep == 0 ) { PreassignStep = 32; }
	SmmObject ObjPtr = malloc(sizeof(SmmStruct));
	if ( ObjPtr ) {
		ObjPtr->StructMemory = 0;
		ObjPtr->StructLenght = iItemLenght;
		ObjPtr->StructCount = 0;
		ObjPtr->AllocCount = 0;
		ObjPtr->AllocStep = PreassignStep;
	}
	return ObjPtr;
}

// 重置结构化内存管理器
XXAPI void SMM_ReInit(SmmObject pObject)
{
	if ( pObject->StructMemory ) { free(pObject->StructMemory); pObject->StructMemory = 0; }
	pObject->StructCount = 0;
	pObject->AllocCount = 0;
}

// 销毁结构化内存管理器
XXAPI void SMM_Destroy(SmmObject pObject)
{
	SMM_ReInit(pObject);
	free(pObject);
}

// 分配内存
XXAPI int SMM_Malloc(SmmObject pObject, unsigned int iCount)
{
	if ( iCount > pObject->AllocCount ) {
		// 增量
		void* pNew = realloc(pObject->StructMemory, iCount * pObject->StructLenght);
		if ( pNew ) {
			pObject->AllocCount = iCount;
			pObject->StructMemory = pNew;
			return -1;
		}
	} else if ( iCount < pObject->AllocCount ) {
		// 裁剪
		void* pNew = realloc(pObject->StructMemory, iCount * pObject->StructLenght);
		if ( pNew ) {
			pObject->AllocCount = iCount;
			pObject->StructMemory = pNew;
			if ( pObject->StructCount > iCount ) {
				// 需要裁剪数据
				pObject->StructCount = iCount;
			}
			return -1;
		}
	} else if ( iCount = 0 ) {
		// 清空
		SMM_ReInit(pObject);
		return -1;
	} else {
		// 不变
		return -1;
	}
	return 0;
}

// 中间插入成员
XXAPI unsigned int SMM_Insert(SmmObject pObject, unsigned int iPos, unsigned int iCount)
{
	// 不能添加 0 个成员
	if ( iCount == 0 ) { iCount = 1; }
	// 分配内存
	if ( (pObject->StructCount + iCount) > pObject->AllocCount ) {
		if ( SMM_Malloc(pObject, pObject->StructCount + iCount + pObject->AllocStep) == 0 ) {
			return 0;
		}
	}
	if ( iPos < pObject->StructCount ) {
		// 插入模式（需要移动内存）
		void* dst = pObject->StructMemory + ((iPos + iCount) * pObject->StructLenght);
		void* src = pObject->StructMemory + (iPos * pObject->StructLenght);
		memmove(dst, src, (pObject->StructCount - iPos) * pObject->StructLenght);
		pObject->StructCount += iCount;
		return iPos + 1;
	} else {
		// 追加模式
		pObject->StructCount += iCount;
		return pObject->StructCount - iCount + 1;
	}
}

// 末尾添加成员
XXAPI unsigned int SMM_Append(SmmObject pObject, unsigned int iCount)
{
	return SMM_Insert(pObject, pObject->StructCount, iCount);
}

// 交换成员
XXAPI int SMM_Swap(SmmObject pObject, unsigned int iPosA, unsigned int iPosB)
{
	// 范围检查
	if ( iPosA == 0 ) { return 0; }
	if ( iPosA > pObject->StructCount ) { return 0; }
	if ( iPosB == 0 ) { return 0; }
	if ( iPosB > pObject->StructCount ) { return 0; }
	if ( iPosA == iPosB ) { return -1; }
	// 交换成员
	iPosA--;
	iPosB--;
	void* pStuA = malloc(pObject->StructLenght);
	memmove(pStuA, pObject->StructMemory + (iPosA * pObject->StructLenght), pObject->StructLenght);
	memmove(pObject->StructMemory + (iPosA * pObject->StructLenght), pObject->StructMemory + (iPosB * pObject->StructLenght), pObject->StructLenght);
	memmove(pObject->StructMemory + (iPosB * pObject->StructLenght), pStuA, pObject->StructLenght);
	free(pStuA);
	return -1;
}

// 删除成员
XXAPI int SMM_Delete(SmmObject pObject, unsigned int iPos, unsigned int iCount)
{
	// 不能添加 0 个成员、不能删除不存在的成员（0号成员也不存在）
	if ( iCount == 0 ) { return 0; }
	if ( iPos == 0 ) { return 0; }
	if ( iPos > pObject->StructCount ) { return 0; }
	// 删除成员（数量不足时删除后面的所有成员）
	iPos--;
	if ( iPos + iCount < pObject->StructCount ) {
		// 中段删除
		void* dst = pObject->StructMemory + (iPos * pObject->StructLenght);
		void* src = pObject->StructMemory + ((iPos + iCount) * pObject->StructLenght);
		memmove(dst, src, (pObject->StructCount - (iPos + iCount)) * pObject->StructLenght);
		pObject->StructCount -= iCount;
	} else {
		// 末尾删除
		pObject->StructCount = iPos;
	}
	return -1;
}

// 获取成员指针
XXAPI void* SMM_GetPtr(SmmObject pObject, unsigned int iPos)
{
	if ( iPos ) {
		iPos--;
		if ( iPos < pObject->StructCount ) {
			return &pObject->StructMemory[iPos * pObject->StructLenght];
		}
	}
	return 0;
}

// 成员排序
XXAPI int SMM_Sort(SmmObject pObject, void* procCompar)
{
	if ( pObject ) {
		qsort(pObject->StructMemory, pObject->StructCount, pObject->StructLenght, procCompar);
		return -1;
	} else {
		return 0;
	}
}


