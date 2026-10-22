/* 字体与位图分别拥有内存；XRT 负责 UTF-8 文件路径和内存分配。 */
#include <math.h>
#include <stdint.h>
#include <string.h>

#ifndef XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_FILE_WHOLE
#endif
#ifndef XRT_MODULE_PATH
#define XRT_MODULE_PATH
#endif
#ifndef XRT_MODULE_ENVIRONMENT
#define XRT_MODULE_ENVIRONMENT
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#include "xrt.h"
#include "include/stb_truetype_native.h"

#define STBTT_malloc(size, user) ((void)(user), xrtMalloc(size))
#define STBTT_free(ptr, user) ((void)(user), xrtFree(ptr))
#define STB_TRUETYPE_IMPLEMENTATION
#include "third_party/stb_truetype.h"

#define XSTBTT_DOMAIN "xlang.extlib.stb_truetype"
#define XSTBTT_FONT_LIMIT (32u * 1024u * 1024u)
#define XSTBTT_BITMAP_LIMIT (64u * 1024u * 1024u)

typedef struct xstbtt_font {
    bytes Bytes;
    size_t Size;
    stbtt_fontinfo Info;
} xstbtt_font;

typedef struct xstbtt_bitmap {
    int Width;
    int Height;
    int Xoff;
    int Yoff;
    bytes Pixels;
} xstbtt_bitmap;

static void xstbtt_error(xerrkind Kind, const char* Message)
{
    xrtSetErrorInfo(Kind, XSTBTT_DOMAIN, 0, Message);
}

static uint32_t xstbtt_u32(const uint8_t* Data)
{
    return ((uint32_t)Data[0] << 24) | ((uint32_t)Data[1] << 16) |
        ((uint32_t)Data[2] << 8) | (uint32_t)Data[3];
}

static size_t xstbtt_font_offset(const uint8_t* Data, size_t Size, int Index)
{
    size_t Offset = 0u;
    size_t Directory;
    unsigned Tables;
    unsigned i;

    if (Data == NULL || Size < 12u || Size > XSTBTT_FONT_LIMIT || Index < 0)
        return SIZE_MAX;
    if (memcmp(Data, "ttcf", 4u) == 0) {
        uint32_t Count = xstbtt_u32(Data + 8u);
        if ((size_t)Count > (Size - 12u) / 4u || (uint32_t)Index >= Count)
            return SIZE_MAX;
        Offset = xstbtt_u32(Data + 12u + (size_t)Index * 4u);
    } else if (Index != 0) {
        return SIZE_MAX;
    }
    if (Offset > Size - 12u) return SIZE_MAX;
    if (xstbtt_u32(Data + Offset) != 0x00010000u &&
        memcmp(Data + Offset, "OTTO", 4u) != 0 &&
        memcmp(Data + Offset, "true", 4u) != 0 &&
        memcmp(Data + Offset, "typ1", 4u) != 0 &&
        memcmp(Data + Offset, "1\0\0\0", 4u) != 0)
        return SIZE_MAX;

    /* 先验证表目录及所有表的文件边界，再让 stb 解析字体。 */
    Tables = ((unsigned)Data[Offset + 4u] << 8) | Data[Offset + 5u];
    if (Tables == 0u || Tables > 256u || (size_t)Tables >
            (Size - Offset - 12u) / 16u)
        return SIZE_MAX;
    Directory = Offset + 12u;
    for (i = 0u; i < Tables; ++i) {
        size_t Entry = Directory + (size_t)i * 16u;
        uint32_t Start = xstbtt_u32(Data + Entry + 8u);
        uint32_t Length = xstbtt_u32(Data + Entry + 12u);
        if ((size_t)Start > Size || (size_t)Length > Size - (size_t)Start)
            return SIZE_MAX;
    }
    return Offset;
}

static bool xstbtt_codepoint(int Codepoint)
{
    return Codepoint >= 0 && Codepoint <= 0x10ffff &&
        (Codepoint < 0xd800 || Codepoint > 0xdfff);
}

int xstbtt_available(void)
{
    return 1;
}

const char* xstbtt_version(void)
{
    return "stb_truetype 1.26";
}

int xstbtt_error_is_memory(void)
{
    const xerror* Error = xrtGetError();
    return Error != NULL && xrtErrorKind(Error) == XERR_MEMORY;
}

