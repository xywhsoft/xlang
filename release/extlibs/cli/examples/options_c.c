#include "../include/cli_native.h"
#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
#include "../cli_native.c"
#endif

static xstrview view(const char* text) { return (xstrview){text, strlen(text)}; }
static xvalue* field(xvalue* value, const char* key) { return xrtValueObjectGet(value, view(key)); }
static bool set_text(xvalue* value, const char* key, const char* text)
{
    return xrtValueObjectSetNew(value, view(key), xrtValueString(view(text)));
}
static xvalue* strings(const char* const* values, size_t count)
{
    xvalue* array = xrtValueArray();
    if (!array) return NULL;
    for (size_t i = 0; i < count; ++i) {
        if (!xrtValueArrayAppendNew(array, xrtValueString(view(values[i])))) {
            xrtValueRelease(array);
            return NULL;
        }
    }
    return array;
}
static bool line(const char* label, xvalue* value)
{
    xstrview text;
    return xrtValueGetString(value, &text) && printf("%s", label) >= 0 &&
        fwrite(text.Data, 1, text.Size, stdout) == text.Size && putchar('\n') != EOF;
}

int main(void)
{
    static const char* const flag_names[] = {"verbose", "force"};
    static const char* const positional_names[] = {"src"};
    static const char* const tokens[] = {"-vfo报告.csv", "--level", "5", "--", "-input", "--literal"};
    static const char* const bad_tokens[] = {"-z", "-v", "--unknown"};
    xvalue *spec = xrtValueObject(), *shorts = xrtValueObject(), *options = xrtValueObject();
    xvalue *defaults = xrtValueObject(), *args = NULL, *result = NULL;
    bool accepted, verbose, force;
    int status = 1;
    if (!spec || !shorts || !options || !defaults ||
        !set_text(shorts, "verbose", "v") || !set_text(shorts, "force", "f") ||
        !set_text(options, "output", "o") || !set_text(options, "level", "l") ||
        !set_text(defaults, "output", "report.csv") || !set_text(defaults, "level", "3") ||
        !xrtValueObjectSetNew(spec, view("flags"), strings(flag_names, 2)) ||
        !xrtValueObjectSetNew(spec, view("positional"), strings(positional_names, 1)) ||
        !xrtValueObjectSetTake(spec, view("shorts"), &shorts) ||
        !xrtValueObjectSetTake(spec, view("options"), &options) ||
        !xrtValueObjectSetTake(spec, view("defaults"), &defaults)) goto done;
    args = strings(tokens, 6);
    result = args ? xcli_parse(args, spec) : NULL;
    if (!result || !xrtValueGetBool(field(result, "ok"), &accepted) || !accepted ||
        !xrtValueGetBool(field(field(result, "flags"), "verbose"), &verbose) || !verbose ||
        !xrtValueGetBool(field(field(result, "flags"), "force"), &force) || !force ||
        xrtValueCount(field(result, "rest")) != 1) goto done;
    // 所有结果拥有自己的引用；参数数组在输出前释放，不借用其存储。
    xrtValueRelease(args);
    args = NULL;
    if (printf("flags: verbose=%s force=%s\n", verbose ? "true" : "false", force ? "true" : "false") < 0 ||
        !line("output: ", field(field(result, "options"), "output")) ||
        !line("level: ", field(field(result, "options"), "level")) ||
        !line("source: ", field(field(result, "positional"), "src")) ||
        !line("rest: ", xrtValueArrayAt(field(result, "rest"), 0))) goto done;
    xrtValueRelease(result);
    result = NULL;
    args = strings(bad_tokens, 3);
    result = args ? xcli_parse(args, spec) : NULL;
    if (!result || !xrtValueGetBool(field(result, "ok"), &accepted) || accepted ||
        !xrtValueGetBool(field(field(result, "flags"), "verbose"), &verbose) || verbose ||
        !line("first-error: ", field(result, "error"))) goto done;
    status = 0;
done:
    if (status) fputs("CLI options example failed\n", stderr);
    xrtValueRelease(result);
    xrtValueRelease(args);
    xrtValueRelease(defaults);
    xrtValueRelease(options);
    xrtValueRelease(shorts);
    xrtValueRelease(spec);
    return status;
}
