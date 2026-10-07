#ifndef XL6_XLON_WRITER_H
#define XL6_XLON_WRITER_H
#include "src/runtime/xl6_serialization_writer.h"

/* Physical adapters for the declared Writer class. Primitive values use
 * explicit checked carriers; finish transfers exact native byte length. */
static xxlonwriteconfig xl6_xlon_writer_config(bool pretty, bool ignoreUnsupported)
{
    xxlonwriteconfig config;
    xrtXlonWriteConfigInit(&config);
    if ( pretty ) { config.Flags |= XXLON_WRITE_PRETTY; }
    if ( ignoreUnsupported ) { config.Unsupported = XXLON_UNSUPPORTED_SKIP; }
    return config;
}

static xxlonwriter* xl6_xlon_writer(void* writer)
{
    return (xxlonwriter*)writer;
}

static bool xl6_xlon_writer_name(void* writer, xlir_string key)
{
    return writer != 0 && key.Data != 0 && xrtXlonWriterName(xl6_xlon_writer(writer), key);
}

static bool xl6_xlon_writer_key(void* writer, int64_t key)
{
    return writer != 0 && xrtXlonWriterKey(xl6_xlon_writer(writer), key);
}

static inline void* xl6_xrt_xlon_writer_create_default(void)
{
    xxlonwriteconfig config = xl6_xlon_writer_config(false, false);
    return xrtXlonWriterCreate(&config);
}

static inline void* xl6_xrt_xlon_writer_create_pretty(bool pretty)
{
    xxlonwriteconfig config = xl6_xlon_writer_config(pretty != 0, false);
    return xrtXlonWriterCreate(&config);
}

static inline void* xl6_xrt_xlon_writer_create_options(bool pretty, bool ignoreEncode, bool ignoreDecode)
{
    xxlonwriteconfig config = xl6_xlon_writer_config(pretty != 0, ignoreEncode != 0);
    (void)ignoreDecode;
    return xrtXlonWriterCreate(&config);
}

