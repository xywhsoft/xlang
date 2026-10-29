/* 直接验证包内分裂启发式，不把节点布局暴露为公开 ABI。 */
#include "../rtree_native.c"
#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "rtree split seed test failed at line %d: %s\n", \
        __LINE__, #condition); \
    return 1; \
} } while (0)

int main(void)
{
    XrtreeNode left = {0}, right = {0};
    int i;
    left.leaf = 1;
    left.count = XRTREE_MAX_ENTRIES + 1;
    for (i = 0; i < left.count; ++i) {
        double x = (double)i * 1.0e155;
        double width = i < 2 ? 1.6e154 : 1.2e154;
        left.entries[i] = (XrtreeEntry){{x, 0, x + width, 1.0e154}, i, NULL};
    }
    /* 每对组合包围盒都饱和；较小的第 2、3 项仍有最大的面积浪费。 */
    CHECK(xrtree_area(xrtree_combine(left.entries[2].rect,
        left.entries[3].rect)) == (long double)DBL_MAX);
    xrtree_split(&left, &right);
    CHECK(left.entries[0].id == 2 && right.entries[0].id == 3);
    CHECK(left.count >= XRTREE_MIN_ENTRIES &&
        right.count >= XRTREE_MIN_ENTRIES && left.count + right.count == 9);
    puts("rtree split seeds: PASS");
    return 0;
}
