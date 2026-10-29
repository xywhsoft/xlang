#if !defined(_WIN32) && !defined(_WIN64)
#define _POSIX_C_SOURCE 200809L
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#ifndef WC_ERR_INVALID_CHARS
#define WC_ERR_INVALID_CHARS 0x00000080
#endif
#else
#include <dlfcn.h>
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xrt.h"
#include "../include/onnxruntime.h"

#if defined(_WIN32) || defined(_WIN64)
#define XL_EXPORT __declspec(dllexport)
#else
#define XL_EXPORT
#endif

typedef const char* (*OrtTextFn)(void);
typedef void* (*OrtCreateSessionOptionsFn)(void);
typedef void (*OrtReleasePtrFn)(void*);
typedef int (*OrtSetOptionIntFn)(void*, int);
typedef int (*OrtOptionFlagFn)(void*);
typedef int (*OrtSetOptionTextFn)(void*, const char*);
typedef int (*OrtSetProviderOptionsFn)(void*, const char*, const char*);
typedef void* (*OrtCreateSessionFn)(const char*, void*);
typedef const char* (*OrtSessionTextFn)(void*);
typedef void* (*OrtRunJsonFn)(void*, const char*, const char*);
typedef void* (*OrtRunJsonWithOptionsFn)(void*, const char*, const char*, void*);
typedef void* (*OrtTensorZerosFn)(const char*);
typedef void* (*OrtTensorFromF32Fn)(const float*, int, const long long*, int);
typedef void* (*OrtTensorAdoptF32Fn)(float*, int, const long long*, int);
typedef void* (*OrtRunTensorFn)(void*, const char*, void*, const char*);
typedef const char* (*OrtResultTextFn)(void*);
typedef int (*OrtResultIntFn)(void*);
typedef float (*OrtResultFloatAtFn)(void*, int);
typedef void* (*OrtCreateIoBindingFn)(void*);
typedef int (*CvToChwTensorF32Fn)(void*, double, double, double, double, double, double, double, float**, long long*, int*);
typedef int (*CvToChwTensorF32ResizedFn)(void*, int, int, double, double, double, double, double, double, double, float**, long long*, int*);
typedef void (*CvReleaseTensorF32Fn)(float*);
typedef int (*OrtIoBindingBindInputFn)(void*, const char*, const char*);
typedef int (*OrtIoBindingBindOutputFn)(void*, const char*);
typedef void* (*OrtRunIoBindingFn)(void*, void*, void*);

typedef struct OrtApi {
	int loaded;
#if defined(_WIN32) || defined(_WIN64)
	HMODULE module;
#else
	void* module;
#endif
	OrtTextFn version;
	OrtTextFn providers;
	OrtTextFn last_error;
	OrtCreateSessionOptionsFn create_options;
	OrtReleasePtrFn release_options;
	OrtSetOptionIntFn set_intra_threads;
	OrtSetOptionIntFn set_inter_threads;
	OrtSetOptionIntFn set_graph_optimization_level;
	OrtSetOptionIntFn set_execution_mode;
	OrtSetOptionTextFn enable_profiling;
	OrtOptionFlagFn disable_profiling;
	OrtOptionFlagFn enable_cpu_mem_arena;
	OrtOptionFlagFn disable_cpu_mem_arena;
	OrtOptionFlagFn enable_mem_pattern;
	OrtOptionFlagFn disable_mem_pattern;
	OrtSetOptionTextFn set_session_log_id;
	OrtSetOptionIntFn set_session_log_verbosity_level;
	OrtSetOptionIntFn set_session_log_severity_level;
	OrtSetOptionTextFn append_execution_provider;
	OrtSetProviderOptionsFn append_execution_provider_with_options;
	OrtCreateSessionOptionsFn create_run_options;
	OrtReleasePtrFn release_run_options;
	OrtSetOptionIntFn set_run_log_verbosity_level;
	OrtSetOptionIntFn set_run_log_severity_level;
	OrtSetOptionTextFn set_run_tag;
	OrtOptionFlagFn set_run_terminate;
	OrtOptionFlagFn unset_run_terminate;
	OrtCreateSessionFn create_session;
	OrtReleasePtrFn release_session;
	OrtSessionTextFn inputs;
	OrtSessionTextFn outputs;
	OrtSessionTextFn modelmeta;
	OrtSessionTextFn end_profiling;
	OrtCreateIoBindingFn create_iobinding;
	OrtIoBindingBindInputFn iobinding_bind_input;
	OrtIoBindingBindOutputFn iobinding_bind_output;
	OrtOptionFlagFn iobinding_clear_inputs;
	OrtOptionFlagFn iobinding_clear_outputs;
	OrtOptionFlagFn iobinding_synchronize_inputs;
	OrtOptionFlagFn iobinding_synchronize_outputs;
	OrtRunIoBindingFn run_with_iobinding;
	OrtSessionTextFn iobinding_outputs;
	OrtReleasePtrFn release_iobinding;
	OrtRunJsonFn run_json;
	OrtRunJsonWithOptionsFn run_json_with_options;
	OrtTensorFromF32Fn tensor_from_f32;
	OrtTensorAdoptF32Fn tensor_adopt_f32;
	OrtTensorZerosFn tensor_zeros_f32;
	OrtReleasePtrFn release_tensor;
	OrtRunTensorFn run_tensor;
	OrtRunTensorFn run_tensor_fast;
	OrtResultTextFn result_json;
	OrtResultTextFn result_float_shape_json;
	OrtResultIntFn result_float_count;
	OrtResultFloatAtFn result_float_at;
	OrtReleasePtrFn release_result;
	char path[4096];
} OrtApi;

static OrtApi g_ort;
static xonce g_ort_once = XRT_ONCE_INIT;
static xonce g_error_once = XRT_ONCE_INIT;
static xthreadkey* g_error_key;

