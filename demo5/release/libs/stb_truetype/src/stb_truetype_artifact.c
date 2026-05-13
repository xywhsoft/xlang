#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../include/stb_truetype.h"

#if defined(_WIN32) || defined(_WIN64)
#define XSTBTT_EXPORT __declspec(dllexport)
#else
#define XSTBTT_EXPORT
#endif

#if defined(__GNUC__)
#define XSTBTT_THREAD __thread
#elif defined(_MSC_VER)
#define XSTBTT_THREAD __declspec(thread)
#else
#define XSTBTT_THREAD
#endif

typedef struct XStbttFont {
	unsigned char* bytes;
	long size;
	int font_index;
	int offset;
	stbtt_fontinfo info;
} XStbttFont;

typedef struct XStbttBitmap {
	int width;
	int height;
	int xoff;
	int yoff;
	unsigned char* data;
} XStbttBitmap;

static XSTBTT_THREAD char g_xstbtt_error[512];
static XSTBTT_THREAD char g_xstbtt_json[256];

static void xstbtt_set_error(const char* text)
{
	if ( text == NULL ) text = "";
	strncpy(g_xstbtt_error, text, sizeof(g_xstbtt_error) - 1u);
	g_xstbtt_error[sizeof(g_xstbtt_error) - 1u] = '\0';
}

static unsigned char* xstbtt_read_file(const char* path, long* out_size)
{
	FILE* fp;
	long size;
	unsigned char* data;
	size_t read_count;
	if ( out_size != NULL ) *out_size = 0;
	if ( path == NULL || path[0] == '\0' ) {
		xstbtt_set_error("empty font path");
		return NULL;
	}
	fp = fopen(path, "rb");
	if ( fp == NULL ) {
		xstbtt_set_error("font open failed");
		return NULL;
	}
	if ( fseek(fp, 0, SEEK_END) != 0 ) {
		fclose(fp);
		xstbtt_set_error("font seek failed");
		return NULL;
	}
	size = ftell(fp);
	if ( size <= 0 ) {
		fclose(fp);
		xstbtt_set_error("empty font file");
		return NULL;
	}
	if ( fseek(fp, 0, SEEK_SET) != 0 ) {
		fclose(fp);
		xstbtt_set_error("font seek failed");
		return NULL;
	}
	data = (unsigned char*)malloc((size_t)size);
	if ( data == NULL ) {
		fclose(fp);
		xstbtt_set_error("out of memory");
		return NULL;
	}
	read_count = fread(data, 1u, (size_t)size, fp);
	fclose(fp);
	if ( read_count != (size_t)size ) {
		free(data);
		xstbtt_set_error("font read failed");
		return NULL;
	}
	if ( out_size != NULL ) *out_size = size;
	return data;
}

static XStbttFont* xstbtt_cast_font(void* handle)
{
	XStbttFont* font;
	font = (XStbttFont*)handle;
	if ( font == NULL || font->bytes == NULL ) {
		xstbtt_set_error("invalid font handle");
		return NULL;
	}
	return font;
}

static XStbttBitmap* xstbtt_cast_bitmap(void* handle)
{
	XStbttBitmap* bitmap;
	bitmap = (XStbttBitmap*)handle;
	if ( bitmap == NULL ) {
		xstbtt_set_error("invalid bitmap handle");
		return NULL;
	}
	return bitmap;
}

XSTBTT_EXPORT const char* xstbtt_version(void)
{
	return "stb_truetype 1.26";
}

XSTBTT_EXPORT const char* xstbtt_last_error(void)
{
	return g_xstbtt_error;
}

XSTBTT_EXPORT void* xstbtt_load(const char* path, int font_index)
{
	XStbttFont* font;
	long size;
	int offset;
	unsigned char* data;
	if ( font_index < 0 ) font_index = 0;
	data = xstbtt_read_file(path, &size);
	if ( data == NULL ) return NULL;
	offset = stbtt_GetFontOffsetForIndex(data, font_index);
	if ( offset < 0 ) {
		free(data);
		xstbtt_set_error("font index not found");
		return NULL;
	}
	font = (XStbttFont*)calloc(1u, sizeof(XStbttFont));
	if ( font == NULL ) {
		free(data);
		xstbtt_set_error("out of memory");
		return NULL;
	}
	if ( !stbtt_InitFont(&font->info, data, offset) ) {
		free(font);
		free(data);
		xstbtt_set_error("font init failed");
		return NULL;
	}
	font->bytes = data;
	font->size = size;
	font->font_index = font_index;
	font->offset = offset;
	xstbtt_set_error("");
	return font;
}

