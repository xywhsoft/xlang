#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if defined(_WIN32) || defined(_WIN64)
#define XL_EXPORT __declspec(dllexport)
#else
#define XL_EXPORT
#endif

typedef void* (*RapidCreateFn)(const char*);
typedef void (*RapidReleaseFn)(void*);
typedef void* (*RapidCallFn)(void*, const char*, int, int, int, int);
typedef int (*RapidOkFn)(void*);
typedef const char* (*RapidTextFn)(void*);
typedef double (*RapidFloatFn)(void*);
typedef int (*RapidVisFn)(void*, const char*);
typedef const char* (*RapidLastErrorFn)(void);
typedef const char* (*OrtResultShapeFn)(void*);
typedef int (*OrtResultCountFn)(void*);
typedef float (*OrtResultFloatAtFn)(void*, int);
typedef void* (*RapidCvImreadFn)(const char*, int);
typedef int (*RapidCvImwriteFn)(const char*, void*);
typedef void* (*RapidCvRectangleFn)(void*, int, int, int, int, double, double, double, double, int);
typedef void (*RapidCvReleaseFn)(void*);

typedef struct RapidApi {
	int attempted;
	int loaded;
#if defined(_WIN32) || defined(_WIN64)
	HMODULE module;
#else
	void* module;
#endif
	RapidCreateFn create;
	RapidReleaseFn release;
	RapidCallFn call;
	RapidReleaseFn output_release;
	RapidOkFn output_ok;
	RapidTextFn output_json;
	RapidTextFn output_text;
	RapidTextFn output_boxes;
	RapidTextFn output_txts;
	RapidTextFn output_scores;
	RapidTextFn output_word_results;
	RapidTextFn output_elapse_list;
	RapidFloatFn output_elapse;
	RapidVisFn output_vis;
	RapidLastErrorFn last_error;
	char path[512];
} RapidApi;

typedef struct RapidFallbackOutput {
	char json[256];
} RapidFallbackOutput;

static RapidApi g_rapid;
static char g_error[512];
static char g_det_boxes_json[32768];
static char g_charset_json[512];
static char g_rec_json_ring[16][4096];
static int g_rec_json_index;
static char g_assembled_json[65536];
static char g_ocr_items_json[65536];
static char g_cls_json[256];
static char g_charset_cache_path[1024];
static char** g_charset_cache_items;
static int g_charset_cache_count;

#define RAPIDOCR_MAX_BOXES 256

typedef struct RapidOrtApi {
	int attempted;
	int loaded;
#if defined(_WIN32) || defined(_WIN64)
	HMODULE module;
#else
	void* module;
#endif
	OrtResultShapeFn shape_json;
	OrtResultCountFn float_count;
	OrtResultFloatAtFn float_at;
} RapidOrtApi;

static RapidOrtApi g_ort;

typedef struct RapidCvApi {
	int attempted;
	int loaded;
#if defined(_WIN32) || defined(_WIN64)
	HMODULE module;
#else
	void* module;
#endif
	RapidCvImreadFn imread;
	RapidCvImwriteFn imwrite;
	RapidCvRectangleFn rectangle;
	RapidCvReleaseFn release;
} RapidCvApi;

static RapidCvApi g_cv;

static char* rapid_next_rec_json(void)
{
	char* buffer = g_rec_json_ring[g_rec_json_index & 15];
	g_rec_json_index = (g_rec_json_index + 1) & 15;
	buffer[0] = '\0';
	return buffer;
}

static void rapid_set_error(const char* text)
{
	if ( text == NULL ) text = "";
	strncpy(g_error, text, sizeof(g_error) - 1u);
	g_error[sizeof(g_error) - 1u] = '\0';
}

static void rapid_copy(char* out, size_t out_size, const char* text)
{
	if ( out == NULL || out_size == 0u ) return;
	if ( text == NULL ) text = "";
	strncpy(out, text, out_size - 1u);
	out[out_size - 1u] = '\0';
}

static void rapid_append(char* out, size_t out_size, size_t* used, const char* text)
{
	size_t n;
	if ( out == NULL || out_size == 0u || used == NULL || text == NULL ) return;
	if ( *used >= out_size ) return;
	n = strlen(text);
	if ( n >= out_size - *used ) n = out_size - *used - 1u;
	memcpy(out + *used, text, n);
	*used += n;
	out[*used] = '\0';
}

static void* rapid_sym(const char* name)
{
#if defined(_WIN32) || defined(_WIN64)
	return g_rapid.module != NULL ? (void*)GetProcAddress(g_rapid.module, name) : NULL;
#else
	return g_rapid.module != NULL ? dlsym(g_rapid.module, name) : NULL;
#endif
}

static int rapid_try_load(const char* path)
{
	if ( path == NULL || path[0] == '\0' ) return 0;
#if defined(_WIN32) || defined(_WIN64)
	g_rapid.module = LoadLibraryA(path);
#else
	g_rapid.module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
	if ( g_rapid.module == NULL ) return 0;
	g_rapid.create = (RapidCreateFn)rapid_sym("rapidocr_xl_create");
	g_rapid.release = (RapidReleaseFn)rapid_sym("rapidocr_xl_release");
	g_rapid.call = (RapidCallFn)rapid_sym("rapidocr_xl_call");
	g_rapid.output_release = (RapidReleaseFn)rapid_sym("rapidocr_xl_output_release");
	g_rapid.output_ok = (RapidOkFn)rapid_sym("rapidocr_xl_output_ok");
	g_rapid.output_json = (RapidTextFn)rapid_sym("rapidocr_xl_output_json");
	g_rapid.output_text = (RapidTextFn)rapid_sym("rapidocr_xl_output_text");
	g_rapid.output_boxes = (RapidTextFn)rapid_sym("rapidocr_xl_output_boxes");
	g_rapid.output_txts = (RapidTextFn)rapid_sym("rapidocr_xl_output_txts");
	g_rapid.output_scores = (RapidTextFn)rapid_sym("rapidocr_xl_output_scores");
	g_rapid.output_word_results = (RapidTextFn)rapid_sym("rapidocr_xl_output_word_results");
	g_rapid.output_elapse_list = (RapidTextFn)rapid_sym("rapidocr_xl_output_elapse_list");
	g_rapid.output_elapse = (RapidFloatFn)rapid_sym("rapidocr_xl_output_elapse");
	g_rapid.output_vis = (RapidVisFn)rapid_sym("rapidocr_xl_output_vis");
	g_rapid.last_error = (RapidLastErrorFn)rapid_sym("rapidocr_xl_last_error");
	if ( g_rapid.create == NULL || g_rapid.call == NULL ) {
		rapid_set_error("rapidocr shim loaded but required symbols are missing");
		return 0;
	}
	g_rapid.loaded = 1;
	rapid_copy(g_rapid.path, sizeof(g_rapid.path), path);
	rapid_set_error("");
	return 1;
}

static int rapid_load(void)
{
	const char* env_path;
	const char* candidates[] = {
		"rapidocr_xlang.dll",
		"bin\\rapidocr_xlang.dll",
		"libs\\rapidocr\\bin\\rapidocr_xlang.dll",
		"release\\libs\\rapidocr\\bin\\rapidocr_xlang.dll",
		"examples\\libs\\rapidocr\\bin\\rapidocr_xlang.dll",
		NULL
	};
	int i;
	if ( g_rapid.loaded ) return 1;
	if ( g_rapid.attempted ) return 0;
	g_rapid.attempted = 1;
	env_path = getenv("RAPIDOCR_XLANG_DLL");
	if ( env_path != NULL && env_path[0] != '\0' && rapid_try_load(env_path) ) return 1;
	for ( i = 0; candidates[i] != NULL; ++i ) {
		if ( rapid_try_load(candidates[i]) ) return 1;
	}
	rapid_set_error("rapidocr shim not found; set RAPIDOCR_XLANG_DLL or place rapidocr_xlang.dll under libs\\rapidocr\\bin");
	return 0;
}

static void* rapid_ort_sym(const char* name)
{
#if defined(_WIN32) || defined(_WIN64)
	return g_ort.module != NULL ? (void*)GetProcAddress(g_ort.module, name) : NULL;
#else
	return g_ort.module != NULL ? dlsym(g_ort.module, name) : NULL;
#endif
}

static int rapid_ort_try_load(const char* path)
{
	if ( path == NULL || path[0] == '\0' ) return 0;
#if defined(_WIN32) || defined(_WIN64)
	g_ort.module = LoadLibraryA(path);
#else
	g_ort.module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
	if ( g_ort.module == NULL ) return 0;
	g_ort.shape_json = (OrtResultShapeFn)rapid_ort_sym("onnxruntime_xl_result_float_shape_json");
	g_ort.float_count = (OrtResultCountFn)rapid_ort_sym("onnxruntime_xl_result_float_count");
	g_ort.float_at = (OrtResultFloatAtFn)rapid_ort_sym("onnxruntime_xl_result_float_at");
	if ( g_ort.shape_json == NULL || g_ort.float_count == NULL || g_ort.float_at == NULL ) return 0;
	g_ort.loaded = 1;
	return 1;
}

static int rapid_ort_load(void)
{
	const char* env_path;
	const char* candidates[] = {
		"onnxruntime_xlang.dll",
		"bin\\onnxruntime_xlang.dll",
		"libs\\onnxruntime\\bin\\onnxruntime_xlang.dll",
		"release\\libs\\onnxruntime\\bin\\onnxruntime_xlang.dll",
		"examples\\libs\\onnxruntime\\bin\\onnxruntime_xlang.dll",
		"D:\\git\\x-lang\\demo5\\release\\libs\\onnxruntime\\bin\\onnxruntime_xlang.dll",
		NULL
	};
	int i;
	if ( g_ort.loaded ) return 1;
	if ( g_ort.attempted ) return 0;
	g_ort.attempted = 1;
	env_path = getenv("ONNXRUNTIME_XLANG_DLL");
	if ( env_path != NULL && env_path[0] != '\0' && rapid_ort_try_load(env_path) ) return 1;
	for ( i = 0; candidates[i] != NULL; ++i ) {
		if ( rapid_ort_try_load(candidates[i]) ) return 1;
	}
	rapid_set_error("onnxruntime_xlang.dll with typed output symbols was not found");
	return 0;
}

static void* rapid_cv_sym(const char* name)
{
#if defined(_WIN32) || defined(_WIN64)
	return g_cv.module != NULL ? (void*)GetProcAddress(g_cv.module, name) : NULL;
#else
	return g_cv.module != NULL ? dlsym(g_cv.module, name) : NULL;
#endif
}

static int rapid_cv_try_load(const char* path)
{
	if ( path == NULL || path[0] == '\0' ) return 0;
#if defined(_WIN32) || defined(_WIN64)
	g_cv.module = LoadLibraryA(path);
#else
	g_cv.module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
	if ( g_cv.module == NULL ) return 0;
	g_cv.imread = (RapidCvImreadFn)rapid_cv_sym("opencv_xl_imread");
	g_cv.imwrite = (RapidCvImwriteFn)rapid_cv_sym("opencv_xl_imwrite");
	g_cv.rectangle = (RapidCvRectangleFn)rapid_cv_sym("opencv_xl_rectangle");
	g_cv.release = (RapidCvReleaseFn)rapid_cv_sym("opencv_xl_release");
	if ( g_cv.imread == NULL || g_cv.imwrite == NULL || g_cv.release == NULL ) return 0;
	g_cv.loaded = 1;
	return 1;
}

static int rapid_cv_load(void)
{
	const char* env_path;
	const char* candidates[] = {
		"opencv_xlang.dll",
		"bin\\opencv_xlang.dll",
		"libs\\opencv\\bin\\opencv_xlang.dll",
		"release\\libs\\opencv\\bin\\opencv_xlang.dll",
		"examples\\libs\\opencv\\bin\\opencv_xlang.dll",
		"D:\\git\\x-lang\\demo5\\release\\libs\\opencv\\bin\\opencv_xlang.dll",
		NULL
	};
	int i;
	if ( g_cv.loaded ) return 1;
	if ( g_cv.attempted ) return 0;
	g_cv.attempted = 1;
	env_path = getenv("OPENCV_XLANG_DLL");
	if ( env_path != NULL && env_path[0] != '\0' && rapid_cv_try_load(env_path) ) return 1;
	for ( i = 0; candidates[i] != NULL; ++i ) {
		if ( rapid_cv_try_load(candidates[i]) ) return 1;
	}
	rapid_set_error("opencv_xlang.dll with drawing symbols was not found");
	return 0;
}

static int rapid_parse_box_rect_at(const char* boxes_json, int target_index, int* out_x1, int* out_y1, int* out_x2, int* out_y2);
static int rapid_parse_shape_tail(const char* shape_json, int* out_time, int* out_classes);

static int rapid_parse_first_box_rect(const char* boxes_json, int* out_x1, int* out_y1, int* out_x2, int* out_y2)
{
	return rapid_parse_box_rect_at(boxes_json, 0, out_x1, out_y1, out_x2, out_y2);
}

