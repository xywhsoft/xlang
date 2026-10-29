#include "xl6_text_value.h"
#include "xl6_future_value.h"
#include "xl6_native_future_payload.h"
#include "xl6_closure.h"
#include "xl6_generator.h"
#include "src/api/xl_foreign_error.h"
#include <string.h>
#include <stdlib.h>

bool xl6FutureValueProfileValidV1(XL6_FutureValueProfileV1 p)
{
    if(p.size!=sizeof(p) || !p.type)return false;
    if(p.kind==XL6_FUTURE_VALUE_NATIVE)
        return xl6NativeCaptureProfileValidV1(p.native) && p.valueSize==p.native->Type()->Size;
    if(p.native)return false;
    switch(p.kind) {
    case XL6_FUTURE_VALUE_VOID:return p.valueSize==0;
    case XL6_FUTURE_VALUE_TRIVIAL:return p.valueSize>0;
    case XL6_FUTURE_VALUE_STRING:return p.valueSize==sizeof(xstrview);
    case XL6_FUTURE_VALUE_XVALUE:return p.valueSize==sizeof(xvalue*);
    case XL6_FUTURE_VALUE_ERROR:return p.valueSize==sizeof(xerror*);
    case XL6_FUTURE_VALUE_CLOSURE:return p.valueSize==sizeof(XL6_Closure*);
    case XL6_FUTURE_VALUE_FUTURE:return p.valueSize==sizeof(xfuture*);
    case XL6_FUTURE_VALUE_GENERATOR:return p.valueSize==sizeof(XL6_Generator*);
    default:return false;
    }
}
static XL6_FuturePayloadKind policyKind(XL6_FutureValueKindV1 kind)
{
    switch(kind) {
    case XL6_FUTURE_VALUE_TRIVIAL:return XL6_FUTURE_PAYLOAD_VALUE;
    case XL6_FUTURE_VALUE_STRING:return XL6_FUTURE_PAYLOAD_STRING;
    case XL6_FUTURE_VALUE_XVALUE:return XL6_FUTURE_PAYLOAD_XVALUE;
    case XL6_FUTURE_VALUE_ERROR:return XL6_FUTURE_PAYLOAD_ERROR;
    case XL6_FUTURE_VALUE_CLOSURE:return XL6_FUTURE_PAYLOAD_CLOSURE;
    case XL6_FUTURE_VALUE_FUTURE:return XL6_FUTURE_PAYLOAD_FUTURE;
    case XL6_FUTURE_VALUE_GENERATOR:return XL6_FUTURE_PAYLOAD_GENERATOR;
    default:return XL6_FUTURE_PAYLOAD_INVALID;
    }
}
static void* allocateCell(XL6_FutureValueProfileV1 profile)
{
    return profile.kind==XL6_FUTURE_VALUE_NATIVE?
        xl6NativeFuturePayloadRuntimeV1Get()->Allocate(profile.native):xrtCalloc(1,profile.valueSize);
}
const xfuturepayloadownershipv1* xl6FutureValuePolicyV1(XL6_FutureValueProfileV1 profile)
{
    if(!xl6FutureValueProfileValidV1(profile))return NULL;
    if(profile.kind==XL6_FUTURE_VALUE_NATIVE)return xl6NativeFuturePayloadRuntimeV1Get()->Policy();
    return xl6PayloadPolicyGet(profile.kind==XL6_FUTURE_VALUE_VOID?XL6_FUTURE_PAYLOAD_VALUE:policyKind(profile.kind));
}
bool xl6FutureValueAllocateV1(XL6_FutureValueProfileV1 profile,void** output)
{
    if(!output||*output||!xl6FutureValueProfileValidV1(profile)){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(profile.kind==XL6_FUTURE_VALUE_VOID)return true;
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope))return false;
    void* value=allocateCell(profile);
    if(!value||scope.failed||xrtGetError()){
        xl6ErrorScopeCaptureFailure(&scope);xl6FutureValueDropV1(profile,value);value=NULL;
    }
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok){xrtSetErrorTake(outcome.error);return false;}
    xrtErrorFree(outcome.error);*output=value;return true;
}
void xl6FutureValueDropV1(XL6_FutureValueProfileV1 profile,void* value)
{
    if(!value)return;
    if(!xl6FutureValueProfileValidV1(profile))abort();
    if(profile.kind==XL6_FUTURE_VALUE_NATIVE)xl6NativeFuturePayloadRuntimeV1Get()->Drop(value,NULL);
    else {
        XL6_FuturePayloadDrop drop=xl6PayloadDropGet(policyKind(profile.kind));
        if(!drop)abort();
        drop(value,NULL);
    }
}
bool xl6FutureValueCopyV1(XL6_FutureValueProfileV1 profile,const void* source,void** output)
{
    if(!output || *output || !xl6FutureValueProfileValidV1(profile) ||
       (profile.kind!=XL6_FUTURE_VALUE_VOID && !source)) {
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    if(profile.kind==XL6_FUTURE_VALUE_VOID)return true;
    xl6_error_scope scope={0};
    if(!xl6ErrorScopeBegin(&scope))return false;
    void* value=allocateCell(profile);
    bool ok=value!=NULL;
    if(value)switch(profile.kind) {
    case XL6_FUTURE_VALUE_TRIVIAL:memcpy(value,source,profile.valueSize);break;
    case XL6_FUTURE_VALUE_STRING: {
        xstrview input=*(const xstrview*)source;
        if(!input.Data && input.Size) {xrtSetErrorKind(XERR_ARGUMENT);ok=false;break;}
        if(input.Data) {
            char* copy=xrtStrDupN(input.Data,input.Size);
            ok=copy!=NULL;
            if(ok)*(xstrview*)value=(xstrview){copy,input.Size};
        }
        break;
    }
    case XL6_FUTURE_VALUE_XVALUE: {
        xvalue* input=*(xvalue* const*)source;
        *(xvalue**)value=xrtValueRetain(input);ok=*(xvalue**)value!=NULL || !input;break;
    }
    case XL6_FUTURE_VALUE_ERROR: {
        xerror* input=*(xerror* const*)source;
        *(xerror**)value=xrtErrorRef(input);ok=*(xerror**)value!=NULL || !input;break;
    }
    case XL6_FUTURE_VALUE_CLOSURE: {
        XL6_Closure* input=*(XL6_Closure* const*)source;
        *(XL6_Closure**)value=xl6ClosureRuntimeV1Get()->Retain(input);
        ok=*(XL6_Closure**)value!=NULL || !input;break;
    }
    case XL6_FUTURE_VALUE_FUTURE: {
        xfuture* input=*(xfuture* const*)source;
        *(xfuture**)value=input?xrtFutureRef(input):NULL;ok=*(xfuture**)value!=NULL || !input;break;
    }
    case XL6_FUTURE_VALUE_GENERATOR: {
        XL6_Generator* input=*(XL6_Generator* const*)source;
        *(XL6_Generator**)value=input?xl6GeneratorRuntimeV2Get()->Retain(input):NULL;
        ok=*(XL6_Generator**)value!=NULL || !input;break;
    }
    case XL6_FUTURE_VALUE_NATIVE:ok=xrtTypeCopyValue(profile.native->Type(),value,source);break;
    default:ok=false;xrtSetErrorKind(XERR_ARGUMENT);break;
    }
    if(!ok || xrtGetError() || scope.failed) {
        xl6ErrorScopeCaptureFailure(&scope);
        xl6FutureValueDropV1(profile,value);value=NULL;
    }
    xl6_error_result result=xl6ErrorScopeEnd(&scope);
    if(!result.ok) {xrtSetErrorTake(result.error);return false;}
    xrtErrorFree(result.error);*output=value;return true;
}
bool xl6FutureValuePublishV1(XL6_FutureValueProfileV1 profile,xpromise* promise,void* value)
{
    if(!xl6FutureValueProfileValidV1(profile) ||
       (profile.kind!=XL6_FUTURE_VALUE_VOID && !value)) {
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    if(profile.kind==XL6_FUTURE_VALUE_VOID)return xrtPromiseResolve(promise,NULL);
    if(profile.kind==XL6_FUTURE_VALUE_NATIVE)
        return xl6NativeFuturePayloadRuntimeV1Get()->ResolveOwned(promise,value);
    return xl6PayloadResolveOwned(promise,value,policyKind(profile.kind));
}
