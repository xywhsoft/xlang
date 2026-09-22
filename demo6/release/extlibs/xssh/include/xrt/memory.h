#ifndef XRT_MEMORY_H
#define XRT_MEMORY_H

#include <xrt/core.h>



/* 自定义底层分配器回调。 */
typedef ptr (*xallocproc)(ptr pContext, size_t iSize);
typedef ptr (*xreallocproc)(ptr pContext, ptr pMemory, size_t iSize);
typedef void (*xfreeproc)(ptr pContext, ptr pMemory);



/* XRT 所有动态内存最终使用同一个底层分配器。 */
typedef struct xallocator {
	ptr Context;
	xallocproc Alloc;
	xreallocproc Realloc;
	xfreeproc Free;
} xallocator;



/* 验证半开内存范围具有合法起点，且末地址计算不会回绕。 */
static inline bool xrtMemRangeValid(const void* pData, size_t iSize)
{
	if ( iSize == 0 ) {
		return true;
	}
	if ( pData == NULL ) {
		return false;
	}
#if SIZE_MAX > UINTPTR_MAX
	if ( iSize > (size_t)UINTPTR_MAX ) {
		return false;
	}
#endif
	return (uintptr_t)pData <=
		( UINTPTR_MAX - (uintptr_t)iSize );
}



/* 判断两个非空半开范围是否重叠；调用方应先验证各自范围合法。 */
static inline bool xrtMemRangesOverlap(
	const void* pLeft,
	size_t iLeftSize,
	const void* pRight,
	size_t iRightSize
)
{
	uintptr_t iLeft;
	uintptr_t iRight;

	if ( (iLeftSize == 0) || (iRightSize == 0) ) {
		return false;
	}
	iLeft = (uintptr_t)pLeft;
	iRight = (uintptr_t)pRight;
	if ( iLeft <= iRight ) {
		return (iRight - iLeft) < iLeftSize;
	}
	return (iLeft - iRight) < iRightSize;
}



XRT_EXTERN_C_BEGIN



/* 在首次分配前替换进程级底层分配器。 */
XRT_API bool xrtSetAllocator(const xallocator* pAllocator);



/* 复制当前进程级底层分配器。 */
XRT_API void xrtGetAllocator(xallocator* pAllocator);



/* 分配至少一个字节的内存。 */
XRT_API ptr xrtMalloc(size_t iSize);



/* 分配并清零内存，乘法溢出时失败。 */
XRT_API ptr xrtCalloc(size_t iCount, size_t iSize);



/* 调整内存大小，大小为零时释放并返回空指针。 */
XRT_API ptr xrtRealloc(ptr pMemory, size_t iSize);



/* 释放内存，允许传入空指针。 */
XRT_API void xrtFree(ptr pMemory);



/* 复制一段二进制内存。 */
XRT_API ptr xrtMemDup(const void* pData, size_t iSize);



/* 用不可被编译器删除的写入清零敏感内存；空区间允许空指针。 */
XRT_API void xrtSecureZero(ptr pData, size_t iSize);



#if defined(XRT_FEATURE_MEMORY_DEBUG)
/* 记录分配调用位置。 */
XRT_API ptr xrtMallocAt(size_t iSize, cstr sFile, uint32 iLine);



/* 记录清零分配调用位置。 */
XRT_API ptr xrtCallocAt(size_t iCount, size_t iSize, cstr sFile, uint32 iLine);



/* 记录重分配调用位置。 */
XRT_API ptr xrtReallocAt(ptr pMemory, size_t iSize, cstr sFile, uint32 iLine);



/* 记录释放调用位置。 */
XRT_API void xrtFreeAt(ptr pMemory, cstr sFile, uint32 iLine);



/* 记录内存复制分配调用位置。 */
XRT_API ptr xrtMemDupAt(const void* pData, size_t iSize, cstr sFile, uint32 iLine);
#endif



XRT_EXTERN_C_END



#if defined(XRT_FEATURE_MEMORY_DEBUG) && !defined(XRT_DECLARATIONS)
	#define xrtMalloc(iSize) xrtMallocAt((iSize), __FILE__, (uint32)__LINE__)
	#define xrtCalloc(iCount, iSize) xrtCallocAt((iCount), (iSize), __FILE__, (uint32)__LINE__)
	#define xrtRealloc(pMemory, iSize) xrtReallocAt((pMemory), (iSize), __FILE__, (uint32)__LINE__)
	#define xrtFree(pMemory) xrtFreeAt((pMemory), __FILE__, (uint32)__LINE__)
	#define xrtMemDup(pData, iSize) xrtMemDupAt((pData), (iSize), __FILE__, (uint32)__LINE__)
#endif

#endif
