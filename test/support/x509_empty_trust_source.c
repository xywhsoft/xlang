/* Canonical empty-source behavior in both shipped implementation and
 * declaration families. Normal offline certificates and ABI checks only. */
#if defined(XL6_TEST_XRUNTIME_DECL)
#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#else
#include "include/xrt_decl.h"
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "x509_path_vectors.h"

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
    xx509cert target,issuer;assert(xrtX509Parse(vector->leaf.Data,vector->leaf.Size,&target) &&
        xrtX509Parse(vector->mid.Data,vector->mid.Size,&issuer));
    xx509store* store=xrtX509StoreCreate();assert(store);
    const xx509cert* candidates[]={&issuer};xx509pathsource source={0};
    assert(xrtX509StoreSource(store,candidates,1,&source) && source.AnchorCount==0 && source.IssuerCount==1);
    xx509pathconfig policy={.Time=pathTime};
    xx509anchor marker={0};xx509pathresult result={73,&marker};
    xx509pathdecision decision={0};const xx509cert* path[32]={0};
    assert(xrtMemDebugFailAfter(0));
    xx509check status=xrtX509PathBuildCheck(&target,&source,&policy,path,32,&result,&decision);
    bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    assert(!hit && !xrtGetError() && status==X509_CHECK_REJECTED &&
        decision.Reason==X509_ERROR_PATH_BUILD && decision.Depth==SIZE_MAX && decision.Message &&
        result.Count==73 && result.Anchor==&marker && !path[0]);
    xrtSetErrorKind(XERR_RANGE);const xerror* prior=xrtGetError();assert(prior);
    assert(xrtX509PathBuildCheck(&target,&source,&policy,path,1,&result,&decision)==X509_CHECK_REJECTED &&
        xrtGetError()==prior && result.Count==73 && result.Anchor==&marker);
    xrtClearError();
    xx509pathsource oldSource=source;
    assert(!xrtX509StoreSource(store,NULL,1,&source) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT &&
        !memcmp(&source,&oldSource,sizeof(source)));xrtClearError();
    source.Anchors=NULL;source.AnchorCount=1;
    xx509pathdecision oldDecision=decision;
    assert(xrtX509PathBuildCheck(&target,&source,&policy,path,1,&result,&decision)==X509_CHECK_ERROR &&
        xrtErrorKind(xrtGetError())==XERR_ARGUMENT && !memcmp(&decision,&oldDecision,sizeof(decision)) &&
        result.Count==73 && result.Anchor==&marker);xrtClearError();
    assert(xrtX509StoreSource(store,NULL,0,&source));
    assert(!xrtX509PathBuild(&target,&source,&policy,path,1,&result) &&
        xrtErrorKind(xrtGetError())==XERR_NOT_FOUND && result.Count==73 && result.Anchor==&marker);xrtClearError();
    assert(xrtX509StoreAdd(store,vector->root.Data,vector->root.Size)==X509_VALUE &&
        xrtX509StoreSource(store,NULL,0,&source) && source.AnchorCount==1 &&
        source.Anchors[0].Certificate.Size==vector->root.Size &&
        !memcmp(source.Anchors[0].Certificate.Data,vector->root.Data,vector->root.Size));
    xrtX509StoreFree(store);balanced(before);
}
int main(void)
{
    for(size_t i=0;i<sizeof(pathVectors)/sizeof(*pathVectors);++i)exercise(&pathVectors[i]);
    assert(xrtMemDebugReset());
    puts("X509 empty trust source: 4 normal families; completed rejection; zero search allocations; prior error and outputs preserved");
    return 0;
}
