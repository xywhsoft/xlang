/* C 与 XLang 同一合同：零值、缺失和空文本须分别判断。 */
#include "../hcl_native.c"
#include <stdio.h>

int main(void)
{
    const char* config = "port = 0\nenabled = false\nratio = 1.25\nnote = \"\"\n";
    bool ok = xhcl_has(config, "port") && xhcl_get_int(config, "port") == 0 &&
        xrtGetError() == NULL && xhcl_get_int(config, "missing") == 0 &&
        xrtGetError() == NULL && !xhcl_has(config, "missing") &&
        !xhcl_get_bool(config, "enabled") && xrtGetError() == NULL &&
        xhcl_get_float(config, "ratio") == 1.25 && xrtGetError() == NULL;
    (void)xhcl_get_int(config, "note");
    ok = xrtErrorKind(xrtGetError()) == XERR_VALUE && ok;
    char* text = xhcl_get(config, "note");
    ok = text != NULL && text[0] == 0 && xrtGetError() == NULL &&
        xhcl_has(config, "note") && ok;
    xrtFree(text);
    if (!ok) return 1;
    puts("port: 0");
    puts("missing port: 0");
    puts("enabled: false");
    puts("ratio: 1.25");
    puts("empty scalar: rejected");
    puts("text stays empty: true");
    return 0;
}
