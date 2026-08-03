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
/* TCC PE 链接器要求外部变量必须声明 dllimport。
 * xCore 是 xrt 的全局变量，由 xlang 宿主进程提供。 */
#if defined(_WIN32) && defined(__TINYC__)
#define XXAPI __declspec(dllimport)
#endif

/* TCC 不支持 SSE2 intrinsics，禁用 stb 库的 SIMD 路径 */
#ifndef STBI_NO_SIMD
#define STBI_NO_SIMD
#endif

#include "xge.h"
#include "src/xge_impl.c"
