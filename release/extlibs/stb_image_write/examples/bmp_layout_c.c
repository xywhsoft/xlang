/* 与 XLang 相同的三份像素；输出缓冲独立拥有，输入仅借用。 */
#include "../stb_image_write_native.c"
#include <stdio.h>

int main(void)
{
    unsigned char pixels[] = {255};
    unsigned char rgbPixels[] = {255,0,0, 0,255,0, 0,0,255, 255,255,255};
    unsigned char rgbaPixels[] = {255,0,0,128};
    xbuffer* gray = xstbiw_encode(XSTBIW_BMP, pixels, sizeof(pixels), 1, 1, 1, 0);
    xbuffer* rgb = xstbiw_encode(XSTBIW_BMP, rgbPixels, sizeof(rgbPixels), 2, 2, 3, 0);
    xbuffer* rgba = xstbiw_encode(XSTBIW_BMP, rgbaPixels, sizeof(rgbaPixels), 1, 1, 4, 0);
    int ok = gray && rgb && rgba && !xrtGetError() &&
        gray->Size == 58 && gray->Data[10] == 54 && gray->Data[54] == 255 &&
        rgb->Size == 70 && rgb->Data[10] == 54 &&
        rgba->Size == 126 && rgba->Data[10] == 122 && rgba->Data[125] == 128 && pixels[0] == 255;
    if (ok) puts("gray BMP: 58 bytes, pixels at 54\nRGB BMP: 70 bytes, pixels at 54\n"
        "RGBA BMP: 126 bytes, pixels at 122\nsource unchanged: true\nstb write BMP layout: OK");
    xrtBufferDestroy(rgba);
    xrtBufferDestroy(rgb);
    xrtBufferDestroy(gray);
    return ok ? 0 : 1;
}
