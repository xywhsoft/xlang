#include "src/abi/xl6_xapi_load.h"

#include "src/common/xl6_common.h"

typedef struct XL6_XapiJson {
    const char* path;
    const char* text;
    size_t len;
    size_t pos;
    XL6_DiagList* diags;
} XL6_XapiJson;

static void xl6XapiJsonError(XL6_XapiJson* json, const char* message)
{
    if ( json != NULL && json->diags != NULL ) {
        xl6DiagAdd(json->diags, 1, 1, message != NULL ? message : "XAPI JSON 解析失败");
    }
}

static void xl6XapiJsonSkipWs(XL6_XapiJson* json)
{
    while ( json != NULL && json->pos < json->len ) {
        char ch = json->text[json->pos];
        if ( ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n' ) {
            break;
        }
        json->pos += 1;
    }
}

static int xl6XapiJsonPeek(XL6_XapiJson* json)
{
    xl6XapiJsonSkipWs(json);
    if ( json == NULL || json->pos >= json->len ) {
        return 0;
    }
    return (unsigned char)json->text[json->pos];
}

static int xl6XapiJsonConsume(XL6_XapiJson* json, char ch)
{
    xl6XapiJsonSkipWs(json);
    if ( json == NULL || json->pos >= json->len || json->text[json->pos] != ch ) {
        return 0;
    }
    json->pos += 1;
    return 1;
}

static int xl6XapiJsonAppendChar(char** out, size_t* len, size_t* capacity, char ch)
{
    char* newData;
    size_t newCapacity;

    if ( out == NULL || len == NULL || capacity == NULL ) {
        return 0;
    }
    if ( *len + 2 > *capacity ) {
        newCapacity = *capacity == 0 ? 32 : *capacity * 2;
        while ( *len + 2 > newCapacity ) {
            newCapacity *= 2;
        }
        newData = (char*)xrtRealloc(*out, newCapacity);
        if ( newData == NULL ) {
            return 0;
        }
        *out = newData;
        *capacity = newCapacity;
    }
    (*out)[*len] = ch;
    *len += 1;
    (*out)[*len] = '\0';
    return 1;
}

static char* xl6XapiJsonReadString(XL6_XapiJson* json)
{
    char* out;
    size_t len;
    size_t capacity;

    if ( !xl6XapiJsonConsume(json, '"') ) {
        xl6XapiJsonError(json, "XAPI JSON 需要字符串");
        return NULL;
    }

    out = NULL;
    len = 0;
    capacity = 0;
    while ( json->pos < json->len ) {
        char ch = json->text[json->pos++];
        if ( ch == '"' ) {
            if ( out == NULL ) {
                out = xrtCopyStr("", 0);
            }
            return out;
        }
        if ( ch == '\\' ) {
            if ( json->pos >= json->len ) {
                break;
            }
            ch = json->text[json->pos++];
            switch ( ch ) {
            case '"': ch = '"'; break;
            case '\\': ch = '\\'; break;
            case '/': ch = '/'; break;
            case 'b': ch = '\b'; break;
            case 'f': ch = '\f'; break;
            case 'n': ch = '\n'; break;
            case 'r': ch = '\r'; break;
            case 't': ch = '\t'; break;
            case 'u':
                if ( json->pos + 4 <= json->len ) {
                    json->pos += 4;
                }
                ch = '?';
                break;
            default:
                break;
            }
        }
        if ( !xl6XapiJsonAppendChar(&out, &len, &capacity, ch) ) {
            xrtFree(out);
            xl6XapiJsonError(json, "XAPI JSON 字符串内存不足");
            return NULL;
        }
    }
    xrtFree(out);
    xl6XapiJsonError(json, "XAPI JSON 字符串未闭合");
    return NULL;
}

static int xl6XapiJsonReadBool(XL6_XapiJson* json, int* outValue)
{
    xl6XapiJsonSkipWs(json);
    if ( json->pos + 4 <= json->len && memcmp(json->text + json->pos, "true", 4) == 0 ) {
        json->pos += 4;
        if ( outValue != NULL ) {
            *outValue = 1;
        }
        return 1;
    }
    if ( json->pos + 5 <= json->len && memcmp(json->text + json->pos, "false", 5) == 0 ) {
        json->pos += 5;
        if ( outValue != NULL ) {
            *outValue = 0;
        }
        return 1;
    }
    xl6XapiJsonError(json, "XAPI JSON 需要布尔值");
    return 0;
}

static int xl6XapiJsonReadInt(XL6_XapiJson* json, int* outValue)
{
    int sign;
    int value;
    int hasDigit;

    xl6XapiJsonSkipWs(json);
    sign = 1;
    value = 0;
    hasDigit = 0;
    if ( json->pos < json->len && json->text[json->pos] == '-' ) {
        sign = -1;
        json->pos += 1;
    }
    while ( json->pos < json->len && json->text[json->pos] >= '0' && json->text[json->pos] <= '9' ) {
        hasDigit = 1;
        value = value * 10 + (json->text[json->pos] - '0');
        json->pos += 1;
    }
    if ( !hasDigit ) {
        xl6XapiJsonError(json, "XAPI JSON 需要整数");
        return 0;
    }
    if ( outValue != NULL ) {
        *outValue = value * sign;
    }
    return 1;
}

