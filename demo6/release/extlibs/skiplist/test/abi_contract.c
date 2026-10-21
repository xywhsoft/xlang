#include <stdint.h>
#include "../include/skiplist_native.h"

/* XLang int 的 C ABI 必须与 LP64 平台的 int64_t 一致。 */
extern int64_t xskiplist_count(void *handle);
