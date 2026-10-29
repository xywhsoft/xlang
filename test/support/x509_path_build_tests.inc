#include "src/runtime/xl6_x509_path_build_internal.h"
static void* buildOutcome(void* target,void* const* candidates,size_t count,void* trust,const xx509pathconfig* policy)
{return xl6X509PathBuildCreateV1(target,candidates,count,trust,policy,64,16777216,4096,67108864,100000);}
static void buildWork(const PathVector* vector)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    void* root=xl6X509CertificateCreateSpanV1(vector->root);
    void* mid=xl6X509CertificateCreateSpanV1(vector->mid);
    void* leaf=xl6X509CertificateCreateSpanV1(vector->leaf);assert(root && mid && leaf);
    void* store=xl6X509StoreCreateV1();assert(store && xl6X509StoreAddV1(store,root));
    void* trust=xl6X509StoreSnapshotV1(store);assert(trust);
    void* candidates[]={mid};xx509pathconfig policy={.Time=pathTime};
    for(int64_t steps=1;steps<=5;steps++){
        void* result=xl6X509PathBuildCreateV1(leaf,candidates,1,trust,&policy,64,16777216,4096,67108864,steps);
        if(steps<5){assert(!result && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();}
        else assert(result && xl6X509PathOutcomeStatusV1(result)==XL6_X509_PATH_VALID_V1);
        xl6X509PathOutcomeDestroyV1(result);
    }
    const int64_t unlimited[]={0,INT64_MAX};
    for(size_t i=0;i<sizeof(unlimited)/sizeof(*unlimited);i++){
        void* result=xl6X509PathBuildCreateV1(leaf,candidates,1,trust,&policy,64,16777216,4096,67108864,unlimited[i]);
        assert(result && !xrtGetError());xl6X509PathOutcomeDestroyV1(result);
    }
    assert(!xl6X509PathBuildCreateV1(leaf,candidates,1,trust,&policy,64,16777216,4096,67108864,-1) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
    assert(!xl6X509PathPolicyCreateV1(pathTime,0,0,(xstrview){0},64,16777216,-1) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
    void* limit=xl6X509PathPolicyCreateV1(pathTime,0,0,(xstrview){0},64,16777216,6);assert(limit);
    assert(xl6X509PathPolicyMaxSearchStepsV1(limit)==6 && !xrtGetError());
    xvalue* array=input(vector->leaf,vector->mid);
    void* result=xl6X509PathBuildV1(arrayContext(),leaf,array,trust,limit,4096,67108864);
    assert(result && xl6X509PathOutcomeStatusV1(result)==XL6_X509_PATH_VALID_V1);
    xl6X509PathOutcomeDestroyV1(result);xl6X509PathPolicyDestroyV1(limit);
    limit=xl6X509PathPolicyCreateV1(pathTime,0,0,(xstrview){0},64,16777216,5);assert(limit);
    assert(!xl6X509PathBuildV1(arrayContext(),leaf,array,trust,limit,4096,67108864) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    xl6X509PathPolicyDestroyV1(limit);xrtValueRelease(array);
    xl6X509StoreSnapshotDestroyV1(trust);xl6X509StoreDestroyV1(store);
    xl6X509CertificateDestroyV1(root);xl6X509CertificateDestroyV1(mid);xl6X509CertificateDestroyV1(leaf);balanced(before);
}
static void buildOrdinary(const PathVector* vector,bool faults)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    void* root=xl6X509CertificateCreateSpanV1(vector->root);
    void* mid=xl6X509CertificateCreateSpanV1(vector->mid);
    void* leaf=xl6X509CertificateCreateSpanV1(vector->leaf);assert(root && mid && leaf);
    assert(xl6X509IssuerMatchV1(leaf,mid) && xl6X509IssuerMatchV1(mid,root) && !xl6X509IssuerMatchV1(leaf,root) && !xrtGetError());
    void* store=xl6X509StoreCreateV1();assert(store && xl6X509StoreAddV1(store,root));
    void* trust=xl6X509StoreSnapshotV1(store);assert(trust);
    void* candidates[]={root,leaf,mid,mid};xx509pathconfig policy={.Time=pathTime};
    if(faults)FAULTS(buildOutcome(leaf,candidates,4,trust,&policy),xl6X509PathOutcomeDestroyV1);
    XL6_X509PathOutcomeV1* result=buildOutcome(leaf,candidates,4,trust,&policy);
    assert(result && result->status==XL6_X509_PATH_VALID_V1 && result->path && result->path->count==2 && result->depth==-1);
    assert(equal(result->path->certificates[0].Raw,vector->leaf) && equal(result->path->certificates[1].Raw,vector->mid) &&
        equal(result->path->anchorCertificate.Raw,vector->root) && !result->path->policy.Critical && !result->path->policy.UserData);
    assert(result->path->certificates[0].Raw.Data!=((XL6_X509CertificateV1*)leaf)->view.Raw.Data);
    concurrent(result->path,vector);
    XL6_X509PathOutcomeV1* direct=buildOutcome(mid,NULL,0,trust,&policy);
    assert(direct && direct->status==XL6_X509_PATH_VALID_V1 && direct->path->count==1);xl6X509PathOutcomeDestroyV1(direct);
    if(faults){
        assert(!xl6X509PathBuildCreateV1(leaf,candidates,4,trust,&policy,1,0,0,0,100000) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
        assert(!xl6X509PathBuildCreateV1(leaf,candidates,4,trust,&policy,0,1,0,0,100000) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
        assert(!xl6X509PathBuildCreateV1(leaf,candidates,4,trust,&policy,0,0,4,0,100000) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
        assert(!xl6X509PathBuildCreateV1(leaf,candidates,4,trust,&policy,0,0,0,1,100000) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
        assert(!xl6X509PathBuildCreateV1(leaf,candidates,4,trust,&policy,0,0,-1,0,100000) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
        policy.Time=0;FAULTS(buildOutcome(leaf,candidates,4,trust,&policy),xl6X509PathOutcomeDestroyV1);
        XL6_X509PathOutcomeV1* negative=buildOutcome(leaf,candidates,4,trust,&policy);
        assert(negative && negative->status==XL6_X509_PATH_REJECTED_V1 && negative->code==X509_ERROR_TIME && negative->depth==0 && !negative->path);
        assert(!xl6X509PathOutcomeRequireV1(negative) && xrtErrorCode(xrtGetError())==X509_ERROR_TIME);xrtClearError();xl6X509PathOutcomeDestroyV1(negative);policy.Time=pathTime;
    }
    xl6X509StoreClearV1(store);void* empty=xl6X509StoreSnapshotV1(store);assert(empty);
    if(faults)FAULTS(buildOutcome(leaf,candidates,4,empty,&policy),xl6X509PathOutcomeDestroyV1);
    XL6_X509PathOutcomeV1* rejected=buildOutcome(leaf,candidates,4,empty,&policy);
    assert(rejected && rejected->status==XL6_X509_PATH_REJECTED_V1 && rejected->code==X509_ERROR_PATH_BUILD && rejected->depth==-1 && !rejected->path);
    xl6X509PathOutcomeDestroyV1(rejected);xl6X509StoreSnapshotDestroyV1(empty);
    xl6X509StoreSnapshotDestroyV1(trust);xl6X509StoreDestroyV1(store);
    xl6X509CertificateDestroyV1(root);xl6X509CertificateDestroyV1(mid);xl6X509CertificateDestroyV1(leaf);
    void* projected=xl6X509PathCertificateV1(result->path,0);assert(projected && equal(((XL6_X509CertificateV1*)projected)->view.Raw,vector->leaf));
    xl6X509CertificateDestroyV1(projected);xl6X509PathOutcomeDestroyV1(result);balanced(before);
}
static void buildCallbacks(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);const PathVector* vector=&pathVectors[3];
    void* root=xl6X509CertificateCreateSpanV1(vector->root);void* target=xl6X509CertificateCreateSpanV1(vector->custom);
    assert(root && target);void* anchor=xl6X509AnchorCreateV1(root);void* store=xl6X509StoreCreateV1();assert(anchor && store && xl6X509StoreAddV1(store,root));
    xl6X509CertificateDestroyV1(root);void* trust=xl6X509StoreSnapshotV1(store);assert(trust);
    void* policy=xl6X509PathPolicyCreateV1(pathTime,0,0,(xstrview){0},64,16777216,100000);void* context=xl6X509PathCriticalCreateV1();assert(policy && context);
    xvalue* array=input(vector->custom,vector->mid);assert(array);
    Call call={.context=context,.anchor=anchor,.policy=policy,.array=array,.nested=true};
    const XL6_ClosureRuntimeV1* closures=xl6ClosureRuntimeV1Get();
    XL6_Closure* callback=closures->Create(NULL,(void (*)(void))invoke,NULL,&call,environmentDrop,NULL,NULL,0,environmentTrace);assert(callback);
    void* limit=xl6X509PathPolicyCreateV1(pathTime,0,0,(xstrview){0},64,16777216,5);assert(limit);
    assert(!xl6X509PathBuildCallbackV1(arrayContext(),target,array,trust,limit,4096,67108864,context,callback) &&
        xrtErrorKind(xrtGetError())==XERR_RANGE && call.calls==0);xrtClearError();xl6X509PathPolicyDestroyV1(limit);
    void* result=xl6X509PathBuildCallbackV1(arrayContext(),target,array,trust,policy,4096,67108864,context,callback);
    assert(result && call.calls==1 && xl6X509PathOutcomeStatusV1(result)==XL6_X509_PATH_VALID_V1);
    xl6X509PathOutcomeDestroyV1(result);clear(&call);call.nested=false;
    limit=xl6X509PathPolicyCreateV1(pathTime,0,0,(xstrview){0},64,16777216,6);assert(limit);
    result=xl6X509PathBuildCallbackV1(arrayContext(),target,array,trust,limit,4096,67108864,context,callback);
    assert(result && call.calls==2);xl6X509PathOutcomeDestroyV1(result);clear(&call);xl6X509PathPolicyDestroyV1(limit);
    for(unsigned mode=0;mode<2;++mode){
        call.fail=true;call.noDiagnostic=mode!=0;xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));
        assert(!xl6X509PathBuildCallbackV1(arrayContext(),target,array,trust,policy,4096,67108864,context,callback));
        xl6_error_result failure=xl6ErrorScopeEnd(&scope);assert(!failure.ok && (mode?!failure.error:xrtErrorKind(failure.error)==XERR_RANGE));
        xrtErrorFree(failure.error);clear(&call);
    }
    call.fail=false;
    for(size_t prefix=0;prefix<128;++prefix){
        xmemdebugsnapshot start;xrtMemDebugSnapshot(&start);xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));assert(xrtMemDebugFailAfter(prefix));
        result=xl6X509PathBuildCallbackV1(arrayContext(),target,array,trust,policy,4096,67108864,context,callback);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++budgets;if(hit)++failures;
        if(!result)xl6ErrorScopeCaptureFailure(&scope);
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope);assert(hit?(!result && !outcome.ok):(result && outcome.ok));
        xrtErrorFree(outcome.error);clear(&call);xl6X509PathOutcomeDestroyV1(result);balanced(start);
        if(!hit)break;
        assert(prefix<127);
    }
    CATALOG(xl6X509PathBuildV1(arrayContext(),target,array,trust,policy,4096,67108864),PathOutcome);
    call.clearInput=true;call.store=store;
    result=xl6X509PathBuildCallbackV1(arrayContext(),target,array,trust,policy,4096,67108864,context,callback);assert(result);
    assert(xl6X509StoreCountV1(store)==0 && xl6X509StoreSnapshotCountV1(trust)==1);
    assert(((XL6_X509PathOutcomeV1*)result)->path->count==2);
    xl6X509PathOutcomeDestroyV1(result);clear(&call);closures->Release(callback);xrtValueRelease(array);
    xl6X509PathCriticalDestroyV1(context);xl6X509PathPolicyDestroyV1(policy);xl6X509AnchorDestroyV1(anchor);
    xl6X509CertificateDestroyV1(target);xl6X509StoreSnapshotDestroyV1(trust);xl6X509StoreDestroyV1(store);balanced(before);
}
static void buildTests(void)
{
    for(size_t i=0;i<sizeof(pathVectors)/sizeof(*pathVectors);++i){buildOrdinary(&pathVectors[i],i==3);buildWork(&pathVectors[i]);}
    buildCallbacks();
    puts("X509 search work: 4 normal families; raw and public exact boundaries; pre-callback stop; balanced");
}
