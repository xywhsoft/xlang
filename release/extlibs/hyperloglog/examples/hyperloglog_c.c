#include "../include/hyperloglog_native.h"
#include <inttypes.h>
#include <stdio.h>

int main(void)
{
    void* visitors = xhll_create(14);
    void* daily = xhll_create(12);
    void* next_day = xhll_create(12);
    char item[32];
    int i, result = 1;
    if (visitors == NULL || daily == NULL || next_day == NULL) goto done;
    printf("precision: %d\n", xhll_precision(visitors));
    for (i = 0; i < 10000; ++i) {
        snprintf(item, sizeof(item), "visitor:%d", i % 8000);
        if (!xhll_add(visitors, item, -1)) goto done;
    }
    printf("added: %" PRId64 "\n", xhll_added(visitors));
    printf("estimated unique: %" PRId64 "\n", xhll_count(visitors));
    if (!xhll_add(daily, "page:/home", -1) ||
        !xhll_add(daily, "page:/about", -1) ||
        !xhll_add(daily, "page:/home", -1)) goto done;
    printf("daily unique pages: %" PRId64 "\n", xhll_count(daily));
    if (!xhll_add(next_day, "page:/home", -1) ||
        !xhll_add(next_day, "page:/contact", -1) ||
        !xhll_merge(daily, next_day)) goto done;
    printf("merged unique pages: %" PRId64 "\n", xhll_count(daily));
    printf("merged added calls: %" PRId64 "\n", xhll_added(daily));
    if (!xhll_clear(daily)) goto done;
    printf("after clear unique pages: %" PRId64 "\n", xhll_count(daily));
    printf("after clear added: %" PRId64 "\n", xhll_added(daily));
    puts("hyperloglog example: PASS");
    result = 0;
done:
    xhll_destroy(next_day);
    xhll_destroy(daily);
    xhll_destroy(visitors);
    return result;
}

/* TCC 单文件运行带入实现；正式包与 GCC 范例仍分离编译。 */
#ifdef __TINYC__
#include "../hyperloglog_native.c"
#endif
