#include "xl6_tls_revocation_internal.h"
#include "xl6_tls_verifier_context.h"
#include "xl6_native_class_callback.h"

typedef struct RevocationPolicy {
    XL6_Closure* driver;
    XL6_TlsRevocationOptionsV1 options;
} RevocationPolicy;
static const XL6_ClosureCaptureSlot slots[]={
    {XL6_CLOSURE_CAPTURE_CLOSURE,offsetof(RevocationPolicy,driver),sizeof(XL6_Closure*)},
    {XL6_CLOSURE_CAPTURE_TRIVIAL,offsetof(RevocationPolicy,options),sizeof(XL6_TlsRevocationOptionsV1)}};
static const XL6_ClosureEnvironmentV1 layout={sizeof(layout),sizeof(RevocationPolicy),2,slots};
static bool verify(void* raw,const xtlsverifiedpeer* peer);
static const RevocationPolicy* checked(void* raw)
{
    if(xrtGetError())return NULL;
    XL6_Closure* self=raw;
    if(!self||self->invoke!=(void(*)(void))verify||!self->environment){
        xl6TlsFailV1(XERR_TYPE,"missing resident TLS revocation policy owner");return NULL;}
    return self->environment;
}
static bool verify(void* raw,const xtlsverifiedpeer* peer)
{
    const RevocationPolicy* self=checked(raw);if(!self)return false;
    XL6_TlsVerifiedPeerPacketV1* packet=xl6TlsVerifiedPeerPacketCopyV1(peer);
    if(!packet){xl6ForeignCallbackFail();return false;}
    bool accepted=true;size_t count=xl6TlsVerifiedPeerPacketPathCountV1(packet);
    for(size_t i=0;i<count&&accepted;i++){
        /* A native selected path excludes its separate anchor. No supplied
         * presented order or arbitrary status decides the target/issuer. */
        void* input=xl6TlsRevocationRequestCreateV1(packet,i,&self->options);xvalue* result=NULL;
        XL6_TlsRevocationRequestV1 binding={packet,i,self->options};
        bool ok=input&&xl6NativeClassCallbackInvokeTakeV1(self->driver,&input,&result);
        if(input)xl6TlsRevocationRequestDestroyV1(input);
        if(ok){
            void* value=NULL;const xvaluehandleops* ops=NULL;void* user=NULL;
            if(result&&(!xrtValueGetHandle(result,&value,&ops,&user)||!value||user||
                ops!=xl6X509RevocationCheckHandlePolicyV1Get()->Ops)){
                xl6TlsFailV1(XERR_TYPE,"TLS revocation provider returned an invalid check owner");ok=false;}
            if(ok)accepted=xl6TlsRevocationCheckBoundV1(&binding,value);
        }
        xrtValueRelease(result);
        if(!ok){xl6ForeignCallbackFail();accepted=false;}
    }
    xl6TlsVerifiedPeerPacketDropV1(packet);return accepted;
}
void* xl6TlsRevocationPolicyCreateV1(XL6_NativeClassCallbackContextV1 context,XL6_Closure* callback,
    bool allowUnknown,int64_t skew,int64_t flags,int64_t maxSources,int64_t maxSignerCertificates)
{
    if(xrtGetError())return NULL;
    if(context.argument.field.policy!=xl6TlsRevocationRequestHandlePolicyV1Get()||
        context.result.policy!=xl6X509RevocationCheckHandlePolicyV1Get()){
        xl6TlsFailV1(XERR_TYPE,"TLS revocation provider needs sealed request/check recipes");return NULL;}
    xx509crlconfig config;
    if(!xl6X509CrlPolicyConfigV1(0,skew,(uint64)flags,NULL,NULL,&config))return NULL;
    if(maxSources<=0||maxSignerCertificates<=0||(uint64)maxSources>SIZE_MAX||(uint64)maxSignerCertificates>SIZE_MAX){
        xl6TlsFailV1(XERR_RANGE,"TLS revocation budgets must be positive native counts");return NULL;}
    XL6_Closure* driver=xl6NativeClassCallbackCreateV1(context,callback);if(!driver)return NULL;
    RevocationPolicy* self=xrtCalloc(1,sizeof(*self));
    if(!self){
        xl6_error_scope cleanup={0};if(!xl6ErrorScopeBeginCleanup(&cleanup,true))abort();
        xl6ClosureRuntimeV1Get()->Release(driver);
        xl6_error_result failure=xl6ErrorScopeEnd(&cleanup);xl6ErrorScopePropagateFailure(failure);return NULL;
    }
    self->driver=driver;self->options=(XL6_TlsRevocationOptionsV1){allowUnknown,skew,config.Flags,
        (size_t)maxSources,(size_t)maxSignerCertificates};
    /* This is an ordinary V2 capture graph with resident code, not a fake
     * CodeOwner or an opaque callback box. The real driver owns both leases. */
    XL6_Closure* result=xl6ClosureRuntimeV2Get()->Create(NULL,(void(*)(void))verify,NULL,self,&layout,NULL,NULL,0);
    if(!result){
        xl6_error_scope cleanup={0};if(!xl6ErrorScopeBeginCleanup(&cleanup,true))abort();
        xl6ClosureRuntimeV2Get()->DropEnvironment(&layout,self);
        xl6_error_result failure=xl6ErrorScopeEnd(&cleanup);xl6ErrorScopePropagateFailure(failure);
    }
    return result;
}
void xl6TlsRevocationPolicyDestroyV1(void* raw){xl6ClosureRuntimeV1Get()->Release(raw);}
bool xl6TlsRevocationPolicyAllowsUnknownV1(void* raw)
{const RevocationPolicy* self=checked(raw);return self&&self->options.allowUnknown;}
int64_t xl6TlsRevocationPolicySkewV1(void* raw)
{const RevocationPolicy* self=checked(raw);return self?self->options.skew:0;}
int64_t xl6TlsRevocationPolicyFlagsV1(void* raw)
{const RevocationPolicy* self=checked(raw);return self?(int64_t)self->options.flags:0;}
int64_t xl6TlsRevocationPolicyMaxSourcesV1(void* raw)
{const RevocationPolicy* self=checked(raw);return self?(int64_t)self->options.maxSources:0;}
int64_t xl6TlsRevocationPolicyMaxSignerCertificatesV1(void* raw)
{const RevocationPolicy* self=checked(raw);return self?(int64_t)self->options.maxSignerCertificates:0;}
static void* ref(void* raw){return xl6ClosureRuntimeV1Get()->Retain(raw);}
static void drop(void* raw){xl6ClosureRuntimeV1Get()->Release(raw);}
static xrtownershipref ownership(const void* raw){return xl6ClosureRuntimeV1Get()->Ownership(raw);}
static const XL6_TlsPeerPolicyOpsV1 operations={ref,drop,ownership,verify};
XL6_TlsPeerPolicyV1 xl6TlsRevocationPolicyOwnerV1(void* raw)
{return checked(raw)?(XL6_TlsPeerPolicyV1){raw,&operations}:(XL6_TlsPeerPolicyV1){0};}
static bool trace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* actual=NULL;
    if(!visit||!xrtValueGetHandle(value,&raw,&actual,&user)||!raw||user||
        actual!=xl6TlsRevocationPolicyHandlePolicyV1Get()->Ops)return false;
    return visit(ownership(raw),context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6TlsRevocationPolicyHandlePolicyV1Get,xl6TlsRevocationPolicyDestroyV1,trace)
