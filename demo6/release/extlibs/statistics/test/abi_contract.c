#include <stdint.h>
#include "../statistics_native.h"

/* XLang int 的公开 C 参数和结果均使用 int64_t。 */
extern int64_t xstat_count(xvalue *values);
extern xvalue *xstat_histogram(xvalue *values, int64_t bins);