void* xstbtt_default_font_path(void)
{
    static const char* Candidates[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf"
    };
    str Root = xrtEnvGet("WINDIR");
    size_t i;
    if (Root == NULL) Root = xrtEnvGet("SystemRoot");
    if (Root != NULL) {
        static const char* Names[] = {
            "Fonts/arial.ttf", "Fonts/segoeui.ttf", "Fonts/tahoma.ttf"
        };
        for (i = 0u; i < sizeof(Names) / sizeof(Names[0]); ++i) {
            str Path = xrtPathJoin(Root, Names[i]);
            if (Path != NULL && xrtFileExists(Path)) {
                xrtFree(Root);
                return Path;
            }
            xrtFree(Path);
        }
        xrtFree(Root);
    }
    for (i = 0u; i < sizeof(Candidates) / sizeof(Candidates[0]); ++i)
        if (xrtFileExists(Candidates[i])) return xrtStrDup(Candidates[i]);
    return xrtStrDup("");
}

void* xstbtt_load_memory(void* Data, size_t Size, int Index)
{
    size_t Offset;
    xstbtt_font* Font;
    xrtClearError();
    Offset = xstbtt_font_offset((const uint8_t*)Data, Size, Index);
    if (Offset == SIZE_MAX) {
        xstbtt_error(XERR_VALUE, "invalid font data or font index");
        return NULL;
    }
    Font = (xstbtt_font*)xrtCalloc(1u, sizeof(*Font));
    if (Font == NULL) {
        xstbtt_error(XERR_MEMORY, "could not allocate font object");
        return NULL;
    }
    Font->Bytes = (bytes)xrtMalloc(Size);
    if (Font->Bytes == NULL) {
        xrtFree(Font);
        xstbtt_error(XERR_MEMORY, "could not copy font data");
        return NULL;
    }
    memcpy(Font->Bytes, Data, Size);
    if (!stbtt_InitFont(&Font->Info, Font->Bytes, (int)Offset)) {
        xrtFree(Font->Bytes);
        xrtFree(Font);
        xstbtt_error(XERR_VALUE, "unsupported or corrupt font");
        return NULL;
    }
    Font->Size = Size;
    return Font;
}

void* xstbtt_load(const char* Path, int Index)
{
    size_t Size = 0u;
    bytes Data;
    void* Font;
    xrtClearError();
    if (Path == NULL || Path[0] == '\0') {
        xstbtt_error(XERR_ARGUMENT, "empty font path");
        return NULL;
    }
    Data = xrtFileReadAllLimit(Path, XSTBTT_FONT_LIMIT, &Size);
    if (Data == NULL) {
        if (xrtGetError() == NULL)
            xstbtt_error(XERR_IO, "could not read font file");
        return NULL;
    }
    Font = xstbtt_load_memory(Data, Size, Index);
    xrtFree(Data);
    return Font;
}

void* xstbtt_font_adopt(void* Font)
{
    return Font;
}

void xstbtt_font_release(void* Handle)
{
    xstbtt_font* Font = (xstbtt_font*)Handle;
    if (Font == NULL) return;
    xrtFree(Font->Bytes);
    xrtFree(Font);
}

double xstbtt_font_scale(void* Handle, double Height)
{
    xstbtt_font* Font = (xstbtt_font*)Handle;
    if (Font == NULL || !isfinite(Height) || Height <= 0.0 || Height > 1024.0)
        return 0.0;
    return stbtt_ScaleForPixelHeight(&Font->Info, (float)Height);
}

int xstbtt_font_ascent(void* Handle)
{
    int Ascent, Descent, Gap;
    if (Handle == NULL) return 0;
    stbtt_GetFontVMetrics(&((xstbtt_font*)Handle)->Info, &Ascent, &Descent, &Gap);
    return Ascent;
}

int xstbtt_font_descent(void* Handle)
{
    int Ascent, Descent, Gap;
    if (Handle == NULL) return 0;
    stbtt_GetFontVMetrics(&((xstbtt_font*)Handle)->Info, &Ascent, &Descent, &Gap);
    return Descent;
}

int xstbtt_font_line_gap(void* Handle)
{
    int Ascent, Descent, Gap;
    if (Handle == NULL) return 0;
    stbtt_GetFontVMetrics(&((xstbtt_font*)Handle)->Info, &Ascent, &Descent, &Gap);
    return Gap;
}

int xstbtt_font_glyph(void* Handle, int Codepoint)
{
    if (Handle == NULL || !xstbtt_codepoint(Codepoint)) return 0;
    return stbtt_FindGlyphIndex(&((xstbtt_font*)Handle)->Info, Codepoint);
}

int xstbtt_font_advance(void* Handle, int Codepoint)
{
    int Advance, Bearing;
    if (Handle == NULL || !xstbtt_codepoint(Codepoint)) return 0;
    stbtt_GetCodepointHMetrics(&((xstbtt_font*)Handle)->Info,
        Codepoint, &Advance, &Bearing);
    return Advance;
}

int xstbtt_font_bearing(void* Handle, int Codepoint)
{
    int Advance, Bearing;
    if (Handle == NULL || !xstbtt_codepoint(Codepoint)) return 0;
    stbtt_GetCodepointHMetrics(&((xstbtt_font*)Handle)->Info,
        Codepoint, &Advance, &Bearing);
    return Bearing;
}

