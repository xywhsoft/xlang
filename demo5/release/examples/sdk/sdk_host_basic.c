#include "xlang.h"

#include <stdio.h>
#include <stdlib.h>

static int sdk_add_one(int value)
{
	return value + 1;
}

int main(void)
{
	xlang_options options;
	xlang_runtime* runtime;
	xlang_run_result result;
	xlang_build_result build_result;
	int rc;

	options.release_root = "release";
	runtime = xlang_runtime_create(&options);
	if ( runtime == 0 ) {
		return 10;
	}
	if ( xlang_runtime_register_function(runtime, "sdk_add_one", (void*)sdk_add_one) != XLANG_OK ) {
		xlang_runtime_destroy(runtime);
		return 11;
	}
	rc = xlang_run_file(runtime, "release\\examples\\sdk\\sdk_host_function.xl", &result);
	if ( rc != XLANG_OK ) {
		xlang_runtime_destroy(runtime);
		return 12;
	}
	if ( result.exit_code != 0 ) {
		xlang_runtime_destroy(runtime);
		return 13;
	}
	rc = xlang_compile_file(runtime, "release\\examples\\sdk\\sdk_compile_basic.xl", "release\\tmp\\sdk_compile_basic.exe", &build_result);
	xlang_runtime_destroy(runtime);
	if ( rc != XLANG_OK ) {
		return 14;
	}
	rc = system("release\\tmp\\sdk_compile_basic.exe");
	remove("release\\tmp\\sdk_compile_basic.exe");
	if ( rc != 0 ) {
		return 15;
	}
	return 0;
}
