#ifndef XLANG_EXTLIB_CHARDET_H
#define XLANG_EXTLIB_CHARDET_H
#ifndef XRT_MODULE_CHARSET_DETECT
#define XRT_MODULE_CHARSET_DETECT
#endif
#include "xrt.h"

XRT_EXTERN_C_BEGIN
/* 语言桥的物理 ABI：point 对应 void*，string 对应 const char*。
 * 所有输入只借用、不修改；名称为进程期静态只读字符串，调用者不得释放。
 * C 使用者也可直接调用 xrtEncodingGuess(xbytesview)，一次取得全部探测信息。
 */
const char* xchardet_detect_name_n(void* data, size_t size);
int xchardet_detect_conf_n(void* data, size_t size);
int xchardet_detect_bom_size_n(void* data, size_t size);
bool xchardet_is_utf8_n(void* data, size_t size);
bool xchardet_is_ascii_n(void* data, size_t size);

const char* xchardet_detect_name_str(const char* data, size_t size);
int xchardet_detect_conf_str(const char* data, size_t size);
int xchardet_detect_bom_size_str(const char* data, size_t size);
bool xchardet_is_utf8_str(const char* data, size_t size);
bool xchardet_is_ascii_str(const char* data, size_t size);
XRT_EXTERN_C_END
#endif