typedef struct CvBridgeApi {
	int loaded;
#if defined(_WIN32) || defined(_WIN64)
	HMODULE module;
#else
	void* module;
#endif
	CvToChwTensorF32Fn to_chw_tensor_f32;
	CvToChwTensorF32ResizedFn to_chw_tensor_f32_resized;
	CvReleaseTensorF32Fn release_tensor_f32;
} CvBridgeApi;

static CvBridgeApi g_cv_bridge;
static xonce g_cv_bridge_once = XRT_ONCE_INIT;

static void ort_copy(char* out, size_t out_size, const char* text)
{
	size_t len;
	if ( out == NULL || out_size == 0u ) return;
	if ( text == NULL ) text = "";
	len = strlen(text);
	if ( len >= out_size ) len = out_size - 1u;
	memcpy(out, text, len);
	out[len] = '\0';
}

static bool ort_error_key_init(ptr unused)
{
	(void)unused;
	g_error_key = xrtThreadKeyCreate(xrtFree);
	return g_error_key != NULL;
}

static char* ort_error_buffer(int create)
{
	char* buffer;
	if ( !xrtOnce(&g_error_once, ort_error_key_init, NULL) ) return NULL;
	buffer = (char*)xrtThreadKeyGet(g_error_key);
	if ( buffer != NULL || !create ) return buffer;
	buffer = (char*)xrtMalloc(512u);
	if ( buffer == NULL ) return NULL;
	buffer[0] = '\0';
	if ( !xrtThreadKeySet(g_error_key, buffer) ) {
		xrtFree(buffer);
		return NULL;
	}
	return buffer;
}

static void ort_set_error(const char* text)
{
	char* buffer = ort_error_buffer(text != NULL && text[0] != '\0');
	if ( buffer != NULL ) ort_copy(buffer, 512u, text);
}

static int ort_join_path(char* out, size_t out_size, const char* a, const char* b)
{
	const char* sep;
	size_t a_len;
	int written;
	if ( out == NULL || out_size == 0u || a == NULL || b == NULL ) return 0;
	a_len = strlen(a);
	sep = (a_len > 0u && (a[a_len - 1u] == '\\' || a[a_len - 1u] == '/')) ? "" : "\\";
	written = snprintf(out, out_size, "%s%s%s", a, sep, b);
	return written > 0 && (size_t)written < out_size;
}

static void ort_dir_name(char* out, size_t out_size, const char* path)
{
	const char* p;
	const char* last_sep = NULL;
	size_t len;
	if ( out == NULL || out_size == 0u ) return;
	out[0] = '\0';
	if ( path == NULL || path[0] == '\0' ) return;
	for ( p = path; *p != '\0'; ++p ) {
		if ( *p == '\\' || *p == '/' || *p == ':' ) last_sep = p;
	}
	if ( last_sep == NULL ) return;
	len = (size_t)(last_sep - path);
	if ( *last_sep == ':' ) len += 1u;
	if ( len >= out_size ) len = out_size - 1u;
	memcpy(out, path, len);
	out[len] = '\0';
}

static int ort_exe_dir(char* out, size_t out_size)
{
	char path[4096];
	if ( out == NULL || out_size == 0u ) return 0;
	out[0] = '\0';
#if defined(_WIN32) || defined(_WIN64)
	{
		wchar_t wide[1024];
		DWORD len = GetModuleFileNameW(NULL, wide, (DWORD)(sizeof(wide) / sizeof(wide[0])));
		int bytes;
		if ( len == 0 || len >= sizeof(wide) / sizeof(wide[0]) ) return 0;
		bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, (int)len,
			path, (int)sizeof(path) - 1, NULL, NULL);
		if ( bytes <= 0 ) return 0;
		path[bytes] = '\0';
	}
#else
	{
		ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1u);
		if ( len <= 0 || (size_t)len >= sizeof(path) ) return 0;
		path[len] = '\0';
	}
#endif
	ort_dir_name(out, out_size, path);
	return out[0] != '\0';
}

static void* ort_sym(const char* name)
{
#if defined(_WIN32) || defined(_WIN64)
	return g_ort.module != NULL ? (void*)GetProcAddress(g_ort.module, name) : NULL;
#else
	return g_ort.module != NULL ? dlsym(g_ort.module, name) : NULL;
#endif
}

#if defined(_WIN32) || defined(_WIN64)
/* 仅在 Windows 装载边界做 UTF-8/UTF-16 转换；无效 UTF-8 直接拒绝。 */
static HMODULE ort_win_load_library(const char* path)
{
	wchar_t* wide;
	HMODULE module;
	int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
	if ( count <= 0 ) return NULL;
	wide = (wchar_t*)malloc((size_t)count * sizeof(wchar_t));
	if ( wide == NULL ) return NULL;
	if ( MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, count) != count ) {
		free(wide);
		return NULL;
	}
	module = LoadLibraryExW(wide, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
	free(wide);
	return module;
}

static char* ort_win_env_path(const wchar_t* name)
{
	wchar_t* wide;
	char* utf8;
	DWORD count = GetEnvironmentVariableW(name, NULL, 0);
	int bytes;
	if ( count == 0 || count > 32768 ) return NULL;
	wide = (wchar_t*)malloc((size_t)count * sizeof(wchar_t));
	if ( wide == NULL ) return NULL;
	if ( GetEnvironmentVariableW(name, wide, count) != count - 1 ) {
		free(wide);
		return NULL;
	}
	bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1, NULL, 0, NULL, NULL);
	if ( bytes <= 0 ) { free(wide); return NULL; }
	utf8 = (char*)malloc((size_t)bytes);
	if ( utf8 == NULL || WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
		wide, -1, utf8, bytes, NULL, NULL) != bytes ) {
		free(wide);
		free(utf8);
		return NULL;
	}
	free(wide);
	return utf8;
}
#endif

