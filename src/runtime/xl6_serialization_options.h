#ifndef XL6_SERIALIZATION_OPTIONS_H
#define XL6_SERIALIZATION_OPTIONS_H

#include <string.h>
#include "xl6_dict.h"

/* Only generated modules using advanced structured-data options include this
 * header. Options are borrowed xvalue objects; every XRT config is initialized
 * first, so omitted fields inherit XRT's bounded defaults. */

static inline bool xl6SerializeOptionObject(const xvalue* options)
{
    if (options == NULL || xrtValueIs(options, XVALUE_NULL) ||
        xrtValueIs(options, XVALUE_OBJECT)) return true;
    int native=xl6DictErasedProbeV1(options);
    if (native) return native>0;
    xrtSetErrorKind(XERR_ARGUMENT);
    return false;
}

static inline bool xl6SerializeOption(const xvalue* options, const char* name,xvalue** output)
{
    if (options == NULL || xrtValueIs(options, XVALUE_NULL)) return true;
    xstrview key={name,strlen(name)};
    int native=xl6DictErasedProbeV1(options);
    if (native<0) return false;
    if (native) { bool found=false; return xl6DictErasedReadV1(options,key,false,output,&found); }
    const xvalue* item=xrtValueObjectGet(options,key);
    if (xrtGetError()) return false;
    if (item) { *output=xrtValueRetain(item); return *output!=NULL; }
    return true;
}
typedef struct XL6_SerializeKeyNames {
    const char* const* names;
    size_t count;
} XL6_SerializeKeyNames;
static inline bool xl6SerializeOptionKey(xstrview key,void* context)
{
    const XL6_SerializeKeyNames* allowed=context;
    for(size_t i=0;i<allowed->count;++i) {
        size_t size=strlen(allowed->names[i]);
        if(key.Size==size && !memcmp(key.Data,allowed->names[i],size)) return true;
    }
    xrtSetErrorKind(XERR_ARGUMENT); return false;
}

static inline bool xl6SerializeOptionKeys(const xvalue* options,
    const char* const* names, size_t nameCount)
{
    size_t count, i, j;
    if (!xl6SerializeOptionObject(options)) return false;
    if (options == NULL || xrtValueIs(options, XVALUE_NULL)) return true;
    int native=xl6DictErasedProbeV1(options);
    if (native<0) return false;
    if (native) {
        XL6_SerializeKeyNames allowed={names,nameCount};
        return xl6DictErasedVisitKeysV1(options,xl6SerializeOptionKey,&allowed);
    }
    count = xrtValueCount(options);
    for (i = 0; i < count; ++i) {
        xstrview key = {0};
        bool recognized = false;
        if (xrtValueObjectAt(options, i, &key) == NULL) return false;
        for (j = 0; j < nameCount; ++j) {
            size_t size = strlen(names[j]);
            if (key.Size == size && memcmp(key.Data, names[j], size) == 0) {
                recognized = true;
                break;
            }
        }
        if (!recognized) {
            xrtSetErrorKind(XERR_ARGUMENT);
            return false;
        }
    }
    return true;
}

#define XL6_SERIALIZE_KEYS(Options, Names) \
    xl6SerializeOptionKeys((Options), (Names), sizeof(Names) / sizeof((Names)[0]))

static inline bool xl6SerializeOptionBool(const xvalue* options, const char* name,
    uint32_t bit, uint32_t* flags)
{
    xvalue* value=NULL;
    if (!xl6SerializeOption(options,name,&value)) return false;
    bool enabled;
    if (value == NULL) return true;
    if (!xrtValueGetBool(value, &enabled)) return xl6DictValueFinishReadV1(value,false);
    if (enabled) *flags |= bit;
    else *flags &= ~bit;
    return xl6DictValueFinishReadV1(value,true);
}

static inline bool xl6SerializeOptionEnum(const xvalue* options, const char* name,
    int maximum, int* result)
{
    xvalue* value=NULL;
    if (!xl6SerializeOption(options,name,&value)) return false;
    int64_t number;
    if (value == NULL) return true;
    if (!xrtValueGetInt(value, &number)) return xl6DictValueFinishReadV1(value,false);
    if (number < 0 || number > maximum) {
        xrtSetErrorKind(XERR_RANGE);
        return xl6DictValueFinishReadV1(value,false);
    }
    *result = (int)number;
    return xl6DictValueFinishReadV1(value,true);
}