static int rapid_parse_box_rect_at(const char* boxes_json, int target_index, int* out_x1, int* out_y1, int* out_x2, int* out_y2)
{
	const char* p = boxes_json;
	int values[8];
	int count = 0;
	int current_index = 0;
	if ( out_x1 == NULL || out_y1 == NULL || out_x2 == NULL || out_y2 == NULL ) return 0;
	if ( p == NULL || target_index < 0 ) return 0;
	while ( *p != '\0' ) {
		const char* box = strstr(p, "\"box\"");
		if ( box == NULL ) return 0;
		p = strchr(box, '[');
		if ( p == NULL ) return 0;
		count = 0;
		while ( *p != '\0' && count < 8 ) {
			char* endp;
			while ( *p != '\0' && *p != '-' && (*p < '0' || *p > '9') ) ++p;
			if ( *p == '\0' ) break;
			values[count++] = (int)strtol(p, &endp, 10);
			if ( endp == p ) break;
			p = endp;
		}
		if ( count >= 8 && current_index == target_index ) break;
		++current_index;
	}
	if ( count < 8 ) return 0;
	*out_x1 = values[0];
	*out_y1 = values[1];
	*out_x2 = values[0];
	*out_y2 = values[1];
	for ( count = 2; count < 8; count += 2 ) {
		if ( values[count] < *out_x1 ) *out_x1 = values[count];
		if ( values[count] > *out_x2 ) *out_x2 = values[count];
		if ( values[count + 1] < *out_y1 ) *out_y1 = values[count + 1];
		if ( values[count + 1] > *out_y2 ) *out_y2 = values[count + 1];
	}
	return 1;
}

static int rapid_box_count(const char* boxes_json)
{
	const char* p = boxes_json;
	int count = 0;
	if ( p == NULL ) return 0;
	while ( (p = strstr(p, "\"box\"")) != NULL ) {
		++count;
		p += 5;
	}
	return count;
}

XL_EXPORT int rapidocr_vis_image_native(const char* source_path, const char* boxes_json, const char* save_path)
{
	void* image;
	void* current;
	int ok;
	int count;
	int i;
	if ( source_path == NULL || source_path[0] == '\0' || save_path == NULL || save_path[0] == '\0' ) {
		rapid_set_error("invalid visualization paths");
		return 0;
	}
	if ( !rapid_cv_load() ) return 0;
	image = g_cv.imread(source_path, 1);
	if ( image == NULL ) {
		rapid_set_error("failed to load visualization source image");
		return 0;
	}
	current = image;
	count = rapid_box_count(boxes_json);
	if ( count > 16 ) count = 16;
	if ( g_cv.rectangle != NULL ) {
		for ( i = 0; i < count; ++i ) {
			int x1 = 0;
			int y1 = 0;
			int x2 = 0;
			int y2 = 0;
			void* next;
			if ( !rapid_parse_box_rect_at(boxes_json, i, &x1, &y1, &x2, &y2) ) continue;
			next = g_cv.rectangle(current, x1, y1, x2, y2, 0.0, 255.0, 0.0, 0.0, 2);
			if ( next == NULL ) continue;
			if ( current != image ) g_cv.release(current);
			current = next;
		}
	}
	ok = g_cv.imwrite(save_path, current);
	if ( current != image ) g_cv.release(current);
	g_cv.release(image);
	rapid_set_error(ok ? "" : "failed to write visualization image");
	return ok ? 1 : 0;
}

XL_EXPORT int rapidocr_first_box_x_native(const char* boxes_json)
{
	int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
	(void)y1; (void)x2; (void)y2;
	return rapid_parse_first_box_rect(boxes_json, &x1, &y1, &x2, &y2) ? x1 : 0;
}

XL_EXPORT int rapidocr_box_count_native(const char* boxes_json)
{
	return rapid_box_count(boxes_json);
}

XL_EXPORT int rapidocr_box_x_native(const char* boxes_json, int index)
{
	int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
	(void)y1; (void)x2; (void)y2;
	return rapid_parse_box_rect_at(boxes_json, index, &x1, &y1, &x2, &y2) ? x1 : 0;
}

XL_EXPORT int rapidocr_box_y_native(const char* boxes_json, int index)
{
	int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
	(void)x1; (void)x2; (void)y2;
	return rapid_parse_box_rect_at(boxes_json, index, &x1, &y1, &x2, &y2) ? y1 : 0;
}

XL_EXPORT int rapidocr_box_width_native(const char* boxes_json, int index)
{
	int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
	(void)y1; (void)y2;
	return rapid_parse_box_rect_at(boxes_json, index, &x1, &y1, &x2, &y2) && x2 > x1 ? x2 - x1 : 0;
}

XL_EXPORT int rapidocr_box_height_native(const char* boxes_json, int index)
{
	int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
	(void)x1; (void)x2;
	return rapid_parse_box_rect_at(boxes_json, index, &x1, &y1, &x2, &y2) && y2 > y1 ? y2 - y1 : 0;
}

XL_EXPORT int rapidocr_first_box_y_native(const char* boxes_json)
{
	int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
	(void)x1; (void)x2; (void)y2;
	return rapid_parse_first_box_rect(boxes_json, &x1, &y1, &x2, &y2) ? y1 : 0;
}

XL_EXPORT int rapidocr_first_box_width_native(const char* boxes_json)
{
	int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
	(void)y1; (void)y2;
	return rapid_parse_first_box_rect(boxes_json, &x1, &y1, &x2, &y2) && x2 > x1 ? x2 - x1 : 0;
}

XL_EXPORT int rapidocr_first_box_height_native(const char* boxes_json)
{
	int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
	(void)x1; (void)x2;
	return rapid_parse_first_box_rect(boxes_json, &x1, &y1, &x2, &y2) && y2 > y1 ? y2 - y1 : 0;
}

XL_EXPORT int rapidocr_available_native(void) { return rapid_load(); }
XL_EXPORT const char* rapidocr_backend_native(void) { return rapid_load() ? g_rapid.path : ""; }
XL_EXPORT const char* rapidocr_last_error_native(void) { return g_error; }
XL_EXPORT void* rapidocr_create_native(const char* config_path) { return (rapid_load() && g_rapid.create != NULL) ? g_rapid.create(config_path) : calloc(1, 1); }
XL_EXPORT int rapidocr_release_native(void* handle) { if ( rapid_load() && g_rapid.release != NULL ) g_rapid.release(handle); else free(handle); return 1; }
XL_EXPORT void* rapidocr_call_native(void* handle, const char* img, int use_det, int use_cls, int use_rec, int return_word_box)
{
	RapidFallbackOutput* out;
	(void)img; (void)use_det; (void)use_cls; (void)use_rec; (void)return_word_box;
	if ( rapid_load() && g_rapid.call != NULL ) return g_rapid.call(handle, img, use_det, use_cls, use_rec, return_word_box);
	out = (RapidFallbackOutput*)calloc(1, sizeof(RapidFallbackOutput));
	if ( out != NULL ) rapid_copy(out->json, sizeof(out->json), "{\"boxes\":[],\"txts\":[],\"scores\":[],\"word_results\":[],\"elapse_list\":[],\"elapse\":0}");
	return out;
}
XL_EXPORT int rapidocr_output_release_native(void* handle) { if ( rapid_load() && g_rapid.output_release != NULL ) g_rapid.output_release(handle); else free(handle); return 1; }
XL_EXPORT int rapidocr_output_ok_native(void* handle) { return (handle != NULL && rapid_load() && g_rapid.output_ok != NULL) ? g_rapid.output_ok(handle) : 0; }
XL_EXPORT const char* rapidocr_output_json_native(void* handle) { return (handle != NULL && rapid_load() && g_rapid.output_json != NULL) ? g_rapid.output_json(handle) : "{\"boxes\":[],\"txts\":[],\"scores\":[],\"word_results\":[],\"elapse_list\":[],\"elapse\":0}"; }
XL_EXPORT const char* rapidocr_output_text_native(void* handle) { return (handle != NULL && rapid_load() && g_rapid.output_text != NULL) ? g_rapid.output_text(handle) : ""; }
XL_EXPORT const char* rapidocr_output_boxes_native(void* handle) { return (handle != NULL && rapid_load() && g_rapid.output_boxes != NULL) ? g_rapid.output_boxes(handle) : "[]"; }
XL_EXPORT const char* rapidocr_output_txts_native(void* handle) { return (handle != NULL && rapid_load() && g_rapid.output_txts != NULL) ? g_rapid.output_txts(handle) : "[]"; }
XL_EXPORT const char* rapidocr_output_scores_native(void* handle) { return (handle != NULL && rapid_load() && g_rapid.output_scores != NULL) ? g_rapid.output_scores(handle) : "[]"; }
XL_EXPORT const char* rapidocr_output_word_results_native(void* handle) { return (handle != NULL && rapid_load() && g_rapid.output_word_results != NULL) ? g_rapid.output_word_results(handle) : "[]"; }
XL_EXPORT const char* rapidocr_output_elapse_list_native(void* handle) { return (handle != NULL && rapid_load() && g_rapid.output_elapse_list != NULL) ? g_rapid.output_elapse_list(handle) : "[]"; }
XL_EXPORT double rapidocr_output_elapse_native(void* handle) { return (handle != NULL && rapid_load() && g_rapid.output_elapse != NULL) ? g_rapid.output_elapse(handle) : 0.0; }
XL_EXPORT int rapidocr_output_vis_native(void* handle, const char* save_path) { return (handle != NULL && rapid_load() && g_rapid.output_vis != NULL) ? g_rapid.output_vis(handle, save_path) : 0; }

static const char* rapid_find_data_array(const char* json)
{
	const char* data = strstr(json != NULL ? json : "", "\"data\"");
	if ( data == NULL ) data = strstr(json != NULL ? json : "", "data");
	if ( data == NULL ) return NULL;
	data = strchr(data, '[');
	return data != NULL ? data + 1 : NULL;
}

static int rapid_parse_shape_hw(const char* json, int* out_h, int* out_w)
{
	const char* shape = strstr(json != NULL ? json : "", "\"shape\"");
	int values[8];
	int count = 0;
	if ( out_h == NULL || out_w == NULL ) return 0;
	if ( shape == NULL ) shape = strstr(json != NULL ? json : "", "shape");
	if ( shape == NULL ) return 0;
	shape = strchr(shape, '[');
	if ( shape == NULL ) return 0;
	++shape;
	while ( *shape != '\0' && *shape != ']' && count < 8 ) {
		char* endp;
		long value = strtol(shape, &endp, 10);
		if ( endp != shape ) {
			values[count++] = (int)value;
			shape = endp;
		} else {
			++shape;
		}
	}
	if ( count >= 2 ) {
		*out_h = values[count - 2];
		*out_w = values[count - 1];
		return *out_h > 0 && *out_w > 0;
	}
	return 0;
}

static int rapid_component_range_bounds(const double* values, int map_w, int min_x, int min_y, int max_x, int max_y, int range_x1, int range_x2, double threshold, int* out_min_x, int* out_min_y, int* out_max_x, int* out_max_y, double* out_score, int* out_hits)
{
	int x;
	int y;
	int hits = 0;
	double sum = 0.0;
	int bx1 = max_x;
	int by1 = max_y;
	int bx2 = min_x;
	int by2 = min_y;
	if ( values == NULL || range_x1 > range_x2 ) return 0;
	if ( range_x1 < min_x ) range_x1 = min_x;
	if ( range_x2 > max_x ) range_x2 = max_x;
	for ( y = min_y; y <= max_y; ++y ) {
		for ( x = range_x1; x <= range_x2; ++x ) {
			double value = values[(size_t)y * (size_t)map_w + (size_t)x];
			if ( value < threshold ) continue;
			if ( x < bx1 ) bx1 = x;
			if ( y < by1 ) by1 = y;
			if ( x > bx2 ) bx2 = x;
			if ( y > by2 ) by2 = y;
			sum += value;
			++hits;
		}
	}
	if ( hits < 4 ) return 0;
	*out_min_x = bx1;
	*out_min_y = by1;
	*out_max_x = bx2;
	*out_max_y = by2;
	*out_score = sum / (double)hits;
	*out_hits = hits;
	return 1;
}