XSTBTT_EXPORT int xstbtt_font_valid(void* handle)
{
	XStbttFont* font;
	font = (XStbttFont*)handle;
	return font != NULL && font->bytes != NULL;
}

XSTBTT_EXPORT int xstbtt_font_release(void* handle)
{
	XStbttFont* font;
	font = (XStbttFont*)handle;
	if ( font == NULL ) return 0;
	free(font->bytes);
	font->bytes = NULL;
	free(font);
	return 1;
}

XSTBTT_EXPORT double xstbtt_scale_for_pixel_height(void* handle, double pixel_height)
{
	XStbttFont* font;
	font = xstbtt_cast_font(handle);
	if ( font == NULL || pixel_height <= 0.0 ) return 0.0;
	return (double)stbtt_ScaleForPixelHeight(&font->info, (float)pixel_height);
}

XSTBTT_EXPORT int xstbtt_ascent(void* handle)
{
	XStbttFont* font;
	int ascent;
	int descent;
	int line_gap;
	font = xstbtt_cast_font(handle);
	if ( font == NULL ) return 0;
	stbtt_GetFontVMetrics(&font->info, &ascent, &descent, &line_gap);
	return ascent;
}

XSTBTT_EXPORT int xstbtt_descent(void* handle)
{
	XStbttFont* font;
	int ascent;
	int descent;
	int line_gap;
	font = xstbtt_cast_font(handle);
	if ( font == NULL ) return 0;
	stbtt_GetFontVMetrics(&font->info, &ascent, &descent, &line_gap);
	return descent;
}

XSTBTT_EXPORT int xstbtt_line_gap(void* handle)
{
	XStbttFont* font;
	int ascent;
	int descent;
	int line_gap;
	font = xstbtt_cast_font(handle);
	if ( font == NULL ) return 0;
	stbtt_GetFontVMetrics(&font->info, &ascent, &descent, &line_gap);
	return line_gap;
}

XSTBTT_EXPORT int xstbtt_find_glyph_index(void* handle, int codepoint)
{
	XStbttFont* font;
	font = xstbtt_cast_font(handle);
	if ( font == NULL ) return 0;
	return stbtt_FindGlyphIndex(&font->info, codepoint);
}

XSTBTT_EXPORT int xstbtt_h_advance(void* handle, int codepoint)
{
	XStbttFont* font;
	int advance;
	int left_bearing;
	font = xstbtt_cast_font(handle);
	if ( font == NULL ) return 0;
	stbtt_GetCodepointHMetrics(&font->info, codepoint, &advance, &left_bearing);
	return advance;
}

XSTBTT_EXPORT int xstbtt_left_bearing(void* handle, int codepoint)
{
	XStbttFont* font;
	int advance;
	int left_bearing;
	font = xstbtt_cast_font(handle);
	if ( font == NULL ) return 0;
	stbtt_GetCodepointHMetrics(&font->info, codepoint, &advance, &left_bearing);
	return left_bearing;
}

XSTBTT_EXPORT int xstbtt_kern_advance(void* handle, int codepoint1, int codepoint2)
{
	XStbttFont* font;
	font = xstbtt_cast_font(handle);
	if ( font == NULL ) return 0;
	return stbtt_GetCodepointKernAdvance(&font->info, codepoint1, codepoint2);
}

XSTBTT_EXPORT const char* xstbtt_vmetrics_json(void* handle)
{
	XStbttFont* font;
	int ascent;
	int descent;
	int line_gap;
	font = xstbtt_cast_font(handle);
	if ( font == NULL ) return "{\"ok\":false,\"ascent\":0,\"descent\":0,\"line_gap\":0}";
	stbtt_GetFontVMetrics(&font->info, &ascent, &descent, &line_gap);
	snprintf(g_xstbtt_json, sizeof(g_xstbtt_json), "{\"ok\":true,\"ascent\":%d,\"descent\":%d,\"line_gap\":%d}", ascent, descent, line_gap);
	xstbtt_set_error("");
	return g_xstbtt_json;
}

