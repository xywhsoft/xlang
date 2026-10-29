/* 显式 C 源没有包清单；宿主必须提供完整声明和实现，不要求用户猜测裁剪宏。 */
#include <xruntime.h>

#if !defined(XRT_FEATURE_CODEC_PERCENT) || !defined(XRUNTIME_FEATURE_TYPED_ARRAY)
#error "explicit C sources must have the complete native environment"
#endif

int xl_native_extra_probe(void)
{
    const int64 expected = INT64_C(9876543210123);
    xtypedarray* values = xrtTypedArrayCreate(xrtTypeInt64());
    int ok = values != NULL && xrtTypedArrayPush(values, &expected) &&
        xrtTypedArrayCount(values) == 1 &&
        *(const int64*)xrtTypedArrayConstGet(values, 0) == expected;
    xrtTypedArrayDestroy(values);
    return ok ? 0 : 1;
}
