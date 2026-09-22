/*
 * cli_native.c - Command line argument parsing with flag/option grammar.
 *
 * Grammar per token:
 *   --name            long flag, or long option taking the next token
 *   --name=value      long option with inline value
 *   -a                short flag
 *   -abc              grouped short flags
 *   -o VALUE          short option taking the next token
 *   -oVALUE           short option with attached value
 *   --                everything after this is positional
 *   otherwise         positional
 *
 * parse never raises for user input mistakes: the result object carries
 * ok=false plus an error string. A malformed spec reports an XRT error and
 * returns null so the xlang wrapper can raise.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"

#define XCLI_DOMAIN "xlang.extlib.cli"

static void xcli_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XCLI_DOMAIN, 0, message);
}

/* ------------------------------------------------------------------ */
/* xvalue object helpers (no struct-by-value calls, bson-style take)   */
/* ------------------------------------------------------------------ */

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
    int64 i;
    int64 n;
    size_t len = strlen(key);
    if (obj == NULL) {
        return NULL;
    }
    n = xrtValueCount(obj);
    for (i = 0; i < n; ++i) {
        xstrview k;
        xvalue *v = xrtValueObjectAt(obj, i, &k);
        if (v != NULL && k.Size == len && memcmp(k.Data, key, len) == 0) {
            return v;
        }
    }
    return NULL;
}

static int xcli_set(xvalue *obj, const char *key, xvalue *value) {
    xstrview view;
    view.Data = key;
    view.Size = strlen(key);
    return xrtValueObjectSetNew(obj, view, value);
}

static int xcli_set_str(xvalue *obj, const char *key, const char *text) {
    return xcli_set(obj, key, xcli_text(text));
}

static int xcli_set_bool(xvalue *obj, const char *key, int b) {
    return xcli_set(obj, key, xrtValueBool(b ? true : false));
}

