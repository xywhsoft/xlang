#include <math.h>
#include <string.h>
#include "xrt.h"
#include "../include/dinov3.h"
#include "../../onnxruntime/include/onnxruntime.h"

#if defined(_WIN32) || defined(_WIN64)
#define XL_EXPORT __declspec(dllexport)
#else
#define XL_EXPORT
#endif

#define DINO_MAX_VALUES 100000

static xonce g_error_once = XRT_ONCE_INIT;
static xthreadkey* g_error_key;

static bool dino_error_init(ptr unused)
{
    (void)unused;
    g_error_key = xrtThreadKeyCreate(xrtFree);
    return g_error_key != NULL;
}

static char* dino_error_buffer(bool create)
{
    char* text;
    if (!xrtOnce(&g_error_once, dino_error_init, NULL)) return NULL;
    text = (char*)xrtThreadKeyGet(g_error_key);
    if (text != NULL || !create) return text;
    text = (char*)xrtMalloc(512u);
    if (text == NULL) return NULL;
    text[0] = '\0';
    if (!xrtThreadKeySet(g_error_key, text)) { xrtFree(text); return NULL; }
    return text;
}

static void dino_set_error(const char* message)
{
    char* text = dino_error_buffer(message != NULL && message[0] != '\0');
    size_t length;
    if (text == NULL) return;
    if (message == NULL) message = "";
    length = strlen(message);
    if (length > 511u) length = 511u;
    memcpy(text, message, length);
    text[length] = '\0';
}

static xvalue* dino_shape(void* handle, int* value_count)
{
    const char* shape_json;
    xvalue* shape;
    size_t dims;
    size_t i;
    int count;
    int product = 1;
    /* 新的形状读取只保留本次首因，旧线程 OOM 不能冒充本次读取失败。 */
    xrtClearError();
    if (handle == NULL || value_count == NULL) {
        dino_set_error("DinoV3 output is unavailable");
        return NULL;
    }
    count = ort_result_float_count_native(handle);
    shape_json = ort_result_float_shape_json_native(handle);
    if (count < 0 || count > DINO_MAX_VALUES || shape_json == NULL ||
        strlen(shape_json) == 0u || strlen(shape_json) > 256u) goto invalid;
    shape = xrtJsonParse(xrtStrView(shape_json));
    if (xrtValueType(shape) != XVALUE_ARRAY) goto invalid_shape;
    dims = xrtValueCount(shape);
    if (dims == 0u || dims > 8u) goto invalid_shape;
    for (i = 0u; i < dims; ++i) {
        int64 dimension;
        if (!xrtValueGetInt(xrtValueArrayGet(shape, i), &dimension) ||
            dimension < 0 || dimension > DINO_MAX_VALUES ||
            product > DINO_MAX_VALUES / (dimension > 0 ? dimension : 1)) goto invalid_shape;
        product *= (int)dimension;
    }
    if (product != count) goto invalid_shape;
    *value_count = count;
    return shape;
invalid_shape:
    xrtValueRelease(shape);
invalid:
    /* 解析 shape 的分配失败不是模型格式错误，不能清掉 XRT 的首因。 */
    if (xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY) {
        dino_set_error("out of memory");
        return NULL;
    }
    xrtClearError();
    dino_set_error("invalid DinoV3 embedding shape or count");
    return NULL;
}

static bool dino_write_embedding(xjsonwriter* writer, void* handle, int count)
{
    int i;
    if (!xrtJsonWriterArray(writer)) return false;
    for (i = 0; i < count; ++i) {
        double value = ort_result_float_at_native(handle, i);
        if (!isfinite(value) || !xrtJsonWriterFloat(writer, value)) return false;
    }
    return xrtJsonWriterEnd(writer);
}

static char* dino_json_take(xjsonwriter* writer, bool valid)
{
    char* text = NULL;
    if (valid && xrtJsonWriterFinish(writer)) text = xrtJsonWriterTake(writer, NULL);
    xrtJsonWriterFree(writer);
    dino_set_error(text != NULL ? "" : "DinoV3 JSON serialization failed");
    return text;
}

XL_EXPORT const char* dinov3_version_native(void)
{
    return "dinov3-xlang 0.1.0";
}

