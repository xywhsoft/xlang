/* 先按命令行清单生成唯一一份 XRT；内部头不能再补全缺失的实现。 */
#include "xrt.h"
#include "../xrt_toml_internal.h"
#include "../../_tomlc17/tomlc17_xrt.c"
#include "../xrt_toml_types.c"
#include "../xrt_toml_parser.c"
#include "../xrt_toml_writer.c"
#include "../toml_native.c"

int main(void)
{
    static const char source[] = "value = 1\n";
    xvalue* root = xtoml_parse(source,sizeof(source)-1,0,0,0,0,0,0);
    str text = root != NULL ? xtoml_stringify_text(root,0,0,0,0,0) : NULL;
    bool ok = text != NULL && strcmp(text,source) == 0 && xtoml_error_code() == 0;
    xrtFree(text); xrtValueRelease(root);
    if (ok) puts("toml manifest-only native bridge: PASS");
    return ok ? 0 : 1;
}
