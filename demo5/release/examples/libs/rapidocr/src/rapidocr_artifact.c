#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
static char g_det_boxes_json[512];
static char g_charset_json[512];
static char g_rec_json[4096];
static char g_assembled_json[8192];
static char g_cls_json[256];

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
	int kept_x1[16];
	int kept_y1[16];
	int kept_x2[16];
	int kept_y2[16];
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
	for ( y = 0; y < map_h && box_count < 16; ++y ) {
		for ( x = 0; x < map_w && box_count < 16; ++x ) {
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
			if ( hits <= 0 ) continue;
			x1 = (int)((double)min_x * (double)image_width / (double)map_w);
			x2 = (int)(((double)max_x + 1.0) * (double)image_width / (double)map_w);
			y1 = (int)((double)min_y * (double)image_height / (double)map_h);
			y2 = (int)(((double)max_y + 1.0) * (double)image_height / (double)map_h);
			score = sum / (double)hits;
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
	*out_items = items;
	*out_count = count;
	return 1;
}

static void rapid_free_charset(char** items, int count)
{
	int i;
	if ( items == NULL ) return;
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
	strcpy(g_rec_json, "{\"text\":\"");
	text_len = (int)strlen(g_rec_json);
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
				if ( text_len + (int)n + 64 < (int)sizeof(g_rec_json) ) {
					memcpy(g_rec_json + text_len, s, n);
					text_len += (int)n;
					g_rec_json[text_len] = '\0';
				}
				score_sum += best_value;
				++score_count;
			}
		}
		last_idx = best_idx;
	}
rapid_rec_decode_done:
	snprintf(g_rec_json + text_len, sizeof(g_rec_json) - (size_t)text_len, "\",\"score\":%.6g,\"tokens\":%d}", score_count > 0 ? score_sum / (double)score_count : 0.0, score_count);
	rapid_free_charset(charset, charset_count);
	rapid_set_error("");
	return g_rec_json;
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
	strcpy(g_rec_json, "{\"text\":\"");
	text_len = (int)strlen(g_rec_json);
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
				if ( text_len + (int)n + 64 < (int)sizeof(g_rec_json) ) {
					memcpy(g_rec_json + text_len, s, n);
					text_len += (int)n;
					g_rec_json[text_len] = '\0';
				}
				score_sum += best_value;
				++score_count;
			}
		}
		last_idx = best_idx;
	}
	snprintf(g_rec_json + text_len, sizeof(g_rec_json) - (size_t)text_len, "\",\"score\":%.6g,\"tokens\":%d}", score_count > 0 ? score_sum / (double)score_count : 0.0, score_count);
	rapid_free_charset(charset, charset_count);
	rapid_set_error("");
	return g_rec_json;
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
