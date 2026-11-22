#include "foreign_xge3d_billboard_share.h"

#include <assert.h>
#include <stdio.h>

static xmemdebugsnapshot before;
static xge3d_billboard_batch *held;

void fixtureBillboardHeapBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
}

void fixtureBillboardHeapEnd(int native)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    printf("Billboard share heap %s: alloc=%llu free=%llu realloc=%llu\n",native?"native":"language",
        (unsigned long long)(after.AllocCount-before.AllocCount),
        (unsigned long long)(after.FreeCount-before.FreeCount),
        (unsigned long long)(after.ReallocCount-before.ReallocCount));
}

void fixtureBillboardSameStorage(xge3d_billboard_batch *first,xge3d_billboard_batch *second)
{
    const xge3d_billboard_geometry_t *a=NULL,*b=NULL;
    size_t countA=0,countB=0;
    assert(xge3dBillboardBatchGetGeometry(first,&a,&countA)==XGE_OK);
    assert(xge3dBillboardBatchGetGeometry(second,&b,&countB)==XGE_OK);
    assert(first==second && a==b && countA==countB);
}

void fixtureBillboardHold(xge3d_billboard_batch *batch)
{
    assert(!held);
    assert(xge3dBillboardBatchShareOwned(batch,NULL)==XGE_ERROR_INVALID_ARGUMENT);
    held=batch;
    assert(xge3dBillboardBatchShareOwned(NULL,&held)==XGE_ERROR_INVALID_ARGUMENT && !held);
    assert(xge3dBillboardBatchShareOwned(batch,&held)==XGE_OK && held==batch);

    fixtureBillboardHeapBegin();
    for (int i=0;i<1000;++i) {
        xge3d_billboard_batch *other=NULL;
        assert(xge3dBillboardBatchShareOwned(batch,&other)==XGE_OK && other==batch);
        xge3dBillboardBatchFree(other);
    }
    fixtureBillboardHeapEnd(1);
}

void fixtureBillboardDropLast(void)
{
    xmemdebugsnapshot prior,after;
    assert(held);
    xrtMemDebugSnapshot(&prior);
    xge3dBillboardBatchFree(held);
    held=NULL;
    xrtMemDebugSnapshot(&after);
    assert(after.FreeCount==prior.FreeCount+1 && after.LiveCount+1==prior.LiveCount);
}

void fixtureBillboardFail(int prefix)
{
    if (prefix<0) xrtMemDebugFailClear();
    else assert(xrtMemDebugFailAfter(prefix));
}
