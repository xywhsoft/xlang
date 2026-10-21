#include <stdint.h>
#include "../include/langdetect_native.h"

/* XLang 的 int 在 C 边界采用 int64_t，不随 LP64 的 long long 改变。 */
extern int64_t xlangdetect_confidence(const char *text, size_t size);
