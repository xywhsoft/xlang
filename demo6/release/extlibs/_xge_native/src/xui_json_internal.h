#ifndef XUI_JSON_INTERNAL_H
#define XUI_JSON_INTERNAL_H

#include "xui_internal.h"



/* 写入对象成员名称。 */
static inline bool __xuiJsonWriterName(xjsonwriter* pWriter, const char* sName)
{
	return xrtJsonWriterName(pWriter, xrtStrView(sName));
}



/* 写入带名称的对象。 */
static inline bool __xuiJsonWriterObjectKey(xjsonwriter* pWriter, const char* sName)
{
	return __xuiJsonWriterName(pWriter, sName) && xrtJsonWriterObject(pWriter);
}



/* 写入带名称的数组。 */
static inline bool __xuiJsonWriterArrayKey(xjsonwriter* pWriter, const char* sName)
{
	return __xuiJsonWriterName(pWriter, sName) && xrtJsonWriterArray(pWriter);
}



/* 写入带名称的 null。 */
static inline bool __xuiJsonWriterNullKey(xjsonwriter* pWriter, const char* sName)
{
	return __xuiJsonWriterName(pWriter, sName) && xrtJsonWriterNull(pWriter);
}



/* 写入带名称的布尔值。 */
static inline bool __xuiJsonWriterBoolKey(xjsonwriter* pWriter,
	const char* sName, bool bValue)
{
	return __xuiJsonWriterName(pWriter, sName) &&
		xrtJsonWriterBool(pWriter, bValue);
}



/* 写入带名称的整数。 */
static inline bool __xuiJsonWriterIntKey(xjsonwriter* pWriter,
	const char* sName, int64 iValue)
{
	return __xuiJsonWriterName(pWriter, sName) &&
		xrtJsonWriterInt(pWriter, iValue);
}



/* 写入带名称的浮点数。 */
static inline bool __xuiJsonWriterFloatKey(xjsonwriter* pWriter,
	const char* sName, double fValue)
{
	return __xuiJsonWriterName(pWriter, sName) &&
		xrtJsonWriterFloat(pWriter, fValue);
}



/* 写入带名称的复制字符串。 */
static inline bool __xuiJsonWriterStringKey(xjsonwriter* pWriter,
	const char* sName, const char* sValue)
{
	return __xuiJsonWriterName(pWriter, sName) &&
		xrtJsonWriterString(pWriter, xrtStrView(sValue));
}

#endif
