// srclib.c - srclib.xl 的同名 C companion
// import 解析器看到 srclib.xl 时，会自动检查同目录的 srclib.c 并加入编译。
// xlang int -> C int64_t (long long)
#include <stdint.h>

long long xl6_pkgdemo_srclib_native_inc(long long v)
{
    return v + 1;
}

long long xl6_pkgdemo_srclib_native_base(void)
{
    return 100;
}
