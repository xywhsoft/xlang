


#ifndef XXRTL_PointMemoryManage
	#define XXRTL_PointMemoryManage
	
	
	
	#ifdef BUILD_DLL
		#define XXAPI	__declspec(dllexport)
	#else
		#define XXAPI
	#endif
	
	
	
	/*
		Point Memory Manage [指针内存管理器]
		┌─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬──┐
		│01│02│03│04│05│06│07│08│09│10│11│12│ .. │
		└─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴──┘
	*/
	
	// 管理器结构体定义
	typedef struct {
		void** Memory;							// 管理器内存指针
		unsigned int Count;						// 管理器中存在多少成员
		unsigned int AllocCount;				// 已经申请的结构数量
	} PmmStruct, *PmmObject;
	
	
	
	// 申请步长
	#define PMM_PREASSIGNSTEP	256
	
	
	
	// 创建指针内存管理器
	XXAPI PmmObject PMM_Create();
	
	// 重置指针内存管理器
	XXAPI void PMM_ReInit(PmmObject pObject);
	
	// 销毁指针内存管理器
	XXAPI void PMM_Destroy(PmmObject pObject);
	
	// 分配内存
	XXAPI int PMM_Malloc(PmmObject pObject, unsigned int iCount);
	
	// 中间插入成员(0为头部插入，pObject->Count为末尾插入)
	XXAPI unsigned int PMM_Insert(PmmObject pObject, unsigned int iPos, void* pVal);
	
	// 末尾添加成员
	XXAPI unsigned int PMM_Append(PmmObject pObject, void* pVal);
	
	// 交换成员
	XXAPI int PMM_Swap(PmmObject pObject, unsigned int iPosA, unsigned int iPosB);
	
	// 删除成员
	XXAPI int PMM_Delete(PmmObject pObject, unsigned int iPos, unsigned int iCount);
	
	// 获取成员指针
	XXAPI void** PMM_GetPtr(PmmObject pObject, unsigned int iPos);
	XXAPI void* PMM_GetVal(PmmObject pObject, unsigned int iPos);
	
	// 设置成员指针
	XXAPI int PMM_SetVal(PmmObject pObject, unsigned int iPos, void* pVal);
	
	// 成员排序
	XXAPI int PMM_Sort(PmmObject pObject, void* procCompar);
	
	// 内联不安全的取值函数
	inline void** PMM_GetPtr_Unsafe(PmmObject pObject, unsigned int iPos)
	{
		return &(pObject->Memory[iPos - 1]);
	}
	inline void* PMM_GetVal_Unsafe(PmmObject pObject, unsigned int iPos)
	{
		return pObject->Memory[iPos - 1];
	}
	inline void PMM_SetVal_Unsafe(PmmObject pObject, unsigned int iPos, void* pVal)
	{
		pObject->Memory[iPos - 1] = pVal;
	}
	
	
	
#endif


