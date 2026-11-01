/* 使用原生float32创建高动态范围灰度；可选输出路径走公开原子写接口。 */
#include "../stb_image_write_native.c"
#include <string.h>

int main(int argc, char** argv)
{
    float pixels[] = {0, 0.125f, 0.5f, 1, 2, 4, 8, 16};
    float saved[8];
    memcpy(saved, pixels, sizeof(pixels));
    xbuffer* encoded = xstbiw_encode(XSTBIW_HDR, pixels, sizeof(pixels), 8, 1, 1, 0);
    bool ok = encoded != NULL && xrtGetError() == NULL;
    xrtBufferDestroy(encoded);
    if (argc > 1) ok = xstbiw_write(argv[1], XSTBIW_HDR, pixels, sizeof(pixels), 8, 1, 1, 0) && ok;
    float invalid = -1;
    encoded = xstbiw_encode(XSTBIW_HDR, &invalid, sizeof(invalid), 1, 1, 1, 0);
    ok = encoded == NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE &&
        memcmp(saved, pixels, sizeof(pixels)) == 0 && ok;
    xrtBufferDestroy(encoded);
    if (!ok) return 1;
    puts("gray HDR: 8x1");
    puts("values above one: true");
    puts("invalid color: rejected");
    puts("source unchanged: true");
    return 0;
}
