#include "../include/xrt_xxhash.h"
#include <stdio.h>

#ifdef __TINYC__
/* xl run 单文件范例：正式构建仍分别编译这两个源文件。 */
#include "../xrt_xxhash.c"
#include "../third_party/xxhash.c"
#endif

int main(void)
{
    xrtxxhashoptions options;
    xrtxxhashhasher* h = NULL;
    uint64 h64;
    uint32 h32;
    int status = 1;
    if (!xrtXxhash64(NULL, 0, 0, &h64, NULL)) goto done;
    printf("empty64=%016llx\n", (unsigned long long)h64);
    if (!xrtXxhash32("abc", 3, 0, &h32, NULL)) goto done;
    printf("abc32=%08x\n", (unsigned int)h32);
    if (!xrtXxhash64("abc", 3, 1, &h64, NULL)) goto done;
    printf("seeded64=%016llx\n", (unsigned long long)h64);
    xrtXxhashOptionsInit(&options);
    h = xrtXxhashHasherCreate(&options);
    if (h == NULL || !xrtXxhashHasherUpdate(h, "a", 1) ||
        !xrtXxhashHasherUpdate(h, "bc", 2) ||
        !xrtXxhashHasherDigest(h, &h64)) goto done;
    printf("stream64=%016llx\n", (unsigned long long)h64);
    printf("bytes=%llu\n", (unsigned long long)xrtXxhashHasherInputBytes(h));
    if (!xrtXxhashHasherReset(h) || !xrtXxhashHasherDigest(h, &h64)) goto done;
    printf("reset64=%016llx\n", (unsigned long long)h64);
    status = 0;
done:
    xrtXxhashHasherDestroy(h);
    return status;
}
