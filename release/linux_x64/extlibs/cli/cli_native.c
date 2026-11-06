/* 命令行解析：用户参数错误写入结果，声明错误通过 XRT 错误返回。 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "include/cli_native.h"
#include <xl_value_read.h>

#define XCLI_DOMAIN "xlang.extlib.cli"

static void xcli_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XCLI_DOMAIN, 0, message);
}

/* One borrowed read scope protects each input container for the whole parse.
 * No temporary xvalue or copied string is created while inspecting input. */
static bool xcli_present(XL_ValueBorrowV1 value) { return value.ref || value.kind != XVALUE_NULL; }
static bool xcli_read_text(XL_ValueBorrowV1 value, xstrview* out) {
    if (value.kind != XVALUE_STRING) return false;
    *out = value.data.text; return true;
}
static bool xcli_read_has(const XL_ValueReadV1* view, xstrview key) {
    return xcli_present(xlValueReadFindV1(view, key));
}
static bool xcli_optional(const XL_ValueReadV1* spec, const char* name, xvaluetype kind,
    XL_ValueReadV1* scopes, XL_ValueReadV1* scope, XL_ValueReadV1** output) {
    XL_ValueBorrowV1 value = xlValueReadFindV1(spec, (xstrview){name, strlen(name)});
    *output = NULL;
    if (!xcli_present(value)) return !xrtGetError();
    for (size_t i = 0; i < 7; ++i)
        if (scopes[i].self && scopes[i].source == value.ref) {
            *output = &scopes[i]; return scopes[i].kind == kind;
        }
    if (!xlValueReadBeginV1(value, scope)) return false;
    if (scope->kind != kind) return false;
    *output = scope; return true;
}

/* ---------- 值对象辅助 ---------- */

static xvalue *xcli_text(const char *s) {
    const char *text = s != NULL ? s : "";
    return xrtValueString((xstrview){ text, strlen(text) });
}

static xvalue *xcli_get(xvalue *obj, const char *key) {
    xstrview view = { key, strlen(key) };
    return obj != NULL ? xrtValueObjectGet(obj, view) : NULL;
}

static int xcli_set_view(xvalue *obj, xstrview key, xvalue *value) {
    return xrtValueObjectSetNew(obj, key, value);
}

static int xcli_set(xvalue *obj, const char *key, xvalue *value) {
    xstrview view = { key, strlen(key) };
    return xcli_set_view(obj, view, value);
}

static int xcli_set_str(xvalue *obj, const char *key, const char *text) {
    return xcli_set(obj, key, xcli_text(text));
}

static int xcli_set_bool(xvalue *obj, const char *key, int b) {
    return xcli_set(obj, key, xrtValueBool(b ? true : false));
}

static int xcli_set_str_view(xvalue *obj, xstrview key, xstrview text) {
    return xcli_set_view(obj, key, xrtValueString(text));
}

static int xcli_set_bool_view(xvalue *obj, xstrview key, int b) {
    return xcli_set_view(obj, key, xrtValueBool(b ? true : false));
}

/* 输入数组在解析期间存活；只借用视图，不为每个参数分配副本。 */
static bool xcli_arg_view(XL_ValueReadV1 *args, int64 index, xstrview *out) {
    XL_ValueBorrowV1 value = xlValueReadAtV1(args, index, NULL);
    return xcli_present(value) && xcli_read_text(value, out) &&
        (out->Size == 0 || memchr(out->Data, '\0', out->Size) == NULL);
}

/* 查找声明的旗标。 */
static int xcli_flags_has_view(XL_ValueReadV1 *flags, xstrview name) {
    int64 i;
    int64 n;
    if (flags == NULL) {
        return 0;
    }
    n = flags->count;
    for (i = 0; i < n; ++i) {
        XL_ValueBorrowV1 value = xlValueReadAtV1(flags, i, NULL);
        xstrview text;
        if (xcli_present(value) && xcli_read_text(value, &text) &&
            text.Size == name.Size &&
            memcmp(text.Data, name.Data, text.Size) == 0) {
            return 1;
        }
    }
    return 0;
}

/* 短名映射返回借用的长名称视图，声明对象在解析期间保持存活。 */
static int xcli_short_in(XL_ValueReadV1 *mapping, char shortName,
    xstrview *out) {
    int64 i;
    int64 n;
    if (mapping == NULL) {
        return 0;
    }
    n = mapping->count;
    for (i = 0; i < n; ++i) {
        xstrview key;
        XL_ValueBorrowV1 value = xlValueReadAtV1(mapping, i, &key);
        xstrview shorts;
        if (!xcli_present(value) || !xcli_read_text(value, &shorts)) {
            continue;
        }
        {
            size_t j;
            for (j = 0; j < shorts.Size; ++j) {
                if (shorts.Data[j] == shortName) {
                    *out = key;
                    return 1;
                }
            }
        }
    }
    return 0;
}

