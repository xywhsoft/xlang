#include "test/cases/foreign_descriptor_arrays.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int entered;
static xmemdebugsnapshot baseline;
static int portSum(const DescriptorPort* ports,int count)
{ assert(count>=0 && ((!count && !ports) || (count && ports)));int sum=0;
  for(int i=0;i<count;++i){assert(ports[i].iSize==sizeof(*ports));assert(ports[i].label);sum+=(int)strlen(ports[i].label)+ports[i].number;}return sum; }
int descriptorArraysConsume(const DescriptorArrays* d)
{++entered;assert(d);return d->tag+portSum(d->inputs,d->inputCount)+portSum(d->outputs,d->outputCount);}
int descriptorArraysNested(const DescriptorNested* d)
{++entered;assert(d);return d->tag+portSum(d->ports.items,d->ports.count);}
int descriptorArraysMixed(int prefix,const DescriptorOnly* d,const int* extra,int count)
{++entered;int sum=prefix+portSum(d->items,d->count);for(int i=0;i<count;++i)sum+=extra[i];return sum;}
int descriptorArraysInspect(DescriptorReceiver* r,const DescriptorOnly* d)
{++entered;assert(r);return r->base+portSum(d->items,d->count);}
static DescriptorReceiver receiver={100,descriptorArraysInspect};
DescriptorReceiver* descriptorArraysReceiver(void){return &receiver;}
int descriptorArraysReenter(const DescriptorOnly* d,int(*callback)(void*),void* user)
{++entered;int before=portSum(d->items,d->count);assert(callback && callback(user)==0);assert(before==portSum(d->items,d->count));return before;}
int descriptorArraysCalls(void){return entered;}
void descriptorArraysBudget(int allowance)
{assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));if(allowance<0)xrtMemDebugFailClear();else assert(xrtMemDebugFailAfter((size_t)allowance));}
int descriptorArraysTriggered(void){return xrtMemDebugFailTriggered();}
void descriptorArraysMark(void){xrtMemDebugSnapshot(&baseline);}
int descriptorArraysBalanced(void)
{xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);if(after.LiveCount!=baseline.LiveCount || after.LiveBytes!=baseline.LiveBytes || after.InvalidFreeCount!=baseline.InvalidFreeCount || after.DoubleFreeCount!=baseline.DoubleFreeCount){fprintf(stderr,"unbalanced descriptor arrays: %llu/%llu -> %llu/%llu\n",(unsigned long long)baseline.LiveCount,(unsigned long long)baseline.LiveBytes,(unsigned long long)after.LiveCount,(unsigned long long)after.LiveBytes);return 0;}return 1;}

static int ownedLive;
int descriptorOwnedCreate(int value,DescriptorOwned** output)
{*output=xrtCalloc(1,sizeof(**output));if(!*output)return -1;(*output)->value=value;(*output)->refs=1;++ownedLive;return 0;}
void descriptorOwnedDestroy(DescriptorOwned* object)
{if(object){assert(object->refs>0);if(!--object->refs){assert(!object->active);--ownedLive;xrtFree(object);}}}
static bool ownedDrop(void* object){descriptorOwnedDestroy(object);return true;}
static bool ownedBegin(void* object,void* storage)
{DescriptorOwnedScope* scope=storage;scope->object=object;++scope->object->refs;++scope->object->active;return true;}
static bool ownedEnd(void* storage)
{DescriptorOwnedScope* scope=storage;DescriptorOwned* object=scope->object;assert(object && object->active);--object->active;scope->object=NULL;descriptorOwnedDestroy(object);return true;}
const XL6_ForeignViewDescriptorV1 descriptorOwnedPolicy={sizeof(descriptorOwnedPolicy),XL6_FOREIGN_VIEW_V1_TAG,"descriptor.owned",0,1,NULL,NULL,ownedDrop,sizeof(DescriptorOwnedScope),_Alignof(DescriptorOwnedScope),ownedBegin,ownedEnd,NULL,NULL,XL6_FOREIGN_VIEW_DEFER_ACTIVE_DESTROY,NULL,NULL};
int descriptorOwnedLive(void){return ownedLive;}
static struct { const uint8_t* data; size_t size; } expectedBytes[3];
void descriptorArraysRemember(int index,const uint8_t* data,size_t size)
{assert(index>=0 && index<3);expectedBytes[index].data=data;expectedBytes[index].size=size;}
static int viewSum(const DescriptorViews* d)
{assert(d && d->count>=0 && (d->items || !d->count));int sum=0;
 for(int i=0;i<d->count;++i){const DescriptorViewItem* v=&d->items[i];assert(i<3 && v->data==expectedBytes[i].data && v->length==expectedBytes[i].size);if(v->object){assert(v->object->active>0);sum+=v->object->value;}assert(v->data || !v->length);for(uint64_t b=0;b<v->length;++b)sum+=v->data[b];}return sum;}
int descriptorArraysViews(const DescriptorViews* d,int(*callback)(void*),void* user)
{++entered;int result=viewSum(d);assert(callback && callback(user)==0);assert(result==viewSum(d));return result;}
void descriptorArraysReport(int native)
{xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);printf("Descriptor array heap %s: alloc=%llu free=%llu realloc=%llu row=%zu\n",native?"native":"language",(unsigned long long)(after.AllocCount-baseline.AllocCount),(unsigned long long)(after.FreeCount-baseline.FreeCount),(unsigned long long)(after.ReallocCount-baseline.ReallocCount),sizeof(DescriptorPort));assert(descriptorArraysBalanced());}
void descriptorArraysNativeHot(void)
{DescriptorPort rows[2]={{sizeof(DescriptorPort),"a",8},{sizeof(DescriptorPort),"xyz",9}};
 DescriptorArrays d={7,rows,2,rows,2};descriptorArraysMark();for(int round=0;round<1000;++round)assert(descriptorArraysConsume(&d)==49);descriptorArraysReport(1);}

int descriptorArraysChecked(const DescriptorArrays* d)
{if(d->tag<0){++entered;return -16;}return descriptorArraysConsume(d);}
