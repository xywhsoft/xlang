#ifndef XL6_JSON_WRITER_H
#define XL6_JSON_WRITER_H
#include "src/runtime/xl6_serialization_writer.h"

/* Physical adapters for the declared Writer class. Primitive values use
 * explicit checked carriers; finish transfers exact native byte length. */
static xjsonwriter* xl6_json_stream_writer(void* writer)
{
    return (xjsonwriter*)writer;
}

static bool xl6_json_stream_writer_name(void* writer, xlir_string key)
{
    return writer != 0 && key.Data != 0 && xrtJsonWriterName(xl6_json_stream_writer(writer), key);
}

static inline void* xl6_xrt_json_writer_create_default(void)
{
    xjsonwriteconfig config;
    xrtJsonWriteConfigInit(&config);
    return xrtJsonWriterCreate(&config);
}

static inline void* xl6_xrt_json_writer_create_pretty(bool pretty)
{
    xjsonwriteconfig config;
    xrtJsonWriteConfigInit(&config);
    if ( pretty != 0 ) { config.Flags |= XJSON_WRITE_PRETTY; }
    return xrtJsonWriterCreate(&config);
}

static inline void xl6_xrt_json_writer_destroy(void* writer)
{
    xrtJsonWriterFree(xl6_json_stream_writer(writer));
    return;
}

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_json_writer_begin_object, (void* writer), (writer != 0 && xrtJsonWriterObject(xl6_json_stream_writer(writer))))
XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_json_writer_begin_object_key, (void* writer, xlir_string key), (xl6_json_stream_writer_name(writer, key) && xrtJsonWriterObject(xl6_json_stream_writer(writer))))
XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_json_writer_end_object, (void* writer), (writer != 0 && xrtJsonWriterEnd(xl6_json_stream_writer(writer))))
XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_json_writer_begin_array, (void* writer), (writer != 0 && xrtJsonWriterArray(xl6_json_stream_writer(writer))))
XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_json_writer_begin_array_key, (void* writer, xlir_string key), (xl6_json_stream_writer_name(writer, key) && xrtJsonWriterArray(xl6_json_stream_writer(writer))))
XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_json_writer_end_array, (void* writer), (writer != 0 && xrtJsonWriterEnd(xl6_json_stream_writer(writer))))
XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_json_writer_null, (void* writer), (writer != 0 && xrtJsonWriterNull(xl6_json_stream_writer(writer))))
XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_json_writer_null_key, (void* writer, xlir_string key), (xl6_json_stream_writer_name(writer, key) && xrtJsonWriterNull(xl6_json_stream_writer(writer))))
XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_json_writer_bool, (void* writer, bool value), (writer != 0 && xrtJsonWriterBool(xl6_json_stream_writer(writer), value != 0)))
XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_json_writer_bool_key, (void* writer, xlir_string key, bool value), (xl6_json_stream_writer_name(writer, key) && xrtJsonWriterBool(xl6_json_stream_writer(writer), value != 0)))
XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_json_writer_int, (void* writer, int64_t value), (writer != 0 && xrtJsonWriterInt(xl6_json_stream_writer(writer), value)))
XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_json_writer_int_key, (void* writer, xlir_string key, int64_t value), (xl6_json_stream_writer_name(writer, key) && xrtJsonWriterInt(xl6_json_stream_writer(writer), value)))
XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_json_writer_uint, (void* writer, uint64_t value), (writer != 0 && xrtJsonWriterUInt(xl6_json_stream_writer(writer), value)))
XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_json_writer_uint_key, (void* writer, xlir_string key, uint64_t value), (xl6_json_stream_writer_name(writer, key) && xrtJsonWriterUInt(xl6_json_stream_writer(writer), value)))
XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_json_writer_float, (void* writer, double value), (writer != 0 && xrtJsonWriterFloat(xl6_json_stream_writer(writer), value)))
XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_json_writer_float_key, (void* writer, xlir_string key, double value), (xl6_json_stream_writer_name(writer, key) && xrtJsonWriterFloat(xl6_json_stream_writer(writer), value)))
XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_json_writer_string, (void* writer, xlir_string value), (writer != 0 && value.Data != 0 && xrtJsonWriterString(xl6_json_stream_writer(writer), value)))
XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_json_writer_string_key, (void* writer, xlir_string key, xlir_string value), (value.Data != 0 && xl6_json_stream_writer_name(writer, key) && xrtJsonWriterString(xl6_json_stream_writer(writer), value)))
XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_json_writer_value, (void* writer, xvalue* value), (writer != 0 && value != 0 && xrtJsonWriterValue(xl6_json_stream_writer(writer), value)))
XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_json_writer_value_key, (void* writer, xlir_string key, xvalue* value), (value != 0 && xl6_json_stream_writer_name(writer, key) && xrtJsonWriterValue(xl6_json_stream_writer(writer), value)))
static inline xlir_result_string xl6_xrt_json_writer_finish(void* writer)
{
    xerror* prior = xrtTakeError();
    size_t size = 0;
    char* text = NULL;
    if (writer != NULL && xrtJsonWriterFinish((xjsonwriter*)writer))
        text = xrtJsonWriterTake((xjsonwriter*)writer, &size);
    if (text == NULL) {
        xerror* failure = xrtTakeError();
        if (failure == NULL) { xrtSetErrorKind(XERR_STATE); failure = xrtTakeError(); }
        xrtErrorFree(prior); xrtSetErrorTake(failure);
        return (xlir_result_string){false, {NULL, 0}};
    }
    xrtClearError(); xrtSetErrorTake(prior);
    return (xlir_result_string){true, {text, size}};
}

#endif
