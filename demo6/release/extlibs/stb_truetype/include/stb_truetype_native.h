#ifndef XSTBTT_NATIVE_H
#define XSTBTT_NATIVE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int xstbtt_available(void);
const char* xstbtt_version(void);
/* 返回由调用方 xrtFree 的 UTF-8 路径；空串表示未找到示例字体。 */
void* xstbtt_default_font_path(void);

void* xstbtt_load(const char* Path, int Index);
void* xstbtt_load_memory(void* Data, size_t Size, int Index);
void* xstbtt_font_adopt(void* Font);
void xstbtt_font_release(void* Font);
double xstbtt_font_scale(void* Font, double Height);
int xstbtt_font_ascent(void* Font);
int xstbtt_font_descent(void* Font);
int xstbtt_font_line_gap(void* Font);
int xstbtt_font_glyph(void* Font, int Codepoint);
int xstbtt_font_advance(void* Font, int Codepoint);
int xstbtt_font_bearing(void* Font, int Codepoint);
int xstbtt_font_kern(void* Font, int Left, int Right);
void* xstbtt_font_bitmap(void* Font, int Codepoint, double Height);

void* xstbtt_bitmap_adopt(void* Bitmap);
void xstbtt_bitmap_release(void* Bitmap);
int xstbtt_bitmap_width(void* Bitmap);
int xstbtt_bitmap_height(void* Bitmap);
int xstbtt_bitmap_xoff(void* Bitmap);
int xstbtt_bitmap_yoff(void* Bitmap);
void* xstbtt_bitmap_data(void* Bitmap);
size_t xstbtt_bitmap_size(void* Bitmap);

#ifdef __cplusplus
}
#endif
#endif
