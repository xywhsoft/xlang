/* Process-owned CPU test provider; no model parsing, files, GPU or network. */
#ifndef XRT_MODULE_ALL
#define XRT_MODULE_ALL
#endif
#include "extlibs/xrt/xrt.h"
#include "test/cases/foreign_xge3_decode_budget.h"
#include <assert.h>
#include <stdio.h>
static xge3d_decode_budget* held;
static unsigned char* block;
int fixtureBudgetAttach(xge3d_decode_budget* budget)
{
    assert(!held && !block);
    int status=xge3dDecodeBudgetShareOwned(budget,&held);if(status!=XGE_OK)return status;
    xallocationscope scope={0};assert(xrtAllocationBudgetEnter(held,&scope));
    block=xrtMalloc(40);assert(xrtAllocationBudgetLeave(&scope));
    if(!block){xerror* first=xrtTakeError();xge3dDecodeBudgetFree(held);held=NULL;xrtSetErrorTake(first);return XGE_ERROR_OUT_OF_MEMORY;}
    block[0]=17;block[39]=91;return XGE_OK;
}
int fixtureBudgetTry(xge3d_decode_budget* budget,uint64_t bytes)
{
    assert(bytes && bytes<=SIZE_MAX);
    xallocationscope scope={0};assert(xrtAllocationBudgetEnter(budget,&scope));
    void* data=xrtMalloc((size_t)bytes);
    xerror* first=xrtTakeError();assert(xrtAllocationBudgetLeave(&scope));
    if(data)xrtFree(data);
    xrtSetErrorTake(first);return data?XGE_OK:XGE_ERROR_OUT_OF_MEMORY;
}
xge3d_decode_budget_info_t fixtureBudgetHeldInfo(void)
{
    assert(held && block && block[0]==17 && block[39]==91);
    xge3d_decode_budget_info_t value={.size=sizeof(value)};
    assert(xge3dDecodeBudgetInfo(held,&value)==XGE_OK);return value;
}
void fixtureBudgetDetach(void)
{
    assert(held && block);xrtFree(block);block=NULL;
    xge3d_decode_budget_info_t value={.size=sizeof(value)};
    assert(xge3dDecodeBudgetInfo(held,&value)==XGE_OK && !value.live_bytes && !value.live_allocations);
    xge3dDecodeBudgetFree(held);held=NULL;
}
int fixtureBudgetDuringCall(xge3d_decode_budget* budget,void (*callback)(void*),void* user)
{
    /* A separate real C owner makes the probe safe even if admission failed.
     * Active-close timing also relies on the resident root's existing deferred
     * close rule for leaf policies without a native CallBegin/CallEnd pair. */
    xge3d_decode_budget* pin=NULL;
    int status=xge3dDecodeBudgetShareOwned(budget,&pin);if(status!=XGE_OK)return status;
    callback(user);
    xerror* first=xrtTakeError();
    xge3d_decode_budget_info_t value={.size=sizeof(value)};
    assert(xge3dDecodeBudgetInfo(pin,&value)==XGE_OK && value.limit>0);
    xge3dDecodeBudgetFree(pin);xrtSetErrorTake(first);return XGE_OK;
}
void fixtureBudgetFail(int allowance)
{
    if(allowance<0)xrtMemDebugFailClear();
    else {assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(allowance));}
}
static xmemdebugsnapshot before;
void fixtureBudgetHeapBegin(void)
{assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
void fixtureBudgetHeapReport(int native)
{
    xmemdebugsnapshot now;xrtMemDebugSnapshot(&now);
    printf("DecodeBudget heap %s: alloc=%llu free=%llu realloc=%llu\n",native?"native":"language",
        (unsigned long long)(now.AllocCount-before.AllocCount),
        (unsigned long long)(now.FreeCount-before.FreeCount),
        (unsigned long long)(now.ReallocCount-before.ReallocCount));
    assert(now.AllocCount==before.AllocCount && now.FreeCount==before.FreeCount && now.ReallocCount==before.ReallocCount);
}
void fixtureBudgetNativePerformance(xge3d_decode_budget* budget)
{
    for(int i=0;i<1000;++i){
        assert(xge3dDecodeBudgetSetLimit(budget,64+(uint64_t)(i%2))==XGE_OK);
        xge3d_decode_budget_info_t value={.size=sizeof(value)};
        assert(xge3dDecodeBudgetInfo(budget,&value)==XGE_OK && value.limit==64+(uint64_t)(i%2));
    }
}
