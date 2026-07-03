#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STBI_NO_SIMD
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"

#if defined(_WIN32) || defined(_WIN64)
#define XSTBI_EXPORT __declspec(dllexport)
#else
#define XSTBI_EXPORT
#endif

#if defined(__GNUC__)
#define XSTBI_THREAD __thread
#elif defined(_MSC_VER)
#define XSTBI_THREAD __declspec(thread)
#else
#define XSTBI_THREAD
#endif

typedef struct XStbiImage {
	int width;
	int height;
	int channels;
	int is_float;
	void* data;
} XStbiImage;

static XSTBI_THREAD char g_xstbi_error[512];
#define XSTBI_JSON_SLOTS 8u

static XSTBI_THREAD char g_xstbi_json[XSTBI_JSON_SLOTS][256];
static XSTBI_THREAD size_t g_xstbi_json_index;

static void xstbi_set_error(const char* text)
{
    size_t i = 0u;

    if ( text == NULL ) {
        text = "";
    }

    while ( i + 1u < sizeof(g_xstbi_error) && text[i] != '\0' ) {
        g_xstbi_error[i] = text[i];
        ++i;
    }
    g_xstbi_error[i] = '\0';
}

static XStbiImage* xstbi_wrap(void* data, int width, int height, int channels, int is_float)
{
	XStbiImage* image;
	if ( data == NULL ) {
		xstbi_set_error(stbi_failure_reason());
		return NULL;
	}
	image = (XStbiImage*)calloc(1u, sizeof(XStbiImage));
	if ( image == NULL ) {
		stbi_image_free(data);
		xstbi_set_error("out of memory");
		return NULL;
	}
	image->width = width;
	image->height = height;
	image->channels = channels;
	image->is_float = is_float;
	image->data = data;
	xstbi_set_error("");
	return image;
}

XSTBI_EXPORT int xstbi_available(void)
{
	return 1;
}

XSTBI_EXPORT const char* xstbi_version(void)
{
	return "stb_image 2.30";
}

XSTBI_EXPORT const char* xstbi_last_error(void)
{
	return g_xstbi_error;
}

XSTBI_EXPORT void* xstbi_load(const char* path, int desired_channels)
{
	int width = 0;
	int height = 0;
	int channels = 0;
	unsigned char* data;
	if ( path == NULL || path[0] == '\0' ) {
		xstbi_set_error("empty image path");
		return NULL;
	}
	if ( desired_channels < 0 || desired_channels > 4 ) desired_channels = 0;
	data = stbi_load(path, &width, &height, &channels, desired_channels);
	if ( desired_channels > 0 ) channels = desired_channels;
	return xstbi_wrap(data, width, height, channels, 0);
}

XSTBI_EXPORT void* xstbi_loadf(const char* path, int desired_channels)
{
	int width = 0;
	int height = 0;
	int channels = 0;
	float* data;
	if ( path == NULL || path[0] == '\0' ) {
		xstbi_set_error("empty image path");
		return NULL;
	}
	if ( desired_channels < 0 || desired_channels > 4 ) desired_channels = 0;
	data = stbi_loadf(path, &width, &height, &channels, desired_channels);
	if ( desired_channels > 0 ) channels = desired_channels;
	return xstbi_wrap(data, width, height, channels, 1);
}

XSTBI_EXPORT int xstbi_is_hdr(const char* path)
{
	if ( path == NULL || path[0] == '\0' ) return 0;
	return stbi_is_hdr(path);
}

XSTBI_EXPORT const char* xstbi_info_json(const char* path)
{
	int width = 0;
	int height = 0;
	int channels = 0;
	int ok;
	if ( path == NULL || path[0] == '\0' ) {
		xstbi_set_error("empty image path");
		return "{\"ok\":false,\"width\":0,\"height\":0,\"channels\":0}";
	}
	ok = stbi_info(path, &width, &height, &channels);
	if ( !ok ) {
		xstbi_set_error(stbi_failure_reason());
		return "{\"ok\":false,\"width\":0,\"height\":0,\"channels\":0}";
	}
	snprintf(g_xstbi_json, sizeof(g_xstbi_json), "{\"ok\":true,\"width\":%d,\"height\":%d,\"channels\":%d}", width, height, channels);
	xstbi_set_error("");
	return g_xstbi_json;
}

XSTBI_EXPORT int xstbi_image_valid(void* handle)
{
	XStbiImage* image = (XStbiImage*)handle;
	return image != NULL && image->data != NULL;
}

XSTBI_EXPORT int xstbi_image_width(void* handle)
{
	XStbiImage* image = (XStbiImage*)handle;
	return image != NULL ? image->width : 0;
}

XSTBI_EXPORT int xstbi_image_height(void* handle)
{
	XStbiImage* image = (XStbiImage*)handle;
	return image != NULL ? image->height : 0;
}

XSTBI_EXPORT int xstbi_image_channels(void* handle)
{
	XStbiImage* image = (XStbiImage*)handle;
	return image != NULL ? image->channels : 0;
}

XSTBI_EXPORT void* xstbi_image_data(void* handle)
{
	XStbiImage* image = (XStbiImage*)handle;
	return image != NULL ? image->data : NULL;
}

XSTBI_EXPORT int xstbi_image_is_float(void* handle)
{
	XStbiImage* image = (XStbiImage*)handle;
	return image != NULL ? image->is_float : 0;
}

XSTBI_EXPORT int xstbi_image_release(void* handle)
{
	XStbiImage* image = (XStbiImage*)handle;
	if ( image == NULL ) return 0;
	if ( image->data != NULL ) stbi_image_free(image->data);
	image->data = NULL;
	free(image);
	return 1;
}
