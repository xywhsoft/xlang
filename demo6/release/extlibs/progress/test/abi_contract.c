#include <stdint.h>
#include "../include/progress_native.h"

/* XLang int 参数和结果必须采用平台 int64_t 原型。 */
extern void *xprog_create(int64_t total);
extern int xprog_advance(void *handle, int64_t count);
extern int xprog_set(void *handle, int64_t current);
extern int64_t xprog_current(void *handle);
extern int64_t xprog_total(void *handle);
