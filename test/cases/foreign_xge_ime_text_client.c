#include "foreign_xge_ime_text_client.h"
#include <xge3_runtime.h>

#undef NDEBUG
#include <assert.h>
#include <stdio.h>

static xmemdebugsnapshot before;
static double started;

void imeSnapshotSampleBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
    assert(xrtMemDebugFailAfter(0));
    started=xrtTimer();
}

void imeSnapshotSampleEnd(int language)
{
    double elapsed=xrtTimer()-started;
    assert(!xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.AllocCount==before.AllocCount && after.FreeCount==before.FreeCount);
    assert(after.ReallocCount==before.ReallocCount && after.LiveCount==before.LiveCount);
    printf("XGE_IME_SNAPSHOT_SAMPLE language=%d count=1000 alloc=0 free=0 realloc=0 wallMs=%.3f\n",
        language,elapsed*1000.0);
}

static int provide_snapshot(xge_ime_text_output_t* output,void* user)
{
    (void)user;
    static const char text[]={'A','\0',(char)0xc3,(char)0xa9};
    return xgeImeTextOutputSet(output,text,4,2,4,10,32,47);
}

void imeSnapshotSampleNative(void)
{
    assert(xgeImeSetTextClientOwned(provide_snapshot,NULL,NULL)==XGE_OK);
    assert(xgeImeRefreshTextClient()==XGE_OK);
    imeSnapshotSampleBegin();
    for(int i=0;i<1000;++i)assert(xgeImeRefreshTextClient()==XGE_OK);
    imeSnapshotSampleEnd(0);
    assert(xgeImeSetTextClientOwned(NULL,NULL,NULL)==XGE_OK);
}
