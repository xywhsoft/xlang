#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xrt.h"
#include "include/opencv.h"

#if defined(_WIN32) || defined(_WIN64)
#define XL_EXPORT __declspec(dllexport)
#else
#define XL_EXPORT
#endif

typedef void* (*CvImreadFn)(const char*, int);
typedef int (*CvImwriteFn)(const char*, void*);
typedef void* (*CvResizeFn)(void*, int, int);
typedef void* (*CvCropFn)(void*, int, int, int, int);
typedef void* (*CvPerspectiveCropFn)(void*, double, double, double, double, double, double, double, double);
typedef void* (*CvRotate180Fn)(void*);
typedef void* (*CvRotate90Fn)(void*);
typedef void* (*CvCaptureRectFn)(int, int, int, int);
typedef void* (*CvCaptureWindowFn)(void*);
typedef void* (*CvCvtColorFn)(void*, int);
typedef void* (*CvCopyMakeBorderFn)(void*, int, int, int, int, int, double, double, double, double);
typedef void* (*CvRectangleFn)(void*, int, int, int, int, double, double, double, double, int);
typedef void* (*CvNormalizeFn)(void*, double, double, double, double, double, double, double);
typedef const char* (*CvToChwTensorJsonFn)(void*, double, double, double, double, double, double, double);
typedef int (*CvIntImageFn)(void*);
typedef void (*CvReleaseFn)(void*);
typedef const char* (*CvTextFn)(void);
typedef int (*CvAvailableFn)(void);

typedef struct CvApi {
	int loaded;
#if defined(_WIN32) || defined(_WIN64)
	HMODULE module;
#else
	void* module;
#endif
	CvImreadFn imread;
	CvImwriteFn imwrite;
	CvResizeFn resize;
	CvCropFn crop;
	CvPerspectiveCropFn perspective_crop;
	CvRotate180Fn rotate180;
	CvRotate90Fn rotate90ccw;
	CvCaptureRectFn capture_rect;
	CvCaptureWindowFn capture_window;
	CvCvtColorFn cvt_color;
	CvCopyMakeBorderFn copy_make_border;
	CvRectangleFn rectangle;
	CvNormalizeFn normalize;
	CvToChwTensorJsonFn to_chw_tensor_json;
	CvIntImageFn width;
	CvIntImageFn height;
	CvIntImageFn channels;
	CvReleaseFn release;
	CvAvailableFn available;
	CvTextFn version;
	CvTextFn last_error;
	char* path;
} CvApi;

static CvApi g_cv;
static xonce g_cv_once = XRT_ONCE_INIT;
static char g_error[512];

static void cv_copy(char* out, size_t out_size, const char* text)
{
	size_t len;

	if ( out == NULL || out_size == 0u ) return;
	if ( text == NULL ) text = "";
	len = strlen(text);
	if ( len >= out_size ) {
		len = out_size - 1u;
	}
	memcpy(out, text, len);
	out[len] = '\0';
}

static void cv_set_error(const char* text)
{
	cv_copy(g_error, sizeof(g_error), text);
}

static void* cv_sym(const char* name)
{
#if defined(_WIN32) || defined(_WIN64)
	return g_cv.module != NULL ? (void*)GetProcAddress(g_cv.module, name) : NULL;
#else
	return g_cv.module != NULL ? dlsym(g_cv.module, name) : NULL;
#endif
}

