#ifndef XL_JSON5_NATIVE_H
#define XL_JSON5_NATIVE_H

#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#include "xrt.h"

/* 返回动态值由调用方 xrtValueRelease；合法 JSON5 null 与解析失败靠 XRT 错误区分。 */
xvalue* xjson5_parse_len(const char* text, size_t size);
/* 格式错误返回 false 并清错；内存故障返回 false 且保留 XERR_MEMORY。 */
bool xjson5_valid_len(const char* text, size_t size);
xvalue* xjson5_get_value_len(const char* text, size_t size, const char* path);
str xjson5_get_text_len(const char* text, size_t size, const char* path);
void* xjson5_get_text_abi(const char* text, size_t size, const char* path);
bool xjson5_has_error(void);
cstr xjson5_error_message(void);

#endif
