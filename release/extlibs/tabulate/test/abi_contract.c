#include <stdint.h>
#include "../include/tabulate_native.h"

/* 行列计数与 XLang int 的 C ABI 保持一致。 */
extern int64_t xtab_row_count(void* table);
extern int64_t xtab_col_count(void* table);
