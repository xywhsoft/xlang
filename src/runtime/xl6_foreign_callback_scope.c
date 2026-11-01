#include "src/common/xl6_xrt.h"
#include "xl6_foreign_callback_scope.h"
#include "xl6_foreign_context.h"
#include <stdlib.h>

static XL6_ForeignCallbackScopeV1* callbackTop(void)
{ return xl6ForeignContextGetV1(XL6_FOREIGN_CONTEXT_CALLBACK_TOP_V1); }

bool xl6ForeignCallbackEnterV1(XL6_ForeignCallbackScopeV1* scope)
{
    if (!scope || scope->self || scope->previous || scope->thread) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    if(!xl6ForeignContextReadyV1())return false;
    XL6_ForeignCallbackScopeV1* previous=callbackTop();
    if(!xl6ForeignContextSetV1(XL6_FOREIGN_CONTEXT_CALLBACK_TOP_V1,scope))return false;
    scope->self=scope;scope->previous=previous;scope->thread=xrtThreadCurrentId();return true;
}
bool xl6ForeignCallbackLeaveV1(XL6_ForeignCallbackScopeV1* scope)
{
    if (!scope || scope->self!=scope || callbackTop()!=scope || scope->thread!=xrtThreadCurrentId()) {
        xrtSetErrorKind(XERR_STATE); return false;
    }
    /* Restoring a successfully installed binding cannot allocate. Leaving a
     * pointer to an expired C stack on an OS failure is never safe. */
    if(!xl6ForeignContextSetV1(XL6_FOREIGN_CONTEXT_CALLBACK_TOP_V1,scope->previous))abort();
    *scope=(XL6_ForeignCallbackScopeV1){0};
    return true;
}
bool xl6ForeignCallbackActiveV1(void)
{ return xl6ForeignContextReadyV1() && callbackTop()!=NULL; }
