/* 命令行解析：用户参数错误写入结果，声明错误通过 XRT 错误返回。 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "include/cli_native.h"

#define XCLI_DOMAIN "xlang.extlib.cli"

static void xcli_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XCLI_DOMAIN, 0, message);
}

/* ---------- 值对象辅助 ---------- */

static xvalue *xcli_text(const char *s) {
    size_t n = s ? strlen(s) : 0;
    str owned = (str)xrtMalloc(n + 1);
    xvalue *value;
    if (owned == NULL) {
        return NULL;
    }
    if (n > 0) {
        memcpy(owned, s, n);
    }
    owned[n] = '\0';
    value = xrtValueStringTake(&owned, n);
    if (value == NULL && owned != NULL) {
        xrtFree(owned);
    }
    return value;
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

static int xcli_set_str_view(xvalue *obj, xstrview key, const char *text) {
    return xcli_set_view(obj, key, xcli_text(text));
}

static int xcli_set_bool_view(xvalue *obj, xstrview key, int b) {
    return xcli_set_view(obj, key, xrtValueBool(b ? true : false));
}

/* 参数必须是无内嵌 NUL 的字符串，再复制为 C 字符串。 */
static char *xcli_arg(xvalue *args, int64 index) {
    xvalue *value = xrtValueArrayAt(args, index);
    xstrview text;
    if (value == NULL || !xrtValueGetString(value, &text) ||
        memchr(text.Data, '\0', text.Size) != NULL) {
        return NULL;
    }
    {
        char *copy = (char *)xrtMalloc(text.Size + 1);
        if (copy == NULL) {
            return NULL;
        }
        memcpy(copy, text.Data, text.Size);
        copy[text.Size] = '\0';
        return copy;
    }
}

/* 查找声明的旗标。 */
static int xcli_flags_has_view(xvalue *flags, xstrview name) {
    int64 i;
    int64 n;
    if (flags == NULL) {
        return 0;
    }
    n = xrtValueCount(flags);
    for (i = 0; i < n; ++i) {
        xvalue *value = xrtValueArrayAt(flags, i);
        xstrview text;
        if (value != NULL && xrtValueGetString(value, &text) &&
            text.Size == name.Size &&
            memcmp(text.Data, name.Data, text.Size) == 0) {
            return 1;
        }
    }
    return 0;
}

static int xcli_flags_has(xvalue *flags, const char *name) {
    return xcli_flags_has_view(flags, (xstrview){ name, strlen(name) });
}

/* 短名映射返回借用的长名称视图，声明对象在解析期间保持存活。 */
static int xcli_short_in(xvalue *mapping, char shortName,
    xstrview *out) {
    int64 i;
    int64 n;
    if (mapping == NULL) {
        return 0;
    }
    n = xrtValueCount(mapping);
    for (i = 0; i < n; ++i) {
        xstrview key;
        xvalue *value = xrtValueObjectAt(mapping, i, &key);
        xstrview shorts;
        if (value == NULL || !xrtValueGetString(value, &shorts)) {
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

static int xcli_is_flag(xvalue *flags, const char *name) {
    return xcli_flags_has(flags, name);
}

static int xcli_is_option(xvalue *options, const char *name) {
    return xcli_get(options, name) != NULL;
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

static bool xcli_validate_names(xvalue *array) {
    size_t i;
    if (array == NULL) return true;
    for (i = 0; i < xrtValueCount(array); ++i) {
        xstrview name;
        size_t j;
        if (!xrtValueGetString(xrtValueArrayAt(array, (int64)i), &name) ||
            !xcli_name_valid(name)) return false;
        for (j = 0; j < i; ++j) {
            xstrview prior;
            if (!xrtValueGetString(xrtValueArrayAt(array, (int64)j), &prior) ||
                (prior.Size == name.Size &&
                 memcmp(prior.Data, name.Data, name.Size) == 0)) return false;
        }
    }
    return true;
}

static bool xcli_validate_mapping(xvalue *mapping, bool bAliases) {
    size_t i;
    if (mapping == NULL) return true;
    for (i = 0; i < xrtValueCount(mapping); ++i) {
        xstrview name;
        xvalue *value = xrtValueObjectAt(mapping, (int64)i, &name);
        if (value == NULL || !xcli_name_valid(name)) return false;
        if (bAliases) {
            xstrview aliases;
            if (!xrtValueGetString(value, &aliases) ||
                (aliases.Size != 0 &&
                memchr(aliases.Data, '\0', aliases.Size) != NULL)) return false;
        }
    }
    return true;
}

static bool xcli_validate_defaults(xvalue *options, xvalue *defaults) {
    size_t i;
    if (defaults == NULL) return true;
    for (i = 0; i < xrtValueCount(defaults); ++i) {
        xstrview name;
        xstrview text;
        xvalue *value = xrtValueObjectAt(defaults, (int64)i, &name);
        if (options == NULL || xrtValueObjectGet(options, name) == NULL ||
            value == NULL || !xrtValueGetString(value, &text) ||
            (text.Size > 0 && memchr(text.Data, '\0', text.Size) != NULL))
            return false;
    }
    return true;
}

static bool xcli_validate_spec_fields(xvalue *spec) {
    static const char *fields[] = {
        "flags", "options", "shorts", "defaults", "positional"
    };
    size_t i, j;
    for (i = 0; i < xrtValueCount(spec); ++i) {
        xstrview name;
        bool known = false;
        xrtValueObjectAt(spec, (int64)i, &name);
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
static bool xcli_validate_short_aliases(xvalue *flags, xvalue *options,
    xvalue *shorts) {
    unsigned char used[128] = { 0 };
    xvalue *mappings[2] = { options, shorts };
    size_t group;
    size_t i;
    if (flags != NULL) {
        for (i = 0; i < xrtValueCount(flags); ++i) {
            xstrview name;
            if (xrtValueGetString(xrtValueArrayAt(flags, (int64)i), &name) &&
                name.Size == 1u && (unsigned char)name.Data[0] < 128u) {
                used[(unsigned char)name.Data[0]] = 1u;
            }
            if (options != NULL &&
                xrtValueObjectGet(options, name) != NULL) return false;
        }
    }
    for (group = 0; group < 2u; ++group) {
        xvalue *mapping = mappings[group];
        if (mapping == NULL) continue;
        for (i = 0; i < xrtValueCount(mapping); ++i) {
            xstrview name;
            xstrview aliases;
            xvalue *value = xrtValueObjectAt(mapping, (int64)i, &name);
            size_t j;
            if (!xrtValueGetString(value, &aliases)) return false;
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
    const char *name, const char *suffix) {
    size_t prefixLen = strlen(prefix);
    size_t nameLen = strlen(name);
    size_t suffixLen = strlen(suffix);
    char *message;
    bool ok;
    if (prefixLen > SIZE_MAX - suffixLen - 1u ||
        nameLen > SIZE_MAX - prefixLen - suffixLen - 1u) {
        xrtSetErrorInfo(XERR_RANGE, XCLI_DOMAIN, 0,
            "option name is too long");
        return false;
    }
    message = (char *)xrtMalloc(prefixLen + nameLen + suffixLen + 1u);
    if (message == NULL) return false;
    memcpy(message, prefix, prefixLen);
    memcpy(message + prefixLen, name, nameLen);
    memcpy(message + prefixLen + nameLen, suffix, suffixLen);
    message[prefixLen + nameLen + suffixLen] = '\0';
    ok = xcli_fail_result(result, message);
    xrtFree(message);
    return ok;
}

xvalue *xcli_parse(xvalue *args, xvalue *spec) {
    xvalue *flagsSpec;
    xvalue *optionsSpec;
    xvalue *shortsSpec;
    xvalue *defaultsSpec;
    xvalue *positionalSpec;
    xvalue *result = NULL;
    xvalue *flagsOut = NULL;
    xvalue *optionsOut = NULL;
    xvalue *positionalOut = NULL;
    xvalue *restOut = NULL;
    char *token = NULL;
    int64 i;
    int64 n;
    int noMoreOptions;
    int64 positionalIndex;
    int64 positionalCount;
    xstrview longName;

    xrtClearError();
    if (args == NULL || spec == NULL ||
        xrtValueType(args) != XVALUE_ARRAY ||
        xrtValueType(spec) != XVALUE_OBJECT) {
        xcli_error("parse requires an args array and a spec object");
        return NULL;
    }
    if (!xcli_validate_spec_fields(spec)) {
        xcli_error("spec contains an unknown field");
        return NULL;
    }

    flagsSpec = xcli_get(spec, "flags");
    optionsSpec = xcli_get(spec, "options");
    shortsSpec = xcli_get(spec, "shorts");
    defaultsSpec = xcli_get(spec, "defaults");
    positionalSpec = xcli_get(spec, "positional");
    if (flagsSpec != NULL && xrtValueType(flagsSpec) != XVALUE_ARRAY) {
        xcli_error("spec flags must be an array");
        return NULL;
    }
    if (optionsSpec != NULL && xrtValueType(optionsSpec) != XVALUE_OBJECT) {
        xcli_error("spec options must be an object");
        return NULL;
    }
    if (shortsSpec != NULL && xrtValueType(shortsSpec) != XVALUE_OBJECT) {
        xcli_error("spec shorts must be an object");
        return NULL;
    }
    if (defaultsSpec != NULL && xrtValueType(defaultsSpec) != XVALUE_OBJECT) {
        xcli_error("spec defaults must be an object");
        return NULL;
    }
    if (positionalSpec != NULL &&
        xrtValueType(positionalSpec) != XVALUE_ARRAY) {
        xcli_error("spec positional must be an array");
        return NULL;
    }
    if (!xcli_validate_names(flagsSpec) ||
        !xcli_validate_names(positionalSpec) ||
        !xcli_validate_mapping(optionsSpec, true) ||
        !xcli_validate_mapping(shortsSpec, true) ||
        !xcli_validate_mapping(defaultsSpec, false) ||
        !xcli_validate_short_aliases(flagsSpec, optionsSpec, shortsSpec)) {
        xcli_error("spec contains an invalid name or short alias");
        return NULL;
    }
    if (!xcli_validate_defaults(optionsSpec, defaultsSpec)) {
        xcli_error("spec defaults must be NUL-free strings for declared options");
        return NULL;
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
        int64 dn = xrtValueCount(defaultsSpec);
        for (di = 0; di < dn; ++di) {
            xstrview key;
            xvalue *value = xrtValueObjectAt(defaultsSpec, di, &key);
            if (value != NULL) {
                xvalue *copy = xrtValueDeepClone(value);
                if (!xcli_set_view(optionsOut, key, copy)) goto fail;
            }
        }
    }
    /* 未指定默认值的选项以空字符串初始化。 */
    if (optionsSpec != NULL) {
        int64 oi;
        int64 on = xrtValueCount(optionsSpec);
        for (oi = 0; oi < on; ++oi) {
            xstrview key;
            xvalue *value = xrtValueObjectAt(optionsSpec, oi, &key);
            if (value != NULL) {
                if (xrtValueObjectGet(optionsOut, key) == NULL) {
                    if (!xcli_set_str_view(optionsOut, key, "")) goto fail;
                }
            }
        }
    }
    /* 所有声明的旗标先置为 false。 */
    if (flagsSpec != NULL) {
        int64 fi;
        int64 fn = xrtValueCount(flagsSpec);
        for (fi = 0; fi < fn; ++fi) {
            xvalue *value = xrtValueArrayAt(flagsSpec, fi);
            xstrview text;
            if (value != NULL && xrtValueGetString(value, &text)) {
                if (!xcli_set_bool_view(flagsOut, text, 0)) goto fail;
            }
        }
    }

    noMoreOptions = 0;
    positionalIndex = 0;
    positionalCount = positionalSpec != NULL
        ? xrtValueCount(positionalSpec) : 0;
    n = xrtValueCount(args);

    for (i = 0; i < n; ++i) {
        token = xcli_arg(args, i);
        if (token == NULL) {
            if (xrtErrorKind(xrtGetError()) != XERR_MEMORY)
                xcli_error("args entries must be strings without NUL");
            goto fail;
        }
        if (noMoreOptions || token[0] != '-' || token[1] == '\0') {
            /* 位置参数。 */
            if (positionalIndex < positionalCount) {
                char *pname = xcli_arg(positionalSpec, positionalIndex);
                if (pname == NULL) {
                    if (xrtErrorKind(xrtGetError()) != XERR_MEMORY)
                        xcli_error("positional names must be strings");
                    goto fail;
                }
                if (!xcli_set_str(positionalOut, pname, token)) {
                    xrtFree(pname);
                    goto fail;
                }
                xrtFree(pname);
                positionalIndex++;
            } else {
                if (!xrtValueArrayAppendNew(restOut, xcli_text(token))) goto fail;
            }
            xrtFree(token);
            token = NULL;
            continue;
        }
        if (strcmp(token, "--") == 0) {
            noMoreOptions = 1;
            xrtFree(token);
            token = NULL;
            continue;
        }
        if (token[1] == '-') {
            /* 长选项与等号内联值。 */
            char *name = token + 2;
            char *eq = strchr(name, '=');
            const char *inlineValue = NULL;
            int hasInline = 0;
            if (eq != NULL) {
                inlineValue = eq + 1;
                *eq = '\0';
                hasInline = 1;
            }
            if (xcli_is_option(optionsSpec, name)) {
                if (hasInline) {
                    if (!xcli_set_str(optionsOut, name, inlineValue)) goto fail;
                } else if (i + 1 < n) {
                    char *next = xcli_arg(args, ++i);
                    if (next == NULL) {
                        if (xrtErrorKind(xrtGetError()) != XERR_MEMORY)
                            xcli_error("option value must be a string");
                        goto fail;
                    }
                    if (!xcli_set_str(optionsOut, name, next)) {
                        xrtFree(next);
                        goto fail;
                    }
                    xrtFree(next);
                } else {
                    if (!xcli_fail_long_result(result,
                        "missing value for --", name, "")) goto fail;
                    xrtFree(token);
                    token = NULL;
                    break;
                }
            } else if (xcli_is_flag(flagsSpec, name)) {
                if (hasInline) {
                    if (!xcli_fail_long_result(result, "flag --", name,
                        " does not take a value")) goto fail;
                    xrtFree(token);
                    token = NULL;
                    break;
                }
                if (!xcli_set_bool(flagsOut, name, 1)) goto fail;
            } else {
                if (!xcli_fail_long_result(result,
                    "unknown option --", name, "")) goto fail;
                xrtFree(token);
                token = NULL;
                break;
            }
            xrtFree(token);
            token = NULL;
            continue;
        }
        /* 短旗标组合与短选项附加值。 */
        {
            char *p = token + 1;
            int stop = 0;
            while (!stop && *p != '\0') {
                char letter = *p;
                if ((unsigned char)letter < 0x21 ||
                    (unsigned char)letter > 0x7e) {
                    if (!xcli_fail_result(result,
                        "short option must be printable ASCII")) goto fail;
                    stop = 2;
                } else if (xcli_short_in(optionsSpec, letter, &longName)) {
                    if (p[1] != '\0') {
                        if (!xcli_set_str_view(optionsOut, longName, p + 1)) goto fail;
                        stop = 1;
                    } else if (i + 1 < n) {
                        char *next = xcli_arg(args, ++i);
                        if (next == NULL) {
                            if (xrtErrorKind(xrtGetError()) != XERR_MEMORY)
                                xcli_error("option value must be a string");
                            goto fail;
                        }
                        if (!xcli_set_str_view(optionsOut, longName, next)) {
                            xrtFree(next);
                            goto fail;
                        }
                        xrtFree(next);
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
                    char flagName[2];
                    flagName[0] = letter;
                    flagName[1] = '\0';
                    if (xcli_is_flag(flagsSpec, flagName)) {
                        if (!xcli_set_bool(flagsOut, flagName, 1)) goto fail;
                    } else {
                        char message[80];
                        snprintf(message, sizeof(message),
                            "unknown short option -%c", letter);
                        if (!xcli_fail_result(result, message)) goto fail;
                        stop = 2;
                    }
                }
                p++;
            }
            xrtFree(token);
            token = NULL;
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
    return result;

fail:
    xrtFree(token);
    xrtValueRelease(restOut);
    xrtValueRelease(positionalOut);
    xrtValueRelease(optionsOut);
    xrtValueRelease(flagsOut);
    xrtValueRelease(result);
    return NULL;
}