/* 名称必须是 UTF-8；Unicode 空白和控制字符会令命令行难以辨认。 */
static bool xcli_name_forbidden(uint32 scalar) {
    return scalar <= 0x20u || (scalar >= 0x7fu && scalar <= 0x9fu) ||
        scalar == '=' || scalar == 0xa0u || scalar == 0x1680u ||
        (scalar >= 0x2000u && scalar <= 0x200fu) ||
        (scalar >= 0x2028u && scalar <= 0x202eu) ||
        scalar == 0x202fu || scalar == 0x205fu ||
        (scalar >= 0x2060u && scalar <= 0x206fu) ||
        scalar == 0x3000u || scalar == 0xfeffu;
}

static bool xcli_name_valid(xstrview name) {
    size_t i = 0;
    if (name.Size == 0 || name.Data[0] == '-') return false;
    while (i < name.Size) {
        uint32 scalar;
        size_t read;
        if (xrtUtf8Decode((xstrview){ name.Data + i, name.Size - i },
            &scalar, &read) != XUTF_OK || read == 0 ||
            xcli_name_forbidden(scalar)) return false;
        i += read;
    }
    return true;
}

static bool xcli_validate_names(XL_ValueReadV1 *array) {
    size_t i;
    if (array == NULL) return true;
    for (i = 0; i < array->count; ++i) {
        xstrview name;
        size_t j;
        if (!xcli_read_text(xlValueReadAtV1(array, (int64)i, NULL), &name) ||
            !xcli_name_valid(name)) return false;
        for (j = 0; j < i; ++j) {
            xstrview prior;
            if (!xcli_read_text(xlValueReadAtV1(array, (int64)j, NULL), &prior) ||
                (prior.Size == name.Size &&
                 memcmp(prior.Data, name.Data, name.Size) == 0)) return false;
        }
    }
    return true;
}

static bool xcli_validate_mapping(XL_ValueReadV1 *mapping, bool bAliases) {
    size_t i;
    if (mapping == NULL) return true;
    for (i = 0; i < mapping->count; ++i) {
        xstrview name;
        XL_ValueBorrowV1 value = xlValueReadAtV1(mapping, (int64)i, &name);
        if (!xcli_present(value) || !xcli_name_valid(name)) return false;
        if (bAliases) {
            xstrview aliases;
            if (!xcli_read_text(value, &aliases) ||
                (aliases.Size != 0 &&
                memchr(aliases.Data, '\0', aliases.Size) != NULL)) return false;
        }
    }
    return true;
}

static bool xcli_validate_defaults(XL_ValueReadV1 *options, XL_ValueReadV1 *defaults) {
    size_t i;
    if (defaults == NULL) return true;
    for (i = 0; i < defaults->count; ++i) {
        xstrview name;
        xstrview text;
        XL_ValueBorrowV1 value = xlValueReadAtV1(defaults, (int64)i, &name);
        if (options == NULL || !xcli_read_has(options, name) ||
            !xcli_present(value) || !xcli_read_text(value, &text) ||
            (text.Size > 0 && memchr(text.Data, '\0', text.Size) != NULL))
            return false;
    }
    return true;
}

static bool xcli_validate_spec_fields(XL_ValueReadV1 *spec) {
    static const char *fields[] = {
        "flags", "options", "shorts", "defaults", "positional"
    };
    size_t i, j;
    for (i = 0; i < spec->count; ++i) {
        xstrview name;
        bool known = false;
        xlValueReadAtV1(spec, (int64)i, &name);
        for (j = 0; j < sizeof(fields) / sizeof(fields[0]); ++j) {
            size_t size = strlen(fields[j]);
            if (name.Size == size && memcmp(name.Data, fields[j], size) == 0) {
                known = true;
                break;
            }
        }
        if (!known) return false;
    }
    return true;
}

