#include "services/language_client.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
static xatomic32* watched;static xide_language_client* watchedClient;static size_t observed,queueWasEmpty;
static int emptyQueue(void);
static bool observeAdmission(xatomic32* p,uint32* old,uint32 value,xmemoryorder success,xmemoryorder failure)
{if(p==watched){++observed;if(emptyQueue())++queueWasEmpty;}return xrtAtomic32CompareExchange(p,old,value,success,failure);}
#define xrtAtomic32CompareExchange observeAdmission
#include "../src/services/language_client.c"
#undef xrtAtomic32CompareExchange
static int emptyQueue(void){return watchedClient&&!watchedClient->sendHead;}
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"client-binding:%d:%s\n",__LINE__,#v);return 1;}}while(0)
typedef struct tickets {uint64* values;} tickets;
static int32 allocateTickets(ptr raw){tickets* t=raw;for(size_t i=0;i<256;++i)t->values[i]=nextConnection();return 0;}
static int order(const void* a,const void* b){uint64 x=*(const uint64*)a,y=*(const uint64*)b;return(x>y)-(x<y);}
int main(void)
{
 CHECK(!xideLanguageClientIdentity(NULL));xrtAtomic64Store(&connectionSerial,UINT64_MAX-1,XMEMORY_RELAXED);CHECK(nextConnection()==UINT64_MAX&&!nextConnection()&&!nextConnection());xide_model_error error={0};xide_language_client_spec spec={.program="C:/no-program-needed.exe",.directory="C:/"};CHECK(!xideLanguageClientStartProcess(&spec,&error)&&error.code);
 xrtAtomic64Store(&connectionSerial,0,XMEMORY_RELAXED);uint64 values[1024];tickets groups[4];xthread* threads[4];for(size_t i=0;i<4;++i){groups[i].values=values+i*256;threads[i]=xrtThreadCreate(allocateTickets,groups+i,0);CHECK(threads[i]);}for(size_t i=0;i<4;++i){CHECK(xrtThreadWaitFor(threads[i],XIDE_XRT_TIMEOUT_MS(10000))==XWAIT_OK);xrtThreadDestroy(threads[i]);}qsort(values,1024,sizeof(uint64),order);for(size_t i=0;i<1024;++i)CHECK(values[i]==i+1);
 xide_language_client client={.identity=77};InitializeCriticalSection(&client.lock);watchedClient=&client;xatomic32 admitted=XRT_ATOMIC32_INIT(0);watched=&admitted;xvalue* params=xrtValueObject();CHECK(params);xide_language_batch* batch=xideLanguageBatchCreate();CHECK(batch&&xideLanguageBatchAppendRequest(batch,"test/request",params,7));node* original=batch->head;
 CHECK(!xideLanguageBatchSendBound(&client,batch,78,&admitted)&&!observed&&!client.sendHead&&!xrtAtomic32Load(&admitted,XMEMORY_ACQUIRE));
 CHECK(!xideLanguageBatchSendBound(&client,batch,0,&admitted)&&!xideLanguageBatchSendBound(&client,batch,77,NULL));
 client.lease.token=1;CHECK(!xideLanguageBatchSendBound(&client,batch,77,&admitted)&&!observed);client.lease.token=0;
 client.stop=1;CHECK(!xideLanguageBatchSendBound(&client,batch,77,&admitted)&&!observed);client.stop=0;
 client.sendCount=QUEUE_COUNT;CHECK(!xideLanguageBatchSendBound(&client,batch,77,&admitted)&&!observed);client.sendCount=0;client.sendBytes=QUEUE_BYTES;CHECK(!xideLanguageBatchSendBound(&client,batch,77,&admitted)&&!observed);client.sendBytes=0;
 xide_language_batch* empty=xideLanguageBatchCreate();CHECK(empty&&!xideLanguageBatchSendBound(&client,empty,77,&admitted)&&!observed);xideLanguageBatchDestroy(empty);
 CHECK(xideLanguageBatchSendBound(&client,batch,77,&admitted)&&observed==1&&queueWasEmpty==1&&client.sendHead==original&&client.sendCount==1&&xrtAtomic32Load(&admitted,XMEMORY_ACQUIRE)==1&&xideLanguageClientIdentity(&client)==77);
 batch=xideLanguageBatchCreate();CHECK(batch&&xideLanguageBatchAppendRequest(batch,"test/request",params,8));CHECK(!xideLanguageBatchSendBound(&client,batch,77,&admitted)&&client.sendCount==1&&client.sendHead==original&&xrtAtomic32Load(&admitted,XMEMORY_ACQUIRE)==1);xideLanguageBatchDestroy(batch);xrtValueRelease(params);clear(client.sendHead);DeleteCriticalSection(&client.lock);
 printf("PASS client connection binding: %u checks; 1024 concurrent unique identities, saturation, actual queue ownership, failure prefixes and admission flag before queue exposure\n",checks);return 0;
}
