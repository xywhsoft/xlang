#include "src/ir/xl6_ir.h"
#include "src/runtime/xl6_dynamic_collection.h"
#include "src/runtime/xl6_array_entry.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#define OWNER UINT64_C(1001)
#define ELEMENT UINT64_C(3001)
static unsigned dropped;
static void failNull(void* token,void* context)
{ assert(token && !context); ++dropped; xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL}); }
static void failArgument(void* token,void* context)
{ assert(token && !context); ++dropped; xrtSetErrorKind(XERR_ARGUMENT); xl6ForeignCallbackFail(); }
static const xvaluehandleops nullOps={.Drop=failNull},argumentOps={.Drop=failArgument};
static xvalue* handle(const xvaluehandleops* ops)
{ void* token=(void*)1; xvalue* item=xrtValueHandleTake(&token,ops,NULL); assert(item && !token); return item; }
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    /* The resident ABI and the canonical IR verifier agree on every arity,
     * including reserved/invalid operations and overflow argument counts. */
    for (unsigned op=0;op<=14;++op)
    for (size_t count=0;count<=4;++count)
        assert(xl6DynamicCollectionArityV1(op,count)==
            (bool)xl6IrDynamicCollectionOpOperandCountValid((XL6_IrDynamicCollectionOp)op,count));
    assert(!xl6DynamicCollectionArityV1(XL6_DYNAMIC_COLLECTION_PUSH,SIZE_MAX));
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
    /* contains normally returns false here. The consumed argument's Drop
     * reports a NULL-diagnostic failure, which MUST invalidate that result. */
    assert(!xl6DynamicCollectionCallTakeV1(xrtValueNull(),XL6_DYNAMIC_COLLECTION_CONTAINS,handle(&nullOps),NULL));
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope); assert(!outcome.ok && !outcome.error && dropped==1);
    assert(xl6ErrorScopeBegin(&scope));
    assert(!xl6DynamicCollectionCallTakeV1(xrtValueNull(),0,handle(&nullOps),handle(&argumentOps)));
    outcome=xl6ErrorScopeEnd(&scope);
    assert(!outcome.ok && xrtErrorKind(outcome.error)==XERR_ARGUMENT && dropped==3);
    xrtErrorFree(outcome.error); xrtClearError();
    /* A physical int64 slot with an int8 admission policy remains restricted
     * after erasure. A rejected push may not alter count or the live element. */
    xvalue* owner=xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,OWNER,ELEMENT,1,
        sizeof(int64_t),xrtTypeInt64(),XL6_VALUE_ADMIT_I8,false,ELEMENT); assert(owner);
    int64_t value=127;
    assert(xl6ArrayPushV1(owner,OWNER,ELEMENT,1,sizeof(value),xrtTypeInt64(),&value));
    owner=xl6ArrayProjectionTakeV1(owner,OWNER,ELEMENT,1,sizeof(value),xrtTypeInt64(),NULL); assert(owner);
    assert(!xl6DynamicCollectionCallTakeV1(owner,XL6_DYNAMIC_COLLECTION_PUSH,xrtValueInt(128),NULL));
    assert(xrtGetError()); xrtClearError(); assert(xl6ArrayErasedSizeV1(owner)==1);
    xvalue* result=xl6DynamicCollectionCallTakeV1(owner,XL6_DYNAMIC_COLLECTION_POP,NULL,NULL);
    assert(result && xrtValueGetInt(result,&value) && value==127 && xl6ArrayErasedSizeV1(owner)==0);
    xrtValueRelease(result); xrtValueRelease(owner);
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    puts("dynamic collection IR arities, NULL failure, all-argument cleanup and erased policy balanced");
    return 0;
}