static int xl6XapiJsonSkipValue(XL6_XapiJson* json);

static int xl6XapiJsonSkipObject(XL6_XapiJson* json)
{
    if ( !xl6XapiJsonConsume(json, '{') ) {
        return 0;
    }
    if ( xl6XapiJsonConsume(json, '}') ) {
        return 1;
    }
    for (;;) {
        char* key = xl6XapiJsonReadString(json);
        if ( key == NULL ) {
            return 0;
        }
        xrtFree(key);
        if ( !xl6XapiJsonConsume(json, ':') || !xl6XapiJsonSkipValue(json) ) {
            return 0;
        }
        if ( xl6XapiJsonConsume(json, '}') ) {
            return 1;
        }
        if ( !xl6XapiJsonConsume(json, ',') ) {
            xl6XapiJsonError(json, "XAPI JSON 对象缺少逗号");
            return 0;
        }
    }
}

static int xl6XapiJsonSkipArray(XL6_XapiJson* json)
{
    if ( !xl6XapiJsonConsume(json, '[') ) {
        return 0;
    }
    if ( xl6XapiJsonConsume(json, ']') ) {
        return 1;
    }
    for (;;) {
        if ( !xl6XapiJsonSkipValue(json) ) {
            return 0;
        }
        if ( xl6XapiJsonConsume(json, ']') ) {
            return 1;
        }
        if ( !xl6XapiJsonConsume(json, ',') ) {
            xl6XapiJsonError(json, "XAPI JSON 数组缺少逗号");
            return 0;
        }
    }
}

static int xl6XapiJsonSkipValue(XL6_XapiJson* json)
{
    int ch;

    ch = xl6XapiJsonPeek(json);
    if ( ch == '"' ) {
        char* text = xl6XapiJsonReadString(json);
        xrtFree(text);
        return text != NULL;
    }
    if ( ch == '{' ) {
        return xl6XapiJsonSkipObject(json);
    }
    if ( ch == '[' ) {
        return xl6XapiJsonSkipArray(json);
    }
    if ( ch == 't' || ch == 'f' ) {
        return xl6XapiJsonReadBool(json, NULL);
    }
    if ( ch == 'n' && json->pos + 4 <= json->len && memcmp(json->text + json->pos, "null", 4) == 0 ) {
        json->pos += 4;
        return 1;
    }
    return xl6XapiJsonReadInt(json, NULL);
}

static int xl6XapiAppendParam(XL6_XapiExport* item, XL6_XapiParam* param)
{
    XL6_XapiParam* newParams;

    if ( item == NULL || param == NULL ) {
        return 0;
    }
    newParams = (XL6_XapiParam*)xrtRealloc(item->params, (item->paramCount + 1) * sizeof(XL6_XapiParam));
    if ( newParams == NULL ) {
        return 0;
    }
    item->params = newParams;
    item->params[item->paramCount] = *param;
    item->paramCount += 1;
    memset(param, 0, sizeof(*param));
    return 1;
}

static int xl6XapiAppendReturn(XL6_XapiExport* item, char* type)
{
    char** newReturns;

    if ( item == NULL || type == NULL ) {
        return 0;
    }
    newReturns = (char**)xrtRealloc(item->returns, (item->returnCount + 1) * sizeof(char*));
    if ( newReturns == NULL ) {
        return 0;
    }
    item->returns = newReturns;
    item->returns[item->returnCount] = type;
    item->returnCount += 1;
    return 1;
}

static int xl6XapiAppendField(XL6_XapiTypeDecl* item, XL6_XapiField* field)
{
    XL6_XapiField* newFields;

    if ( item == NULL || field == NULL ) {
        return 0;
    }
    newFields = (XL6_XapiField*)xrtRealloc(item->fields, (item->fieldCount + 1) * sizeof(XL6_XapiField));
    if ( newFields == NULL ) {
        return 0;
    }
    item->fields = newFields;
    item->fields[item->fieldCount] = *field;
    item->fieldCount += 1;
    memset(field, 0, sizeof(*field));
    return 1;
}

static char* xl6XapiLoadDupText(const char* text)
{
    const char* src = text != NULL ? text : "";
    return xrtCopyStr((char*)src, strlen(src));
}

static int xl6XapiAppendString(char*** items, size_t* count, char* text)
{
    char** newItems;

    if ( items == NULL || count == NULL || text == NULL ) {
        return 0;
    }
    newItems = (char**)xrtRealloc(*items, (*count + 1) * sizeof(char*));
    if ( newItems == NULL ) {
        return 0;
    }
    *items = newItems;
    (*items)[*count] = text;
    *count += 1;
    return 1;
}

static int xl6XapiParseStringArray(XL6_XapiJson* json, char*** items, size_t* count)
{
    if ( !xl6XapiJsonConsume(json, '[') ) {
        return 0;
    }
    if ( xl6XapiJsonConsume(json, ']') ) {
        return 1;
    }
    for (;;) {
        char* text = xl6XapiJsonReadString(json);
        if ( text == NULL || !xl6XapiAppendString(items, count, text) ) {
            xrtFree(text);
            return 0;
        }
        if ( xl6XapiJsonConsume(json, ']') ) {
            return 1;
        }
        if ( !xl6XapiJsonConsume(json, ',') ) {
            return 0;
        }
    }
}

