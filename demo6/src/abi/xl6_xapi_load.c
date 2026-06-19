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
    xrtFree(text);
    if ( !ok ) {
        xl6XapiModuleFree(module);
    }
    return ok;
}
