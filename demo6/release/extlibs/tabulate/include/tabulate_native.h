#ifndef XL_TABULATE_NATIVE_H
#define XL_TABULATE_NATIVE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void* xtab_create(void);
void xtab_destroy(void* table);

/* 事务式逐格写入：失败时自动 abort，原表保持不变。 */
int xtab_begin_headers(void* table);
int xtab_begin_row(void* table);
int xtab_append_cell(void* table, const char* text, long long length);
int xtab_commit(void* table);
void xtab_abort(void* table);

/* C 便捷入口接受 NUL 结尾字符串数组，每格至多读取 4097 字节。 */
int xtab_set_headers(void* table, const char* const* cells, size_t count);
int xtab_add_row(void* table, const char* const* cells, size_t count);

/* 文本由 XRT 独立分配，C 调用方用 xrtFree 释放。 */
void* xtab_render(void* table);
int64_t xtab_row_count(void* table);
int64_t xtab_col_count(void* table);

#ifdef __cplusplus
}
#endif
#endif