int xstbtt_font_kern(void* Handle, int Left, int Right)
{
    if (Handle == NULL || !xstbtt_codepoint(Left) || !xstbtt_codepoint(Right))
        return 0;
    return stbtt_GetCodepointKernAdvance(&((xstbtt_font*)Handle)->Info,
        Left, Right);
}

void* xstbtt_font_bitmap(void* Handle, int Codepoint, double Height)
{
    xstbtt_font* Font = (xstbtt_font*)Handle;
    xstbtt_bitmap* Bitmap;
    int X0, Y0, X1, Y1;
    float Scale;
    size_t Width, Rows;
    xrtClearError();
    if (Font == NULL || !xstbtt_codepoint(Codepoint) ||
        !isfinite(Height) || Height <= 0.0 || Height > 1024.0) {
        xstbtt_error(XERR_ARGUMENT, "invalid glyph or pixel height");
        return NULL;
    }
    Scale = stbtt_ScaleForPixelHeight(&Font->Info, (float)Height);
    stbtt_GetCodepointBitmapBox(&Font->Info, Codepoint,
        Scale, Scale, &X0, &Y0, &X1, &Y1);
    if (X1 < X0 || Y1 < Y0) {
        xstbtt_error(XERR_VALUE, "glyph bitmap has invalid dimensions");
        return NULL;
    }
    Width = (size_t)((int64_t)X1 - (int64_t)X0);
    Rows = (size_t)((int64_t)Y1 - (int64_t)Y0);
    if (Rows != 0u && Width > XSTBTT_BITMAP_LIMIT / Rows) {
        xstbtt_error(XERR_RANGE, "glyph bitmap exceeds 64 MiB");
        return NULL;
    }
    Bitmap = (xstbtt_bitmap*)xrtCalloc(1u, sizeof(*Bitmap));
    if (Bitmap == NULL) {
        xstbtt_error(XERR_MEMORY, "could not allocate bitmap object");
        return NULL;
    }
    Bitmap->Pixels = stbtt_GetCodepointBitmap(&Font->Info, 0.0f, Scale,
        Codepoint, &Bitmap->Width, &Bitmap->Height,
        &Bitmap->Xoff, &Bitmap->Yoff);
    /* stbtt 可能跳过分配失败的边缘并仍返回像素；禁止交付残缺位图。 */
    if (xstbtt_error_is_memory()) {
        stbtt_FreeBitmap(Bitmap->Pixels, NULL);
        xrtFree(Bitmap);
        return NULL;
    }
    if (Bitmap->Width < 0 || Bitmap->Height < 0 ||
        (Bitmap->Height != 0 && (size_t)Bitmap->Width >
            XSTBTT_BITMAP_LIMIT / (size_t)Bitmap->Height)) {
        stbtt_FreeBitmap(Bitmap->Pixels, NULL);
        xrtFree(Bitmap);
        xstbtt_error(XERR_RANGE, "rasterized glyph exceeds 64 MiB");
        return NULL;
    }
    if (Width * Rows != 0u && Bitmap->Pixels == NULL) {
        xrtFree(Bitmap);
        xstbtt_error(XERR_MEMORY, "could not rasterize glyph");
        return NULL;
    }
    return Bitmap;
}

void* xstbtt_bitmap_adopt(void* Bitmap)
{
    return Bitmap;
}

void xstbtt_bitmap_release(void* Handle)
{
    xstbtt_bitmap* Bitmap = (xstbtt_bitmap*)Handle;
    if (Bitmap == NULL) return;
    stbtt_FreeBitmap(Bitmap->Pixels, NULL);
    xrtFree(Bitmap);
}

int xstbtt_bitmap_width(void* Handle)
{
    return Handle != NULL ? ((xstbtt_bitmap*)Handle)->Width : 0;
}

int xstbtt_bitmap_height(void* Handle)
{
    return Handle != NULL ? ((xstbtt_bitmap*)Handle)->Height : 0;
}

int xstbtt_bitmap_xoff(void* Handle)
{
    return Handle != NULL ? ((xstbtt_bitmap*)Handle)->Xoff : 0;
}

int xstbtt_bitmap_yoff(void* Handle)
{
    return Handle != NULL ? ((xstbtt_bitmap*)Handle)->Yoff : 0;
}

void* xstbtt_bitmap_data(void* Handle)
{
    return Handle != NULL ? ((xstbtt_bitmap*)Handle)->Pixels : NULL;
}

size_t xstbtt_bitmap_size(void* Handle)
{
    xstbtt_bitmap* Bitmap = (xstbtt_bitmap*)Handle;
    return Bitmap != NULL ? (size_t)Bitmap->Width * (size_t)Bitmap->Height : 0u;
}
