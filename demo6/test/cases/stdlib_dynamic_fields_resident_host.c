#include "src/runtime/xl6_dynamic_fields.h"
#include "src/runtime/xl6_ownership_plan.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>

static size_t edges, drops;
static bool edge(xrtownershipref ref, void* context)
{
    assert(!context && ref.Data && ref.Ops);
    ++edges; return true;
}
static const XL6_OwnershipAdapterV1* resolve(xrtownershipref ref, void* context)
{
    const XL6_NativeHandlePolicyV1* policy=xl6DynamicFieldsHandlePolicyV1Get();
    const XL6_OwnershipAdapterV1* adapter;
    (void)context;
    if ((adapter=xrtDynamicFieldsOwnershipAdapterV1(ref))) return adapter;
    if ((adapter=xrtValueHandleOwnershipAdapterV1(ref,policy->Ops,policy->Trace))) return adapter;
    return xrtValueOwnershipAdapterV1(ref);
}
static void silentDrop(void* raw, void* user)
{
    (void)raw; assert(!user); ++drops;
    /* Explicit language failure without an allocated diagnostic. */
    xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
}
static bool leaf(const xvalue* value,xrtownershipvisitor visit,void* context)
{ (void)value; (void)context; return visit!=NULL; }
static void physicalGraph(void)
{
    void* raw=xl6_xrt_dynamic_fields_create(); assert(raw);
    xvalue* boxed=xl6NativeHandleTakeV1(&raw,xl6DynamicFieldsHandlePolicyV1Get());
    assert(boxed && !raw);
    void* fields=NULL; assert(xrtValueGetHandle(boxed,&fields,NULL,NULL));
    xvalue* array=xrtValueArray(); assert(array);
    assert(xrtValueArrayAppendNew(array,xrtValueInt(42)));
    assert(xl6_xrt_dynamic_fields_set(fields,XRT_STR_LITERAL("a"),array));
    assert(xl6_xrt_dynamic_fields_set(fields,XRT_STR_LITERAL("a\0你"),array));
    xrtValueRelease(array);
    xrtownershipscope freeze={0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
    edges=0; assert(xl6DynamicFieldsHandlePolicyV1Get()->Trace(boxed,edge,NULL) && edges==1);
    xrtownershipref child=xrtDynamicFieldsOwnership(fields);
    size_t count=0; assert(child.Ops->Count(child.Data,&count) && count==1);
    edges=0; assert(child.Ops->Trace(child.Data,edge,NULL) && edges==2);
    xrtownershipref root=xrtValueOwnership(boxed);
    xrtownershipresult result={0};
    assert(xrtOwnershipInspect(&root,1,NULL,0,&result));
    assert(result.NodeCount==5 && result.EdgeCount==5 && result.ExternalRootCount==1);
    assert(xrtOwnershipScopeEnd(&freeze));
    xvalue* absent=xl6_xrt_dynamic_fields_get(fields,XRT_STR_LITERAL("absent"));
    assert(absent && xrtValueType(absent)==XVALUE_NULL); xrtValueRelease(absent);
    xerror* prior=xrtErrorCreate(XERR_STATE,"test",17,"prior"); assert(prior);
    xrtSetErrorTake(prior);
    absent=xl6_xrt_dynamic_fields_copy(fields,XRT_STR_LITERAL("absent"));
    assert(absent && xrtValueType(absent)==XVALUE_NULL && xrtGetError()==prior);
    xrtValueRelease(absent); xrtClearError();
    assert(!xl6_xrt_dynamic_fields_reserve(fields,-1)); xrtClearError();
    xrtValueRelease(boxed); assert(!xrtGetError());
}
static void cyclePlan(void)
{
    void* raw=xl6_xrt_dynamic_fields_create(); assert(raw);
    xvalue* boxed=xl6NativeHandleTakeV1(&raw,xl6DynamicFieldsHandlePolicyV1Get());
    void* fields=NULL; assert(boxed && xrtValueGetHandle(boxed,&fields,NULL,NULL));
    assert(xl6_xrt_dynamic_fields_set(fields,XRT_STR_LITERAL("self"),boxed));
    assert(xl6_xrt_dynamic_fields_set(fields,XRT_STR_LITERAL("again"),boxed));
    xrtownershipref root=xrtValueOwnership(boxed);
    XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreate(root,resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ROOTED && plan);
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    xrtValueRelease(boxed); /* two real internal slots now keep the cycle alive */
    plan=NULL;
    assert(xl6OwnershipPlanTryCreate(root,resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_READY && plan);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanFinish(plan));
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(!xrtGetError());
}
static void cleanupFailure(void)
{
    static const xvaluehandleops foreign={NULL,silentDrop,NULL,NULL};
    void* fields=xl6_xrt_dynamic_fields_create(); assert(fields);
    void* raw=(void*)1;
    xvalue* value=xrtValueHandleTake(&raw,&foreign,NULL);
    assert(value && xrtValueHandleOwnershipBindPhased(value,leaf));
    assert(xl6_xrt_dynamic_fields_set(fields,XRT_STR_LITERAL("failure"),value));
    xrtValueRelease(value);
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
    xl6_xrt_dynamic_fields_destroy(fields);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(!outcome.ok && !outcome.error && drops==1);
    xrtClearError();
    /* Native bool Finish is not the language outcome. This is the SAME
     * surrounding checked boundary used by the host's ownership driver. */
    fields=xl6_xrt_dynamic_fields_create(); raw=(void*)1;
    value=xrtValueHandleTake(&raw,&foreign,NULL); assert(value);
    assert(xrtValueHandleOwnershipBindPhased(value,leaf));
    assert(xl6_xrt_dynamic_fields_set(fields,XRT_STR_LITERAL("failure"),value));
    xrtValueRelease(value);
    xrtownershipscope freeze={0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
    const xrtownershipadapterv1* adapter=xrtDynamicFieldsOwnershipAdapterV1(xrtDynamicFieldsOwnership(fields));
    int token=0; assert(adapter && adapter->Hold(fields) && adapter->Claim(fields,&token));
    adapter->Clear(fields,&token); assert(xrtOwnershipScopeEnd(&freeze));
    assert(xl6ErrorScopeBegin(&scope));
    assert(adapter->Finish(fields,&token));
    outcome=xl6ErrorScopeEnd(&scope); assert(!outcome.ok && !outcome.error && drops==2);
    assert(adapter->Finish(fields,&token) && drops==2);
    adapter->Drop(fields); xl6_xrt_dynamic_fields_destroy(fields);
    assert(!xrtGetError());
}
static void copyPrefixes(void)
{
    void* source=xl6_xrt_dynamic_fields_create(); assert(source);
    xvalue* array=xrtValueArray(); assert(array);
    assert(xrtValueArrayAppendNew(array,xrtValueInt(7)));
    assert(xl6_xrt_dynamic_fields_set(source,XRT_STR_LITERAL("a\0b"),array));
    xrtValueRelease(array);
    xvalue* wire=xl6_xrt_dynamic_fields_to_value(source); assert(wire);
    size_t total=0;
    for (size_t mode=0;mode<6;++mode) {
        size_t prefix;
        for (prefix=0;prefix<1000;++prefix) {
            xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
            xl6ProductMemoryBegin(prefix);
            xvalue* output=NULL; void* fields=NULL; bool ok=false;
            switch (mode) {
            case 0: output=xl6_xrt_dynamic_fields_copy(source,XRT_STR_LITERAL("a\0b")); ok=output!=NULL; break;
            case 1: output=xl6_xrt_dynamic_fields_values(source); ok=output!=NULL; break;
            case 2: output=xl6_xrt_dynamic_fields_items(source); ok=output!=NULL; break;
            case 3: fields=xl6_xrt_dynamic_fields_from_value(wire); ok=fields!=NULL; break;
            case 4: fields=xl6_xrt_dynamic_fields_clone(source); ok=fields!=NULL; break;
            case 5: fields=xl6_xrt_dynamic_fields_create(); ok=fields && xl6_xrt_dynamic_fields_merge(fields,source,true); break;
            }
            bool hit=xl6ProductMemoryEnd();
            xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
            assert(hit ? !ok && !outcome.ok : ok && outcome.ok);
            if (hit) assert(outcome.error && xrtErrorKind(outcome.error)==XERR_MEMORY);
            xrtValueRelease(output); xl6_xrt_dynamic_fields_destroy(fields);
            xrtErrorFree(outcome.error); xrtClearError();
            xl6ProductMemoryVerify();
            assert(xrtValueCount(xrtDynamicFieldsGet(source,XRT_STR_LITERAL("a\0b")))==1);
            if (!hit) break;
        }
        assert(prefix<1000); total+=prefix+1;
    }
    xrtValueRelease(wire); xl6_xrt_dynamic_fields_destroy(source);
    printf("%zu checked copy/factory/merge OOM prefixes balanced\n",total);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    xl6_error_scope warm={0}; assert(xl6ErrorScopeBegin(&warm));
    xl6_error_result ready=xl6ErrorScopeEnd(&warm); assert(ready.ok && !ready.error);
    xl6ProductMemoryBegin(SIZE_MAX);
    physicalGraph(); cyclePlan(); cleanupFailure();
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    size_t prefix=0;
    for (;prefix<1000;++prefix) {
        xl6ProductMemoryBegin(prefix);
        void* raw=xl6_xrt_dynamic_fields_create();
        xvalue* boxed=raw ? xl6NativeHandleTakeV1(&raw,xl6DynamicFieldsHandlePolicyV1Get()) : NULL;
        bool hit=xl6ProductMemoryEnd();
        assert(!raw && (hit ? !boxed : boxed!=NULL));
        xerror* failure=xrtTakeError();
        xrtValueRelease(boxed); xrtErrorFree(failure); xrtClearError();
        xl6ProductMemoryVerify();
        if (!hit) break;
    }
    assert(prefix<1000);
    copyPrefixes();
    printf("resident DynamicFields physical graph, cycle, diagnosticless cleanup and %zu factory OOM prefixes balanced\n",prefix+1);
    return 0;
}
