#include "../include/opencv_xlang_c_api.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#if defined(_WIN32) || defined(_WIN64)
#define XLCV_EXPORT __declspec(dllexport)
#else
#define XLCV_EXPORT
#endif

#if defined(__GNUC__)
#define XLCV_THREAD __thread
#elif defined(_MSC_VER)
#define XLCV_THREAD __declspec(thread)
#else
#define XLCV_THREAD
#endif

typedef struct XlCvImageImpl {
	int width;
	int height;
	int channels;
	float* data;
} XlCvImageImpl;

static XLCV_THREAD char g_error[512];
static XLCV_THREAD char* g_json;
static XLCV_THREAD size_t g_json_cap;

static void xlcv_set_error(const char* text)
{
	if ( text == NULL ) text = "";
	strncpy(g_error, text, sizeof(g_error) - 1u);
	g_error[sizeof(g_error) - 1u] = '\0';
}

static XlCvImageImpl* xlcv_alloc(int width, int height, int channels)
{
	XlCvImageImpl* image;
	size_t count;
	if ( width <= 0 || height <= 0 || channels <= 0 ) {
		xlcv_set_error("invalid image shape");
		return NULL;
	}
	image = (XlCvImageImpl*)calloc(1u, sizeof(XlCvImageImpl));
	if ( image == NULL ) {
		xlcv_set_error("out of memory");
		return NULL;
	}
	count = (size_t)width * (size_t)height * (size_t)channels;
	image->data = (float*)calloc(count, sizeof(float));
	if ( image->data == NULL ) {
		free(image);
		xlcv_set_error("out of memory");
		return NULL;
	}
	image->width = width;
	image->height = height;
	image->channels = channels;
	xlcv_set_error("");
	return image;
}

static double xlcv_channel_value(int channel, double value0, double value1, double value2)
{
	if ( channel == 1 ) return value1;
	if ( channel >= 2 ) return value2;
	return value0;
}

static float xlcv_clampf(float value, float low, float high)
{
	if ( value < low ) return low;
	if ( value > high ) return high;
	return value;
}

static int xlcv_streq_ext(const char* path, const char* ext)
{
	size_t path_len;
	size_t ext_len;
	if ( path == NULL || ext == NULL ) return 0;
	path_len = strlen(path);
	ext_len = strlen(ext);
	if ( path_len < ext_len ) return 0;
#if defined(_WIN32) || defined(_WIN64)
	return _stricmp(path + path_len - ext_len, ext) == 0;
#else
	return strcasecmp(path + path_len - ext_len, ext) == 0;
#endif
}

XLCV_EXPORT int opencv_xl_available(void)
{
	return 1;
}

XLCV_EXPORT const char* opencv_xl_version(void)
{
	return "stb-image backend";
}

XLCV_EXPORT const char* opencv_xl_last_error(void)
{
	return g_error;
}

XLCV_EXPORT XlCvImage opencv_xl_imread(const char* path, int flags)
{
	int width = 0;
	int height = 0;
	int channels = 0;
	int desired = 3;
	unsigned char* pixels;
	XlCvImageImpl* image;
	size_t count;
	size_t i;
	if ( path == NULL || path[0] == '\0' ) {
		xlcv_set_error("empty image path");
		return NULL;
	}
	if ( flags == 0 ) desired = 1;
	else if ( flags < 0 ) desired = 0;
	pixels = stbi_load(path, &width, &height, &channels, desired);
	if ( pixels == NULL ) {
		xlcv_set_error(stbi_failure_reason());
		return NULL;
	}
	if ( desired > 0 ) channels = desired;
	image = xlcv_alloc(width, height, channels);
	if ( image == NULL ) {
		stbi_image_free(pixels);
		return NULL;
	}
	count = (size_t)width * (size_t)height * (size_t)channels;
	for ( i = 0; i < count; ++i ) image->data[i] = (float)pixels[i];
	stbi_image_free(pixels);
	xlcv_set_error("");
	return image;
}

