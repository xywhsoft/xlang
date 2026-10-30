#include "analysis/frontend_internal.h"
#include <stdio.h>
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
/* The normal build resolves this to its audited generated semantic TU. The
 * standalone test object supplies its symbols before the core archive,so the
 * ordinary semantic object is not linked twice or changed for the service. */
#include "xl6_sem.c"
static unsigned frontChecks;
#define FRONT_CHECK(v) do{++frontChecks;if(!(v)){fprintf(stderr,"analysis_semantic_read_front_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
typedef struct front_fixture {
 XL6_SemContext ctx;XL6_Scope global;
 xls_sem_memo memo;xls_sem_read_context proof;xls_sem_build build;
 xls_sem_call_scope calls;xls_sem_read_capture capture;
} front_fixture;
static int frontInit(front_fixture* f)
{
 memset(f,0,sizeof(*f));f->ctx.root=xl6AstNew(XL6_AST_MODULE,NULL);FRONT_CHECK(f->ctx.root);
 f->ctx.scope=&f->global;f->ctx.moduleName="read-front-fixture";xl6TypeRegistryInit(&f->ctx.types);
 FRONT_CHECK(xl6TypeRegistryAddBuiltins(&f->ctx.types));FRONT_CHECK(xl6TypeRegistryFind(&f->ctx.types,"int"));
 f->memo.readContext=&f->proof;f->build.memo=&f->memo;
 xlsSemCallsBegin(&f->calls,&f->ctx);xlsSemReadsStart(&f->capture,&f->build,&f->ctx);
 FRONT_CHECK(f->capture.reads && f->capture.reads->complete);return 1;
}
static xls_sem_reads* frontStop(front_fixture* f)
{
 xls_sem_reads* reads=xlsSemReadsFinish(&f->capture);xlsSemCallsEnd(&f->calls);
 xl6TypeRegistryFree(&f->ctx.types);xl6AstFree(f->ctx.root);return reads;
}
static void frontObserve(front_fixture* f,int indexed,XL6_SemContext* ctx,const char* name,int type,const XL6_TypeInfo* actual)
{
 (void)f;
 if(indexed)xlsSemReadsObserve(ctx,name,type,actual);
 else xlsSemReadsObserveSlow(ctx,name,type,actual);
}
static void frontLookup(front_fixture* f,int indexed,const char* name)
{frontObserve(f,indexed,&f->ctx,name,1,xl6TypeRegistryFind(&f->ctx.types,name));}
static int frontSame(const xls_sem_reads* a,const xls_sem_reads* b)
{
 if(!a || !b || a->complete!=b->complete || a->bytes!=b->bytes || a->items.Count!=b->items.Count)return 0;
 for(size_t i=0;i<a->items.Count;++i){
  const xls_sem_read* x=xrtArrayConstGet(&a->items,i);const xls_sem_read* y=xrtArrayConstGet(&b->items,i);
  if(x->type!=y->type || x->count!=y->count || strcmp(x->name,y->name) || !xlsSemEqualKey(&x->key,&y->key))return 0;
 }
 return 1;
}
static uint64_t frontLive(void)
{xmemstats s;xrtMemStatsGet(&s);return s.BlockAllocCalls-s.BlockFreeCalls;}
static int frontCase(unsigned scenario,int indexed,xls_sem_reads** result,xls_sem_read_work* work)
{
 front_fixture f;FRONT_CHECK(frontInit(&f));frontLookup(&f,indexed,"int");
 FRONT_CHECK(f.capture.reads->complete);
 const XL6_TypeInfo* original=xl6TypeRegistryFind(&f.ctx.types,"int");FRONT_CHECK(original);
 int accepted=1;
 if(scenario==0){
  for(unsigned i=0;i<256;++i){frontLookup(&f,indexed,"int");frontLookup(&f,indexed,i&1 ? "bool" : "int");}
  FRONT_CHECK(!indexed || f.capture.frontComparisons>200);
 }else if(scenario<=8){
  XL6_TypeInfo changed=*original;char name[64],abi[128];
  FRONT_CHECK(original->name && original->abiName && strlen(original->name)<sizeof(name) && strlen(original->abiName)<sizeof(abi));
  strcpy(name,original->name);strcpy(abi,original->abiName);changed.name=name;changed.abiName=abi;
  frontObserve(&f,indexed,&f.ctx,"int",1,&changed);
  switch(scenario){case 1:++changed.id;break;case 2:++changed.abiId;break;case 3:name[0]^=1;break;
   case 4:abi[0]^=1;break;case 5:changed.kind=XL6_TYPE_ALIAS;break;case 6:changed.allowGeneric=!changed.allowGeneric;break;
   case 7:++changed.genericArity;break;default:changed.decl=f.ctx.root;break;}
  frontObserve(&f,indexed,&f.ctx,"int",1,&changed);accepted=0;
 }else if(scenario==9){
  frontLookup(&f,indexed,"front_missing");frontLookup(&f,indexed,"front_missing");
  FRONT_CHECK(xl6TypeRegistryAdd(&f.ctx.types,"front_missing","front.missing",XL6_TYPE_BUILTIN,0,0,NULL));
  frontLookup(&f,indexed,"front_missing");accepted=0;
 }else if(scenario==10){
  XL6_TypeInfo* mutable=(XL6_TypeInfo*)original;char first=mutable->name[0];mutable->name[0]^=1;
  FRONT_CHECK(!xl6TypeRegistryFind(&f.ctx.types,"int"));frontLookup(&f,indexed,"int");mutable->name[0]=first;accepted=0;
 }else if(scenario==11){
  XL6_SemContext other=f.ctx;frontObserve(&f,indexed,&other,"int",1,original);accepted=0;
 }else if(scenario==12){
  frontObserve(&f,indexed,&f.ctx,"",1,original);accepted=0;
 }else if(scenario==13){
  char longName[258];memset(longName,'a',257);longName[257]=0;
  frontObserve(&f,indexed,&f.ctx,longName,1,original);accepted=0;
 }else if(scenario==14){
  /* The unused actual parameter must not merge global/type namespaces. */
  frontObserve(&f,indexed,&f.ctx,"int",0,original);frontLookup(&f,indexed,"int");
  FRONT_CHECK(f.capture.reads->items.Count==2);
 }else if(scenario==15){
  for(unsigned i=0;i<255;++i){char name[48];snprintf(name,sizeof(name),"front_negative_%u",i);frontLookup(&f,indexed,name);}
  FRONT_CHECK(f.capture.reads->complete && f.capture.reads->items.Count==256);
  frontLookup(&f,indexed,"int");accepted=0;
 }else if(scenario==16){
  xlsSemReadValuesDrop(&f.capture);
  for(unsigned i=0;i<32;++i)FRONT_CHECK(!f.capture.typedSlots[i]);
  frontLookup(&f,indexed,"int");frontLookup(&f,indexed,"int");FRONT_CHECK(!f.capture.frontComparisons);
 }else if(scenario==17){
  XL6_TypeInfo* other=(XL6_TypeInfo*)xl6TypeRegistryFind(&f.ctx.types,"bool");FRONT_CHECK(other);other->id=original->id;
  for(unsigned i=0;i<64;++i){frontLookup(&f,indexed,i&1 ? "int" : "bool");}
  FRONT_CHECK(f.capture.reads->items.Count==2);
 }else if(scenario==18){
  size_t capacity=f.capture.reads->items.Capacity;
  for(unsigned i=0;i<96;++i){char name[48];snprintf(name,sizeof(name),"FrontType_%u",i);
   FRONT_CHECK(xl6TypeRegistryAdd(&f.ctx.types,name,name,XL6_TYPE_BUILTIN,0,0,NULL));frontLookup(&f,indexed,name);}
  FRONT_CHECK(f.capture.reads->items.Capacity>capacity);
  frontLookup(&f,indexed,"int");frontLookup(&f,indexed,"int");FRONT_CHECK(!indexed || f.capture.frontComparisons);
 }else if(scenario==19){
  /* A captured value whose name differs from its lookup cannot stand in for
   * that lookup. Exercise even the adversarial caller/result relationship. */
  XL6_TypeInfo altered=*original;altered.name="front_other";
  xlsSemReadValuesDrop(&f.capture);f.capture.valuesReady=xrtArrayInit(&f.capture.values,sizeof(xls_sem_read_value));
  /* Use a new capture so its first record can have the mismatched value. */
  xls_sem_reads* discarded=xlsSemReadsFinish(&f.capture);xlsSemReadsDestroy(discarded);
  xlsSemReadsStart(&f.capture,&f.build,&f.ctx);
  frontObserve(&f,indexed,&f.ctx,"int",1,&altered);frontObserve(&f,indexed,&f.ctx,"front_other",1,&altered);
  FRONT_CHECK(f.capture.reads->items.Count==2 && !f.capture.frontComparisons);
 }else if(scenario==20){
  frontObserve(&f,indexed,&f.ctx,NULL,1,original);accepted=0;
 }else if(scenario==21){
  /* The final original serialized proof detects changes after the last
   * successful observation even though no fast-path comparison follows. */
  frontLookup(&f,indexed,"int");((XL6_TypeInfo*)original)->allowGeneric=!original->allowGeneric;accepted=0;
 }else if(scenario==22){
  char query[]="int";frontLookup(&f,indexed,query);query[0]='z';frontLookup(&f,indexed,query);
  query[0]='i';frontLookup(&f,indexed,query);FRONT_CHECK(f.capture.reads->items.Count==2);
 }
 if(scenario!=21)FRONT_CHECK(f.capture.reads->complete==accepted);
 *result=frontStop(&f);*work=f.memo.readWork;
 FRONT_CHECK(*result && (*result)->complete==accepted && (!indexed || scenario || f.capture.frontComparisons));
 FRONT_CHECK(!xlsActiveSemReads && !xlsActiveCallScope);
 return 1;
}
static int frontCases(void)
{
 for(unsigned scenario=0;scenario<=22;++scenario){
  uint64_t live=frontLive();xls_sem_reads* old=NULL,*next=NULL;xls_sem_read_work a={0},b={0};
  FRONT_CHECK(frontCase(scenario,0,&old,&a) && frontCase(scenario,1,&next,&b));
  FRONT_CHECK(frontSame(old,next));
  FRONT_CHECK(a.observations==b.observations && a.typedComparisons==b.typedComparisons && a.recorded==b.recorded && a.negative==b.negative && a.scratchReuses==b.scratchReuses && a.valueBytes==b.valueBytes);
  xlsSemReadsDestroy(old);xlsSemReadsDestroy(next);FRONT_CHECK(frontLive()==live);
 }
 return 1;
}
static int frontNested(void)
{
 uint64_t live=frontLive();front_fixture outer,inner;FRONT_CHECK(frontInit(&outer));
 frontLookup(&outer,1,"int");frontLookup(&outer,1,"int");uint64_t before=outer.capture.frontComparisons;FRONT_CHECK(before);
 FRONT_CHECK(frontInit(&inner));frontLookup(&inner,1,"int");frontLookup(&inner,1,"int");FRONT_CHECK(inner.capture.frontComparisons);
 frontObserve(&inner,1,&outer.ctx,"int",1,xl6TypeRegistryFind(&outer.ctx.types,"int"));
 xls_sem_reads* a=frontStop(&inner);FRONT_CHECK(a && !a->complete && xlsActiveSemReads==&outer.capture);
 frontLookup(&outer,1,"int");FRONT_CHECK(outer.capture.frontComparisons>before);
 xls_sem_reads* b=frontStop(&outer);FRONT_CHECK(b && b->complete && !xlsActiveSemReads && !xlsActiveCallScope);
 xlsSemReadsDestroy(a);xlsSemReadsDestroy(b);FRONT_CHECK(frontLive()==live);return 1;
}
int main(void)
{
 /* Warm XRT's ordinary thread-local allocator state before balance checks. */
 front_fixture warm;if(!frontInit(&warm))return 1;xlsSemReadsDestroy(frontStop(&warm));
 if(!frontCases() || !frontNested())return 1;
 printf("analysis_semantic_read_front_test passed: %u checks; production slow/front snapshot parity,all live registry fields,negative/present changes,context and namespace,256-record limit,vector growth/collisions,value fallback,final proof,nested lifetime and allocation balance\n",frontChecks);return 0;
}
#else
int main(void){puts("analysis_semantic_read_front_test passed: legacy cohort retains its original observer");return 0;}
#endif