static inline bool xl6SerializeOptionU32(const xvalue* options, const char* name,
    uint32_t* result)
{
    xvalue* value=NULL;
    if (!xl6SerializeOption(options,name,&value)) return false;
    int64_t number;
    if (value == NULL) return true;
    if (!xrtValueGetInt(value, &number)) return xl6DictValueFinishReadV1(value,false);
    if (number == -1) return xl6DictValueFinishReadV1(value,true);
    if (number < 0 || (uint64_t)number > UINT32_MAX) {
        xrtSetErrorKind(XERR_RANGE);
        return xl6DictValueFinishReadV1(value,false);
    }
    *result = (uint32_t)number;
    return xl6DictValueFinishReadV1(value,true);
}

static inline bool xl6SerializeOptionSize(const xvalue* options, const char* name,
    size_t* result)
{
    xvalue* value=NULL;
    if (!xl6SerializeOption(options,name,&value)) return false;
    int64_t number;
    if (value == NULL) return true;
    if (!xrtValueGetInt(value, &number)) return xl6DictValueFinishReadV1(value,false);
    if (number == -1) return xl6DictValueFinishReadV1(value,true);
    if (number < 0 || (uint64_t)number > (uint64_t)SIZE_MAX) {
        xrtSetErrorKind(XERR_RANGE);
        return xl6DictValueFinishReadV1(value,false);
    }
    *result = (size_t)number;
    return xl6DictValueFinishReadV1(value,true);
}

static inline bool xl6JsonReadOptions(const xvalue* options, xjsonreadconfig* config)
{
    static const char* const keys[] = { "comments", "trailingComma",
        "duplicate", "bigInteger", "maxDepth", "maxInputBytes",
        "maxStringBytes", "maxValues", "maxContainerItems" };
    int setting;
    xrtJsonReadConfigInit(config);
    if (!XL6_SERIALIZE_KEYS(options, keys) ||
        !xl6SerializeOptionBool(options, "comments", XJSON_READ_COMMENTS, &config->Flags) ||
        !xl6SerializeOptionBool(options, "trailingComma", XJSON_READ_TRAILING_COMMA, &config->Flags)) return false;
    setting = (int)config->Duplicate;
    if (!xl6SerializeOptionEnum(options, "duplicate", XJSON_DUPLICATE_REPLACE, &setting)) return false;
    config->Duplicate = (xjsonduplicate)setting;
    setting = (int)config->BigInteger;
    if (!xl6SerializeOptionEnum(options, "bigInteger", XJSON_BIGINT_FLOAT, &setting)) return false;
    config->BigInteger = (xjsonbigint)setting;
    return xl6SerializeOptionU32(options, "maxDepth", &config->MaxDepth) &&
        xl6SerializeOptionSize(options, "maxInputBytes", &config->MaxInputBytes) &&
        xl6SerializeOptionSize(options, "maxStringBytes", &config->MaxStringBytes) &&
        xl6SerializeOptionSize(options, "maxValues", &config->MaxValues) &&
        xl6SerializeOptionSize(options, "maxContainerItems", &config->MaxContainerItems);
}

static inline bool xl6JsonWriteOptions(const xvalue* options, xjsonwriteconfig* config)
{
    static const char* const keys[] = { "pretty", "escapeSlash", "escapeHtml",
        "escapeNonAscii", "containerCompat", "nonFinite", "unsupported",
        "maxDepth", "indent", "maxOutputBytes" };
    int setting;
    xrtJsonWriteConfigInit(config);
    if (!XL6_SERIALIZE_KEYS(options, keys) ||
        !xl6SerializeOptionBool(options, "pretty", XJSON_WRITE_PRETTY, &config->Flags) ||
        !xl6SerializeOptionBool(options, "escapeSlash", XJSON_WRITE_ESCAPE_SLASH, &config->Flags) ||
        !xl6SerializeOptionBool(options, "escapeHtml", XJSON_WRITE_ESCAPE_HTML, &config->Flags) ||
        !xl6SerializeOptionBool(options, "escapeNonAscii", XJSON_WRITE_ESCAPE_NON_ASCII, &config->Flags) ||
        !xl6SerializeOptionBool(options, "containerCompat", XJSON_WRITE_CONTAINER_COMPAT, &config->Flags)) return false;
    setting = (int)config->NonFinite;
    if (!xl6SerializeOptionEnum(options, "nonFinite", XJSON_NONFINITE_STRING, &setting)) return false;
    config->NonFinite = (xjsonnonfinite)setting;
    setting = (int)config->Unsupported;
    if (!xl6SerializeOptionEnum(options, "unsupported", XJSON_UNSUPPORTED_SKIP, &setting)) return false;
    config->Unsupported = (xjsonunsupported)setting;
    return xl6SerializeOptionU32(options, "maxDepth", &config->MaxDepth) &&
        xl6SerializeOptionU32(options, "indent", &config->Indent) &&
        xl6SerializeOptionSize(options, "maxOutputBytes", &config->MaxOutputBytes);
}