static int ort_try_load(const char* path)
{
	if ( path == NULL || path[0] == '\0' ) return 0;
#if defined(_WIN32) || defined(_WIN64)
	g_ort.module = ort_win_load_library(path);
#else
	g_ort.module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
	if ( g_ort.module == NULL ) return 0;

	g_ort.version = (OrtTextFn)ort_sym("onnxruntime_xl_get_version_string");
	g_ort.providers = (OrtTextFn)ort_sym("onnxruntime_xl_get_available_providers_json");
	g_ort.last_error = (OrtTextFn)ort_sym("onnxruntime_xl_last_error");
	g_ort.create_options = (OrtCreateSessionOptionsFn)ort_sym("onnxruntime_xl_create_session_options");
	g_ort.release_options = (OrtReleasePtrFn)ort_sym("onnxruntime_xl_release_session_options");
	g_ort.set_intra_threads = (OrtSetOptionIntFn)ort_sym("onnxruntime_xl_session_options_set_intra_op_num_threads");
	g_ort.set_inter_threads = (OrtSetOptionIntFn)ort_sym("onnxruntime_xl_session_options_set_inter_op_num_threads");
	g_ort.set_graph_optimization_level = (OrtSetOptionIntFn)ort_sym("onnxruntime_xl_session_options_set_graph_optimization_level");
	g_ort.set_execution_mode = (OrtSetOptionIntFn)ort_sym("onnxruntime_xl_session_options_set_execution_mode");
	g_ort.enable_profiling = (OrtSetOptionTextFn)ort_sym("onnxruntime_xl_session_options_enable_profiling");
	g_ort.disable_profiling = (OrtOptionFlagFn)ort_sym("onnxruntime_xl_session_options_disable_profiling");
	g_ort.enable_cpu_mem_arena = (OrtOptionFlagFn)ort_sym("onnxruntime_xl_session_options_enable_cpu_mem_arena");
	g_ort.disable_cpu_mem_arena = (OrtOptionFlagFn)ort_sym("onnxruntime_xl_session_options_disable_cpu_mem_arena");
	g_ort.enable_mem_pattern = (OrtOptionFlagFn)ort_sym("onnxruntime_xl_session_options_enable_mem_pattern");
	g_ort.disable_mem_pattern = (OrtOptionFlagFn)ort_sym("onnxruntime_xl_session_options_disable_mem_pattern");
	g_ort.set_session_log_id = (OrtSetOptionTextFn)ort_sym("onnxruntime_xl_session_options_set_log_id");
	g_ort.set_session_log_verbosity_level = (OrtSetOptionIntFn)ort_sym("onnxruntime_xl_session_options_set_log_verbosity_level");
	g_ort.set_session_log_severity_level = (OrtSetOptionIntFn)ort_sym("onnxruntime_xl_session_options_set_log_severity_level");
	g_ort.append_execution_provider = (OrtSetOptionTextFn)ort_sym("onnxruntime_xl_session_options_append_execution_provider");
	g_ort.append_execution_provider_with_options = (OrtSetProviderOptionsFn)ort_sym("onnxruntime_xl_session_options_append_execution_provider_with_options");
	g_ort.create_run_options = (OrtCreateSessionOptionsFn)ort_sym("onnxruntime_xl_create_run_options");
	g_ort.release_run_options = (OrtReleasePtrFn)ort_sym("onnxruntime_xl_release_run_options");
	g_ort.set_run_log_verbosity_level = (OrtSetOptionIntFn)ort_sym("onnxruntime_xl_run_options_set_log_verbosity_level");
	g_ort.set_run_log_severity_level = (OrtSetOptionIntFn)ort_sym("onnxruntime_xl_run_options_set_log_severity_level");
	g_ort.set_run_tag = (OrtSetOptionTextFn)ort_sym("onnxruntime_xl_run_options_set_run_tag");
	g_ort.set_run_terminate = (OrtOptionFlagFn)ort_sym("onnxruntime_xl_run_options_set_terminate");
	g_ort.unset_run_terminate = (OrtOptionFlagFn)ort_sym("onnxruntime_xl_run_options_unset_terminate");
	g_ort.create_session = (OrtCreateSessionFn)ort_sym("onnxruntime_xl_create_session");
	g_ort.release_session = (OrtReleasePtrFn)ort_sym("onnxruntime_xl_release_session");
	g_ort.inputs = (OrtSessionTextFn)ort_sym("onnxruntime_xl_get_inputs_json");
	g_ort.outputs = (OrtSessionTextFn)ort_sym("onnxruntime_xl_get_outputs_json");
	g_ort.modelmeta = (OrtSessionTextFn)ort_sym("onnxruntime_xl_get_modelmeta_json");
	g_ort.end_profiling = (OrtSessionTextFn)ort_sym("onnxruntime_xl_session_end_profiling");
	g_ort.create_iobinding = (OrtCreateIoBindingFn)ort_sym("onnxruntime_xl_create_iobinding");
	g_ort.iobinding_bind_input = (OrtIoBindingBindInputFn)ort_sym("onnxruntime_xl_iobinding_bind_cpu_input_json");
	g_ort.iobinding_bind_output = (OrtIoBindingBindOutputFn)ort_sym("onnxruntime_xl_iobinding_bind_output");
	g_ort.iobinding_clear_inputs = (OrtOptionFlagFn)ort_sym("onnxruntime_xl_iobinding_clear_inputs");
	g_ort.iobinding_clear_outputs = (OrtOptionFlagFn)ort_sym("onnxruntime_xl_iobinding_clear_outputs");
	g_ort.iobinding_synchronize_inputs = (OrtOptionFlagFn)ort_sym("onnxruntime_xl_iobinding_synchronize_inputs");
	g_ort.iobinding_synchronize_outputs = (OrtOptionFlagFn)ort_sym("onnxruntime_xl_iobinding_synchronize_outputs");
	g_ort.run_with_iobinding = (OrtRunIoBindingFn)ort_sym("onnxruntime_xl_run_with_iobinding");
	g_ort.iobinding_outputs = (OrtSessionTextFn)ort_sym("onnxruntime_xl_iobinding_outputs_json");
	g_ort.release_iobinding = (OrtReleasePtrFn)ort_sym("onnxruntime_xl_release_iobinding");
	g_ort.run_json = (OrtRunJsonFn)ort_sym("onnxruntime_xl_run_json");
	g_ort.run_json_with_options = (OrtRunJsonWithOptionsFn)ort_sym("onnxruntime_xl_run_json_with_options");
	g_ort.tensor_from_f32 = (OrtTensorFromF32Fn)ort_sym("onnxruntime_xl_tensor_from_f32");
	g_ort.tensor_adopt_f32 = (OrtTensorAdoptF32Fn)ort_sym("onnxruntime_xl_tensor_adopt_f32");
	g_ort.tensor_zeros_f32 = (OrtTensorZerosFn)ort_sym("onnxruntime_xl_tensor_zeros_f32");
	g_ort.release_tensor = (OrtReleasePtrFn)ort_sym("onnxruntime_xl_release_tensor");
	g_ort.run_tensor = (OrtRunTensorFn)ort_sym("onnxruntime_xl_run_tensor");
	g_ort.run_tensor_fast = (OrtRunTensorFn)ort_sym("onnxruntime_xl_run_tensor_fast");
	g_ort.result_json = (OrtResultTextFn)ort_sym("onnxruntime_xl_result_json");
	g_ort.result_float_shape_json = (OrtResultTextFn)ort_sym("onnxruntime_xl_result_float_shape_json");
	g_ort.result_float_count = (OrtResultIntFn)ort_sym("onnxruntime_xl_result_float_count");
	g_ort.result_float_at = (OrtResultFloatAtFn)ort_sym("onnxruntime_xl_result_float_at");
	g_ort.release_result = (OrtReleasePtrFn)ort_sym("onnxruntime_xl_release_result");
	#define ORT_REQUIRE(field) do { \
		if ( g_ort.field == NULL ) { \
			ort_set_error("onnxruntime shim missing required symbols: " #field); \
			goto missing_symbol; \
		} \
	} while (0)
	ORT_REQUIRE(version);
	ORT_REQUIRE(providers);
	ORT_REQUIRE(last_error);
	ORT_REQUIRE(create_options);
	ORT_REQUIRE(release_options);
	ORT_REQUIRE(set_intra_threads);
	ORT_REQUIRE(set_inter_threads);
	ORT_REQUIRE(set_graph_optimization_level);
	ORT_REQUIRE(set_execution_mode);
	ORT_REQUIRE(enable_profiling);
	ORT_REQUIRE(disable_profiling);
	ORT_REQUIRE(enable_cpu_mem_arena);
	ORT_REQUIRE(disable_cpu_mem_arena);
	ORT_REQUIRE(enable_mem_pattern);
	ORT_REQUIRE(disable_mem_pattern);
	ORT_REQUIRE(set_session_log_id);
	ORT_REQUIRE(set_session_log_verbosity_level);
	ORT_REQUIRE(set_session_log_severity_level);
	ORT_REQUIRE(append_execution_provider);
	ORT_REQUIRE(append_execution_provider_with_options);
	ORT_REQUIRE(create_run_options);
	ORT_REQUIRE(release_run_options);
	ORT_REQUIRE(set_run_log_verbosity_level);
	ORT_REQUIRE(set_run_log_severity_level);
	ORT_REQUIRE(set_run_tag);
	ORT_REQUIRE(set_run_terminate);
	ORT_REQUIRE(unset_run_terminate);
	ORT_REQUIRE(create_session);
	ORT_REQUIRE(release_session);
	ORT_REQUIRE(inputs);
	ORT_REQUIRE(outputs);
	ORT_REQUIRE(modelmeta);
	ORT_REQUIRE(end_profiling);
	ORT_REQUIRE(create_iobinding);
	ORT_REQUIRE(iobinding_bind_input);
	ORT_REQUIRE(iobinding_bind_output);
	ORT_REQUIRE(iobinding_clear_inputs);
	ORT_REQUIRE(iobinding_clear_outputs);
	ORT_REQUIRE(iobinding_synchronize_inputs);
	ORT_REQUIRE(iobinding_synchronize_outputs);
	ORT_REQUIRE(run_with_iobinding);
	ORT_REQUIRE(iobinding_outputs);
	ORT_REQUIRE(release_iobinding);
	ORT_REQUIRE(run_json);
	ORT_REQUIRE(run_json_with_options);
	ORT_REQUIRE(tensor_from_f32);
	ORT_REQUIRE(tensor_adopt_f32);
	ORT_REQUIRE(tensor_zeros_f32);
	ORT_REQUIRE(release_tensor);
	ORT_REQUIRE(run_tensor);
	ORT_REQUIRE(run_tensor_fast);
	ORT_REQUIRE(result_json);
	ORT_REQUIRE(result_float_shape_json);
	ORT_REQUIRE(result_float_count);
	ORT_REQUIRE(result_float_at);
	ORT_REQUIRE(release_result);
	#undef ORT_REQUIRE
	g_ort.loaded = 1;
	ort_copy(g_ort.path, sizeof(g_ort.path), path);
	ort_set_error("");
	return 1;

missing_symbol:
	#if defined(_WIN32) || defined(_WIN64)
	FreeLibrary(g_ort.module);
	#else
	dlclose(g_ort.module);
	#endif
	g_ort.module = NULL;
	return 0;
}

