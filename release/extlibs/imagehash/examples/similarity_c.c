#include "../include/imagehash_native.h"
#include "xrt.h"
#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
#include "../../stb_image/stb_image_native.c"
#include "../../stb_image_resize2/stb_image_resize2_native.c"
#include "../imagehash_native.c"
#endif

/* 与 XLang 范例相同：比较同一种算法的结果，由业务选择阈值，不认证图像相等。 */
int main(int argc, char** argv)
{
    const char* left_path = argc == 3 ? argv[1] : "release/examples/extlibs/assets/tiny.png";
    const char* right_path = argc == 3 ? argv[2] : "release/examples/games/assets/player.png";
    char* left = xihash_phash(left_path);
    char* right = NULL;
    int result = 1;
    if (!left) goto done;
    right = xihash_phash(right_path);
    if (!right || !xihash_valid(left, strlen(left)) || !xihash_valid(right, strlen(right))) goto done;
    int distance = xihash_hamming(left, strlen(left), right, strlen(right));
    if (distance < 0) goto done;
    printf("pHash distance: %d\nwithin threshold 4: %s\n", distance, distance <= 4 ? "true" : "false");
    result = 0;
done:
    if (result) fprintf(stderr, "imagehash: %s\n", xihash_last_error());
    xrtFree(left);
    xrtFree(right);
    return result;
}
