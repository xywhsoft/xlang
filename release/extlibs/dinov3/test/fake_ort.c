#include "fake_ort.h"
#include "../../onnxruntime/include/onnxruntime.h"

/* 只替代结果处理依赖；真实 DLL 装载由 live_bridge.c 单独验证。 */
int ort_available_native(void)
{
    return 1;
}

const char* ort_last_error_native(void)
{
    return "dinov3 result test stub";
}

const char* ort_result_float_shape_json_native(void* handle)
{
    return ((const DinoFakeResult*)handle)->shape;
}

int ort_result_float_count_native(void* handle)
{
    return ((const DinoFakeResult*)handle)->count;
}

double ort_result_float_at_native(void* handle, int index)
{
    const DinoFakeResult* result = (const DinoFakeResult*)handle;
    return index >= 0 && index < result->count ? result->data[index] : 0.0;
}
