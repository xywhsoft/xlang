#define XRT_MODULE_ALL
#include "xrt.h"
#include "../../extlibs/stb_image/include/stb_image_native.h"
#include "../../extlibs/stb_image_write/include/stb_image_write_native.h"
#include "../../extlibs/stb_image/stb_image_native.c"
#include "../../extlibs/stb_image_write/stb_image_write_native.c"

#include <stdio.h>
#include <string.h>

int main(int argc, char** argv)
{
    str AppDir = argc > 2 ? NULL : xrtPathAppDir();
    str DefaultSource = AppDir != NULL ? xrtPathJoin(AppDir,
        "examples/extlibs/assets/tiny.png") : NULL;
    const char* Source = argc > 2 ? argv[2] : DefaultSource;
    char* WorkDir = argc > 1 ? NULL : xrtDirTemp(NULL, "xlang_stbiw_example_", NULL);
    const char* OutDir = argc > 1 ? argv[1] : WorkDir;
    void* Image = Source != NULL ? xstbi_load(Source, 4) : NULL;
    void* Floats = Source != NULL ? xstbi_loadf(Source, 4) : NULL;
    int Ok = OutDir != NULL && Image != NULL && Floats != NULL;
    int Width = Ok ? xstbi_image_width(Image) : 0;
    int Height = Ok ? xstbi_image_height(Image) : 0;
    int Channels = Ok ? xstbi_image_channels(Image) : 0;
    xbuffer* Png = NULL;
    xbuffer* Jpeg = NULL;
    xbuffer* Bmp = NULL;
    xbuffer* Tga = NULL;
    xbuffer* Hdr = NULL;
    void* Roundtrip = NULL;
    const char* Names[5] = {"tiny.png", "tiny.jpg", "tiny.bmp", "tiny.tga", "tiny.hdr"};
    char* Destinations[5] = {0};

    if (Ok) {
        void* Pixels = xstbi_image_data(Image);
        size_t Size = xstbi_image_byte_size(Image);
        Png = (xbuffer*)xstbiw_encode(XSTBIW_PNG, Pixels, Size,
            Width, Height, Channels, 0);
        Jpeg = (xbuffer*)xstbiw_encode(XSTBIW_JPEG, Pixels, Size,
            Width, Height, Channels, 90);
        Bmp = (xbuffer*)xstbiw_encode(XSTBIW_BMP, Pixels, Size,
            Width, Height, Channels, 0);
        Tga = (xbuffer*)xstbiw_encode(XSTBIW_TGA, Pixels, Size,
            Width, Height, Channels, 0);
        Hdr = (xbuffer*)xstbiw_encode(XSTBIW_HDR,
            xstbi_image_data(Floats), xstbi_image_byte_size(Floats),
            Width, Height, Channels, 0);
        Ok = Png != NULL && Jpeg != NULL && Bmp != NULL &&
            Tga != NULL && Hdr != NULL;
        if (Ok) {
            Roundtrip = xstbi_load_memory(Png->Data, Png->Size, 4, 0);
            Ok = Roundtrip != NULL &&
                memcmp(xstbi_image_data(Roundtrip), Pixels, Size) == 0;
        }
        if (Ok) Ok = xrtDirCreateAll(OutDir);
        /* C 与 XLang 范例写出同样的五种格式，不能只比较成功消息。 */
        for (int i = 0; Ok && i < 5; ++i) {
            void* Input = i == 4 ? xstbi_image_data(Floats) : Pixels;
            size_t InputSize = i == 4 ? xstbi_image_byte_size(Floats) : Size;
            Destinations[i] = xrtPathJoin(OutDir, Names[i]);
            Ok = Destinations[i] != NULL &&
                xstbiw_write(Destinations[i], i + 1, Input, InputSize,
                    Width, Height, Channels, i == 1 ? 90 : 0);
        }
        if (Ok) {
            size_t FileSize = 0;
            bytes FileData = xrtFileReadAllLimit(Destinations[0], Png->Size, &FileSize);
            Ok = FileData != NULL && FileSize == Png->Size &&
                memcmp(FileData, Png->Data, FileSize) == 0;
            xrtFree(FileData);
        }
    }
    xstbi_image_release(Roundtrip);
    xrtBufferDestroy(Hdr);
    xrtBufferDestroy(Tga);
    xrtBufferDestroy(Bmp);
    xrtBufferDestroy(Jpeg);
    xrtBufferDestroy(Png);
    xstbi_image_release(Floats);
    xstbi_image_release(Image);
    if (WorkDir != NULL) {
        for (int i = 0; i < 5; ++i) {
            if (Destinations[i] != NULL && xrtFileExists(Destinations[i]) &&
                !xrtFileDelete(Destinations[i])) Ok = 0;
        }
        if (!xrtDirRemove(WorkDir)) Ok = 0;
    }
    for (int i = 0; i < 5; ++i) xrtFree(Destinations[i]);
    xrtFree(WorkDir);
    xrtFree(DefaultSource);
    xrtFree(AppDir);
    if (Ok) puts("stb_image_write C/XL example: PASS");
    return Ok ? 0 : 2;
}
