#include "src/common/xl6_xrt.h"
#include "xl6_foreign_scope_view.h"
#include <stdlib.h>

static xatomic64 nextIdentity;
static XL6_ForeignScopeLifetimeV1* top(void)
{return xl6ForeignContextGetV1(XL6_FOREIGN_CONTEXT_SCOPED_VIEWS_V1);}
static bool contains(const XL6_ForeignScopeLifetimeV1* scope)
{
    for(const XL6_ForeignScopeLifetimeV1* item=top();item;item=item->older)
        if(item==scope)return item->self==item && item->identity;
    return false;
}
static bool emptyOutput(const XL6_ForeignScopeViewV1* value)
{return value && !((uintptr_t)value%_Alignof(XL6_ForeignScopeViewV1)) && !value->address && !value->lifetime;}
static const XL6_ForeignScopeLifetimeV1* resolve(XL6_ForeignScopeViewV1 value)
{
    if(!value.address || !value.lifetime)return NULL;
    for(const XL6_ForeignScopeLifetimeV1* item=top();item;item=item->older)
        if(item->identity==value.lifetime && item->self==item && item->ownerCall)
            return item;
    return NULL;
}
bool xl6ForeignScopeBeginV1(XL6_ForeignScopeLifetimeV1* scope,const XL6_ForeignViewCallV1* owner)
{
    if(!scope || (uintptr_t)scope%_Alignof(XL6_ForeignScopeLifetimeV1) ||
        scope->self || scope->older || scope->newer || scope->ownerCall || scope->identity || !owner){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    if(!xl6ForeignContextReadyV1())return false;
    if(!xl6ForeignCallbackActiveV1() || owner->self!=owner || !owner->root ||
        !xl6ForeignContextCallContainsV1(&owner->execution)){
        xrtSetErrorKind(XERR_STATE);return false;
    }
    uint64_t prior=xrtAtomic64Load(&nextIdentity,XMEMORY_RELAXED);
    do {
        if(prior==UINT64_MAX){xrtSetErrorKind(XERR_RANGE);return false;}
    }while(!xrtAtomic64CompareExchange(&nextIdentity,&prior,prior+1,XMEMORY_RELAXED,XMEMORY_RELAXED));
    XL6_ForeignScopeLifetimeV1* older=top();
    if(!xl6ForeignContextSetV1(XL6_FOREIGN_CONTEXT_SCOPED_VIEWS_V1,scope))return false;
    *scope=(XL6_ForeignScopeLifetimeV1){scope,older,NULL,owner,prior+1};
    if(older)older->newer=scope;
    return true;
}
bool xl6ForeignScopeEndV1(XL6_ForeignScopeLifetimeV1* scope)
{
    if(!scope){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(!xl6ForeignContextReadyV1())return false;
    if(!contains(scope)){xrtSetErrorKind(XERR_STATE);return false;}
    /* Publication succeeded at Begin. A failed OS slot update cannot leave a
     * borrowed stack pointer published while returning from its frame. */
    if(!scope->newer && !xl6ForeignContextSetV1(XL6_FOREIGN_CONTEXT_SCOPED_VIEWS_V1,scope->older))abort();
    if(scope->newer)scope->newer->older=scope->older;
    if(scope->older)scope->older->newer=scope->newer;
    *scope=(XL6_ForeignScopeLifetimeV1){0};
    return true;
}
bool xl6ForeignScopeViewMakeV1(const XL6_ForeignScopeLifetimeV1* scope,void* raw,XL6_ForeignScopeViewV1* output)
{
    if(!emptyOutput(output)){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(!xl6ForeignContextReadyV1())return false;
    if(!contains(scope) || !scope->ownerCall){xrtSetErrorKind(XERR_STATE);return false;}
    if(raw)*output=(XL6_ForeignScopeViewV1){(uintptr_t)raw,scope->identity};
    return true;
}
bool xl6ForeignScopeViewDeriveV1(XL6_ForeignScopeViewV1 source,void* raw,XL6_ForeignScopeViewV1* output)
{
    if(!emptyOutput(output)){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(!xl6ForeignContextReadyV1())return false;
    if(!resolve(source)){xrtSetErrorKind(XERR_STATE);return false;}
    if(raw)*output=(XL6_ForeignScopeViewV1){(uintptr_t)raw,source.lifetime};
    return true;
}
bool xl6ForeignScopeViewGetV1(XL6_ForeignScopeViewV1 value,void** raw,XL6_ForeignView** owner)
{
    if((raw && (uintptr_t)raw%_Alignof(void*)) || (owner && (uintptr_t)owner%_Alignof(XL6_ForeignView*))){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    if(!xl6ForeignContextReadyV1())return false;
    const XL6_ForeignScopeLifetimeV1* scope=resolve(value);
    if(!scope){xrtSetErrorKind(XERR_STATE);return false;}
    if(raw)*raw=(void*)value.address;
    if(owner)*owner=scope->ownerCall->root;
    return true;
}
bool xl6ForeignScopeViewAliveV1(XL6_ForeignScopeViewV1 value)
{return xl6ForeignContextReadyV1() && resolve(value)!=NULL;}
void xl6ForeignScopeExpireOwnerV1(const XL6_ForeignViewCallV1* owner)
{
    for(XL6_ForeignScopeLifetimeV1* item=top();item;item=item->older)
        if(item->ownerCall==owner)item->ownerCall=NULL;
}
