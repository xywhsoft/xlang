#define XRT_MODULE_ALL
#define XRUNTIME_MODULE_ALL
#include "lib/xruntime/xruntime_features.h"
#include "lib/xrt/xrt.h"
#include "lib/xruntime/xruntime.h"
#include "foreign_bound_view.h"
#include <assert.h>
#include <stdio.h>
static bound_view views[]={{10},{100}};
static int calls;
static xmemdebugsnapshot before;
static xmemdebugsnapshot balance;
bound_view* bound_view_at(int index){assert(index>=0 && index<2);return &views[index];}
bound_view* bound_view_child(int index){return bound_view_at(index);}
int64_t bound_view_add(bound_view* view,int64_t amount){assert(view);++calls;return view->value+amount;}
int64_t bound_view_mul(bound_view* view,int64_t amount){assert(view);++calls;return view->value*amount;}
void bound_view_store(bound_view* view,int64_t amount){assert(view);++calls;view->value=amount;}
void bound_view_check_begin(void){assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
int bound_view_check_unchanged(void)
{xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);return before.AllocCount==after.AllocCount && before.ReallocCount==after.ReallocCount;}
int bound_view_calls(void){return calls;}
void bound_view_binding_cost(void)
{xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);printf("bound creation: allocations=%llu liveBytes=%llu reallocations=%llu\n",
    (unsigned long long)(after.AllocCount-before.AllocCount),(unsigned long long)(after.LiveBytes-before.LiveBytes),
    (unsigned long long)(after.ReallocCount-before.ReallocCount));}
void bound_view_fail_after(int prefix){assert(xrtMemDebugFailAfter(prefix));}
void bound_view_mark_balance(void){xrtMemDebugSnapshot(&balance);}
int bound_view_balanced(void)
{xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);return after.LiveCount==balance.LiveCount && after.LiveBytes==balance.LiveBytes && after.InvalidFreeCount==balance.InvalidFreeCount && after.DoubleFreeCount==balance.DoubleFreeCount;}
