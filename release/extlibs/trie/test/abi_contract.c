#include <stdint.h>
#include "../include/trie_native.h"

/* XLang int 的前缀计数与总数使用 int64_t。 */
extern int64_t xtrie_count_prefix(void *handle, const char *prefix, size_t size);
extern int64_t xtrie_count(void *handle);
