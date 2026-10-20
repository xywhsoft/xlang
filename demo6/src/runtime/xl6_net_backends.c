#include "xl6_net_backends_internal.h"

/* Query the native product, not this declaration unit's feature macros. The
 * same resident adapter is used by JIT, modules and pruned standalone AOT. */
bool xl6NetBackendCompiledV1(int64_t backend)
{xnetportbackendinfo info;return xl6NetBackendInfoInternalV1(backend,&info);}
int64_t xl6NetBackendCapabilitiesV1(int64_t backend)
{xnetportbackendinfo info;return xl6NetBackendInfoInternalV1(backend,&info)?info.Capabilities:0;}
int64_t xl6NetDefaultBackendV1(void)
{
    xnetportbackendinfo info;
    if(!xl6NetBackendInfoInternalV1(XNET_PORT_AUTO,&info)){xrtSetErrorKind(XERR_NOT_FOUND);return 0;}
    return info.Backend;
}
