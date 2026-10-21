#include "../include/imagehash_native.h"
#include "xrt.h"

#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
/* 单文件教学入口：正式包构建仍按 manifest 分离编译三个实现。 */
#include "../../stb_image/stb_image_native.c"
#include "../../stb_image_resize2/stb_image_resize2_native.c"
#include "../imagehash_native.c"
#endif

int main(void) {
    const char *path = "release/examples/extlibs/assets/tiny.png";
    const char *other_path = "release/examples/games/assets/player.png";
    char *a = xihash_ahash(path);
    char *d = xihash_dhash(path);
    char *p = xihash_phash(path);
    char *other_p = xihash_phash(other_path);
    int distance;
    int status = 1;
    if (a == NULL || d == NULL || p == NULL || other_p == NULL) goto done;
    distance = xihash_hamming(p, strlen(p), other_p, strlen(other_p));
    if (distance < 0) goto done;
    printf("aHash: %s\n", a);
    printf("dHash: %s\n", d);
    printf("pHash: %s\n", p);
    printf("pHash(other): %s\n", other_p);
    printf("distance(p,pOther): %d\n", distance);
    status = 0;
done:
    if (status != 0) fprintf(stderr, "imagehash: %s\n", xihash_last_error());
    xrtFree(a);
    xrtFree(d);
    xrtFree(p);
    xrtFree(other_p);
    return status;
}