static int ort_try_load_from_exe(const char* relative)
{
	char exe_dir[4096];
	char path[4096];
	if ( !ort_exe_dir(exe_dir, sizeof(exe_dir)) ) return 0;
	if ( !ort_join_path(path, sizeof(path), exe_dir, relative) ) return 0;
	return ort_try_load(path);
}

static void* cv_bridge_sym(const char* name)
{
#if defined(_WIN32) || defined(_WIN64)
	return g_cv_bridge.module != NULL ? (void*)GetProcAddress(g_cv_bridge.module, name) : NULL;
#else
	return g_cv_bridge.module != NULL ? dlsym(g_cv_bridge.module, name) : NULL;
#endif
}

static int cv_bridge_try_load(const char* path)
{
	if ( path == NULL || path[0] == '\0' ) return 0;
#if defined(_WIN32) || defined(_WIN64)
	g_cv_bridge.module = ort_win_load_library(path);
#else
	g_cv_bridge.module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
	if ( g_cv_bridge.module == NULL ) return 0;
	g_cv_bridge.to_chw_tensor_f32 = (CvToChwTensorF32Fn)cv_bridge_sym("opencv_xl_to_chw_tensor_f32");
	g_cv_bridge.to_chw_tensor_f32_resized = (CvToChwTensorF32ResizedFn)cv_bridge_sym("opencv_xl_to_chw_tensor_f32_resized");
	g_cv_bridge.release_tensor_f32 = (CvReleaseTensorF32Fn)cv_bridge_sym("opencv_xl_release_tensor_f32");
	if ( g_cv_bridge.to_chw_tensor_f32 == NULL || g_cv_bridge.release_tensor_f32 == NULL ) {
		#if defined(_WIN32) || defined(_WIN64)
		FreeLibrary(g_cv_bridge.module);
		#else
		dlclose(g_cv_bridge.module);
		#endif
		g_cv_bridge.module = NULL;
		return 0;
	}
	g_cv_bridge.loaded = 1;
	return 1;
}

