#include "foreign_xge3_particle_definition.h"
/* Exact pinned native layout for test-only reference accounting, never a
 * public binding field or a guessed offset. */
#include "src/xge/particle/xge_particle_internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static xmemdebugsnapshot prefix,hot;
static xge_particle_definition kept;
void fixtureDefinitionBudget(int budget)
{
 if(budget<0)xrtMemDebugFailClear();
 else {assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(budget));}
}
void fixtureDefinitionPrefixBegin(int budget)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&prefix);fixtureDefinitionBudget(budget);}
void fixtureDefinitionPrefixEnd(void){xrtMemDebugFailClear();xrtClearError();}
void fixtureDefinitionHeapEqual(void)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 if(prefix.LiveCount!=after.LiveCount||prefix.LiveBytes!=after.LiveBytes)
  fprintf(stderr,"Definition leak: count %zu -> %zu bytes %zu -> %zu\n",prefix.LiveCount,after.LiveCount,prefix.LiveBytes,after.LiveBytes);
 assert(prefix.LiveCount==after.LiveCount && prefix.LiveBytes==after.LiveBytes &&
  prefix.InvalidFreeCount==after.InvalidFreeCount && prefix.DoubleFreeCount==after.DoubleFreeCount);
}
void fixtureDefinitionHotBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&hot);}
void fixtureDefinitionHotEnd(int native)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 printf("ParticleDefinition heap %s: alloc=%llu free=%llu realloc=%llu\n",native?"native":"language",
  (unsigned long long)(after.AllocCount-hot.AllocCount),(unsigned long long)(after.FreeCount-hot.FreeCount),
  (unsigned long long)(after.ReallocCount-hot.ReallocCount));
 assert(after.AllocCount==hot.AllocCount && after.FreeCount==hot.FreeCount && after.ReallocCount==hot.ReallocCount);
}
void fixtureDefinitionColdEnd(int operation)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 printf("ParticleDefinition cold operation=%d alloc=%llu free=%llu realloc=%llu\n",operation,
  (unsigned long long)(after.AllocCount-hot.AllocCount),(unsigned long long)(after.FreeCount-hot.FreeCount),
  (unsigned long long)(after.ReallocCount-hot.ReallocCount));
}
void fixtureDefinitionNativeCreate(void)
{
 xge_particle_emitter_t emitters[2];xge_particle_definition d=NULL;
 xgeParticleEmitterInit(&emitters[0]);xgeParticleEmitterInit(&emitters[1]);
 emitters[0].fRate=99;emitters[1].fRate=17;
 assert(xgeParticleDefinitionCreate(&d,emitters,2)==XGE_OK);xgeParticleDefinitionFree(d);
}
void fixtureDefinitionNativeSerialization(xge_particle_definition definition,int xlon)
{char* text=NULL;size_t size=0;assert(xgeParticleDefinitionStringify(definition,xlon,&text,&size)==XGE_OK && text && size);xrtFree(text);}
double fixtureDefinitionNativeHot(xge_particle_definition definition)
{
 double result=0;
 for(int i=0;i<1000;++i){xge_particle_emitter_t emitter;assert(xgeParticleDefinitionCount(definition)==2);
  assert(xgeParticleDefinitionGet(definition,i%2,&emitter)==XGE_OK);result+=emitter.fRate;}
 return result;
}
void fixtureDefinitionKeep(xge_particle_definition definition)
{assert(!kept && definition && definition->iRefs==1);xgeParticleDefinitionAddRef(definition);kept=definition;assert(kept->iRefs==2);}
void fixtureDefinitionCheckShare(xge_particle_definition definition,xge_particle_definition shared)
{
 assert(definition && shared==definition && definition->iRefs==2);
}
int fixtureDefinitionReferences(xge_particle_definition definition)
{
 assert(definition);
 return definition->iRefs;
}
void fixtureDefinitionNativeShares(xge_particle_definition definition)
{
 xge_particle_definition shared=definition;
 assert(xgeParticleDefinitionShareOwned(NULL,&shared)==XGE_ERROR_INVALID_ARGUMENT && !shared);
 assert(xgeParticleDefinitionShareOwned(definition,NULL)==XGE_ERROR_INVALID_ARGUMENT);
 assert(definition->iRefs==1);
 definition->iRefs=INT32_MAX;
 shared=definition;
 assert(xgeParticleDefinitionShareOwned(definition,&shared)==XGE_ERROR_INVALID_STATE && !shared);
 assert(definition->iRefs==INT32_MAX);
 definition->iRefs=1;
 for(int i=0;i<1000;++i){
  assert(xgeParticleDefinitionShareOwned(definition,&shared)==XGE_OK);
  fixtureDefinitionCheckShare(definition,shared);
  xgeParticleDefinitionFree(shared);
 }
 assert(definition->iRefs==1);
}
void fixtureDefinitionCheckKept(void)
{xge_particle_emitter_t e;assert(kept && kept->iRefs==1 && xgeParticleDefinitionCount(kept)==2);assert(xgeParticleDefinitionGet(kept,1,&e)==XGE_OK && e.fRate==17);}
void fixtureDefinitionReleaseKept(void){xgeParticleDefinitionFree(kept);kept=NULL;}
int fixtureDefinitionSerializationOperations(xge_particle_definition definition,int xlon)
{
 xmemdebugsnapshot before,after;char* text=NULL;size_t size=0;
 xrtMemDebugSnapshot(&before);assert(xgeParticleDefinitionStringify(definition,xlon,&text,&size)==XGE_OK);
 xrtMemDebugSnapshot(&after);xrtFree(text);
 uint64_t count=after.AllocCount-before.AllocCount+after.ReallocCount-before.ReallocCount;
 assert(count<10000);return (int)count;
}
void fixtureDefinitionNativeValidation(void)
{
 xge_particle_emitter_t emitters[2],read;
 for(int i=0;i<2;++i){xgeParticleEmitterInit(&emitters[i]);emitters[i].fRate=(float)(11+i*6);}
 memcpy(emitters[0].sName,"\xe7\x81\xab\xe8\x8a\xb1",7);
 emitters[0].tSizeOverLife.iCount=2;
 emitters[0].tSizeOverLife.arrKeys[0]=(xge_particle_key_t){0,2,1,3};
 emitters[0].tSizeOverLife.arrKeys[1]=(xge_particle_key_t){1,4,5,6};
 emitters[0].iBurstCount=1;emitters[0].arrBursts[0]=(xge_particle_burst_t){0.5f,3};
 emitters[0].arrSubEmitters[0]=(xge_particle_subemitter_t){1,2,0.5f};
 xge_particle_definition definition=NULL;assert(xgeParticleDefinitionCreate(&definition,emitters,2)==XGE_OK);
 fixtureDefinitionNativeShares(definition);
 assert(xgeParticleDefinitionGet(definition,2,&read)==XGE_ERROR_INVALID_ARGUMENT);
 assert(xgeParticleDefinitionGet(definition,-1,&read)==XGE_ERROR_INVALID_ARGUMENT);
 for(int xlon=0;xlon<2;++xlon){
  char* text=NULL;size_t size=0;assert(xgeParticleDefinitionStringify(definition,xlon,&text,&size)==XGE_OK && text && size);
  xge_particle_definition parsed=NULL;
  assert(xgeParticleDefinitionParse(&parsed,text,size,xlon,NULL,0)==XGE_OK);
  assert(xgeParticleDefinitionGet(parsed,0,&read)==XGE_OK && !memcmp(&read,&emitters[0],sizeof(read)));
  xgeParticleDefinitionFree(parsed);
  xmemdebugsnapshot start,end;
  xrtMemDebugSnapshot(&start);char* baseline=NULL;size_t baselineSize=0;
  assert(xgeParticleDefinitionStringify(definition,xlon,&baseline,&baselineSize)==XGE_OK);xrtMemDebugSnapshot(&end);
  size_t operations=(size_t)(end.AllocCount-start.AllocCount+end.ReallocCount-start.ReallocCount);
  xrtFree(baseline);assert(operations<10000);
  int failed=0,succeeded=0;
  for(size_t budget=0;budget<=operations+2;++budget){
   fixtureDefinitionPrefixBegin((int)budget);char* result=NULL;size_t n=0;
   int status=xgeParticleDefinitionStringify(definition,xlon,&result,&n);fixtureDefinitionPrefixEnd();
   if(status==XGE_OK){assert(result && n==size && !memcmp(text,result,size));++succeeded;}
   else {assert(!result && !n);++failed;}
   xrtFree(result);fixtureDefinitionHeapEqual();
  }
  assert(failed && succeeded);printf("ParticleDefinition native OOM format=%d stringifyBudgets=%zu\n",xlon,operations+3);
  xrtMemDebugSnapshot(&start);assert(xgeParticleDefinitionParse(&parsed,text,size,xlon,NULL,0)==XGE_OK);
  xrtMemDebugSnapshot(&end);operations=(size_t)(end.AllocCount-start.AllocCount+end.ReallocCount-start.ReallocCount);
  xgeParticleDefinitionFree(parsed);assert(operations<10000);failed=succeeded=0;
  for(size_t budget=0;budget<=operations+2;++budget){
   fixtureDefinitionPrefixBegin((int)budget);parsed=NULL;
   int status=xgeParticleDefinitionParse(&parsed,text,size,xlon,NULL,0);fixtureDefinitionPrefixEnd();
   if(status==XGE_OK){assert(parsed && xgeParticleDefinitionCount(parsed)==2);++succeeded;}
   else {assert(!parsed);++failed;}
   xgeParticleDefinitionFree(parsed);fixtureDefinitionHeapEqual();
  }
  assert(failed && succeeded);xrtFree(text);
  printf("ParticleDefinition native OOM format=%d parseBudgets=%zu\n",xlon,operations+3);
 }
 for(int budget=0;budget<6;++budget){
  fixtureDefinitionPrefixBegin(budget);xge_particle_definition d=NULL;
  int status=xgeParticleDefinitionCreate(&d,emitters,2);fixtureDefinitionPrefixEnd();
  assert((status==XGE_OK)==(d!=NULL));xgeParticleDefinitionFree(d);fixtureDefinitionHeapEqual();
 }
 emitters[0].arrSubEmitters[0].iEmitter=0;
 xge_particle_definition invalid=NULL;assert(xgeParticleDefinitionCreate(&invalid,emitters,2)!=XGE_OK && !invalid);
 xgeParticleDefinitionFree(definition);puts("ParticleDefinition native roundtrip, DAG and exhaustive allocation prefixes passed");
}
