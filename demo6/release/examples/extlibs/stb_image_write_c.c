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
    str AppDir = xrtPathAppDir();
    str Source = AppDir != NULL ? xrtPathJoin(AppDir,
        "examples/extlibs/assets/tiny.png") : NULL;
    char* WorkDir = argc > 1 ? NULL : xrtDirTemp(NULL, "xlang_stbiw_example_", NULL);
    const char* OutDir = argc > 1 ? argv[1] : WorkDir;
    char* Destination = OutDir != NULL ? xrtPathJoin(OutDir, "tiny.png") : NULL;
    void* Image = Source != NULL ? xstbi_load(Source, 4) : NULL;
    void* Floats = Source != NULL ? xstbi_loadf(Source, 4) : NULL;
    int Ok = Destination != NULL && Image != NULL && Floats != NULL;
    int Width = Ok ? xstbi_image_width(Image) : 0;
    int Height = Ok ? xstbi_image_height(Image) : 0;
    int Channels = Ok ? xstbi_image_channels(Image) : 0;
    xbuffer* Png = NULL;
    xbuffer* Jpeg = NULL;
    xbuffer* Bmp = NULL;
    xbuffer* Tga = NULL;
    xbuffer* Hdr = NULL;
    void* Roundtrip = NULL;

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
        if (Ok) {
            Ok = xrtDirCreateAll(OutDir) &&
                xstbiw_write(Destination, XSTBIW_PNG, Pixels, Size,
                    Width, Height, Channels, 0);
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
        if (Destination != NULL && xrtFileExists(Destination) && !xrtFileDelete(Destination)) Ok = 0;
        if (!xrtDirRemove(WorkDir)) Ok = 0;
    }
    xrtFree(Destination);
    xrtFree(WorkDir);
    xrtFree(Source);
    xrtFree(AppDir);
    if (Ok) puts("stb_image_write C/XL example: PASS");
    return Ok ? 0 : 2;
}