static int xl6XapiParseDocValue(XL6_XapiJson* json, char** outDoc)
{
    if ( outDoc == NULL ) {
        return 0;
    }
    if ( xl6XapiJsonPeek(json) == '"' ) {
        xrtFree(*outDoc);
        *outDoc = xl6XapiJsonReadString(json);
        return *outDoc != NULL;
    }
    if ( !xl6XapiJsonConsume(json, '{') ) {
        return 0;
    }
    if ( xl6XapiJsonConsume(json, '}') ) {
        return 1;
    }
    for (;;) {
        char* key = xl6XapiJsonReadString(json);
        if ( key == NULL || !xl6XapiJsonConsume(json, ':') ) {
            xrtFree(key);
            return 0;
        }
        if ( strcmp(key, "raw") == 0 ) {
            xrtFree(*outDoc);
            *outDoc = xl6XapiJsonReadString(json);
            if ( *outDoc == NULL ) {
                xrtFree(key);
                return 0;
            }
        } else if ( !xl6XapiJsonSkipValue(json) ) {
            xrtFree(key);
            return 0;
        }
        xrtFree(key);
        if ( xl6XapiJsonConsume(json, '}') ) {
            return 1;
        }
        if ( !xl6XapiJsonConsume(json, ',') ) {
            return 0;
        }
    }
}

