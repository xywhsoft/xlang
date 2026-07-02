#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__AVX2__)
#include <immintrin.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
#define XL_EXPORT __declspec(dllexport)
#else
#define XL_EXPORT
#endif

#define XFEAT_DESC_DIM 64
#define XFEAT_ROW_DIM 67
#define XFEAT_MAX_QUERY_SCALES 16

typedef const char* (*OrtResultTextFn)(void*);
typedef int (*OrtResultIntFn)(void*);
typedef float (*OrtResultFloatAtFn)(void*, int);
typedef const float* (*OrtResultFloatDataFn)(void*);

typedef struct XFeatOrtApi {
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
	OrtResultFloatDataFn result_data;
} XFeatOrtApi;

typedef struct XFeatFeatures {
	int count;
	int capacity;
	int image_w;
	int image_h;
	int resized_w;
	int resized_h;
	float* xy;
	float* score;
	float* desc;
} XFeatFeatures;

typedef struct XFeatLocation {
	int found;
	int matches;
	int inliers;
	float scale;
	double reprojection_error;
	double score;
	double polygon[8];
	int bbox[4];
	const char* method;
} XFeatLocation;

typedef struct XFeatEngine {
	void* query_session;
	void* target_session;
} XFeatEngine;

typedef struct XFeatQuerySet {
	int count;
	float scales[XFEAT_MAX_QUERY_SCALES];
	void* features[XFEAT_MAX_QUERY_SCALES];
} XFeatQuerySet;

typedef struct XFeatStatsBucket {
	int count;
	double preprocess_ms;
	double ort_ms;
	double parse_ms;
} XFeatStatsBucket;

XL_EXPORT int xfeat_features_release_native(void* handle);

static XFeatOrtApi g_ort;
static char g_error[512];
static char* g_json;
static size_t g_json_cap;
static XFeatStatsBucket g_stats[3];

static void xfeat_copy(char* out, size_t out_size, const char* text)
{
	size_t len;
	if ( out == NULL || out_size == 0u ) return;
	if ( text == NULL ) text = "";
	len = strlen(text);
	if ( len >= out_size ) len = out_size - 1u;
	memcpy(out, text, len);
	out[len] = '\0';
}

static void xfeat_set_error(const char* text)
{
	xfeat_copy(g_error, sizeof(g_error), text);
}

static void xfeat_append(char* out, size_t out_size, size_t* used, const char* text)
{
	size_t len;
	if ( out == NULL || out_size == 0u || used == NULL || text == NULL || *used >= out_size ) return;
	len = strlen(text);
	if ( len >= out_size - *used ) len = out_size - *used - 1u;
	memcpy(out + *used, text, len);
	*used += len;
	out[*used] = '\0';
}

static void xfeat_appendf(char* out, size_t out_size, size_t* used, const char* fmt, ...)
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

static void xfeat_json_escape(char* out, size_t out_size, size_t* used, const char* text)
{
	const char* p;
	if ( text == NULL ) return;
	for ( p = text; *p != '\0' && *used + 2u < out_size; ++p ) {
		if ( *p == '"' || *p == '\\' ) out[(*used)++] = '\\';
		out[(*used)++] = *p;
	}
	out[*used] = '\0';
}

static int xfeat_json_reserve(size_t cap)
{
	char* next;
	if ( cap < 512u ) cap = 512u;
	if ( g_json != NULL && g_json_cap >= cap ) {
		g_json[0] = '\0';
		return 1;
	}
	next = (char*)realloc(g_json, cap);
	if ( next == NULL ) {
		xfeat_set_error("out of memory");
		return 0;
	}
	g_json = next;
	g_json_cap = cap;
	g_json[0] = '\0';
	return 1;
}

static void* xfeat_sym(const char* name)
{
#if defined(_WIN32) || defined(_WIN64)
	return g_ort.module != NULL ? (void*)GetProcAddress(g_ort.module, name) : NULL;
#else
	return g_ort.module != NULL ? dlsym(g_ort.module, name) : NULL;
#endif
}

