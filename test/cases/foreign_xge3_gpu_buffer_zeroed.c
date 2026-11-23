#include "foreign_xge3_gpu_buffer_zeroed.h"

#undef NDEBUG
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

static xmemdebugsnapshot before;
static double started;

bool fixtureGpuBufferContents(xge_buffer buffer,int lastByte,bool uploaded)
{
    if(!buffer || !buffer->pData || buffer->iSize!=16 ||
       (buffer->iBackendId!=0)!=uploaded)return false;
    const unsigned char* data=buffer->pData;
    for(int index=0;index<15;++index){
        if(data[index]!=0)return false;
    }
    return data[15]==lastByte;
}

void fixtureGpuBufferStartMeasure(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
    started=xrtTimer();
}

void fixtureGpuBufferFinishMeasure(int language)
{
    double elapsed=xrtTimer()-started;
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before.LiveCount && after.LiveBytes==before.LiveBytes);
    printf("XGE_ZEROED_BUFFER_SAMPLE language=%d count=256 alloc=%" PRIu64
        " free=%" PRIu64 " realloc=%" PRIu64 " wallMs=%.6f\n",language,
        after.AllocCount-before.AllocCount,after.FreeCount-before.FreeCount,
        after.ReallocCount-before.ReallocCount,elapsed*1000.0);
}

void fixtureGpuBufferMeasureC(void)
{
    fixtureGpuBufferStartMeasure();
    for(int index=0;index<256;++index){
        xge_buffer buffer=NULL;
        assert(xgeBufferCreateOwned(&buffer,XGE_BUFFER_VERTEX,XGE_BUFFER_DYNAMIC,NULL,16)==XGE_OK);
        assert(fixtureGpuBufferContents(buffer,0,false));
        xgeBufferFree(buffer);
    }
    fixtureGpuBufferFinishMeasure(0);
}