XLCV_EXPORT int opencv_xl_imwrite(const char* path, XlCvImage handle)
{
	XlCvImageImpl* image = (XlCvImageImpl*)handle;
	unsigned char* pixels;
	size_t count;
	size_t i;
	int ok;
	if ( path == NULL || path[0] == '\0' || image == NULL || image->data == NULL ) {
		xlcv_set_error("invalid image or output path");
		return 0;
	}
	count = (size_t)image->width * (size_t)image->height * (size_t)image->channels;
	pixels = (unsigned char*)malloc(count);
	if ( pixels == NULL ) {
		xlcv_set_error("out of memory");
		return 0;
	}
	for ( i = 0; i < count; ++i ) pixels[i] = (unsigned char)(xlcv_clampf(image->data[i], 0.0f, 255.0f) + 0.5f);
	if ( xlcv_streq_ext(path, ".jpg") || xlcv_streq_ext(path, ".jpeg") ) {
		ok = stbi_write_jpg(path, image->width, image->height, image->channels, pixels, 90);
	} else if ( xlcv_streq_ext(path, ".bmp") ) {
		ok = stbi_write_bmp(path, image->width, image->height, image->channels, pixels);
	} else {
		ok = stbi_write_png(path, image->width, image->height, image->channels, pixels, image->width * image->channels);
	}
	free(pixels);
	xlcv_set_error(ok ? "" : "image write failed");
	return ok ? 1 : 0;
}

XLCV_EXPORT XlCvImage opencv_xl_resize(XlCvImage handle, int width, int height)
{
	XlCvImageImpl* image = (XlCvImageImpl*)handle;
	XlCvImageImpl* out;
	int x;
	int y;
	int c;
	if ( image == NULL || image->data == NULL || width <= 0 || height <= 0 ) {
		xlcv_set_error("invalid image or resize dimensions");
		return NULL;
	}
	out = xlcv_alloc(width, height, image->channels);
	if ( out == NULL ) return NULL;
	for ( y = 0; y < height; ++y ) {
		int sy = (int)((double)y * (double)image->height / (double)height);
		if ( sy >= image->height ) sy = image->height - 1;
		for ( x = 0; x < width; ++x ) {
			int sx = (int)((double)x * (double)image->width / (double)width);
			if ( sx >= image->width ) sx = image->width - 1;
			for ( c = 0; c < image->channels; ++c ) {
				out->data[((size_t)y * width + x) * image->channels + c] = image->data[((size_t)sy * image->width + sx) * image->channels + c];
			}
		}
	}
	return out;
}

XLCV_EXPORT XlCvImage opencv_xl_crop(XlCvImage handle, int x, int y, int width, int height)
{
	XlCvImageImpl* image = (XlCvImageImpl*)handle;
	XlCvImageImpl* out;
	int xx;
	int yy;
	int c;
	if ( image == NULL || image->data == NULL || width <= 0 || height <= 0 ) {
		xlcv_set_error("invalid image or crop dimensions");
		return NULL;
	}
	if ( x < 0 ) {
		width += x;
		x = 0;
	}
	if ( y < 0 ) {
		height += y;
		y = 0;
	}
	if ( x >= image->width || y >= image->height || width <= 0 || height <= 0 ) {
		xlcv_set_error("crop is outside image");
		return NULL;
	}
	if ( x + width > image->width ) width = image->width - x;
	if ( y + height > image->height ) height = image->height - y;
	out = xlcv_alloc(width, height, image->channels);
	if ( out == NULL ) return NULL;
	for ( yy = 0; yy < height; ++yy ) {
		for ( xx = 0; xx < width; ++xx ) {
			for ( c = 0; c < image->channels; ++c ) {
				out->data[((size_t)yy * width + xx) * image->channels + c] = image->data[((size_t)(y + yy) * image->width + (x + xx)) * image->channels + c];
			}
		}
	}
	xlcv_set_error("");
	return out;
}

