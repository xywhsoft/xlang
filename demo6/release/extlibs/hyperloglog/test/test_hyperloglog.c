#include "xl_runtime.h"
#include <stdio.h>

extern void *xhll_create(int precision);
extern void xhll_destroy(void *handle);
extern void xhll_add(void *handle, const char *data, long long length);
extern long long xhll_count(void *handle);
extern int xhll_merge(void *destination, void *source);

int main(void)
{
    void *left = xhll_create(12);
    void *right = xhll_create(12);
    if (left == NULL || right == NULL) return 1;

    xhll_add(left, "alpha", -1);
    xhll_add(right, "beta", -1);
    if (!xhll_merge(left, right)) return 2;
    if (xhll_count(left) < 1) return 3;
    xhll_destroy(right);
    xhll_destroy(left);
    return 0;
}