static int cv_bridge_try_load_from_exe(const char* relative)
{
	char exe_dir[4096];
	char path[4096];
	if ( !ort_exe_dir(exe_dir, sizeof(exe_dir)) ) return 0;
	if ( !ort_join_path(path, sizeof(path), exe_dir, relative) ) return 0;
	return cv_bridge_try_load(path);
}

/* OpenCV 辅助桥接与主 ONNX 桥接各自完成一次原子装载。 */
static bool cv_bridge_load_once(ptr unused)
{
	int env_loaded;
	(void)unused;
	{
#if defined(_WIN32) || defined(_WIN64)
		char* env_path = ort_win_env_path(L"OPENCV_XLANG_DLL");
#else
		const char* env_path = getenv("OPENCV_XLANG_DLL");
#endif
		env_loaded = env_path != NULL && env_path[0] != '\0' && cv_bridge_try_load(env_path);
#if defined(_WIN32) || defined(_WIN64)
		free(env_path);
#endif
	}
	if ( env_loaded ) return true;
	if ( cv_bridge_try_load_from_exe("extlibs\\opencv\\bin\\opencv_xlang.dll") ) return true;
	if ( cv_bridge_try_load("extlibs\\opencv\\bin\\opencv_xlang.dll") ) return true;
	ort_set_error("opencv_xlang.dll with tensor bridge symbols was not found");
	return true;
}

static int cv_bridge_load(void)
{
	if ( xrtOnce(&g_cv_bridge_once, cv_bridge_load_once, NULL) && g_cv_bridge.loaded ) return 1;
	ort_set_error("opencv_xlang.dll with tensor bridge symbols was not found");
	return 0;
}

static bool ort_load_once(ptr unused)
{
	(void)unused;
	ort_set_error("");
	{
#if defined(_WIN32) || defined(_WIN64)
		char* env_path = ort_win_env_path(L"ONNXRUNTIME_XLANG_DLL");
#else
		const char* env_path = getenv("ONNXRUNTIME_XLANG_DLL");
#endif
		if ( env_path != NULL && env_path[0] != '\0' ) {
			if ( !ort_try_load(env_path) ) {
				char* error = ort_error_buffer(0);
				if ( error == NULL || error[0] == '\0' ) {
					ort_set_error("onnxruntime shim not found at ONNXRUNTIME_XLANG_DLL");
				}
			}
#if defined(_WIN32) || defined(_WIN64)
			free(env_path);
#endif
			return true;
		}
#if defined(_WIN32) || defined(_WIN64)
		free(env_path);
#endif
	}
	if ( ort_try_load_from_exe("extlibs\\onnxruntime\\bin\\onnxruntime_xlang.dll") ) return true;
	if ( ort_try_load("extlibs\\onnxruntime\\bin\\onnxruntime_xlang.dll") ) return true;
	{
		char* error = ort_error_buffer(0);
		if ( error == NULL || error[0] == '\0' ) {
			ort_set_error("onnxruntime shim not found; set ONNXRUNTIME_XLANG_DLL or place onnxruntime_xlang.dll under extlibs\\onnxruntime\\bin");
		}
	}
	return true;
}

static int ort_load(void)
{
	if ( xrtOnce(&g_ort_once, ort_load_once, NULL) && g_ort.loaded ) return 1;
	{
		char* error = ort_error_buffer(0);
		if ( error == NULL || error[0] == '\0' ) {
			ort_set_error("onnxruntime shim not found; set ONNXRUNTIME_XLANG_DLL or place onnxruntime_xlang.dll under extlibs\\onnxruntime\\bin");
		}
	}
	return 0;
}

