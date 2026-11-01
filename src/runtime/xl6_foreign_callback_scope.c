#include "src/common/xl6_xrt.h"
#include "xl6_foreign_callback_scope.h"

static _Thread_local XL6_ForeignCallbackScopeV1* callbackTop;

bool xl6ForeignCallbackEnterV1(XL6_ForeignCallbackScopeV1* scope)
{
    if (!scope || scope->self || scope->previous || scope->thread) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    scope->self=scope; scope->previous=callbackTop; scope->thread=xrtThreadCurrentId();
    callbackTop=scope; return true;
}
bool xl6ForeignCallbackLeaveV1(XL6_ForeignCallbackScopeV1* scope)
{
    if (!scope || scope->self!=scope || callbackTop!=scope || scope->thread!=xrtThreadCurrentId()) {
        xrtSetErrorKind(XERR_STATE); return false;
    }
    callbackTop=scope->previous; *scope=(XL6_ForeignCallbackScopeV1){0};
    return true;
}
bool xl6ForeignCallbackActiveV1(void)
{ return callbackTop!=NULL; }