XSTBTT_EXPORT const char* xstbtt_hmetrics_json(void* handle, int codepoint)
{
	XStbttFont* font;
	int advance;
	int left_bearing;
	int glyph_index;
	font = xstbtt_cast_font(handle);
	if ( font == NULL ) return "{\"ok\":false,\"codepoint\":0,\"glyph_index\":0,\"advance\":0,\"left_bearing\":0}";
	glyph_index = stbtt_FindGlyphIndex(&font->info, codepoint);
	stbtt_GetCodepointHMetrics(&font->info, codepoint, &advance, &left_bearing);
	snprintf(g_xstbtt_json, sizeof(g_xstbtt_json), "{\"ok\":true,\"codepoint\":%d,\"glyph_index\":%d,\"advance\":%d,\"left_bearing\":%d}", codepoint, glyph_index, advance, left_bearing);
	xstbtt_set_error("");
	return g_xstbtt_json;
}

XSTBTT_EXPORT void* xstbtt_codepoint_bitmap(void* handle, int codepoint, double pixel_height)
{
	XStbttFont* font;
	XStbttBitmap* bitmap;
	float scale;
	int width;
	int height;
	int xoff;
	int yoff;
	unsigned char* data;
	font = xstbtt_cast_font(handle);
	if ( font == NULL || pixel_height <= 0.0 ) {
		xstbtt_set_error("invalid bitmap input");
		return NULL;
	}
	scale = stbtt_ScaleForPixelHeight(&font->info, (float)pixel_height);
	data = stbtt_GetCodepointBitmap(&font->info, 0.0f, scale, codepoint, &width, &height, &xoff, &yoff);
	bitmap = (XStbttBitmap*)calloc(1u, sizeof(XStbttBitmap));
	if ( bitmap == NULL ) {
		if ( data != NULL ) stbtt_FreeBitmap(data, NULL);
		xstbtt_set_error("out of memory");
		return NULL;
	}
	bitmap->width = width;
	bitmap->height = height;
	bitmap->xoff = xoff;
	bitmap->yoff = yoff;
	bitmap->data = data;
	xstbtt_set_error("");
	return bitmap;
}

XSTBTT_EXPORT const char* xstbtt_codepoint_bitmap_box_json(void* handle, int codepoint, double pixel_height)
{
	XStbttFont* font;
	float scale;
	int x0;
	int y0;
	int x1;
	int y1;
	font = xstbtt_cast_font(handle);
	if ( font == NULL || pixel_height <= 0.0 ) return "{\"ok\":false,\"x0\":0,\"y0\":0,\"x1\":0,\"y1\":0}";
	scale = stbtt_ScaleForPixelHeight(&font->info, (float)pixel_height);
	stbtt_GetCodepointBitmapBox(&font->info, codepoint, scale, scale, &x0, &y0, &x1, &y1);
	snprintf(g_xstbtt_json, sizeof(g_xstbtt_json), "{\"ok\":true,\"x0\":%d,\"y0\":%d,\"x1\":%d,\"y1\":%d}", x0, y0, x1, y1);
	xstbtt_set_error("");
	return g_xstbtt_json;
}

XSTBTT_EXPORT int xstbtt_bitmap_valid(void* handle)
{
	return handle != NULL;
}

XSTBTT_EXPORT int xstbtt_bitmap_width(void* handle)
{
	XStbttBitmap* bitmap;
	bitmap = xstbtt_cast_bitmap(handle);
	return bitmap != NULL ? bitmap->width : 0;
}

XSTBTT_EXPORT int xstbtt_bitmap_height(void* handle)
{
	XStbttBitmap* bitmap;
	bitmap = xstbtt_cast_bitmap(handle);
	return bitmap != NULL ? bitmap->height : 0;
}

XSTBTT_EXPORT int xstbtt_bitmap_xoff(void* handle)
{
	XStbttBitmap* bitmap;
	bitmap = xstbtt_cast_bitmap(handle);
	return bitmap != NULL ? bitmap->xoff : 0;
}

XSTBTT_EXPORT int xstbtt_bitmap_yoff(void* handle)
{
	XStbttBitmap* bitmap;
	bitmap = xstbtt_cast_bitmap(handle);
	return bitmap != NULL ? bitmap->yoff : 0;
}

XSTBTT_EXPORT void* xstbtt_bitmap_data(void* handle)
{
	XStbttBitmap* bitmap;
	bitmap = xstbtt_cast_bitmap(handle);
	return bitmap != NULL ? bitmap->data : NULL;
}

XSTBTT_EXPORT int xstbtt_bitmap_release(void* handle)
{
	XStbttBitmap* bitmap;
	bitmap = (XStbttBitmap*)handle;
	if ( bitmap == NULL ) return 0;
	if ( bitmap->data != NULL ) stbtt_FreeBitmap(bitmap->data, NULL);
	bitmap->data = NULL;
	free(bitmap);
	return 1;
}