#if defined(_WIN32) || defined(_WIN64)
static XlCvImage xlcv_capture_hdc_region(HDC source_dc, int x, int y, int width, int height)
{
	HDC mem_dc;
	HBITMAP bitmap;
	HGDIOBJ old_bitmap;
	BITMAPINFO bmi;
	unsigned char* pixels;
	XlCvImageImpl* image;
	int row_stride;
	int xx;
	int yy;
	if ( source_dc == NULL || width <= 0 || height <= 0 ) {
		xlcv_set_error("invalid capture region");
		return NULL;
	}
	mem_dc = CreateCompatibleDC(source_dc);
	if ( mem_dc == NULL ) {
		xlcv_set_error("CreateCompatibleDC failed");
		return NULL;
	}
	bitmap = CreateCompatibleBitmap(source_dc, width, height);
	if ( bitmap == NULL ) {
		DeleteDC(mem_dc);
		xlcv_set_error("CreateCompatibleBitmap failed");
		return NULL;
	}
	old_bitmap = SelectObject(mem_dc, bitmap);
	if ( old_bitmap == NULL ) {
		DeleteObject(bitmap);
		DeleteDC(mem_dc);
		xlcv_set_error("SelectObject failed");
		return NULL;
	}
	if ( BitBlt(mem_dc, 0, 0, width, height, source_dc, x, y, SRCCOPY | CAPTUREBLT) == 0 ) {
		SelectObject(mem_dc, old_bitmap);
		DeleteObject(bitmap);
		DeleteDC(mem_dc);
		xlcv_set_error("BitBlt failed");
		return NULL;
	}
	memset(&bmi, 0, sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = width;
	bmi.bmiHeader.biHeight = -height;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	row_stride = width * 4;
	pixels = (unsigned char*)malloc((size_t)row_stride * (size_t)height);
	if ( pixels == NULL ) {
		SelectObject(mem_dc, old_bitmap);
		DeleteObject(bitmap);
		DeleteDC(mem_dc);
		xlcv_set_error("out of memory");
		return NULL;
	}
	if ( GetDIBits(mem_dc, bitmap, 0, (UINT)height, pixels, &bmi, DIB_RGB_COLORS) == 0 ) {
		free(pixels);
		SelectObject(mem_dc, old_bitmap);
		DeleteObject(bitmap);
		DeleteDC(mem_dc);
		xlcv_set_error("GetDIBits failed");
		return NULL;
	}
	image = xlcv_alloc(width, height, 3);
	if ( image != NULL ) {
		for ( yy = 0; yy < height; ++yy ) {
			for ( xx = 0; xx < width; ++xx ) {
				unsigned char* px = pixels + ((size_t)yy * (size_t)width + (size_t)xx) * 4u;
				float* out = image->data + ((size_t)yy * (size_t)width + (size_t)xx) * 3u;
				out[0] = (float)px[2];
				out[1] = (float)px[1];
				out[2] = (float)px[0];
			}
		}
		xlcv_set_error("");
	}
	free(pixels);
	SelectObject(mem_dc, old_bitmap);
	DeleteObject(bitmap);
	DeleteDC(mem_dc);
	return image;
}
#endif

XLCV_EXPORT XlCvImage opencv_xl_capture_rect(int x, int y, int width, int height)
{
#if defined(_WIN32) || defined(_WIN64)
	HDC screen_dc;
	XlCvImage image;
	if ( width <= 0 || height <= 0 ) {
		xlcv_set_error("invalid capture dimensions");
		return NULL;
	}
	screen_dc = GetDC(NULL);
	if ( screen_dc == NULL ) {
		xlcv_set_error("GetDC failed");
		return NULL;
	}
	image = xlcv_capture_hdc_region(screen_dc, x, y, width, height);
	ReleaseDC(NULL, screen_dc);
	return image;
#else
	(void)x; (void)y; (void)width; (void)height;
	xlcv_set_error("capture_rect is only supported on Windows");
	return NULL;
#endif
}

XLCV_EXPORT XlCvImage opencv_xl_capture_window(void* hwnd)
{
#if defined(_WIN32) || defined(_WIN64)
	HWND window = (HWND)hwnd;
	RECT rect;
	HDC window_dc;
	XlCvImage image;
	if ( window == NULL || IsWindow(window) == 0 ) {
		xlcv_set_error("invalid window handle");
		return NULL;
	}
	if ( GetWindowRect(window, &rect) == 0 || rect.right <= rect.left || rect.bottom <= rect.top ) {
		xlcv_set_error("invalid window rectangle");
		return NULL;
	}
	window_dc = GetWindowDC(window);
	if ( window_dc == NULL ) {
		xlcv_set_error("GetWindowDC failed");
		return NULL;
	}
	image = xlcv_capture_hdc_region(window_dc, 0, 0, (int)(rect.right - rect.left), (int)(rect.bottom - rect.top));
	ReleaseDC(window, window_dc);
	return image;
#else
	(void)hwnd;
	xlcv_set_error("capture_window is only supported on Windows");
	return NULL;
#endif
}

XLCV_EXPORT XlCvImage opencv_xl_rotate180(XlCvImage handle)
{
	XlCvImageImpl* image = (XlCvImageImpl*)handle;
	XlCvImageImpl* out;
	int x;
	int y;
	int c;
	if ( image == NULL || image->data == NULL ) {
		xlcv_set_error("invalid image");
		return NULL;
	}
	out = xlcv_alloc(image->width, image->height, image->channels);
	if ( out == NULL ) return NULL;
	for ( y = 0; y < image->height; ++y ) {
		for ( x = 0; x < image->width; ++x ) {
			int sx = image->width - 1 - x;
			int sy = image->height - 1 - y;
			for ( c = 0; c < image->channels; ++c ) {
				out->data[((size_t)y * image->width + x) * image->channels + c] = image->data[((size_t)sy * image->width + sx) * image->channels + c];
			}
		}
	}
	xlcv_set_error("");
	return out;
}

XLCV_EXPORT XlCvImage opencv_xl_cvt_color(XlCvImage handle, int code)
{
	XlCvImageImpl* image = (XlCvImageImpl*)handle;
	XlCvImageImpl* out;
	size_t pixels;
	size_t i;
	if ( image == NULL || image->data == NULL ) {
		xlcv_set_error("invalid image");
		return NULL;
	}
	pixels = (size_t)image->width * (size_t)image->height;
	if ( code == 6 && image->channels >= 3 ) {
		out = xlcv_alloc(image->width, image->height, 1);
		if ( out == NULL ) return NULL;
		for ( i = 0; i < pixels; ++i ) {
			float b = image->data[i * image->channels + 0];
			float g = image->data[i * image->channels + 1];
			float r = image->data[i * image->channels + 2];
			out->data[i] = 0.114f * b + 0.587f * g + 0.299f * r;
		}
		return out;
	}
	if ( code == 8 && image->channels == 1 ) {
		out = xlcv_alloc(image->width, image->height, 3);
		if ( out == NULL ) return NULL;
		for ( i = 0; i < pixels; ++i ) {
			out->data[i * 3 + 0] = image->data[i];
			out->data[i * 3 + 1] = image->data[i];
			out->data[i * 3 + 2] = image->data[i];
		}
		return out;
	}
	if ( code == 4 && image->channels >= 3 ) {
		out = xlcv_alloc(image->width, image->height, image->channels);
		if ( out == NULL ) return NULL;
		memcpy(out->data, image->data, pixels * (size_t)image->channels * sizeof(float));
		for ( i = 0; i < pixels; ++i ) {
			float t = out->data[i * image->channels + 0];
			out->data[i * image->channels + 0] = out->data[i * image->channels + 2];
			out->data[i * image->channels + 2] = t;
		}
		return out;
	}
	xlcv_set_error("unsupported color conversion code");
	return NULL;
}

XLCV_EXPORT XlCvImage opencv_xl_copy_make_border(XlCvImage handle, int top, int bottom, int left, int right, int border_type, double value0, double value1, double value2, double value3)
{
	XlCvImageImpl* image = (XlCvImageImpl*)handle;
	XlCvImageImpl* out;
	int width;
	int height;
	int x;
	int y;
	int c;
	(void)value3;
	if ( image == NULL || image->data == NULL || top < 0 || bottom < 0 || left < 0 || right < 0 ) {
		xlcv_set_error("invalid image or border sizes");
		return NULL;
	}
	width = image->width + left + right;
	height = image->height + top + bottom;
	out = xlcv_alloc(width, height, image->channels);
	if ( out == NULL ) return NULL;
	for ( y = 0; y < height; ++y ) {
		for ( x = 0; x < width; ++x ) {
			int sx = x - left;
			int sy = y - top;
			for ( c = 0; c < image->channels; ++c ) {
				float v;
				if ( sx >= 0 && sx < image->width && sy >= 0 && sy < image->height ) {
					v = image->data[((size_t)sy * image->width + sx) * image->channels + c];
				} else if ( border_type == 1 ) {
					if ( sx < 0 ) sx = 0;
					if ( sx >= image->width ) sx = image->width - 1;
					if ( sy < 0 ) sy = 0;
					if ( sy >= image->height ) sy = image->height - 1;
					v = image->data[((size_t)sy * image->width + sx) * image->channels + c];
				} else {
					v = (float)xlcv_channel_value(c, value0, value1, value2);
				}
				out->data[((size_t)y * width + x) * image->channels + c] = v;
			}
		}
	}
	return out;
}

XLCV_EXPORT XlCvImage opencv_xl_rectangle(XlCvImage handle, int x1, int y1, int x2, int y2, double value0, double value1, double value2, double value3, int thickness)
{
	XlCvImageImpl* image = (XlCvImageImpl*)handle;
	XlCvImageImpl* out;
	size_t count;
	int x;
	int y;
	int c;
	(void)value3;
	if ( image == NULL || image->data == NULL ) {
		xlcv_set_error("invalid image");
		return NULL;
	}
	if ( thickness <= 0 ) thickness = 1;
	if ( x1 > x2 ) { int t = x1; x1 = x2; x2 = t; }
	if ( y1 > y2 ) { int t = y1; y1 = y2; y2 = t; }
	if ( x1 < 0 ) x1 = 0;
	if ( y1 < 0 ) y1 = 0;
	if ( x2 >= image->width ) x2 = image->width - 1;
	if ( y2 >= image->height ) y2 = image->height - 1;
	out = xlcv_alloc(image->width, image->height, image->channels);
	if ( out == NULL ) return NULL;
	count = (size_t)image->width * (size_t)image->height * (size_t)image->channels;
	memcpy(out->data, image->data, count * sizeof(float));
	for ( y = y1; y <= y2; ++y ) {
		for ( x = x1; x <= x2; ++x ) {
			int on_border = (x - x1 < thickness) || (x2 - x < thickness) || (y - y1 < thickness) || (y2 - y < thickness);
			if ( !on_border ) continue;
			for ( c = 0; c < out->channels; ++c ) {
				out->data[((size_t)y * out->width + x) * out->channels + c] = (float)xlcv_channel_value(c, value0, value1, value2);
			}
		}
	}
	xlcv_set_error("");
	return out;
}

XLCV_EXPORT XlCvImage opencv_xl_normalize(XlCvImage handle, double scale, double mean0, double mean1, double mean2, double std0, double std1, double std2)
{
	XlCvImageImpl* image = (XlCvImageImpl*)handle;
	XlCvImageImpl* out;
	size_t count;
	size_t i;
	if ( image == NULL || image->data == NULL ) {
		xlcv_set_error("invalid image");
		return NULL;
	}
	out = xlcv_alloc(image->width, image->height, image->channels);
	if ( out == NULL ) return NULL;
	count = (size_t)image->width * (size_t)image->height * (size_t)image->channels;
	for ( i = 0; i < count; ++i ) {
		int c = (int)(i % (size_t)image->channels);
		double mean = xlcv_channel_value(c, mean0, mean1, mean2);
		double stddev = xlcv_channel_value(c, std0, std1, std2);
		if ( stddev == 0.0 ) stddev = 1.0;
		out->data[i] = (float)(((double)image->data[i] * scale - mean) / stddev);
	}
	return out;
}

XLCV_EXPORT const char* opencv_xl_to_chw_tensor_json(XlCvImage handle, double scale, double mean0, double mean1, double mean2, double std0, double std1, double std2)
{
	XlCvImageImpl* image = (XlCvImageImpl*)handle;
	size_t estimate;
	size_t pos = 0;
	int c;
	int y;
	int x;
	if ( image == NULL || image->data == NULL ) {
		xlcv_set_error("invalid image");
		return "";
	}
	estimate = 128u + (size_t)image->width * (size_t)image->height * (size_t)image->channels * 16u;
	if ( estimate > g_json_cap ) {
		char* next = (char*)realloc(g_json, estimate);
		if ( next == NULL ) {
			xlcv_set_error("out of memory");
			return "";
		}
		g_json = next;
		g_json_cap = estimate;
	}
	pos += (size_t)snprintf(g_json + pos, g_json_cap - pos, "{\"dtype\":\"float32\",\"shape\":[1,%d,%d,%d],\"data\":[", image->channels, image->height, image->width);
	for ( c = 0; c < image->channels; ++c ) {
		for ( y = 0; y < image->height; ++y ) {
			for ( x = 0; x < image->width; ++x ) {
				double mean = xlcv_channel_value(c, mean0, mean1, mean2);
				double stddev = xlcv_channel_value(c, std0, std1, std2);
				double v;
				if ( stddev == 0.0 ) stddev = 1.0;
				v = ((double)image->data[((size_t)y * image->width + x) * image->channels + c] * scale - mean) / stddev;
				pos += (size_t)snprintf(g_json + pos, g_json_cap - pos, "%s%.8g", (c || y || x) ? "," : "", v);
			}
		}
	}
	snprintf(g_json + pos, g_json_cap - pos, "]}");
	xlcv_set_error("");
	return g_json;
}

XLCV_EXPORT int opencv_xl_to_chw_tensor_f32(XlCvImage handle, double scale, double mean0, double mean1, double mean2, double std0, double std1, double std2, float** out_data, long long* out_shape, int* out_count)
{
	XlCvImageImpl* image = (XlCvImageImpl*)handle;
	float* data;
	size_t count;
	size_t pos = 0;
	int c;
	int y;
	int x;
	if ( out_data == NULL || out_shape == NULL || out_count == NULL ) {
		xlcv_set_error("invalid tensor output pointers");
		return 0;
	}
	*out_data = NULL;
	*out_count = 0;
	if ( image == NULL || image->data == NULL ) {
		xlcv_set_error("invalid image");
		return 0;
	}
	count = (size_t)image->width * (size_t)image->height * (size_t)image->channels;
	data = (float*)malloc(sizeof(float) * count);
	if ( data == NULL ) {
		xlcv_set_error("out of memory");
		return 0;
	}
	for ( c = 0; c < image->channels; ++c ) {
		for ( y = 0; y < image->height; ++y ) {
			for ( x = 0; x < image->width; ++x ) {
				double mean = xlcv_channel_value(c, mean0, mean1, mean2);
				double stddev = xlcv_channel_value(c, std0, std1, std2);
				double v;
				if ( stddev == 0.0 ) stddev = 1.0;
				v = ((double)image->data[((size_t)y * image->width + x) * image->channels + c] * scale - mean) / stddev;
				data[pos++] = (float)v;
			}
		}
	}
	out_shape[0] = 1;
	out_shape[1] = image->channels;
	out_shape[2] = image->height;
	out_shape[3] = image->width;
	*out_count = (int)count;
	*out_data = data;
	xlcv_set_error("");
	return 1;
}

XLCV_EXPORT void opencv_xl_release_tensor_f32(float* data)
{
	free(data);
}

XLCV_EXPORT int opencv_xl_width(XlCvImage handle)
{
	XlCvImageImpl* image = (XlCvImageImpl*)handle;
	return image != NULL ? image->width : 0;
}

XLCV_EXPORT int opencv_xl_height(XlCvImage handle)
{
	XlCvImageImpl* image = (XlCvImageImpl*)handle;
	return image != NULL ? image->height : 0;
}

XLCV_EXPORT int opencv_xl_channels(XlCvImage handle)
{
	XlCvImageImpl* image = (XlCvImageImpl*)handle;
	return image != NULL ? image->channels : 0;
}

XLCV_EXPORT void opencv_xl_release(XlCvImage handle)
{
	XlCvImageImpl* image = (XlCvImageImpl*)handle;
	if ( image != NULL ) {
		free(image->data);
		free(image);
	}
}
