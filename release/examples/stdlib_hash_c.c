#define XRT_MODULE_HASH32
#define XRT_MODULE_HASH64
#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



/* 检查哈希示例中的稳定性与 seed 语义。 */
static void require_hash(int ok)
{
    if ( !ok ) {
        fprintf(stderr, "hash=fail\n");
        exit(1);
    }
}



/* 演示当前 XRT 非密码哈希 API。 */
int main(void)
{
    const char* text = "xlang";
    size_t len = strlen(text);
    uint32 id32 = xrtHash32(text, len);
    uint64 id64 = xrtHash64(text, len);
    uint32 seeded32 = xrtHash32Seed(text, len, 123u);
    uint64 seeded64 = xrtHash64Seed(text, len, 123ull);

    require_hash(id32 == xrtHash32("xlang", 5u));
    require_hash(id64 == xrtHash64("xlang", 5u));
    require_hash(seeded32 == xrtHash32Seed("xlang", 5u, 123u));
    require_hash(seeded64 == xrtHash64Seed("xlang", 5u, 123ull));

    require_hash(id32 != xrtHash32("xlang!", 6u));
    require_hash(id64 != xrtHash64("xlang!", 6u));
    require_hash(seeded32 != xrtHash32Seed("xlang", 5u, 124u));
    require_hash(seeded64 != xrtHash64Seed("xlang", 5u, 124ull));

    printf("hash=ok\n");
    return 0;
}
