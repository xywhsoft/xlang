#include <stdint.h>
#include "../include/fuzzy_native.h"

/* XLang int 的 C ABI 必须是 int64_t，包括 LP64 平台。 */
extern int64_t xfuzzy_levenshtein(const char* a, const char* b);
extern int64_t xfuzzy_ratio(const char* a, const char* b);
extern int64_t xfuzzy_match(const char* pattern, const char* text);