static int rapid_component_vertical_split(const double* values, int map_w, int min_x, int min_y, int max_x, int max_y, double threshold, int* out_left_x2, int* out_right_x1)
{
	int comp_w = max_x - min_x + 1;
	int comp_h = max_y - min_y + 1;
	int margin;
	int low_limit;
	int x;
	int best_start = -1;
	int best_end = -1;
	int run_start = -1;
	int run_score = 0;
	int best_score = 0;
	int left_hits = 0;
	int right_hits = 0;
	if ( values == NULL || out_left_x2 == NULL || out_right_x1 == NULL ) return 0;
	if ( comp_w < 8 || comp_h < 6 ) return 0;
	if ( comp_h > 16 ) return 0;
	if ( comp_w < comp_h * 2 || comp_w > comp_h * 8 ) return 0;
	margin = comp_w / 6;
	if ( margin < 2 ) margin = 2;
	low_limit = comp_h / 4;
	if ( low_limit < 1 ) low_limit = 1;
	for ( x = min_x + margin; x <= max_x - margin; ++x ) {
		int y;
		int col_hits = 0;
		for ( y = min_y; y <= max_y; ++y ) {
			if ( values[(size_t)y * (size_t)map_w + (size_t)x] >= threshold ) ++col_hits;
		}
		if ( col_hits <= low_limit ) {
			if ( run_start < 0 ) {
				run_start = x;
				run_score = 0;
			}
			run_score += low_limit - col_hits + 1;
		} else if ( run_start >= 0 ) {
			int run_len = x - run_start;
			if ( run_len > best_end - best_start + 1 || (run_len == best_end - best_start + 1 && run_score > best_score) ) {
				best_start = run_start;
				best_end = x - 1;
				best_score = run_score;
			}
			run_start = -1;
			run_score = 0;
		}
	}
	if ( run_start >= 0 ) {
		int run_len = max_x - margin - run_start + 1;
		if ( run_len > best_end - best_start + 1 || (run_len == best_end - best_start + 1 && run_score > best_score) ) {
			best_start = run_start;
			best_end = max_x - margin;
		}
	}
	if ( best_start < 0 ) return 0;
	for ( x = min_x; x < best_start; ++x ) {
		int y;
		for ( y = min_y; y <= max_y; ++y ) {
			if ( values[(size_t)y * (size_t)map_w + (size_t)x] >= threshold ) ++left_hits;
		}
	}
	for ( x = best_end + 1; x <= max_x; ++x ) {
		int y;
		for ( y = min_y; y <= max_y; ++y ) {
			if ( values[(size_t)y * (size_t)map_w + (size_t)x] >= threshold ) ++right_hits;
		}
	}
	if ( left_hits < 4 || right_hits < 4 ) return 0;
	*out_left_x2 = best_start - 1;
	*out_right_x1 = best_end + 1;
	return 1;
}

XL_EXPORT const char* rapidocr_det_boxes_from_output_native(const char* output_json, int image_width, int image_height, double threshold)
{
	const char* p;
	int map_h = 0;
	int map_w = 0;
	int x;
	int y;
	double* values;
	unsigned char* visited;
	int* queue_x;
	int* queue_y;
	int box_count = 0;
	int kept_x1[RAPIDOCR_MAX_BOXES];
	int kept_y1[RAPIDOCR_MAX_BOXES];
	int kept_x2[RAPIDOCR_MAX_BOXES];
	int kept_y2[RAPIDOCR_MAX_BOXES];
	size_t used = 0;
	if ( output_json == NULL || output_json[0] == '\0' || image_width <= 0 || image_height <= 0 ) {
		rapid_set_error("invalid detector output or image size");
		return "[]";
	}
	if ( threshold <= 0.0 ) threshold = 0.3;
	if ( !rapid_parse_shape_hw(output_json, &map_h, &map_w) ) {
		rapid_set_error("detector output shape not found");
		return "[]";
	}
	p = rapid_find_data_array(output_json);
	if ( p == NULL ) {
		rapid_set_error("detector output data not found");
		return "[]";
	}
	values = (double*)calloc((size_t)map_w * (size_t)map_h, sizeof(double));
	visited = (unsigned char*)calloc((size_t)map_w * (size_t)map_h, sizeof(unsigned char));
	queue_x = (int*)malloc((size_t)map_w * (size_t)map_h * sizeof(int));
	queue_y = (int*)malloc((size_t)map_w * (size_t)map_h * sizeof(int));
	if ( values == NULL || visited == NULL || queue_x == NULL || queue_y == NULL ) {
		free(values);
		free(visited);
		free(queue_x);
		free(queue_y);
		rapid_set_error("out of memory");
		return "[]";
	}
	for ( y = 0; y < map_h; ++y ) {
		for ( x = 0; x < map_w; ++x ) {
			char* endp;
			double v = strtod(p, &endp);
			if ( endp == p ) {
				free(values);
				free(visited);
				free(queue_x);
				free(queue_y);
				rapid_set_error("detector output data parse failed");
				return "[]";
			}
			values[(size_t)y * (size_t)map_w + (size_t)x] = v;
			p = endp;
			while ( *p == ',' || *p == ' ' || *p == '\r' || *p == '\n' || *p == '\t' ) ++p;
		}
	}
	g_det_boxes_json[0] = '\0';
	rapid_copy(g_det_boxes_json, sizeof(g_det_boxes_json), "[");
	used = 1;
	for ( y = 0; y < map_h && box_count < RAPIDOCR_MAX_BOXES; ++y ) {
		for ( x = 0; x < map_w && box_count < RAPIDOCR_MAX_BOXES; ++x ) {
			int head = 0;
			int tail = 0;
			int min_x = x;
			int min_y = y;
			int max_x = x;
			int max_y = y;
			double sum = 0.0;
			int hits = 0;
			int x1;
			int x2;
			int y1;
			int y2;
			double score;
			size_t idx = (size_t)y * (size_t)map_w + (size_t)x;
			if ( visited[idx] || values[idx] < threshold ) continue;
			visited[idx] = 1;
			queue_x[tail] = x;
			queue_y[tail] = y;
			++tail;
			while ( head < tail ) {
				int cx = queue_x[head];
				int cy = queue_y[head];
				double cv = values[(size_t)cy * (size_t)map_w + (size_t)cx];
				int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
				int d;
				++head;
				if ( cx < min_x ) min_x = cx;
				if ( cx > max_x ) max_x = cx;
				if ( cy < min_y ) min_y = cy;
				if ( cy > max_y ) max_y = cy;
				sum += cv;
				++hits;
				for ( d = 0; d < 4; ++d ) {
					int nx = cx + dirs[d][0];
					int ny = cy + dirs[d][1];
					size_t nidx;
					if ( nx < 0 || nx >= map_w || ny < 0 || ny >= map_h ) continue;
					nidx = (size_t)ny * (size_t)map_w + (size_t)nx;
					if ( visited[nidx] || values[nidx] < threshold ) continue;
					visited[nidx] = 1;
					queue_x[tail] = nx;
					queue_y[tail] = ny;
					++tail;
				}
			}
			if ( hits < 4 ) continue;
			x1 = (int)((double)min_x * (double)image_width / (double)map_w);
			x2 = (int)(((double)max_x + 1.0) * (double)image_width / (double)map_w);
			y1 = (int)((double)min_y * (double)image_height / (double)map_h);
			y2 = (int)(((double)max_y + 1.0) * (double)image_height / (double)map_h);
			score = sum / (double)hits;
			{
				int bw = x2 - x1;
				int bh = y2 - y1;
				int xpad = bw > 250 ? 16 : (bw < 160 ? bw / 5 : bw / 10);
				int ypad = bw > 150 ? (bh * 4) / 5 : bh / 2;
				if ( bh > bw * 2 && bw < 120 ) {
					xpad = bw / 2;
					ypad = bh / 4;
					if ( xpad < 12 ) xpad = 12;
					if ( ypad < 8 ) ypad = 8;
				}
				if ( bw >= 80 && bw < 160 && bh < 60 ) {
					int medium_xpad = bw / 6;
					int medium_ypad = (bh * 3) / 4;
					if ( medium_xpad < 12 ) medium_xpad = 12;
					if ( medium_ypad < 10 ) medium_ypad = 10;
					if ( xpad > medium_xpad ) xpad = medium_xpad;
					if ( ypad < medium_ypad ) ypad = medium_ypad;
				}
				if ( bw < 80 && bh < 90 ) {
					if ( xpad < 8 ) xpad = 8;
					if ( ypad < 8 ) ypad = 8;
				}
				if ( bw < 80 && bh >= 20 && bh < 90 && bw * 3 <= bh * 5 ) {
					int small_xpad = bw / 2;
					if ( small_xpad < 8 ) small_xpad = 8;
					if ( xpad < small_xpad ) xpad = small_xpad;
				}
				if ( xpad < 1 ) xpad = 1;
				if ( ypad < 2 ) ypad = 2;
				x1 -= xpad;
				y1 -= ypad;
				x2 += xpad;
				y2 += ypad;
			}
			if ( x1 < 0 ) x1 = 0;
			if ( y1 < 0 ) y1 = 0;
			if ( x2 > image_width ) x2 = image_width;
			if ( y2 > image_height ) y2 = image_height;
			{
				int suppress = 0;
				int k;
				for ( k = 0; k < box_count; ++k ) {
					int ix1 = x1 > kept_x1[k] ? x1 : kept_x1[k];
					int iy1 = y1 > kept_y1[k] ? y1 : kept_y1[k];
					int ix2 = x2 < kept_x2[k] ? x2 : kept_x2[k];
					int iy2 = y2 < kept_y2[k] ? y2 : kept_y2[k];
					int iw = ix2 - ix1;
					int ih = iy2 - iy1;
					if ( iw > 0 && ih > 0 ) {
						double inter = (double)iw * (double)ih;
						double area_a = (double)(x2 - x1) * (double)(y2 - y1);
						double area_b = (double)(kept_x2[k] - kept_x1[k]) * (double)(kept_y2[k] - kept_y1[k]);
						double iou = inter / (area_a + area_b - inter);
						if ( iou > 0.3 ) {
							suppress = 1;
							break;
						}
					}
				}
				if ( suppress ) continue;
			}
			if ( box_count > 0 ) rapid_append(g_det_boxes_json, sizeof(g_det_boxes_json), &used, ",");
			snprintf(g_det_boxes_json + used, sizeof(g_det_boxes_json) - used, "{\"box\":[[%d,%d],[%d,%d],[%d,%d],[%d,%d]],\"score\":%.6g,\"pixels\":%d}", x1, y1, x2, y1, x2, y2, x1, y2, score, hits);
			used += strlen(g_det_boxes_json + used);
			kept_x1[box_count] = x1;
			kept_y1[box_count] = y1;
			kept_x2[box_count] = x2;
			kept_y2[box_count] = y2;
			++box_count;
		}
	}
	if ( box_count == 0 ) {
		free(values);
		free(visited);
		free(queue_x);
		free(queue_y);
		rapid_set_error("");
		return "[]";
	}
	rapid_append(g_det_boxes_json, sizeof(g_det_boxes_json), &used, "]");
	free(values);
	free(visited);
	free(queue_x);
	free(queue_y);
	rapid_set_error("");
	return g_det_boxes_json;
}

