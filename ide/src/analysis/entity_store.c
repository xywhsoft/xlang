#include "analysis/entity_store.h"
#include <xrt.h>
#include <string.h>
struct xls_entity_domain {struct xls_entity_domain* next;xls_entity_store* owner;xls_context* context;uint64_t identity;size_t bytes;};
typedef struct domain_bucket {xavlnode tree;uint64_t hash;xls_entity_domain* first;} domain_bucket;
typedef struct entity_key {uint64_t domain,module;const void* key;size_t size;} entity_key;
typedef struct entity_record {xavlnode tree;uint64_t identity,domain,module;size_t size;unsigned char key[];} entity_record;
struct xls_entity_store {xatomic32 references;xmutex lock;xavl domains,entities;xls_entity_limits limits;xls_entity_stats stats;uint64_t nextDomain,nextEntity;};
static int domainCompare(const void* key,const xavlnode* node,void* user)
{(void)user;uint64_t a=*(const uint64_t*)key,b=((const domain_bucket*)node)->hash;return (a>b)-(a<b);}
static int entityCompare(const void* key,const xavlnode* node,void* user)
{
 (void)user;const entity_key* a=key;const entity_record* b=(const entity_record*)node;
 if(a->domain!=b->domain)return (a->domain>b->domain)-(a->domain<b->domain);
 if(a->module!=b->module)return (a->module>b->module)-(a->module<b->module);
 if(a->size!=b->size)return (a->size>b->size)-(a->size<b->size);
 return memcmp(a->key,b->key,a->size);
}
static void increment(uint64_t* n){if(*n<INT64_MAX)++*n;}
xls_entity_store* xlsEntityStoreCreate(const xls_entity_limits* limits)
{
 xls_entity_limits chosen=limits ? *limits : (xls_entity_limits){1024,262144,65536,64u*1024u*1024u};
 if(!chosen.domains || chosen.domains>65536 || !chosen.entities || chosen.entities>1048576 || !chosen.keyBytes || chosen.keyBytes>262144 || chosen.bytes<sizeof(xls_entity_store) || chosen.bytes>SIZE_MAX/2)return NULL;
 xls_entity_store* s=xrtCalloc(1,sizeof(*s));if(!s)return NULL;
 if(!xrtMutexInit(&s->lock)){xrtFree(s);return NULL;}
 if(!xrtAVLInit(&s->domains) || !xrtAVLInit(&s->entities)){xrtMutexUnit(&s->lock);xrtFree(s);return NULL;}
 xrtAtomic32Init(&s->references,1);s->limits=chosen;s->stats.bytes=sizeof(*s);s->stats.limit=chosen.bytes;return s;
}
xls_entity_store* xlsEntityStoreRetain(xls_entity_store* s){if(s)xrtAtomic32FetchAdd(&s->references,1,XMEMORY_RELAXED);return s;}
void xlsEntityStoreRelease(xls_entity_store* s)
{
 if(!s || xrtAtomic32FetchSub(&s->references,1,XMEMORY_ACQ_REL)!=1)return;
 while(s->entities.Root){entity_record* r=(entity_record*)s->entities.Root;entity_key key={r->domain,r->module,r->key,r->size};xrtAVLRemove(&s->entities,&key,entityCompare,NULL);xrtFree(r);}
 while(s->domains.Root){domain_bucket* bucket=(domain_bucket*)s->domains.Root;xrtAVLRemove(&s->domains,&bucket->hash,domainCompare,NULL);xls_entity_domain* domain=bucket->first;while(domain){xls_entity_domain* next=domain->next;xlsContextRelease(domain->context);xrtFree(domain);domain=next;}xrtFree(bucket);}
 xrtMutexUnit(&s->lock);xrtFree(s);
}
const xls_entity_domain* xlsEntityStoreDomain(xls_entity_store* s,xls_context* context)
{
 if(!s || !context)return NULL;
 uint64_t hash=xlsContextHash(context);domain_bucket* freshBucket=NULL;xls_entity_domain* fresh=NULL;const xls_entity_domain* result=NULL;
 for(;;){
  xrtMutexLock(&s->lock);domain_bucket* bucket=(domain_bucket*)xrtAVLFind(&s->domains,&hash,domainCompare,NULL);xls_entity_domain* first=bucket ? bucket->first : NULL;xrtMutexUnlock(&s->lock);
  /* Published lists are immutable except their locked head. Records remain
   * alive while retaining store;full canonical context comparisons take no lock. */
  size_t collisionCount=0;
  for(xls_entity_domain* d=first;d;d=d->next){if(xlsContextEqual(d->context,context)){result=d;break;}if(++collisionCount>=64)break;}
  if(result || collisionCount>=64)break;
  if(!fresh){fresh=xrtCalloc(1,sizeof(*fresh));if(!fresh)break;fresh->owner=s;fresh->context=xlsContextRetain(context);fresh->bytes=sizeof(*fresh)+xlsContextBytes(context);}
  if(!bucket && !freshBucket){freshBucket=xrtCalloc(1,sizeof(*freshBucket));if(!freshBucket)break;freshBucket->hash=hash;}
  xrtMutexLock(&s->lock);bucket=(domain_bucket*)xrtAVLFind(&s->domains,&hash,domainCompare,NULL);
  if((bucket ? bucket->first : NULL)!=first){xrtMutexUnlock(&s->lock);continue;}
  size_t charge=fresh->bytes+(bucket ? 0 : sizeof(*freshBucket));
  if(s->stats.domains>=s->limits.domains || s->nextDomain==UINT64_MAX || charge>s->limits.bytes-s->stats.bytes){xrtMutexUnlock(&s->lock);break;}
  if(!bucket){bool added=false;xavlnode* actual=xrtAVLInsert(&s->domains,&freshBucket->tree,&hash,domainCompare,NULL,&added);if(!added || actual!=&freshBucket->tree){xrtMutexUnlock(&s->lock);break;}bucket=freshBucket;freshBucket=NULL;}
  fresh->next=first;fresh->identity=++s->nextDomain;bucket->first=fresh;result=fresh;fresh=NULL;++s->stats.domains;s->stats.bytes+=charge;xrtMutexUnlock(&s->lock);break;
 }
 if(!result){xrtMutexLock(&s->lock);increment(&s->stats.declined);xrtMutexUnlock(&s->lock);}
 if(fresh){xlsContextRelease(fresh->context);xrtFree(fresh);}xrtFree(freshBucket);return result;
}
uint64_t xlsEntityDomainIdentity(const xls_entity_domain* domain){return domain ? domain->identity : 0;}
const xls_context* xlsEntityDomainContext(const xls_entity_domain* domain){return domain ? domain->context : NULL;}
uint64_t xlsEntityStoreIntern(xls_entity_store* s,const xls_entity_domain* domain,uint64_t module,const void* bytes,size_t size)
{
 if(!s || !domain || domain->owner!=s || !module || !bytes || !size || size>s->limits.keyBytes)return 0;
 entity_key key={domain->identity,module,bytes,size};
 xrtMutexLock(&s->lock);entity_record* old=(entity_record*)xrtAVLFind(&s->entities,&key,entityCompare,NULL);uint64_t result=old ? old->identity : 0;if(result)increment(&s->stats.hits);xrtMutexUnlock(&s->lock);if(result)return result;
 size_t charge=sizeof(entity_record)+size;entity_record* fresh=xrtCalloc(1,charge);if(!fresh)return 0;
 fresh->domain=key.domain;fresh->module=module;fresh->size=size;memcpy(fresh->key,bytes,size);
 xrtMutexLock(&s->lock);old=(entity_record*)xrtAVLFind(&s->entities,&key,entityCompare,NULL);
 if(old){result=old->identity;increment(&s->stats.hits);}
 else if(s->stats.entities<s->limits.entities && s->nextEntity<UINT64_MAX && charge<=s->limits.bytes-s->stats.bytes){bool added=false;xavlnode* actual=xrtAVLInsert(&s->entities,&fresh->tree,&key,entityCompare,NULL,&added);if(added && actual==&fresh->tree){fresh->identity=++s->nextEntity;result=fresh->identity;fresh=NULL;++s->stats.entities;s->stats.bytes+=charge;increment(&s->stats.admitted);}}
 if(!result)increment(&s->stats.declined);
 xrtMutexUnlock(&s->lock);xrtFree(fresh);return result;
}
void xlsEntityStoreStats(xls_entity_store* s,xls_entity_stats* stats)
{if(stats){*stats=(xls_entity_stats){0};if(s){xrtMutexLock(&s->lock);*stats=s->stats;xrtMutexUnlock(&s->lock);}}}