static int xl6XapiParseParamObject(XL6_XapiJson* json, XL6_XapiParam* param)
{
    if ( !xl6XapiJsonConsume(json, '{') ) {
        xl6XapiJsonError(json, "XAPI param 需要对象");
        return 0;
    }
    if ( xl6XapiJsonConsume(json, '}') ) {
        return 1;
    }
    for (;;) {
        char* key = xl6XapiJsonReadString(json);
        if ( key == NULL || !xl6XapiJsonConsume(json, ':') ) {
            xrtFree(key);
            return 0;
        }
        if ( strcmp(key, "name") == 0 ) {
            xrtFree(param->name);
            param->name = xl6XapiJsonReadString(json);
        } else if ( strcmp(key, "type") == 0 ) {
            xrtFree(param->type);
            param->type = xl6XapiJsonReadString(json);
        } else if ( strcmp(key, "optional") == 0 ) {
            if ( !xl6XapiJsonReadBool(json, &param->optional) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( strcmp(key, "vararg") == 0 ) {
            if ( !xl6XapiJsonReadBool(json, &param->vararg) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( !xl6XapiJsonSkipValue(json) ) {
            xrtFree(key);
            return 0;
        }
        xrtFree(key);
        if ( xl6XapiJsonConsume(json, '}') ) {
            return 1;
        }
        if ( !xl6XapiJsonConsume(json, ',') ) {
            xl6XapiJsonError(json, "XAPI param 缺少逗号");
            return 0;
        }
    }
}

static int xl6XapiParseParamsArray(XL6_XapiJson* json, XL6_XapiExport* item)
{
    if ( !xl6XapiJsonConsume(json, '[') ) {
        return 0;
    }
    if ( xl6XapiJsonConsume(json, ']') ) {
        return 1;
    }
    for (;;) {
        XL6_XapiParam param;
        memset(&param, 0, sizeof(param));
        if ( !xl6XapiParseParamObject(json, &param) || !xl6XapiAppendParam(item, &param) ) {
            xrtFree(param.name);
            xrtFree(param.type);
            return 0;
        }
        if ( xl6XapiJsonConsume(json, ']') ) {
            return 1;
        }
        if ( !xl6XapiJsonConsume(json, ',') ) {
            xl6XapiJsonError(json, "XAPI params 缺少逗号");
            return 0;
        }
    }
}

static int xl6XapiParseReturnObject(XL6_XapiJson* json, XL6_XapiExport* item)
{
    char* type;

    type = NULL;
    if ( !xl6XapiJsonConsume(json, '{') ) {
        return 0;
    }
    if ( xl6XapiJsonConsume(json, '}') ) {
        return 1;
    }
    for (;;) {
        char* key = xl6XapiJsonReadString(json);
        if ( key == NULL || !xl6XapiJsonConsume(json, ':') ) {
            xrtFree(key);
            xrtFree(type);
            return 0;
        }
        if ( strcmp(key, "type") == 0 ) {
            xrtFree(type);
            type = xl6XapiJsonReadString(json);
        } else if ( !xl6XapiJsonSkipValue(json) ) {
            xrtFree(key);
            xrtFree(type);
            return 0;
        }
        xrtFree(key);
        if ( xl6XapiJsonConsume(json, '}') ) {
            if ( type != NULL && !xl6XapiAppendReturn(item, type) ) {
                xrtFree(type);
                return 0;
            }
            return 1;
        }
        if ( !xl6XapiJsonConsume(json, ',') ) {
            xrtFree(type);
            return 0;
        }
    }
}

static int xl6XapiParseReturnsArray(XL6_XapiJson* json, XL6_XapiExport* item)
{
    if ( !xl6XapiJsonConsume(json, '[') ) {
        return 0;
    }
    if ( xl6XapiJsonConsume(json, ']') ) {
        return 1;
    }
    for (;;) {
        if ( !xl6XapiParseReturnObject(json, item) ) {
            return 0;
        }
        if ( xl6XapiJsonConsume(json, ']') ) {
            return 1;
        }
        if ( !xl6XapiJsonConsume(json, ',') ) {
            return 0;
        }
    }
}

static int xl6XapiParseExportObject(XL6_XapiJson* json, XL6_XapiExport* item)
{
    if ( !xl6XapiJsonConsume(json, '{') ) {
        return 0;
    }
    if ( xl6XapiJsonConsume(json, '}') ) {
        return 1;
    }
    for (;;) {
        char* key = xl6XapiJsonReadString(json);
        if ( key == NULL || !xl6XapiJsonConsume(json, ':') ) {
            xrtFree(key);
            return 0;
        }
        if ( strcmp(key, "kind") == 0 ) {
            item->kind = xl6XapiJsonReadString(json);
        } else if ( strcmp(key, "name") == 0 ) {
            item->name = xl6XapiJsonReadString(json);
        } else if ( strcmp(key, "cSymbol") == 0 ) {
            item->cSymbol = xl6XapiJsonReadString(json);
        } else if ( strcmp(key, "type") == 0 ) {
            item->type = xl6XapiJsonReadString(json);
        } else if ( strcmp(key, "signature") == 0 ) {
            item->signature = xl6XapiJsonReadString(json);
        } else if ( strcmp(key, "exportId") == 0 ) {
            item->exportId = xl6XapiJsonReadString(json);
        } else if ( strcmp(key, "doc") == 0 ) {
            if ( !xl6XapiParseDocValue(json, &item->doc) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( strcmp(key, "callingConvention") == 0 ) {
            item->callingConvention = xl6XapiJsonReadString(json);
        } else if ( strcmp(key, "varargMode") == 0 ) {
            item->varargMode = xl6XapiJsonReadString(json);
        } else if ( strcmp(key, "requiredParamCount") == 0 ) {
            if ( !xl6XapiJsonReadInt(json, &item->requiredParamCount) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( strcmp(key, "fixedParamCount") == 0 ) {
            if ( !xl6XapiJsonReadInt(json, &item->fixedParamCount) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( strcmp(key, "params") == 0 ) {
            if ( !xl6XapiParseParamsArray(json, item) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( strcmp(key, "returns") == 0 ) {
            if ( !xl6XapiParseReturnsArray(json, item) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( !xl6XapiJsonSkipValue(json) ) {
            xrtFree(key);
            return 0;
        }
        xrtFree(key);
        if ( xl6XapiJsonConsume(json, '}') ) {
            return 1;
        }
        if ( !xl6XapiJsonConsume(json, ',') ) {
            return 0;
        }
    }
}

static int xl6XapiAppendExportSlot(XL6_XapiModule* module, XL6_XapiExport** outItem)
{
    XL6_XapiExport* newItems;

    if ( module == NULL || outItem == NULL ) {
        return 0;
    }
    newItems = (XL6_XapiExport*)xrtRealloc(module->exports, (module->exportCount + 1) * sizeof(XL6_XapiExport));
    if ( newItems == NULL ) {
        return 0;
    }
    module->exports = newItems;
    *outItem = &module->exports[module->exportCount];
    memset(*outItem, 0, sizeof(**outItem));
    module->exportCount += 1;
    return 1;
}

static int xl6XapiParseExportsArray(XL6_XapiJson* json, XL6_XapiModule* module)
{
    if ( !xl6XapiJsonConsume(json, '[') ) {
        return 0;
    }
    if ( xl6XapiJsonConsume(json, ']') ) {
        return 1;
    }
    for (;;) {
        XL6_XapiExport* item;
        if ( !xl6XapiAppendExportSlot(module, &item) || !xl6XapiParseExportObject(json, item) ) {
            return 0;
        }
        if ( xl6XapiJsonConsume(json, ']') ) {
            return 1;
        }
        if ( !xl6XapiJsonConsume(json, ',') ) {
            return 0;
        }
    }
}

static int xl6XapiParseTypeFieldObject(XL6_XapiJson* json, XL6_XapiField* field)
{
    if ( !xl6XapiJsonConsume(json, '{') ) {
        return 0;
    }
    if ( xl6XapiJsonConsume(json, '}') ) {
        return 1;
    }
    for (;;) {
        char* key = xl6XapiJsonReadString(json);
        if ( key == NULL || !xl6XapiJsonConsume(json, ':') ) {
            xrtFree(key);
            return 0;
        }
        if ( strcmp(key, "name") == 0 ) {
            xrtFree(field->name);
            field->name = xl6XapiJsonReadString(json);
        } else if ( strcmp(key, "type") == 0 ) {
            xrtFree(field->type);
            field->type = xl6XapiJsonReadString(json);
        } else if ( strcmp(key, "storage") == 0 ) {
            xrtFree(field->storage);
            field->storage = xl6XapiJsonReadString(json);
        } else if ( strcmp(key, "doc") == 0 ) {
            if ( !xl6XapiParseDocValue(json, &field->doc) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( !xl6XapiJsonSkipValue(json) ) {
            xrtFree(key);
            return 0;
        }
        xrtFree(key);
        if ( xl6XapiJsonConsume(json, '}') ) {
            if ( field->storage == NULL ) {
                field->storage = xl6XapiLoadDupText("field");
                if ( field->storage == NULL ) {
                    return 0;
                }
            }
            return 1;
        }
        if ( !xl6XapiJsonConsume(json, ',') ) {
            return 0;
        }
    }
}

static int xl6XapiParseTypeFieldsArray(XL6_XapiJson* json, XL6_XapiTypeDecl* item)
{
    if ( !xl6XapiJsonConsume(json, '[') ) {
        return 0;
    }
    if ( xl6XapiJsonConsume(json, ']') ) {
        return 1;
    }
    for (;;) {
        XL6_XapiField field;
        memset(&field, 0, sizeof(field));
        if ( !xl6XapiParseTypeFieldObject(json, &field) || !xl6XapiAppendField(item, &field) ) {
            xrtFree(field.name);
            xrtFree(field.type);
            xrtFree(field.doc);
            xrtFree(field.storage);
            return 0;
        }
        if ( xl6XapiJsonConsume(json, ']') ) {
            return 1;
        }
        if ( !xl6XapiJsonConsume(json, ',') ) {
            return 0;
        }
    }
}

static int xl6XapiParseTypeObject(XL6_XapiJson* json, XL6_XapiTypeDecl* item)
{
    if ( !xl6XapiJsonConsume(json, '{') ) {
        return 0;
    }
    if ( xl6XapiJsonConsume(json, '}') ) {
        return 1;
    }
    for (;;) {
        char* key = xl6XapiJsonReadString(json);
        if ( key == NULL || !xl6XapiJsonConsume(json, ':') ) {
            xrtFree(key);
            return 0;
        }
        if ( strcmp(key, "kind") == 0 ) {
            xrtFree(item->kind);
            item->kind = xl6XapiJsonReadString(json);
        } else if ( strcmp(key, "name") == 0 ) {
            xrtFree(item->name);
            item->name = xl6XapiJsonReadString(json);
        } else if ( strcmp(key, "doc") == 0 ) {
            if ( !xl6XapiParseDocValue(json, &item->doc) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( strcmp(key, "base") == 0 ) {
            xrtFree(item->base);
            item->base = xl6XapiJsonReadString(json);
        } else if ( strcmp(key, "typeParams") == 0 ) {
            if ( !xl6XapiParseStringArray(json, &item->typeParams, &item->typeParamCount) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( strcmp(key, "fields") == 0 ) {
            if ( !xl6XapiParseTypeFieldsArray(json, item) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( !xl6XapiJsonSkipValue(json) ) {
            xrtFree(key);
            return 0;
        }
        xrtFree(key);
        if ( xl6XapiJsonConsume(json, '}') ) {
            return 1;
        }
        if ( !xl6XapiJsonConsume(json, ',') ) {
            return 0;
        }
    }
}

static int xl6XapiAppendTypeSlot(XL6_XapiModule* module, XL6_XapiTypeDecl** outItem)
{
    XL6_XapiTypeDecl* newItems;

    if ( module == NULL || outItem == NULL ) {
        return 0;
    }
    newItems = (XL6_XapiTypeDecl*)xrtRealloc(module->types, (module->typeCount + 1) * sizeof(XL6_XapiTypeDecl));
    if ( newItems == NULL ) {
        return 0;
    }
    module->types = newItems;
    *outItem = &module->types[module->typeCount];
    memset(*outItem, 0, sizeof(**outItem));
    module->typeCount += 1;
    return 1;
}

static int xl6XapiParseTypesArray(XL6_XapiJson* json, XL6_XapiModule* module)
{
    if ( !xl6XapiJsonConsume(json, '[') ) {
        return 0;
    }
    if ( xl6XapiJsonConsume(json, ']') ) {
        return 1;
    }
    for (;;) {
        XL6_XapiTypeDecl* item;
        if ( !xl6XapiAppendTypeSlot(module, &item) || !xl6XapiParseTypeObject(json, item) ) {
            return 0;
        }
        if ( xl6XapiJsonConsume(json, ']') ) {
            return 1;
        }
        if ( !xl6XapiJsonConsume(json, ',') ) {
            return 0;
        }
    }
}

static int xl6XapiParsePackageObject(XL6_XapiJson* json, XL6_XapiModule* module)
{
    if ( !xl6XapiJsonConsume(json, '{') ) {
        return 0;
    }
    if ( xl6XapiJsonConsume(json, '}') ) {
        return 1;
    }
    for (;;) {
        char* key = xl6XapiJsonReadString(json);
        if ( key == NULL || !xl6XapiJsonConsume(json, ':') ) {
            xrtFree(key);
            return 0;
        }
        if ( strcmp(key, "name") == 0 ) {
            module->packageName = xl6XapiJsonReadString(json);
        } else if ( strcmp(key, "version") == 0 ) {
            module->packageVersion = xl6XapiJsonReadString(json);
        } else if ( strcmp(key, "abiVersion") == 0 ) {
            module->packageAbiVersion = xl6XapiJsonReadString(json);
        } else if ( strcmp(key, "packageId") == 0 ) {
            module->packageId = xl6XapiJsonReadString(json);
        } else if ( !xl6XapiJsonSkipValue(json) ) {
            xrtFree(key);
            return 0;
        }
        xrtFree(key);
        if ( xl6XapiJsonConsume(json, '}') ) {
            return 1;
        }
        if ( !xl6XapiJsonConsume(json, ',') ) {
            return 0;
        }
    }
}

static int xl6XapiParseModuleObject(XL6_XapiJson* json, XL6_XapiModule* module)
{
    if ( !xl6XapiJsonConsume(json, '{') ) {
        return 0;
    }
    if ( xl6XapiJsonConsume(json, '}') ) {
        return 1;
    }
    for (;;) {
        char* key = xl6XapiJsonReadString(json);
        if ( key == NULL || !xl6XapiJsonConsume(json, ':') ) {
            xrtFree(key);
            return 0;
        }
        if ( strcmp(key, "name") == 0 ) {
            module->name = xl6XapiJsonReadString(json);
        } else if ( strcmp(key, "moduleId") == 0 ) {
            module->moduleId = xl6XapiJsonReadString(json);
        } else if ( strcmp(key, "source") == 0 ) {
            module->source = xl6XapiJsonReadString(json);
        } else if ( strcmp(key, "sourceHash") == 0 ) {
            module->sourceHash = xl6XapiJsonReadString(json);
        } else if ( !xl6XapiJsonSkipValue(json) ) {
            xrtFree(key);
            return 0;
        }
        xrtFree(key);
        if ( xl6XapiJsonConsume(json, '}') ) {
            return 1;
        }
        if ( !xl6XapiJsonConsume(json, ',') ) {
            return 0;
        }
    }
}

static int xl6XapiParseNativeObject(XL6_XapiJson* json, XL6_XapiModule* module)
{
    if ( !xl6XapiJsonConsume(json, '{') ) {
        return 0;
    }
    if ( xl6XapiJsonConsume(json, '}') ) {
        return 1;
    }
    for (;;) {
        char* key = xl6XapiJsonReadString(json);
        if ( key == NULL || !xl6XapiJsonConsume(json, ':') ) {
            xrtFree(key);
            return 0;
        }
        if ( strcmp(key, "singleton") == 0 ) {
            if ( !xl6XapiJsonReadBool(json, &module->nativeSingleton) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( strcmp(key, "loadName") == 0 ) {
            module->nativeLoadName = xl6XapiJsonReadString(json);
        } else if ( strcmp(key, "version") == 0 ) {
            module->nativeVersion = xl6XapiJsonReadString(json);
        } else if ( !xl6XapiJsonSkipValue(json) ) {
            xrtFree(key);
            return 0;
        }
        xrtFree(key);
        if ( xl6XapiJsonConsume(json, '}') ) {
            return 1;
        }
        if ( !xl6XapiJsonConsume(json, ',') ) {
            return 0;
        }
    }
}

static int xl6XapiParseRoot(XL6_XapiJson* json, XL6_XapiModule* module)
{
    int sawFormat;

    sawFormat = 0;
    if ( !xl6XapiJsonConsume(json, '{') ) {
        xl6XapiJsonError(json, "XAPI JSON 顶层必须是对象");
        return 0;
    }
    if ( xl6XapiJsonConsume(json, '}') ) {
        return 1;
    }
    for (;;) {
        char* key = xl6XapiJsonReadString(json);
        if ( key == NULL || !xl6XapiJsonConsume(json, ':') ) {
            xrtFree(key);
            return 0;
        }
        if ( strcmp(key, "format") == 0 ) {
            char* value = xl6XapiJsonReadString(json);
            sawFormat = value != NULL && strcmp(value, "xlang-xapi-1") == 0;
            xrtFree(value);
        } else if ( strcmp(key, "package") == 0 ) {
            if ( !xl6XapiParsePackageObject(json, module) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( strcmp(key, "module") == 0 ) {
            if ( !xl6XapiParseModuleObject(json, module) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( strcmp(key, "exports") == 0 ) {
            if ( !xl6XapiParseExportsArray(json, module) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( strcmp(key, "types") == 0 ) {
            if ( !xl6XapiParseTypesArray(json, module) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( strcmp(key, "links") == 0 ) {
            if ( !xl6XapiParseStringArray(json, &module->links, &module->linkCount) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( strcmp(key, "loadlibs") == 0 ) {
            if ( !xl6XapiParseStringArray(json, &module->loadlibs, &module->loadlibCount) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( strcmp(key, "cSources") == 0 ) {
            if ( !xl6XapiParseStringArray(json, &module->cSources, &module->cSourceCount) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( strcmp(key, "native") == 0 ) {
            if ( !xl6XapiParseNativeObject(json, module) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( !xl6XapiJsonSkipValue(json) ) {
            xrtFree(key);
            return 0;
        }
        xrtFree(key);
        if ( xl6XapiJsonConsume(json, '}') ) {
            if ( !sawFormat ) {
                xl6XapiJsonError(json, "XAPI format 不是 xlang-xapi-1");
                return 0;
            }
            return 1;
        }
        if ( !xl6XapiJsonConsume(json, ',') ) {
            return 0;
        }
    }
}

static char* xl6XapiLoadBuildPackageId(const char* name, const char* version, const char* abiVersion)
{
    char buffer[512];
    int written;

    written = snprintf(buffer, sizeof(buffer), "%s@%s#abi%s",
                       name != NULL && name[0] != '\0' ? name : "app",
                       version != NULL && version[0] != '\0' ? version : "0.0.0",
                       abiVersion != NULL && abiVersion[0] != '\0' ? abiVersion : "1");
    if ( written <= 0 || (size_t)written >= sizeof(buffer) ) {
        return NULL;
    }
    return xl6XapiLoadDupText(buffer);
}

static char* xl6XapiLoadBuildModuleId(const char* packageId, const char* moduleName)
{
    char buffer[768];
    int written;

    written = snprintf(buffer, sizeof(buffer), "%s::%s",
                       packageId != NULL && packageId[0] != '\0' ? packageId : "app@0.0.0#abi1",
                       moduleName != NULL && moduleName[0] != '\0' ? moduleName : "app");
    if ( written <= 0 || (size_t)written >= sizeof(buffer) ) {
        return NULL;
    }
    return xl6XapiLoadDupText(buffer);
}

static int xl6XapiLoadAppendText(char** out, size_t* len, size_t* capacity, const char* text)
{
    char* newData;
    size_t add;
    size_t required;
    size_t newCapacity;

    if ( out == NULL || len == NULL || capacity == NULL || text == NULL ) {
        return 0;
    }
    add = strlen(text);
    required = *len + add + 1;
    if ( required > *capacity ) {
        newCapacity = *capacity == 0 ? 64 : *capacity;
        while ( newCapacity < required ) {
            newCapacity *= 2;
        }
        newData = (char*)xrtRealloc(*out, newCapacity);
        if ( newData == NULL ) {
            return 0;
        }
        *out = newData;
        *capacity = newCapacity;
    }
    if ( add > 0 ) {
        memcpy(*out + *len, text, add);
    }
    *len += add;
    (*out)[*len] = '\0';
    return 1;
}

static char* xl6XapiLoadBuildSignature(const XL6_XapiExport* item)
{
    char* out;
    size_t len;
    size_t capacity;
    size_t i;

    out = NULL;
    len = 0;
    capacity = 0;
    if ( item == NULL || item->kind == NULL ) {
        return xl6XapiLoadDupText("");
    }
    if ( strcmp(item->kind, "function") == 0 ) {
        if ( !xl6XapiLoadAppendText(&out, &len, &capacity, "func(") ) {
            xrtFree(out);
            return NULL;
        }
        for ( i = 0; i < item->paramCount; ++i ) {
            if ( i > 0 && !xl6XapiLoadAppendText(&out, &len, &capacity, ",") ) {
                xrtFree(out);
                return NULL;
            }
            if ( !xl6XapiLoadAppendText(&out, &len, &capacity, item->params[i].type != NULL ? item->params[i].type : "<missing-type>") ) {
                xrtFree(out);
                return NULL;
            }
        }
        if ( !xl6XapiLoadAppendText(&out, &len, &capacity, ")") ) {
            xrtFree(out);
            return NULL;
        }
        if ( item->returnCount > 0 ) {
            if ( !xl6XapiLoadAppendText(&out, &len, &capacity, "->") ) {
                xrtFree(out);
                return NULL;
            }
            if ( item->returnCount > 1 && !xl6XapiLoadAppendText(&out, &len, &capacity, "(") ) {
                xrtFree(out);
                return NULL;
            }
            for ( i = 0; i < item->returnCount; ++i ) {
                if ( i > 0 && !xl6XapiLoadAppendText(&out, &len, &capacity, ",") ) {
                    xrtFree(out);
                    return NULL;
                }
                if ( !xl6XapiLoadAppendText(&out, &len, &capacity, item->returns[i] != NULL ? item->returns[i] : "<missing-type>") ) {
                    xrtFree(out);
                    return NULL;
                }
            }
            if ( item->returnCount > 1 && !xl6XapiLoadAppendText(&out, &len, &capacity, ")") ) {
                xrtFree(out);
                return NULL;
            }
        }
    } else {
        if ( !xl6XapiLoadAppendText(&out, &len, &capacity, item->kind) ||
             !xl6XapiLoadAppendText(&out, &len, &capacity, ":") ||
             !xl6XapiLoadAppendText(&out, &len, &capacity, item->type != NULL ? item->type : "<missing-type>") ) {
            xrtFree(out);
            return NULL;
        }
    }
    return out != NULL ? out : xl6XapiLoadDupText("");
}

static char* xl6XapiLoadBuildExportId(const XL6_XapiModule* module, const XL6_XapiExport* item)
{
    char* out;
    size_t len;
    size_t capacity;

    out = NULL;
    len = 0;
    capacity = 0;
    if ( !xl6XapiLoadAppendText(&out, &len, &capacity, module != NULL && module->moduleId != NULL ? module->moduleId : "app@0.0.0#abi1::app") ||
         !xl6XapiLoadAppendText(&out, &len, &capacity, "::") ||
         !xl6XapiLoadAppendText(&out, &len, &capacity, item != NULL && item->name != NULL ? item->name : "") ||
         !xl6XapiLoadAppendText(&out, &len, &capacity, "(") ||
         !xl6XapiLoadAppendText(&out, &len, &capacity, item != NULL && item->signature != NULL ? item->signature : "") ||
         !xl6XapiLoadAppendText(&out, &len, &capacity, ")") ) {
        xrtFree(out);
        return NULL;
    }
    return out;
}

static int xl6XapiFinalizeLoadedModule(XL6_XapiModule* module)
{
    size_t i;

    if ( module == NULL ) {
        return 0;
    }
    if ( module->name == NULL ) {
        module->name = xl6XapiLoadDupText("app");
    }
    if ( module->source == NULL ) {
        module->source = xl6XapiLoadDupText("");
    }
    if ( module->sourceHash == NULL ) {
        module->sourceHash = xl6XapiLoadDupText("");
    }
    if ( module->packageName == NULL ) {
        module->packageName = xl6XapiLoadDupText(module->name != NULL ? module->name : "app");
    }
    if ( module->packageVersion == NULL ) {
        module->packageVersion = xl6XapiLoadDupText("0.0.0");
    }
    if ( module->packageAbiVersion == NULL ) {
        module->packageAbiVersion = xl6XapiLoadDupText("1");
    }
    if ( module->packageId == NULL ) {
        module->packageId = xl6XapiLoadBuildPackageId(module->packageName, module->packageVersion, module->packageAbiVersion);
    }
    if ( module->moduleId == NULL ) {
        module->moduleId = xl6XapiLoadBuildModuleId(module->packageId, module->name);
    }
    if ( module->nativeLoadName == NULL ) {
        module->nativeLoadName = xl6XapiLoadDupText("");
    }
    if ( module->nativeVersion == NULL ) {
        module->nativeVersion = xl6XapiLoadDupText("");
    }
    if ( module->name == NULL || module->source == NULL || module->sourceHash == NULL ||
         module->packageName == NULL || module->packageVersion == NULL ||
         module->packageAbiVersion == NULL || module->packageId == NULL ||
         module->moduleId == NULL || module->nativeLoadName == NULL ||
         module->nativeVersion == NULL ) {
        return 0;
    }

    for ( i = 0; i < module->exportCount; ++i ) {
        XL6_XapiExport* item = &module->exports[i];
        if ( item->kind != NULL && strcmp(item->kind, "function") == 0 ) {
            if ( item->callingConvention == NULL ) {
                item->callingConvention = xl6XapiLoadDupText("cdecl");
            }
            if ( item->varargMode == NULL ) {
                item->varargMode = xl6XapiLoadDupText("none");
            }
        }
        if ( item->signature == NULL ) {
            item->signature = xl6XapiLoadBuildSignature(item);
        }
        if ( item->exportId == NULL ) {
            item->exportId = xl6XapiLoadBuildExportId(module, item);
        }
        if ( item->signature == NULL || item->exportId == NULL ||
             (item->kind != NULL && strcmp(item->kind, "function") == 0 &&
              (item->callingConvention == NULL || item->varargMode == NULL)) ) {
            return 0;
        }
    }
    return 1;
}

int xl6XapiLoadJsonFile(XL6_XapiModule* module, const char* path, XL6_DiagList* diags)
{
    XL6_XapiJson json;
    char* text;
    size_t len;
    int ok;

    if ( module == NULL || path == NULL ) {
        return 0;
    }
    xrtClearError();
    text = xrtFileReadAll((char*)path, XRT_CP_UTF8, &len);
    if ( xrtGetError()[0] != '\0' || text == NULL ) {
        xrtFree(text);
        if ( diags != NULL ) {
            xl6DiagAdd(diags, 1, 1, "无法读取 XAPI 文件");
        }
        return 0;
    }

    xl6XapiModuleFree(module);
    xl6XapiModuleInit(module);
    memset(&json, 0, sizeof(json));
    json.path = path;
    json.text = text;
    json.len = len;
    json.diags = diags;

    ok = xl6XapiParseRoot(&json, module);
    if ( ok ) {
        xl6XapiJsonSkipWs(&json);
        if ( json.pos != json.len ) {
            xl6XapiJsonError(&json, "XAPI JSON 末尾存在多余字符");
            ok = 0;
        }
    }
    if ( ok && !xl6XapiFinalizeLoadedModule(module) ) {
        xl6XapiJsonError(&json, "XAPI metadata finalize failed");
        ok = 0;
    }
    xrtFree(text);
    if ( !ok ) {
        xl6XapiModuleFree(module);
    }
    return ok;
}
