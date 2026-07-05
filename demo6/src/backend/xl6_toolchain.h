#ifndef XL6_TOOLCHAIN_H
#define XL6_TOOLCHAIN_H

/*
    DEMO6 默认使用 build.bat 自举出的本地 TCC。
    所有默认路径都留在 demo6 内部，避免依赖旧版本目录或外部 XRT 构建产物。
*/
#define XL6_TCC_EXE_PATH "build\\tmp\\tcc_bootstrap.exe"
#define XL6_TCC_DLL_PATH "lib\\tcc\\win32\\dist\\x86_64\\libtcc.dll"

#if defined(CONFIG_TCC_STATIC) || defined(XL_STATIC_TCC)
#define XL6_TCC_ROOT_PATH "."
#define XL6_TCC_INCLUDE_PATH "include"
#define XL6_TCC_WIN_INCLUDE_PATH "include"
#define XL6_TCC_WINAPI_INCLUDE_PATH "include\\winapi"
#define XL6_TCC_LIB_PATH "lib"
#define XL6_TCC_LIBTCC1_PATH "lib\\libtcc1.a"
#define XL6_XRT_OBJ_PATH "build\\tmp\\xrt.o"
#else
#define XL6_TCC_ROOT_PATH "lib\\tcc\\win32\\dist\\x86_64"
#define XL6_TCC_INCLUDE_PATH "lib\\tcc\\include"
#define XL6_TCC_WIN_INCLUDE_PATH "lib\\tcc\\win32\\dist\\x86_64\\include"
#define XL6_TCC_WINAPI_INCLUDE_PATH "lib\\tcc\\win32\\dist\\x86_64\\include\\winapi"
#define XL6_TCC_LIB_PATH "lib\\tcc\\win32\\dist\\x86_64\\lib"
#define XL6_TCC_LIBTCC1_PATH "lib\\tcc\\win32\\dist\\x86_64\\lib\\libtcc1.a"
#define XL6_XRT_OBJ_PATH "build\\tmp\\xrt.o"
#endif

#define XL6_TCC_DEFAULT_COMMAND_PREFIX \
    XL6_TCC_EXE_PATH " " \
    "-Ilib\\xrt " \
    "-I" XL6_TCC_INCLUDE_PATH " " \
    "-I" XL6_TCC_WIN_INCLUDE_PATH " " \
    "-I" XL6_TCC_WINAPI_INCLUDE_PATH " " \
    "-Ibuild\\tmp " \
    "-L" XL6_TCC_LIB_PATH

#endif
