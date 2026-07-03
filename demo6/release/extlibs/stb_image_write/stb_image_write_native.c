#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "third_party/stb_image_write.h"

#if defined(_WIN32) || defined(_WIN64)
#define XSTBIW_EXPORT __declspec(dllexport)
#else
#define XSTBIW_EXPORT
#endif

#if defined(__GNUC__)
#define XSTBIW_THREAD __thread
#elif defined(_MSC_VER)
#define XSTBIW_THREAD __declspec(thread)
#else
#define XSTBIW_THREAD
#endif

static XSTBIW_THREAD char g_xstbiw_error[512];

static void xstbiw_set_error(const char* text)
{
    size_t i = 0u;

    if ( text == NULL ) {
        text = "";
    }

    while ( i + 1u < sizeof(g_xstbiw_error) && text[i] != '\0' ) {
        g_xstbiw_error[i] = text[i];
        ++i;
    }
    g_xstbiw_error[i] = '\0';
}

static int xstbiw_valid(const char* path, const void* data, int width, int height, int channels)
{
	if ( path == NULL || path[0] == '\0' ) {
		xstbiw_set_error("empty output path");
		return 0;
	}
	if ( data == NULL || width <= 0 || height <= 0 || channels <= 0 || channels > 4 ) {
		xstbiw_set_error("invalid image data");
		return 0;
	}
	return 1;
}

XSTBIW_EXPORT int xstbiw_available(void)
{
	return 1;
}

XSTBIW_EXPORT const char* xstbiw_version(void)
{
	return "stb_image_write 1.16";
}

XSTBIW_EXPORT const char* xstbiw_last_error(void)
{
	return g_xstbiw_error;
}

XSTBIW_EXPORT int xstbiw_write_png(const char* path, const void* data, int width, int height, int channels, int stride)
{
	int ok;
	if ( !xstbiw_valid(path, data, width, height, channels) ) return 0;
	if ( stride <= 0 ) stride = width * channels;
	ok = stbi_write_png(path, width, height, channels, data, stride);
	xstbiw_set_error(ok ? "" : "png write failed");
	return ok ? 1 : 0;
}

XSTBIW_EXPORT int xstbiw_write_jpg(const char* path, const void* data, int width, int height, int channels, int quality)
{
	int ok;
	if ( !xstbiw_valid(path, data, width, height, channels) ) return 0;
	if ( quality <= 0 ) quality = 90;
	if ( quality > 100 ) quality = 100;
	ok = stbi_write_jpg(path, width, height, channels, data, quality);
	xstbiw_set_error(ok ? "" : "jpg write failed");
	return ok ? 1 : 0;
}

XSTBIW_EXPORT int xstbiw_write_bmp(const char* path, const void* data, int width, int height, int channels)
{
	int ok;
	if ( !xstbiw_valid(path, data, width, height, channels) ) return 0;
	ok = stbi_write_bmp(path, width, height, channels, data);
	xstbiw_set_error(ok ? "" : "bmp write failed");
	return ok ? 1 : 0;
}

XSTBIW_EXPORT int xstbiw_write_tga(const char* path, const void* data, int width, int height, int channels)
{
	int ok;
	if ( !xstbiw_valid(path, data, width, height, channels) ) return 0;
	ok = stbi_write_tga(path, width, height, channels, data);
	xstbiw_set_error(ok ? "" : "tga write failed");
	return ok ? 1 : 0;
}

XSTBIW_EXPORT int xstbiw_write_hdr(const char* path, const void* data, int width, int height, int channels)
{
	int ok;
	if ( !xstbiw_valid(path, data, width, height, channels) ) return 0;
	ok = stbi_write_hdr(path, width, height, channels, (const float*)data);
	xstbiw_set_error(ok ? "" : "hdr write failed");
	return ok ? 1 : 0;
}
