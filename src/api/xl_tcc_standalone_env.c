#include "libtcc.h"
#include "tcc_builtin_vfs.h"

#include "src/common/xl6_xrt.h"

#include <stddef.h>


typedef struct XL6_TccRuntimeSymbol {
	const char* Name;
	const void* Value;
} XL6_TccRuntimeSymbol;


static const float xl6__mzerosf = -0.0f;
static const double xl6__mzerodf = -0.0;


#define XL_XRT_SYMBOL(name) { #name, (const void*)(name) },
static const XL6_TccRuntimeSymbol xl6__xrtRuntimeSymbols[] = {
#include "src/api/xl_xrt_symbols.inc"
};
#undef XL_XRT_SYMBOL


#define XL6_TCC_SYMBOL(name) { #name, (const void*)(name) },
static const XL6_TccRuntimeSymbol xl6__tccRuntimeSymbols[] = {
	XL6_TCC_SYMBOL(tcc_new)
	XL6_TCC_SYMBOL(tcc_delete)
	XL6_TCC_SYMBOL(tcc_set_lib_path)
	XL6_TCC_SYMBOL(tcc_set_error_func)
	XL6_TCC_SYMBOL(tcc_set_options)
	XL6_TCC_SYMBOL(tcc_add_include_path)
	XL6_TCC_SYMBOL(tcc_add_sysinclude_path)
	XL6_TCC_SYMBOL(tcc_define_symbol)
	XL6_TCC_SYMBOL(tcc_undefine_symbol)
	XL6_TCC_SYMBOL(tcc_add_library_path)
	XL6_TCC_SYMBOL(tcc_add_library)
	XL6_TCC_SYMBOL(tcc_add_file)
	XL6_TCC_SYMBOL(tcc_add_symbol)
	XL6_TCC_SYMBOL(tcc_set_output_type)
	XL6_TCC_SYMBOL(tcc_compile_string)
	XL6_TCC_SYMBOL(tcc_output_file)
	XL6_TCC_SYMBOL(tcc_run)
	XL6_TCC_SYMBOL(tcc_relocate)
	XL6_TCC_SYMBOL(tcc_get_symbol)
	XL6_TCC_SYMBOL(tcc_list_symbols)
	XL6_TCC_SYMBOL(tcc_set_backtrace_func)
	XL6_TCC_SYMBOL(tcc_vfs_open)
	XL6_TCC_SYMBOL(tcc_vfs_close)
	XL6_TCC_SYMBOL(tcc_vfs_read)
	XL6_TCC_SYMBOL(tcc_vfs_lseek)
	XL6_TCC_SYMBOL(tcc_vfs_fopen)
	XL6_TCC_SYMBOL(tcc_vfs_fclose)
	XL6_TCC_SYMBOL(tcc_vfs_mount_memory)
	XL6_TCC_SYMBOL(tcc_vfs_clear_dynamic)
};
#undef XL6_TCC_SYMBOL



/* 向内嵌 TCC 注册一组符号，任何失败都终止环境创建。 */
static int xl6TccAddSymbols(
	TCCState* state,
	const XL6_TccRuntimeSymbol* symbols,
	size_t count)
{
	size_t i;

	for ( i = 0u; i < count; ++i ) {
		if ( tcc_add_symbol(state, symbols[i].Name, symbols[i].Value) != 0 ) {
			return 0;
		}
	}
	return 1;
}



/* 为独立 xlang 产物配置静态 TCC，并暴露该产物携带的完整 XRT。 */
int xlTccRegisterXrtEnvironment(TCCState* state, int outputType)
{
	if ( state == NULL ||
		 outputType < TCC_OUTPUT_MEMORY ||
		 outputType > TCC_OUTPUT_PREPROCESS ) {
		return 0;
	}

	tcc_set_lib_path(state, ".");
	(void)tcc_set_options(state, "-std=c11 -D_CRT_SECURE_NO_WARNINGS");
	(void)tcc_add_include_path(state, ".");
	(void)tcc_add_include_path(state, "include");
	(void)tcc_add_sysinclude_path(state, "include");
	(void)tcc_add_library_path(state, "lib");
#ifdef _WIN32
	(void)tcc_add_include_path(state, "include/winapi");
	(void)tcc_add_sysinclude_path(state, "include/winapi");
#endif

	if ( tcc_add_symbol(state, "__mzerosf", &xl6__mzerosf) != 0 ||
		 tcc_add_symbol(state, "__mzerodf", &xl6__mzerodf) != 0 ||
		 !xl6TccAddSymbols(
			state,
			xl6__xrtRuntimeSymbols,
			sizeof(xl6__xrtRuntimeSymbols) / sizeof(xl6__xrtRuntimeSymbols[0])) ||
		 !xl6TccAddSymbols(
			state,
			xl6__tccRuntimeSymbols,
			sizeof(xl6__tccRuntimeSymbols) / sizeof(xl6__tccRuntimeSymbols[0])) ||
		 tcc_add_symbol(
			state,
			"xlTccRegisterXrtEnvironment",
			(const void*)xlTccRegisterXrtEnvironment) != 0 ) {
		return 0;
	}
	return 1;
}
