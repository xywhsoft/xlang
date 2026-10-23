/* Only normal offline chains, routine policy arguments, owned lifetime and
 * allocation/publication rollback. No malformed crypto or network tests. */
#include "src/runtime/xl6_x509_path_internal.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include "x509_path_vectors.h"
#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif
static size_t budgets,failures,publications;
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);
}
#define FAULTS(Call,Drop) do { \
    for(size_t prefix=0;prefix<256;++prefix) { \
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(prefix)); \
        void* output=(void*)(Call);bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++budgets; \
        assert(hit?(!output && xrtGetError()):(output && !xrtGetError()));if(hit)++failures; \
        xrtClearError();Drop(output);balanced(before);if(!hit)break;assert(prefix<255); \
    } \
}while(0)
static bool publish(void* raw,const XL6_NativeHandlePolicyV1* policy,size_t prefix)
{
    assert(raw && xrtMemDebugFailAfter(prefix));xvalue* value=xl6NativeHandleTakeV1(&raw,policy);
    bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++publications;
    assert(!raw && (hit?(!value && xrtGetError()):(value && !xrtGetError())));xrtClearError();
    xrtValueRelease(value);assert(!xrtGetError());return hit;
}
#define PUBLISH(Call,Policy) do { \
    for(size_t prefix=0;prefix<32;++prefix) { \
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before); \
        bool hit=publish(Call,Policy,prefix);balanced(before);if(!hit)break;assert(prefix<31); \
    } \
}while(0)
static bool equal(xbytesview a,xbytesview b)
{return a.Size==b.Size && (!a.Size || !memcmp(a.Data,b.Data,a.Size));}
static xx509result acceptCritical(const xx509cert* certificate,const xx509ext* ext,size_t depth,void* data)
{assert(certificate && ext && depth==0 && ext->Critical);++*(unsigned*)data;return X509_VALUE;}
static xx509result resource(const xx509cert* c,const xx509ext* e,size_t d,void* p)
{(void)c;(void)e;(void)d;(void)p;xrtSetErrorKind(XERR_MEMORY);return X509_ERROR;}
static xx509result noDiagnostic(const xx509cert* c,const xx509ext* e,size_t d,void* p)
{(void)c;(void)e;(void)d;(void)p;return X509_ERROR;}
static void* detachedPath(void* const* chain,void* anchor,const xx509pathconfig* policy)
{
    XL6_X509PathOutcomeV1* result=xl6X509PathValidateCreateV1(chain,2,anchor,policy,0,0);assert(result);
    void* path=result->path;assert(path);result->path=NULL;xl6X509PathOutcomeDestroyV1(result);return path;
}
static void decisionIsolation(void)
{
    const PathVector* vector=&pathVectors[3];xx509cert root,mid,leaf;xx509anchor anchor;
    assert(xrtX509Parse(vector->root.Data,vector->root.Size,&root) && xrtX509Anchor(&root,&anchor));
    assert(xrtX509Parse(vector->mid.Data,vector->mid.Size,&mid) && xrtX509Parse(vector->leaf.Data,vector->leaf.Size,&leaf));
    const xx509cert* chain[2]={&leaf,&mid};xx509pathconfig config={0};xx509pathdecision decision;
    /* Even a pre-existing diagnostic that looks like a validation rejection
     * cannot determine the explicit native result or alter its precise reason. */
    xerror* prior=xrtErrorCreate(XERR_PROTOCOL,"xrt.x509",X509_ERROR_PATH,"unrelated diagnostic");assert(prior);
    xrtSetErrorTake(prior);assert(xrtMemDebugFailAfter(0));
    assert(xrtX509PathCheck(chain,2,&anchor,&config,&decision)==X509_CHECK_REJECTED &&
        decision.Reason==X509_ERROR_TIME && decision.Depth==0 && xrtGetError()==prior && !xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();xrtClearError();
}
typedef struct Reads {XL6_X509PathV1* path;const PathVector* vector;} Reads;
static void readMany(Reads* input)
{
    for(unsigned i=0;i<32;++i) {
        XL6_X509CertificateV1* cert=xl6X509PathCertificateV1(input->path,0);
        assert(cert && equal(cert->view.Raw,input->vector->leaf));xl6X509CertificateDestroyV1(cert);
    }
}
#if defined(_WIN32)
static DWORD WINAPI worker(void* data){readMany(data);return 0;}
#else
static void* worker(void* data){readMany(data);return NULL;}
#endif
static void concurrent(XL6_X509PathV1* path,const PathVector* vector)
{
    Reads data={path,vector};
#if defined(_WIN32)
    HANDLE threads[4];for(unsigned i=0;i<4;++i){threads[i]=CreateThread(NULL,0,worker,&data,0,NULL);assert(threads[i]);}
    for(unsigned i=0;i<4;++i){assert(WaitForSingleObject(threads[i],INFINITE)==WAIT_OBJECT_0);assert(CloseHandle(threads[i]));}
#else
    pthread_t threads[4];for(unsigned i=0;i<4;++i)assert(!pthread_create(&threads[i],NULL,worker,&data));
    for(unsigned i=0;i<4;++i)assert(!pthread_join(threads[i],NULL));
#endif
}
static void ordinary(const PathVector* vector,bool faults)
{
    void* root=xl6X509CertificateCreateSpanV1(vector->root);void* mid=xl6X509CertificateCreateSpanV1(vector->mid);
    void* leaf=xl6X509CertificateCreateSpanV1(vector->leaf);void* custom=xl6X509CertificateCreateSpanV1(vector->custom);
    assert(root && mid && leaf && custom);
    if(faults)FAULTS(xl6X509AnchorCreateV1(root),xl6X509AnchorDestroyV1);
    void* anchor=xl6X509AnchorCreateV1(root);assert(anchor);xl6X509CertificateDestroyV1(root);
    uint8 purpose[]={43,6,1,5,5,7,3,1};
    xx509pathconfig policy={.Time=pathTime,.Flags=X509_PATH_REQUIRE_KEY_USAGE|X509_PATH_REQUIRE_PURPOSE,
        .KeyUsage=X509_USAGE_DIGITAL_SIGNATURE,.Purpose={purpose,sizeof(purpose)}};
    void* chain[]={leaf,mid};
    if(faults)FAULTS(xl6X509PathValidateCreateV1(chain,2,anchor,&policy,64,16777216),xl6X509PathOutcomeDestroyV1);
    XL6_X509PathOutcomeV1* result=xl6X509PathValidateCreateV1(chain,2,anchor,&policy,64,16777216);
    assert(result && xl6X509PathOutcomeStatusV1(result)==XL6_X509_PATH_VALID_V1 && xl6X509PathOutcomeCodeV1(result)==0 && xl6X509PathOutcomeDepthV1(result)==-1);
    assert(result->path && result->path->count==2 && xl6X509PathOutcomeRequireV1(result));
    assert(equal(result->path->policy.Purpose,policy.Purpose) && result->path->policy.Purpose.Data!=purpose);
    assert(!result->path->policy.Critical && !result->path->policy.UserData);
    if(faults) {
        assert(!xl6X509PathValidateCreateV1(chain,2,anchor,&policy,1,0) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
        assert(!xl6X509PathValidateCreateV1(chain,2,anchor,&policy,0,1) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
        assert(!xl6X509PathValidateCreateV1(chain,2,anchor,&policy,0,-1) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
        xx509pathconfig bad=policy;bad.Flags=128;
        assert(!xl6X509PathValidateCreateV1(chain,2,anchor,&bad,0,0) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
        policy.Time=0;
        FAULTS(xl6X509PathValidateCreateV1(chain,2,anchor,&policy,0,0),xl6X509PathOutcomeDestroyV1);
        XL6_X509PathOutcomeV1* rejected=xl6X509PathValidateCreateV1(chain,2,anchor,&policy,0,0);
        assert(rejected && rejected->status==XL6_X509_PATH_REJECTED_V1 && !rejected->path && rejected->code==X509_ERROR_TIME &&
            xl6X509PathOutcomeDepthV1(rejected)==0 && !xrtGetError());
        xstrview data=xrtErrorDataView(rejected->diagnostic);assert(data.Size==7 && !memcmp(data.Data,"depth=0",7));
        assert(!xl6X509PathOutcomeRequireV1(rejected) && xrtGetError()==rejected->diagnostic);
        xl6X509PathOutcomeDestroyV1(rejected);assert(xrtErrorCode(xrtGetError())==X509_ERROR_TIME);xrtClearError();policy.Time=pathTime;
        unsigned calls=0;chain[0]=custom;policy.Critical=acceptCritical;policy.UserData=&calls;
        XL6_X509PathOutcomeV1* handled=xl6X509PathValidateCreateV1(chain,2,anchor,&policy,0,0);
        assert(handled && handled->status==XL6_X509_PATH_VALID_V1 && calls==1);
        assert(!handled->path->policy.Critical && !handled->path->policy.UserData);xl6X509PathOutcomeDestroyV1(handled);
        policy.Critical=resource;
        assert(!xl6X509PathValidateCreateV1(chain,2,anchor,&policy,0,0) && xrtGetError());
        const xerror* cause=xrtGetError();while(cause && xrtErrorKind(cause)!=XERR_MEMORY)cause=xrtErrorCause(cause);assert(cause);xrtClearError();
        policy.Critical=noDiagnostic;xl6_error_scope checked={0};assert(xl6ErrorScopeBegin(&checked));
        assert(!xl6X509PathValidateCreateV1(chain,2,anchor,&policy,0,0));
        xl6_error_result failed=xl6ErrorScopeEnd(&checked);assert(!failed.ok && !failed.error && !xrtGetError());
        policy.Critical=NULL;policy.UserData=NULL;
        handled=xl6X509PathValidateCreateV1(chain,2,anchor,&policy,0,0);
        assert(handled && handled->status==XL6_X509_PATH_REJECTED_V1 && handled->code==X509_ERROR_CRITICAL_EXTENSION);xl6X509PathOutcomeDestroyV1(handled);
        chain[0]=leaf;
        xerror* prior=xrtErrorCreate(XERR_STATE,"test",1,"prior");assert(prior);xrtSetErrorTake(prior);
        assert(!xl6X509PathValidateCreateV1(chain,2,anchor,&policy,0,0) && xrtGetError()==prior);xrtClearError();
        PUBLISH(xl6X509AnchorCreateV1(&((XL6_X509CertificateV1){.view=((XL6_X509AnchorV1*)anchor)->certificate})),xl6X509AnchorHandlePolicyV1Get());
        PUBLISH(xl6X509PathValidateCreateV1(chain,2,anchor,&policy,0,0),xl6X509PathOutcomeHandlePolicyV1Get());
        PUBLISH(detachedPath(chain,anchor,&policy),xl6X509PathHandlePolicyV1Get());
        FAULTS(xl6X509PathCertificateV1(result->path,0),xl6X509CertificateDestroyV1);
        FAULTS(xl6X509PathAnchorV1(result->path),xl6X509AnchorDestroyV1);
    }
    xl6X509CertificateDestroyV1(mid);xl6X509CertificateDestroyV1(leaf);xl6X509CertificateDestroyV1(custom);xl6X509AnchorDestroyV1(anchor);
    memset(purpose,0,sizeof(purpose));
    XL6_X509CertificateV1* saved=xl6X509PathCertificateV1(result->path,0);assert(saved && equal(saved->view.Raw,vector->leaf));
    XL6_X509AnchorV1* savedAnchor=xl6X509PathAnchorV1(result->path);assert(savedAnchor && equal(savedAnchor->certificate.Raw,vector->root));
    concurrent(result->path,vector);xl6X509PathOutcomeDestroyV1(result);
    assert(equal(saved->view.Raw,vector->leaf) && equal(savedAnchor->certificate.Raw,vector->root));
    xl6X509CertificateDestroyV1(saved);xl6X509AnchorDestroyV1(savedAnchor);assert(!xrtGetError());
}
int main(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);decisionIsolation();
    for(size_t i=0;i<sizeof(pathVectors)/sizeof(*pathVectors);++i)ordinary(&pathVectors[i],i==3);
    balanced(before);printf("X509 ordered path: 4 normal chains, %zu allocation budgets, %zu failures, %zu publication budgets; balanced\n",budgets,failures,publications);return 0;
}
