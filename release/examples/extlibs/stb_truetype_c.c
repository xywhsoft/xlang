#define XRT_MODULE_ALL
#include "xrt.h"
#include "../../extlibs/stb_truetype/include/stb_truetype_native.h"
#include "../../extlibs/stb_truetype/stb_truetype_native.c"

#include <stdio.h>

int main(int argc, char** argv)
{
    str AppDir = argc > 1 ? NULL : xrtPathAppDir();
    str Path = AppDir != NULL ? xrtPathJoin(AppDir,
        "extlibs/stb_truetype/test/assets/DejaVuSans-test.ttf") : NULL;
    cstr FontPath = argc > 1 ? argv[1] : Path;
    void* Font = FontPath != NULL ? xstbtt_load(FontPath, 0) : NULL;
    void* Glyph = Font != NULL ? xstbtt_font_bitmap(Font, 'A', 24.0) : NULL;
    int Ok = Font != NULL && Glyph != NULL &&
        xstbtt_font_glyph(Font, 'A') > 0 &&
        xstbtt_font_advance(Font, 'A') > 0 &&
        xstbtt_bitmap_width(Glyph) > 0 &&
        xstbtt_bitmap_height(Glyph) > 0 &&
        xstbtt_bitmap_data(Glyph) != NULL;

    if (Ok) {
        printf("glyph A bytes:  %llu\n",
            (unsigned long long)xstbtt_bitmap_size(Glyph));
        puts("stb_truetype C/XL example: PASS");
    }
    xstbtt_bitmap_release(Glyph);
    xstbtt_font_release(Font);
    xrtFree(Path);
    xrtFree(AppDir);
    return Ok ? 0 : 2;
}
