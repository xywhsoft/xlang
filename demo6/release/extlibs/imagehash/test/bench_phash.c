#include "imagehash_native.h"
#include "xrt.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

int main(void)
{
    const char* path = "release/examples/extlibs/assets/tiny.png";
    const int iterations = 50000;
    clock_t start = clock();
    int i;
    for (i = 0; i < iterations; ++i) {
        char* hash = (char*)xihash_phash(path);
        if (hash == NULL || strcmp(hash, "0000000000000000") != 0) {
            xrtFree(hash);
            return 1;
        }
        xrtFree(hash);
    }
    printf("imagehash phash: %d calls, %.1f ms CPU\n", iterations,
        1000.0 * (double)(clock() - start) / CLOCKS_PER_SEC);
    return 0;
}