static inline bool xl6JsonQuoteOptions(const xvalue* options, uint32_t* flags)
{
    static const char* const keys[] = { "escapeSlash", "escapeHtml", "escapeNonAscii" };
    *flags = 0;
    return XL6_SERIALIZE_KEYS(options, keys) &&
        xl6SerializeOptionBool(options, "escapeSlash", XJSON_WRITE_ESCAPE_SLASH, flags) &&
        xl6SerializeOptionBool(options, "escapeHtml", XJSON_WRITE_ESCAPE_HTML, flags) &&
        xl6SerializeOptionBool(options, "escapeNonAscii", XJSON_WRITE_ESCAPE_NON_ASCII, flags);
}

static inline bool xl6XlonReadOptions(const xvalue* options, xxlonreadconfig* config)
{
    static const char* const keys[] = { "comments", "trailingComma",
        "duplicate", "bigInteger", "maxDepth", "maxInputBytes",
        "maxStringBytes", "maxValues", "maxContainerItems", "maxDecodedBytes" };
    int setting;
    xrtXlonReadConfigInit(config);
    if (!XL6_SERIALIZE_KEYS(options, keys) ||
        !xl6SerializeOptionBool(options, "comments", XXLON_READ_COMMENTS, &config->Flags) ||
        !xl6SerializeOptionBool(options, "trailingComma", XXLON_READ_TRAILING_COMMA, &config->Flags)) return false;
    setting = (int)config->Duplicate;
    if (!xl6SerializeOptionEnum(options, "duplicate", XXLON_DUPLICATE_REPLACE, &setting)) return false;
    config->Duplicate = (xxlonduplicate)setting;
    setting = (int)config->BigInteger;
    if (!xl6SerializeOptionEnum(options, "bigInteger", XXLON_BIGINT_FLOAT, &setting)) return false;
    config->BigInteger = (xxlonbigint)setting;
    return xl6SerializeOptionU32(options, "maxDepth", &config->MaxDepth) &&
        xl6SerializeOptionSize(options, "maxInputBytes", &config->MaxInputBytes) &&
        xl6SerializeOptionSize(options, "maxStringBytes", &config->MaxStringBytes) &&
        xl6SerializeOptionSize(options, "maxValues", &config->MaxValues) &&
        xl6SerializeOptionSize(options, "maxContainerItems", &config->MaxContainerItems) &&
        xl6SerializeOptionSize(options, "maxDecodedBytes", &config->MaxDecodedBytes);
}

static inline bool xl6XlonWriteOptions(const xvalue* options, xxlonwriteconfig* config)
{
    static const char* const keys[] = { "pretty", "escapeSlash", "escapeHtml",
        "escapeNonAscii", "unsupported", "maxDepth", "indent", "maxOutputBytes" };
    int setting;
    xrtXlonWriteConfigInit(config);
    if (!XL6_SERIALIZE_KEYS(options, keys) ||
        !xl6SerializeOptionBool(options, "pretty", XXLON_WRITE_PRETTY, &config->Flags) ||
        !xl6SerializeOptionBool(options, "escapeSlash", XXLON_WRITE_ESCAPE_SLASH, &config->Flags) ||
        !xl6SerializeOptionBool(options, "escapeHtml", XXLON_WRITE_ESCAPE_HTML, &config->Flags) ||
        !xl6SerializeOptionBool(options, "escapeNonAscii", XXLON_WRITE_ESCAPE_NON_ASCII, &config->Flags)) return false;
    setting = (int)config->Unsupported;
    if (!xl6SerializeOptionEnum(options, "unsupported", XXLON_UNSUPPORTED_SKIP, &setting)) return false;
    config->Unsupported = (xxlonunsupported)setting;
    return xl6SerializeOptionU32(options, "maxDepth", &config->MaxDepth) &&
        xl6SerializeOptionU32(options, "indent", &config->Indent) &&
        xl6SerializeOptionSize(options, "maxOutputBytes", &config->MaxOutputBytes);
}