static inline void xl6_xrt_xlon_writer_destroy(void* writer)
{
    xrtXlonWriterFree(xl6_xlon_writer(writer));
    return;
}

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_begin_array, (void* writer), (writer != 0 && xrtXlonWriterArray(xl6_xlon_writer(writer))))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_begin_array_key, (void* writer, xlir_string key), (xl6_xlon_writer_name(writer, key) && xrtXlonWriterArray(xl6_xlon_writer(writer))))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_begin_array_index, (void* writer, int64_t index), (xl6_xlon_writer_key(writer, index) && xrtXlonWriterArray(xl6_xlon_writer(writer))))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_end_array, (void* writer), (writer != 0 && xrtXlonWriterEnd(xl6_xlon_writer(writer))))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_begin_list, (void* writer), (writer != 0 && xrtXlonWriterIntMap(xl6_xlon_writer(writer))))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_begin_list_key, (void* writer, xlir_string key), (xl6_xlon_writer_name(writer, key) && xrtXlonWriterIntMap(xl6_xlon_writer(writer))))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_begin_list_index, (void* writer, int64_t index), (xl6_xlon_writer_key(writer, index) && xrtXlonWriterIntMap(xl6_xlon_writer(writer))))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_end_list, (void* writer), (writer != 0 && xrtXlonWriterEnd(xl6_xlon_writer(writer))))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_begin_dict, (void* writer), (writer != 0 && xrtXlonWriterObject(xl6_xlon_writer(writer))))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_begin_dict_key, (void* writer, xlir_string key), (xl6_xlon_writer_name(writer, key) && xrtXlonWriterObject(xl6_xlon_writer(writer))))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_begin_dict_index, (void* writer, int64_t index), (xl6_xlon_writer_key(writer, index) && xrtXlonWriterObject(xl6_xlon_writer(writer))))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_end_dict, (void* writer), (writer != 0 && xrtXlonWriterEnd(xl6_xlon_writer(writer))))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_begin_set, (void* writer), (writer != 0 && xrtXlonWriterSet(xl6_xlon_writer(writer))))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_begin_set_key, (void* writer, xlir_string key), (xl6_xlon_writer_name(writer, key) && xrtXlonWriterSet(xl6_xlon_writer(writer))))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_begin_set_index, (void* writer, int64_t index), (xl6_xlon_writer_key(writer, index) && xrtXlonWriterSet(xl6_xlon_writer(writer))))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_end_set, (void* writer), (writer != 0 && xrtXlonWriterEnd(xl6_xlon_writer(writer))))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_null, (void* writer), (writer != 0 && xrtXlonWriterNull(xl6_xlon_writer(writer))))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_null_key, (void* writer, xlir_string key), (xl6_xlon_writer_name(writer, key) && xrtXlonWriterNull(xl6_xlon_writer(writer))))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_null_index, (void* writer, int64_t index), (xl6_xlon_writer_key(writer, index) && xrtXlonWriterNull(xl6_xlon_writer(writer))))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_bool, (void* writer, bool value), (writer != 0 && xrtXlonWriterBool(xl6_xlon_writer(writer), value != 0)))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_bool_key, (void* writer, xlir_string key, bool value), (xl6_xlon_writer_name(writer, key) && xrtXlonWriterBool(xl6_xlon_writer(writer), value != 0)))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_bool_index, (void* writer, int64_t index, bool value), (xl6_xlon_writer_key(writer, index) && xrtXlonWriterBool(xl6_xlon_writer(writer), value != 0)))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_int, (void* writer, int64_t value), (writer != 0 && xrtXlonWriterInt(xl6_xlon_writer(writer), value)))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_int_key, (void* writer, xlir_string key, int64_t value), (xl6_xlon_writer_name(writer, key) && xrtXlonWriterInt(xl6_xlon_writer(writer), value)))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_int_index, (void* writer, int64_t index, int64_t value), (xl6_xlon_writer_key(writer, index) && xrtXlonWriterInt(xl6_xlon_writer(writer), value)))
XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_uint, (void* writer, uint64_t value), (writer != 0 && xrtXlonWriterUInt(xl6_xlon_writer(writer), value)))
XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_uint_key, (void* writer, xlir_string key, uint64_t value), (xl6_xlon_writer_name(writer, key) && xrtXlonWriterUInt(xl6_xlon_writer(writer), value)))
XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_uint_index, (void* writer, int64_t index, uint64_t value), (xl6_xlon_writer_key(writer, index) && xrtXlonWriterUInt(xl6_xlon_writer(writer), value)))
XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_char, (void* writer, uint32_t value), (writer != 0 && xrtXlonWriterChar(xl6_xlon_writer(writer), value)))
XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_char_key, (void* writer, xlir_string key, uint32_t value), (xl6_xlon_writer_name(writer, key) && xrtXlonWriterChar(xl6_xlon_writer(writer), value)))
XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_char_index, (void* writer, int64_t index, uint32_t value), (xl6_xlon_writer_key(writer, index) && xrtXlonWriterChar(xl6_xlon_writer(writer), value)))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_float, (void* writer, double value), (writer != 0 && xrtXlonWriterFloat(xl6_xlon_writer(writer), value)))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_float_key, (void* writer, xlir_string key, double value), (xl6_xlon_writer_name(writer, key) && xrtXlonWriterFloat(xl6_xlon_writer(writer), value)))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_float_index, (void* writer, int64_t index, double value), (xl6_xlon_writer_key(writer, index) && xrtXlonWriterFloat(xl6_xlon_writer(writer), value)))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_string, (void* writer, xlir_string value), (writer != 0 && value.Data != 0 && xrtXlonWriterString(xl6_xlon_writer(writer), value)))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_string_key, (void* writer, xlir_string key, xlir_string value), (value.Data != 0 && xl6_xlon_writer_name(writer, key) && xrtXlonWriterString(xl6_xlon_writer(writer), value)))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_string_index, (void* writer, int64_t index, xlir_string value), (value.Data != 0 && xl6_xlon_writer_key(writer, index) && xrtXlonWriterString(xl6_xlon_writer(writer), value)))
static inline xlir_result_bool xl6_xrt_xlon_writer_bytes(void* writer, xvalue* value)
{
    xbytesview data; xerror* prior = xrtTakeError(); bool result = false;
    bool written = writer != NULL && value != NULL && xrtValueGetBytes(value, &data) &&
        xrtXlonWriterBytes(xl6_xlon_writer(writer), data);
    bool status = xl6SerializeWriterFinishV1(written, prior, &result);
    return (xlir_result_bool){status, result};
}
static inline xlir_result_bool xl6_xrt_xlon_writer_bytes_key(void* writer, xlir_string key, xvalue* value)
{
    xbytesview data; xerror* prior = xrtTakeError(); bool result = false;
    bool written = value != NULL && xrtValueGetBytes(value, &data) &&
        xl6_xlon_writer_name(writer, key) && xrtXlonWriterBytes(xl6_xlon_writer(writer), data);
    bool status = xl6SerializeWriterFinishV1(written, prior, &result);
    return (xlir_result_bool){status, result};
}
static inline xlir_result_bool xl6_xrt_xlon_writer_bytes_index(void* writer, int64_t index, xvalue* value)
{
    xbytesview data; xerror* prior = xrtTakeError(); bool result = false;
    bool written = value != NULL && xrtValueGetBytes(value, &data) &&
        xl6_xlon_writer_key(writer, index) && xrtXlonWriterBytes(xl6_xlon_writer(writer), data);
    bool status = xl6SerializeWriterFinishV1(written, prior, &result);
    return (xlir_result_bool){status, result};
}
XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_tag, (void* writer, xlir_string tag, xlir_string payload), (writer != 0 && tag.Data != 0 && payload.Data != 0 && xrtXlonWriterTag(xl6_xlon_writer(writer), tag, payload)))
XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_tag_key, (void* writer, xlir_string key, xlir_string tag, xlir_string payload), (tag.Data != 0 && payload.Data != 0 && xl6_xlon_writer_name(writer, key) && xrtXlonWriterTag(xl6_xlon_writer(writer), tag, payload)))
XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_tag_index, (void* writer, int64_t index, xlir_string tag, xlir_string payload), (tag.Data != 0 && payload.Data != 0 && xl6_xlon_writer_key(writer, index) && xrtXlonWriterTag(xl6_xlon_writer(writer), tag, payload)))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_time, (void* writer, int64_t value), (writer != 0 && xrtXlonWriterTime(xl6_xlon_writer(writer), value)))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_time_key, (void* writer, xlir_string key, int64_t value), (xl6_xlon_writer_name(writer, key) && xrtXlonWriterTime(xl6_xlon_writer(writer), value)))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_time_index, (void* writer, int64_t index, int64_t value), (xl6_xlon_writer_key(writer, index) && xrtXlonWriterTime(xl6_xlon_writer(writer), value)))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_value, (void* writer, xvalue* value), (writer != 0 && value != 0 && xrtXlonWriterValue(xl6_xlon_writer(writer), value)))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_value_key, (void* writer, xlir_string key, xvalue* value), (value != 0 && xl6_xlon_writer_name(writer, key) && xrtXlonWriterValue(xl6_xlon_writer(writer), value)))

XL6_SERIALIZATION_WRITER_STEP_V1(xl6_xrt_xlon_writer_value_index, (void* writer, int64_t index, xvalue* value), (value != 0 && xl6_xlon_writer_key(writer, index) && xrtXlonWriterValue(xl6_xlon_writer(writer), value)))

static inline xlir_result_string xl6_xrt_xlon_writer_finish(void* writer)
{
    xerror* prior = xrtTakeError();
    size_t size = 0;
    char* text = NULL;
    if (writer != NULL && xrtXlonWriterFinish((xxlonwriter*)writer))
        text = xrtXlonWriterTake((xxlonwriter*)writer, &size);
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
