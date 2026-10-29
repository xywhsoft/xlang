// mylib.c - mylib.xl 的同名 C companion
// package 解析器展开 mylib.xl 时，会自动检查同目录的 mylib.c 并加入编译。
// xlang int -> C int64_t (long long)
#include <stdint.h>

long long xl6_pkgdemo_mylib_native_magic(long long v)
{
    return v * 1000 + 7;
}
