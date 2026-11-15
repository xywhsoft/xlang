#include "foreign_xui_document_streaming.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static xmemdebugsnapshot streamBefore;
void fixtureStreamBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&streamBefore);}
void fixtureStreamBudget(int count)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));if(count<0)xrtMemDebugFailClear();else assert(xrtMemDebugFailAfter(count));}
void fixtureStreamEndBudget(void)
{xrtMemDebugFailClear();xrtClearError();}
void fixtureStreamBalanced(void)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==streamBefore.LiveCount&&after.LiveBytes==streamBefore.LiveBytes);
    assert(after.InvalidFreeCount==streamBefore.InvalidFreeCount&&after.DoubleFreeCount==streamBefore.DoubleFreeCount);
}
void fixtureStreamReport(int native)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    printf("Document stream info heap %s: alloc=%llu free=%llu realloc=%llu, record=%zu\n",native?"native":"language",
        (unsigned long long)(after.AllocCount-streamBefore.AllocCount),(unsigned long long)(after.FreeCount-streamBefore.FreeCount),
        (unsigned long long)(after.ReallocCount-streamBefore.ReallocCount),sizeof(xui_doc_prepare_info_t));
}
void fixtureStreamNativeHot(xui_document_prepare prepare)
{
    for(int i=0;i<1000;++i){xui_doc_prepare_info_t value;memset(&value,0,sizeof(value));value.iSize=sizeof(value);
        assert(xuiDocumentPrepareGetInfo(prepare,&value)==XUI_OK&&value.iBufferedUtf8Bytes==0&&value.iSourceBytes==19);}
}
