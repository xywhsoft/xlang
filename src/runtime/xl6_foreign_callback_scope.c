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
bool xl6ForeignCallbackContainsV1(const XL6_ForeignCallbackScopeV1* scope)
{
    if(!scope || !xl6ForeignContextReadyV1())return false;
    for(const XL6_ForeignCallbackScopeV1* item=callbackTop();item;item=item->previous)
        if(item==scope)return item->self==item && item->thread==xrtThreadCurrentId();
    return false;
}
/* These callbacks are resident even when their publishing native object's
 * code is the final dependency being retired. The marker covers host Release
 * and defers native-image retirement until the publisher's physical return. */
bool xl6CodeLeaseHoldNativeV1(const void* data)
{return !data || xl6CodeLeaseRetain((XL6_CodeLease*)data)!=NULL;}
bool xl6CodeLeaseDropNativeV1(const void* data)
{
    if(!data)return true;
    XL6_ForeignCallbackScopeV1 callback={0};
    if(!xl6ForeignCallbackEnterV1(&callback))abort();
    xl6CodeLeaseRelease((XL6_CodeLease*)data);
    if(!xl6ForeignCallbackLeaveV1(&callback))abort();
    return true;
}
