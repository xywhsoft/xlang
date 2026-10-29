#define XRT_MODULE_ALL
#define XRUNTIME_MODULE_ALL
#include "lib/xruntime/xruntime_features.h"
#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)
#define XRT_IMPLEMENTATION
#endif
#include "lib/xrt/xrt.h"
#include "lib/xruntime/xruntime.h"
#include "src/api/xl_foreign_error.h"
#include "src/runtime/xl6_array_producer.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/runtime/xl6_string_type.h"
#include "src/runtime/xl6_list_algebra.h"
#include "src/runtime/xl6_set_methods.h"
#include "src/runtime/xl6_tree.h"
#include "src/runtime/xl6_slot_map.h"
#include "src/runtime/xl6_native_leaf_value.h"
#include "src/runtime/xl6_list_typed_read.h"
#include "src/runtime/xl6_list_value_read.h"
#include <assert.h>
#include <stdio.h>

static unsigned drops;
static int payload;
static void failedDrop(void* data,void* context)
{
    assert(data==&payload && !context);++drops;
    /* A real void callback has no return carrier. Its NULL failure must be
     * captured by the factory's local rollback, not the language caller. */
    xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
}
static const xvaluehandleops ops={NULL,failedDrop,NULL,NULL};
static const XL6_NativeHandlePolicyV1 policy={sizeof(policy),XL6_NATIVE_HANDLE_POLICY_V1_TAG,
    &ops,xl6NativeHandleTraceLeaf};
static void producerFailure(const xerror* error)
{
    XL6_ArrayProducerV1 producer={0};
    assert(xl6ErrorScopeBegin(&producer.transaction));
    xrtSetError(error);
    assert(!xl6ArrayProducerFinishV1(&producer,false));
    assert(xrtGetError()==error);
    xrtClearError(); /* Language catch consumes the ordinary failure. */
}
static void checkedFailures(void)
{
    xstrview source={"a\0你",5},target={0};
    assert(xrtMemDebugFailAfter(0));
    assert(!xrtTypeCopyValue(xl6StringTypeV1(),&target,&source));
    assert(xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    assert(!target.Data && !target.Size);xrtClearError();
    assert(!xl6ListValueCloneV1(NULL));
    xrtClearError();
    assert(!xl6SetValueCloneV1(NULL));
    xrtClearError();
    bool found=true;
    assert(!xl6TreeQueryV2(NULL,NULL,XL6_TREE_QUERY_FIRST_V2,NULL,false,
        (XL6_TreeQuerySlotV2){0},(XL6_TreeQuerySlotV2){0},&found));
    assert(found);xrtClearError();
    assert(!xl6SlotMapCreateTakeV1(NULL,NULL));
    xrtClearError();
    assert(!xl6NativeLeafValueCopyV1(NULL,NULL,NULL));
    xrtClearError();
    xstrview missing=xl6ListReadStringV1(NULL,0,0,0,0);
    assert(!missing.Data && !missing.Size);xrtClearError();
    assert(!xl6ListValueReadRequiredCopyV1(NULL,0));xrtClearError();
}
static void check(bool failedParent,const xerror* first,const xerror* later)
{
    xl6_error_scope parent={0};assert(xl6ErrorScopeBegin(&parent));
    if(failedParent) {
        xrtSetError(first);xl6ErrorScopeCaptureFailure(&parent);
        assert(parent.failed && parent.pending==first);
    }
    producerFailure(NULL);producerFailure(later);
    checkedFailures();
    assert(parent.failed==failedParent && parent.pending==(failedParent?first:NULL));
    void* slot=&payload;unsigned before=drops;
    assert(xrtMemDebugFailAfter(0));
    assert(!xl6NativeHandleTakeV1(&slot,&policy) && !slot);
    assert(xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    assert(drops==before+1);
    xrtClearError();
    assert(parent.failed==failedParent && parent.pending==(failedParent?first:NULL));
    xl6_error_result result=xl6ErrorScopeEnd(&parent);
    assert(result.ok==!failedParent && result.error==(failedParent?first:NULL));
    xrtErrorFree(result.error);assert(!xrtGetError());
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    xerror* first=xrtErrorCreate(XERR_STATE,"test",1,"outer first");
    xerror* later=xrtErrorCreate(XERR_MEMORY,"test",2,"local failure");assert(first && later);
    for(unsigned i=0;i<100;++i) {check(false,first,later);check(true,first,later);}
    /* Conversely, genuinely escaping callback failures remain sticky even if
     * native code consumes the slot. No global propagation suppression. */
    xl6_error_scope callback={0};assert(xl6ErrorScopeBegin(&callback));
    xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});xrtClearError();
    xl6_error_result result=xl6ErrorScopeEnd(&callback);assert(!result.ok && !result.error);
    xrtErrorFree(first);xrtErrorFree(later);assert(drops==200 && !xrtGetError());
    puts("checked return: 400 producer NULL/non-NULL failures, 200 real string-copy faults, 1400 checked argument refusals, 200 real handle rollbacks, clean/failed caller isolation and sticky escaping callback");
    return 0;
}