XL_EXPORT int ort_available_native(void) { return ort_load(); }
XL_EXPORT const char* ort_backend_native(void) { return ort_load() ? g_ort.path : ""; }
XL_EXPORT const char* ort_get_version_string_native(void) { return (ort_load() && g_ort.version != NULL) ? g_ort.version() : ""; }
XL_EXPORT const char* ort_get_available_providers_json_native(void) { return (ort_load() && g_ort.providers != NULL) ? g_ort.providers() : "[]"; }
XL_EXPORT const char* ort_last_error_native(void)
{
	const char* backend = (ort_load() && g_ort.last_error != NULL) ? g_ort.last_error() : NULL;
	char* local = ort_error_buffer(0);
	if ( backend != NULL && backend[0] != '\0' ) return backend;
	return local != NULL ? local : "";
}
XL_EXPORT void* ort_session_options_create_native(void) { return (ort_load() && g_ort.create_options != NULL) ? g_ort.create_options() : NULL; }
XL_EXPORT int ort_session_options_set_intra_op_num_threads_native(void* handle, int value) { return (handle != NULL && ort_load() && g_ort.set_intra_threads != NULL) ? g_ort.set_intra_threads(handle, value) : 0; }
XL_EXPORT int ort_session_options_set_inter_op_num_threads_native(void* handle, int value) { return (handle != NULL && ort_load() && g_ort.set_inter_threads != NULL) ? g_ort.set_inter_threads(handle, value) : 0; }
XL_EXPORT int ort_session_options_set_graph_optimization_level_native(void* handle, int value) { return (handle != NULL && ort_load() && g_ort.set_graph_optimization_level != NULL) ? g_ort.set_graph_optimization_level(handle, value) : 0; }
XL_EXPORT int ort_session_options_set_execution_mode_native(void* handle, int value) { return (handle != NULL && ort_load() && g_ort.set_execution_mode != NULL) ? g_ort.set_execution_mode(handle, value) : 0; }
XL_EXPORT int ort_session_options_enable_profiling_native(void* handle, const char* value) { return (handle != NULL && ort_load() && g_ort.enable_profiling != NULL) ? g_ort.enable_profiling(handle, value) : 0; }
XL_EXPORT int ort_session_options_disable_profiling_native(void* handle) { return (handle != NULL && ort_load() && g_ort.disable_profiling != NULL) ? g_ort.disable_profiling(handle) : 0; }
XL_EXPORT int ort_session_options_enable_cpu_mem_arena_native(void* handle) { return (handle != NULL && ort_load() && g_ort.enable_cpu_mem_arena != NULL) ? g_ort.enable_cpu_mem_arena(handle) : 0; }
XL_EXPORT int ort_session_options_disable_cpu_mem_arena_native(void* handle) { return (handle != NULL && ort_load() && g_ort.disable_cpu_mem_arena != NULL) ? g_ort.disable_cpu_mem_arena(handle) : 0; }
XL_EXPORT int ort_session_options_enable_mem_pattern_native(void* handle) { return (handle != NULL && ort_load() && g_ort.enable_mem_pattern != NULL) ? g_ort.enable_mem_pattern(handle) : 0; }
XL_EXPORT int ort_session_options_disable_mem_pattern_native(void* handle) { return (handle != NULL && ort_load() && g_ort.disable_mem_pattern != NULL) ? g_ort.disable_mem_pattern(handle) : 0; }
XL_EXPORT int ort_session_options_set_log_id_native(void* handle, const char* value) { return (handle != NULL && ort_load() && g_ort.set_session_log_id != NULL) ? g_ort.set_session_log_id(handle, value) : 0; }
XL_EXPORT int ort_session_options_set_log_verbosity_level_native(void* handle, int value) { return (handle != NULL && ort_load() && g_ort.set_session_log_verbosity_level != NULL) ? g_ort.set_session_log_verbosity_level(handle, value) : 0; }
XL_EXPORT int ort_session_options_set_log_severity_level_native(void* handle, int value) { return (handle != NULL && ort_load() && g_ort.set_session_log_severity_level != NULL) ? g_ort.set_session_log_severity_level(handle, value) : 0; }
XL_EXPORT int ort_session_options_append_execution_provider_native(void* handle, const char* value) { return (handle != NULL && ort_load() && g_ort.append_execution_provider != NULL) ? g_ort.append_execution_provider(handle, value) : 0; }
XL_EXPORT int ort_session_options_append_execution_provider_with_options_native(void* handle, const char* value, const char* options_json) { return (handle != NULL && ort_load() && g_ort.append_execution_provider_with_options != NULL) ? g_ort.append_execution_provider_with_options(handle, value, options_json) : 0; }
XL_EXPORT int ort_session_options_release_native(void* handle) { if ( handle != NULL && ort_load() && g_ort.release_options != NULL ) g_ort.release_options(handle); return 1; }
XL_EXPORT void* ort_run_options_create_native(void) { return (ort_load() && g_ort.create_run_options != NULL) ? g_ort.create_run_options() : NULL; }
XL_EXPORT int ort_run_options_set_log_verbosity_level_native(void* handle, int value) { return (handle != NULL && ort_load() && g_ort.set_run_log_verbosity_level != NULL) ? g_ort.set_run_log_verbosity_level(handle, value) : 0; }
XL_EXPORT int ort_run_options_set_log_severity_level_native(void* handle, int value) { return (handle != NULL && ort_load() && g_ort.set_run_log_severity_level != NULL) ? g_ort.set_run_log_severity_level(handle, value) : 0; }
XL_EXPORT int ort_run_options_set_run_tag_native(void* handle, const char* value) { return (handle != NULL && ort_load() && g_ort.set_run_tag != NULL) ? g_ort.set_run_tag(handle, value) : 0; }
XL_EXPORT int ort_run_options_set_terminate_native(void* handle) { return (handle != NULL && ort_load() && g_ort.set_run_terminate != NULL) ? g_ort.set_run_terminate(handle) : 0; }
XL_EXPORT int ort_run_options_unset_terminate_native(void* handle) { return (handle != NULL && ort_load() && g_ort.unset_run_terminate != NULL) ? g_ort.unset_run_terminate(handle) : 0; }
XL_EXPORT int ort_run_options_release_native(void* handle) { if ( handle != NULL && ort_load() && g_ort.release_run_options != NULL ) g_ort.release_run_options(handle); return 1; }
XL_EXPORT void* ort_session_create_native(const char* model_path, void* options) { return (ort_load() && g_ort.create_session != NULL) ? g_ort.create_session(model_path, options) : NULL; }
XL_EXPORT int ort_session_release_native(void* handle) { if ( handle != NULL && ort_load() && g_ort.release_session != NULL ) g_ort.release_session(handle); return 1; }
XL_EXPORT const char* ort_session_inputs_native(void* handle) { return (handle != NULL && ort_load() && g_ort.inputs != NULL) ? g_ort.inputs(handle) : "[]"; }
XL_EXPORT const char* ort_session_outputs_native(void* handle) { return (handle != NULL && ort_load() && g_ort.outputs != NULL) ? g_ort.outputs(handle) : "[]"; }
XL_EXPORT const char* ort_session_modelmeta_native(void* handle) { return (handle != NULL && ort_load() && g_ort.modelmeta != NULL) ? g_ort.modelmeta(handle) : "{}"; }
XL_EXPORT const char* ort_session_end_profiling_native(void* handle) { return (handle != NULL && ort_load() && g_ort.end_profiling != NULL) ? g_ort.end_profiling(handle) : ""; }
XL_EXPORT void* ort_iobinding_create_native(void* session) { return (session != NULL && ort_load() && g_ort.create_iobinding != NULL) ? g_ort.create_iobinding(session) : NULL; }
XL_EXPORT int ort_iobinding_bind_cpu_input_native(void* binding, const char* name, const char* tensor_json) { return (binding != NULL && ort_load() && g_ort.iobinding_bind_input != NULL) ? g_ort.iobinding_bind_input(binding, name, tensor_json) : 0; }
XL_EXPORT int ort_iobinding_bind_output_native(void* binding, const char* name) { return (binding != NULL && ort_load() && g_ort.iobinding_bind_output != NULL) ? g_ort.iobinding_bind_output(binding, name) : 0; }
XL_EXPORT int ort_iobinding_clear_inputs_native(void* binding) { return (binding != NULL && ort_load() && g_ort.iobinding_clear_inputs != NULL) ? g_ort.iobinding_clear_inputs(binding) : 0; }
XL_EXPORT int ort_iobinding_clear_outputs_native(void* binding) { return (binding != NULL && ort_load() && g_ort.iobinding_clear_outputs != NULL) ? g_ort.iobinding_clear_outputs(binding) : 0; }
XL_EXPORT int ort_iobinding_synchronize_inputs_native(void* binding) { return (binding != NULL && ort_load() && g_ort.iobinding_synchronize_inputs != NULL) ? g_ort.iobinding_synchronize_inputs(binding) : 0; }
XL_EXPORT int ort_iobinding_synchronize_outputs_native(void* binding) { return (binding != NULL && ort_load() && g_ort.iobinding_synchronize_outputs != NULL) ? g_ort.iobinding_synchronize_outputs(binding) : 0; }
XL_EXPORT void* ort_session_run_with_iobinding_native(void* session, void* binding, void* options) { return (session != NULL && binding != NULL && ort_load() && g_ort.run_with_iobinding != NULL) ? g_ort.run_with_iobinding(session, binding, options) : NULL; }
XL_EXPORT const char* ort_iobinding_outputs_native(void* binding) { return (binding != NULL && ort_load() && g_ort.iobinding_outputs != NULL) ? g_ort.iobinding_outputs(binding) : "{}"; }
XL_EXPORT int ort_iobinding_release_native(void* binding) { if ( binding != NULL && ort_load() && g_ort.release_iobinding != NULL ) g_ort.release_iobinding(binding); return 1; }
XL_EXPORT void* ort_session_run_json_native(void* handle, const char* outputs, const char* inputs) { return (handle != NULL && ort_load() && g_ort.run_json != NULL) ? g_ort.run_json(handle, outputs, inputs) : NULL; }
XL_EXPORT void* ort_session_run_json_with_options_native(void* handle, const char* outputs, const char* inputs, void* options) { return (handle != NULL && ort_load() && g_ort.run_json_with_options != NULL) ? g_ort.run_json_with_options(handle, outputs, inputs, options) : NULL; }
XL_EXPORT void* ort_tensor_zeros_f32_native(const char* shape_json) { return (ort_load() && g_ort.tensor_zeros_f32 != NULL) ? g_ort.tensor_zeros_f32(shape_json) : NULL; }
XL_EXPORT void* ort_tensor_from_opencv_native(void* image, double scale, double mean0, double mean1, double mean2, double std0, double std1, double std2)
{
	float* data = NULL;
	long long shape[4] = {0, 0, 0, 0};
	int count = 0;
	void* tensor;
	if ( image == NULL ) {
		ort_set_error("invalid OpenCV image handle");
		return NULL;
	}
	if ( !ort_load() || g_ort.tensor_from_f32 == NULL ) {
		ort_set_error("onnxruntime tensor_from_f32 is unavailable");
		return NULL;
	}
	if ( !cv_bridge_load() || g_cv_bridge.to_chw_tensor_f32 == NULL || g_cv_bridge.release_tensor_f32 == NULL ) {
		return NULL;
	}
	if ( !g_cv_bridge.to_chw_tensor_f32(image, scale, mean0, mean1, mean2, std0, std1, std2, &data, shape, &count) || data == NULL || count <= 0 ) {
		ort_set_error("OpenCV image to CHW tensor conversion failed");
		return NULL;
	}
	if ( g_ort.tensor_adopt_f32 != NULL ) {
		tensor = g_ort.tensor_adopt_f32(data, count, shape, 4);
		if ( tensor != NULL ) data = NULL;
	} else {
		tensor = g_ort.tensor_from_f32(data, count, shape, 4);
	}
	if ( data != NULL ) g_cv_bridge.release_tensor_f32(data);
	return tensor;
}

