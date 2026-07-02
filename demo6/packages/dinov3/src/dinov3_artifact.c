#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#define XL_EXPORT __declspec(dllexport)
#else
#define XL_EXPORT
#endif

typedef const char* (*OrtResultTextFn)(void*);
typedef int (*OrtResultIntFn)(void*);
typedef float (*OrtResultFloatAtFn)(void*, int);

typedef struct DinoOrtApi {
	int attempted;
	int loaded;
#if defined(_WIN32) || defined(_WIN64)
	HMODULE module;
#else
	void* module;
#endif
	OrtResultTextFn result_shape;
	OrtResultIntFn result_count;
	OrtResultFloatAtFn result_at;
} DinoOrtApi;

static DinoOrtApi g_ort;
static char g_error[512];
static char* g_json;
static size_t g_json_cap;

static void dino_copy(char* out, size_t out_size, const char* text)
{
	size_t len;
	if ( out == NULL || out_size == 0u ) return;
	if ( text == NULL ) text = "";
	len = strlen(text);
	if ( len >= out_size ) len = out_size - 1u;
	memcpy(out, text, len);
	out[len] = '\0';
}

static void dino_set_error(const char* text)
{
	dino_copy(g_error, sizeof(g_error), text);
}

static void dino_append(char* out, size_t out_size, size_t* used, const char* text)
{
	size_t len;
	if ( out == NULL || out_size == 0u || used == NULL || text == NULL || *used >= out_size ) return;
	len = strlen(text);
	if ( len >= out_size - *used ) len = out_size - *used - 1u;
	memcpy(out + *used, text, len);
	*used += len;
	out[*used] = '\0';
}

static void dino_appendf(char* out, size_t out_size, size_t* used, const char* fmt, ...)
{
	va_list args;
	int n;
	if ( out == NULL || out_size == 0u || used == NULL || fmt == NULL || *used >= out_size ) return;
	va_start(args, fmt);
	n = vsnprintf(out + *used, out_size - *used, fmt, args);
	va_end(args);
	if ( n <= 0 ) return;
	if ( (size_t)n >= out_size - *used ) {
		*used = out_size - 1u;
	} else {
		*used += (size_t)n;
	}
	out[*used] = '\0';
}

static void dino_json_escape(char* out, size_t out_size, size_t* used, const char* text)
{
	const char* p;
	if ( text == NULL ) return;
	for ( p = text; *p != '\0' && *used + 2u < out_size; ++p ) {
		if ( *p == '"' || *p == '\\' ) out[(*used)++] = '\\';
		out[(*used)++] = *p;
	}
	out[*used] = '\0';
}

static int dino_json_reserve(size_t cap)
{
	char* next;
	if ( cap < 256u ) cap = 256u;
	if ( g_json_cap >= cap && g_json != NULL ) {
		g_json[0] = '\0';
		return 1;
	}
	next = (char*)realloc(g_json, cap);
	if ( next == NULL ) {
		dino_set_error("out of memory");
		return 0;
	}
	g_json = next;
	g_json_cap = cap;
	g_json[0] = '\0';
	return 1;
}

static void* dino_sym(const char* name)
{
#if defined(_WIN32) || defined(_WIN64)
	return g_ort.module != NULL ? (void*)GetProcAddress(g_ort.module, name) : NULL;
#else
	return g_ort.module != NULL ? dlsym(g_ort.module, name) : NULL;
#endif
}

