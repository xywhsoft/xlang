#include "test/cases/foreign_field_receiver.h"
#include <assert.h>
#include <stdio.h>
static unsigned live;
static ReceiverPayload* payload(void* raw)
{
 ReceiverPayload* p=raw;
 assert(p && p->table && p->table->depth && raw!=(void*)p->table);
 return p;
}
static int add(void* raw,int value){return payload(raw)->bias+value;}
static int read(const ReceiverPayload* raw){return payload((void*)raw)->bias;}
static size_t count(void* raw,const uint8_t* data,size_t size)
{(void)payload(raw);assert(data || !size);return size;}
static void pair(void* raw,int* first,float* second)
{*first=payload(raw)->bias;*second=3.5f;}
static int checked(void* raw,int status){(void)payload(raw);return status;}
static xstrview echo(void* raw,xstrview text){(void)payload(raw);return text;}
static int apply(void* raw,int value,int(*callback)(int,void*),void* user)
{(void)payload(raw);assert(callback);return callback(value,user);}
static bool isNull(void* raw){return raw==NULL;}
static int info(void* raw,ReceiverInfo* out)
{(void)payload(raw);*out=(ReceiverInfo){2,2};return 0;}
static int readPixels(void* raw,void* output,int stride)
{
 (void)payload(raw);assert(output && stride>=2);
 unsigned char* p=output;p[0]=1;p[1]=2;p[stride]=3;p[stride+1]=4;return 0;
}
int receiver_create(ReceiverTable** output)
{
 if(!output)return -1;
 *output=xrtCalloc(1,sizeof(**output));if(!*output)return -1;
 ReceiverTable* t=*output;
 *t=(ReceiverTable){.add=add,.read=read,.count=count,.pair=pair,.checked=checked,.echo=echo,.apply=apply,.isNull=isNull,.info=info,.readPixels=readPixels};
 t->payload=(ReceiverPayload){t,7};t->pUser=&t->payload;t->constUser=&t->payload;++live;return 0;
}
void receiver_destroy(ReceiverTable* table)
{if(table){table->closing=true;if(!table->depth){assert(live);--live;xrtFree(table);}}}
ReceiverTable* receiver_null(void){return NULL;}
int receiver_live_count(void){return (int)live;}
static bool begin(void* raw,void* token)
{ReceiverTable* t=raw;ReceiverScope* s=token;if(t->closing || s->table)return false;s->table=t;++t->depth;return true;}
static bool end(void* token)
{ReceiverScope* s=token;ReceiverTable* t=s->table;assert(t && t->depth);s->table=NULL;--t->depth;if(t->closing)receiver_destroy(t);return true;}
static bool destroy(void* raw){receiver_destroy(raw);return true;}
static bool busy(const void* raw){return ((const ReceiverTable*)raw)->depth!=0;}
const XL6_ForeignViewDescriptorV1 receiverPolicy={sizeof(receiverPolicy),XL6_FOREIGN_VIEW_V1_TAG,
 "binding.field.receiver",0,1,NULL,NULL,destroy,sizeof(ReceiverScope),_Alignof(ReceiverScope),begin,end,NULL,busy,0,NULL,NULL};
static xmemdebugsnapshot before;
void receiver_heap_begin(void)
{assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
void receiver_heap_end(void)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 printf("RECEIVER_HEAP alloc=%llu free=%llu realloc=%llu\n",(unsigned long long)(after.AllocCount-before.AllocCount),
  (unsigned long long)(after.FreeCount-before.FreeCount),(unsigned long long)(after.ReallocCount-before.ReallocCount));
 assert(after.AllocCount==before.AllocCount && after.FreeCount==before.FreeCount && after.ReallocCount==before.ReallocCount);
}
void receiver_native_samples(ReceiverTable* t,int repetitions)
{
 ReceiverScope scope={0};assert(begin(t,&scope));
 for(int i=0;i<repetitions;++i)assert(t->add(t->pUser,3)==10);
 assert(end(&scope));
}