static int xfeat_try_load(const char* path)
{
	if ( path == NULL || path[0] == '\0' ) return 0;
#if defined(_WIN32) || defined(_WIN64)
	g_ort.module = LoadLibraryExA(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
#else
	g_ort.module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
	if ( g_ort.module == NULL ) return 0;
	g_ort.result_shape = (OrtResultTextFn)xfeat_sym("onnxruntime_xl_result_float_shape_json");
	g_ort.result_count = (OrtResultIntFn)xfeat_sym("onnxruntime_xl_result_float_count");
	g_ort.result_at = (OrtResultFloatAtFn)xfeat_sym("onnxruntime_xl_result_float_at");
	g_ort.result_data = (OrtResultFloatDataFn)xfeat_sym("onnxruntime_xl_result_float_data");
	if ( g_ort.result_shape == NULL || g_ort.result_count == NULL || g_ort.result_at == NULL ) {
		xfeat_set_error("onnxruntime_xlang.dll loaded but result reader symbols are missing");
		return 0;
	}
	g_ort.loaded = 1;
	xfeat_set_error("");
	return 1;
}

static int xfeat_ort_load(void)
{
	const char* env_path;
	const char* candidates[] = {
		"onnxruntime_xlang.dll",
		"bin\\onnxruntime_xlang.dll",
		"extlibs\\onnxruntime\\bin\\onnxruntime_xlang.dll",
		"release\\extlibs\\onnxruntime\\bin\\onnxruntime_xlang.dll",
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
	if ( xfeat_try_load(env_path) ) return 1;
	for ( i = 0; candidates[i] != NULL; ++i ) {
		if ( xfeat_try_load(candidates[i]) ) return 1;
	}
	xfeat_set_error("failed to load onnxruntime_xlang.dll");
	return 0;
}

static int xfeat_is_finite(float v)
{
	return v == v && v > -3.4e38f && v < 3.4e38f;
}

static void xfeat_normalize_desc(float* desc)
{
	int i;
	double sum = 0.0;
	if ( desc == NULL ) return;
	for ( i = 0; i < XFEAT_DESC_DIM; ++i ) {
		sum += (double)desc[i] * (double)desc[i];
	}
	if ( sum <= 1.0e-24 ) return;
	sum = 1.0 / sqrt(sum);
	for ( i = 0; i < XFEAT_DESC_DIM; ++i ) {
		desc[i] = (float)((double)desc[i] * sum);
	}
}

XL_EXPORT double xfeat_now_ms_native(void)
{
#if defined(_WIN32) || defined(_WIN64)
	static LARGE_INTEGER freq;
	LARGE_INTEGER now;
	if ( freq.QuadPart == 0 ) QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&now);
	return ((double)now.QuadPart * 1000.0) / (double)freq.QuadPart;
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
#endif
}

XL_EXPORT int xfeat_stats_reset_native(void)
{
	memset(g_stats, 0, sizeof(g_stats));
	return 1;
}

XL_EXPORT int xfeat_stats_add_native(int kind, double preprocess_ms, double ort_ms, double parse_ms)
{
	XFeatStatsBucket* bucket;
	if ( kind < 0 || kind >= (int)(sizeof(g_stats) / sizeof(g_stats[0])) ) kind = 0;
	bucket = &g_stats[kind];
	bucket->count++;
	bucket->preprocess_ms += preprocess_ms;
	bucket->ort_ms += ort_ms;
	bucket->parse_ms += parse_ms;
	return 1;
}

static void xfeat_stats_bucket_json(char* out, size_t out_size, size_t* used, const char* name, const XFeatStatsBucket* bucket)
{
	double count = bucket != NULL && bucket->count > 0 ? (double)bucket->count : 1.0;
	xfeat_appendf(out, out_size, used,
		"\"%s\":{\"count\":%d,\"preprocess_ms\":%.6g,\"ort_ms\":%.6g,\"parse_ms\":%.6g,\"feature_total_ms\":%.6g,\"avg_preprocess_ms\":%.6g,\"avg_ort_ms\":%.6g,\"avg_parse_ms\":%.6g,\"avg_feature_total_ms\":%.6g}",
		name,
		bucket != NULL ? bucket->count : 0,
		bucket != NULL ? bucket->preprocess_ms : 0.0,
		bucket != NULL ? bucket->ort_ms : 0.0,
		bucket != NULL ? bucket->parse_ms : 0.0,
		bucket != NULL ? bucket->preprocess_ms + bucket->ort_ms + bucket->parse_ms : 0.0,
		bucket != NULL ? bucket->preprocess_ms / count : 0.0,
		bucket != NULL ? bucket->ort_ms / count : 0.0,
		bucket != NULL ? bucket->parse_ms / count : 0.0,
		bucket != NULL ? (bucket->preprocess_ms + bucket->ort_ms + bucket->parse_ms) / count : 0.0
	);
}

XL_EXPORT const char* xfeat_stats_json_native(void)
{
	size_t used = 0;
	if ( !xfeat_json_reserve(2048u) ) return "{}";
	xfeat_append(g_json, g_json_cap, &used, "{");
	xfeat_stats_bucket_json(g_json, g_json_cap, &used, "all", &g_stats[0]);
	xfeat_append(g_json, g_json_cap, &used, ",");
	xfeat_stats_bucket_json(g_json, g_json_cap, &used, "query", &g_stats[1]);
	xfeat_append(g_json, g_json_cap, &used, ",");
	xfeat_stats_bucket_json(g_json, g_json_cap, &used, "target", &g_stats[2]);
	xfeat_append(g_json, g_json_cap, &used, "}");
	return g_json;
}

static float xfeat_dot64(const float* a, const float* b)
{
#if defined(__AVX2__) && defined(__FMA__)
	__m256 acc0 = _mm256_setzero_ps();
	__m256 acc1 = _mm256_setzero_ps();
	__m256 acc2 = _mm256_setzero_ps();
	__m256 acc3 = _mm256_setzero_ps();
	__m256 av;
	__m256 bv;
	__m128 lo;
	__m128 hi;
	__m128 sum4;
	float tmp[4];
	av = _mm256_loadu_ps(a);
	bv = _mm256_loadu_ps(b);
	acc0 = _mm256_fmadd_ps(av, bv, acc0);
	av = _mm256_loadu_ps(a + 8);
	bv = _mm256_loadu_ps(b + 8);
	acc1 = _mm256_fmadd_ps(av, bv, acc1);
	av = _mm256_loadu_ps(a + 16);
	bv = _mm256_loadu_ps(b + 16);
	acc2 = _mm256_fmadd_ps(av, bv, acc2);
	av = _mm256_loadu_ps(a + 24);
	bv = _mm256_loadu_ps(b + 24);
	acc3 = _mm256_fmadd_ps(av, bv, acc3);
	av = _mm256_loadu_ps(a + 32);
	bv = _mm256_loadu_ps(b + 32);
	acc0 = _mm256_fmadd_ps(av, bv, acc0);
	av = _mm256_loadu_ps(a + 40);
	bv = _mm256_loadu_ps(b + 40);
	acc1 = _mm256_fmadd_ps(av, bv, acc1);
	av = _mm256_loadu_ps(a + 48);
	bv = _mm256_loadu_ps(b + 48);
	acc2 = _mm256_fmadd_ps(av, bv, acc2);
	av = _mm256_loadu_ps(a + 56);
	bv = _mm256_loadu_ps(b + 56);
	acc3 = _mm256_fmadd_ps(av, bv, acc3);
	acc0 = _mm256_add_ps(_mm256_add_ps(acc0, acc1), _mm256_add_ps(acc2, acc3));
	lo = _mm256_castps256_ps128(acc0);
	hi = _mm256_extractf128_ps(acc0, 1);
	sum4 = _mm_add_ps(lo, hi);
	sum4 = _mm_hadd_ps(sum4, sum4);
	sum4 = _mm_hadd_ps(sum4, sum4);
	_mm_storeu_ps(tmp, sum4);
	return tmp[0];
#else
	float dot = 0.0f;
	int k;
	for ( k = 0; k < XFEAT_DESC_DIM; ++k ) dot += a[k] * b[k];
	return dot;
#endif
}

XL_EXPORT const char* xfeat_version_native(void)
{
	return "xfeat-xlang 0.1.0";
}

XL_EXPORT const char* xfeat_last_error_native(void)
{
	return g_error;
}

XL_EXPORT void* xfeat_engine_create_native(void* query_session, void* target_session)
{
	XFeatEngine* engine;
	if ( query_session == NULL || target_session == NULL ) {
		xfeat_set_error("invalid XFeat session handles");
		return NULL;
	}
	engine = (XFeatEngine*)calloc(1u, sizeof(*engine));
	if ( engine == NULL ) {
		xfeat_set_error("out of memory");
		return NULL;
	}
	engine->query_session = query_session;
	engine->target_session = target_session;
	xfeat_set_error("");
	return engine;
}

XL_EXPORT void* xfeat_engine_query_session_native(void* handle)
{
	XFeatEngine* engine = (XFeatEngine*)handle;
	return engine != NULL ? engine->query_session : NULL;
}

XL_EXPORT void* xfeat_engine_target_session_native(void* handle)
{
	XFeatEngine* engine = (XFeatEngine*)handle;
	return engine != NULL ? engine->target_session : NULL;
}

XL_EXPORT int xfeat_engine_release_native(void* handle)
{
	free(handle);
	return 1;
}

XL_EXPORT void* xfeat_query_create_native(void)
{
	XFeatQuerySet* query_set = (XFeatQuerySet*)calloc(1u, sizeof(*query_set));
	if ( query_set == NULL ) xfeat_set_error("out of memory");
	else xfeat_set_error("");
	return query_set;
}

XL_EXPORT int xfeat_query_add_native(void* handle, void* features, double scale)
{
	XFeatQuerySet* query_set = (XFeatQuerySet*)handle;
	if ( query_set == NULL || features == NULL ) {
		xfeat_set_error("invalid XFeat query set or features");
		return 0;
	}
	if ( query_set->count >= XFEAT_MAX_QUERY_SCALES ) {
		xfeat_set_error("too many XFeat query scales");
		return 0;
	}
	query_set->features[query_set->count] = features;
	query_set->scales[query_set->count] = (float)scale;
	query_set->count++;
	xfeat_set_error("");
	return 1;
}

XL_EXPORT int xfeat_query_count_native(void* handle)
{
	XFeatQuerySet* query_set = (XFeatQuerySet*)handle;
	return query_set != NULL ? query_set->count : 0;
}

XL_EXPORT void* xfeat_query_features_native(void* handle, int index)
{
	XFeatQuerySet* query_set = (XFeatQuerySet*)handle;
	if ( query_set == NULL || index < 0 || index >= query_set->count ) return NULL;
	return query_set->features[index];
}

XL_EXPORT double xfeat_query_scale_native(void* handle, int index)
{
	XFeatQuerySet* query_set = (XFeatQuerySet*)handle;
	if ( query_set == NULL || index < 0 || index >= query_set->count ) return 1.0;
	return query_set->scales[index];
}

XL_EXPORT int xfeat_query_release_native(void* handle)
{
	XFeatQuerySet* query_set = (XFeatQuerySet*)handle;
	int i;
	if ( query_set != NULL ) {
		for ( i = 0; i < query_set->count; ++i ) {
			if ( query_set->features[i] != NULL ) xfeat_features_release_native(query_set->features[i]);
		}
		free(query_set);
	}
	return 1;
}

XL_EXPORT void* xfeat_features_from_result_native(void* result_handle, int image_w, int image_h, int resized_w, int resized_h)
{
	XFeatFeatures* features;
	const float* result_data;
	int float_count;
	int row_count;
	int i;
	if ( result_handle == NULL || image_w <= 0 || image_h <= 0 || resized_w <= 0 || resized_h <= 0 ) {
		xfeat_set_error("invalid XFeat result or image shape");
		return NULL;
	}
	if ( !xfeat_ort_load() ) return NULL;
	float_count = g_ort.result_count(result_handle);
	if ( float_count < XFEAT_ROW_DIM || (float_count % XFEAT_ROW_DIM) != 0 ) {
		xfeat_set_error("unexpected XFeat ONNX output shape");
		return NULL;
	}
	result_data = g_ort.result_data != NULL ? g_ort.result_data(result_handle) : NULL;
	row_count = float_count / XFEAT_ROW_DIM;
	features = (XFeatFeatures*)calloc(1, sizeof(*features));
	if ( features == NULL ) {
		xfeat_set_error("out of memory");
		return NULL;
	}
	features->capacity = row_count;
	features->image_w = image_w;
	features->image_h = image_h;
	features->resized_w = resized_w;
	features->resized_h = resized_h;
	features->xy = (float*)malloc(sizeof(float) * (size_t)row_count * 2u);
	features->score = (float*)malloc(sizeof(float) * (size_t)row_count);
	features->desc = (float*)malloc(sizeof(float) * (size_t)row_count * XFEAT_DESC_DIM);
	if ( features->xy == NULL || features->score == NULL || features->desc == NULL ) {
		xfeat_features_release_native(features);
		xfeat_set_error("out of memory");
		return NULL;
	}
	for ( i = 0; i < row_count; ++i ) {
		int base = i * XFEAT_ROW_DIM;
		const float* row = result_data != NULL ? result_data + base : NULL;
		float score = row != NULL ? row[2] : g_ort.result_at(result_handle, base + 2);
		int j;
		if ( score <= 0.0f || !xfeat_is_finite(score) ) continue;
		features->xy[features->count * 2] = (row != NULL ? row[0] : g_ort.result_at(result_handle, base)) * (float)image_w / (float)resized_w;
		features->xy[features->count * 2 + 1] = (row != NULL ? row[1] : g_ort.result_at(result_handle, base + 1)) * (float)image_h / (float)resized_h;
		features->score[features->count] = score;
		for ( j = 0; j < XFEAT_DESC_DIM; ++j ) {
			features->desc[features->count * XFEAT_DESC_DIM + j] = row != NULL ? row[3 + j] : g_ort.result_at(result_handle, base + 3 + j);
		}
		xfeat_normalize_desc(features->desc + features->count * XFEAT_DESC_DIM);
		features->count++;
	}
	xfeat_set_error("");
	return features;
}

XL_EXPORT int xfeat_features_release_native(void* handle)
{
	XFeatFeatures* features = (XFeatFeatures*)handle;
	if ( features != NULL ) {
		free(features->xy);
		free(features->score);
		free(features->desc);
		free(features);
	}
	return 1;
}

XL_EXPORT int xfeat_features_count_native(void* handle)
{
	XFeatFeatures* features = (XFeatFeatures*)handle;
	return features != NULL ? features->count : 0;
}

static double xfeat_det3(double a00, double a01, double a02, double a10, double a11, double a12, double a20, double a21, double a22)
{
	return a00 * (a11 * a22 - a12 * a21) - a01 * (a10 * a22 - a12 * a20) + a02 * (a10 * a21 - a11 * a20);
}

static int xfeat_solve3(double m[9], double b[3], double out[3])
{
	double det = xfeat_det3(m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
	double mx[9];
	if ( fabs(det) < 1.0e-9 ) return 0;
	memcpy(mx, m, sizeof(mx));
	mx[0] = b[0]; mx[3] = b[1]; mx[6] = b[2];
	out[0] = xfeat_det3(mx[0], mx[1], mx[2], mx[3], mx[4], mx[5], mx[6], mx[7], mx[8]) / det;
	memcpy(mx, m, sizeof(mx));
	mx[1] = b[0]; mx[4] = b[1]; mx[7] = b[2];
	out[1] = xfeat_det3(mx[0], mx[1], mx[2], mx[3], mx[4], mx[5], mx[6], mx[7], mx[8]) / det;
	memcpy(mx, m, sizeof(mx));
	mx[2] = b[0]; mx[5] = b[1]; mx[8] = b[2];
	out[2] = xfeat_det3(mx[0], mx[1], mx[2], mx[3], mx[4], mx[5], mx[6], mx[7], mx[8]) / det;
	return 1;
}

static int xfeat_fit_affine3(const double* src, const double* dst, int i0, int i1, int i2, double affine[6])
{
	double m[9];
	double bu[3];
	double bv[3];
	double au[3];
	double av[3];
	int ids[3];
	int k;
	ids[0] = i0; ids[1] = i1; ids[2] = i2;
	for ( k = 0; k < 3; ++k ) {
		int idx = ids[k];
		m[k * 3] = src[idx * 2];
		m[k * 3 + 1] = src[idx * 2 + 1];
		m[k * 3 + 2] = 1.0;
		bu[k] = dst[idx * 2];
		bv[k] = dst[idx * 2 + 1];
	}
	if ( !xfeat_solve3(m, bu, au) || !xfeat_solve3(m, bv, av) ) return 0;
	affine[0] = au[0]; affine[1] = au[1]; affine[2] = au[2];
	affine[3] = av[0]; affine[4] = av[1]; affine[5] = av[2];
	return 1;
}

static int xfeat_fit_affine_ls(const double* src, const double* dst, const unsigned char* mask, int count, double affine[6])
{
	double m[9];
	double bu[3];
	double bv[3];
	double au[3];
	double av[3];
	int i;
	int n = 0;
	memset(m, 0, sizeof(m));
	memset(bu, 0, sizeof(bu));
	memset(bv, 0, sizeof(bv));
	for ( i = 0; i < count; ++i ) {
		double x;
		double y;
		double u;
		double v;
		if ( mask != NULL && !mask[i] ) continue;
		x = src[i * 2];
		y = src[i * 2 + 1];
		u = dst[i * 2];
		v = dst[i * 2 + 1];
		m[0] += x * x;
		m[1] += x * y;
		m[2] += x;
		m[4] += y * y;
		m[5] += y;
		m[8] += 1.0;
		bu[0] += x * u;
		bu[1] += y * u;
		bu[2] += u;
		bv[0] += x * v;
		bv[1] += y * v;
		bv[2] += v;
		n++;
	}
	if ( n < 3 ) return 0;
	m[3] = m[1];
	m[6] = m[2];
	m[7] = m[5];
	if ( !xfeat_solve3(m, bu, au) || !xfeat_solve3(m, bv, av) ) return 0;
	affine[0] = au[0]; affine[1] = au[1]; affine[2] = au[2];
	affine[3] = av[0]; affine[4] = av[1]; affine[5] = av[2];
	return 1;
}

static void xfeat_project_affine(const double affine[6], double x, double y, double* u, double* v)
{
	*u = affine[0] * x + affine[1] * y + affine[2];
	*v = affine[3] * x + affine[4] * y + affine[5];
}

static int xfeat_eval_affine(const double* src, const double* dst, int count, const double affine[6], double thresh, unsigned char* mask, double* mean_error)
{
	int i;
	int inliers = 0;
	double sum_error = 0.0;
	double thresh2 = thresh * thresh;
	for ( i = 0; i < count; ++i ) {
		double u;
		double v;
		double dx;
		double dy;
		double e2;
		xfeat_project_affine(affine, src[i * 2], src[i * 2 + 1], &u, &v);
		dx = u - dst[i * 2];
		dy = v - dst[i * 2 + 1];
		e2 = dx * dx + dy * dy;
		if ( e2 <= thresh2 ) {
			if ( mask != NULL ) mask[i] = 1;
			inliers++;
			sum_error += sqrt(e2);
		} else if ( mask != NULL ) {
			mask[i] = 0;
		}
	}
	if ( mean_error != NULL ) *mean_error = inliers > 0 ? sum_error / (double)inliers : 1.0e9;
	return inliers;
}

static unsigned int xfeat_lcg(unsigned int* seed)
{
	*seed = (*seed * 1664525u) + 1013904223u;
	return *seed;
}

static void xfeat_compute_polygon_bbox(XFeatLocation* loc, const double affine[6], int query_w, int query_h, int target_w, int target_h)
{
	double corners[8];
	double minx;
	double miny;
	double maxx;
	double maxy;
	int i;
	corners[0] = 0.0; corners[1] = 0.0;
	corners[2] = (double)(query_w - 1); corners[3] = 0.0;
	corners[4] = (double)(query_w - 1); corners[5] = (double)(query_h - 1);
	corners[6] = 0.0; corners[7] = (double)(query_h - 1);
	for ( i = 0; i < 4; ++i ) {
		xfeat_project_affine(affine, corners[i * 2], corners[i * 2 + 1], &loc->polygon[i * 2], &loc->polygon[i * 2 + 1]);
	}
	minx = maxx = loc->polygon[0];
	miny = maxy = loc->polygon[1];
	for ( i = 1; i < 4; ++i ) {
		double x = loc->polygon[i * 2];
		double y = loc->polygon[i * 2 + 1];
		if ( x < minx ) minx = x;
		if ( y < miny ) miny = y;
		if ( x > maxx ) maxx = x;
		if ( y > maxy ) maxy = y;
	}
	loc->bbox[0] = (int)floor(minx + 0.5);
	loc->bbox[1] = (int)floor(miny + 0.5);
	loc->bbox[2] = (int)ceil(maxx - minx);
	loc->bbox[3] = (int)ceil(maxy - miny);
	if ( loc->bbox[2] < 0 ) loc->bbox[2] = 0;
	if ( loc->bbox[3] < 0 ) loc->bbox[3] = 0;
	(void)target_w;
	(void)target_h;
}

static int xfeat_polygon_reasonable(const XFeatLocation* loc, int target_w, int target_h)
{
	int i;
	if ( loc == NULL || loc->bbox[2] < 12 || loc->bbox[3] < 12 ) return 0;
	if ( loc->bbox[2] > target_w / 2 || loc->bbox[3] > target_h / 2 ) return 0;
	for ( i = 0; i < 4; ++i ) {
		double x = loc->polygon[i * 2];
		double y = loc->polygon[i * 2 + 1];
		if ( !(x > -1000000.0 && x < 1000000.0 && y > -1000000.0 && y < 1000000.0) ) return 0;
	}
	return 1;
}

static XFeatLocation* xfeat_estimate_affine(const double* src, const double* dst, int match_count, int query_w, int query_h, int target_w, int target_h, int min_inliers, double reproj, float scale)
{
	XFeatLocation* loc;
	unsigned char* best_mask;
	unsigned char* mask;
	double best_affine[6];
	double affine[6];
	double refined[6];
	double best_error = 1.0e9;
	int best_inliers = 0;
	int iter;
	int max_iter;
	unsigned int seed;
	if ( match_count < min_inliers || match_count < 3 ) return NULL;
	loc = (XFeatLocation*)calloc(1, sizeof(*loc));
	best_mask = (unsigned char*)calloc((size_t)match_count, 1u);
	mask = (unsigned char*)calloc((size_t)match_count, 1u);
	if ( loc == NULL || best_mask == NULL || mask == NULL ) {
		free(loc);
		free(best_mask);
		free(mask);
		xfeat_set_error("out of memory");
		return NULL;
	}
	max_iter = match_count < 50 ? 600 : 2000;
	seed = (unsigned int)(2166136261u ^ (unsigned int)match_count ^ ((unsigned int)query_w << 16));
	for ( iter = 0; iter < max_iter; ++iter ) {
		int i0 = (int)(xfeat_lcg(&seed) % (unsigned int)match_count);
		int i1 = (int)(xfeat_lcg(&seed) % (unsigned int)match_count);
		int i2 = (int)(xfeat_lcg(&seed) % (unsigned int)match_count);
		double err = 0.0;
		int inliers;
		if ( i0 == i1 || i0 == i2 || i1 == i2 ) continue;
		if ( !xfeat_fit_affine3(src, dst, i0, i1, i2, affine) ) continue;
		inliers = xfeat_eval_affine(src, dst, match_count, affine, reproj, mask, &err);
		if ( inliers > best_inliers || (inliers == best_inliers && err < best_error) ) {
			best_inliers = inliers;
			best_error = err;
			memcpy(best_affine, affine, sizeof(best_affine));
			memcpy(best_mask, mask, (size_t)match_count);
		}
	}
	if ( best_inliers >= min_inliers && xfeat_fit_affine_ls(src, dst, best_mask, match_count, refined) ) {
		best_inliers = xfeat_eval_affine(src, dst, match_count, refined, reproj, best_mask, &best_error);
		memcpy(best_affine, refined, sizeof(best_affine));
	}
	loc->matches = match_count;
	loc->inliers = best_inliers;
	loc->reprojection_error = best_error;
	loc->scale = scale;
	loc->method = "affine";
	if ( best_inliers >= min_inliers ) {
		xfeat_compute_polygon_bbox(loc, best_affine, query_w, query_h, target_w, target_h);
		if ( xfeat_polygon_reasonable(loc, target_w, target_h) ) {
			loc->found = 1;
			loc->score = (double)best_inliers * 100.0 - best_error + (double)match_count * 0.05;
		} else {
			loc->score = -1000000000.0 + (double)match_count * 0.05;
		}
	} else {
		loc->score = -1000000000.0 + (double)match_count * 0.05;
	}
	free(best_mask);
	free(mask);
	return loc;
}

XL_EXPORT void* xfeat_match_native(void* query_handle, void* target_handle, double min_cossim, int min_inliers, double ransac_reproj, double scale)
{
	XFeatFeatures* q = (XFeatFeatures*)query_handle;
	XFeatFeatures* t = (XFeatFeatures*)target_handle;
	int* best12 = NULL;
	int* best21 = NULL;
	float* best12_score = NULL;
	float* best21_score = NULL;
	double* src = NULL;
	double* dst = NULL;
	int matches = 0;
	int qi;
	int ti;
	XFeatLocation* loc;
	if ( q == NULL || t == NULL || q->count <= 0 || t->count <= 0 ) {
		xfeat_set_error("empty XFeat features");
		return NULL;
	}
	best12 = (int*)malloc(sizeof(int) * (size_t)q->count);
	best21 = (int*)malloc(sizeof(int) * (size_t)t->count);
	best12_score = (float*)malloc(sizeof(float) * (size_t)q->count);
	best21_score = (float*)malloc(sizeof(float) * (size_t)t->count);
	src = (double*)malloc(sizeof(double) * (size_t)q->count * 2u);
	dst = (double*)malloc(sizeof(double) * (size_t)q->count * 2u);
	if ( best12 == NULL || best21 == NULL || best12_score == NULL || best21_score == NULL || src == NULL || dst == NULL ) {
		free(best12); free(best21); free(best12_score); free(best21_score); free(src); free(dst);
		xfeat_set_error("out of memory");
		return NULL;
	}
	for ( qi = 0; qi < q->count; ++qi ) {
		best12[qi] = -1;
		best12_score[qi] = -2.0f;
	}
	for ( ti = 0; ti < t->count; ++ti ) {
		best21[ti] = -1;
		best21_score[ti] = -2.0f;
	}
	for ( qi = 0; qi < q->count; ++qi ) {
		const float* qd = q->desc + qi * XFEAT_DESC_DIM;
		for ( ti = 0; ti < t->count; ++ti ) {
			const float* td = t->desc + ti * XFEAT_DESC_DIM;
			float dot = xfeat_dot64(qd, td);
			if ( dot > best12_score[qi] ) {
				best12_score[qi] = dot;
				best12[qi] = ti;
			}
			if ( dot > best21_score[ti] ) {
				best21_score[ti] = dot;
				best21[ti] = qi;
			}
		}
	}
	for ( qi = 0; qi < q->count; ++qi ) {
		ti = best12[qi];
		if ( ti >= 0 && ti < t->count && best21[ti] == qi && (double)best12_score[qi] > min_cossim ) {
			src[matches * 2] = q->xy[qi * 2];
			src[matches * 2 + 1] = q->xy[qi * 2 + 1];
			dst[matches * 2] = t->xy[ti * 2];
			dst[matches * 2 + 1] = t->xy[ti * 2 + 1];
			matches++;
		}
	}
	loc = xfeat_estimate_affine(src, dst, matches, q->image_w, q->image_h, t->image_w, t->image_h, min_inliers, ransac_reproj, (float)scale);
	if ( loc == NULL ) {
		loc = (XFeatLocation*)calloc(1, sizeof(*loc));
		if ( loc != NULL ) {
			loc->matches = matches;
			loc->scale = (float)scale;
			loc->score = -1000000000.0 + (double)matches * 0.05;
			loc->method = "none";
		}
	}
	free(best12);
	free(best21);
	free(best12_score);
	free(best21_score);
	free(src);
	free(dst);
	xfeat_set_error("");
	return loc;
}

XL_EXPORT int xfeat_location_release_native(void* handle)
{
	free(handle);
	return 1;
}

XL_EXPORT int xfeat_location_found_native(void* handle)
{
	XFeatLocation* loc = (XFeatLocation*)handle;
	return loc != NULL && loc->found ? 1 : 0;
}

XL_EXPORT double xfeat_location_score_native(void* handle)
{
	XFeatLocation* loc = (XFeatLocation*)handle;
	return loc != NULL ? loc->score : -1000000000.0;
}

XL_EXPORT int xfeat_location_bbox_x_native(void* handle)
{
	XFeatLocation* loc = (XFeatLocation*)handle;
	return loc != NULL ? loc->bbox[0] : 0;
}

XL_EXPORT int xfeat_location_bbox_y_native(void* handle)
{
	XFeatLocation* loc = (XFeatLocation*)handle;
	return loc != NULL ? loc->bbox[1] : 0;
}

XL_EXPORT int xfeat_location_bbox_w_native(void* handle)
{
	XFeatLocation* loc = (XFeatLocation*)handle;
	return loc != NULL ? loc->bbox[2] : 0;
}

XL_EXPORT int xfeat_location_bbox_h_native(void* handle)
{
	XFeatLocation* loc = (XFeatLocation*)handle;
	return loc != NULL ? loc->bbox[3] : 0;
}

XL_EXPORT const char* xfeat_location_json_native(void* handle, const char* image_name, const char* output_name)
{
	XFeatLocation* loc = (XFeatLocation*)handle;
	size_t used = 0;
	if ( !xfeat_json_reserve(2048u) ) return "{}";
	xfeat_append(g_json, g_json_cap, &used, "{\"image\":\"");
	xfeat_json_escape(g_json, g_json_cap, &used, image_name != NULL ? image_name : "");
	xfeat_append(g_json, g_json_cap, &used, "\",\"output\":\"");
	xfeat_json_escape(g_json, g_json_cap, &used, output_name != NULL ? output_name : "");
	xfeat_append(g_json, g_json_cap, &used, "\"");
	if ( loc == NULL ) {
		xfeat_append(g_json, g_json_cap, &used, ",\"found\":false,\"matches\":0}");
		return g_json;
	}
	if ( !loc->found ) {
		xfeat_appendf(g_json, g_json_cap, &used, ",\"found\":false,\"matches\":%d,\"scale\":%.6g}", loc->matches, (double)loc->scale);
		return g_json;
	}
	xfeat_appendf(
		g_json,
		g_json_cap,
		&used,
		",\"found\":true,\"method\":\"%s\",\"scale\":%.6g,\"matches\":%d,\"inliers\":%d,\"reprojection_error\":%.9g,\"score\":%.9g,",
		loc->method != NULL ? loc->method : "affine",
		(double)loc->scale,
		loc->matches,
		loc->inliers,
		loc->reprojection_error,
		loc->score
	);
	xfeat_append(g_json, g_json_cap, &used, "\"polygon\":[");
	xfeat_appendf(g_json, g_json_cap, &used, "[%.6g,%.6g],[%.6g,%.6g],[%.6g,%.6g],[%.6g,%.6g]",
		loc->polygon[0], loc->polygon[1], loc->polygon[2], loc->polygon[3],
		loc->polygon[4], loc->polygon[5], loc->polygon[6], loc->polygon[7]);
	xfeat_appendf(g_json, g_json_cap, &used, "],\"bbox_xywh\":[%d,%d,%d,%d]}",
		loc->bbox[0], loc->bbox[1], loc->bbox[2], loc->bbox[3]);
	return g_json;
}