static int cv_try_load(const char* path)
{
	if ( path == NULL || path[0] == '\0' ) return 0;
#if defined(_WIN32) || defined(_WIN64)
	{
		uint16* wide = xrtUtf8To16(path, NULL);
		if ( wide == NULL ) return 0;
		g_cv.module = LoadLibraryExW((LPCWSTR)wide, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
		xrtFree(wide);
	}
#else
	g_cv.module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
	if ( g_cv.module == NULL ) return 0;

	g_cv.imread = (CvImreadFn)cv_sym("opencv_xl_imread");
	g_cv.imwrite = (CvImwriteFn)cv_sym("opencv_xl_imwrite");
	g_cv.resize = (CvResizeFn)cv_sym("opencv_xl_resize");
	g_cv.crop = (CvCropFn)cv_sym("opencv_xl_crop");
	g_cv.perspective_crop = (CvPerspectiveCropFn)cv_sym("opencv_xl_perspective_crop");
	g_cv.rotate180 = (CvRotate180Fn)cv_sym("opencv_xl_rotate180");
	g_cv.rotate90ccw = (CvRotate90Fn)cv_sym("opencv_xl_rotate90ccw");
	g_cv.capture_rect = (CvCaptureRectFn)cv_sym("opencv_xl_capture_rect");
	g_cv.capture_window = (CvCaptureWindowFn)cv_sym("opencv_xl_capture_window");
	g_cv.cvt_color = (CvCvtColorFn)cv_sym("opencv_xl_cvt_color");
	g_cv.copy_make_border = (CvCopyMakeBorderFn)cv_sym("opencv_xl_copy_make_border");
	g_cv.rectangle = (CvRectangleFn)cv_sym("opencv_xl_rectangle");
	g_cv.normalize = (CvNormalizeFn)cv_sym("opencv_xl_normalize");
	g_cv.to_chw_tensor_json = (CvToChwTensorJsonFn)cv_sym("opencv_xl_to_chw_tensor_json");
	g_cv.width = (CvIntImageFn)cv_sym("opencv_xl_width");
	g_cv.height = (CvIntImageFn)cv_sym("opencv_xl_height");
	g_cv.channels = (CvIntImageFn)cv_sym("opencv_xl_channels");
	g_cv.release = (CvReleaseFn)cv_sym("opencv_xl_release");
	g_cv.available = (CvAvailableFn)cv_sym("opencv_xl_available");
	g_cv.version = (CvTextFn)cv_sym("opencv_xl_version");
	g_cv.last_error = (CvTextFn)cv_sym("opencv_xl_last_error");
	if ( g_cv.imread == NULL || g_cv.imwrite == NULL || g_cv.resize == NULL ||
		g_cv.crop == NULL || g_cv.perspective_crop == NULL ||
		g_cv.rotate180 == NULL || g_cv.rotate90ccw == NULL ||
		g_cv.capture_rect == NULL || g_cv.capture_window == NULL ||
		g_cv.cvt_color == NULL || g_cv.copy_make_border == NULL ||
		g_cv.rectangle == NULL || g_cv.normalize == NULL ||
		g_cv.to_chw_tensor_json == NULL || g_cv.width == NULL ||
		g_cv.height == NULL || g_cv.channels == NULL || g_cv.release == NULL ||
		g_cv.available == NULL || g_cv.version == NULL ||
		g_cv.last_error == NULL ) {
		#if defined(_WIN32) || defined(_WIN64)
		FreeLibrary(g_cv.module);
		#else
		dlclose(g_cv.module);
		#endif
		g_cv.module = NULL;
		cv_set_error("opencv shim loaded but required symbols are missing");
		return 0;
	}
	g_cv.path = xrtStrDup(path);
	if ( g_cv.path == NULL ) {
		#if defined(_WIN32) || defined(_WIN64)
		FreeLibrary(g_cv.module);
		#else
		dlclose(g_cv.module);
		#endif
		g_cv.module = NULL;
		cv_set_error("opencv shim path allocation failed");
		return 0;
	}
	g_cv.loaded = 1;
	cv_set_error("");
	return 1;
}

static int cv_try_load_from_exe(const char* relative)
{
	char* exe_dir = xrtPathAppDir();
	char* parent_dir = exe_dir != NULL ? xrtPathParent(exe_dir) : NULL;
	char* path = exe_dir != NULL ? xrtPathJoin(exe_dir, relative) : NULL;
	int loaded = path != NULL && cv_try_load(path);
	xrtFree(path);
	if ( !loaded && parent_dir != NULL ) {
		path = xrtPathJoin(parent_dir, relative);
		loaded = path != NULL && cv_try_load(path);
		xrtFree(path);
	}
	xrtFree(parent_dir);
	xrtFree(exe_dir);
	return loaded;
}

/* DLL 装载和函数表发布必须一起完成，等待中的线程不能观察半初始化状态。 */
static bool cv_load_once(ptr unused)
{
	char* env_path;
	const char* candidates[] = {
		"opencv_xlang.dll",
		"bin\\opencv_xlang.dll",
		"extlibs\\opencv\\bin\\opencv_xlang.dll",
		"release\\extlibs\\opencv\\bin\\opencv_xlang.dll",
		"libs\\opencv\\bin\\opencv_xlang.dll",
		"release\\libs\\opencv\\bin\\opencv_xlang.dll",
		NULL
	};
	int i;
	(void)unused;
	env_path = xrtEnvGet("OPENCV_XLANG_DLL");
	if ( env_path != NULL && env_path[0] != '\0' ) {
		int loaded = cv_try_load(env_path);
		if ( !loaded && g_error[0] == '\0' ) {
			cv_set_error("opencv shim not found at OPENCV_XLANG_DLL");
		}
		xrtFree(env_path);
		return true;
	}
	xrtFree(env_path);
	if ( cv_try_load_from_exe("extlibs/opencv/bin/opencv_xlang.dll") ) return true;
	for ( i = 0; candidates[i] != NULL; ++i ) {
		if ( cv_try_load(candidates[i]) ) return true;
	}
	if ( g_error[0] == '\0' ) {
		cv_set_error("opencv shim not found; set OPENCV_XLANG_DLL or place opencv_xlang.dll under extlibs\\opencv\\bin");
	}
	return true;
}

static int cv_load(void)
{
	return xrtOnce(&g_cv_once, cv_load_once, NULL) && g_cv.loaded;
}

XL_EXPORT int opencv_available_native(void)
{
	if ( !cv_load() ) return 0;
	return g_cv.available != NULL ? g_cv.available() : 1;
}

XL_EXPORT const char* opencv_backend_native(void) { return cv_load() ? g_cv.path : ""; }

XL_EXPORT const char* opencv_last_error_native(void)
{
	if ( cv_load() && g_cv.last_error != NULL ) {
		const char* backend_error = g_cv.last_error();
		if ( backend_error != NULL && backend_error[0] != '\0' ) return backend_error;
	}
	return g_error;
}

XL_EXPORT void* opencv_imread_native(const char* path, int flags)
{
	if ( !cv_load() || g_cv.imread == NULL ) return NULL;
	return g_cv.imread(path, flags);
}

XL_EXPORT int opencv_imwrite_native(const char* path, void* image)
{
	if ( !cv_load() || g_cv.imwrite == NULL ) return 0;
	return g_cv.imwrite(path, image);
}

XL_EXPORT void* opencv_resize_native(void* image, int width, int height)
{
	if ( !cv_load() || g_cv.resize == NULL ) return NULL;
	return g_cv.resize(image, width, height);
}

XL_EXPORT void* opencv_crop_native(void* image, int x, int y, int width, int height)
{
	if ( !cv_load() || g_cv.crop == NULL ) return NULL;
	return g_cv.crop(image, x, y, width, height);
}

XL_EXPORT void* opencv_perspective_crop_native(void* image, double x0, double y0, double x1, double y1, double x2, double y2, double x3, double y3)
{
	if ( !cv_load() || g_cv.perspective_crop == NULL ) return NULL;
	return g_cv.perspective_crop(image, x0, y0, x1, y1, x2, y2, x3, y3);
}

XL_EXPORT void* opencv_rotate180_native(void* image)
{
	if ( !cv_load() || g_cv.rotate180 == NULL ) return NULL;
	return g_cv.rotate180(image);
}

XL_EXPORT void* opencv_rotate90ccw_native(void* image)
{
	if ( !cv_load() || g_cv.rotate90ccw == NULL ) return NULL;
	return g_cv.rotate90ccw(image);
}

XL_EXPORT void* opencv_capture_rect_native(int x, int y, int width, int height)
{
	if ( !cv_load() || g_cv.capture_rect == NULL ) return NULL;
	return g_cv.capture_rect(x, y, width, height);
}

XL_EXPORT void* opencv_capture_window_native(void* hwnd)
{
	if ( !cv_load() || g_cv.capture_window == NULL ) return NULL;
	return g_cv.capture_window(hwnd);
}

XL_EXPORT void* opencv_cvt_color_native(void* image, int code)
{
	if ( !cv_load() || g_cv.cvt_color == NULL ) return NULL;
	return g_cv.cvt_color(image, code);
}

XL_EXPORT void* opencv_copy_make_border_native(void* image, int top, int bottom, int left, int right, int border_type, double value0, double value1, double value2, double value3)
{
	if ( !cv_load() || g_cv.copy_make_border == NULL ) return NULL;
	return g_cv.copy_make_border(image, top, bottom, left, right, border_type, value0, value1, value2, value3);
}

XL_EXPORT void* opencv_rectangle_native(void* image, int x1, int y1, int x2, int y2, double value0, double value1, double value2, double value3, int thickness)
{
	if ( !cv_load() || g_cv.rectangle == NULL ) return NULL;
	return g_cv.rectangle(image, x1, y1, x2, y2, value0, value1, value2, value3, thickness);
}

XL_EXPORT void* opencv_normalize_native(void* image, double scale, double mean0, double mean1, double mean2, double std0, double std1, double std2)
{
	if ( !cv_load() || g_cv.normalize == NULL ) return NULL;
	return g_cv.normalize(image, scale, mean0, mean1, mean2, std0, std1, std2);
}

XL_EXPORT const char* opencv_to_chw_tensor_json_native(void* image, double scale, double mean0, double mean1, double mean2, double std0, double std1, double std2)
{
	if ( !cv_load() || g_cv.to_chw_tensor_json == NULL ) return "";
	return g_cv.to_chw_tensor_json(image, scale, mean0, mean1, mean2, std0, std1, std2);
}

XL_EXPORT int opencv_width_native(void* image)
{
	if ( !cv_load() || g_cv.width == NULL ) return 0;
	return g_cv.width(image);
}

XL_EXPORT int opencv_height_native(void* image)
{
	if ( !cv_load() || g_cv.height == NULL ) return 0;
	return g_cv.height(image);
}

XL_EXPORT int opencv_channels_native(void* image)
{
	if ( !cv_load() || g_cv.channels == NULL ) return 0;
	return g_cv.channels(image);
}

XL_EXPORT int opencv_release_native(void* image)
{
	if ( image != NULL && cv_load() && g_cv.release != NULL ) {
		g_cv.release(image);
		return 1;
	}
	return 0;
}
