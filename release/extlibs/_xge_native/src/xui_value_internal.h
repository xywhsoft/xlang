#ifndef XUI_VALUE_INTERNAL_H
#define XUI_VALUE_INTERNAL_H

#include "xrt.h"

#include <string.h>



/* 把允许省略长度的内部键和文本转换为新版 XRT 视图。 */
static xstrview __xuiValueTextView(const char* sText, size_t iSize)
{
	xstrview Text;

	Text.Data = sText != NULL ? sText : "";
	Text.Size = (sText != NULL) && (iSize == 0u) ? strlen(sText) : iSize;
	return Text;
}



/* 创建复制文本的动态值；旧调用中的 take 标志只用于保持源签名清晰。 */
static xvalue* __xuiValueCreateText(
	const char* sText,
	size_t iSize,
	bool bTake
)
{
	(void)bTake;
	return xrtValueString(__xuiValueTextView(sText, iSize));
}



/* 返回对象键对应的借用值。 */
static xvalue* __xuiValueObjectGet(
	const xvalue* pObject,
	const char* sKey,
	size_t iKeySize
)
{
	return xrtValueObjectGet(
		pObject,
		__xuiValueTextView(sKey, iKeySize)
	);
}



/* 设置对象值；take 成功时移交引用，失败时仍由调用方负责释放。 */
static bool __xuiValueObjectSetValue(
	xvalue* pObject,
	const char* sKey,
	size_t iKeySize,
	xvalue* pValue,
	bool bTake
)
{
	xvalue* pOwned;

	if ( !bTake ) {
		return xrtValueObjectSet(
			pObject,
			__xuiValueTextView(sKey, iKeySize),
			pValue
		);
	}
	pOwned = pValue;
	return xrtValueObjectSetTake(
		pObject,
		__xuiValueTextView(sKey, iKeySize),
		&pOwned
	);
}



/* 创建标量临时值并无条件交给对象，失败路径不会泄漏。 */
static bool __xuiValueObjectSetInt(
	xvalue* pObject,
	const char* sKey,
	size_t iKeySize,
	int64 iValue
)
{
	return xrtValueObjectSetNew(
		pObject,
		__xuiValueTextView(sKey, iKeySize),
		xrtValueInt(iValue)
	);
}



/* 创建浮点临时值并无条件交给对象。 */
static bool __xuiValueObjectSetFloat(
	xvalue* pObject,
	const char* sKey,
	size_t iKeySize,
	double fValue
)
{
	return xrtValueObjectSetNew(
		pObject,
		__xuiValueTextView(sKey, iKeySize),
		xrtValueFloat(fValue)
	);
}



/* 设置进程期布尔单例。 */
static bool __xuiValueObjectSetBool(
	xvalue* pObject,
	const char* sKey,
	size_t iKeySize,
	bool bValue
)
{
	return xrtValueObjectSetNew(
		pObject,
		__xuiValueTextView(sKey, iKeySize),
		xrtValueBool(bValue)
	);
}



/* 复制文本并设置对象键。 */
static bool __xuiValueObjectSetText(
	xvalue* pObject,
	const char* sKey,
	size_t iKeySize,
	const char* sText,
	size_t iTextSize,
	bool bTake
)
{
	(void)bTake;
	return xrtValueObjectSetNew(
		pObject,
		__xuiValueTextView(sKey, iKeySize),
		xrtValueString(__xuiValueTextView(sText, iTextSize))
	);
}



/* 向数组移交值；失败时保留调用方原引用。 */
static bool __xuiValueArrayAppendValue(
	xvalue* pArray,
	xvalue* pValue,
	bool bTake
)
{
	xvalue* pOwned;

	if ( !bTake ) {
		return xrtValueArrayAppend(pArray, pValue);
	}
	pOwned = pValue;
	return xrtValueArrayAppendTake(pArray, &pOwned);
}



/* 创建整数临时值并加入数组。 */
static bool __xuiValueArrayAppendInt(xvalue* pArray, int64 iValue)
{
	return xrtValueArrayAppendNew(pArray, xrtValueInt(iValue));
}



/* 创建浮点临时值并加入数组。 */
static bool __xuiValueArrayAppendFloat(xvalue* pArray, double fValue)
{
	return xrtValueArrayAppendNew(pArray, xrtValueFloat(fValue));
}



