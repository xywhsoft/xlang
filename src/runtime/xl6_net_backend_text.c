#include "xl6_text_internal.h"
#include "xl6_net_backends_internal.h"
#include <string.h>
xstrview xl6NetBackendNameV1(int64_t backend)
{
    xnetportbackendinfo info;
    if(!xl6NetBackendInfoInternalV1(backend,&info)){xrtSetErrorKind(XERR_NOT_FOUND);return xl6TextFailure;}
    return xl6TextCopy((xstrview){info.Name,strlen(info.Name)});
}
