#include "analysis/projection.h"
#include <xrt.h>
#include <string.h>
struct xls_projection {xatomic32 references;xls_semantic_check* owner;void* value;void (*destroy)(void*);size_t size,bytes;unsigned char key[];};
struct xls_projection_store {xmutex lock;xls_projection* resident;xls_projection_stats stats;};
static void increment(uint64_t* n){if(*n<INT64_MAX)++*n;}
static int usable(xls_semantic_check* owner)
{xls_check_info info=xlsSemanticCheckInfo(owner);return info.accepted && info.hasIndex;}
static int same(const xls_projection* p,xls_semantic_check* owner,const void* key,size_t size)
{return p && p->owner==owner && p->size==size && !memcmp(p->key,key,size);}
xls_projection_store* xlsProjectionStoreCreate(size_t bytes)
{
 if(!bytes || bytes>128u*1024u*1024u)return NULL;
 xls_projection_store* s=xrtCalloc(1,sizeof(*s));if(!s)return NULL;
 if(!xrtMutexInit(&s->lock)){xrtFree(s);return NULL;}s->stats.limit=bytes;return s;
}
xls_projection* xlsProjectionRetain(xls_projection* p){if(p)xrtAtomic32FetchAdd(&p->references,1,XMEMORY_RELAXED);return p;}
void xlsProjectionRelease(xls_projection* p)
{if(p && xrtAtomic32FetchSub(&p->references,1,XMEMORY_ACQ_REL)==1){p->destroy(p->value);xlsSemanticCheckDestroy(p->owner);xrtFree(p);}}
void xlsProjectionStoreDestroy(xls_projection_store* s)
{
 if(!s)return;
 xrtMutexLock(&s->lock);xls_projection* p=s->resident;s->resident=NULL;s->stats.entries=s->stats.bytes=0;xrtMutexUnlock(&s->lock);
 /* Destructors may observe/reenter this store;its lock is still usable. */
 xlsProjectionRelease(p);xrtMutexUnit(&s->lock);xrtFree(s);
}
xls_projection* xlsProjectionCreate(xls_semantic_check* owner,const void* key,size_t size,void* value,size_t knownBytes,void (*destroy)(void*))
{
 if(!owner || !key || !size || size>XLS_PROJECTION_KEY_BYTES || !value || !knownBytes || knownBytes>XLS_PROJECTION_VALUE_BYTES || !destroy || !usable(owner))return NULL;
 xls_projection* p=xrtCalloc(1,sizeof(*p)+size);if(!p)return NULL;xrtAtomic32Init(&p->references,1);p->owner=xlsSemanticCheckRetain(owner);p->size=size;p->bytes=sizeof(*p)+size+knownBytes;p->value=value;p->destroy=destroy;memcpy(p->key,key,size);return p;
}
xls_projection* xlsProjectionAcquire(xls_projection_store* s,xls_semantic_check* owner,const void* key,size_t size)
{
 if(!s || !owner || !key || !size || size>XLS_PROJECTION_KEY_BYTES)return NULL;
 int valid=usable(owner);xrtMutexLock(&s->lock);xls_projection* p=valid && same(s->resident,owner,key,size) ? xlsProjectionRetain(s->resident) : NULL;
 increment(p ? &s->stats.hits : &s->stats.misses);xrtMutexUnlock(&s->lock);return p;
}
xls_projection* xlsProjectionPublish(xls_projection_store* s,xls_projection* p)
{
 if(!s || !p)return p;
 int valid=usable(p->owner),winner=0;xls_projection* old=NULL,*result=p;
 xrtMutexLock(&s->lock);
 if(valid && same(s->resident,p->owner,p->key,p->size)){result=xlsProjectionRetain(s->resident);winner=1;}
 else if(valid && p->bytes<=s->stats.limit){old=s->resident;s->resident=xlsProjectionRetain(p);s->stats.entries=1;s->stats.bytes=p->bytes;increment(&s->stats.published);if(old)increment(&s->stats.replaced);}
 else increment(&s->stats.declined);
 xrtMutexUnlock(&s->lock);
 if(winner)xlsProjectionRelease(p);
 xlsProjectionRelease(old);return result;
}
void* xlsProjectionValue(const xls_projection* p){return p ? p->value : NULL;}
void xlsProjectionStoreStats(xls_projection_store* s,xls_projection_stats* stats)
{if(stats){*stats=(xls_projection_stats){0};if(s){xrtMutexLock(&s->lock);*stats=s->stats;xrtMutexUnlock(&s->lock);}}}