/* Copy an argument token out of the args array. */
static char *xcli_arg(xvalue *args, int64 index) {
    xvalue *value = xrtValueArrayAt(args, index);
    xstrview text;
    if (value == NULL || !xrtValueGetString(value, &text)) {
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

/* Does the flags array contain name? */
static int xcli_flags_has(xvalue *flags, const char *name) {
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
            text.Size == strlen(name) &&
            memcmp(text.Data, name, text.Size) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Look up the long name for a short letter in an options/shorts object. */
static int xcli_short_in(xvalue *mapping, char shortName,
    char *out, size_t outCap) {
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
                    if (key.Size >= outCap) {
                        return 0;
                    }
                    memcpy(out, key.Data, key.Size);
                    out[key.Size] = '\0';
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

static void xcli_fail_result(xvalue *result, const char *message) {
    xcli_set_bool(result, "ok", 0);
    xcli_set_str(result, "error", message);
}

xvalue *xcli_parse(xvalue *args, xvalue *spec) {
    xvalue *flagsSpec;
    xvalue *optionsSpec;
    xvalue *shortsSpec;
    xvalue *defaultsSpec;
    xvalue *positionalSpec;
    xvalue *result;
    xvalue *flagsOut;
    xvalue *optionsOut;
    xvalue *positionalOut;
    xvalue *restOut;
    int64 i;
    int64 n;
    int noMoreOptions;
    int64 positionalIndex;
    int64 positionalCount;
    char longName[128];

    if (args == NULL || spec == NULL ||
        xrtValueType(args) != XVALUE_ARRAY ||
        xrtValueType(spec) != XVALUE_OBJECT) {
        xcli_error("parse requires an args array and a spec object");
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

    result = xrtValueObject();
    if (result == NULL) {
        return NULL;
    }
    flagsOut = xrtValueObject();
    optionsOut = xrtValueObject();
    positionalOut = xrtValueObject();
    restOut = xrtValueArray();
    if (flagsOut == NULL || optionsOut == NULL || positionalOut == NULL ||
        restOut == NULL) {
        xcli_error("out of memory");
        return NULL;
    }

    /* seed options with defaults */
    if (defaultsSpec != NULL) {
        int64 di;
        int64 dn = xrtValueCount(defaultsSpec);
        for (di = 0; di < dn; ++di) {
            xstrview key;
            xvalue *value = xrtValueObjectAt(defaultsSpec, di, &key);
            if (value != NULL) {
                char name[128];
                if (key.Size < sizeof(name)) {
                    memcpy(name, key.Data, key.Size);
                    name[key.Size] = '\0';
                    {
                        xvalue *copy = xrtValueDeepClone(value);
                        xcli_set(optionsOut, name, copy);
                    }
                }
            }
        }
    }
    /* every declared option resolves to at least an empty string */
    if (optionsSpec != NULL) {
        int64 oi;
        int64 on = xrtValueCount(optionsSpec);
        for (oi = 0; oi < on; ++oi) {
            xstrview key;
            xvalue *value = xrtValueObjectAt(optionsSpec, oi, &key);
            if (value != NULL && key.Size < sizeof(longName)) {
                memcpy(longName, key.Data, key.Size);
                longName[key.Size] = '\0';
                if (xcli_get(optionsOut, longName) == NULL) {
                    xcli_set_str(optionsOut, longName, "");
                }
            }
        }
    }
    /* seed all declared flags as false */
    if (flagsSpec != NULL) {
        int64 fi;
        int64 fn = xrtValueCount(flagsSpec);
        for (fi = 0; fi < fn; ++fi) {
            xvalue *value = xrtValueArrayAt(flagsSpec, fi);
            xstrview text;
            if (value != NULL && xrtValueGetString(value, &text) &&
                text.Size < sizeof(longName)) {
                memcpy(longName, text.Data, text.Size);
                longName[text.Size] = '\0';
                xcli_set_bool(flagsOut, longName, 0);
            }
        }
    }

    noMoreOptions = 0;
    positionalIndex = 0;
    positionalCount = positionalSpec != NULL
        ? xrtValueCount(positionalSpec) : 0;
    n = xrtValueCount(args);

    for (i = 0; i < n; ++i) {
        char *token = xcli_arg(args, i);
        if (token == NULL) {
            xcli_error("args entries must be strings");
            return NULL;
        }
        if (noMoreOptions || token[0] != '-' || token[1] == '\0') {
            /* positional */
            if (positionalIndex < positionalCount) {
                char *pname = xcli_arg(positionalSpec, positionalIndex);
                if (pname == NULL) {
                    xrtFree(token);
                    xcli_error("positional names must be strings");
                    return NULL;
                }
                xcli_set_str(positionalOut, pname, token);
                xrtFree(pname);
                positionalIndex++;
            } else {
                xrtValueArrayAppendNew(restOut, xcli_text(token));
            }
            xrtFree(token);
            continue;
        }
        if (strcmp(token, "--") == 0) {
            noMoreOptions = 1;
            xrtFree(token);
            continue;
        }
        if (token[1] == '-') {
            /* long option --name or --name=value */
            char *name = token + 2;
            char *eq = strchr(name, '=');
            char valueBuf[512];
            int hasInline = 0;
            if (eq != NULL) {
                size_t vlen = strlen(eq + 1);
                if (vlen >= sizeof(valueBuf)) vlen = sizeof(valueBuf) - 1;
                memcpy(valueBuf, eq + 1, vlen);
                valueBuf[vlen] = '\0';
                *eq = '\0';
                hasInline = 1;
            }
            if (xcli_is_option(optionsSpec, name)) {
                if (hasInline) {
                    xcli_set_str(optionsOut, name, valueBuf);
                } else if (i + 1 < n) {
                    char *next = xcli_arg(args, ++i);
                    if (next == NULL) {
                        xrtFree(token);
                        xcli_error("option value must be a string");
                        return NULL;
                    }
                    xcli_set_str(optionsOut, name, next);
                    xrtFree(next);
                } else {
                    char message[160];
                    snprintf(message, sizeof(message),
                        "missing value for --%s", name);
                    xcli_fail_result(result, message);
                    xrtFree(token);
                    break;
                }
            } else if (xcli_is_flag(flagsSpec, name)) {
                xcli_set_bool(flagsOut, name, 1);
            } else {
                char message[160];
                snprintf(message, sizeof(message), "unknown option --%s", name);
                xcli_fail_result(result, message);
                xrtFree(token);
                break;
            }
            xrtFree(token);
            continue;
        }
        /* short options: -a, -abc, -oVALUE, -o VALUE */
        {
            char *p = token + 1;
            int stop = 0;
            while (!stop && *p != '\0') {
                char letter = *p;
                if (xcli_short_in(optionsSpec, letter, longName,
                        sizeof(longName))) {
                    if (p[1] != '\0') {
                        xcli_set_str(optionsOut, longName, p + 1);
                        stop = 1;
                    } else if (i + 1 < n) {
                        char *next = xcli_arg(args, ++i);
                        if (next == NULL) {
                            xrtFree(token);
                            xcli_error("option value must be a string");
                            return NULL;
                        }
                        xcli_set_str(optionsOut, longName, next);
                        xrtFree(next);
                        stop = 1;
                    } else {
                        char message[80];
                        snprintf(message, sizeof(message),
                            "missing value for -%c", letter);
                        xcli_fail_result(result, message);
                        stop = 1;
                    }
                } else if (xcli_short_in(shortsSpec, letter, longName,
                               sizeof(longName))) {
                    xcli_set_bool(flagsOut, longName, 1);
                } else {
                    char flagName[2];
                    flagName[0] = letter;
                    flagName[1] = '\0';
                    if (xcli_is_flag(flagsSpec, flagName)) {
                        xcli_set_bool(flagsOut, flagName, 1);
                    } else {
                        char message[80];
                        snprintf(message, sizeof(message),
                            "unknown short option -%c", letter);
                        xcli_fail_result(result, message);
                        stop = 1;
                    }
                }
                p++;
            }
            xrtFree(token);
            continue;
        }
    }

    xcli_set(result, "flags", flagsOut);
    xcli_set(result, "options", optionsOut);
    xcli_set(result, "positional", positionalOut);
    xcli_set(result, "rest", restOut);
    if (xcli_get(result, "ok") == NULL) {
        xcli_set_bool(result, "ok", 1);
        xcli_set_str(result, "error", "");
    }
    return result;
}
