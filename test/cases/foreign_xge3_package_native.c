/* Headless shared-XRT bootstrap/policy smoke. The public package initialization
 * is separately tested through an imported language module, not this main. */
#include <xge3_runtime.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    if (xl6Xge3RuntimeBindV1() || xl6Xge3RuntimeBindV1()) return 1;
    const XL6_ForeignViewDescriptorV1* context=xl6Xui3ContextPolicyGet();
    const XL6_ForeignViewDescriptorV1* widget=xl6Xui3WidgetPolicyGet();
    if (!context || !widget || !context->TraceRaw || !context->Busy ||
        !context->CallBegin || !context->CallEnd || !context->Watch || !widget->Watch) return 2;
    xui_context raw=NULL;
    if (xuiCreate(&raw)!=XUI_OK || !raw) return 3;
    xui_widget child=NULL;
    if (xuiWidgetCreate(raw,&child)!=XUI_OK || !child || xuiSetRootWidget(raw,child)!=XUI_OK) return 4;
    if (!context->Destroy(raw)) return 5;
    puts("shared-XRT package bootstrap and policies passed");
    return 0;
}
