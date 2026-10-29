#include "xl6_text_internal.h"
xstrview xl6NetInterfaceNameV1(void* raw)
{
    if(!raw){xrtSetErrorKind(XERR_ARGUMENT);return xl6TextFailure;}
    xstrview text=((const xnetinterface*)raw)->Name;return xl6TextSelect(&text,-1)?xl6TextCopy(text):xl6TextFailure;
}
xstrview xl6NetInterfaceDisplayNameV1(void* raw)
{
    if(!raw){xrtSetErrorKind(XERR_ARGUMENT);return xl6TextFailure;}
    xstrview text=((const xnetinterface*)raw)->DisplayName;return xl6TextSelect(&text,-1)?xl6TextCopy(text):xl6TextFailure;
}