XL_EXPORT const char* rapidocr_det_boxes_from_result_native(void* result_handle, int image_width, int image_height, double threshold)
{
	int map_h = 0;
	int map_w = 0;
	int x;
	int y;
	double* values;
	unsigned char* visited;
	int* queue_x;
	int* queue_y;
	int box_count = 0;
	int kept_x1[RAPIDOCR_MAX_BOXES];
	int kept_y1[RAPIDOCR_MAX_BOXES];
	int kept_x2[RAPIDOCR_MAX_BOXES];
	int kept_y2[RAPIDOCR_MAX_BOXES];
	double kept_score[RAPIDOCR_MAX_BOXES];
	int kept_pixels[RAPIDOCR_MAX_BOXES];
	int kept_active[RAPIDOCR_MAX_BOXES];
	size_t used = 0;
	int data_count;
	if ( result_handle == NULL || image_width <= 0 || image_height <= 0 ) {
		rapid_set_error("invalid detector result or image size");
		return "[]";
	}
	if ( threshold <= 0.0 ) threshold = 0.3;
	if ( !rapid_ort_load() ) return "[]";
	if ( !rapid_parse_shape_tail(g_ort.shape_json(result_handle), &map_h, &map_w) ) {
		rapid_set_error("detector typed output shape not found");
		return "[]";
	}
	data_count = g_ort.float_count(result_handle);
	if ( data_count < map_w * map_h ) {
		rapid_set_error("detector typed output data is incomplete");
		return "[]";
	}
	values = (double*)calloc((size_t)map_w * (size_t)map_h, sizeof(double));
	visited = (unsigned char*)calloc((size_t)map_w * (size_t)map_h, sizeof(unsigned char));
	queue_x = (int*)malloc((size_t)map_w * (size_t)map_h * sizeof(int));
	queue_y = (int*)malloc((size_t)map_w * (size_t)map_h * sizeof(int));
	if ( values == NULL || visited == NULL || queue_x == NULL || queue_y == NULL ) {
		free(values);
		free(visited);
		free(queue_x);
		free(queue_y);
		rapid_set_error("out of memory");
		return "[]";
	}
	for ( y = 0; y < map_h; ++y ) {
		for ( x = 0; x < map_w; ++x ) {
			values[(size_t)y * (size_t)map_w + (size_t)x] = (double)g_ort.float_at(result_handle, y * map_w + x);
		}
	}
	g_det_boxes_json[0] = '\0';
	rapid_copy(g_det_boxes_json, sizeof(g_det_boxes_json), "[");
	used = 1;
	for ( y = 0; y < map_h && box_count < RAPIDOCR_MAX_BOXES; ++y ) {
		for ( x = 0; x < map_w && box_count < RAPIDOCR_MAX_BOXES; ++x ) {
			int head = 0;
			int tail = 0;
			int min_x = x;
			int min_y = y;
			int max_x = x;
			int max_y = y;
			double sum = 0.0;
			int hits = 0;
			int x1;
			int x2;
			int y1;
			int y2;
			double score;
			size_t idx = (size_t)y * (size_t)map_w + (size_t)x;
			if ( visited[idx] || values[idx] < threshold ) continue;
			visited[idx] = 1;
			queue_x[tail] = x;
			queue_y[tail] = y;
			++tail;
			while ( head < tail ) {
				int cx = queue_x[head];
				int cy = queue_y[head];
				double cv = values[(size_t)cy * (size_t)map_w + (size_t)cx];
				int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
				int d;
				++head;
				if ( cx < min_x ) min_x = cx;
				if ( cx > max_x ) max_x = cx;
				if ( cy < min_y ) min_y = cy;
				if ( cy > max_y ) max_y = cy;
				sum += cv;
				++hits;
				for ( d = 0; d < 4; ++d ) {
					int nx = cx + dirs[d][0];
					int ny = cy + dirs[d][1];
					size_t nidx;
					if ( nx < 0 || nx >= map_w || ny < 0 || ny >= map_h ) continue;
					nidx = (size_t)ny * (size_t)map_w + (size_t)nx;
					if ( visited[nidx] || values[nidx] < threshold ) continue;
					visited[nidx] = 1;
					queue_x[tail] = nx;
					queue_y[tail] = ny;
					++tail;
				}
			}
			if ( hits < 4 ) continue;
			{
				int part_count = 1;
				int part_min_x[2] = { min_x, 0 };
				int part_min_y[2] = { min_y, 0 };
				int part_max_x[2] = { max_x, 0 };
				int part_max_y[2] = { max_y, 0 };
				double part_score[2] = { sum / (double)hits, 0.0 };
				int part_hits[2] = { hits, 0 };
				int left_x2;
				int right_x1;
				int part_index;
				if ( rapid_component_vertical_split(values, map_w, min_x, min_y, max_x, max_y, threshold, &left_x2, &right_x1) ) {
					int lmin_x, lmin_y, lmax_x, lmax_y, lhits;
					int rmin_x, rmin_y, rmax_x, rmax_y, rhits;
					double lscore;
					double rscore;
					if ( rapid_component_range_bounds(values, map_w, min_x, min_y, max_x, max_y, min_x, left_x2, threshold, &lmin_x, &lmin_y, &lmax_x, &lmax_y, &lscore, &lhits) &&
						rapid_component_range_bounds(values, map_w, min_x, min_y, max_x, max_y, right_x1, max_x, threshold, &rmin_x, &rmin_y, &rmax_x, &rmax_y, &rscore, &rhits) ) {
						part_count = 2;
						part_min_x[0] = lmin_x;
						part_min_y[0] = lmin_y;
						part_max_x[0] = lmax_x;
						part_max_y[0] = lmax_y;
						part_score[0] = lscore;
						part_hits[0] = lhits;
						part_min_x[1] = rmin_x;
						part_min_y[1] = rmin_y;
						part_max_x[1] = rmax_x;
						part_max_y[1] = rmax_y;
						part_score[1] = rscore;
						part_hits[1] = rhits;
					}
				}
				for ( part_index = 0; part_index < part_count && box_count < RAPIDOCR_MAX_BOXES; ++part_index ) {
					x1 = (int)((double)part_min_x[part_index] * (double)image_width / (double)map_w);
					x2 = (int)(((double)part_max_x[part_index] + 1.0) * (double)image_width / (double)map_w);
					y1 = (int)((double)part_min_y[part_index] * (double)image_height / (double)map_h);
					y2 = (int)(((double)part_max_y[part_index] + 1.0) * (double)image_height / (double)map_h);
					score = part_score[part_index];
					{
						int bw = x2 - x1;
						int bh = y2 - y1;
						int xpad = bw > 250 ? 16 : (bw < 160 ? bw / 5 : bw / 10);
						int ypad = bw > 150 ? (bh * 4) / 5 : bh / 2;
						if ( bh > bw * 2 && bw < 120 ) {
							xpad = bw / 2;
							ypad = bh / 4;
							if ( xpad < 12 ) xpad = 12;
							if ( ypad < 8 ) ypad = 8;
						}
						if ( bw >= 80 && bw < 160 && bh < 60 ) {
							int medium_xpad = bw / 6;
							int medium_ypad = (bh * 3) / 4;
							if ( medium_xpad < 12 ) medium_xpad = 12;
							if ( medium_ypad < 10 ) medium_ypad = 10;
							if ( xpad > medium_xpad ) xpad = medium_xpad;
							if ( ypad < medium_ypad ) ypad = medium_ypad;
						}
						if ( bw < 80 && bh < 90 ) {
							if ( xpad < 8 ) xpad = 8;
							if ( ypad < 8 ) ypad = 8;
						}
						if ( bw < 80 && bh >= 20 && bh < 90 && bw * 3 <= bh * 5 ) {
							int small_xpad = bw / 2;
							if ( small_xpad < 8 ) small_xpad = 8;
							if ( xpad < small_xpad ) xpad = small_xpad;
						}
						if ( xpad < 1 ) xpad = 1;
						if ( ypad < 2 ) ypad = 2;
						x1 -= xpad;
						y1 -= ypad;
						x2 += xpad;
						y2 += ypad;
					}
					if ( x1 < 0 ) x1 = 0;
					if ( y1 < 0 ) y1 = 0;
					if ( x2 > image_width ) x2 = image_width;
					if ( y2 > image_height ) y2 = image_height;
					{
						int suppress = 0;
						int k;
						for ( k = 0; k < box_count; ++k ) {
							int ix1 = x1 > kept_x1[k] ? x1 : kept_x1[k];
							int iy1 = y1 > kept_y1[k] ? y1 : kept_y1[k];
							int ix2 = x2 < kept_x2[k] ? x2 : kept_x2[k];
							int iy2 = y2 < kept_y2[k] ? y2 : kept_y2[k];
							int iw = ix2 - ix1;
							int ih = iy2 - iy1;
							if ( iw > 0 && ih > 0 ) {
								double inter = (double)iw * (double)ih;
								double area_a = (double)(x2 - x1) * (double)(y2 - y1);
								double area_b = (double)(kept_x2[k] - kept_x1[k]) * (double)(kept_y2[k] - kept_y1[k]);
								double iou = inter / (area_a + area_b - inter);
								if ( iou > 0.3 ) {
									suppress = 1;
									break;
								}
							}
						}
						if ( suppress ) continue;
					}
					kept_x1[box_count] = x1;
					kept_y1[box_count] = y1;
					kept_x2[box_count] = x2;
					kept_y2[box_count] = y2;
					kept_score[box_count] = score;
					kept_pixels[box_count] = part_hits[part_index];
					kept_active[box_count] = 1;
					++box_count;
				}
			}
		}
	}
	if ( box_count == 0 ) {
		free(values);
		free(visited);
		free(queue_x);
		free(queue_y);
		rapid_set_error("");
		return "[]";
	}
	{
		int i;
		int j;
		int changed = 1;
		for ( i = 0; i < box_count; ++i ) {
			if ( !kept_active[i] ) continue;
			for ( j = 0; j < box_count; ++j ) {
				int area_i;
				int area_j;
				int cx_j;
				int cy_j;
				if ( i == j || !kept_active[j] ) continue;
				area_i = (kept_x2[i] - kept_x1[i]) * (kept_y2[i] - kept_y1[i]);
				area_j = (kept_x2[j] - kept_x1[j]) * (kept_y2[j] - kept_y1[j]);
				cx_j = (kept_x1[j] + kept_x2[j]) / 2;
				cy_j = (kept_y1[j] + kept_y2[j]) / 2;
				if ( area_i > 0 && area_j > 0 && area_j * 4 < area_i &&
					cx_j >= kept_x1[i] && cx_j <= kept_x2[i] && cy_j >= kept_y1[i] && cy_j <= kept_y2[i] ) {
					int wi = kept_x2[i] - kept_x1[i];
					int hi = kept_y2[i] - kept_y1[i];
					int wj = kept_x2[j] - kept_x1[j];
					int hj = kept_y2[j] - kept_y1[j];
					int i_tall = hi * 2 >= wi * 3;
					int j_wide = wj * 3 >= hj * 2;
					int j_near_edge = cx_j >= kept_x2[i] - wi / 3 || cy_j >= kept_y2[i] - hi / 4;
					if ( i_tall && j_wide && j_near_edge ) continue;
					kept_active[j] = 0;
				}
			}
		}
		while ( changed ) {
			changed = 0;
			for ( i = 0; i < box_count; ++i ) {
				if ( !kept_active[i] ) continue;
				for ( j = i + 1; j < box_count; ++j ) {
					int y_overlap;
					int min_h;
					int gap;
					int merge_gap;
					int ci_y;
					int cj_y;
					int wi;
					int hi;
					int wj;
					int hj;
					int i_tall;
					int j_tall;
					int i_wide;
					int j_wide;
					int i_square;
					int j_square;
					if ( !kept_active[j] ) continue;
					wi = kept_x2[i] - kept_x1[i];
					hi = kept_y2[i] - kept_y1[i];
					wj = kept_x2[j] - kept_x1[j];
					hj = kept_y2[j] - kept_y1[j];
					i_tall = hi * 2 >= wi * 3;
					j_tall = hj * 2 >= wj * 3;
					i_wide = wi * 2 >= hi * 3;
					j_wide = wj * 2 >= hj * 3;
					i_square = wi * 3 >= hi * 2 && hi * 3 >= wi * 2;
					j_square = wj * 3 >= hj * 2 && hj * 3 >= wj * 2;
					if ( (i_tall && (j_wide || j_square)) || (j_tall && (i_wide || i_square)) ) continue;
					y_overlap = (kept_y2[i] < kept_y2[j] ? kept_y2[i] : kept_y2[j]) - (kept_y1[i] > kept_y1[j] ? kept_y1[i] : kept_y1[j]);
					min_h = (kept_y2[i] - kept_y1[i]) < (kept_y2[j] - kept_y1[j]) ? (kept_y2[i] - kept_y1[i]) : (kept_y2[j] - kept_y1[j]);
					ci_y = (kept_y1[i] + kept_y2[i]) / 2;
					cj_y = (kept_y1[j] + kept_y2[j]) / 2;
					if ( kept_x2[i] < kept_x1[j] ) gap = kept_x1[j] - kept_x2[i];
					else if ( kept_x2[j] < kept_x1[i] ) gap = kept_x1[i] - kept_x2[j];
					else gap = 0;
					merge_gap = 4;
					if ( ((kept_y2[i] - kept_y1[i]) * 2 < (kept_y2[j] - kept_y1[j])) || ((kept_y2[j] - kept_y1[j]) * 2 < (kept_y2[i] - kept_y1[i])) ) merge_gap = min_h;
					if ( min_h > 0 && gap <= merge_gap && (y_overlap * 2 >= min_h || abs(ci_y - cj_y) <= 10) ) {
						if ( kept_x1[j] < kept_x1[i] ) kept_x1[i] = kept_x1[j];
						if ( kept_y1[j] < kept_y1[i] ) kept_y1[i] = kept_y1[j];
						if ( kept_x2[j] > kept_x2[i] ) kept_x2[i] = kept_x2[j];
						if ( kept_y2[j] > kept_y2[i] ) kept_y2[i] = kept_y2[j];
						if ( kept_score[j] > kept_score[i] ) kept_score[i] = kept_score[j];
						kept_pixels[i] += kept_pixels[j];
						kept_active[j] = 0;
						changed = 1;
					}
				}
			}
		}
		j = 0;
		for ( i = 0; i < box_count; ++i ) {
			if ( !kept_active[i] ) continue;
			if ( i != j ) {
				kept_x1[j] = kept_x1[i];
				kept_y1[j] = kept_y1[i];
				kept_x2[j] = kept_x2[i];
				kept_y2[j] = kept_y2[i];
				kept_score[j] = kept_score[i];
				kept_pixels[j] = kept_pixels[i];
			}
			++j;
		}
		box_count = j;
		for ( i = 0; i < box_count; ++i ) {
			for ( j = i + 1; j < box_count; ++j ) {
				int swap = 0;
				if ( kept_y1[j] < kept_y1[i] - 10 ) swap = 1;
				else if ( abs(kept_y1[j] - kept_y1[i]) < 10 && kept_x1[j] < kept_x1[i] ) swap = 1;
				if ( swap ) {
					int tx1 = kept_x1[i], ty1 = kept_y1[i], tx2 = kept_x2[i], ty2 = kept_y2[i], tp = kept_pixels[i];
					double ts = kept_score[i];
					kept_x1[i] = kept_x1[j]; kept_y1[i] = kept_y1[j]; kept_x2[i] = kept_x2[j]; kept_y2[i] = kept_y2[j]; kept_score[i] = kept_score[j]; kept_pixels[i] = kept_pixels[j];
					kept_x1[j] = tx1; kept_y1[j] = ty1; kept_x2[j] = tx2; kept_y2[j] = ty2; kept_score[j] = ts; kept_pixels[j] = tp;
				}
			}
		}
	}
	if ( box_count == 0 ) {
		free(values);
		free(visited);
		free(queue_x);
		free(queue_y);
		rapid_set_error("");
		return "[]";
	}
	rapid_copy(g_det_boxes_json, sizeof(g_det_boxes_json), "[");
	used = 1;
	{
		int i;
		for ( i = 0; i < box_count; ++i ) {
			if ( i > 0 ) rapid_append(g_det_boxes_json, sizeof(g_det_boxes_json), &used, ",");
			snprintf(g_det_boxes_json + used, sizeof(g_det_boxes_json) - used, "{\"box\":[[%d,%d],[%d,%d],[%d,%d],[%d,%d]],\"score\":%.6g,\"pixels\":%d}", kept_x1[i], kept_y1[i], kept_x2[i], kept_y1[i], kept_x2[i], kept_y2[i], kept_x1[i], kept_y2[i], kept_score[i], kept_pixels[i]);
			used += strlen(g_det_boxes_json + used);
		}
	}
	rapid_append(g_det_boxes_json, sizeof(g_det_boxes_json), &used, "]");
	free(values);
	free(visited);
	free(queue_x);
	free(queue_y);
	rapid_set_error("");
	return g_det_boxes_json;
}

