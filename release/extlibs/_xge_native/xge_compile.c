/*
 * xge_compile.c - XGE 引擎 unity build 编译单元
 *
 * 注意：不定义 XRT_IMPLEMENTATION。
 * XRT 实现由 xlang 运行时提供，此处仅使用 XRT 声明。
 *
 * xge_impl.c 位于 src/ 目录，其内部 include 路径已由 sync 脚本 patch：
 *   "lib/sokol/sokol_app.h" -> "../lib/sokol/sokol_app.h"
 *   "src/xge_gl.h"          -> "xge_gl.h"
 * 末尾的 #include "xge_core.c" 等解析到同目录 src/ 下的文件。
 */
/* TCC PE 链接器要求 XRT 导入符号使用 dllimport。
 * 具体符号由 xlang 宿主进程注入，不依赖已移除的全局状态。 */
#if defined(_WIN32) && defined(__TINYC__)
#define XRT_API __declspec(dllimport)
#endif

/* TCC 不提供完整 SIMD intrinsic 头，源码包构建时关闭对应快速路径。 */
#if defined(__TINYC__)
	#ifndef STBI_NO_SIMD
		#define STBI_NO_SIMD
	#endif
	#ifndef ZSTD_NO_INTRINSICS
		#define ZSTD_NO_INTRINSICS
	#endif
#endif

/*
 * Native packages compile beside generated C. Include the current XRT public
 * header directly instead of relying on the generated runtime's narrow
 * declaration subset; XGE uses path, file and structured-error facilities.
 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "xge.h"
#include "src/xge_impl.c"
