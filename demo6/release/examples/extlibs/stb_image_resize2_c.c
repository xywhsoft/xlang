#define XRT_MODULE_ALL
#include "xrt.h"
#include "../../extlibs/stb_image/include/stb_image_native.h"
#include "../../extlibs/stb_image_resize2/include/stb_image_resize2_native.h"
#include "../../extlibs/stb_image/stb_image_native.c"
#include "../../extlibs/stb_image_resize2/stb_image_resize2_native.c"

#include <stdio.h>
#include <string.h>

int main(void)
{
    str AppDir = xrtPathAppDir();
    str Path = AppDir != NULL ? xrtPathJoin(AppDir,
        "examples/extlibs/assets/tiny.png") : NULL;
    void* Image = Path != NULL ? xstbi_load(Path, 4) : NULL;
    xbuffer* Srgb = NULL;
    xbuffer* Linear = NULL;
    xbuffer* GrayWhite = NULL;
    xbuffer* GrayBlack = NULL;
    unsigned char TransparentWhite[4] = {255, 0, 0, 255};
    unsigned char TransparentBlack[4] = {0, 0, 0, 255};
    int Ok = Image != NULL;
    if (Ok) {
        void* Pixels = xstbi_image_data(Image);
        size_t Size = xstbi_image_byte_size(Image);
        int Width = xstbi_image_width(Image);
        int Height = xstbi_image_height(Image);
        Srgb = (xbuffer*)xstbir_resize(Pixels, Size,
            Width, Height, 4, 4, 4, 0, 0);
        Linear = (xbuffer*)xstbir_resize(Pixels, Size,
            Width, Height, 4, 3, 3, 0, 1);
        Ok = Srgb != NULL && Linear != NULL &&
            Srgb->Size == 64u && Linear->Size == 36u;
    }
    if (Ok) {
        GrayWhite = (xbuffer*)xstbir_resize(TransparentWhite,
            sizeof(TransparentWhite), 2, 1, 2, 1, 1, 0, 0);
        GrayBlack = (xbuffer*)xstbir_resize(TransparentBlack,
            sizeof(TransparentBlack), 2, 1, 2, 1, 1, 0, 0);
        Ok = GrayWhite != NULL && GrayBlack != NULL &&
            GrayWhite->Size == 2u && GrayBlack->Size == 2u &&
            memcmp(GrayWhite->Data, GrayBlack->Data, 2u) == 0;
    }
    if (Ok) {
        printf("sRGB bytes:  %llu\n", (unsigned long long)Srgb->Size);
        printf("linear bytes:  %llu\n", (unsigned long long)Linear->Size);
        printf("gray-alpha bytes:  %llu\n", (unsigned long long)GrayWhite->Size);
        puts("stb_image_resize2 C/XL example: PASS");
    }
    xrtBufferDestroy(GrayBlack);
    xrtBufferDestroy(GrayWhite);
    xrtBufferDestroy(Linear);
    xrtBufferDestroy(Srgb);
    xstbi_image_release(Image);
    xrtFree(Path);
    xrtFree(AppDir);
    return Ok ? 0 : 2;
}