typedef struct RapidDbBox {
	int pts[8];
	double score;
	int pixels;
	int min_x;
	int min_y;
} RapidDbBox;

typedef struct RapidPoint {
	double x;
	double y;
} RapidPoint;

static double rapid_db_distance(double x0, double y0, double x1, double y1)
{
	double dx = x0 - x1;
	double dy = y0 - y1;
	return sqrt(dx * dx + dy * dy);
}

static double rapid_db_clamp(double value, double low, double high)
{
	if ( value < low ) return low;
	if ( value > high ) return high;
	return value;
}

static int rapid_point_compare(const void* a, const void* b)
{
	const RapidPoint* pa = (const RapidPoint*)a;
	const RapidPoint* pb = (const RapidPoint*)b;
	if ( pa->x < pb->x ) return -1;
	if ( pa->x > pb->x ) return 1;
	if ( pa->y < pb->y ) return -1;
	if ( pa->y > pb->y ) return 1;
	return 0;
}

static double rapid_point_cross(RapidPoint o, RapidPoint a, RapidPoint b)
{
	return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

static int rapid_build_convex_hull(RapidPoint* points, int count, RapidPoint* hull)
{
	int i;
	int unique_count = 0;
	int k = 0;
	if ( points == NULL || hull == NULL || count <= 0 ) return 0;
	qsort(points, (size_t)count, sizeof(RapidPoint), rapid_point_compare);
	for ( i = 0; i < count; ++i ) {
		if ( unique_count > 0 && fabs(points[i].x - points[unique_count - 1].x) < 1.0e-9 && fabs(points[i].y - points[unique_count - 1].y) < 1.0e-9 ) continue;
		points[unique_count++] = points[i];
	}
	if ( unique_count <= 1 ) {
		if ( unique_count == 1 ) hull[0] = points[0];
		return unique_count;
	}
	for ( i = 0; i < unique_count; ++i ) {
		while ( k >= 2 && rapid_point_cross(hull[k - 2], hull[k - 1], points[i]) <= 0.0 ) --k;
		hull[k++] = points[i];
	}
	{
		int lower_limit = k + 1;
		for ( i = unique_count - 2; i >= 0; --i ) {
			while ( k >= lower_limit && rapid_point_cross(hull[k - 2], hull[k - 1], points[i]) <= 0.0 ) --k;
			hull[k++] = points[i];
		}
	}
	if ( k > 1 ) --k;
	return k;
}

static void rapid_db_order_points(double pts[8])
{
	double ordered[8];
	int i;
	int tl = 0;
	int tr = 0;
	int br = 0;
	int bl = 0;
	double min_sum = 1.0e300;
	double max_sum = -1.0e300;
	double min_diff = 1.0e300;
	double max_diff = -1.0e300;
	for ( i = 0; i < 4; ++i ) {
		double x = pts[i * 2];
		double y = pts[i * 2 + 1];
		double sum = x + y;
		double diff = y - x;
		if ( sum < min_sum ) {
			min_sum = sum;
			tl = i;
		}
		if ( sum > max_sum ) {
			max_sum = sum;
			br = i;
		}
		if ( diff < min_diff ) {
			min_diff = diff;
			tr = i;
		}
		if ( diff > max_diff ) {
			max_diff = diff;
			bl = i;
		}
	}
	ordered[0] = pts[tl * 2];
	ordered[1] = pts[tl * 2 + 1];
	ordered[2] = pts[tr * 2];
	ordered[3] = pts[tr * 2 + 1];
	ordered[4] = pts[br * 2];
	ordered[5] = pts[br * 2 + 1];
	ordered[6] = pts[bl * 2];
	ordered[7] = pts[bl * 2 + 1];
	memcpy(pts, ordered, sizeof(ordered));
}

static double rapid_db_rect_score(const double* values, int map_w, int map_h, double ux, double uy, double vx, double vy, double min_u, double max_u, double min_v, double max_v)
{
	double corners[8];
	double min_x;
	double max_x;
	double min_y;
	double max_y;
	int x1;
	int x2;
	int y1;
	int y2;
	int x;
	int y;
	double sum = 0.0;
	int count = 0;
	corners[0] = min_u * ux + min_v * vx;
	corners[1] = min_u * uy + min_v * vy;
	corners[2] = max_u * ux + min_v * vx;
	corners[3] = max_u * uy + min_v * vy;
	corners[4] = max_u * ux + max_v * vx;
	corners[5] = max_u * uy + max_v * vy;
	corners[6] = min_u * ux + max_v * vx;
	corners[7] = min_u * uy + max_v * vy;
	min_x = max_x = corners[0];
	min_y = max_y = corners[1];
	for ( x = 1; x < 4; ++x ) {
		if ( corners[x * 2] < min_x ) min_x = corners[x * 2];
		if ( corners[x * 2] > max_x ) max_x = corners[x * 2];
		if ( corners[x * 2 + 1] < min_y ) min_y = corners[x * 2 + 1];
		if ( corners[x * 2 + 1] > max_y ) max_y = corners[x * 2 + 1];
	}
	x1 = (int)floor(min_x);
	x2 = (int)ceil(max_x);
	y1 = (int)floor(min_y);
	y2 = (int)ceil(max_y);
	if ( x1 < 0 ) x1 = 0;
	if ( y1 < 0 ) y1 = 0;
	if ( x2 >= map_w ) x2 = map_w - 1;
	if ( y2 >= map_h ) y2 = map_h - 1;
	for ( y = y1; y <= y2; ++y ) {
		for ( x = x1; x <= x2; ++x ) {
			double px = (double)x + 0.5;
			double py = (double)y + 0.5;
			double u = px * ux + py * uy;
			double v = px * vx + py * vy;
			if ( u >= min_u && u <= max_u && v >= min_v && v <= max_v ) {
				sum += values[(size_t)y * (size_t)map_w + (size_t)x];
				++count;
			}
		}
	}
	return count > 0 ? sum / (double)count : 0.0;
}

static int rapid_db_component_box(const double* values, int map_w, int map_h, const int* queue_x, const int* queue_y, int count, int image_width, int image_height, double box_thresh, double unclip_ratio, int min_box_size, int filter_small_boxes, RapidDbBox* out)
{
	int i;
	RapidPoint* points;
	RapidPoint* hull;
	int point_count;
	int hull_count;
	double best_area = 1.0e300;
	double best_ux = 1.0;
	double best_uy = 0.0;
	double best_vx = 0.0;
	double best_vy = 1.0;
	double best_min_u = 0.0;
	double best_max_u = 0.0;
	double best_min_v = 0.0;
	double best_max_v = 0.0;
	double ux = 1.0;
	double uy = 0.0;
	double vx = 0.0;
	double vy = 1.0;
	double min_u = 1.0e300;
	double max_u = -1.0e300;
	double min_v = 1.0e300;
	double max_v = -1.0e300;
	double width;
	double height;
	double score;
	double distance;
	double pts[8];
	int min_x = image_width;
	int min_y = image_height;
	if ( values == NULL || queue_x == NULL || queue_y == NULL || out == NULL || count <= 0 ) return 0;
	if ( box_thresh <= 0.0 ) box_thresh = 0.5;
	if ( unclip_ratio <= 0.0 ) unclip_ratio = 1.6;
	if ( min_box_size <= 0 ) min_box_size = 3;
	point_count = count * 4;
	points = (RapidPoint*)malloc((size_t)point_count * sizeof(RapidPoint));
	hull = (RapidPoint*)malloc((size_t)point_count * sizeof(RapidPoint));
	if ( points == NULL || hull == NULL ) {
		free(points);
		free(hull);
		return 0;
	}
	for ( i = 0; i < count; ++i ) {
		double x = (double)queue_x[i];
		double y = (double)queue_y[i];
		points[i * 4 + 0].x = x;
		points[i * 4 + 0].y = y;
		points[i * 4 + 1].x = x + 1.0;
		points[i * 4 + 1].y = y;
		points[i * 4 + 2].x = x + 1.0;
		points[i * 4 + 2].y = y + 1.0;
		points[i * 4 + 3].x = x;
		points[i * 4 + 3].y = y + 1.0;
	}
	hull_count = rapid_build_convex_hull(points, point_count, hull);
	if ( hull_count < 2 ) {
		free(points);
		free(hull);
		return 0;
	}
	for ( i = 0; i < hull_count; ++i ) {
		int next = (i + 1) % hull_count;
		double ex = hull[next].x - hull[i].x;
		double ey = hull[next].y - hull[i].y;
		double len = sqrt(ex * ex + ey * ey);
		double area;
		int j;
		if ( len <= 1.0e-9 ) continue;
		ux = ex / len;
		uy = ey / len;
		vx = -uy;
		vy = ux;
		min_u = 1.0e300;
		max_u = -1.0e300;
		min_v = 1.0e300;
		max_v = -1.0e300;
		for ( j = 0; j < hull_count; ++j ) {
			double u = hull[j].x * ux + hull[j].y * uy;
			double v = hull[j].x * vx + hull[j].y * vy;
			if ( u < min_u ) min_u = u;
			if ( u > max_u ) max_u = u;
			if ( v < min_v ) min_v = v;
			if ( v > max_v ) max_v = v;
		}
		width = max_u - min_u;
		height = max_v - min_v;
		area = width * height;
		if ( area < best_area ) {
			best_area = area;
			best_ux = ux;
			best_uy = uy;
			best_vx = vx;
			best_vy = vy;
			best_min_u = min_u;
			best_max_u = max_u;
			best_min_v = min_v;
			best_max_v = max_v;
		}
	}
	free(points);
	free(hull);
	if ( best_area >= 1.0e299 ) return 0;
	ux = best_ux;
	uy = best_uy;
	vx = best_vx;
	vy = best_vy;
	min_u = best_min_u;
	max_u = best_max_u;
	min_v = best_min_v;
	max_v = best_max_v;
	width = max_u - min_u;
	height = max_v - min_v;
	if ( filter_small_boxes && (width < (double)min_box_size || height < (double)min_box_size) ) return 0;
	score = rapid_db_rect_score(values, map_w, map_h, ux, uy, vx, vy, min_u, max_u, min_v, max_v);
	if ( score < box_thresh ) return 0;
	if ( width <= 0.0 || height <= 0.0 ) return 0;
	distance = width * height * unclip_ratio / (2.0 * (width + height));
	min_u -= distance;
	max_u += distance;
	min_v -= distance;
	max_v += distance;
	width = max_u - min_u;
	height = max_v - min_v;
	if ( filter_small_boxes && (width < (double)(min_box_size + 2) || height < (double)(min_box_size + 2)) ) return 0;
	pts[0] = min_u * ux + min_v * vx;
	pts[1] = min_u * uy + min_v * vy;
	pts[2] = max_u * ux + min_v * vx;
	pts[3] = max_u * uy + min_v * vy;
	pts[4] = max_u * ux + max_v * vx;
	pts[5] = max_u * uy + max_v * vy;
	pts[6] = min_u * ux + max_v * vx;
	pts[7] = min_u * uy + max_v * vy;
	rapid_db_order_points(pts);
	for ( i = 0; i < 4; ++i ) {
		double sx = rapid_db_clamp(pts[i * 2] * (double)image_width / (double)map_w, 0.0, (double)image_width);
		double sy = rapid_db_clamp(pts[i * 2 + 1] * (double)image_height / (double)map_h, 0.0, (double)image_height);
		int ix = (int)(sx + 0.5);
		int iy = (int)(sy + 0.5);
		out->pts[i * 2] = ix;
		out->pts[i * 2 + 1] = iy;
		if ( ix < min_x ) min_x = ix;
		if ( iy < min_y ) min_y = iy;
	}
	if ( rapid_db_distance((double)out->pts[0], (double)out->pts[1], (double)out->pts[2], (double)out->pts[3]) <= 1.0 &&
		rapid_db_distance((double)out->pts[2], (double)out->pts[3], (double)out->pts[4], (double)out->pts[5]) <= 1.0 ) return 0;
	out->score = score;
	out->pixels = count;
	out->min_x = min_x;
	out->min_y = min_y;
	return 1;
}

static void rapid_db_sort_boxes(RapidDbBox* boxes, int count)
{
	int i;
	int j;
	for ( i = 0; i < count; ++i ) {
		for ( j = i + 1; j < count; ++j ) {
			int swap = 0;
			if ( boxes[j].min_y < boxes[i].min_y - 10 ) swap = 1;
			else if ( abs(boxes[j].min_y - boxes[i].min_y) < 10 && boxes[j].min_x < boxes[i].min_x ) swap = 1;
			if ( swap ) {
				RapidDbBox tmp = boxes[i];
				boxes[i] = boxes[j];
				boxes[j] = tmp;
			}
		}
	}
}

XL_EXPORT const char* rapidocr_det_boxes_from_result_db_native(void* result_handle, int image_width, int image_height, double threshold, double box_thresh, double unclip_ratio, int use_dilation, int min_box_size, int filter_small_boxes)
{
	int map_h = 0;
	int map_w = 0;
	int data_count;
	double* values;
	unsigned char* raw;
	unsigned char* bitmap;
	unsigned char* visited;
	int* queue_x;
	int* queue_y;
	RapidDbBox boxes[RAPIDOCR_MAX_BOXES];
	int box_count = 0;
	int x;
	int y;
	size_t used = 0;
	if ( result_handle == NULL || image_width <= 0 || image_height <= 0 ) {
		rapid_set_error("invalid detector result or image size");
		return "[]";
	}
	if ( threshold <= 0.0 ) threshold = 0.3;
	if ( box_thresh <= 0.0 ) box_thresh = 0.5;
	if ( unclip_ratio <= 0.0 ) unclip_ratio = 1.6;
	if ( min_box_size <= 0 ) min_box_size = 3;
	if ( !rapid_ort_load() ) return "[]";
	if ( !rapid_parse_shape_tail(g_ort.shape_json(result_handle), &map_h, &map_w) ) {
		rapid_set_error("detector typed output shape not found");
		return "[]";
	}
	data_count = g_ort.float_count(result_handle);
	if ( data_count < map_w * map_h ) {
		rapid_set_error("detector typed output data is incomplete");
		return "[]";
	}
	values = (double*)calloc((size_t)map_w * (size_t)map_h, sizeof(double));
	raw = (unsigned char*)calloc((size_t)map_w * (size_t)map_h, sizeof(unsigned char));
	bitmap = (unsigned char*)calloc((size_t)map_w * (size_t)map_h, sizeof(unsigned char));
	visited = (unsigned char*)calloc((size_t)map_w * (size_t)map_h, sizeof(unsigned char));
	queue_x = (int*)malloc((size_t)map_w * (size_t)map_h * sizeof(int));
	queue_y = (int*)malloc((size_t)map_w * (size_t)map_h * sizeof(int));
	if ( values == NULL || raw == NULL || bitmap == NULL || visited == NULL || queue_x == NULL || queue_y == NULL ) {
		free(values);
		free(raw);
		free(bitmap);
		free(visited);
		free(queue_x);
		free(queue_y);
		rapid_set_error("out of memory");
		return "[]";
	}
	for ( y = 0; y < map_h; ++y ) {
		for ( x = 0; x < map_w; ++x ) {
			size_t idx = (size_t)y * (size_t)map_w + (size_t)x;
			values[idx] = (double)g_ort.float_at(result_handle, y * map_w + x);
			raw[idx] = values[idx] > threshold ? 1u : 0u;
		}
	}
	if ( use_dilation ) {
		for ( y = 0; y < map_h; ++y ) {
			for ( x = 0; x < map_w; ++x ) {
				int hit = raw[(size_t)y * (size_t)map_w + (size_t)x] != 0u;
				if ( !hit && x > 0 ) hit = raw[(size_t)y * (size_t)map_w + (size_t)(x - 1)] != 0u;
				if ( !hit && y > 0 ) hit = raw[(size_t)(y - 1) * (size_t)map_w + (size_t)x] != 0u;
				if ( !hit && x > 0 && y > 0 ) hit = raw[(size_t)(y - 1) * (size_t)map_w + (size_t)(x - 1)] != 0u;
				bitmap[(size_t)y * (size_t)map_w + (size_t)x] = hit ? 1u : 0u;
			}
		}
	} else {
		memcpy(bitmap, raw, (size_t)map_w * (size_t)map_h);
	}
	for ( y = 0; y < map_h && box_count < RAPIDOCR_MAX_BOXES; ++y ) {
		for ( x = 0; x < map_w && box_count < RAPIDOCR_MAX_BOXES; ++x ) {
			size_t idx = (size_t)y * (size_t)map_w + (size_t)x;
			int head = 0;
			int tail = 0;
			if ( visited[idx] || bitmap[idx] == 0u ) continue;
			visited[idx] = 1u;
			queue_x[tail] = x;
			queue_y[tail] = y;
			++tail;
			while ( head < tail ) {
				int cx = queue_x[head];
				int cy = queue_y[head];
				int dy;
				int dx;
				++head;
				for ( dy = -1; dy <= 1; ++dy ) {
					for ( dx = -1; dx <= 1; ++dx ) {
						int nx;
						int ny;
						size_t nidx;
						if ( dx == 0 && dy == 0 ) continue;
						nx = cx + dx;
						ny = cy + dy;
						if ( nx < 0 || nx >= map_w || ny < 0 || ny >= map_h ) continue;
						nidx = (size_t)ny * (size_t)map_w + (size_t)nx;
						if ( visited[nidx] || bitmap[nidx] == 0u ) continue;
						visited[nidx] = 1u;
						queue_x[tail] = nx;
						queue_y[tail] = ny;
						++tail;
					}
				}
			}
			if ( rapid_db_component_box(values, map_w, map_h, queue_x, queue_y, tail, image_width, image_height, box_thresh, unclip_ratio, min_box_size, filter_small_boxes, &boxes[box_count]) ) {
				++box_count;
			}
		}
	}
	if ( box_count == 0 ) {
		free(values);
		free(raw);
		free(bitmap);
		free(visited);
		free(queue_x);
		free(queue_y);
		rapid_set_error("");
		return "[]";
	}
	rapid_db_sort_boxes(boxes, box_count);
	rapid_copy(g_det_boxes_json, sizeof(g_det_boxes_json), "[");
	used = 1;
	for ( x = 0; x < box_count; ++x ) {
		char item[256];
		if ( x > 0 ) rapid_append(g_det_boxes_json, sizeof(g_det_boxes_json), &used, ",");
		snprintf(item, sizeof(item), "{\"box\":[[%d,%d],[%d,%d],[%d,%d],[%d,%d]],\"score\":%.6g,\"pixels\":%d}",
			boxes[x].pts[0], boxes[x].pts[1],
			boxes[x].pts[2], boxes[x].pts[3],
			boxes[x].pts[4], boxes[x].pts[5],
			boxes[x].pts[6], boxes[x].pts[7],
			boxes[x].score, boxes[x].pixels);
		rapid_append(g_det_boxes_json, sizeof(g_det_boxes_json), &used, item);
	}
	rapid_append(g_det_boxes_json, sizeof(g_det_boxes_json), &used, "]");
	free(values);
	free(raw);
	free(bitmap);
	free(visited);
	free(queue_x);
	free(queue_y);
	rapid_set_error("");
	return g_det_boxes_json;
}

XL_EXPORT const char* rapidocr_charset_info_native(const char* path)
{
	FILE* fp;
	char line[512];
	char first[128] = "";
	char last[128] = "";
	int count = 0;
	if ( path == NULL || path[0] == '\0' ) {
		rapid_set_error("empty charset path");
		return "{}";
	}
	fp = fopen(path, "rb");
	if ( fp == NULL ) {
		snprintf(g_error, sizeof(g_error), "failed to open charset: %s", path);
		return "{}";
	}
	while ( fgets(line, sizeof(line), fp) != NULL ) {
		size_t len = strlen(line);
		while ( len > 0u && (line[len - 1u] == '\n' || line[len - 1u] == '\r') ) line[--len] = '\0';
		if ( count == 0 ) rapid_copy(first, sizeof(first), line);
		rapid_copy(last, sizeof(last), line);
		++count;
	}
	fclose(fp);
	snprintf(g_charset_json, sizeof(g_charset_json), "{\"path\":\"%s\",\"count\":%d,\"first\":\"%s\",\"last\":\"%s\"}", path, count, first, last);
	rapid_set_error("");
	return g_charset_json;
}

static int rapid_load_charset(const char* path, char*** out_items, int* out_count)
{
	FILE* fp;
	char line[512];
	char** items = NULL;
	int count = 0;
	int cap = 0;
	if ( out_items == NULL || out_count == NULL ) return 0;
	*out_items = NULL;
	*out_count = 0;
	if ( path != NULL && g_charset_cache_items != NULL && strcmp(path, g_charset_cache_path) == 0 ) {
		*out_items = g_charset_cache_items;
		*out_count = g_charset_cache_count;
		return 1;
	}
	fp = fopen(path, "rb");
	if ( fp == NULL ) {
		snprintf(g_error, sizeof(g_error), "failed to open charset: %s", path != NULL ? path : "");
		return 0;
	}
	while ( fgets(line, sizeof(line), fp) != NULL ) {
		size_t len = strlen(line);
		char* item;
		while ( len > 0u && (line[len - 1u] == '\n' || line[len - 1u] == '\r') ) line[--len] = '\0';
		if ( count >= cap ) {
			int next_cap = cap == 0 ? 256 : cap * 2;
			char** next = (char**)realloc(items, (size_t)next_cap * sizeof(char*));
			if ( next == NULL ) {
				fclose(fp);
				rapid_set_error("out of memory");
				return 0;
			}
			items = next;
			cap = next_cap;
		}
		item = (char*)malloc(len + 1u);
		if ( item == NULL ) {
			fclose(fp);
			rapid_set_error("out of memory");
			return 0;
		}
		memcpy(item, line, len + 1u);
		items[count++] = item;
	}
	fclose(fp);
	if ( path != NULL && path[0] != '\0' ) {
		if ( g_charset_cache_items != NULL ) {
			int i;
			for ( i = 0; i < g_charset_cache_count; ++i ) free(g_charset_cache_items[i]);
			free(g_charset_cache_items);
		}
		g_charset_cache_items = items;
		g_charset_cache_count = count;
		rapid_copy(g_charset_cache_path, sizeof(g_charset_cache_path), path);
	}
	*out_items = items;
	*out_count = count;
	return 1;
}

static void rapid_free_charset(char** items, int count)
{
	int i;
	if ( items == NULL ) return;
	if ( items == g_charset_cache_items ) return;
	for ( i = 0; i < count; ++i ) free(items[i]);
	free(items);
}

XL_EXPORT const char* rapidocr_rec_decode_native(const char* output_json, const char* keys_path)
{
	int shape_h = 0;
	int classes = 0;
	int t;
	int c;
	const char* p;
	char** charset = NULL;
	int charset_count = 0;
	int last_idx = -1;
	int text_len = 0;
	double score_sum = 0.0;
	int score_count = 0;
	char* rec_json = rapid_next_rec_json();
	if ( output_json == NULL || output_json[0] == '\0' || keys_path == NULL || keys_path[0] == '\0' ) {
		rapid_set_error("invalid recognizer output or keys path");
		return "{}";
	}
	if ( !rapid_parse_shape_hw(output_json, &shape_h, &classes) || shape_h <= 0 || classes <= 0 ) {
		rapid_set_error("recognizer output shape not found");
		return "{}";
	}
	p = rapid_find_data_array(output_json);
	if ( p == NULL ) {
		rapid_set_error("recognizer output data not found");
		return "{}";
	}
	if ( !rapid_load_charset(keys_path, &charset, &charset_count) ) return "{}";
	strcpy(rec_json, "{\"text\":\"");
	text_len = (int)strlen(rec_json);
	for ( t = 0; t < shape_h; ++t ) {
		int best_idx = 0;
		double best_value = -1.0e300;
		for ( c = 0; c < classes; ++c ) {
			char* endp;
			while ( *p != '\0' && *p != ']' && *p != '-' && *p != '+' && *p != '.' && (*p < '0' || *p > '9') ) ++p;
			if ( *p == ']' || *p == '\0' ) goto rapid_rec_decode_done;
			double v = strtod(p, &endp);
			if ( endp == p ) {
				if ( *p == ']' ) goto rapid_rec_decode_done;
				rapid_free_charset(charset, charset_count);
				rapid_set_error("recognizer output data parse failed");
				return "{}";
			}
			if ( v > best_value ) {
				best_value = v;
				best_idx = c;
			}
			p = endp;
			while ( *p == ',' || *p == ' ' || *p == '\r' || *p == '\n' || *p == '\t' ) ++p;
		}
		if ( best_idx > 0 && best_idx != last_idx ) {
			int char_idx = best_idx - 1;
			if ( char_idx >= 0 && char_idx < charset_count ) {
				const char* s = charset[char_idx];
				size_t n = strlen(s);
				if ( text_len + (int)n + 64 < 4096 ) {
					memcpy(rec_json + text_len, s, n);
					text_len += (int)n;
					rec_json[text_len] = '\0';
				}
				score_sum += best_value;
				++score_count;
			}
		}
		last_idx = best_idx;
	}
rapid_rec_decode_done:
	snprintf(rec_json + text_len, 4096u - (size_t)text_len, "\",\"score\":%.6g,\"tokens\":%d}", score_count > 0 ? score_sum / (double)score_count : 0.0, score_count);
	rapid_free_charset(charset, charset_count);
	rapid_set_error("");
	return rec_json;
}

static int rapid_parse_shape_tail(const char* shape_json, int* out_time, int* out_classes)
{
	const char* p = shape_json;
	int values[8];
	int count = 0;
	if ( out_time == NULL || out_classes == NULL ) return 0;
	*out_time = 0;
	*out_classes = 0;
	if ( p == NULL ) return 0;
	while ( *p != '\0' && count < 8 ) {
		char* endp;
		while ( *p != '\0' && *p != '-' && (*p < '0' || *p > '9') ) ++p;
		if ( *p == '\0' ) break;
		values[count++] = (int)strtol(p, &endp, 10);
		if ( endp == p ) break;
		p = endp;
	}
	if ( count < 2 ) return 0;
	*out_time = values[count - 2];
	*out_classes = values[count - 1];
	return *out_time > 0 && *out_classes > 0;
}

static int rapid_parse_shape_dims(const char* shape_json, int* values, int max_values)
{
	const char* p = shape_json;
	int count = 0;
	if ( values == NULL || max_values <= 0 || p == NULL ) return 0;
	while ( *p != '\0' && count < max_values ) {
		char* endp;
		while ( *p != '\0' && *p != '-' && (*p < '0' || *p > '9') ) ++p;
		if ( *p == '\0' ) break;
		values[count++] = (int)strtol(p, &endp, 10);
		if ( endp == p ) break;
		p = endp;
	}
	return count;
}

XL_EXPORT const char* rapidocr_rec_decode_result_native(void* result_handle, const char* keys_path)
{
	int shape_h = 0;
	int classes = 0;
	int data_count;
	int t;
	int c;
	char** charset = NULL;
	int charset_count = 0;
	int last_idx = -1;
	int text_len = 0;
	double score_sum = 0.0;
	int score_count = 0;
	char* rec_json = rapid_next_rec_json();
	if ( result_handle == NULL || keys_path == NULL || keys_path[0] == '\0' ) {
		rapid_set_error("invalid recognizer result or keys path");
		return "{}";
	}
	if ( !rapid_ort_load() ) return "{}";
	if ( !rapid_parse_shape_tail(g_ort.shape_json(result_handle), &shape_h, &classes) ) {
		rapid_set_error("recognizer typed output shape not found");
		return "{}";
	}
	data_count = g_ort.float_count(result_handle);
	if ( data_count <= 0 || data_count < shape_h * classes ) {
		rapid_set_error("recognizer typed output data is incomplete");
		return "{}";
	}
	if ( !rapid_load_charset(keys_path, &charset, &charset_count) ) return "{}";
	strcpy(rec_json, "{\"text\":\"");
	text_len = (int)strlen(rec_json);
	for ( t = 0; t < shape_h; ++t ) {
		int best_idx = 0;
		double best_value = -1.0e300;
		for ( c = 0; c < classes; ++c ) {
			double v = (double)g_ort.float_at(result_handle, t * classes + c);
			if ( v > best_value ) {
				best_value = v;
				best_idx = c;
			}
		}
		if ( best_idx > 0 && best_idx != last_idx ) {
			int char_idx = best_idx - 1;
			if ( char_idx >= 0 && char_idx < charset_count ) {
				const char* s = charset[char_idx];
				size_t n = strlen(s);
				if ( text_len + (int)n + 64 < 4096 ) {
					memcpy(rec_json + text_len, s, n);
					text_len += (int)n;
					rec_json[text_len] = '\0';
				}
				score_sum += best_value;
				++score_count;
			}
		}
		last_idx = best_idx;
	}
	snprintf(rec_json + text_len, 4096u - (size_t)text_len, "\",\"score\":%.6g,\"tokens\":%d}", score_count > 0 ? score_sum / (double)score_count : 0.0, score_count);
	rapid_free_charset(charset, charset_count);
	rapid_set_error("");
	return rec_json;
}

XL_EXPORT const char* rapidocr_rec_decode_result_at_native(void* result_handle, const char* keys_path, int batch_index)
{
	int dims[8];
	int dim_count;
	int batch = 1;
	int shape_h = 0;
	int classes = 0;
	int data_count;
	int offset;
	int t;
	int c;
	char** charset = NULL;
	int charset_count = 0;
	int last_idx = -1;
	int text_len = 0;
	double score_sum = 0.0;
	int score_count = 0;
	char* rec_json = rapid_next_rec_json();
	if ( result_handle == NULL || keys_path == NULL || keys_path[0] == '\0' || batch_index < 0 ) {
		rapid_set_error("invalid recognizer result, keys path, or batch index");
		return "{}";
	}
	if ( !rapid_ort_load() ) return "{}";
	dim_count = rapid_parse_shape_dims(g_ort.shape_json(result_handle), dims, 8);
	if ( dim_count < 2 ) {
		rapid_set_error("recognizer typed output shape not found");
		return "{}";
	}
	shape_h = dims[dim_count - 2];
	classes = dims[dim_count - 1];
	if ( dim_count >= 3 ) batch = dims[dim_count - 3];
	if ( batch_index >= batch || shape_h <= 0 || classes <= 0 ) {
		rapid_set_error("recognizer batch index is out of range");
		return "{}";
	}
	data_count = g_ort.float_count(result_handle);
	offset = batch_index * shape_h * classes;
	if ( data_count <= 0 || data_count < offset + shape_h * classes ) {
		rapid_set_error("recognizer typed output data is incomplete");
		return "{}";
	}
	if ( !rapid_load_charset(keys_path, &charset, &charset_count) ) return "{}";
	strcpy(rec_json, "{\"text\":\"");
	text_len = (int)strlen(rec_json);
	for ( t = 0; t < shape_h; ++t ) {
		int best_idx = 0;
		double best_value = -1.0e300;
		for ( c = 0; c < classes; ++c ) {
			double v = (double)g_ort.float_at(result_handle, offset + t * classes + c);
			if ( v > best_value ) {
				best_value = v;
				best_idx = c;
			}
		}
		if ( best_idx > 0 && best_idx != last_idx ) {
			int char_idx = best_idx - 1;
			if ( char_idx >= 0 && char_idx < charset_count ) {
				const char* s = charset[char_idx];
				size_t n = strlen(s);
				if ( text_len + (int)n + 64 < 4096 ) {
					memcpy(rec_json + text_len, s, n);
					text_len += (int)n;
					rec_json[text_len] = '\0';
				}
				score_sum += best_value;
				++score_count;
			}
		}
		last_idx = best_idx;
	}
	snprintf(rec_json + text_len, 4096u - (size_t)text_len, "\",\"score\":%.6g,\"tokens\":%d}", score_count > 0 ? score_sum / (double)score_count : 0.0, score_count);
	rapid_free_charset(charset, charset_count);
	rapid_set_error("");
	return rec_json;
}

XL_EXPORT const char* rapidocr_cls_decode_result_native(void* result_handle)
{
	int classes = 0;
	int data_count;
	int i;
	int best_idx = 0;
	double best_value = -1.0e300;
	if ( result_handle == NULL ) {
		rapid_set_error("invalid classifier result");
		return "{}";
	}
	if ( !rapid_ort_load() ) return "{}";
	data_count = g_ort.float_count(result_handle);
	if ( data_count <= 0 ) {
		rapid_set_error("classifier typed output data is empty");
		return "{}";
	}
	classes = data_count;
	for ( i = 0; i < data_count; ++i ) {
		double v = (double)g_ort.float_at(result_handle, i);
		if ( v > best_value ) {
			best_value = v;
			best_idx = i;
		}
	}
	snprintf(g_cls_json, sizeof(g_cls_json), "{\"label\":\"%s\",\"index\":%d,\"score\":%.6g,\"classes\":%d}", best_idx == 1 ? "180" : "0", best_idx, best_value, classes);
	rapid_set_error("");
	return g_cls_json;
}

XL_EXPORT const char* rapidocr_cls_decode_result_at_native(void* result_handle, int batch_index)
{
	int dims[8];
	int dim_count;
	int classes;
	int batch = 1;
	int data_count;
	int offset;
	int i;
	int best_idx = 0;
	double best_value = -1.0e300;
	if ( result_handle == NULL || batch_index < 0 ) {
		rapid_set_error("invalid classifier result or batch index");
		return "{}";
	}
	if ( !rapid_ort_load() ) return "{}";
	data_count = g_ort.float_count(result_handle);
	if ( data_count <= 0 ) {
		rapid_set_error("classifier typed output data is empty");
		return "{}";
	}
	dim_count = rapid_parse_shape_dims(g_ort.shape_json(result_handle), dims, 8);
	if ( dim_count >= 2 ) {
		classes = dims[dim_count - 1];
		batch = dims[dim_count - 2];
	} else {
		classes = data_count;
		batch = 1;
	}
	if ( classes <= 0 || batch_index >= batch || data_count < (batch_index + 1) * classes ) {
		rapid_set_error("classifier batch index is out of range");
		return "{}";
	}
	offset = batch_index * classes;
	for ( i = 0; i < classes; ++i ) {
		double v = (double)g_ort.float_at(result_handle, offset + i);
		if ( v > best_value ) {
			best_value = v;
			best_idx = i;
		}
	}
	snprintf(g_cls_json, sizeof(g_cls_json), "{\"label\":\"%s\",\"index\":%d,\"score\":%.6g,\"classes\":%d}", best_idx == 1 ? "180" : "0", best_idx, best_value, classes);
	rapid_set_error("");
	return g_cls_json;
}

static int rapid_json_string_value(const char* json, const char* key, char* out, size_t out_size)
{
	const char* p;
	size_t key_len;
	if ( out == NULL || out_size == 0u ) return 0;
	out[0] = '\0';
	if ( json == NULL || key == NULL ) return 0;
	p = strstr(json, key);
	if ( p == NULL ) return 0;
	key_len = strlen(key);
	p += key_len;
	p = strchr(p, ':');
	if ( p == NULL ) return 0;
	p = strchr(p, '"');
	if ( p == NULL ) return 0;
	++p;
	{
		size_t n = 0;
		while ( p[n] != '\0' && p[n] != '"' && n + 1u < out_size ) {
			out[n] = p[n];
			++n;
		}
		out[n] = '\0';
		return 1;
	}
}

static double rapid_json_number_value(const char* json, const char* key, double fallback)
{
	const char* p;
	if ( json == NULL || key == NULL ) return fallback;
	p = strstr(json, key);
	if ( p == NULL ) return fallback;
	p = strchr(p + strlen(key), ':');
	if ( p == NULL ) return fallback;
	return strtod(p + 1, NULL);
}

XL_EXPORT int rapidocr_cls_should_rotate_native(const char* cls_json, double threshold)
{
	char label[32];
	double score;
	if ( threshold <= 0.0 ) threshold = 0.9;
	if ( !rapid_json_string_value(cls_json, "\"label\"", label, sizeof(label)) ) return 0;
	score = rapid_json_number_value(cls_json, "\"score\"", 0.0);
	return strcmp(label, "180") == 0 && score > threshold ? 1 : 0;
}

XL_EXPORT const char* rapidocr_assemble_result_native(const char* boxes_json, const char* rec_json, double elapse)
{
	char text[1024];
	double score;
	if ( boxes_json == NULL || boxes_json[0] == '\0' ) boxes_json = "[]";
	if ( rec_json == NULL || rec_json[0] == '\0' ) rec_json = "{}";
	if ( !rapid_json_string_value(rec_json, "\"text\"", text, sizeof(text)) ) text[0] = '\0';
	score = rapid_json_number_value(rec_json, "\"score\"", 0.0);
	snprintf(g_assembled_json, sizeof(g_assembled_json), "{\"boxes\":%s,\"txts\":[\"%s\"],\"scores\":[%.6g],\"word_results\":[],\"elapse_list\":[],\"elapse\":%.6g}", boxes_json, text, score, elapse);
	rapid_set_error("");
	return g_assembled_json;
}

XL_EXPORT const char* rapidocr_assemble_result2_native(const char* boxes_json, const char* rec0_json, const char* rec1_json, double elapse)
{
	char text0[1024];
	char text1[1024];
	double score0;
	double score1;
	if ( boxes_json == NULL || boxes_json[0] == '\0' ) boxes_json = "[]";
	if ( rec0_json == NULL || rec0_json[0] == '\0' ) rec0_json = "{}";
	if ( rec1_json == NULL || rec1_json[0] == '\0' ) rec1_json = "{}";
	if ( !rapid_json_string_value(rec0_json, "\"text\"", text0, sizeof(text0)) ) text0[0] = '\0';
	if ( !rapid_json_string_value(rec1_json, "\"text\"", text1, sizeof(text1)) ) text1[0] = '\0';
	score0 = rapid_json_number_value(rec0_json, "\"score\"", 0.0);
	score1 = rapid_json_number_value(rec1_json, "\"score\"", 0.0);
	snprintf(g_assembled_json, sizeof(g_assembled_json), "{\"boxes\":%s,\"txts\":[\"%s\",\"%s\"],\"scores\":[%.6g,%.6g],\"word_results\":[],\"elapse_list\":[],\"elapse\":%.6g}", boxes_json, text0, text1, score0, score1, elapse);
	rapid_set_error("");
	return g_assembled_json;
}

XL_EXPORT const char* rapidocr_assemble_result4_native(const char* boxes_json, const char* rec0_json, const char* rec1_json, const char* rec2_json, const char* rec3_json, double elapse)
{
	char text0[1024], text1[1024], text2[1024], text3[1024];
	double score0, score1, score2, score3;
	if ( boxes_json == NULL || boxes_json[0] == '\0' ) boxes_json = "[]";
	if ( rec0_json == NULL || rec0_json[0] == '\0' ) rec0_json = "{}";
	if ( rec1_json == NULL || rec1_json[0] == '\0' ) rec1_json = "{}";
	if ( rec2_json == NULL || rec2_json[0] == '\0' ) rec2_json = "{}";
	if ( rec3_json == NULL || rec3_json[0] == '\0' ) rec3_json = "{}";
	if ( !rapid_json_string_value(rec0_json, "\"text\"", text0, sizeof(text0)) ) text0[0] = '\0';
	if ( !rapid_json_string_value(rec1_json, "\"text\"", text1, sizeof(text1)) ) text1[0] = '\0';
	if ( !rapid_json_string_value(rec2_json, "\"text\"", text2, sizeof(text2)) ) text2[0] = '\0';
	if ( !rapid_json_string_value(rec3_json, "\"text\"", text3, sizeof(text3)) ) text3[0] = '\0';
	score0 = rapid_json_number_value(rec0_json, "\"score\"", 0.0);
	score1 = rapid_json_number_value(rec1_json, "\"score\"", 0.0);
	score2 = rapid_json_number_value(rec2_json, "\"score\"", 0.0);
	score3 = rapid_json_number_value(rec3_json, "\"score\"", 0.0);
	snprintf(g_assembled_json, sizeof(g_assembled_json), "{\"boxes\":%s,\"txts\":[\"%s\",\"%s\",\"%s\",\"%s\"],\"scores\":[%.6g,%.6g,%.6g,%.6g],\"word_results\":[],\"elapse_list\":[],\"elapse\":%.6g}", boxes_json, text0, text1, text2, text3, score0, score1, score2, score3, elapse);
	rapid_set_error("");
	return g_assembled_json;
}

XL_EXPORT const char* rapidocr_assemble_result_list_native(const char* boxes_json, const char* recs_json, double elapse)
{
	const char* p = recs_json != NULL ? recs_json : "";
	size_t used = 0;
	int count = 0;
	if ( boxes_json == NULL || boxes_json[0] == '\0' ) boxes_json = "[]";
	snprintf(g_assembled_json, sizeof(g_assembled_json), "{\"boxes\":%s,\"txts\":[", boxes_json);
	used = strlen(g_assembled_json);
	while ( (p = strstr(p, "\"text\"")) != NULL && count < 256 ) {
		char text[1024];
		if ( !rapid_json_string_value(p, "\"text\"", text, sizeof(text)) ) text[0] = '\0';
		if ( count > 0 ) rapid_append(g_assembled_json, sizeof(g_assembled_json), &used, ",");
		rapid_append(g_assembled_json, sizeof(g_assembled_json), &used, "\"");
		rapid_append(g_assembled_json, sizeof(g_assembled_json), &used, text);
		rapid_append(g_assembled_json, sizeof(g_assembled_json), &used, "\"");
		++count;
		p += 6;
	}
	rapid_append(g_assembled_json, sizeof(g_assembled_json), &used, "],\"scores\":[");
	p = recs_json != NULL ? recs_json : "";
	count = 0;
	while ( (p = strstr(p, "\"score\"")) != NULL && count < 256 ) {
		char number[64];
		double score = rapid_json_number_value(p, "\"score\"", 0.0);
		if ( count > 0 ) rapid_append(g_assembled_json, sizeof(g_assembled_json), &used, ",");
		snprintf(number, sizeof(number), "%.6g", score);
		rapid_append(g_assembled_json, sizeof(g_assembled_json), &used, number);
		++count;
		p += 7;
	}
	rapid_append(g_assembled_json, sizeof(g_assembled_json), &used, "],\"word_results\":[],\"elapse_list\":[],\"elapse\":");
	{
		char number[64];
		snprintf(number, sizeof(number), "%.6g", elapse);
		rapid_append(g_assembled_json, sizeof(g_assembled_json), &used, number);
	}
	rapid_append(g_assembled_json, sizeof(g_assembled_json), &used, "}");
	rapid_set_error("");
	return g_assembled_json;
}

static const char* rapid_find_matching_bracket(const char* open)
{
	const char* p = open;
	int depth = 0;
	int in_string = 0;
	int escape = 0;
	if ( open == NULL || *open != '[' ) return NULL;
	while ( *p != '\0' ) {
		if ( in_string ) {
			if ( escape ) {
				escape = 0;
			} else if ( *p == '\\' ) {
				escape = 1;
			} else if ( *p == '"' ) {
				in_string = 0;
			}
		} else if ( *p == '"' ) {
			in_string = 1;
		} else if ( *p == '[' ) {
			++depth;
		} else if ( *p == ']' ) {
			--depth;
			if ( depth == 0 ) return p;
		}
		++p;
	}
	return NULL;
}

static int rapid_json_array_slice(const char* json, const char* key, const char** out_begin, const char** out_end)
{
	const char* p;
	if ( out_begin == NULL || out_end == NULL ) return 0;
	*out_begin = NULL;
	*out_end = NULL;
	if ( json == NULL || key == NULL ) return 0;
	p = strstr(json, key);
	if ( p == NULL ) return 0;
	p = strchr(p, '[');
	if ( p == NULL ) return 0;
	*out_begin = p;
	*out_end = rapid_find_matching_bracket(p);
	return *out_end != NULL;
}

static int rapid_parse_box_points_at(const char* boxes_json, int target_index, double values[8])
{
	const char* p = boxes_json;
	int count = 0;
	int current_index = 0;
	if ( values == NULL || p == NULL || target_index < 0 ) return 0;
	while ( *p != '\0' ) {
		const char* box = strstr(p, "\"box\"");
		if ( box == NULL ) return 0;
		p = strchr(box, '[');
		if ( p == NULL ) return 0;
		count = 0;
		while ( *p != '\0' && count < 8 ) {
			char* endp;
			while ( *p != '\0' && *p != '-' && *p != '+' && *p != '.' && (*p < '0' || *p > '9') ) ++p;
			if ( *p == '\0' ) break;
			values[count] = strtod(p, &endp);
			if ( endp == p ) break;
			++count;
			p = endp;
		}
		if ( count >= 8 && current_index == target_index ) return 1;
		++current_index;
	}
	return 0;
}

XL_EXPORT double rapidocr_box_point_x_native(const char* boxes_json, int index, int point_index)
{
	double pts[8];
	if ( point_index < 0 || point_index >= 4 ) return 0.0;
	if ( !rapid_parse_box_points_at(boxes_json, index, pts) ) return 0.0;
	return pts[point_index * 2];
}

XL_EXPORT double rapidocr_box_point_y_native(const char* boxes_json, int index, int point_index)
{
	double pts[8];
	if ( point_index < 0 || point_index >= 4 ) return 0.0;
	if ( !rapid_parse_box_points_at(boxes_json, index, pts) ) return 0.0;
	return pts[point_index * 2 + 1];
}

XL_EXPORT int rapidocr_box_rec_target_width_native(const char* boxes_json, int index, int rec_img_w, int rec_img_h)
{
	double pts[8];
	double width_top;
	double width_bottom;
	double height_left;
	double height_right;
	double crop_w;
	double crop_h;
	int target_w;
	if ( rec_img_w <= 0 ) rec_img_w = 320;
	if ( rec_img_h <= 0 ) rec_img_h = 48;
	if ( !rapid_parse_box_points_at(boxes_json, index, pts) ) return rec_img_w;
	width_top = rapid_db_distance(pts[0], pts[1], pts[2], pts[3]);
	width_bottom = rapid_db_distance(pts[4], pts[5], pts[6], pts[7]);
	height_left = rapid_db_distance(pts[0], pts[1], pts[6], pts[7]);
	height_right = rapid_db_distance(pts[2], pts[3], pts[4], pts[5]);
	crop_w = width_top > width_bottom ? width_top : width_bottom;
	crop_h = height_left > height_right ? height_left : height_right;
	if ( crop_w <= 0.0 || crop_h <= 0.0 ) return rec_img_w;
	if ( crop_h * 2.0 >= crop_w * 3.0 ) {
		double tmp = crop_w;
		crop_w = crop_h;
		crop_h = tmp;
	}
	if ( crop_h <= 0.0 ) return rec_img_w;
	target_w = (int)ceil(crop_w * (double)rec_img_h / crop_h);
	if ( target_w < rec_img_w ) target_w = rec_img_w;
	if ( target_w > 2048 ) target_w = 2048;
	return target_w;
}

static int rapid_json_string_array_value_at(const char* begin, const char* end, int target_index, char* out, size_t out_size)
{
	const char* p = begin;
	int index = 0;
	if ( out == NULL || out_size == 0u ) return 0;
	out[0] = '\0';
	if ( begin == NULL || end == NULL || target_index < 0 ) return 0;
	while ( p < end && *p != '\0' ) {
		if ( *p == '"' ) {
			size_t used = 0;
			++p;
			while ( p < end && *p != '\0' ) {
				if ( *p == '\\' && p + 1 < end ) {
					if ( used + 2u < out_size ) {
						out[used++] = *p++;
						out[used++] = *p++;
					} else {
						p += 2;
					}
					continue;
				}
				if ( *p == '"' ) break;
				if ( used + 1u < out_size ) out[used++] = *p;
				++p;
			}
			out[used] = '\0';
			if ( index == target_index ) return 1;
			++index;
		}
		++p;
	}
	return 0;
}

static int rapid_json_number_array_value_at(const char* begin, const char* end, int target_index, double* out)
{
	const char* p = begin;
	int index = 0;
	if ( out == NULL || begin == NULL || end == NULL || target_index < 0 ) return 0;
	while ( p < end && *p != '\0' ) {
		char* endp;
		while ( p < end && *p != '\0' && *p != '-' && *p != '+' && *p != '.' && (*p < '0' || *p > '9') ) ++p;
		if ( p >= end || *p == '\0' ) break;
		*out = strtod(p, &endp);
		if ( endp == p ) break;
		if ( index == target_index ) return 1;
		++index;
		p = endp;
	}
	return 0;
}

XL_EXPORT const char* rapidocr_to_ocr_items_json2_native(const char* result_json, double text_score)
{
	const char* boxes_begin = NULL;
	const char* boxes_end = NULL;
	const char* txts_begin = NULL;
	const char* txts_end = NULL;
	const char* scores_begin = NULL;
	const char* scores_end = NULL;
	size_t used = 0;
	int count;
	int i;
	int out_count = 0;
	if ( text_score < 0.0 ) text_score = 0.0;
	rapid_copy(g_ocr_items_json, sizeof(g_ocr_items_json), "[");
	used = 1;
	if ( result_json == NULL || result_json[0] == '\0' ) return g_ocr_items_json;
	if ( !rapid_json_array_slice(result_json, "\"boxes\"", &boxes_begin, &boxes_end) ) return "[]";
	if ( !rapid_json_array_slice(result_json, "\"txts\"", &txts_begin, &txts_end) ) return "[]";
	if ( !rapid_json_array_slice(result_json, "\"scores\"", &scores_begin, &scores_end) ) return "[]";
	(void)boxes_end;
	count = rapid_box_count(result_json);
	for ( i = 0; i < count && i < 256; ++i ) {
		double pts[8];
		double score = 0.0;
		double x;
		double y;
		double w;
		double h;
		double min_x;
		double min_y;
		double max_x;
		double max_y;
		int p;
		int cx;
		int cy;
		char text[1024];
		char item[1536];
		if ( !rapid_parse_box_points_at(result_json, i, pts) ) continue;
		if ( !rapid_json_string_array_value_at(txts_begin, txts_end, i, text, sizeof(text)) ) text[0] = '\0';
		(void)rapid_json_number_array_value_at(scores_begin, scores_end, i, &score);
		if ( text[0] == '\0' || score < text_score ) continue;
		min_x = max_x = pts[0];
		min_y = max_y = pts[1];
		for ( p = 1; p < 4; ++p ) {
			if ( pts[p * 2] < min_x ) min_x = pts[p * 2];
			if ( pts[p * 2] > max_x ) max_x = pts[p * 2];
			if ( pts[p * 2 + 1] < min_y ) min_y = pts[p * 2 + 1];
			if ( pts[p * 2 + 1] > max_y ) max_y = pts[p * 2 + 1];
		}
		x = min_x;
		y = min_y;
		w = max_x - min_x;
		h = max_y - min_y;
		cx = (int)(x + w / 2.0);
		cy = (int)(y + h / 2.0);
		if ( out_count > 0 ) rapid_append(g_ocr_items_json, sizeof(g_ocr_items_json), &used, ",");
		snprintf(item, sizeof(item), "\n    {\n        \"Ratio\": %.15g,\n        \"Text\": \"%s\",\n        \"cx\": %d,\n        \"cy\": %d,\n        \"h\": %.1f,\n        \"w\": %.1f,\n        \"x\": %.1f,\n        \"y\": %.1f\n    }", score, text, cx, cy, h, w, x, y);
		rapid_append(g_ocr_items_json, sizeof(g_ocr_items_json), &used, item);
		++out_count;
	}
	if ( out_count > 0 ) rapid_append(g_ocr_items_json, sizeof(g_ocr_items_json), &used, "\n");
	rapid_append(g_ocr_items_json, sizeof(g_ocr_items_json), &used, "]");
	rapid_set_error("");
	return g_ocr_items_json;
}

XL_EXPORT const char* rapidocr_to_ocr_items_json_native(const char* result_json)
{
	return rapidocr_to_ocr_items_json2_native(result_json, 0.5);
}
