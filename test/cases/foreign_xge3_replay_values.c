#include "foreign_xge3_replay_values.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static xmemdebugsnapshot replay_before;
static volatile int replay_sink;
xge_touch_event_t fixtureReplayTouch(xge_touch_event_t value)
{assert(value.iCount==8 && value.arrPoints[7].iId==7);value.arrPoints[7].fX+=1;return value;}
int fixtureReplayCategory(const xui_property_grid_category_t* in,xui_property_grid_category_t* out)
{assert(in && out && in->sId[63]=='Z' && in->sName[95]=='Y');*out=*in;out->iPropertyCount++;return 0;}
int fixtureReplaySearch(const xui_code_search_result_t* in,xui_code_search_result_t* out)
{assert(in && out && in->iCaptureCount==10);*out=*in;out->iSize=sizeof(*out);out->arrCaptures[9].iEnd++;return 0;}
int fixtureReplayFind(const xui_find_result_t* in,xui_find_result_t* out)
{assert(in && out);*out=*in;out->iSize=sizeof(*out);out->iEnd++;out->arrCaptures[9].iEnd++;return in->iStart<0 ? 73:0;}
xui_code_document_batch_layout_t fixtureReplayLayout(xui_code_document_batch_layout_t value)
{assert(value.iCapture==UINT64_C(0x100000001) && value.iDocuments==3);value.iSize=sizeof(value);value.iDocuments++;return value;}
int fixtureReplaySource(const xui_code_document_batch_source_t* in,xui_code_document_batch_source_t* out)
{assert(in && out && in->iUndoCount==4);*out=*in;out->iSize=sizeof(*out);out->iLength++;return 0;}
void fixtureReplayBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&replay_before);}
void fixtureReplayReport(int native)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    printf("Replay value heap %s: alloc=%llu free=%llu realloc=%llu\n",native?"native":"language",
        (unsigned long long)(after.AllocCount-replay_before.AllocCount),
        (unsigned long long)(after.FreeCount-replay_before.FreeCount),
        (unsigned long long)(after.ReallocCount-replay_before.ReallocCount));
}
void fixtureReplayConsume(int value){replay_sink=value;}
void fixtureReplayNativeLoop(void)
{
    xui_find_result_t value;memset(&value,0,sizeof(value));value.iStart=10;value.iEnd=20;value.iCaptureCount=10;value.arrCaptures[9].iEnd=19;
    for(int i=0;i<1000;i++){xui_find_result_t output;assert(fixtureReplayFind(&value,&output)==0);fixtureReplayConsume(output.arrCaptures[9].iEnd);}
}
void fixtureReplayBudget(int n)
{if(n<0)xrtMemDebugFailClear();else assert(xrtMemDebugFailAfter(n));}