XL_EXPORT void* ort_tensor_from_opencv_resized_native(void* image, int width, int height, double scale, double mean0, double mean1, double mean2, double std0, double std1, double std2)
{
	float* data = NULL;
	long long shape[4] = {0, 0, 0, 0};
	int count = 0;
	void* tensor;
	if ( image == NULL || width <= 0 || height <= 0 ) {
		ort_set_error("invalid OpenCV image handle or resize dimensions");
		return NULL;
	}
	if ( !ort_load() || g_ort.tensor_from_f32 == NULL ) {
		ort_set_error("onnxruntime tensor_from_f32 is unavailable");
		return NULL;
	}
	if ( !cv_bridge_load() || g_cv_bridge.to_chw_tensor_f32_resized == NULL || g_cv_bridge.release_tensor_f32 == NULL ) {
		ort_set_error("OpenCV resized CHW tensor bridge is unavailable");
		return NULL;
	}
	if ( !g_cv_bridge.to_chw_tensor_f32_resized(image, width, height, scale, mean0, mean1, mean2, std0, std1, std2, &data, shape, &count) || data == NULL || count <= 0 ) {
		ort_set_error("OpenCV image resize to CHW tensor conversion failed");
		return NULL;
	}
	if ( g_ort.tensor_adopt_f32 != NULL ) {
		tensor = g_ort.tensor_adopt_f32(data, count, shape, 4);
		if ( tensor != NULL ) data = NULL;
	} else {
		tensor = g_ort.tensor_from_f32(data, count, shape, 4);
	}
	if ( data != NULL ) g_cv_bridge.release_tensor_f32(data);
	return tensor;
}

