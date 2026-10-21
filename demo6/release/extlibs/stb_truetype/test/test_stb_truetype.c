#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/stb_truetype_native.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "stb_truetype C test failed at line %d: %s\n", \
        __LINE__, #expr); return 2; \
} } while (0)

int main(void)
{
    char* WorkDir = NULL;
    char* Temporary = NULL;
    char Invalid[28] = {0};
    char Ttc[16] = "ttcf";
    cstr Path = "release/extlibs/stb_truetype/test/assets/DejaVuSans-test.ttf";
    size_t Size = 0u;
    bytes Data;
    void* FileFont;
    void* MemoryFont;
    void* Bitmap;
    void* Space;

    CHECK(xstbtt_available() && xstbtt_version()[0] != '\0');
    CHECK(Path != NULL && Path[0] != '\0');
    Data = xrtFileReadAllLimit(Path, 32u * 1024u * 1024u, &Size);
    CHECK(Data != NULL && Size > 12u);
    {
        unsigned point;
        unsigned failures = 0u;
        for (point = 0u; point < 16u; ++point) {
            int triggered;
            void* Loaded;
            CHECK(xrtMemDebugFailAfter(point));
            Loaded = xstbtt_load(Path, 0);
            triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (triggered) {
                ++failures;
                CHECK(Loaded == NULL && xrtGetError() != NULL &&
                    xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            } else {
                CHECK(Loaded != NULL && xrtGetError() == NULL);
                xstbtt_font_release(Loaded);
            }
        }
        CHECK(failures >= 2u);
    }
    WorkDir = xrtDirTemp(NULL, "xlang_stbtt_c_test_", NULL);
    CHECK(WorkDir != NULL);
    Temporary = xrtPathJoin(WorkDir, "字体_临时.ttf");
    CHECK(Temporary != NULL);
    {
        const unsigned char Marker = 0u;
        CHECK(xrtFileWriteAll(Temporary, (xbytesview){&Marker, 1u}));
        CHECK(xrtFileSetSize(Temporary, 32u * 1024u * 1024u + 1u));
        CHECK(xstbtt_load(Temporary, 0) == NULL && xrtGetError() != NULL &&
            xrtErrorKind(xrtGetError()) == XERR_RANGE);
        CHECK(xrtFileDelete(Temporary));
    }
    FileFont = xstbtt_load(Path, 0);
    MemoryFont = xstbtt_load_memory(Data, Size, 0);
    CHECK(FileFont != NULL && MemoryFont != NULL);
    CHECK(xstbtt_load_memory(NULL, 0u, 0) == NULL && xrtGetError() != NULL);
    {
        void* Recovered = xstbtt_load_memory(Data, Size, 0);
        CHECK(Recovered != NULL && xrtGetError() == NULL);
        xstbtt_font_release(Recovered);
    }
    xrtFree(Data);
    CHECK(xstbtt_font_ascent(FileFont) == xstbtt_font_ascent(MemoryFont));
    CHECK(xstbtt_font_scale(MemoryFont, 24.0) > 0.0);
    CHECK(xstbtt_font_glyph(MemoryFont, 'A') > 0);
    CHECK(xstbtt_font_advance(MemoryFont, 'A') > 0);
    CHECK(xstbtt_font_descent(MemoryFont) <= 0);
    CHECK(xstbtt_font_kern(MemoryFont, 'A', 'V') ==
        xstbtt_font_kern(FileFont, 'A', 'V'));

    Bitmap = xstbtt_font_bitmap(MemoryFont, 'A', 24.0);
    CHECK(Bitmap != NULL && xstbtt_bitmap_width(Bitmap) > 0 &&
        xstbtt_bitmap_height(Bitmap) > 0);
    CHECK(xstbtt_bitmap_size(Bitmap) ==
        (size_t)xstbtt_bitmap_width(Bitmap) * (size_t)xstbtt_bitmap_height(Bitmap));
    CHECK(xstbtt_bitmap_data(Bitmap) != NULL);
    Space = xstbtt_font_bitmap(MemoryFont, ' ', 24.0);
    CHECK(Space != NULL && xstbtt_bitmap_size(Space) == 0u);
    xstbtt_bitmap_release(Space);
    xstbtt_bitmap_release(Bitmap);

    CHECK(xstbtt_font_bitmap(MemoryFont, 'A', NAN) == NULL);
    CHECK(xrtGetError() != NULL);
    Bitmap = xstbtt_font_bitmap(MemoryFont, 'A', 24.0);
    CHECK(Bitmap != NULL && xrtGetError() == NULL);
    xstbtt_bitmap_release(Bitmap);
    CHECK(xstbtt_font_bitmap(MemoryFont, 0x110000, 24.0) == NULL);
    CHECK(xstbtt_font_bitmap(MemoryFont, 'A', 2048.0) == NULL);
    CHECK(xstbtt_font_scale(MemoryFont, INFINITY) == 0.0);
    CHECK(xstbtt_load_memory(NULL, 0u, 0) == NULL);
    Invalid[1] = 1;
    Invalid[5] = 1;
    Invalid[20] = (char)0xff;
    CHECK(xstbtt_load_memory(Invalid, sizeof(Invalid), 0) == NULL);
    CHECK(xstbtt_load_memory(Ttc, sizeof(Ttc), 0) == NULL);
    CHECK(xstbtt_load(Path, -1) == NULL);
    CHECK(xstbtt_load("missing_font.ttf", 0) == NULL);

    Data = xrtFileReadAllLimit(Path, 32u * 1024u * 1024u, &Size);
    CHECK(Data != NULL && xrtFileWriteAll(Temporary, (xbytesview){Data, Size}));
    xrtFree(Data);
    Bitmap = xstbtt_load(Temporary, 0);
    CHECK(Bitmap != NULL && xstbtt_font_ascent(Bitmap) == xstbtt_font_ascent(FileFont));
    xstbtt_font_release(Bitmap);
    CHECK(xrtFileDelete(Temporary));
    CHECK(xrtDirRemove(WorkDir));
    xrtFree(Temporary);
    xrtFree(WorkDir);
    xstbtt_font_release(MemoryFont);
    xstbtt_font_release(FileFont);
    puts("stb_truetype C boundaries: PASS");
    return 0;
}
