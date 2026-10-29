/* Ordinary fixed certificate profiles, declaration ABI and resource budgets.
 * No network, malformed crypto or original security tests. */
#if defined(XL6_TEST_XRUNTIME_DECL)
#include "include/xruntime_decl.h"
#else
#include "include/xrt_decl.h"
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "x509_path_vectors.h"

static unsigned Checks, Calls;
typedef struct WorkCallback {
    const xx509cert* target;
    const xx509pathsource* source;
    xx509pathconfig policy;
    bool fail;
} WorkCallback;

static xx509result critical(const xx509cert* certificate,const xx509ext* extension,size_t depth,void* data)
{
    WorkCallback* call=data;
    assert(certificate && extension && depth==0);Calls++;
    const xx509cert* path[2];xx509pathresult result;xx509pathdecision decision;
    /* Outer search has already consumed all five steps. A nested two-step
     * direct chain must still succeed: counters belong to calls, not TLS. */
    assert(xrtX509PathBuildCheckLimit(call->target,call->source,&call->policy,path,2,2,&result,&decision)==X509_CHECK_VALID);
    if(call->fail){xrtClearError();return X509_ERROR;}
    return X509_VALUE;
}

static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
        before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
        before.UseAfterFreeCount==after.UseAfterFreeCount);
}

static void exercise(const PathVector* vector)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    xx509cert leaf,mid,root,custom;xx509anchor anchor;
    assert(xrtX509Parse(vector->leaf.Data,vector->leaf.Size,&leaf) &&
        xrtX509Parse(vector->mid.Data,vector->mid.Size,&mid) &&
        xrtX509Parse(vector->root.Data,vector->root.Size,&root) &&
        xrtX509Parse(vector->custom.Data,vector->custom.Size,&custom) && xrtX509Anchor(&root,&anchor));
    xx509pathconfig policy={.Time=pathTime};
    const xx509cert* issuers[]={&mid};xx509pathsource source={issuers,1,&anchor,1};
    const xx509cert* path[32];xx509anchor marker={0};
    for(size_t limit=1;limit<=5;limit++){
        xx509pathresult result={73,&marker};xx509pathdecision decision={X509_ERROR_SIGNATURE,7,"unchanged"},old=decision;
        xx509check status=xrtX509PathBuildCheckLimit(&leaf,&source,&policy,path,32,limit,&result,&decision);
        if(limit<5){
            assert(status==X509_CHECK_ERROR && xrtErrorKind(xrtGetError())==XERR_RANGE &&
                result.Count==73 && result.Anchor==&marker && !memcmp(&decision,&old,sizeof(old)));
            xrtClearError();
        }else assert(status==X509_CHECK_VALID && result.Count==2 && result.Anchor==&anchor &&
            path[0]==&leaf && path[1]==&mid && !decision.Reason && !xrtGetError());
        Checks++;
    }
    xx509pathresult result;xx509pathdecision decision;
    assert(xrtX509PathBuildCheckLimit(&leaf,&source,&policy,path,32,0,&result,&decision)==X509_CHECK_VALID &&
        xrtX509PathBuildCheckLimit(&leaf,&source,&policy,path,32,SIZE_MAX,&result,&decision)==X509_CHECK_VALID);
    xx509pathsource empty={0};
    assert(xrtMemDebugFailAfter(0));
    assert(xrtX509PathBuildCheckLimit(&leaf,&empty,&policy,path,32,1,&result,&decision)==X509_CHECK_REJECTED);
    bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();assert(!hit && !xrtGetError());
    WorkCallback callback={.target=&mid,.source=&source,.policy=policy};
    policy.Critical=critical;policy.UserData=&callback;
    unsigned calls=Calls;
    assert(xrtX509PathBuildCheckLimit(&custom,&source,&policy,path,32,4,&result,&decision)==X509_CHECK_ERROR &&
        xrtErrorKind(xrtGetError())==XERR_RANGE && Calls==calls);xrtClearError();
    assert(xrtX509PathBuildCheckLimit(&custom,&source,&policy,path,32,5,&result,&decision)==X509_CHECK_VALID && Calls==calls+1);
    callback.fail=true;
    result=(xx509pathresult){73,&marker};decision=(xx509pathdecision){X509_ERROR_SIGNATURE,7,"unchanged"};
    xx509pathdecision old=decision;
    assert(xrtX509PathBuildCheckLimit(&custom,&source,&policy,path,32,5,&result,&decision)==X509_CHECK_ERROR &&
        !xrtGetError() && Calls==calls+2 && result.Count==73 && result.Anchor==&marker && !memcmp(&decision,&old,sizeof(old)));
    balanced(before);
}

int main(void)
{
    for(size_t i=0;i<sizeof(pathVectors)/sizeof(*pathVectors);i++)exercise(&pathVectors[i]);
    assert(Checks==20 && Calls==8 && xrtMemDebugReset());
    puts("X509 path work snapshot: 4 normal families; 20 exact budgets; 8 independent nested calls; outputs and memory balanced");
    return 0;
}
