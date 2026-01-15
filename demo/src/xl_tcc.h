/**
 * xlang - TCC 状态机管理
 * 
 * 提供 TCC 运行时环境的创建、配置和运行功能。
 * 参考 xserver 的 other.h 和 dynload.h 实现。
 */



// 包含 xrt 函数注册
#include "xl_import_xrt.h"



// TCC 状态机错误处理回调
static void xlTccErrorHandler(void *opaque, const char *msg)
{
	fprintf(stderr, "[TCC] %s\n", msg);
}



// 创建 xlang 的 TCC 状态机
// sWorkPath: 额外的工作目录（可为 NULL）
// 返回: 创建好的 TCCState，失败返回 NULL
TCCState* xlCreateTCC(const char* sWorkPath)
{
	TCCState* s = tcc_new();
	if ( s == NULL ) {
		return NULL;
	}
	
	// 设置错误输出回调函数
	tcc_set_error_func(s, stderr, xlTccErrorHandler);
	
	// 获取程序所在目录
	str sExeDir = xrtPathGetDir((str)xCore.AppPath, 0);
	
	// 如果在 release 子目录，向上一级到项目根目录
	if ( strstr(sExeDir, "release") ) {
		str sParent = xrtPathGetDir(sExeDir, 0);
		xrtFree(sExeDir);
		sExeDir = sParent;
	}
	
	// 获取 TCC 根目录：sExeDir + "release/tcc"
	str sTccPath = xrtPathJoin(3, sExeDir, "release", "tcc");
	xrtFree(sExeDir);
	
	// 添加系统引用文件目录
	#if defined(_WIN32) || defined(_WIN64)
		str sWinApi = xrtPathJoin(2, sTccPath, "include_win/winapi");
		str sWinInc = xrtPathJoin(2, sTccPath, "include_win");
		tcc_add_include_path(s, sWinApi);
		tcc_add_include_path(s, sWinInc);
		xrtFree(sWinApi);
		xrtFree(sWinInc);
	#else
		str sLinuxInc = xrtPathJoin(2, sTccPath, "include_linux");
		if ( xrtDirExists(sLinuxInc) ) {
			tcc_add_include_path(s, sLinuxInc);
		}
		xrtFree(sLinuxInc);
		tcc_add_include_path(s, "/usr/include");
		tcc_add_library_path(s, "/usr/lib");
		tcc_add_include_path(s, "/usr/include/x86_64-linux-gnu");
		tcc_add_include_path(s, "/usr/include/x86_64-linux-gnu/sys");
		tcc_add_library_path(s, "/usr/lib/x86_64-linux-gnu");
	#endif
	
	// 添加 xlang 引用文件目录
	str sIncXl = xrtPathJoin(2, sTccPath, "inc_xl");
	str sLib = xrtPathJoin(2, sTccPath, "lib");
	str sXrt = xrtPathJoin(2, sTccPath, "xrt");
	tcc_add_include_path(s, sIncXl);
	tcc_add_include_path(s, sXrt);
	tcc_add_library_path(s, sLib);
	xrtFree(sIncXl);
	xrtFree(sLib);
	xrtFree(sXrt);
	xrtFree(sTccPath);
	
	// 添加工作目录
	if ( sWorkPath && sWorkPath[0] != '\0' ) {
		tcc_add_include_path(s, sWorkPath);
		tcc_add_library_path(s, sWorkPath);
	}
	
	// 设置编译到内存
	tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
	
	// 导入 xrt 函数
	xlImportXRT(s);
	
	return s;
}



// 销毁 TCC 状态机
void xlDestroyTCC(TCCState* s)
{
	if ( s ) {
		tcc_delete(s);
	}
}



// TCC 运行模式入口
// pszCode: 要运行的 C 代码
// pszSourceFile: 源文件路径（用于获取工作目录，可为 NULL）
// 返回: 程序退出码
int xlRunWithTCC(const char* pszCode, const char* pszSourceFile)
{
	// 获取源文件目录作为工作目录
	str sWorkPath = pszSourceFile ? xrtPathGetDir((str)pszSourceFile, 0) : NULL;
	
	// 创建 TCC 状态机
	TCCState* s = xlCreateTCC(sWorkPath);
	if ( sWorkPath ) {
		xrtFree(sWorkPath);
	}
	
	if ( s == NULL ) {
		fprintf(stderr, "错误: 创建 TCC 状态机失败\n");
		return 1;
	}
	
	// 预处理代码：将独立编译模式的代码转换为 TCC 运行模式
	// 1. 替换 include 路径
	// 2. 移除 xrtInit/xrtUnit 调用（主程序已初始化）
	str sModifiedCode = xrtReplace((str)pszCode, 0, (str)"#include \"xrt/xrt.h\"", 0, (str)"#include \"xlbase.h\"", 0, NULL);
	str sCode2 = xrtReplace(sModifiedCode, 0, (str)"xrtInit();", 0, (str)"// xrtInit() - 由主程序调用", 0, NULL);
	xrtFree(sModifiedCode);
	str sCode3 = xrtReplace(sCode2, 0, (str)"xrtUnit();", 0, (str)"// xrtUnit() - 由主程序调用", 0, NULL);
	xrtFree(sCode2);
	
	// 编译代码
	if ( tcc_compile_string(s, sCode3) == -1 ) {
		fprintf(stderr, "错误: TCC 编译失败\n");
		xrtFree(sCode3);
		xlDestroyTCC(s);
		return 1;
	}
	xrtFree(sCode3);
	
	// 地址重定向
	if ( tcc_relocate(s) < 0 ) {
		fprintf(stderr, "错误: TCC 重定向失败\n");
		xlDestroyTCC(s);
		return 1;
	}
	
	// 传递全局数据
	void (*XL_SetGlobalData)(int, void*) = tcc_get_symbol(s, "XL_SetGlobalData");
	if ( XL_SetGlobalData ) {
		XL_SetGlobalData(1, &xCore);
	}
	
	// 查找并运行 main 函数
	int (*pMain)(void) = tcc_get_symbol(s, "main");
	if ( pMain == NULL ) {
		fprintf(stderr, "错误: 找不到 main 函数\n");
		xlDestroyTCC(s);
		return 1;
	}
	
	int iRet = pMain();
	
	xlDestroyTCC(s);
	return iRet;
}


