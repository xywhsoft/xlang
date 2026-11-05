#ifndef XL6_TOOLCHAIN_H
#define XL6_TOOLCHAIN_H

/*
    默认工具链路径只描述当前宿主平台，不在运行时做兼容回退。
    静态 TCC 使用统一的 VFS 虚拟路径；外部工具链使用平台原生路径。
*/
#if defined(_WIN32)

#define XL6_TCC_EXE_PATH "build\\tmp\\tcc_bootstrap.exe"
#define XL6_TCC_DLL_PATH ""

# if defined(CONFIG_TCC_STATIC) || defined(XL_STATIC_TCC)
#  define XL6_TCC_ROOT_PATH "."
#  define XL6_TCC_INCLUDE_PATH "include"
#  define XL6_TCC_WIN_INCLUDE_PATH "include"
#  define XL6_TCC_WINAPI_INCLUDE_PATH "include\\winapi"
#  define XL6_TCC_LIB_PATH "lib"
#  define XL6_TCC_LIBTCC1_PATH "lib\\libtcc1.a"
#  define XL6_XRT_OBJ_PATH "build\\tmp\\xrt.o"
# else
#  define XL6_TCC_ROOT_PATH "res\\tcc\\win64\\x86_64"
#  define XL6_TCC_INCLUDE_PATH "lib\\tcc\\include"
#  define XL6_TCC_WIN_INCLUDE_PATH "res\\tcc\\win64\\x86_64\\include"
#  define XL6_TCC_WINAPI_INCLUDE_PATH "res\\tcc\\win64\\x86_64\\include\\winapi"
#  define XL6_TCC_LIB_PATH "res\\tcc\\win64\\x86_64\\lib"
#  define XL6_TCC_LIBTCC1_PATH "res\\tcc\\win64\\x86_64\\lib\\libtcc1.a"
#  define XL6_XRT_OBJ_PATH "build\\tmp\\xrt.o"
# endif

# define XL6_TCC_DEFAULT_COMMAND_PREFIX \
    XL6_TCC_EXE_PATH " " \
    "-Ilib\\xrt " \
    "-I" XL6_TCC_INCLUDE_PATH " " \
    "-I" XL6_TCC_WIN_INCLUDE_PATH " " \
    "-I" XL6_TCC_WINAPI_INCLUDE_PATH " " \
    "-Ibuild\\tmp " \
    "-L" XL6_TCC_LIB_PATH
# define XL6_TCC_RUNTIME_RESOURCE_SOURCE "build\\tmp\\tcc_builtin_resources_runtime.c"

#else

#define XL6_TCC_EXE_PATH "build/linux/tcc"
#define XL6_TCC_DLL_PATH ""

# if defined(CONFIG_TCC_STATIC) || defined(XL_STATIC_TCC)
#  define XL6_TCC_ROOT_PATH "."
#  define XL6_TCC_INCLUDE_PATH "include"
#  define XL6_TCC_WIN_INCLUDE_PATH "include"
#  define XL6_TCC_WINAPI_INCLUDE_PATH "include"
#  define XL6_TCC_LIB_PATH "lib"
#  define XL6_TCC_LIBTCC1_PATH "lib/libtcc1.a"
#  define XL6_XRT_OBJ_PATH "build/linux/xrt.o"
# else
#  define XL6_TCC_ROOT_PATH "res/tcc/linux/x86_64"
#  define XL6_TCC_INCLUDE_PATH "lib/tcc/include"
#  define XL6_TCC_WIN_INCLUDE_PATH "res/tcc/linux/x86_64/include"
#  define XL6_TCC_WINAPI_INCLUDE_PATH "res/tcc/linux/x86_64/include"
#  define XL6_TCC_LIB_PATH "res/tcc/linux/x86_64/lib"
#  define XL6_TCC_LIBTCC1_PATH "res/tcc/linux/x86_64/lib/libtcc1.a"
#  define XL6_XRT_OBJ_PATH "build/linux/xrt.o"
# endif

# define XL6_TCC_DEFAULT_COMMAND_PREFIX \
    XL6_TCC_EXE_PATH " " \
    "-Ilib/xrt " \
    "-I" XL6_TCC_INCLUDE_PATH " " \
    "-Ibuild/linux " \
    "-L" XL6_TCC_LIB_PATH
# define XL6_TCC_RUNTIME_RESOURCE_SOURCE "build/linux/tcc_builtin_resources_runtime.c"

#endif

#endif
