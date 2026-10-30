/* 仅隔离 CLI 的值投影边界，不导入带 parseOs 的语言模块，不作为用户替代实现。 */
#include "../../release/extlibs/cli/cli_native.c"

bool xcli_projection_native_control(void);

/* 同一执行环境中以真正的XRT数组/对象传入完全相同参数，先确认解析器可用。 */
bool xcli_projection_native_control(void)
{
    static const char token[] = "--output=report.csv";
    xvalue *args = xrtValueArray(), *spec = xrtValueObject(), *options = xrtValueObject();
    xvalue* result = NULL;
    xstrview text;
    bool accepted = false, ok = false;
    if (!args || !spec || !options ||
        !xrtValueArrayAppendNew(args, xrtValueString((xstrview){token, sizeof(token) - 1})) ||
        !xrtValueObjectSetNew(options, (xstrview){"output", 6}, xrtValueString((xstrview){"o", 1})) ||
        !xrtValueObjectSetTake(spec, (xstrview){"options", 7}, &options)) goto done;
    result = xcli_parse(args, spec);
    if (result && xrtValueGetBool(xrtValueObjectGet(result, (xstrview){"ok", 2}), &accepted) && accepted) {
        xvalue* output = xrtValueObjectGet(result, (xstrview){"options", 7});
        ok = output && xrtValueGetString(xrtValueObjectGet(output, (xstrview){"output", 6}), &text) &&
            text.Size == 10 && !memcmp(text.Data, "report.csv", 10) && !xrtGetError();
    }
done:
    xrtValueRelease(result);
    xrtValueRelease(options);
    xrtValueRelease(spec);
    xrtValueRelease(args);
    return ok;
}
