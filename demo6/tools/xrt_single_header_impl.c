/*
    本文件只用于本地构建 xlang 默认运行时对象。
    xruntime.h 包含所需 XRT 闭包，XRUNTIME_IMPLEMENTATION 只能定义一次。
*/
#define XRT_MODULE_ALL
#define XRUNTIME_MODULE_ALL
#define XRUNTIME_IMPLEMENTATION
#include "lib/xruntime/xruntime.h"
#include "src/api/xl_foreign_error.c"

/* XLIR links the public runtime ABI directly.  These are exported facade
 * functions (not Cgen snippets) for the native DynamicFields class. */
void* xl6_xrt_dynamic_fields_native_handle(void* handle)
{
    return handle;
}

int64_t xl6_xrt_dynamic_fields_count(void* handle)
{
    return (int64_t)xrtDynamicFieldsCount((const xrtdynamicfields*)handle);
}

int64_t xl6_xrt_dynamic_fields_capacity(void* handle)
{
    return (int64_t)xrtDynamicFieldsCapacity((const xrtdynamicfields*)handle);
}

bool xl6_xrt_dynamic_fields_reserve(void* handle, int64_t capacity)
{
    return capacity >= 0 && xrtDynamicFieldsReserve((xrtdynamicfields*)handle, (size_t)capacity);
}

bool xl6_xrt_dynamic_fields_clear(void* handle)
{
    return xrtDynamicFieldsClear((xrtdynamicfields*)handle);
}

bool xl6_xrt_dynamic_fields_has(void* handle, char* name)
{
    return xrtDynamicFieldsHas((const xrtdynamicfields*)handle, xrtStrView(name));
}

xvalue* xl6_xrt_dynamic_fields_get(void* handle, char* name)
{
    xvalue* value = xrtDynamicFieldsGetRef((const xrtdynamicfields*)handle, xrtStrView(name));
    return value != NULL ? value : xrtValueNull();
}

xvalue* xl6_xrt_dynamic_fields_copy(void* handle, char* name)
{
    xvalue* value = xrtDynamicFieldsCopy((const xrtdynamicfields*)handle, xrtStrView(name));
    return value != NULL ? value : xrtValueNull();
}

bool xl6_xrt_dynamic_fields_set(void* handle, char* name, xvalue* value)
{
    return xrtDynamicFieldsSetRef((xrtdynamicfields*)handle, xrtStrView(name), value);
}

bool xl6_xrt_dynamic_fields_remove(void* handle, char* name)
{
    return xrtDynamicFieldsRemove((xrtdynamicfields*)handle, xrtStrView(name));
}

xvalue* xl6_xrt_dynamic_fields_take(void* handle, char* name)
{
    xvalue* value = xrtDynamicFieldsTake((xrtdynamicfields*)handle, xrtStrView(name));
    return value != NULL ? value : xrtValueNull();
}

xvalue* xl6_xrt_dynamic_fields_to_value(void* handle)
{
    xvalue* value = xrtDynamicFieldsToValue((const xrtdynamicfields*)handle);
    return value != NULL ? value : xrtValueNull();
}

bool xl6_xrt_dynamic_fields_merge(void* handle, void* source, bool replace)
{
    return xrtDynamicFieldsMerge((xrtdynamicfields*)handle, (const xrtdynamicfields*)source, replace);
}

void* xl6_xrt_dynamic_fields_clone(void* handle)
{
    return xrtDynamicFieldsClone((const xrtdynamicfields*)handle);
}

void* xl6_xrt_dynamic_fields_wrap(void* handle)
{
    return handle;
}

void* xl6_xrt_dynamic_fields_from_value(xvalue* value)
{
    return xrtDynamicFieldsFromValue(value);
}

void xl6_xrt_dynamic_fields_destroy(void* handle)
{
    xrtDynamicFieldsUnref((xrtdynamicfields*)handle);
}
