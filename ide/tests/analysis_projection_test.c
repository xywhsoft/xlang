#include "analysis/projection.h"
#include "analysis/worker.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
static xatomic32 created,destroyed;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_projection_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
typedef struct payload {unsigned value;xls_projection_store* observe;} payload;
static void destroy(void* value)
{payload* p=value;if(p->observe){xls_projection_stats s;xlsProjectionStoreStats(p->observe,&s);}xrtAtomic32FetchAdd(&destroyed,1,XMEMORY_RELAXED);xrtFree(p);}
static xls_projection* make(xls_semantic_check* owner,const void* key,size_t size,unsigned number,xls_projection_store* observe)
{payload* p=xrtCalloc(1,sizeof(*p));if(!p)return NULL;xrtAtomic32FetchAdd(&created,1,XMEMORY_RELAXED);*p=(payload){number,observe};xls_projection* result=xlsProjectionCreate(owner,key,size,p,sizeof(*p),destroy);if(!result)destroy(p);return result;}
typedef struct reader {xls_projection_store* store;xls_semantic_check* owner;int okay;} reader;
static int32 share(void* user)
{
 reader* r=user;r->okay=1;const char key[]="parallel\0fullkey";
 for(unsigned i=0;i<32;++i){xls_projection* p=make(r->owner,key,sizeof(key),77,NULL);if(!p){r->okay=0;break;}p=xlsProjectionPublish(r->store,p);xls_projection* hit=xlsProjectionAcquire(r->store,r->owner,key,sizeof(key));if(!p || !hit || ((payload*)xlsProjectionValue(p))->value!=77 || xlsProjectionValue(p)!=xlsProjectionValue(hit))r->okay=0;xlsProjectionRelease(hit);xlsProjectionRelease(p);}
 return 0;
}
static int run(void)
{
 char* path=xrtPathAbs("projection-owner.xl");const char* text="func main(...) as int\n return 0\nend\n";xls_semantic_input input={path,text,strlen(text)};xls_semantic_check *a=xlsSemanticCheckCreate(path,&input,1,1,NULL,NULL,NULL,0),*b=xlsSemanticCheckCreate(path,&input,1,1,NULL,NULL,NULL,0);CHECK(a && b && a!=b && xlsSemanticCheckInfo(a).hasIndex && xlsSemanticCheckInfo(b).hasIndex);
 xls_projection_store* store=xlsProjectionStoreCreate(4096);CHECK(store && !xlsProjectionStoreCreate(0) && !xlsProjectionStoreCreate(SIZE_MAX));const unsigned char key[]={1,0,2},different[]={1,0,3};CHECK(!xlsProjectionAcquire(store,a,key,sizeof(key)));
 xls_projection* p=make(a,key,sizeof(key),7,NULL);CHECK(p);p=xlsProjectionPublish(store,p);p=xlsProjectionPublish(store,p);xls_projection* held=xlsProjectionAcquire(store,a,key,sizeof(key));CHECK(held && xlsProjectionValue(p)==xlsProjectionValue(held) && ((payload*)xlsProjectionValue(held))->value==7);xlsProjectionRelease(p);
 CHECK(!xlsProjectionAcquire(store,a,different,sizeof(different)) && !xlsProjectionAcquire(store,b,key,sizeof(key)));
 unsigned before=xrtAtomic32Load(&destroyed,XMEMORY_ACQUIRE);p=make(a,key,sizeof(key),7,store);CHECK(p);p=xlsProjectionPublish(store,p);CHECK(p==held && xrtAtomic32Load(&destroyed,XMEMORY_ACQUIRE)==before+1);xlsProjectionRelease(p);
 p=make(b,key,sizeof(key),9,store);CHECK(p);p=xlsProjectionPublish(store,p);CHECK(((payload*)xlsProjectionValue(p))->value==9 && ((payload*)xlsProjectionValue(held))->value==7);xlsProjectionRelease(p);
 p=make(a,different,sizeof(different),11,NULL);CHECK(p);before=xrtAtomic32Load(&destroyed,XMEMORY_ACQUIRE);p=xlsProjectionPublish(store,p);CHECK(xrtAtomic32Load(&destroyed,XMEMORY_ACQUIRE)==before+1);xlsProjectionRelease(p);
 xls_projection_stats stats;xlsProjectionStoreStats(store,&stats);CHECK(stats.entries==1 && stats.bytes<=stats.limit && stats.published==3 && stats.replaced==2 && stats.hits==1 && stats.misses==3);
 xls_projection_store* small=xlsProjectionStoreCreate(1);CHECK(small);p=make(a,key,sizeof(key),12,NULL);CHECK(p);p=xlsProjectionPublish(small,p);CHECK(p && !xlsProjectionAcquire(small,a,key,sizeof(key)));xlsProjectionStoreStats(small,&stats);CHECK(!stats.entries && !stats.bytes && stats.declined==1);xlsProjectionRelease(p);xlsProjectionStoreDestroy(small);
 payload* value=xrtCalloc(1,sizeof(*value));CHECK(value);xrtAtomic32FetchAdd(&created,1,XMEMORY_RELAXED);CHECK(!xlsProjectionCreate(a,key,0,value,1,destroy) && !xlsProjectionCreate(a,key,XLS_PROJECTION_KEY_BYTES+1u,value,1,destroy) && !xlsProjectionCreate(a,key,sizeof(key),value,XLS_PROJECTION_VALUE_BYTES+1u,destroy));destroy(value);
 xls_projection_store* concurrent=xlsProjectionStoreCreate(4096);CHECK(concurrent);reader readers[4];xthread* threads[4];for(size_t i=0;i<4;++i){readers[i]=(reader){concurrent,a,0};threads[i]=xrtThreadCreate(share,&readers[i],0);CHECK(threads[i]);}for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}xlsProjectionStoreStats(concurrent,&stats);CHECK(stats.entries==1 && stats.published==1 && !stats.replaced && stats.hits==128);xlsProjectionStoreDestroy(concurrent);
 xlsSemanticCheckDestroy(a);xlsSemanticCheckDestroy(b);xlsProjectionStoreDestroy(store);CHECK(((payload*)xlsProjectionValue(held))->value==7 && xlsSemanticCheckInfo(a).hasIndex);xlsProjectionRelease(held);xrtFree(path);
 /* A resident never makes a known-invalid check current again. */
 path=xrtPathAbs("build/projection-disk.xl");CHECK(path && xrtDirCreateAll("build") && xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)}));a=xlsSemanticCheckCreate(path,NULL,0,1,NULL,NULL,NULL,0);CHECK(a && xlsSemanticCheckInfo(a).accepted);store=xlsProjectionStoreCreate(4096);CHECK(store);p=make(a,key,sizeof(key),17,NULL);CHECK(p);p=xlsProjectionPublish(store,p);CHECK(xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)}) && !xlsSemanticCheckValidate(a));CHECK(!xlsProjectionAcquire(store,a,key,sizeof(key)));p=xlsProjectionPublish(store,p);CHECK(p && ((payload*)xlsProjectionValue(p))->value==17);xlsProjectionRelease(p);xlsProjectionStoreDestroy(store);xlsSemanticCheckDestroy(a);xrtFree(path);
 xls_worker* worker=xlsWorkerCreate(NULL,4);CHECK(worker && xlsWorkerProjectionStore(worker));xlsWorkerDestroy(worker);CHECK(xrtAtomic32Load(&created,XMEMORY_ACQUIRE)==135 && xrtAtomic32Load(&created,XMEMORY_ACQUIRE)==xrtAtomic32Load(&destroyed,XMEMORY_ACQUIRE));return 1;
}
int main(void){if(!run())return 1;printf("analysis_projection_test passed: %u checks;exact owners/fullbinarykeys,bounded single resident,refusal/replacement,destructor reentry,four concurrent publishers,independent lease lifetime and known-invalid rejection\n",checks);return 0;}