static int dino_try_load(const char* path)
{
	if ( path == NULL || path[0] == '\0' ) return 0;
#if defined(_WIN32) || defined(_WIN64)
	g_ort.module = LoadLibraryExA(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
#else
	g_ort.module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
	if ( g_ort.module == NULL ) return 0;
	g_ort.result_shape = (OrtResultTextFn)dino_sym("onnxruntime_xl_result_float_shape_json");
	g_ort.result_count = (OrtResultIntFn)dino_sym("onnxruntime_xl_result_float_count");
	g_ort.result_at = (OrtResultFloatAtFn)dino_sym("onnxruntime_xl_result_float_at");
	if ( g_ort.result_shape == NULL || g_ort.result_count == NULL || g_ort.result_at == NULL ) {
		dino_set_error("onnxruntime_xlang.dll loaded but result reader symbols are missing");
		return 0;
	}
	g_ort.loaded = 1;
	dino_set_error("");
	return 1;
}

static int dino_ort_load(void)
{
	const char* env_path;
	const char* candidates[] = {
		"onnxruntime_xlang.dll",
		"bin\\onnxruntime_xlang.dll",
		"packages\\onnxruntime\\bin\\onnxruntime_xlang.dll",
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
	if ( dino_try_load(env_path) ) return 1;
	for ( i = 0; candidates[i] != NULL; ++i ) {
		if ( dino_try_load(candidates[i]) ) return 1;
	}
	dino_set_error("failed to load onnxruntime_xlang.dll");
	return 0;
}

XL_EXPORT const char* dinov3_version_native(void)
{
	return "dinov3-xlang 0.1.0";
}

XL_EXPORT const char* dinov3_last_error_native(void)
{
	return g_error;
}

XL_EXPORT const char* dinov3_embedding_json_native(void* result_handle)
{
	int count;
	int i;
	size_t used = 0;
	if ( result_handle == NULL || !dino_ort_load() ) return "[]";
	count = g_ort.result_count(result_handle);
	if ( count <= 0 ) return "[]";
	if ( !dino_json_reserve((size_t)count * 18u + 64u) ) return "[]";
	dino_append(g_json, g_json_cap, &used, "[");
	for ( i = 0; i < count; ++i ) {
		if ( i > 0 ) dino_append(g_json, g_json_cap, &used, ",");
		dino_appendf(g_json, g_json_cap, &used, "%.9g", (double)g_ort.result_at(result_handle, i));
	}
	dino_append(g_json, g_json_cap, &used, "]");
	return g_json;
}

XL_EXPORT const char* dinov3_output_json_native(void* result_handle, const char* source_path)
{
	int count;
	int i;
	const char* shape;
	size_t used = 0;
	if ( result_handle == NULL || !dino_ort_load() ) return "{}";
	count = g_ort.result_count(result_handle);
	shape = g_ort.result_shape(result_handle);
	if ( count <= 0 ) return "{}";
	if ( !dino_json_reserve((size_t)count * 18u + 512u) ) return "{}";
	dino_append(g_json, g_json_cap, &used, "{\"source_path\":\"");
	dino_json_escape(g_json, g_json_cap, &used, source_path != NULL ? source_path : "");
	dino_append(g_json, g_json_cap, &used, "\",\"shape\":");
	dino_append(g_json, g_json_cap, &used, (shape != NULL && shape[0] != '\0') ? shape : "[]");
	dino_appendf(g_json, g_json_cap, &used, ",\"count\":%d,\"embedding\":[", count);
	for ( i = 0; i < count; ++i ) {
		if ( i > 0 ) dino_append(g_json, g_json_cap, &used, ",");
		dino_appendf(g_json, g_json_cap, &used, "%.9g", (double)g_ort.result_at(result_handle, i));
	}
	dino_append(g_json, g_json_cap, &used, "]}");
	return g_json;
}

XL_EXPORT double dinov3_l2_norm_native(void* result_handle)
{
	int count;
	int i;
	double sum = 0.0;
	if ( result_handle == NULL || !dino_ort_load() ) return 0.0;
	count = g_ort.result_count(result_handle);
	for ( i = 0; i < count; ++i ) {
		double v = (double)g_ort.result_at(result_handle, i);
		sum += v * v;
	}
	return sqrt(sum);
}

XL_EXPORT double dinov3_cosine_native(void* left_handle, void* right_handle)
{
	int left_count;
	int right_count;
	int i;
	double dot = 0.0;
	double left_sum = 0.0;
	double right_sum = 0.0;
	if ( left_handle == NULL || right_handle == NULL || !dino_ort_load() ) return 0.0;
	left_count = g_ort.result_count(left_handle);
	right_count = g_ort.result_count(right_handle);
	if ( left_count <= 0 || left_count != right_count ) return 0.0;
	for ( i = 0; i < left_count; ++i ) {
		double a = (double)g_ort.result_at(left_handle, i);
		double b = (double)g_ort.result_at(right_handle, i);
		dot += a * b;
		left_sum += a * a;
		right_sum += b * b;
	}
	if ( left_sum <= 0.0 || right_sum <= 0.0 ) return 0.0;
	return dot / (sqrt(left_sum) * sqrt(right_sum));
}
