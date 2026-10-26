#include "xl6_tls_verifier_context.h"

struct XL6_TlsVerifierContextV1 {
    volatile int32 refs;
    XL6_TlsVerificationPolicyV1 policy;
    XL6_TlsPeerPolicyV1 callback,revocation;
    const void* claim;
    bool cleared,finishing,finished,failed;
    xerror* failure;
};
static void begin(xrtownershipscope* scope){if(!xrtOwnershipMutationBegin(scope))abort();}
static void end(xrtownershipscope* scope){if(!xrtOwnershipScopeEnd(scope))abort();}
static bool busy(const void* raw){return raw&&((const XL6_TlsVerifierContextV1*)raw)->finishing;}
static bool count(const void* raw,size_t* output)
{
    const XL6_TlsVerifierContextV1* self=raw;
    if(!self||!output||self->cleared||busy(self)||self->refs<=0)return false;
    *output=(size_t)self->refs;return true;
}
static bool trace(const void* raw,xrtownershipvisitor visit,void* data)
{
    const XL6_TlsVerifierContextV1* self=raw;size_t refs;
    return visit&&count(self,&refs)&&(!self->callback.data||
        visit(self->callback.ops->Ownership(self->callback.data),data))&&(!self->revocation.data||
        visit(self->revocation.ops->Ownership(self->revocation.data),data));
}
static const xrtownershipops ops={count,trace};
static bool complete(XL6_TlsPeerPolicyV1 value)
{return value.data?(value.ops&&value.ops->Ref&&value.ops->Drop&&value.ops->Ownership&&value.ops->Verify):!value.ops;}
XL6_TlsVerifierContextV1* xl6TlsVerifierContextCreatePoliciesV1(
    const XL6_TlsVerificationPolicyV1* policy,XL6_TlsPeerPolicyV1 callback,XL6_TlsPeerPolicyV1 revocation)
{
    if(xrtGetError())return NULL;
    if(!policy||!complete(callback)||!complete(revocation)){
        xl6TlsFailV1(XERR_ARGUMENT,"TLS verifier needs a complete resident policy owner");return NULL;}
#define XL6_TLS_VERIFY_LIMIT(Field,Name) if(policy->limits.Field<0||(uint64_t)policy->limits.Field>SIZE_MAX){xl6TlsFailV1(XERR_RANGE,"TLS verification budget exceeds its native range");return NULL;}
#include "xl6_tls_verification_policy_fields.inc"
#undef XL6_TLS_VERIFY_LIMIT
    XL6_TlsVerifierContextV1* self=xrtCalloc(1,sizeof(*self));if(!self)return NULL;
    self->refs=1;self->policy=*policy;
    if(callback.data){
        self->callback=(XL6_TlsPeerPolicyV1){callback.ops->Ref(callback.data),callback.ops};
        if(!self->callback.data){xl6TlsOutcomeV1(false);xrtFree(self);return NULL;}
    }
    if(revocation.data){
        self->revocation=(XL6_TlsPeerPolicyV1){revocation.ops->Ref(revocation.data),revocation.ops};
        if(!self->revocation.data){
            xl6TlsOutcomeV1(false);xl6_error_scope cleanup={0};
            if(!xl6ErrorScopeBeginCleanup(&cleanup,true))abort();
            xl6TlsVerifierContextDropV1(self);xl6_error_result failure=xl6ErrorScopeEnd(&cleanup);
            xl6ErrorScopePropagateFailure(failure);return NULL;}
    }
    return self;
}
XL6_TlsVerifierContextV1* xl6TlsVerifierContextCreateV1(
    const XL6_TlsVerificationPolicyV1* policy,XL6_TlsPeerPolicyV1 callback)
{return xl6TlsVerifierContextCreatePoliciesV1(policy,callback,(XL6_TlsPeerPolicyV1){0});}
const XL6_TlsVerificationPolicyV1* xl6TlsVerifierContextPolicyV1(const XL6_TlsVerifierContextV1* self)
{return self?&self->policy:NULL;}
static bool cleanup(XL6_TlsVerifierContextV1* self)
{
    xrtownershipscope mutation={0};begin(&mutation);
    if(self->finishing){end(&mutation);return false;}
    if(self->finished){
        bool failed=self->failed;xerror* failure=self->failure;end(&mutation);
        if(failure&&!xrtGetError())xrtSetError(failure);
        return !failed;
    }
    self->finishing=true;XL6_TlsPeerPolicyV1 callback=self->callback,revocation=self->revocation;
    self->callback=self->revocation=(XL6_TlsPeerPolicyV1){0};end(&mutation);
    /* Judge this duty's own outcome. Inheriting an unrelated earlier failure
     * would permanently poison an otherwise successful Context retirement. */
    xl6_error_result result={true,NULL};
    if(callback.data||revocation.data){
        xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope))abort();
        if(callback.data)callback.ops->Drop(callback.data);
        if(revocation.data)revocation.ops->Drop(revocation.data);
        result=xl6ErrorScopeEnd(&scope);
    }
    begin(&mutation);self->failed=!result.ok;self->failure=result.error;
    self->finishing=false;self->finished=true;end(&mutation);
    if(result.error&&!xrtGetError())xrtSetError(result.error);
    return result.ok;
}
void xl6TlsVerifierContextDropV1(void* raw)
{
    XL6_TlsVerifierContextV1* self=raw;if(!self)return;
    xrtownershipscope mutation={0};begin(&mutation);
    int32 refs=xrtRefRelease(&self->refs);if(refs<0)abort();end(&mutation);
    if(refs)return;
    bool ok=cleanup(self);xerror* failure=self->failure;self->failure=NULL;xrtFree(self);
    if(!ok)xl6ErrorScopePropagateFailure((xl6_error_result){false,failure});
    else xrtErrorFree(failure);
}
static xtime clockValue(void* raw)
{
    const XL6_TlsVerifierContextV1* self=raw;
    return self->policy.fixedTime?self->policy.at:xrtNow();
}
static bool verify(const xtlsverifiedpeer* peer,void* raw)
{
    const XL6_TlsVerifierContextV1* self=raw;
    return (!self->revocation.data||self->revocation.ops->Verify(self->revocation.data,peer))&&
        (!self->callback.data||self->callback.ops->Verify(self->callback.data,peer));
}
const xtlsverifiercontextownershipv1* xl6TlsVerifierContextNativePolicyV1Get(void)
{
    static const xtlsverifiercontextownershipv1 policy={sizeof(policy),NULL,verify,clockValue,
        xl6TlsVerifierContextDropV1,&ops};return &policy;
}
static bool hold(const void* raw)
{
    XL6_TlsVerifierContextV1* self=(XL6_TlsVerifierContextV1*)raw;if(!self)return false;
    xrtownershipscope mutation={0};begin(&mutation);
    bool ok=!self->cleared&&xrtRefRetain(&self->refs)>0;end(&mutation);return ok;
}
static bool claim(const void* raw,const void* token)
{
    XL6_TlsVerifierContextV1* self=(XL6_TlsVerifierContextV1*)raw;size_t refs;
    if(!token||!count(self,&refs)||(self->claim&&self->claim!=token))return false;
    self->claim=token;return true;
}
static void restore(const void* raw,const void* token)
{
    XL6_TlsVerifierContextV1* self=(XL6_TlsVerifierContextV1*)raw;
    if(!token||self->claim!=token||self->cleared)abort();
    self->claim=NULL;
}
static void clear(const void* raw,const void* token)
{
    XL6_TlsVerifierContextV1* self=(XL6_TlsVerifierContextV1*)raw;
    if(!token||self->claim!=token||self->cleared||busy(self))abort();
    self->cleared=true;
}
static bool finish(const void* raw,const void* token)
{
    XL6_TlsVerifierContextV1* self=(XL6_TlsVerifierContextV1*)raw;
    if(!token||self->claim!=token||!self->cleared)abort();
    return cleanup(self);
}
static void drop(const void* raw){xl6TlsVerifierContextDropV1((void*)raw);}
const xrtownershipadapterv1* xl6TlsVerifierContextOwnershipAdapterV1(xrtownershipref ref)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),hold,
        drop,claim,restore,NULL,clear,finish};
    size_t refs;return ref.Ops==&ops&&count(ref.Data,&refs)?&adapter:NULL;
}
const xrtownershipquiescencev1* xl6TlsVerifierContextQuiescenceV1Get(void)
{static const xrtownershipquiescencev1 policy={sizeof(policy),&ops,busy};return &policy;}
