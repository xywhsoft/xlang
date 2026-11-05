#include "test/cases/foreign_output_record_size.h"
#include <assert.h>
static int alive,producerCalls;
int output_record_hold(OutputRecordNode* node)
{
    assert(node && node->refs>0);++node->holds;
    if(node->mode==1 && node->holds==2)return -9;
    ++node->refs;return 0;
}
int output_record_drop(OutputRecordNode* node)
{
    assert(node && node->refs>0);
    if(!--node->refs){--alive;xrtFree(node);}
    return 0;
}
static bool destroy(void* raw){return output_record_drop(raw)==0;}
const XL6_ForeignViewDescriptorV1 outputRecordPolicy={
    sizeof(outputRecordPolicy),XL6_FOREIGN_VIEW_V1_TAG,"fixture.outputRecord",0,1,NULL,NULL,destroy,
    0,1,NULL,NULL,NULL,NULL,0,NULL,NULL
};
int output_record_create(OutputRecordNode** out,int mode)
{
    *out=xrtCalloc(1,sizeof(**out));if(!*out)return -1;
    **out=(OutputRecordNode){1,87,mode,0};++alive;return 0;
}
int output_record_read(OutputRecordNode* node,OutputRecordNative* out)
{
    assert(node && node->refs>0 && out && out->iSize==sizeof(*out));++producerCalls;
    if(node->mode==6){out->first=(OutputRecordNode*)(uintptr_t)1;out->second=(OutputRecordNode*)(uintptr_t)1;return -23;}
    *out=(OutputRecordNative){sizeof(*out),73,node->mode==5 ? NULL:node,{1.25f,2.5f},node->mode==3 ? NULL:node,node->mode};
    if(node->mode==4)return -7;
    if(node->mode==2)assert(xrtMemDebugFailAfter(0));
    return 0;
}
void output_record_read_void(OutputRecordNode* node,OutputRecordNative* out)
{assert(output_record_read(node,out)==0);}
int output_record_read_pod(OutputRecordNode* node,OutputRecordNative* out)
{return output_record_read(node,out);}
int output_record_read_one(OutputRecordNode* node,OutputRecordNative* out)
{return output_record_read(node,out);}
int output_record_read_owned(OutputRecordNode* node,OutputRecordNative* out)
{return output_record_read(node,out);}
int output_record_refs(OutputRecordNode* node){assert(node);return node->refs;}
int output_record_live(void){return alive;}
int output_record_producers(void){return producerCalls;}
void output_record_fail_after(int count){assert(xrtMemDebugFailAfter(count));}
void output_record_reset(void){assert(xrtMemDebugFailAfter(-1));xrtClearError();}
