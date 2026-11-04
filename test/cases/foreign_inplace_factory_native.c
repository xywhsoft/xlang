#include "foreign_inplace_factory.h"
#include <assert.h>
static int live;
int inplace_plain_init(PlainRecord* record,int mode)
{
    assert(record && !record->value && !record->payload);
    if(mode==2)return -2;
    record->payload=xrtMalloc(1);if(!record->payload)return -1;
    record->value=42;++live;return mode==1 ? -1:0;
}
void inplace_plain_free(PlainRecord* record)
{if(record->payload){xrtFree(record->payload);--live;}*record=(PlainRecord){0};}
int inplace_plain_read(PlainRecord* record){assert(record && record->payload);return record->value;}
static bool destroy_plain(void* raw){inplace_plain_free(raw);xrtFree(raw);return true;}
const XL6_ForeignViewDescriptorV1 inplacePlainPolicy={sizeof(inplacePlainPolicy),XL6_FOREIGN_VIEW_V1_TAG,"inplace.plain",0,1,NULL,NULL,destroy_plain,0,1,NULL,NULL,NULL,NULL,0,destroy_plain,NULL};
int inplace_init(InplaceRecord* record,int mode,xstrview text)
{
    assert(record && !record->refs && !record->payload && !record->adopted);
    if(mode==2)return -2;
    record->payload=xrtMalloc(1);if(!record->payload)return -1;
    record->refs=1;record->mode=mode;record->value=(int)text.Size;++live;
    if(mode==7){inplace_free(record);return -7;}
    return mode==1 ? -1:0;
}
int inplace_adopt(InplaceRecord* record)
{
    assert(record && record->refs==1 && !record->adopted);
    if(record->mode==3)return -3;
    if(record->mode==6){(void)xrtMemDebugFailAfter(0);xrtSetErrorKind(XERR_STATE);return -6;}
    record->adopted=true;
    if(record->mode==4)(void)xrtMemDebugFailAfter(0);
    if(record->mode==5)xrtSetErrorKind(XERR_STATE);
    return 0;
}
void inplace_free(InplaceRecord* record)
{
    if(!record)return;
    if(record->refs>1){--record->refs;return;}
    if(record->payload){xrtFree(record->payload);record->payload=NULL;--live;}
    bool adopted=record->adopted;*record=(InplaceRecord){0};
    if(adopted)xrtFree(record);
}
int inplace_duplicate(InplaceRecord* record,InplaceRecord** output)
{assert(record && record->refs && record->adopted && output);++record->refs;*output=record;return 0;}
int inplace_from(InplaceRecord* source,InplaceRecord* output)
{assert(source && source->value<=10);return inplace_init(output,0,(xstrview){"0123456789",(size_t)source->value});}
int inplace_read(InplaceRecord* record){assert(record && record->refs && record->payload && record->adopted);return record->value;}
int inplace_live(void){return live;}
void inplace_fail_after(int count){(void)xrtMemDebugFailAfter(count);}
void inplace_reset(void){(void)xrtMemDebugFailAfter(-1);}
static bool destroy(void* raw){inplace_free(raw);return true;}
static bool destroy_unadopted(void* raw)
{
    InplaceRecord* record=raw;
    if(record->adopted || record->refs>1){xrtSetErrorKind(XERR_STATE);return false;}
    inplace_free(record);xrtFree(record);return true;
}
const XL6_ForeignViewDescriptorV1 inplacePolicy={sizeof(inplacePolicy),XL6_FOREIGN_VIEW_V1_TAG,"inplace.record",0,1,NULL,NULL,destroy,0,1,NULL,NULL,NULL,NULL,0,destroy_unadopted,NULL};
