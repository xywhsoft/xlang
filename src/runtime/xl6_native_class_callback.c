#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_native_class_callback.h"
#include "xl6_native_class_borrow.h"
#include "src/api/xl_foreign_error.h"
#include <string.h>

typedef struct ClassCallback {
    XL6_Closure* target;
    xstrview argumentName,resultName;
    XL6_NativeClassCallbackContextV1 context;
} ClassCallback;
static const XL6_ClosureCaptureSlot slots[]={
    {XL6_CLOSURE_CAPTURE_CLOSURE,offsetof(ClassCallback,target),sizeof(XL6_Closure*)},
    {XL6_CLOSURE_CAPTURE_STRING,offsetof(ClassCallback,argumentName),sizeof(xstrview)},
    {XL6_CLOSURE_CAPTURE_STRING,offsetof(ClassCallback,resultName),sizeof(xstrview)},
    {XL6_CLOSURE_CAPTURE_TRIVIAL,offsetof(ClassCallback,context),sizeof(XL6_NativeClassCallbackContextV1)}};
static const XL6_ClosureEnvironmentV1 layout={sizeof(layout),sizeof(ClassCallback),4,slots};
static const char* const names[]={"argument"};
static bool forward(void* raw,xvalue** result,xvalue* argument)
{
    ClassCallback* self=raw;XL6_ClosureScope scope={0};
    xl6ClosureRuntimeV1Get()->Enter(&scope,self->target);
    bool ok=((bool(*)(void*,xvalue**,xvalue*))self->target->invoke)(self->target->environment,result,argument);
    xl6ClosureRuntimeV1Get()->Leave(&scope);return ok;
}
static bool predicateForward(void* raw,bool* result,xvalue* argument)
{
    ClassCallback* self=raw;XL6_ClosureScope scope={0};
    xl6ClosureRuntimeV1Get()->Enter(&scope,self->target);
    bool ok=((bool(*)(void*,bool*,xvalue*))self->target->invoke)(self->target->environment,result,argument);
    xl6ClosureRuntimeV1Get()->Leave(&scope);return ok;
}
static bool validField(XL6_NativeClassFieldContextV1 field)
{
    const XL6_NativeHandlePolicyV1* p=field.policy;
    return field.fieldName&&field.fieldSize&&field.fieldSize<=INT64_MAX&&
        (field.fieldSize<6||memcmp(field.fieldName,"$xlir.",6))&&
        p&&p->size==sizeof(*p)&&p->abiTag==XL6_NATIVE_HANDLE_POLICY_V1_TAG&&p->Ops&&p->Trace&&
        p->Ops->Drop&&!p->Ops->Clone&&!p->Ops->Hash&&!p->Ops->Equal;
}
static bool copyName(xstrview* output,const char* input,size_t size)
{
    char* owned=xrtMalloc(size);if(!owned)return false;
    memcpy(owned,input,size);*output=(xstrview){owned,size};return true;
}
static XL6_Closure* create(XL6_NativeClassCallbackContextV1 context,XL6_Closure* target,bool predicate)
{
    if(xrtGetError())return NULL;
    /* NativeClassCallbackPlan seals the unary typed invoke ABI. The optional
     * call_parameter_count describes keyword/default metadata, not arity:
     * typed adapters over erased bound methods deliberately have none. The
     * supplied entry must remain the compiler's checked typed adapter, never
     * an erased callable entry. Its actual coordinated owner is retained. */
    if((!predicate&&(!context.resultIdentity||!validField(context.result)))||!target||!target->invoke||
        !xl6ClosureOwnershipAdapterV1(xl6ClosureRuntimeV1Get()->Ownership(target))){
        xrtSetErrorKind(XERR_TYPE);return NULL;
    }
    if(!xl6NativeClassResultContextValidV1(context.argument))return NULL;
    ClassCallback* self=xrtCalloc(1,sizeof(*self));if(!self)return NULL;
    self->target=xl6ClosureRuntimeV1Get()->Retain(target);self->context=context;
    /* Retaining the V2 closure, not this borrowed CodeOwner pointer, protects
     * the actual argument schema. Names are copied independently below. */
    self->context.argument.codeOwner=NULL;
    XL6_Closure* result=NULL;
    if(copyName(&self->argumentName,context.argument.field.fieldName,context.argument.field.fieldSize)&&
        (predicate||copyName(&self->resultName,context.result.fieldName,context.result.fieldSize))){
        self->context.argument.field.fieldName=self->argumentName.Data;
        self->context.result.fieldName=self->resultName.Data;
        result=xl6ClosureRuntimeV2Get()->Create(context.argument.codeOwner,
            predicate?(void(*)(void))predicateForward:(void(*)(void))forward,NULL,self,&layout,names,NULL,1);
    }
    if(!result){
        xerror* primary=xrtTakeError();xl6ClosureRuntimeV2Get()->DropEnvironment(&layout,self);
        xrtClearError();xrtSetErrorTake(primary);
    }
    return result;
}
XL6_Closure* xl6NativeClassCallbackCreateV1(XL6_NativeClassCallbackContextV1 context,XL6_Closure* target)
{return create(context,target,false);}
XL6_Closure* xl6NativeClassPredicateCreateV1(XL6_NativeClassResultContextV1 context,XL6_Closure* target)
{return create((XL6_NativeClassCallbackContextV1){.argument=context},target,true);}
static bool invoke(XL6_Closure* driver,void** argument,xvalue** output,bool* predicate)
{
    if(!driver||driver->invoke!=(predicate?(void(*)(void))predicateForward:(void(*)(void))forward)||
        !driver->environment||!argument||!*argument||(!predicate&&(!output||*output))){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    ClassCallback* self=driver->environment;void* raw=*argument;*argument=NULL;
    if(xrtGetError()){self->context.argument.field.policy->Ops->Drop(raw,NULL);return false;}
    XL6_ClosureScope code={0};xl6ClosureRuntimeV1Get()->Enter(&code,driver);
    xl6_error_scope errors={0};bool begun=xl6ErrorScopeBegin(&errors);
    xvalue *prepared=NULL,*input=NULL,*returned=NULL,*handle=NULL;bool ok=false,decision=false;
    if(begun){
        prepared=xl6NativeClassResultPrepareFromLeaseV1(self->context.argument,driver->lease);
        if(prepared)input=xl6NativeClassResultCommitTakeV1(self->context.argument,&prepared,&raw);
        if(input)ok=predicate?predicateForward(self,&decision,input):forward(self,&returned,input);
        if(!predicate&&ok&&returned&&xrtValueType(returned)!=XVALUE_NULL){
            if(xrtValueTypeId(returned)!=self->context.resultIdentity){xrtSetErrorKind(XERR_TYPE);ok=false;}
            else{
                void* native=NULL;
                handle=xl6NativeClassFieldRetainV1(self->context.result,returned,self->context.result.policy,&native);
                ok=handle!=NULL;
            }
        }
        if(!ok)xl6ErrorScopeCaptureFailure(&errors);
        xrtValueRelease(returned);xrtValueRelease(input);xrtValueRelease(prepared);
        if(raw)self->context.argument.field.policy->Ops->Drop(raw,NULL);
    }else if(raw)self->context.argument.field.policy->Ops->Drop(raw,NULL);
    xl6_error_result outcome=begun?xl6ErrorScopeEnd(&errors):(xl6_error_result){false,xrtTakeError()};
    if(!outcome.ok){
        xrtSetErrorTake(outcome.error);xl6_error_scope cleanup={0};
        if(!xl6ErrorScopeBeginCleanup(&cleanup,true))abort();
        xrtValueRelease(handle);outcome=xl6ErrorScopeEnd(&cleanup);handle=NULL;
    }
    xl6ClosureRuntimeV1Get()->Leave(&code);
    if(!outcome.ok){xrtSetErrorTake(outcome.error);return false;}
    xrtErrorFree(outcome.error);if(predicate)*predicate=decision;else *output=handle;return true;
}
bool xl6NativeClassCallbackInvokeTakeV1(XL6_Closure* driver,void** argument,xvalue** output)
{return invoke(driver,argument,output,NULL);}
bool xl6NativeClassPredicateInvokeTakeV1(XL6_Closure* driver,void** argument,bool* output)
{
    if(!output){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    return invoke(driver,argument,NULL,output);
}
