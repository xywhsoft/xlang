#ifndef XLANG_EXTLIB_JSON_PATCH_H
#define XLANG_EXTLIB_JSON_PATCH_H

#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#ifndef XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_VALUE_GRAPH
#endif
#include "xrt.h"

XRT_EXTERN_C_BEGIN

/* 输入只借用；返回的值由调用方释放。失败返回 null 值并设置 XRT error。 */
xvalue* xjsonpatch_apply(xvalue* doc, xvalue* patch);
xvalue* xjsonpatch_merge(xvalue* doc, xvalue* patch);
xvalue* xjsonpatch_get(xvalue* doc, const char* pointer);
bool xjsonpatch_has(xvalue* doc, const char* pointer);
xvalue* xjsonpatch_set(xvalue* doc, const char* pointer, xvalue* value);
xvalue* xjsonpatch_remove(xvalue* doc, const char* pointer);
/* 长度感知路径允许 JSON 键含 U+0000；C 字符串入口是便利包装。 */
xvalue* xjsonpatch_get_len(xvalue* doc, const char* pointer, size_t size);
bool xjsonpatch_has_len(xvalue* doc, const char* pointer, size_t size);
xvalue* xjsonpatch_set_len(xvalue* doc, const char* pointer,
    size_t size, xvalue* value);
xvalue* xjsonpatch_remove_len(xvalue* doc, const char* pointer, size_t size);
/* XLang 的 var 桥负责无损传递嵌入 NUL 的 string。 */
xvalue* xjsonpatch_get_value(xvalue* doc, xvalue* pointer);
bool xjsonpatch_has_value(xvalue* doc, xvalue* pointer);
xvalue* xjsonpatch_set_value(xvalue* doc, xvalue* pointer, xvalue* value);
xvalue* xjsonpatch_remove_value(xvalue* doc, xvalue* pointer);
bool xjsonpatch_has_error(void);
/* 返回独立的 XRT 字符串，使用 xrtFree 释放；内嵌 NUL 的键拒绝。 */
void* xjsonpatch_encode_pointer_len(const char* key, size_t size);
/* 完整 JSON 字符串结果，允许键中含 U+0000；返回值由调用方释放。 */
xvalue* xjsonpatch_encode_pointer_value(xvalue* key);

XRT_EXTERN_C_END

#endif
