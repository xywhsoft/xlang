#include "../../include/xhttp_multipart_package.h"

#ifndef XHTTP_INTERNAL_H
#define XHTTP_INTERNAL_H


#include <limits.h>
#include <string.h>



/* 设置 xhttp 参数错误。 */
static inline void __xhttpErrorSetInvalidArgument(void)
{
	xrtSetErrorInfo(
		XERR_ARGUMENT, "xhttp", 1, "invalid argument"
	);
}



/* 设置 xhttp 非法值错误。 */
static inline void __xhttpErrorSetValue(void)
{
	xrtSetErrorInfo(
		XERR_VALUE, "xhttp", 2, "invalid value"
	);
}



/* 设置 xhttp 状态错误。 */
static inline void __xhttpErrorSetInvalidState(void)
{
	xrtSetErrorInfo(
		XERR_STATE, "xhttp", 3, "invalid state"
	);
}



/* 设置 xhttp 大小溢出错误。 */
static inline void __xhttpErrorSetSizeOverflow(void)
{
	xrtSetErrorInfo(
		XERR_RANGE, "xhttp", 4, "size overflow"
	);
}



/* 设置 xhttp 内存分配失败。 */
static inline void __xhttpErrorSetOutOfMemory(void)
{
	xrtSetErrorInfo(
		XERR_MEMORY, "xhttp", 7, "out of memory"
	);
}



/* 设置 xhttp 范围错误。 */
static inline void __xhttpErrorSetRange(void)
{
	xrtSetErrorInfo(
		XERR_RANGE, "xhttp", 5, "range is out of bounds"
	);
}



/* 设置 xhttp 内部契约错误。 */
static inline void __xhttpErrorSetInternal(void)
{
	xrtSetErrorInfo(
		XERR_INTERNAL, "xhttp", 6, "internal contract violation"
	);
}



/* 将错误所有权转移到当前执行上下文。 */
static inline void __xhttpErrorSetOwned(xerror* pError)
{
	xrtSetErrorTake(pError);
}



/* 静默交换当前错误所有权。 */
static inline xerror* __xhttpErrorSwapOwned(xerror* pError)
{
	xerror* pPrevious = xrtTakeError();

	xrtSetErrorTake(pError);
	return pPrevious;
}



/* 验证借用文本是一段不会发生地址回绕的连续内存。 */
static inline bool __xhttpViewValid(xstrview Text)
{
	if ( !xrtMemRangeValid(Text.Data, Text.Size) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 按 ASCII 规则把大写字母转换为小写。 */
static inline unsigned char __xhttpAsciiLower(unsigned char iByte)
{
	if ( (iByte >= (unsigned char)'A') &&
		(iByte <= (unsigned char)'Z') ) {
		return (unsigned char)(iByte +
			((unsigned char)'a' - (unsigned char)'A'));
	}
	return iByte;
}



#if defined(XRT_FEATURE_HTTP)

/* 从已经验证的字段数组复制一个描述符，兼容未对齐存储。 */
static inline void __xhttpFieldLoad(
	const xhttpfield* pFields,
	size_t iIndex,
	xhttpfield* pField
)
{
	memcpy(
		pField,
		((const uint8*)pFields) + (iIndex * sizeof(*pFields)),
		sizeof(*pField)
	);
}



/* 验证字段描述符数组及其借用文本的基础内存边界。 */
static inline bool __xhttpFieldArrayValid(
	const xhttpfield* pFields,
	size_t iCount
)
{
	xhttpfield Field;
	size_t iBytes;
	size_t i;

	if ( ((pFields == NULL) && (iCount != 0)) ||
		(iCount > (SIZE_MAX / sizeof(*pFields))) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	iBytes = iCount * sizeof(*pFields);
	if ( !xrtMemRangeValid(pFields, iBytes) ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		__xhttpFieldLoad(pFields, i, &Field);
		if ( !__xhttpViewValid(Field.Name) ||
			!__xhttpViewValid(Field.Value) ) {
			return false;
		}
	}
	return true;
}



/* 判断输出内存是否覆盖字段数组或任意借用文本。 */
static inline bool __xhttpFieldArrayOverlap(
	const xhttpfield* pFields,
	size_t iCount,
	const void* pMemory,
	size_t iSize
)
{
	xhttpfield Field;
	size_t i;

	if ( xrtMemRangesOverlap(
		pFields,
		iCount * sizeof(*pFields),
		pMemory,
		iSize
	) ) {
		return true;
	}
	for ( i = 0; i < iCount; i++ ) {
		__xhttpFieldLoad(pFields, i, &Field);
		if ( xrtMemRangesOverlap(
			Field.Name.Data,
			Field.Name.Size,
			pMemory,
			iSize
		) || xrtMemRangesOverlap(
			Field.Value.Data,
			Field.Value.Size,
			pMemory,
			iSize
		) ) {
			return true;
		}
	}
	return false;
}


/* 验证 Header 查询使用的非空字段名。 */
static inline bool __xhttpLookupNameValid(xstrview Name)
{
	if ( !__xhttpViewValid(Name) ) {
		return false;
	}
	if ( !xrtHttpTokenValid(Name) ) {
		__xhttpErrorSetValue();
		return false;
	}
	return true;
}

#endif



#endif
