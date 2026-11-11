/* Native descriptor consumers, not adapters for flat callback functions. */
#include "foreign_pack_callback.h"
#include <assert.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif
static int live,tailCalls;
static int sum(const PcDesc* d)
{
 assert(d && d->size==sizeof(*d) && d->count>=0 && d->count<=1000);
 assert((d->callback==NULL)==(d->user==NULL));int value=0;
 for(int i=0;i<d->count;++i)value+=d->callback ? d->callback(d->seed+i,d->user):d->seed+i;
 return value;
}
int pc_call(const PcDesc* d,int* output){assert(output);*output=sum(d);return 0;}
int pc_scalar(const PcDesc* d){return sum(d);}
int pc_pair(const PcPair* d)
{
 assert(d && d->right.size==sizeof(d->right) && d->right.count>=0 && d->right.count<=1000);
 assert((d->right.callback==NULL)==(d->right.user==NULL));int value=sum(&d->left);
 for(int i=0;i<d->right.count;++i)value+=d->right.callback ? d->right.callback(d->right.user,d->right.seed+i):d->right.seed+i;
 return value;
}
int pc_two(const PcDesc* left,const PcFirst* right)
{
 assert(right && right->size==sizeof(*right) && right->count>=0 && right->count<=1000);
 assert((right->callback==NULL)==(right->user==NULL));int value=sum(left);
 for(int i=0;i<right->count;++i)value+=right->callback ? right->callback(right->user,right->seed+i):right->seed+i;
 return value;
}
void pc_action(const PcAction* d)
{assert(d && d->count>=0 && d->count<=1000 && ((d->callback==NULL)==(d->user==NULL)));for(int i=0;i<d->count;++i)if(d->callback)d->callback(d->user);}
int pc_event(const PcEventDesc* d)
{assert(d && ((d->callback==NULL)==(d->user==NULL)));PcEvent event={d->seed};return d->callback ? d->callback(&event,d->user):0;}
#if defined(_WIN32)
static DWORD WINAPI worker(void* data){(void)sum(data);return 0;}
#else
static void* worker(void* data){(void)sum(data);return NULL;}
#endif
int pc_worker(const PcDesc* d)
{
#if defined(_WIN32)
 HANDLE thread=CreateThread(NULL,0,worker,(void*)d,0,NULL);assert(thread);assert(WaitForSingleObject(thread,INFINITE)==WAIT_OBJECT_0);assert(CloseHandle(thread));
#else
 pthread_t thread;assert(!pthread_create(&thread,NULL,worker,(void*)d));assert(!pthread_join(thread,NULL));
#endif
 return 0;
}
static int apply(PcRoot* root,const PcDesc* d,int* output)
{assert(root && output);int value=sum(d);*output=root->value+value;return 0;}
int pc_make(const PcDesc* d,PcRoot** output)
{
 assert(output);*output=NULL;int value=sum(d);PcRoot* root=xrtCalloc(1,sizeof(*root));if(!root)return -9;
 root->value=value;root->apply=apply;++live;*output=root;
 return 0;
}
void pc_drop(PcRoot* root)
{
 if(!root)return;
 --live;xrtFree(root);
}
int pc_value(PcRoot* root){assert(root);return root->value;}
int pc_live(void){return live;}
void pc_heap(size_t* count,size_t* bytes)
{xmemdebugsnapshot info;memset(&info,0,sizeof(info));xrtMemDebugSnapshot(&info);*count=info.LiveCount;*bytes=info.LiveBytes;}
void pc_receipt(PcReceipt* record,const PcDesc* d)
{
 assert(record && !record->payload);(void)sum(d);
 record->payload=xrtMalloc(1);if(record->payload)++live;
 record->callback=d->callback;record->user=d->user;
}
int pc_receipt_adopt(PcReceipt* record)
{
 assert(record && record->payload);int (*callback)(int,void*)=record->callback;void* user=record->user;
 record->callback=NULL;record->user=NULL;
 /* Deliberately attempt after the producer returned, during adoption while
  * the invocation stack still exists. Completed must reject without entering
  * captured state. No callback/user survives adoption or rollback. */
 if(callback){assert(callback(999,user)==-19);++tailCalls;}
 return -29;
}
void pc_receipt_rollback(PcReceipt* record)
{if(record->payload){xrtFree(record->payload);--live;}memset(record,0,sizeof(*record));}
int pc_tail_calls(void){return tailCalls;}
static bool destroy(void* raw){pc_drop(raw);return true;}
const XL6_ForeignViewDescriptorV1 pcPolicy={sizeof(pcPolicy),XL6_FOREIGN_VIEW_V1_TAG,"pc.root",0,1,NULL,NULL,destroy,0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};
static bool destroyReceipt(void* raw){pc_receipt_rollback(raw);xrtFree(raw);return true;}
const XL6_ForeignViewDescriptorV1 pcReceiptPolicy={sizeof(pcReceiptPolicy),XL6_FOREIGN_VIEW_V1_TAG,"pc.receipt",0,1,NULL,NULL,destroyReceipt,0,1,NULL,NULL,NULL,NULL,0,destroyReceipt,NULL};