/* 短名必须唯一，否则解析顺序会决定命中哪个选项。 */
static bool xcli_validate_short_aliases(XL_ValueReadV1 *flags, XL_ValueReadV1 *options,
    XL_ValueReadV1 *shorts) {
    unsigned char used[128] = { 0 };
    XL_ValueReadV1 *mappings[2] = { options, shorts };
    size_t group;
    size_t i;
    if (flags != NULL) {
        for (i = 0; i < flags->count; ++i) {
            xstrview name;
            if (xcli_read_text(xlValueReadAtV1(flags, (int64)i, NULL), &name) &&
                name.Size == 1u && (unsigned char)name.Data[0] < 128u) {
                used[(unsigned char)name.Data[0]] = 1u;
            }
            if (options != NULL &&
                xcli_read_has(options, name)) return false;
        }
    }
    for (group = 0; group < 2u; ++group) {
        XL_ValueReadV1 *mapping = mappings[group];
        if (mapping == NULL) continue;
        for (i = 0; i < mapping->count; ++i) {
            xstrview name;
            xstrview aliases;
            XL_ValueBorrowV1 value = xlValueReadAtV1(mapping, (int64)i, &name);
            size_t j;
            if (!xcli_read_text(value, &aliases)) return false;
            if (group == 1u && !xcli_flags_has_view(flags, name)) return false;
            for (j = 0; j < aliases.Size; ++j) {
                unsigned char c = (unsigned char)aliases.Data[j];
                if (c <= 0x20u || c >= 0x7fu || c == '-' || c == '=' ||
                    used[c]) return false;
                used[c] = 1u;
            }
        }
    }
    return true;
}

static bool xcli_fail_result(xvalue *result, const char *message) {
    return xcli_set_bool(result, "ok", 0) &&
        xcli_set_str(result, "error", message);
}

static bool xcli_fail_long_result(xvalue *result, const char *prefix,
    xstrview name, const char *suffix) {
    size_t prefixLen = strlen(prefix);
    size_t nameLen = name.Size;
    size_t suffixLen = strlen(suffix);
    size_t length;
    str message;
    xvalue *text;
    if (prefixLen > SIZE_MAX - suffixLen - 1u ||
        nameLen > SIZE_MAX - prefixLen - suffixLen - 1u) {
        xrtSetErrorInfo(XERR_RANGE, XCLI_DOMAIN, 0,
            "option name is too long");
        return false;
    }
    length = prefixLen + nameLen + suffixLen;
    message = (str)xrtMalloc(length + 1u);
    if (message == NULL) return false;
    memcpy(message, prefix, prefixLen);
    if (nameLen != 0u) memcpy(message + prefixLen, name.Data, nameLen);
    memcpy(message + prefixLen + nameLen, suffix, suffixLen);
    message[length] = '\0';
    if (!xcli_set_bool(result, "ok", 0)) {
        xrtFree(message);
        return false;
    }
    text = xrtValueStringTake(&message, length);
    xrtFree(message);
    return text != NULL && xcli_set(result, "error", text);
}

