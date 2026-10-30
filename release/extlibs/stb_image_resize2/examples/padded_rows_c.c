/* 与 XLang 同一组灰度像素；输入仅借用，输出缓冲分别拥有。 */
#include "../stb_image_resize2_native.c"
#include <stdio.h>

int main(void)
{
    unsigned char pixels[] = {255, 0, 165, 255, 0};
    const unsigned char expected[] = {255, 0, 165, 255, 0};
    xbuffer* srgb = xstbir_resize(pixels, sizeof(pixels), 2, 2, 1, 1, 1, 3, 0);
    xbuffer* linear = xstbir_resize(pixels, sizeof(pixels), 2, 2, 1, 1, 1, 3, 1);
    xbuffer* shortResult = xstbir_resize(pixels, sizeof(pixels) - 1, 2, 2, 1, 1, 1, 3, 0);
    bool rejected = !shortResult && xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_RANGE;
    xbuffer* recovered = xstbir_resize(pixels, sizeof(pixels), 2, 2, 1, 1, 1, 3, 0);
    int ok = srgb && linear && recovered && srgb->Size == 1 && linear->Size == 1 && recovered->Size == 1 &&
        srgb->Data[0] == 188 && linear->Data[0] == 128 && recovered->Data[0] == 188 &&
        rejected && !xrtGetError() && memcmp(pixels, expected, sizeof(pixels)) == 0;
    if (ok) {
        printf("source bytes: %u\nsRGB midpoint: %u\nlinear midpoint: %u\n",
            (unsigned)sizeof(pixels), (unsigned)srgb->Data[0], (unsigned)linear->Data[0]);
        puts("short stride rejected\nsource unchanged: true\nstb resize padded rows: OK");
    }
    xrtBufferDestroy(shortResult);
    xrtBufferDestroy(recovered);
    xrtBufferDestroy(linear);
    xrtBufferDestroy(srgb);
    return ok ? 0 : 1;
}
