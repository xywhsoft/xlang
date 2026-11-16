/* Actual native test producers; no product adaptation. */
#include "foreign_return_outputs.h"
#include <assert.h>
static unsigned returnOutputCalls;
int returnOutputQuery(int mode,double* value,int* count)
{
    ++returnOutputCalls;*value=12.5;*count=7;
    if(mode<0){*value=900;*count=901;return -7;}
    if(mode==2)xrtSetErrorKind(XERR_STATE);
    return mode;
}
unsigned returnOutputCount(void){return returnOutputCalls;}
int returnOutputReceiverQuery(ReturnOutputReceiver* receiver,int mode,double* value,int* count)
{if(!receiver)return -9;return returnOutputQuery(mode,value,count);}
int returnOutputReceiverCreate(ReturnOutputReceiver** output)
{
    *output=(ReturnOutputReceiver*)xrtCalloc(1,sizeof(**output));if(!*output)return -1;
    (*output)->query=returnOutputReceiverQuery;return 0;
}
void returnOutputReceiverDestroy(ReturnOutputReceiver* receiver){xrtFree(receiver);}
static bool returnOutputReceiverDrop(void* receiver)
{returnOutputReceiverDestroy((ReturnOutputReceiver*)receiver);return true;}
const XL6_ForeignViewDescriptorV1 returnOutputReceiverPolicy={
    sizeof(returnOutputReceiverPolicy),XL6_FOREIGN_VIEW_V1_TAG,"test.return.outputs",0,1,NULL,NULL,
    returnOutputReceiverDrop,0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};
int returnOutputRawQuery(int mode,double* value,int* count)
{++returnOutputCalls;*value=12.5;*count=7;return mode;}
int returnOutputMaybeQuery(int mode,double* value,int* count)
{++returnOutputCalls;*value=12.5;*count=7;return mode;}
int returnOutputSpan(const uint8* data,int size,int* count)
{++returnOutputCalls;*count=size;return size && data && data[0]=='a' ? 1:0;}
int returnOutputPacked(const ReturnOutputPack* input,int* count)
{++returnOutputCalls;*count=input->first+input->second;return input->first!=0;}
int returnOutputNoInputs(int* count)
{++returnOutputCalls;*count=19;return 0;}
static xmemdebugsnapshot returnOutputHeapBefore;
void returnOutputHeapMark(void)
{xrtMemDebugSnapshot(&returnOutputHeapBefore);assert(returnOutputHeapBefore.Enabled);}
bool returnOutputHeapCheck(void)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    return after.Enabled && after.AllocCount==returnOutputHeapBefore.AllocCount && after.FreeCount==returnOutputHeapBefore.FreeCount &&
        after.ReallocCount==returnOutputHeapBefore.ReallocCount && after.LiveCount==returnOutputHeapBefore.LiveCount &&
        after.LiveBytes==returnOutputHeapBefore.LiveBytes;
}
bool returnOutputHeapBaseline(void)
{
    returnOutputHeapMark();
    for(int i=0;i<1000;++i){double value=0;int count=0;
        if(returnOutputQuery(0,&value,&count)!=0 || value!=12.5 || count!=7)return false;}
    return returnOutputHeapCheck();
}