XL_EXPORT void* ort_tensor_from_opencv_batch_native(int batch_count, void* image0, void* image1, void* image2, void* image3, void* image4, void* image5, double scale, double mean0, double mean1, double mean2, double std0, double std1, double std2)
{
	void* images[6];
	float* batch_data = NULL;
	long long batch_shape[4] = {0, 0, 0, 0};
	int per_image_count = 0;
	int i;
	void* tensor;
	images[0] = image0;
	images[1] = image1;
	images[2] = image2;
	images[3] = image3;
	images[4] = image4;
	images[5] = image5;
	if ( batch_count <= 0 || batch_count > 6 ) {
		ort_set_error("invalid OpenCV batch count");
		return NULL;
	}
	for ( i = 0; i < batch_count; ++i ) {
		if ( images[i] == NULL ) {
			ort_set_error("invalid OpenCV image handle in batch");
			return NULL;
		}
	}
	if ( !ort_load() || g_ort.tensor_from_f32 == NULL ) {
		ort_set_error("onnxruntime tensor_from_f32 is unavailable");
		return NULL;
	}
	if ( !cv_bridge_load() || g_cv_bridge.to_chw_tensor_f32 == NULL || g_cv_bridge.release_tensor_f32 == NULL ) {
		return NULL;
	}
	for ( i = 0; i < batch_count; ++i ) {
		float* data = NULL;
		long long shape[4] = {0, 0, 0, 0};
		int count = 0;
		if ( !g_cv_bridge.to_chw_tensor_f32(images[i], scale, mean0, mean1, mean2, std0, std1, std2, &data, shape, &count) || data == NULL || count <= 0 ) {
			free(batch_data);
			ort_set_error("OpenCV image to CHW tensor conversion failed");
			return NULL;
		}
		if ( i == 0 ) {
			per_image_count = count;
			batch_shape[0] = batch_count;
			batch_shape[1] = shape[1];
			batch_shape[2] = shape[2];
			batch_shape[3] = shape[3];
			batch_data = (float*)malloc((size_t)per_image_count * (size_t)batch_count * sizeof(float));
			if ( batch_data == NULL ) {
				g_cv_bridge.release_tensor_f32(data);
				ort_set_error("out of memory");
				return NULL;
			}
		} else if ( count != per_image_count || shape[1] != batch_shape[1] || shape[2] != batch_shape[2] || shape[3] != batch_shape[3] ) {
			g_cv_bridge.release_tensor_f32(data);
			free(batch_data);
			ort_set_error("OpenCV batch images must have the same CHW shape");
			return NULL;
		}
		memcpy(batch_data + (size_t)i * (size_t)per_image_count, data, (size_t)per_image_count * sizeof(float));
		g_cv_bridge.release_tensor_f32(data);
	}
	if ( g_ort.tensor_adopt_f32 != NULL ) {
		tensor = g_ort.tensor_adopt_f32(batch_data, per_image_count * batch_count, batch_shape, 4);
		if ( tensor != NULL ) batch_data = NULL;
	} else {
		tensor = g_ort.tensor_from_f32(batch_data, per_image_count * batch_count, batch_shape, 4);
	}
	free(batch_data);
	return tensor;
}

XL_EXPORT int ort_tensor_release_native(void* handle) { if ( handle != NULL && ort_load() && g_ort.release_tensor != NULL ) g_ort.release_tensor(handle); return 1; }
XL_EXPORT void* ort_session_run_tensor_native(void* handle, const char* input_name, void* tensor, const char* outputs) { return (handle != NULL && tensor != NULL && ort_load() && g_ort.run_tensor != NULL) ? g_ort.run_tensor(handle, input_name, tensor, outputs) : NULL; }
XL_EXPORT void* ort_session_run_tensor_fast_native(void* handle, const char* input_name, void* tensor, const char* outputs) { return (handle != NULL && tensor != NULL && ort_load() && g_ort.run_tensor_fast != NULL) ? g_ort.run_tensor_fast(handle, input_name, tensor, outputs) : NULL; }
XL_EXPORT const char* ort_result_json_native(void* handle) { return (handle != NULL && ort_load() && g_ort.result_json != NULL) ? g_ort.result_json(handle) : "{}"; }
XL_EXPORT const char* ort_result_float_shape_json_native(void* handle) { return (handle != NULL && ort_load() && g_ort.result_float_shape_json != NULL) ? g_ort.result_float_shape_json(handle) : "[]"; }
XL_EXPORT int ort_result_float_count_native(void* handle) { return (handle != NULL && ort_load() && g_ort.result_float_count != NULL) ? g_ort.result_float_count(handle) : 0; }
XL_EXPORT double ort_result_float_at_native(void* handle, int index) { return (handle != NULL && ort_load() && g_ort.result_float_at != NULL) ? (double)g_ort.result_float_at(handle, index) : 0.0; }
XL_EXPORT int ort_result_release_native(void* handle) { if ( handle != NULL && ort_load() && g_ort.release_result != NULL ) g_ort.release_result(handle); return 1; }
