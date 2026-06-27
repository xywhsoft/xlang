#ifndef XL6_TOOLCHAIN_H
#define XL6_TOOLCHAIN_H

/*
    DEMO6 当前仍复用既有 TCC 工具链目录。
    这里是唯一的源码级默认路径入口，后续迁移到 demo6 自带工具链时只改本文件。
*/
#define XL6_TCC_EXE_PATH "..\\demo5\\lib\\tcc\\tcc.exe"
#define XL6_TCC_DLL_PATH "lib\\tcc\\win32\\dist\\x86_64\\libtcc.dll"

#if defined(CONFIG_TCC_STATIC) || defined(XL_STATIC_TCC)
#define XL6_TCC_ROOT_PATH "."
#define XL6_TCC_INCLUDE_PATH "include"
#define XL6_TCC_WIN_INCLUDE_PATH "include"
#define XL6_TCC_WINAPI_INCLUDE_PATH "include\\winapi"
#define XL6_TCC_LIB_PATH "lib"
#define XL6_TCC_LIBTCC1_PATH "lib\\libtcc1.a"
#define XL6_XRT_OBJ_PATH "lib\\xrt\\xrt.o"
#else
#define XL6_TCC_ROOT_PATH "lib\\tcc\\win32\\dist\\x86_64"
#define XL6_TCC_INCLUDE_PATH "lib\\tcc\\include"
#define XL6_TCC_WIN_INCLUDE_PATH "lib\\tcc\\win32\\dist\\x86_64\\include"
#define XL6_TCC_WINAPI_INCLUDE_PATH "lib\\tcc\\win32\\dist\\x86_64\\include\\winapi"
#define XL6_TCC_LIB_PATH "lib\\tcc\\win32\\dist\\x86_64\\lib"
#define XL6_TCC_LIBTCC1_PATH "lib\\tcc\\win32\\dist\\x86_64\\lib\\libtcc1.a"
#define XL6_XRT_OBJ_PATH "..\\..\\xrt\\release\\x64\\xrt.o"
#endif

#define XL6_TCC_DEFAULT_COMMAND_PREFIX \
    XL6_TCC_EXE_PATH " " \
    "-Ilib\\xrt " \
    "-I" XL6_TCC_INCLUDE_PATH " " \
    "-I" XL6_TCC_WIN_INCLUDE_PATH " " \
    "-I" XL6_TCC_WINAPI_INCLUDE_PATH " " \
    "-Irelease\\tmp " \
    "-L" XL6_TCC_LIB_PATH

#endif
