#define XRT_MODULE_ALL
#include "xrt.h"
#include "../../extlibs/stb_image/include/stb_image_native.h"
#include "../../extlibs/stb_image/stb_image_native.c"

#include <stdio.h>

int main(int argc, char** argv)
{
    str AppDir = argc > 1 ? NULL : xrtPathAppDir();
    str Path = AppDir != NULL ? xrtPathJoin(AppDir,
        "examples/extlibs/assets/tiny.png") : NULL;
    cstr ImagePath = argc > 1 ? argv[1] : Path;
    size_t Size = 0u;
    bytes Encoded = ImagePath != NULL ? xrtFileReadAllLimit(ImagePath, 128u * 1024u * 1024u, &Size) : NULL;
    void* Image = ImagePath != NULL ? xstbi_load(ImagePath, 4) : NULL;
    void* FromMemory = Encoded != NULL ? xstbi_load_memory(Encoded, Size, 4, 0) : NULL;
    void* Floats = Encoded != NULL ? xstbi_load_memory(Encoded, Size, 4, 1) : NULL;
    int Ok = Image != NULL && FromMemory != NULL && Floats != NULL &&
        xstbi_image_valid(Image) && xstbi_image_channels(Image) == 4 &&
        xstbi_image_width(Image) == xstbi_image_width(FromMemory) &&
        xstbi_image_height(Image) == xstbi_image_height(FromMemory) &&
        xstbi_image_is_float(Floats);

    if (Ok) {
        printf("image:  %d x %d\n", xstbi_image_width(Image), xstbi_image_height(Image));
        printf("channels:  %d\n", xstbi_image_channels(Image));
        printf("pixel bytes:  %llu\n", (unsigned long long)xstbi_image_byte_size(Image));
        puts("stb_image C/XL example: PASS");
    }
    xstbi_image_release(Floats);
    xstbi_image_release(FromMemory);
    xstbi_image_release(Image);
    xrtFree(Encoded);
    xrtFree(Path);
    xrtFree(AppDir);
    return Ok ? 0 : 2;
}
