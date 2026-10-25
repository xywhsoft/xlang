#ifndef XL_EXTLIB_HCL_NATIVE_H
#define XL_EXTLIB_HCL_NATIVE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* HCL 正文须为有效 C 字符串；NULL 报 XERR_ARGUMENT，"" 是合法空文档。 */
/* 字符串结果归调用方所有，使用 xrtFree 释放；NULL 表示失败，检查 XRT 错误。 */
void* xhcl_get(const char* hcl, const char* key);
void* xhcl_get_raw(const char* hcl, const char* key);
int64_t xhcl_get_int(const char* hcl, const char* key);
double xhcl_get_float(const char* hcl, const char* key);
bool xhcl_get_bool(const char* hcl, const char* key);
bool xhcl_has(const char* hcl, const char* key);
int64_t xhcl_count_blocks(const char* hcl, const char* block_type);
void* xhcl_block_label(const char* hcl, const char* block_type, int64_t index);
int64_t xhcl_block_label_count(const char* hcl, const char* block_type,
    int64_t index);
void* xhcl_block_label_at(const char* hcl, const char* block_type,
    int64_t index, int64_t label_index);
bool xhcl_validate(const char* hcl);
int64_t xhcl_count_attrs(const char* hcl);

#ifdef __cplusplus
}
#endif

#endif
