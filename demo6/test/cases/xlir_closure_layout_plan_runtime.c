#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_STRING
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_closure.h"
#include "src/runtime/xl6_cell.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Owner {
    XL6_CodeOwnerGraph descriptor;
    int refs;
    bool closed, cleared;
    const void* claim;
    XL6_Closure *closure, *retired;
} Owner;
typedef struct Environment { xvalue* value; XL6_Cell* cell; XL6_Closure* child; char* text; uint64_t number; } Environment;
static unsigned freed, faults, denied, transactions;
static const XL6_ClosureCaptureSlot slots[] = {
    {XL6_CLOSURE_CAPTURE_XVALUE, offsetof(Environment, value), sizeof(xvalue*)},
    {XL6_CLOSURE_CAPTURE_CELL, offsetof(Environment, cell), sizeof(XL6_Cell*)},
    {XL6_CLOSURE_CAPTURE_CLOSURE, offsetof(Environment, child), sizeof(XL6_Closure*)},
    {XL6_CLOSURE_CAPTURE_STRING, offsetof(Environment, text), sizeof(char*)},
    {XL6_CLOSURE_CAPTURE_TRIVIAL, offsetof(Environment, number), sizeof(uint64_t)}
};
static const XL6_ClosureEnvironmentV1 layout = {sizeof(layout), sizeof(Environment), 5, slots};
static void entry(void) {}
static int acquire(void* data) { Owner* o=data; if(o->closed)return 0; ++o->refs; return 1; }
static void release(void* data)
{
    Owner* o=data; assert(o->refs>0); if(--o->refs)return;
    assert(!o->closure && !o->retired); xrtFree(o); ++freed;
}
static bool count(const void* data,size_t* output) { const Owner* o=data; if(o->cleared)return false; *output=(size_t)o->refs; return true; }
static bool trace(const void* data,xrtownershipvisitor visit,void* context)
{ return visit(xl6ClosureRuntimeV1Get()->Ownership(((const Owner*)data)->closure),context); }
static const xrtownershipops owner_ops={count,trace};
static bool hold(const void* data) { ++((Owner*)data)->refs; return true; }
static void drop(const void* data) { release((void*)data); }
static bool claim(const void* data,const void* token)
{ Owner* o=(Owner*)data;if(o->claim && o->claim!=token)return false;o->claim=token;return true; }
static void restore(const void* data,const void* token)
{ Owner* o=(Owner*)data;assert(o->claim==token && !o->cleared);o->claim=NULL; }
static void clear(const void* data,const void* token)
{ Owner* o=(Owner*)data;assert(o->claim==token && !o->cleared);o->retired=o->closure;o->closure=NULL;o->cleared=true; }
static bool finish_owner(const void* data,const void* token)
{
    Owner* o=(Owner*)data; XL6_Closure* closure=o->retired;
    assert(o->claim==token && o->cleared);o->retired=NULL;xl6ClosureRuntimeV1Get()->Release(closure);return true;
}
static const XL6_OwnershipAdapterV1 owner_adapter={sizeof(owner_adapter),hold,drop,claim,restore,NULL,clear,finish_owner};
static const XL6_OwnershipAdapterV1* resolve(xrtownershipref ref,void* unused)
{
    const XL6_OwnershipAdapterV1* adapter;(void)unused;
    if(ref.Ops==&owner_ops)return &owner_adapter;
    if((adapter=xl6ClosureOwnershipAdapterV1(ref)))return adapter;
    if((adapter=xl6CellOwnershipAdapterV1(ref)))return adapter;
    if((adapter=xl6CodeLeaseOwnershipAdapterV1(ref)))return adapter;
    return xrtValueOwnershipAdapterV1(ref);
}
static Owner* make(xvalue** weak)
{
    Owner* o=xrtCalloc(1,sizeof(*o));Environment* env=xrtCalloc(1,sizeof(*env));xvalue* item;
    assert(o && env);o->refs=1;o->descriptor=(XL6_CodeOwnerGraph){{sizeof(o->descriptor),o,acquire,release},0x584C36434F574E31ULL,&owner_ops};
    env->value=xrtValueArray();item=xrtValueInt(42);assert(env->value && item && xrtValueArrayAppendNew(env->value,item));
    *weak=xrtValueWeakRef(env->value);assert(*weak);
    env->cell=xl6CellRuntimeV1Get()->Create(XL6_CELL_XVALUE,sizeof(xvalue*));assert(env->cell);
    item=xrtValueRetain(env->value);assert(item);xl6CellRuntimeV1Get()->Initialize(env->cell,&item,sizeof(item));
    env->child=xl6ClosureRuntimeV2Get()->Create(&o->descriptor.base,entry,NULL,NULL,NULL,NULL,NULL,0);assert(env->child);
    env->text=xrtStrDup("layout");env->number=42;assert(env->text);
    o->closure=xl6ClosureRuntimeV2Get()->Create(&o->descriptor.base,entry,NULL,env,&layout,NULL,NULL,0);assert(o->closure);
    o->closed=true;release(o);assert(o->refs==2);return o;
}
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
}
static void close_plan(XL6_OwnershipPlan* plan)
{ assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY); }
static void transaction(unsigned mode)
{
    xmemdebugsnapshot before;xvalue* weak=NULL;XL6_OwnershipPlan* plan=NULL;XL6_Closure* external=NULL;
    xrtMemDebugSnapshot(&before);Owner* o=make(&weak);Environment* env=o->closure->environment;
    if(mode==1)external=xl6ClosureRuntimeV1Get()->Retain(env->child);
    XL6_OwnershipPlanResult result=xl6OwnershipPlanTryCreate((xrtownershipref){o,&owner_ops},resolve,NULL,&plan);
    assert(result==(mode==1?XL6_OWNERSHIP_PLAN_ROOTED:XL6_OWNERSHIP_PLAN_READY));
    if(mode==1 || mode==2) {
        if(mode==2){
            external=xl6ClosureRuntimeV1Get()->Retain(env->child);
            assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_ROOTED);
        }
        close_plan(plan);plan=NULL;assert(!o->claim && o->closure->environment==env);
        xl6ClosureRuntimeV1Get()->Release(external);
        assert(xl6OwnershipPlanTryCreate((xrtownershipref){o,&owner_ops},resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_READY);
    }
    if(mode==3){
        xvalue* replacement=xrtValueArray();xvalue* old=NULL;assert(replacement);
        assert(xrtValueArrayAppendNew(replacement,xrtValueInt(43)));
        xl6CellRuntimeV1Get()->Exchange(env->cell,&replacement,&old,sizeof(replacement));xrtValueRelease(old);
    }
    xvalue* denied_value=xrtValueWeakRefLock(weak);assert(denied_value && xrtValueType(denied_value)==XVALUE_NULL);
    xrtValueRelease(denied_value);++denied;
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY && xl6OwnershipPlanFinish(plan));
    close_plan(plan);xrtValueRelease(weak);balance(&before);++transactions;
}
static void plan_fault(unsigned point,bool commit)
{
    xmemdebugsnapshot before;xvalue* weak=NULL;XL6_OwnershipPlan* plan=NULL;XL6_OwnershipPlanResult result;
    xrtMemDebugSnapshot(&before);Owner* o=make(&weak);
    if(commit)assert(xl6OwnershipPlanTryCreate((xrtownershipref){o,&owner_ops},resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xrtMemDebugFailAfter(point));
    result=commit?xl6OwnershipPlanTryCommit(plan):xl6OwnershipPlanTryCreate((xrtownershipref){o,&owner_ops},resolve,NULL,&plan);
    bool failed=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xrtClearError();
    if(failed){++faults;assert(result==XL6_OWNERSHIP_PLAN_ERROR && !o->cleared);if(plan)close_plan(plan);plan=NULL;}
    if(!plan)assert(xl6OwnershipPlanTryCreate((xrtownershipref){o,&owner_ops},resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_READY);
    if(failed || !commit)assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanFinish(plan));close_plan(plan);xrtValueRelease(weak);balance(&before);
}
static bool poison_trace(const void* env,XL6_ModuleOwnershipVisit visit,void* data)
{ (void)env;(void)visit;(void)data;abort(); }
static void opaque_drop(void* env) { ++*(unsigned*)env; }
static void invalid_layouts(void)
{
    Environment env={0};XL6_ClosureCaptureSlot bad[2]={{XL6_CLOSURE_CAPTURE_XVALUE,0,sizeof(void*)},{XL6_CLOSURE_CAPTURE_XVALUE,0,sizeof(void*)}};
    XL6_ClosureEnvironmentV1 invalid={sizeof(invalid),sizeof(env),2,bad};unsigned disposed=0;
    XL6_Closure* opaque=xl6ClosureRuntimeV1Get()->Create(NULL,entry,NULL,&disposed,opaque_drop,NULL,NULL,0,poison_trace);
    XL6_OwnershipPlan* plan=NULL;assert(opaque);
    assert(xl6OwnershipPlanTryCreate(xl6ClosureRuntimeV1Get()->Ownership(opaque),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ERROR && !plan);
    xl6ClosureRuntimeV1Get()->Release(opaque);assert(disposed==1);xrtClearError();
    for(unsigned mode=0;mode<5;++mode){
        if(mode==1)invalid.size=sizeof(invalid)-1;
        if(mode==2){invalid.size=sizeof(invalid);invalid.count=1;bad[0].offset=SIZE_MAX;}
        if(mode==3){bad[0].offset=0;bad[0].bytes=1;}
        if(mode==4){bad[0].bytes=sizeof(void*);bad[0].kind=(XL6_ClosureCaptureKind)99;}
        assert(!xl6ClosureRuntimeV2Get()->Create(NULL,entry,NULL,&env,&invalid,NULL,NULL,0));
        assert(xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
    }
}
static void value_bridge_faults(void)
{
    unsigned failures=0;
    for(unsigned round=0;round<100;++round)for(unsigned point=0;point<5;++point){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        xvalue* value=xrtValueInt(42);assert(value);xrtSetErrorKind(XERR_RANGE);
        const xerror* prior=xrtGetError();assert(prior && xrtMemDebugFailAfter(point));
        XL6_Closure* closure=xl6ClosureRuntimeV2Get()->CreateValue(NULL,entry,value);
        bool failed=xrtMemDebugFailTriggered();xrtMemDebugFailClear();assert(xrtGetError()==prior);
        if(failed){++failures;assert(!closure);}
        else{
            assert(closure && *(xvalue**)closure->environment==value &&
                xl6ClosureOwnershipAdapterV1(xl6ClosureRuntimeV1Get()->Ownership(closure)));
            xl6ClosureRuntimeV1Get()->Release(closure);
        }
        int64_t actual=0;assert(xrtValueGetInt(value,&actual) && actual==42);
        xrtValueRelease(value);balance(&before);
    }
    assert(failures==200);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    for(unsigned round=0;round<100;++round)for(unsigned mode=0;mode<4;++mode)transaction(mode);
    for(unsigned point=0;point<48;++point){plan_fault(point,false);plan_fault(point,true);}
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);invalid_layouts();balance(&before);value_bridge_faults();
    assert(transactions==400 && denied==400 && freed==496 && faults>0);
    printf("Resident layout plans: 400 closure/cell/value/code-owner cycles, 400 quarantined promotions, 96 creation/commit probes, %u actual failures, 496 physical owners freed; 500 value bridges with 200 faults; invalid and legacy callbacks refused; zero live delta\n",faults);
    return 0;
}