xvalue *xcli_parse(xvalue *argsValue, xvalue *specValue) {
    XL_ValueReadV1 scopes[7] = {0};
    XL_ValueReadV1 *args = &scopes[0], *spec = &scopes[1];
    XL_ValueReadV1 *flagsSpec = NULL;
    XL_ValueReadV1 *optionsSpec = NULL;
    XL_ValueReadV1 *shortsSpec = NULL;
    XL_ValueReadV1 *defaultsSpec = NULL;
    XL_ValueReadV1 *positionalSpec = NULL;
    xvalue *result = NULL;
    xvalue *flagsOut = NULL;
    xvalue *optionsOut = NULL;
    xvalue *positionalOut = NULL;
    xvalue *restOut = NULL;
    int64 i;
    int64 n;
    int noMoreOptions;
    int64 positionalIndex;
    int64 positionalCount;
    xstrview longName;

    xrtClearError();
    if (!xlValueReadBeginV1(xlValueBorrowV1(argsValue), args) ||
        !xlValueReadBeginV1(xlValueBorrowV1(specValue), spec) ||
        args->kind != XVALUE_ARRAY || spec->kind != XVALUE_OBJECT) {
        xcli_error("parse requires an args array and a spec object"); goto fail;
    }
    if (!xcli_validate_spec_fields(spec)) {
        xcli_error("spec contains an unknown field"); goto fail;
    }
    if (!xcli_optional(spec, "flags", XVALUE_ARRAY, scopes, &scopes[2], &flagsSpec)) {
        xcli_error("spec flags must be an array"); goto fail;
    }
    if (!xcli_optional(spec, "options", XVALUE_OBJECT, scopes, &scopes[3], &optionsSpec)) {
        xcli_error("spec options must be an object"); goto fail;
    }
    if (!xcli_optional(spec, "shorts", XVALUE_OBJECT, scopes, &scopes[4], &shortsSpec)) {
        xcli_error("spec shorts must be an object"); goto fail;
    }
    if (!xcli_optional(spec, "defaults", XVALUE_OBJECT, scopes, &scopes[5], &defaultsSpec)) {
        xcli_error("spec defaults must be an object"); goto fail;
    }
    if (!xcli_optional(spec, "positional", XVALUE_ARRAY, scopes, &scopes[6], &positionalSpec)) {
        xcli_error("spec positional must be an array"); goto fail;
    }
    if (!xcli_validate_names(flagsSpec) ||
        !xcli_validate_names(positionalSpec) ||
        !xcli_validate_mapping(optionsSpec, true) ||
        !xcli_validate_mapping(shortsSpec, true) ||
        !xcli_validate_mapping(defaultsSpec, false) ||
        !xcli_validate_short_aliases(flagsSpec, optionsSpec, shortsSpec)) {
        xcli_error("spec contains an invalid name or short alias");
        goto fail;
    }
    if (!xcli_validate_defaults(optionsSpec, defaultsSpec)) {
        xcli_error("spec defaults must be NUL-free strings for declared options");
        goto fail;
    }

    result = xrtValueObject();
    if (result == NULL) goto fail;
    flagsOut = xrtValueObject();
    optionsOut = xrtValueObject();
    positionalOut = xrtValueObject();
    restOut = xrtValueArray();
    if (flagsOut == NULL || optionsOut == NULL || positionalOut == NULL ||
        restOut == NULL) {
        goto fail;
    }

    /* 默认值先进入选项结果。 */
    if (defaultsSpec != NULL) {
        int64 di;
        int64 dn = defaultsSpec->count;
        for (di = 0; di < dn; ++di) {
            xstrview key;
            XL_ValueBorrowV1 value = xlValueReadAtV1(defaultsSpec, di, &key);
            if (xcli_present(value)) {
                /* 默认值已校验为不可变字符串，只需标量复制，不进入图遍历。 */
                xvalue *copy = xlValueBorrowCopyV1(value);
                if (!xcli_set_view(optionsOut, key, copy)) goto fail;
            }
        }
    }
    /* 未指定默认值的选项以空字符串初始化。 */
    if (optionsSpec != NULL) {
        int64 oi;
        int64 on = optionsSpec->count;
        for (oi = 0; oi < on; ++oi) {
            xstrview key;
            XL_ValueBorrowV1 value = xlValueReadAtV1(optionsSpec, oi, &key);
            if (xcli_present(value)) {
                if (xrtValueObjectGet(optionsOut, key) == NULL) {
                    if (!xcli_set_str_view(optionsOut, key,
                        (xstrview){ "", 0 })) goto fail;
                }
            }
        }
    }
    /* 所有声明的旗标先置为 false。 */
    if (flagsSpec != NULL) {
        int64 fi;
        int64 fn = flagsSpec->count;
        for (fi = 0; fi < fn; ++fi) {
            XL_ValueBorrowV1 value = xlValueReadAtV1(flagsSpec, fi, NULL);
            xstrview text;
            if (xcli_present(value) && xcli_read_text(value, &text)) {
                if (!xcli_set_bool_view(flagsOut, text, 0)) goto fail;
            }
        }
    }

    noMoreOptions = 0;
    positionalIndex = 0;
    positionalCount = positionalSpec != NULL
        ? positionalSpec->count : 0;
    n = args->count;

    /* flagsOut已包含全部声明；false值也非NULL，可直接查键而不反复扫数组。 */
    for (i = 0; i < n; ++i) {
        xstrview token;
        if (!xcli_arg_view(args, i, &token)) {
            xcli_error("args entries must be strings without NUL");
            goto fail;
        }
        if (noMoreOptions || token.Size == 0 || token.Data[0] != '-' ||
            token.Size == 1) {
            /* 位置参数。 */
            if (positionalIndex < positionalCount) {
                xstrview pname;
                if (!xcli_arg_view(positionalSpec, positionalIndex, &pname)) {
                    xcli_error("positional names must be strings");
                    goto fail;
                }
                if (!xcli_set_str_view(positionalOut, pname, token)) goto fail;
                positionalIndex++;
            } else {
                if (!xrtValueArrayAppendNew(restOut, xrtValueString(token)))
                    goto fail;
            }
            continue;
        }
        if (token.Size == 2 && token.Data[1] == '-') {
            noMoreOptions = 1;
            continue;
        }
        if (token.Data[1] == '-') {
            /* 长选项与等号内联值。 */
            xstrview name = { token.Data + 2, token.Size - 2 };
            xstrview inlineValue;
            const char *eq = memchr(name.Data, '=', name.Size);
            int hasInline = eq != NULL;
            if (eq != NULL) {
                inlineValue = (xstrview){ eq + 1,
                    name.Size - (size_t)(eq - name.Data) - 1u };
                name.Size = (size_t)(eq - name.Data);
            }
            if (optionsSpec != NULL && xcli_read_has(optionsSpec, name)) {
                if (hasInline) {
                    if (!xcli_set_str_view(optionsOut, name, inlineValue)) goto fail;
                } else if (i + 1 < n) {
                    xstrview next;
                    if (!xcli_arg_view(args, ++i, &next)) {
                        xcli_error("option value must be a string");
                        goto fail;
                    }
                    if (!xcli_set_str_view(optionsOut, name, next)) goto fail;
                } else {
                    if (!xcli_fail_long_result(result,
                        "missing value for --", name, "")) goto fail;
                    break;
                }
            } else if (xrtValueObjectGet(flagsOut, name) != NULL) {
                if (hasInline) {
                    if (!xcli_fail_long_result(result, "flag --", name,
                        " does not take a value")) goto fail;
                    break;
                }
                if (!xcli_set_bool_view(flagsOut, name, 1)) goto fail;
            } else {
                if (!xcli_fail_long_result(result,
                    "unknown option --", name, "")) goto fail;
                break;
            }
            continue;
        }
        /* 短旗标组合与短选项附加值。 */
        {
            size_t pos = 1u;
            int stop = 0;
            while (!stop && pos < token.Size) {
                char letter = token.Data[pos];
                if ((unsigned char)letter < 0x21 ||
                    (unsigned char)letter > 0x7e) {
                    if (!xcli_fail_result(result,
                        "short option must be printable ASCII")) goto fail;
                    stop = 2;
                } else if (xcli_short_in(optionsSpec, letter, &longName)) {
                    if (pos + 1u < token.Size) {
                        xstrview attached = { token.Data + pos + 1u,
                            token.Size - pos - 1u };
                        if (!xcli_set_str_view(optionsOut, longName, attached))
                            goto fail;
                        stop = 1;
                    } else if (i + 1 < n) {
                        xstrview next;
                        if (!xcli_arg_view(args, ++i, &next)) {
                            xcli_error("option value must be a string");
                            goto fail;
                        }
                        if (!xcli_set_str_view(optionsOut, longName, next))
                            goto fail;
                        stop = 1;
                    } else {
                        char message[80];
                        snprintf(message, sizeof(message),
                            "missing value for -%c", letter);
                        if (!xcli_fail_result(result, message)) goto fail;
                        stop = 2;
                    }
                } else if (xcli_short_in(shortsSpec, letter, &longName)) {
                    if (!xcli_set_bool_view(flagsOut, longName, 1)) goto fail;
                } else {
                    xstrview flagName = { &letter, 1u };
                    if (xrtValueObjectGet(flagsOut, flagName) != NULL) {
                        if (!xcli_set_bool_view(flagsOut, flagName, 1)) goto fail;
                    } else {
                        char message[80];
                        snprintf(message, sizeof(message),
                            "unknown short option -%c", letter);
                        if (!xcli_fail_result(result, message)) goto fail;
                        stop = 2;
                    }
                }
                pos++;
            }
            if (stop == 2) break;
            continue;
        }
    }

    if (!xcli_set(result, "flags", flagsOut)) {
        flagsOut = NULL;
        goto fail;
    }
    flagsOut = NULL;
    if (!xcli_set(result, "options", optionsOut)) {
        optionsOut = NULL;
        goto fail;
    }
    optionsOut = NULL;
    if (!xcli_set(result, "positional", positionalOut)) {
        positionalOut = NULL;
        goto fail;
    }
    positionalOut = NULL;
    if (!xcli_set(result, "rest", restOut)) {
        restOut = NULL;
        goto fail;
    }
    restOut = NULL;
    if (xcli_get(result, "ok") == NULL) {
        if (!xcli_set_bool(result, "ok", 1) ||
            !xcli_set_str(result, "error", "")) goto fail;
    }
    goto done;

fail:
    xrtValueRelease(restOut);
    xrtValueRelease(positionalOut);
    xrtValueRelease(optionsOut);
    xrtValueRelease(flagsOut);
    xrtValueRelease(result); result = NULL;
done:
    /* Scope close does not allocate or call source code. Preserve the first
     * parse failure while unwinding every successfully opened scope. */
    for (size_t scopeIndex = 7; scopeIndex > 0; --scopeIndex)
        if (!xlValueReadEndV1(&scopes[scopeIndex - 1])) {
            xrtValueRelease(result); result = NULL;
        }
    return result;
}
