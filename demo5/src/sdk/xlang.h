#ifndef XLANG_SDK_H
#define XLANG_SDK_H

#include <stdint.h>

#if defined(_WIN32) || defined(_WIN64)
	#if defined(XLANG_BUILD_DLL)
		#define XLANG_API __declspec(dllexport)
	#elif defined(XLANG_USE_DLL)
		#define XLANG_API __declspec(dllimport)
	#else
		#define XLANG_API
	#endif
#else
	#if defined(XLANG_BUILD_DLL)
		#define XLANG_API __attribute__((visibility("default")))
	#else
		#define XLANG_API
	#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define XLANG_OK 0
#define XLANG_ERROR 1
#define XLANG_BAD_ARG 2

#define XLANG_PATH_MAX 1024
#define XLANG_TEXT_MAX 2048

typedef struct xlang_runtime xlang_runtime;

typedef struct xlang_options {
	const char* release_root;
} xlang_options;

typedef struct xlang_run_result {
	int rc;
	int exit_code;
	int had_runtime_error;
	int had_exception;
	int exception_kind;
	char exception_text[XLANG_TEXT_MAX];
	char exception_file[XLANG_PATH_MAX];
	int exception_line;
	int exception_column;
	int64_t exception_int;
	double exception_float;
} xlang_run_result;

typedef struct xlang_build_result {
	int rc;
	int had_build_error;
	char output_file[XLANG_PATH_MAX];
	char output_kind[32];
} xlang_build_result;

XLANG_API xlang_runtime* xlang_runtime_create(const xlang_options* options);
XLANG_API void xlang_runtime_destroy(xlang_runtime* runtime);
XLANG_API const char* xlang_runtime_last_error(const xlang_runtime* runtime);

XLANG_API int xlang_runtime_register_function(xlang_runtime* runtime, const char* name, void* address);
XLANG_API int xlang_runtime_add_library(xlang_runtime* runtime, const char* path);
XLANG_API int xlang_runtime_add_link_library(xlang_runtime* runtime, const char* name);
XLANG_API int xlang_runtime_add_library_path(xlang_runtime* runtime, const char* path);
XLANG_API int xlang_runtime_add_include_path(xlang_runtime* runtime, const char* path);

XLANG_API int xlang_run_file(xlang_runtime* runtime, const char* file, xlang_run_result* result);
XLANG_API int xlang_run_text(xlang_runtime* runtime, const char* virtual_path, const char* source, xlang_run_result* result);
XLANG_API int xlang_compile_file(xlang_runtime* runtime, const char* file, const char* output_file, xlang_build_result* result);
XLANG_API int xlang_compile_text(xlang_runtime* runtime, const char* virtual_path, const char* source, const char* output_file, xlang_build_result* result);

#ifdef __cplusplus
}
#endif

#endif
