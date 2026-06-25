#ifndef XL6_TOOLCHAIN_H
#define XL6_TOOLCHAIN_H

/*
    DEMO6 当前仍复用既有 TCC 工具链目录。
    这里是唯一的源码级默认路径入口，后续迁移到 demo6 自带工具链时只改本文件。
*/
#define XL6_TCC_EXE_PATH "..\\demo5\\lib\\tcc\\tcc.exe"
#define XL6_TCC_DLL_PATH "..\\demo5\\lib\\tcc\\libtcc.dll"
#define XL6_TCC_ROOT_PATH "..\\demo5\\release\\tcc"
#define XL6_TCC_INCLUDE_PATH "..\\demo5\\lib\\tcc\\include"
#define XL6_TCC_WIN_INCLUDE_PATH "..\\demo5\\release\\tcc\\include_win"
#define XL6_TCC_WINAPI_INCLUDE_PATH "..\\demo5\\release\\tcc\\include_win\\winapi"
#define XL6_TCC_LIB_PATH "..\\demo5\\release\\tcc\\lib"
#define XL6_TCC_LIBTCC1_PATH "..\\demo5\\release\\tcc\\lib\\libtcc1-64.a"

#define XL6_TCC_DEFAULT_COMMAND_PREFIX \
    XL6_TCC_EXE_PATH " " \
    "-Ilib\\xrt " \
    "-Irelease\\tcc\\include_win " \
    "-I" XL6_TCC_INCLUDE_PATH " " \
    "-I" XL6_TCC_WIN_INCLUDE_PATH " " \
    "-I" XL6_TCC_WINAPI_INCLUDE_PATH " " \
    "-Irelease\\tmp " \
    "-L" XL6_TCC_LIB_PATH

#endif
