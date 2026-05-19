#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <stdlib.h>
#include <string.h>

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
typedef void* (*OrtRunTensorFn)(void*, const char*, void*, const char*);
typedef const char* (*OrtResultTextFn)(void*);
typedef int (*OrtResultIntFn)(void*);
typedef float (*OrtResultFloatAtFn)(void*, int);
typedef void* (*OrtCreateIoBindingFn)(void*);
typedef int (*CvToChwTensorF32Fn)(void*, double, double, double, double, double, double, double, float**, long long*, int*);
typedef void (*CvReleaseTensorF32Fn)(float*);
typedef int (*OrtIoBindingBindInputFn)(void*, const char*, const char*);
typedef int (*OrtIoBindingBindOutputFn)(void*, const char*);
typedef void* (*OrtRunIoBindingFn)(void*, void*, void*);

typedef struct OrtApi {
	int attempted;
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
	OrtTensorZerosFn tensor_zeros_f32;
	OrtReleasePtrFn release_tensor;
	OrtRunTensorFn run_tensor;
	OrtRunTensorFn run_tensor_fast;
	OrtResultTextFn result_json;
	OrtResultTextFn result_float_shape_json;
	OrtResultIntFn result_float_count;
	OrtResultFloatAtFn result_float_at;
	OrtReleasePtrFn release_result;
	char path[512];
} OrtApi;

static OrtApi g_ort;
static char g_error[512];

typedef struct CvBridgeApi {
	int attempted;
	int loaded;
#if defined(_WIN32) || defined(_WIN64)
	HMODULE module;
#else
	void* module;
#endif
	CvToChwTensorF32Fn to_chw_tensor_f32;
	CvReleaseTensorF32Fn release_tensor_f32;
} CvBridgeApi;

static CvBridgeApi g_cv_bridge;

static void ort_set_error(const char* text)
{
	if ( text == NULL ) text = "";
	strncpy(g_error, text, sizeof(g_error) - 1u);
	g_error[sizeof(g_error) - 1u] = '\0';
}

static void ort_copy(char* out, size_t out_size, const char* text)
{
	if ( out == NULL || out_size == 0u ) return;
	if ( text == NULL ) text = "";
	strncpy(out, text, out_size - 1u);
	out[out_size - 1u] = '\0';
}

static void* ort_sym(const char* name)
{
#if defined(_WIN32) || defined(_WIN64)
	return g_ort.module != NULL ? (void*)GetProcAddress(g_ort.module, name) : NULL;
#else
	return g_ort.module != NULL ? dlsym(g_ort.module, name) : NULL;
#endif
}

static int ort_try_load(const char* path)
{
	if ( path == NULL || path[0] == '\0' ) return 0;
#if defined(_WIN32) || defined(_WIN64)
	g_ort.module = LoadLibraryA(path);
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
	g_ort.tensor_zeros_f32 = (OrtTensorZerosFn)ort_sym("onnxruntime_xl_tensor_zeros_f32");
	g_ort.release_tensor = (OrtReleasePtrFn)ort_sym("onnxruntime_xl_release_tensor");
	g_ort.run_tensor = (OrtRunTensorFn)ort_sym("onnxruntime_xl_run_tensor");
	g_ort.run_tensor_fast = (OrtRunTensorFn)ort_sym("onnxruntime_xl_run_tensor_fast");
	g_ort.result_json = (OrtResultTextFn)ort_sym("onnxruntime_xl_result_json");
	g_ort.result_float_shape_json = (OrtResultTextFn)ort_sym("onnxruntime_xl_result_float_shape_json");
	g_ort.result_float_count = (OrtResultIntFn)ort_sym("onnxruntime_xl_result_float_count");
	g_ort.result_float_at = (OrtResultFloatAtFn)ort_sym("onnxruntime_xl_result_float_at");
	g_ort.release_result = (OrtReleasePtrFn)ort_sym("onnxruntime_xl_release_result");
	if ( g_ort.create_session == NULL || g_ort.run_json == NULL ) {
		ort_set_error("onnxruntime shim loaded but required symbols are missing");
		return 0;
	}
	g_ort.loaded = 1;
	ort_copy(g_ort.path, sizeof(g_ort.path), path);
	ort_set_error("");
	return 1;
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
	g_cv_bridge.module = LoadLibraryA(path);
#else
	g_cv_bridge.module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
	if ( g_cv_bridge.module == NULL ) return 0;
	g_cv_bridge.to_chw_tensor_f32 = (CvToChwTensorF32Fn)cv_bridge_sym("opencv_xl_to_chw_tensor_f32");
	g_cv_bridge.release_tensor_f32 = (CvReleaseTensorF32Fn)cv_bridge_sym("opencv_xl_release_tensor_f32");
	if ( g_cv_bridge.to_chw_tensor_f32 == NULL || g_cv_bridge.release_tensor_f32 == NULL ) return 0;
	g_cv_bridge.loaded = 1;
	return 1;
}

static int cv_bridge_load(void)
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
	if ( g_cv_bridge.loaded ) return 1;
	if ( g_cv_bridge.attempted ) return 0;
	g_cv_bridge.attempted = 1;
	env_path = getenv("OPENCV_XLANG_DLL");
	if ( env_path != NULL && env_path[0] != '\0' && cv_bridge_try_load(env_path) ) return 1;
	for ( i = 0; candidates[i] != NULL; ++i ) {
		if ( cv_bridge_try_load(candidates[i]) ) return 1;
	}
	ort_set_error("opencv_xlang.dll with tensor bridge symbols was not found");
	return 0;
}

static int ort_load(void)
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
	if ( env_path != NULL && env_path[0] != '\0' && ort_try_load(env_path) ) return 1;
	for ( i = 0; candidates[i] != NULL; ++i ) {
		if ( ort_try_load(candidates[i]) ) return 1;
	}
	ort_set_error("onnxruntime shim not found; set ONNXRUNTIME_XLANG_DLL or place onnxruntime_xlang.dll under libs\\onnxruntime\\bin");
	return 0;
}

XL_EXPORT int ort_available_native(void) { return ort_load(); }
XL_EXPORT const char* ort_backend_native(void) { return ort_load() ? g_ort.path : ""; }
XL_EXPORT const char* ort_get_version_string_native(void) { return (ort_load() && g_ort.version != NULL) ? g_ort.version() : ""; }
XL_EXPORT const char* ort_get_available_providers_json_native(void) { return (ort_load() && g_ort.providers != NULL) ? g_ort.providers() : "[]"; }
XL_EXPORT const char* ort_last_error_native(void) { return (ort_load() && g_ort.last_error != NULL) ? g_ort.last_error() : g_error; }
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
	tensor = g_ort.tensor_from_f32(data, count, shape, 4);
	g_cv_bridge.release_tensor_f32(data);
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
	tensor = g_ort.tensor_from_f32(batch_data, per_image_count * batch_count, batch_shape, 4);
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
