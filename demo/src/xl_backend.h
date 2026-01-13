/**
 * xlang - TCC 后端模块
 * 
 * 使用 libtcc 编译并执行生成的 C 代码
 */

#ifndef XL_BACKEND_H
#define XL_BACKEND_H

#include "xl_common.h"
#include "xl_error.h"
#include "xl_codegen.h"
#include "libtcc.h"




// ============================================================================
// TCC 后端
// ============================================================================

// TCC 错误处理函数
static void xlTccErrorFunc( void *opaque, const char *msg )
{
	fprintf(stderr, "[TCC] %s\n", msg);
}


// 使用 TCC 编译并运行 C 代码
static inline int xlBackendRun( const char *pszCode, const char *pszIncludePath )
{
	TCCState *pTcc = tcc_new();
	if ( !pTcc ) {
		fprintf(stderr, "错误: 无法创建 TCC 状态\n");
		return -1;
	}

	// 设置错误处理
	tcc_set_error_func(pTcc, NULL, xlTccErrorFunc);

	// 设置输出类型为内存执行
	tcc_set_output_type(pTcc, TCC_OUTPUT_MEMORY);

	// 添加 include 路径
	if ( pszIncludePath ) {
		tcc_add_include_path(pTcc, pszIncludePath);
	}

	// 编译代码
	if ( tcc_compile_string(pTcc, pszCode) < 0 ) {
		fprintf(stderr, "错误: TCC 编译失败\n");
		tcc_delete(pTcc);
		return -1;
	}

	// 重定位
	if ( tcc_relocate(pTcc) < 0 ) {
		fprintf(stderr, "错误: TCC 重定位失败\n");
		tcc_delete(pTcc);
		return -1;
	}

	// 获取 main 函数
	typedef int (*MainFunc)(int, char**);
	MainFunc pfnMain = (MainFunc)tcc_get_symbol(pTcc, "main");
	if ( !pfnMain ) {
		fprintf(stderr, "错误: 找不到 main 函数\n");
		tcc_delete(pTcc);
		return -1;
	}

	// 执行
	char *argv[] = { "xlang", NULL };
	int iResult = pfnMain(1, argv);

	// 清理
	tcc_delete(pTcc);

	return iResult;
}


// 使用 TCC 编译为可执行文件
static inline int xlBackendCompile( const char *pszCode, const char *pszOutput, const char *pszIncludePath )
{
	TCCState *pTcc = tcc_new();
	if ( !pTcc ) {
		fprintf(stderr, "错误: 无法创建 TCC 状态\n");
		return -1;
	}

	// 设置错误处理
	tcc_set_error_func(pTcc, NULL, xlTccErrorFunc);

	// 设置输出类型为可执行文件
	tcc_set_output_type(pTcc, TCC_OUTPUT_EXE);

	// 添加 include 路径
	if ( pszIncludePath ) {
		tcc_add_include_path(pTcc, pszIncludePath);
	}

	// 编译代码
	if ( tcc_compile_string(pTcc, pszCode) < 0 ) {
		fprintf(stderr, "错误: TCC 编译失败\n");
		tcc_delete(pTcc);
		return -1;
	}

	// 输出文件
	if ( tcc_output_file(pTcc, pszOutput) < 0 ) {
		fprintf(stderr, "错误: 无法输出文件 %s\n", pszOutput);
		tcc_delete(pTcc);
		return -1;
	}

	// 清理
	tcc_delete(pTcc);

	printf("已编译: %s\n", pszOutput);
	return 0;
}


#endif // XL_BACKEND_H
