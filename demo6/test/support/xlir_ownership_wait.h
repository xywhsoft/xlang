#ifndef XLIR_OWNERSHIP_WAIT_H
#define XLIR_OWNERSHIP_WAIT_H

#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <windows.h>

/* Generated module initialization starts real executor workers. Their short
 * ownership mutations may refuse a nonblocking snapshot before a test job
 * runs. Retry only BUSY with a deadline; callers still assert every semantic
 * result, exact reference count, diagnostic and allocator balance. */
static void xlirOwnershipFreezeBegin(xrtownershipscope* scope)
{
    ULONGLONG stop=GetTickCount64()+5000;
    const xerror* previous=xrtGetError();
    while(!xrtOwnershipFreezeTryBegin(scope)){
        assert(xrtGetError()==previous&&GetTickCount64()<stop);SwitchToThread();
    }
}
static XL6_OwnershipPlanResult xlirOwnershipPlanCreateV2(xrtownershipref root,
    XL6_OwnershipResolveV2 resolve,void* context,XL6_OwnershipPlan** plan)
{
    ULONGLONG stop=GetTickCount64()+5000;XL6_OwnershipPlan* before=*plan;
    XL6_OwnershipPlanResult result;
    while((result=xl6OwnershipPlanTryCreateV2(root,resolve,context,plan))==XL6_OWNERSHIP_PLAN_BUSY){
        assert(*plan==before&&GetTickCount64()<stop);SwitchToThread();
    }
    return result;
}
static XL6_OwnershipPlanResult xlirOwnershipPlanStep(
    XL6_OwnershipPlanResult (*step)(XL6_OwnershipPlan*),XL6_OwnershipPlan* plan)
{
    ULONGLONG stop=GetTickCount64()+5000;XL6_OwnershipPlanResult result;
    while((result=step(plan))==XL6_OWNERSHIP_PLAN_BUSY){
        assert(GetTickCount64()<stop);SwitchToThread();
    }
    return result;
}
#endif