/* 复制文本并加入数组。 */
static bool __xuiValueArrayAppendText(
	xvalue* pArray,
	const char* sText,
	size_t iSize,
	bool bTake
)
{
	(void)bTake;
	return xrtValueArrayAppendNew(
		pArray,
		xrtValueString(__xuiValueTextView(sText, iSize))
	);
}



/* 读取精确整数；空值或类型不匹配返回零。 */
static int64 __xuiValueGetInt(const xvalue* pValue)
{
	int64 iValue = 0;

	if ( xrtValueType(pValue) == XVALUE_INT ) {
		(void)xrtValueGetInt(pValue, &iValue);
	}
	return iValue;
}



/* 读取数字；整数按无损方向转换为 double。 */
static double __xuiValueGetFloat(const xvalue* pValue)
{
	double fValue = 0.0;
	int64 iValue;

	if ( xrtValueType(pValue) == XVALUE_FLOAT ) {
		(void)xrtValueGetFloat(pValue, &fValue);
	} else if ( xrtValueType(pValue) == XVALUE_INT ) {
		iValue = 0;
		(void)xrtValueGetInt(pValue, &iValue);
		fValue = (double)iValue;
	}
	return fValue;
}



/* 读取精确布尔值；空值或类型不匹配返回 false。 */
static bool __xuiValueGetBool(const xvalue* pValue)
{
	bool bValue = false;

	if ( xrtValueType(pValue) == XVALUE_BOOL ) {
		(void)xrtValueGetBool(pValue, &bValue);
	}
	return bValue;
}



/* 返回带末尾零的借用字符串；非字符串返回空指针。 */
static const char* __xuiValueGetText(const xvalue* pValue)
{
	xstrview Text;

	if ( (xrtValueType(pValue) != XVALUE_STRING) ||
		!xrtValueGetString(pValue, &Text) ) {
		return NULL;
	}
	return Text.Data;
}



/* 返回文本或字节长度，容器返回元素数，其余标量返回零。 */
static size_t __xuiValueGetSize(const xvalue* pValue)
{
	xstrview Text;
	xbytesview Data;

	if ( (xrtValueType(pValue) == XVALUE_STRING) &&
		xrtValueGetString(pValue, &Text) ) {
		return Text.Size;
	}
	if ( (xrtValueType(pValue) == XVALUE_BYTES) &&
		xrtValueGetBytes(pValue, &Data) ) {
		return Data.Size;
	}
	return xrtValueIsContainer(pValue) ? xrtValueCount(pValue) : 0u;
}



/* 常用对象标量读取保持旧调用点的默认零语义。 */
static int64 __xuiValueObjectGetInt(
	const xvalue* pObject,
	const char* sKey,
	size_t iKeySize
)
{
	return __xuiValueGetInt(
		__xuiValueObjectGet(pObject, sKey, iKeySize)
	);
}



static double __xuiValueObjectGetFloat(
	const xvalue* pObject,
	const char* sKey,
	size_t iKeySize
)
{
	return __xuiValueGetFloat(
		__xuiValueObjectGet(pObject, sKey, iKeySize)
	);
}



static bool __xuiValueObjectGetBool(
	const xvalue* pObject,
	const char* sKey,
	size_t iKeySize
)
{
	return __xuiValueGetBool(
		__xuiValueObjectGet(pObject, sKey, iKeySize)
	);
}



static const char* __xuiValueObjectGetText(
	const xvalue* pObject,
	const char* sKey,
	size_t iKeySize
)
{
	return __xuiValueGetText(
		__xuiValueObjectGet(pObject, sKey, iKeySize)
	);
}



/* 类型判断集中使用新版不透明值 API。 */
static bool __xuiValueIsNull(const xvalue* pValue)
{
	return xrtValueType(pValue) == XVALUE_NULL;
}



static bool __xuiValueIsBool(const xvalue* pValue)
{
	return xrtValueType(pValue) == XVALUE_BOOL;
}



static bool __xuiValueIsInt(const xvalue* pValue)
{
	return xrtValueType(pValue) == XVALUE_INT;
}



static bool __xuiValueIsText(const xvalue* pValue)
{
	return xrtValueType(pValue) == XVALUE_STRING;
}



static bool __xuiValueIsArray(const xvalue* pValue)
{
	return xrtValueType(pValue) == XVALUE_ARRAY;
}



static bool __xuiValueIsObject(const xvalue* pValue)
{
	return xrtValueType(pValue) == XVALUE_OBJECT;
}



#endif
