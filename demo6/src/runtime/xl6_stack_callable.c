#define XRUNTIME_MODULE_RUNTIME_CALL
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#include "src/runtime/xl6_stack_callable.h"
#include "src/runtime/xl6_sequence.h"
#include "src/api/xl_foreign_error.h"

static bool overlaps(const void* output,size_t bytes,const xarray* storage)
{
    uintptr_t target=(uintptr_t)output,base=(uintptr_t)storage->Data;
    size_t span=storage->Capacity*sizeof(xvalue*);
    return storage->Data && ((target>=base && target-base<span) || (base>=target && base-target<bytes));
}
bool xl6StackCallableTakeV1(const xvalue* value,uint64_t identity,uint64_t element,uint64_t signature,
    bool nullable,int64_t depth,bool pop,const XL6_CodeOwner* owner,void (*entry)(void),
    XL6_Closure** output,bool* found)
{
    if(!output || (uintptr_t)output%_Alignof(XL6_Closure*) || !entry || !signature ||
        (nullable ? element==signature : element!=signature) || (pop && depth!=0)) {
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    if(depth<0 || (uint64_t)depth>SIZE_MAX){xrtSetErrorKind(XERR_RANGE);return false;}
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope))return false;
    XL6_Sequence* stack=xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_STACK_V1,identity,element,3,sizeof(xvalue*),xrtTypeValue());
    XL6_Closure* acquired=NULL;xvalue* moved=NULL;bool ok=false,present=false;
    if(!stack)goto done;
    xtypedstack* storage=xl6SequenceStorageV1(stack);
    if(storage->Storage.Capacity>SIZE_MAX/sizeof(xvalue*) || overlaps(output,sizeof(*output),&storage->Storage) ||
        (found && (overlaps(found,sizeof(*found),&storage->Storage) ||
         ((uintptr_t)found>=(uintptr_t)output && (uintptr_t)found-(uintptr_t)output<sizeof(*output)))) || *output) {
        xrtSetErrorKind(XERR_ARGUMENT);goto done;
    }
    if(found)*found=false;
    if((size_t)depth>=xrtTypedStackCount(storage)) {
        if(found)ok=true;else xrtSetErrorKind(XERR_RANGE);
        goto done;
    }
    const xvalue* const* slot=xrtTypedStackConstPeek(storage,(size_t)depth);
    if(!slot || !xl6SequenceAdmitSlotV1(stack,slot)){if(!xrtGetError())xrtSetErrorKind(XERR_ARGUMENT);goto done;}
    const xvalue* source=*slot;
    if(!source || xrtValueType(source)==XVALUE_NULL) {
        if(!nullable){xrtSetErrorKind(XERR_ARGUMENT);goto done;}
    } else {
        if(xrtValueTypeId(source)!=signature || !xrtValueIsCallable(source)){xrtSetErrorKind(XERR_ARGUMENT);goto done;}
        acquired=xl6ClosureRuntimeV2Get()->CreateValue(owner,entry,source);
        if(!acquired){if(!xrtGetError())xrtSetErrorKind(XERR_MEMORY);goto done;}
    }
    /* No fallible projection remains after this move. Drop the detached box
     * outside the Stack pin; the projected closure already owns that box. */
    if(xrtGetError())goto done;
    if(pop && !xrtTypedStackPop(storage,&moved)){if(!xrtGetError())xrtSetErrorKind(XERR_STATE);goto done;}
    present=true;ok=true;
done:
    if(!ok)xl6ErrorScopeCaptureFailure(&scope);
    if(stack)xl6SequenceEndV1(stack);
    xrtValueRelease(moved);
    if(!ok){xl6ClosureRuntimeV1Get()->Release(acquired);acquired=NULL;}
    xl6_error_result result=xl6ErrorScopeEnd(&scope);
    if(!result.ok){xl6ClosureRuntimeV1Get()->Release(acquired);xl6ErrorScopePropagateFailure(result);return false;}
    if(!ok)return false;
    *output=acquired;if(found)*found=present;return true;
}
