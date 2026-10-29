#include <stdint.h>
#include "../include/tabulate_native.h"

/* 行列计数与 XLang int 的 C ABI 保持一致。 */
extern int64_t xtab_row_count(void* table);
extern int64_t xtab_col_count(void* table);

/* 渲染结果是拥有型 C 字符串，不是不透明句柄。 */
_Static_assert(_Generic(&xtab_render, char* (*)(void*): 1, default: 0),
    "xtab_render must return char*");