XL_EXPORT int dinov3_available_native(void)
{
    int available = ort_available_native();
    dino_set_error(available ? "" : ort_last_error_native());
    return available;
}

XL_EXPORT const char* dinov3_last_error_native(void)
{
    char* text = dino_error_buffer(false);
    return text != NULL ? text : ort_last_error_native();
}

XL_EXPORT char* dinov3_embedding_json_native(void* result_handle)
{
    xjsonwriteconfig config;
    xjsonwriter* writer;
    xvalue* shape;
    int count = 0;
    bool valid;
    if (result_handle == NULL) {
        dino_set_error("DinoV3 output is unavailable");
        return NULL;
    }
    shape = dino_shape(result_handle, &count);
    if (shape == NULL) return NULL;
    xrtValueRelease(shape);
    xrtJsonWriteConfigInit(&config);
    writer = xrtJsonWriterCreate(&config);
    if (writer == NULL) { dino_set_error("out of memory"); return NULL; }
    valid = dino_write_embedding(writer, result_handle, count);
    return dino_json_take(writer, valid);
}

XL_EXPORT char* dinov3_output_json_native(void* result_handle, const char* source_path)
{
    xjsonwriteconfig config;
    xjsonwriter* writer;
    xvalue* shape;
    int count = 0;
    bool valid;
    if (result_handle == NULL) {
        dino_set_error("DinoV3 output is unavailable");
        return NULL;
    }
    shape = dino_shape(result_handle, &count);
    if (shape == NULL) return NULL;
    xrtJsonWriteConfigInit(&config);
    writer = xrtJsonWriterCreate(&config);
    if (writer == NULL) { xrtValueRelease(shape); dino_set_error("out of memory"); return NULL; }
    valid = xrtJsonWriterObject(writer) &&
        xrtJsonWriterName(writer, xrtStrView("source_path")) &&
        xrtJsonWriterString(writer, xrtStrView(source_path != NULL ? source_path : "")) &&
        xrtJsonWriterName(writer, xrtStrView("shape")) &&
        xrtJsonWriterValue(writer, shape) &&
        xrtJsonWriterName(writer, xrtStrView("count")) &&
        xrtJsonWriterInt(writer, count) &&
        xrtJsonWriterName(writer, xrtStrView("embedding")) &&
        dino_write_embedding(writer, result_handle, count) &&
        xrtJsonWriterEnd(writer);
    xrtValueRelease(shape);
    return dino_json_take(writer, valid);
}

XL_EXPORT double dinov3_l2_norm_native(void* result_handle)
{
    xvalue* shape;
    double sum = 0.0;
    int count;
    int i;
    shape = dino_shape(result_handle, &count);
    if (shape == NULL) return 0.0;
    xrtValueRelease(shape);
    for (i = 0; i < count; ++i) {
        double value = ort_result_float_at_native(result_handle, i);
        if (!isfinite(value)) { dino_set_error("non-finite DinoV3 embedding"); return 0.0; }
        sum += value * value;
    }
    dino_set_error("");
    return sqrt(sum);
}

XL_EXPORT double dinov3_cosine_native(void* left_handle, void* right_handle)
{
    xvalue* left_shape;
    xvalue* right_shape;
    double dot = 0.0;
    double left_sum = 0.0;
    double right_sum = 0.0;
    int left_count;
    int right_count;
    int i;
    left_shape = dino_shape(left_handle, &left_count);
    if (left_shape == NULL) return 0.0;
    right_shape = dino_shape(right_handle, &right_count);
    xrtValueRelease(left_shape);
    if (right_shape == NULL) return 0.0;
    xrtValueRelease(right_shape);
    if (left_count == 0 || left_count != right_count) {
        dino_set_error("DinoV3 embeddings have different lengths");
        return 0.0;
    }
    for (i = 0; i < left_count; ++i) {
        double left = ort_result_float_at_native(left_handle, i);
        double right = ort_result_float_at_native(right_handle, i);
        if (!isfinite(left) || !isfinite(right)) {
            dino_set_error("non-finite DinoV3 embedding");
            return 0.0;
        }
        dot += left * right;
        left_sum += left * left;
        right_sum += right * right;
    }
    dino_set_error("");
    return left_sum > 0.0 && right_sum > 0.0 ? dot / sqrt(left_sum * right_sum) : 0.0;
}
