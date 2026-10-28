#define XRT_MODULE_ALL
#include "xrt.h"
#include <stdio.h>

static xvalue* borrowed[16];
static size_t borrowed_count;
static unsigned borrowed_releases;
static bool overflow;

/* 此范例的 ObjectGet 结果只借用父对象，不另行 retain。 */
static xvalue* checked_get(const xvalue* object, xstrview key)
{
    xvalue* value = xrtValueObjectGet(object, key);
    if (value != NULL) {
        size_t i;
        for (i = 0; i < borrowed_count && borrowed[i] != value; ++i) {}
        if (i == borrowed_count) {
            if (borrowed_count == sizeof(borrowed) / sizeof(borrowed[0])) overflow = true;
            else borrowed[borrowed_count++] = value;
        }
    }
    return value;
}

static void checked_release(xvalue* value)
{
    size_t i;
    for (i = 0; i < borrowed_count; ++i) {
        if (value == borrowed[i]) {
            ++borrowed_releases;
            return;
        }
    }
    xrtValueRelease(value);
}

#define xrtValueObjectGet checked_get
#define xrtValueRelease checked_release
#define main redis_example_main
#include "../../../examples/extlibs/redis_c.c"
#undef main
#undef xrtValueObjectGet
#undef xrtValueRelease

/* 直接审计调用合同，不依赖分配池能否检测释放后读取。 */
int main(void)
{
    borrowed_count = borrowed_releases = 0u;
    overflow = false;
    xrtClearError();
    int result = redis_example_main();
    if (result != 0 || borrowed_releases != 0u || overflow || xrtGetError() != NULL) {
        fprintf(stderr, "redis C example cleanup failed: borrowed releases=%u\n",
            borrowed_releases);
        return 1;
    }
    puts("redis C example lifetime: PASS");
    return 0;
}
