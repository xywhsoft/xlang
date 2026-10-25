#include "analysis/entity_store.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
/* Diagnostic copy of the EXACT production store,with only bucket hash forced
 * to collide. Normal core/store code remains separately linked and unmodified. */
#define xlsContextHash(context) UINT64_C(7)
#define xlsEntityStoreCreate collisionCreate
#define xlsEntityStoreRetain collisionRetain
#define xlsEntityStoreRelease collisionRelease
#define xlsEntityStoreDomain collisionDomain
#define xlsEntityDomainIdentity collisionIdentity
#define xlsEntityDomainContext collisionContext
#define xlsEntityStoreIntern collisionIntern
#define xlsEntityStoreStats collisionStats
#include "../src/analysis/entity_store.c"
#undef xlsContextHash
#undef xlsEntityStoreCreate
#undef xlsEntityStoreRetain
#undef xlsEntityStoreRelease
#undef xlsEntityStoreDomain
#undef xlsEntityDomainIdentity
#undef xlsEntityDomainContext
#undef xlsEntityStoreIntern
#undef xlsEntityStoreStats
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"entity_store:%d: %s\n",__LINE__,#x);return 0;}}while(0)
static xls_context* context(const char* config)
{
 xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project="D:/fixture/a.xlp",.configuration=config,.entry="D:/fixture/main.xl",.directory="D:/fixture",.analysisProfile="entity-store-test",.targetKind="exe"};return xlsContextCreate(&spec,65536,NULL);
}
static int domains(void)
{
 CHECK(!xlsEntityStoreCreate(&(xls_entity_limits){0,1,1,4096}));CHECK(!xlsEntityStoreCreate(&(xls_entity_limits){1,0,1,4096}));CHECK(!xlsEntityStoreCreate(&(xls_entity_limits){1,1,0,4096}));CHECK(!xlsEntityStoreCreate(&(xls_entity_limits){1,1,1,1}));
 xls_entity_store* s=xlsEntityStoreCreate(NULL);xls_context *a=context("debug"),*twin=context("debug"),*b=context("release");CHECK(s && a && twin && b && a!=twin && xlsContextEqual(a,twin));
 const xls_entity_domain *da=xlsEntityStoreDomain(s,a),*dt=xlsEntityStoreDomain(s,twin),*db=xlsEntityStoreDomain(s,b);CHECK(da && dt==da && db && db!=da && xlsEntityDomainIdentity(da)!=xlsEntityDomainIdentity(db));
 unsigned char key[]={0,1,2,0,4};uint64_t id=xlsEntityStoreIntern(s,da,1,key,sizeof(key));CHECK(id && xlsEntityStoreIntern(s,dt,1,key,sizeof(key))==id && xlsEntityStoreIntern(s,db,1,key,sizeof(key))!=id && xlsEntityStoreIntern(s,da,2,key,sizeof(key))!=id);
 key[4]=5;CHECK(xlsEntityStoreIntern(s,da,1,key,sizeof(key))!=id);CHECK(!xlsEntityStoreIntern(s,da,0,key,sizeof(key)) && !xlsEntityStoreIntern(s,da,1,NULL,1) && !xlsEntityStoreIntern(s,da,1,key,0));
 xls_entity_store* foreign=xlsEntityStoreCreate(NULL);CHECK(foreign && !xlsEntityStoreIntern(foreign,da,1,key,sizeof(key)));xlsEntityStoreRelease(foreign);
 xls_entity_stats stats;xlsEntityStoreStats(s,&stats);CHECK(stats.domains==2 && stats.entities==4 && stats.hits>=1 && stats.bytes<=stats.limit);
 xlsContextRelease(a);xlsContextRelease(twin);xlsContextRelease(b);CHECK(!strcmp(xlsContextSpec(xlsEntityDomainContext(da))->configuration,"debug"));xlsEntityStoreRelease(s);return 1;
}
static int collisionsAndLimits(void)
{
 xls_entity_store* s=collisionCreate(NULL);xls_context *a=context("debug"),*b=context("release"),*twin=context("debug");CHECK(s && a && b && twin);
 const xls_entity_domain *da=collisionDomain(s,a),*db=collisionDomain(s,b);CHECK(da && db && da!=db && collisionDomain(s,twin)==da);CHECK(collisionIntern(s,da,1,"same",4)!=collisionIntern(s,db,1,"same",4));collisionRelease(s);
 s=xlsEntityStoreCreate(&(xls_entity_limits){1,2,4,65536});CHECK(s);da=xlsEntityStoreDomain(s,a);CHECK(da && !xlsEntityStoreDomain(s,b));CHECK(xlsEntityStoreIntern(s,da,1,"one",3) && xlsEntityStoreIntern(s,da,1,"two",3));CHECK(!xlsEntityStoreIntern(s,da,1,"three",5) && !xlsEntityStoreIntern(s,da,1,"new",3));xls_entity_stats stats;xlsEntityStoreStats(s,&stats);CHECK(stats.entities==2 && stats.domains==1 && stats.declined>=2 && stats.bytes<=stats.limit);xlsEntityStoreRelease(s);
 for(size_t bytes=256;bytes<=8192;bytes*=2){s=xlsEntityStoreCreate(&(xls_entity_limits){2,2,8,bytes});if(s){da=xlsEntityStoreDomain(s,a);if(da)(void)xlsEntityStoreIntern(s,da,1,"item",4);xlsEntityStoreStats(s,&stats);CHECK(stats.bytes<=bytes);xlsEntityStoreRelease(s);}}
 xlsContextRelease(a);xlsContextRelease(b);xlsContextRelease(twin);return 1;
}
typedef struct reader {xls_entity_store* store;xls_context* context;uint64_t domain,entity;int okay;} reader;
static int32 readStore(void* user)
{
 reader* r=user;r->okay=1;
 for(size_t i=0;i<128;++i){const xls_entity_domain* d=xlsEntityStoreDomain(r->store,r->context);uint64_t id=d ? xlsEntityStoreIntern(r->store,d,1,"parallel",8) : 0;if(!d || !id){r->okay=0;break;}if(!i){r->domain=xlsEntityDomainIdentity(d);r->entity=id;}else if(r->domain!=xlsEntityDomainIdentity(d) || r->entity!=id)r->okay=0;}
 return 0;
}
static int concurrent(void)
{
 xls_entity_store* store=xlsEntityStoreCreate(NULL);CHECK(store);reader readers[4];xthread* threads[4];
 for(size_t i=0;i<4;++i){readers[i]=(reader){store,context("concurrent"),0,0,0};CHECK(readers[i].context);threads[i]=xrtThreadCreate(readStore,&readers[i],0);CHECK(threads[i]);}
 for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay && readers[i].domain==readers[0].domain && readers[i].entity==readers[0].entity);xrtThreadDestroy(threads[i]);xlsContextRelease(readers[i].context);}
 xls_entity_stats stats;xlsEntityStoreStats(store,&stats);CHECK(stats.domains==1 && stats.entities==1 && stats.hits>=511 && stats.bytes<=stats.limit);xlsEntityStoreRelease(store);return 1;
}
int main(void){if(!domains() || !collisionsAndLimits() || !concurrent())return 1;printf("analysis_entity_store_test passed: %u checks;exact context collision separation,binary keys,domain/module isolation,limits and four concurrent interning readers\n",checks);return 0;}
