#include <stdlib.h>
#include <string.h>

#define STBIR_NO_SIMD
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "third_party/stb_image_resize2.h"

#if defined(_WIN32) || defined(_WIN64)
#define XSTBIR_EXPORT __declspec(dllexport)
#else
#define XSTBIR_EXPORT
#endif

#if defined(__GNUC__)
#define XSTBIR_THREAD __thread
#elif defined(_MSC_VER)
#define XSTBIR_THREAD __declspec(thread)
#else
#define XSTBIR_THREAD
#endif

typedef struct XStbirImage {
	int width;
	int height;
	int channels;
	unsigned char* data;
} XStbirImage;

static XSTBIR_THREAD char g_xstbir_error[512];

static void xstbir_set_error(const char* text)
{
    size_t i = 0u;

    if ( text == NULL ) {
        text = "";
    }

    while ( i + 1u < sizeof(g_xstbir_error) && text[i] != '\0' ) {
        g_xstbir_error[i] = text[i];
        ++i;
    }
    g_xstbir_error[i] = '\0';
}

static stbir_pixel_layout xstbir_layout(int channels)
{
	if ( channels == 1 ) return (stbir_pixel_layout)1;
	if ( channels == 2 ) return (stbir_pixel_layout)2;
	if ( channels == 3 ) return STBIR_RGB;
	if ( channels == 4 ) return STBIR_RGBA;
	return (stbir_pixel_layout)0;
}

static XStbirImage* xstbir_alloc(int width, int height, int channels)
{
	XStbirImage* image;
	size_t count;
	if ( width <= 0 || height <= 0 || channels <= 0 || channels > 4 ) {
		xstbir_set_error("invalid image shape");
		return NULL;
	}
	image = (XStbirImage*)calloc(1u, sizeof(XStbirImage));
	if ( image == NULL ) {
		xstbir_set_error("out of memory");
		return NULL;
	}
	count = (size_t)width * (size_t)height * (size_t)channels;
	image->data = (unsigned char*)malloc(count);
	if ( image->data == NULL ) {
		free(image);
		xstbir_set_error("out of memory");
		return NULL;
	}
	image->width = width;
	image->height = height;
	image->channels = channels;
	return image;
}

static void* xstbir_resize_uint8_impl(const unsigned char* data, int width, int height, int channels, int out_width, int out_height, int input_stride, int linear)
{
	XStbirImage* out;
	stbir_pixel_layout layout;
	if ( data == NULL || width <= 0 || height <= 0 || out_width <= 0 || out_height <= 0 || channels <= 0 || channels > 4 ) {
		xstbir_set_error("invalid resize input");
		return NULL;
	}
	layout = xstbir_layout(channels);
	out = xstbir_alloc(out_width, out_height, channels);
	if ( out == NULL ) return NULL;
	if ( linear ) {
		if ( stbir_resize_uint8_linear(data, width, height, input_stride, out->data, out_width, out_height, 0, layout) == NULL ) {
			free(out->data);
			free(out);
			xstbir_set_error("resize failed");
			return NULL;
		}
	} else {
		if ( stbir_resize_uint8_srgb(data, width, height, input_stride, out->data, out_width, out_height, 0, layout) == NULL ) {
			free(out->data);
			free(out);
			xstbir_set_error("resize failed");
			return NULL;
		}
	}
	xstbir_set_error("");
	return out;
}

XSTBIR_EXPORT const char* xstbir_version(void)
{
	return "stb_image_resize2 2.18";
}

XSTBIR_EXPORT const char* xstbir_last_error(void)
{
	return g_xstbir_error;
}

XSTBIR_EXPORT void* xstbir_resize_uint8_srgb_native(void* data, int width, int height, int channels, int out_width, int out_height, int input_stride)
{
	return xstbir_resize_uint8_impl((const unsigned char*)data, width, height, channels, out_width, out_height, input_stride, 0);
}

XSTBIR_EXPORT void* xstbir_resize_uint8_linear_native(void* data, int width, int height, int channels, int out_width, int out_height, int input_stride)
{
	return xstbir_resize_uint8_impl((const unsigned char*)data, width, height, channels, out_width, out_height, input_stride, 1);
}

XSTBIR_EXPORT int xstbir_image_valid(void* handle)
{
	XStbirImage* image = (XStbirImage*)handle;
	return image != NULL && image->data != NULL;
}

XSTBIR_EXPORT int xstbir_image_width(void* handle)
{
	XStbirImage* image = (XStbirImage*)handle;
	return image != NULL ? image->width : 0;
}

XSTBIR_EXPORT int xstbir_image_height(void* handle)
{
	XStbirImage* image = (XStbirImage*)handle;
	return image != NULL ? image->height : 0;
}

XSTBIR_EXPORT int xstbir_image_channels(void* handle)
{
	XStbirImage* image = (XStbirImage*)handle;
	return image != NULL ? image->channels : 0;
}

XSTBIR_EXPORT void* xstbir_image_data(void* handle)
{
	XStbirImage* image = (XStbirImage*)handle;
	return image != NULL ? image->data : NULL;
}

XSTBIR_EXPORT int xstbir_image_release(void* handle)
{
	XStbirImage* image = (XStbirImage*)handle;
	if ( image == NULL ) return 0;
	free(image->data);
	image->data = NULL;
	free(image);
	return 1;
}
