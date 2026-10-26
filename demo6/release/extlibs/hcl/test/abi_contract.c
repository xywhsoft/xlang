#include <stdint.h>
#include "../include/hcl_native.h"

/* 整数与索引必须使用 XLang int 的 int64_t ABI。 */
extern int64_t xhcl_get_int(const char* hcl, const char* key);
extern int64_t xhcl_count_blocks(const char* hcl, const char* block_type);
extern char* xhcl_get(const char* hcl, const char* key);
extern char* xhcl_get_raw(const char* hcl, const char* key);
extern char* xhcl_block_label(const char* hcl, const char* block_type, int64_t index);
extern int64_t xhcl_block_label_count(const char* hcl, const char* block_type,
    int64_t index);
extern char* xhcl_block_label_at(const char* hcl, const char* block_type,
    int64_t index, int64_t label_index);
extern int64_t xhcl_count_attrs(const char* hcl);