static inline bool xl6JsonlReadOptions(const xvalue* options, xjsonlreadconfig* config)
{
    static const char* const keys[] = { "record", "rejectEmptyLines",
        "maxInputBytes", "maxRecords", "maxTotalValues" };
    xrtJsonlReadConfigInit(config);
    xvalue* record=NULL;
    if (!XL6_SERIALIZE_KEYS(options,keys) || !xl6SerializeOption(options,"record",&record)) return false;
    if (!xl6DictValueFinishReadV1(record,xl6JsonReadOptions(record,&config->Record))) return false;
    return
        xl6SerializeOptionBool(options, "rejectEmptyLines", XJSONL_READ_REJECT_EMPTY_LINES, &config->Flags) &&
        xl6SerializeOptionSize(options, "maxInputBytes", &config->MaxInputBytes) &&
        xl6SerializeOptionSize(options, "maxRecords", &config->MaxRecords) &&
        xl6SerializeOptionSize(options, "maxTotalValues", &config->MaxTotalValues);
}

static inline bool xl6JsonlWriteOptions(const xvalue* options, xjsonlwriteconfig* config)
{
    static const char* const keys[] = { "record", "maxOutputBytes", "maxRecords" };
    xrtJsonlWriteConfigInit(config);
    xvalue* record=NULL;
    if (!XL6_SERIALIZE_KEYS(options,keys) || !xl6SerializeOption(options,"record",&record)) return false;
    if (!xl6DictValueFinishReadV1(record,xl6JsonWriteOptions(record,&config->Record))) return false;
    return
        xl6SerializeOptionSize(options, "maxOutputBytes", &config->MaxOutputBytes) &&
        xl6SerializeOptionSize(options, "maxRecords", &config->MaxRecords);
}

static inline bool xl6XlonlReadOptions(const xvalue* options, xxlonlreadconfig* config)
{
    static const char* const keys[] = { "record", "rejectEmptyLines",
        "maxInputBytes", "maxRecords", "maxTotalValues", "maxTotalDecodedBytes" };
    xrtXlonlReadConfigInit(config);
    xvalue* record=NULL;
    if (!XL6_SERIALIZE_KEYS(options,keys) || !xl6SerializeOption(options,"record",&record)) return false;
    if (!xl6DictValueFinishReadV1(record,xl6XlonReadOptions(record,&config->Record))) return false;
    return
        xl6SerializeOptionBool(options, "rejectEmptyLines", XXLONL_READ_REJECT_EMPTY_LINES, &config->Flags) &&
        xl6SerializeOptionSize(options, "maxInputBytes", &config->MaxInputBytes) &&
        xl6SerializeOptionSize(options, "maxRecords", &config->MaxRecords) &&
        xl6SerializeOptionSize(options, "maxTotalValues", &config->MaxTotalValues) &&
        xl6SerializeOptionSize(options, "maxTotalDecodedBytes", &config->MaxTotalDecodedBytes);
}

static inline bool xl6XlonlWriteOptions(const xvalue* options, xxlonlwriteconfig* config)
{
    static const char* const keys[] = { "record", "maxOutputBytes", "maxRecords" };
    xrtXlonlWriteConfigInit(config);
    xvalue* record=NULL;
    if (!XL6_SERIALIZE_KEYS(options,keys) || !xl6SerializeOption(options,"record",&record)) return false;
    if (!xl6DictValueFinishReadV1(record,xl6XlonWriteOptions(record,&config->Record))) return false;
    return
        xl6SerializeOptionSize(options, "maxOutputBytes", &config->MaxOutputBytes) &&
        xl6SerializeOptionSize(options, "maxRecords", &config->MaxRecords);
}

static inline bool xl6SerializeBufferWrite(xbytesview data, void* context)
{
    return xrtBufferAppend((xbuffer*)context, data);
}

/* XRT's configured write APIs are callback-based. Keep exactly one copy of
 * their output and transfer it to the ordinary language string owner. */
static inline char* xl6SerializeBufferTake(xbuffer* buffer)
{
    char* result;
    if (buffer == NULL) return NULL;
    if (!xrtBufferAppendByte(buffer, 0)) {
        xrtBufferDestroy(buffer);
        return NULL;
    }
    result = (char*)xrtBufferTake(buffer, NULL, NULL);
    xrtBufferDestroy(buffer);
    return result;
}

#undef XL6_SERIALIZE_KEYS

#endif
