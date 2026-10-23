#include "services/diagnostic_store.h"
#include "protocol/diagnostic_paths.h"
#include "platform/path.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static unsigned checks;
#define CHECK(v) do { ++checks; if(!(v)) { fprintf(stderr,"diagnostic_store_test:%d: %s\n",__LINE__,#v); return 0; } } while(0)
typedef struct fixture { xide_language_input_guard* guard; xvalue* result; } fixture;
static const xvalue* get(const xvalue* object,const char* key) { return xrtValueObjectGet(object,xrtStrView(key)); }
static const char* string(const xvalue* value) { xstrview text={0}; return xrtValueGetString(value,&text) ? text.Data : NULL; }
static int writeText(const char* path,const char* text) { return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)}); }
static fixture capture(const char* root,const char* entry,const xide_language_snapshot* inputs,size_t count,int accepted)
{
 fixture result={0}; xide_model_error error={0}; xide_toolchains* cache=xideToolchainsCreate(); char* compiler=xrtPathJoin(root,"no-executor/xl.exe");
 xide_language_analysis_options options={.nativeBackend=1};
 xide_language_check* check=xideLanguageCheckStartSnapshotsEx(cache,compiler,root,entry,inputs,count,&options,&error);
 uint64_t began=xrtClock();
 while(check && xideLanguageCheckState(check,error.message,sizeof(error.message))==XIDE_CHECK_RUNNING && xrtClock()-began<15000000) xrtSleep(1);
 result.result=xideLanguageCheckResult(check); bool okay=false;
 if(result.result && xrtValueGetBool(get(result.result,"ok"),&okay) && okay==accepted) result.guard=xideLanguageCheckCaptureInputGuard(check,&error);
 if(!result.guard) { fprintf(stderr,"capture: %s\n",error.message); xrtValueRelease(result.result); result.result=NULL; }
 xideLanguageCheckDestroy(check); xideToolchainsDestroy(cache); xrtFree(compiler); return result;
}
static xide_diagnostic_query* insert(xide_diagnostic_store* store,const char* key,fixture value)
{
 xide_diagnostic_query* query=xideDiagnosticStorePutTake(store,key,value.guard,value.result);
 xideLanguageInputGuardRelease(value.guard); return query;
}
static enum xide_diagnostic_state waitFor(xide_diagnostic_query* query,xide_diagnostic_snapshot* result)
{
 uint64_t began=xrtClock(); enum xide_diagnostic_state state;
 while((state=xideDiagnosticQueryResult(query,result))==XIDE_DIAGNOSTIC_PENDING && xrtClock()-began<15000000) xrtSleep(1);
 if(state==XIDE_DIAGNOSTIC_FAILED) fprintf(stderr,"diagnostic query: %s\n",result->error);
 return state;
}
static size_t errors(const xvalue* groups,const char* path)
{
 for(size_t i=0;i<xrtValueCount(groups);++i) { const xvalue* group=xrtValueArrayGet(groups,i); const char* name=string(get(group,"path")); if(name && xidePathLexicalEqual(name,path)) return xrtValueCount(get(group,"diagnostics")); }
 return SIZE_MAX;
}
static int invalidated(const xvalue* keys,const char* key)
{ for(size_t i=0;i<xrtValueCount(keys);++i) { const char* value=string(xrtValueArrayGet(keys,i)); if(value && !strcmp(value,key)) return 1; } return 0; }
static void wake(void* user) { xrtAtomic32FetchAdd(user,1,XMEMORY_RELAXED); }
static xvalue* dependencyPaths(const xide_diagnostic_publication* publication,size_t index)
{
 const char* id=xideDiagnosticPublicationDependencyId(publication,index);const xvalue* chunks=xideDiagnosticPublicationDependencyChunks(publication,index);
 CHECK(id && chunks && xrtValueCount(chunks)>0 && xideDiagnosticPublicationFindDependency(publication,id)==index);
 xvalue* paths=xrtValueArray();CHECK(paths);
 for(size_t i=0;i<xrtValueCount(chunks);++i) {
  const xvalue* chunk=xrtValueArrayGet(chunks,i),*part=get(chunk,"paths");int64 schema=0,number=0,total=0;bool removed=true;size_t bytes=0;
  char* encoded=xrtJsonStringify(chunk,false,&bytes);CHECK(encoded && bytes<=256u*1024u);xrtFree(encoded);
  CHECK(xrtValueGetInt(get(chunk,"schema"),&schema) && schema==1 && !strcmp(string(get(chunk,"id")),id) &&
   xrtValueGetInt(get(chunk,"part"),&number) && number==(int64)i && xrtValueGetInt(get(chunk,"parts"),&total) && total==(int64)xrtValueCount(chunks) &&
   xrtValueGetBool(get(chunk,"removed"),&removed) && !removed && part && xrtValueType(part)==XVALUE_ARRAY);
  for(size_t j=0;j<xrtValueCount(part);++j) CHECK(xrtValueArrayAppend(paths,xrtValueArrayGet(part,j)));
 }
 char digest[65];CHECK(xlsDiagnosticPathsDigest(paths,digest) && !strcmp(digest,id));return paths;
}
static int dependencyContains(const xvalue* paths,const char* path)
{for(size_t i=0;i<xrtValueCount(paths);++i) if(xidePathLexicalEqual(string(xrtValueArrayGet(paths,i)),path)) return 1;return 0;}
static xide_diagnostic_publication* takePublication(xide_diagnostic_query* query)
{
 xide_diagnostic_snapshot result={0};CHECK(query && waitFor(query,&result)==XIDE_DIAGNOSTIC_READY);
 xide_diagnostic_publication* p=NULL;xvalue* invalidated=NULL;CHECK(xideDiagnosticQueryTake(query,&p,&invalidated) && p);
 xrtValueRelease(invalidated);xideDiagnosticQueryDestroy(query);return p;
}
static int dependencyScopes(void)
{
 CHECK(!xideDiagnosticStoreCreateWithDependencies(NULL,2));CHECK(!xideDiagnosticPublicationDependencyCount(NULL) && !xideDiagnosticPublicationDependencyId(NULL,0));
 CHECK(!xideDiagnosticPublicationGroupDependency(NULL,0) && !xideDiagnosticPublicationDependencyChunks(NULL,0) && xideDiagnosticPublicationFindDependency(NULL,"x")==SIZE_MAX);
 char relative[100];snprintf(relative,sizeof(relative),"build/dependency-scopes-%llu",(unsigned long long)xrtClock());CHECK(xrtDirCreateAll(relative));char* root=xrtPathAbs(relative);CHECK(root);
 char *a=xrtPathJoin(root,"a.xl"),*b=xrtPathJoin(root,"b.xl"),*shared=xrtPathJoin(root,"shared.xl"),*other=xrtPathJoin(root,"other.xl");CHECK(a && b && shared && other);
 const char* entry="import shared\nfunc main(...) as int\n return shared.value()\nend\n";
 xide_language_snapshot inputs[]={{a,entry,1},{b,entry,1},{shared,"export func value() as int\n return \"bad\"\nend\n",1},{other,"func main(...) as int\n return missing\nend\n",1}};
 xide_diagnostic_store* store=xideDiagnosticStoreCreateWithDependencies(NULL,1);fixture value=capture(root,a,inputs,4,0);CHECK(store && value.guard);
 xvalue* expected=xideLanguageInputGuardPaths(value.guard,NULL,NULL);CHECK(expected);
 xide_diagnostic_publication* first=takePublication(insert(store,"a",value));CHECK(first && xideDiagnosticPublicationDependencyCount(first)==1);
 xvalue* firstPaths=dependencyPaths(first,0);CHECK(firstPaths && xrtValueEqual(expected,firstPaths));xrtValueRelease(expected);xrtValueRelease(firstPaths);
 char firstId[65];memcpy(firstId,xideDiagnosticPublicationDependencyId(first,0),65);
 value=capture(root,b,inputs,4,0);CHECK(value.guard);xide_diagnostic_publication* second=takePublication(insert(store,"b",value));
 CHECK(second && xideDiagnosticPublicationDependencyCount(second)==3 && xideDiagnosticPublicationFindDependency(second,firstId)!=SIZE_MAX);
 size_t group=xideDiagnosticPublicationFind(second,shared);const char* id=xideDiagnosticPublicationGroupDependency(second,group);CHECK(id);
 xvalue* paths=dependencyPaths(second,xideDiagnosticPublicationFindDependency(second,id));CHECK(paths && dependencyContains(paths,a) && dependencyContains(paths,b) && dependencyContains(paths,shared) && !dependencyContains(paths,other));xrtValueRelease(paths);
 value=capture(root,other,inputs,4,0);CHECK(value.guard);xide_diagnostic_publication* third=takePublication(insert(store,"other",value));CHECK(third);
 size_t independent=xideDiagnosticPublicationFind(third,other);char independentId[65];memcpy(independentId,xideDiagnosticPublicationGroupDependency(third,independent),65);
 xide_diagnostic_publication* fourth=takePublication(xideDiagnosticStoreRemove(store,"a"));CHECK(fourth && xideDiagnosticPublicationFindDependency(fourth,independentId)!=SIZE_MAX);
 CHECK(xideDiagnosticPublicationEqual(third,independent,fourth,xideDiagnosticPublicationFind(fourth,other)));
 id=xideDiagnosticPublicationGroupDependency(fourth,xideDiagnosticPublicationFind(fourth,shared));CHECK(id);
 paths=dependencyPaths(fourth,xideDiagnosticPublicationFindDependency(fourth,id));CHECK(paths && !dependencyContains(paths,a) && dependencyContains(paths,b));
 xideDiagnosticStoreDestroy(store);xideDiagnosticPublicationDestroy(first);xideDiagnosticPublicationDestroy(second);xideDiagnosticPublicationDestroy(third);xideDiagnosticPublicationDestroy(fourth);
 CHECK(dependencyContains(paths,b));xrtValueRelease(paths);xrtFree(a);xrtFree(b);xrtFree(shared);xrtFree(other);xrtFree(root);return 1;
}
static int dependencyParts(void)
{
 char* base=xrtPathAbs("build/long-virtual-dependency-root");CHECK(base);xstrbuf directory;xrtStrBufInit(&directory);CHECK(xrtStrBufAppend(&directory,xrtStrView(base)));xrtFree(base);
 while(directory.Size<6000) CHECK(xrtStrBufAppend(&directory,xrtStrView("/segment-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
 xstrbuf entry;xrtStrBufInit(&entry);
 for(size_t i=0;i<47;++i) {char line[64];snprintf(line,sizeof(line),"import m%zu\n",i);CHECK(xrtStrBufAppend(&entry,xrtStrView(line)));}
 CHECK(xrtStrBufAppend(&entry,xrtStrView("func main(...) as int\n return 0\nend\n")));
 /* The native frontend limits source imports to512-byte paths. Long missing
    package candidates still belong to its observed input guard, including an
    erroneous entry; exercise multipart metadata without inventing imports. */
 char* path=xrtPathJoin(directory.Data,"entry.xl");char* candidate=xrtPathJoin(directory.Data,"m0/xlang.package.json");CHECK(path && candidate);
 xide_language_snapshot input={path,entry.Data,1};
 xide_diagnostic_store* store=xideDiagnosticStoreCreateWithDependencies(NULL,1);fixture value=capture(directory.Data,path,&input,1,0);CHECK(store && value.guard);
 xide_diagnostic_publication* publication=takePublication(insert(store,"long",value));CHECK(publication && xideDiagnosticPublicationCount(publication)==1 && xideDiagnosticPublicationDependencyCount(publication)==1);
 const xvalue* chunks=xideDiagnosticPublicationDependencyChunks(publication,0);CHECK(xrtValueCount(chunks)>1);
 xvalue* paths=dependencyPaths(publication,0);CHECK(paths);const char* id=xideDiagnosticPublicationDependencyId(publication,0);
 CHECK(dependencyContains(paths,path) && dependencyContains(paths,candidate) && !strcmp(xideDiagnosticPublicationGroupDependency(publication,0),id));
 xideDiagnosticStoreDestroy(store);xideDiagnosticPublicationDestroy(publication);CHECK(dependencyContains(paths,path));xrtValueRelease(paths);xrtFree(path);xrtFree(candidate);
 xrtStrBufFree(&entry);xrtStrBufFree(&directory);return 1;
}
static int run(void)
{
 xide_diagnostic_snapshot snapshot={0}; CHECK(xideDiagnosticQueryResult(NULL,&snapshot)==XIDE_DIAGNOSTIC_FAILED && !snapshot.documents);
 xideDiagnosticQueryDestroy(NULL); xideDiagnosticQueryCancel(NULL); xideDiagnosticStoreDestroy(NULL);
 CHECK(!xideDiagnosticStoreCreate(&(xide_diagnostic_limits){65,10000,10000,1}));
 CHECK(!xideDiagnosticStorePutTake(NULL,"bad",NULL,xrtValueObject()));
 char relative[160]; snprintf(relative,sizeof(relative),"build/diagnostic-store-%llu-%llu",(unsigned long long)xrtThreadCurrentId(),(unsigned long long)xrtClock()); CHECK(xrtDirCreateAll(relative));
 char* root=xrtPathAbs(relative); CHECK(root);
 char *first=xrtPathJoin(root,"Δelta.xl"),*second=xrtPathJoin(root,"second.xl"),*common=xrtPathJoin(root,"common.xl"),*single=xrtPathJoin(root,"single.xl"),*unused=xrtPathJoin(root,"unused.xl"),*future=xrtPathJoin(root,"future.xl");
 CHECK(first && second && common && single && unused && future);
 const char* entry="import common\nfunc main(...) as int\n string 中文 = \"😀\"\n return common.value()\nend\n";
 const char* bad="export func value() as int\n return \"bad\"\nend\n";
 const char* good="export func value() as int\n return 7\nend\n";
 const char* independent="func main(...) as int\n return missingName\nend\n";
 xide_language_snapshot inputs[5]={{first,entry,1},{second,entry,1},{common,bad,8},{single,independent,1},{unused,"func unused()\nend\n",1}};
 xide_diagnostic_store* store=xideDiagnosticStoreCreate(NULL); CHECK(store);
 xide_diagnostic_query* query=xideDiagnosticStoreSnapshot(store); CHECK(query && waitFor(query,&snapshot)==XIDE_DIAGNOSTIC_READY && !snapshot.records && !xrtValueCount(snapshot.documents));
 CHECK(snapshot.workerThreadId!=xrtThreadCurrentId()); uint64_t worker=snapshot.workerThreadId; xideDiagnosticQueryDestroy(query);
 fixture value=capture(root,first,inputs,5,0); CHECK(value.guard && value.result);
 CHECK(xideLanguageInputGuardTouches(value.guard,common) && !xideLanguageInputGuardTouches(value.guard,unused));
 query=insert(store,"first",value); xatomic32 wakes; xrtAtomic32Init(&wakes,0);
 CHECK(query && xideDiagnosticQuerySetWake(query,wake,&wakes) && !xideDiagnosticQuerySetWake(query,wake,&wakes));
 CHECK(waitFor(query,&snapshot)==XIDE_DIAGNOSTIC_READY && snapshot.records==1 && snapshot.workerThreadId==worker);
 CHECK(xrtValueCount(snapshot.documents)==2 && errors(snapshot.documents,common)==1 && errors(snapshot.documents,second)==SIZE_MAX && errors(snapshot.documents,unused)==SIZE_MAX);
 CHECK(xrtAtomic32Load(&wakes,XMEMORY_ACQUIRE)==1);
 char* original=xrtJsonStringify(snapshot.documents,false,NULL); CHECK(original); xide_diagnostic_query* retained=query;

 value=capture(root,second,inputs,5,0); CHECK(value.guard);
 /* The same facts with a different valid object-key order must still dedupe. */
 xvalue* documents=xrtValueObjectGet(value.result,xrtStrView("documents"));
 for(size_t i=0;i<xrtValueCount(documents);++i) {
  xvalue* group=xrtValueArrayGet(documents,i); xvalue* list=xrtValueObjectGet(group,xrtStrView("diagnostics"));
  if(xrtValueCount(list)) {
   const xvalue* old=xrtValueArrayGet(list,0); xvalue* reordered=xrtValueObject();
   for(size_t j=xrtValueCount(old);j>0;--j) { xstrview key={0}; const xvalue* field=xrtValueObjectAt(old,j-1,&key); CHECK(field && xrtValueObjectSet(reordered,key,field)); }
   CHECK(xrtValueArraySet(list,0,reordered)); xrtValueRelease(reordered);
  }
 }
 query=insert(store,"second",value); CHECK(query && waitFor(query,&snapshot)==XIDE_DIAGNOSTIC_READY && snapshot.records==2);
 CHECK(xrtValueCount(snapshot.documents)==3 && errors(snapshot.documents,common)==1 && errors(snapshot.documents,first)==0 && errors(snapshot.documents,second)==0);
 xideDiagnosticQueryDestroy(query);
 value=capture(root,first,inputs,5,0); CHECK(value.guard); char* alias=xrtPathJoin(root,"δELTA.XL"); CHECK(alias && xidePathLexicalEqual(alias,first));
 documents=xrtValueObjectGet(value.result,xrtStrView("documents"));
 for(size_t i=0;i<xrtValueCount(documents);++i) {
  xvalue* group=xrtValueArrayGet(documents,i);
  if(xidePathLexicalEqual(string(get(group,"path")),first)) CHECK(xrtValueObjectSetNew(group,xrtStrView("path"),xrtValueString(xrtStrView(alias))));
 }
 query=insert(store,"unicode-alias",value); CHECK(query && waitFor(query,&snapshot)==XIDE_DIAGNOSTIC_READY && snapshot.records==3 && xrtValueCount(snapshot.documents)==3 && errors(snapshot.documents,alias)==0); xideDiagnosticQueryDestroy(query); xrtFree(alias);
 query=xideDiagnosticStoreRemove(store,"unicode-alias"); CHECK(query && waitFor(query,&snapshot)==XIDE_DIAGNOSTIC_READY && snapshot.records==2); xideDiagnosticQueryDestroy(query);
 /* Controlled metadata keeps real compiler locations/messages: equal bucket
    scalars but a distinct optional code must remain a separate diagnostic. */
 value=capture(root,second,inputs,5,0); CHECK(value.guard);
 documents=xrtValueObjectGet(value.result,xrtStrView("documents"));
 for(size_t i=0;i<xrtValueCount(documents);++i) {
  xvalue* list=xrtValueObjectGet(xrtValueArrayGet(documents,i),xrtStrView("diagnostics"));
  if(xrtValueCount(list)) CHECK(xrtValueObjectSetNew(xrtValueArrayGet(list,0),xrtStrView("code"),xrtValueString(xrtStrView("controlled-metadata"))));
 }
 query=insert(store,"decorated",value); CHECK(query && waitFor(query,&snapshot)==XIDE_DIAGNOSTIC_READY && snapshot.records==3 && errors(snapshot.documents,common)==2); xideDiagnosticQueryDestroy(query);
 query=xideDiagnosticStoreRemove(store,"decorated"); CHECK(query && waitFor(query,&snapshot)==XIDE_DIAGNOSTIC_READY && snapshot.records==2 && errors(snapshot.documents,common)==1); xideDiagnosticQueryDestroy(query);
 xide_diagnostic_snapshot old={0}; CHECK(xideDiagnosticQueryResult(retained,&old)==XIDE_DIAGNOSTIC_READY);
 char* after=xrtJsonStringify(old.documents,false,NULL); CHECK(after && !strcmp(original,after)); xrtFree(after);
 value=capture(root,single,inputs,5,0); CHECK(value.guard); query=insert(store,"single",value);
 CHECK(query && waitFor(query,&snapshot)==XIDE_DIAGNOSTIC_READY && snapshot.records==3 && errors(snapshot.documents,common)==1 && errors(snapshot.documents,single)==1); xideDiagnosticQueryDestroy(query);
 query=xideDiagnosticStoreRemove(store,"first"); CHECK(query && waitFor(query,&snapshot)==XIDE_DIAGNOSTIC_READY && snapshot.records==2 && invalidated(snapshot.invalidated,"first") && errors(snapshot.documents,common)==1 && errors(snapshot.documents,first)==SIZE_MAX); xideDiagnosticQueryDestroy(query);
 query=xideDiagnosticStoreInvalidate(store,unused); CHECK(query && waitFor(query,&snapshot)==XIDE_DIAGNOSTIC_READY && snapshot.records==2 && !xrtValueCount(snapshot.invalidated)); xideDiagnosticQueryDestroy(query);
 query=xideDiagnosticStoreInvalidate(store,common); CHECK(query && waitFor(query,&snapshot)==XIDE_DIAGNOSTIC_READY && snapshot.records==1 && invalidated(snapshot.invalidated,"second") && !invalidated(snapshot.invalidated,"single") && errors(snapshot.documents,common)==SIZE_MAX && errors(snapshot.documents,single)==1); xideDiagnosticQueryDestroy(query);
 inputs[2].text=good; inputs[2].version=9; value=capture(root,first,inputs,5,1); CHECK(value.guard); query=insert(store,"first",value);
 CHECK(query && waitFor(query,&snapshot)==XIDE_DIAGNOSTIC_READY && snapshot.records==2 && errors(snapshot.documents,common)==0 && errors(snapshot.documents,single)==1); xideDiagnosticQueryDestroy(query);

 inputs[4].text="import future\nfunc main(...) as int\n return future.value()\nend\n";
 value=capture(root,unused,inputs,5,0); CHECK(value.guard && xideLanguageInputGuardTouches(value.guard,future)); query=insert(store,"missing",value);
 CHECK(query && waitFor(query,&snapshot)==XIDE_DIAGNOSTIC_READY && snapshot.records==3); xideDiagnosticQueryDestroy(query);
 query=xideDiagnosticStoreInvalidate(store,future); CHECK(query && waitFor(query,&snapshot)==XIDE_DIAGNOSTIC_READY && snapshot.records==2 && invalidated(snapshot.invalidated,"missing") && !invalidated(snapshot.invalidated,"single")); xideDiagnosticQueryDestroy(query); inputs[4].text="func unused()\nend\n";
 /* A contradictory version is an error, and cannot replace existing facts. */
 inputs[2].version=10; value=capture(root,second,inputs,5,1); CHECK(value.guard); query=insert(store,"second",value);
 CHECK(query && waitFor(query,&snapshot)==XIDE_DIAGNOSTIC_FAILED && strstr(snapshot.error,"Conflicting") && snapshot.conflictPath && xidePathLexicalEqual(snapshot.conflictPath,common) && !snapshot.documents); xideDiagnosticQueryDestroy(query);
 query=xideDiagnosticStoreSnapshot(store); CHECK(query && waitFor(query,&snapshot)==XIDE_DIAGNOSTIC_READY && snapshot.records==2 && errors(snapshot.documents,single)==1); xideDiagnosticQueryDestroy(query); inputs[2].version=9;
 xideDiagnosticStoreDestroy(store); /* old completed snapshot still owns its DOM */
 CHECK(xideDiagnosticQueryResult(retained,&old)==XIDE_DIAGNOSTIC_READY); after=xrtJsonStringify(old.documents,false,NULL); CHECK(after && !strcmp(original,after)); xrtFree(after); xrtFree(original); xideDiagnosticQueryDestroy(retained);

 for(unsigned mode=0;mode<3;++mode) {
  xide_diagnostic_limits limits={mode==0 ? 1 : 64,mode==1 ? 1 : 32u*1024u*1024u,mode==2 ? 32 : 4u*1024u*1024u,8};
  store=xideDiagnosticStoreCreate(&limits); CHECK(store);
  value=capture(root,first,inputs,5,1); CHECK(value.guard); query=insert(store,"first",value);
  CHECK(query && waitFor(query,&snapshot)==(mode==0 ? XIDE_DIAGNOSTIC_READY : XIDE_DIAGNOSTIC_FAILED));
  if(mode) CHECK(strstr(snapshot.error,"byte limit") && !snapshot.documents);
  xideDiagnosticQueryDestroy(query);
  if(!mode) { value=capture(root,second,inputs,5,1); CHECK(value.guard); query=insert(store,"second",value); CHECK(query && waitFor(query,&snapshot)==XIDE_DIAGNOSTIC_FAILED && strstr(snapshot.error,"entry limit")); xideDiagnosticQueryDestroy(query); }
  query=xideDiagnosticStoreSnapshot(store); CHECK(query && waitFor(query,&snapshot)==XIDE_DIAGNOSTIC_READY && snapshot.records==(mode==0 ? 1u : 0u)); xideDiagnosticQueryDestroy(query); xideDiagnosticStoreDestroy(store);
 }

 /* A real disk input and unified sweep detect same-byte identity changes. */
 CHECK(writeText(common,good)); xide_language_snapshot diskInput={first,entry,1};
 store=xideDiagnosticStoreCreate(NULL); value=capture(root,first,&diskInput,1,1); CHECK(store && value.guard);
 query=insert(store,"disk",value); CHECK(query && waitFor(query,&snapshot)==XIDE_DIAGNOSTIC_READY && snapshot.records==1); xideDiagnosticQueryDestroy(query);
 CHECK(writeText(common,good)); query=xideDiagnosticStoreSweep(store);
 CHECK(query && waitFor(query,&snapshot)==XIDE_DIAGNOSTIC_READY && !snapshot.records && invalidated(snapshot.invalidated,"disk") && !xrtValueCount(snapshot.documents)); xideDiagnosticQueryDestroy(query); xideDiagnosticStoreDestroy(store);

 /* Real 4MiB disk hashing holds the single observer busy: prove bounded
    queuing/cancel isolation using its actual running state, without hooks. */
 size_t largeSize=4u*1024u*1024u; char* large=xrtMalloc(largeSize+strlen(good)+2); CHECK(large);
 memset(large,'a',largeSize); large[0]='/'; large[1]='/'; large[largeSize]='\n'; memcpy(large+largeSize+1,good,strlen(good)+1);
 CHECK(writeText(common,large)); xrtFree(large);
 store=xideDiagnosticStoreCreate(&(xide_diagnostic_limits){64,32u*1024u*1024u,4u*1024u*1024u,1});
 value=capture(root,first,&diskInput,1,1); CHECK(store && value.guard); query=insert(store,"large-disk",value);
 CHECK(query && waitFor(query,&snapshot)==XIDE_DIAGNOSTIC_READY && snapshot.records==1); xideDiagnosticQueryDestroy(query);
 query=xideDiagnosticStoreSweep(store); CHECK(query);
 xide_diagnostic_load load={0}; uint64_t began=xrtClock();
 do { CHECK(xideDiagnosticStoreLoad(store,&load)); if(load.running) break; xrtSleep(1); } while(xrtClock()-began<15000000);
 CHECK(load.running==1 && xideDiagnosticQueryResult(query,&snapshot)==XIDE_DIAGNOSTIC_PENDING);
 xide_diagnostic_query* queued=xideDiagnosticStoreSnapshot(store); CHECK(queued);
 xide_diagnostic_query* overflow=xideDiagnosticStoreSnapshot(store); CHECK(!overflow);
 xideDiagnosticQueryCancel(queued); CHECK(waitFor(queued,&snapshot)==XIDE_DIAGNOSTIC_CANCELLED && !snapshot.documents && !snapshot.invalidated); xideDiagnosticQueryDestroy(queued);
 CHECK(waitFor(query,&snapshot)==XIDE_DIAGNOSTIC_READY && snapshot.records==1); xideDiagnosticQueryDestroy(query);
 query=xideDiagnosticStoreSnapshot(store); CHECK(query && waitFor(query,&snapshot)==XIDE_DIAGNOSTIC_READY && snapshot.records==1 && snapshot.workerThreadId!=xrtThreadCurrentId()); xideDiagnosticQueryDestroy(query);
 CHECK(xideDiagnosticStoreLoad(store,&load) && load.rejected>=1 && load.cancelled>=1);
 query=xideDiagnosticStoreSweep(store); CHECK(query); xideDiagnosticStoreDestroy(store);
 enum xide_diagnostic_state ended=xideDiagnosticQueryResult(query,&snapshot); CHECK(ended==XIDE_DIAGNOSTIC_READY || ended==XIDE_DIAGNOSTIC_CANCELLED); xideDiagnosticQueryDestroy(query);

 /* Manifests are actual inputs too. Their verified result groups must not
    disappear just because they have no source-AST kind bit. */
 char* package=xrtPathJoin(root,"pkg/src"); CHECK(package && xrtDirCreateAll(package)); xrtFree(package);
 char *manifest=xrtPathJoin(root,"pkg/xlang.package.json"),*api=xrtPathJoin(root,"pkg/src/api.xl"); CHECK(manifest && api);
 const char* manifestText="{\"format\":\"xlang-package-1\",\"name\":\"pkg\",\"version\":\"1.0.0\",\"abiVersion\":\"1\",\"kind\":\"source\",\"artifact\":{\"kind\":\"source\",\"xlSources\":[\"src/api.xl\"]},\"dependencies\":[],\"links\":[],\"loadlibs\":[]}";
 CHECK(writeText(manifest,manifestText) && writeText(api,bad));
 xide_language_snapshot packageInput={first,"import pkg\nfunc main(...) as int\n return pkg.value()\nend\n",1};
 store=xideDiagnosticStoreCreate(NULL); value=capture(root,first,&packageInput,1,0); CHECK(store && value.guard && xideLanguageInputGuardTouches(value.guard,manifest));
 query=insert(store,"package",value); CHECK(query && waitFor(query,&snapshot)==XIDE_DIAGNOSTIC_READY && snapshot.records==1 && xrtValueCount(snapshot.documents)==3 && errors(snapshot.documents,manifest)==0 && errors(snapshot.documents,api)==1); xideDiagnosticQueryDestroy(query);
 CHECK(writeText(manifest,manifestText)); query=xideDiagnosticStoreSweep(store);
 CHECK(query && waitFor(query,&snapshot)==XIDE_DIAGNOSTIC_READY && !snapshot.records && invalidated(snapshot.invalidated,"package")); xideDiagnosticQueryDestroy(query);
 value=capture(root,first,&packageInput,1,0); CHECK(value.guard); query=insert(store,"package-again",value);
 CHECK(query && waitFor(query,&snapshot)==XIDE_DIAGNOSTIC_READY && snapshot.records==1);
 xide_diagnostic_publication* publication=NULL; xvalue* invalid=NULL;
 CHECK(xideDiagnosticQueryTake(query,&publication,&invalid) && publication && invalid && xideDiagnosticPublicationCount(publication)==3);
 size_t manifestIndex=xideDiagnosticPublicationFind(publication,manifest);
 CHECK(manifestIndex!=SIZE_MAX && xideDiagnosticPublicationGroup(publication,manifestIndex) && xideDiagnosticPublicationCompare(publication,manifestIndex,publication,manifestIndex)==0 && xideDiagnosticPublicationEqual(publication,manifestIndex,publication,manifestIndex));
 xide_diagnostic_publication* duplicate=publication; xvalue* duplicateKeys=invalid;
 CHECK(!xideDiagnosticQueryTake(query,&duplicate,&duplicateKeys) && !duplicate && !duplicateKeys && xideDiagnosticQueryResult(query,&snapshot)==XIDE_DIAGNOSTIC_FAILED);
 xideDiagnosticQueryDestroy(query); xrtValueRelease(invalid);
 query=xideDiagnosticStoreClear(store); CHECK(query && waitFor(query,&snapshot)==XIDE_DIAGNOSTIC_READY && !snapshot.records && invalidated(snapshot.invalidated,"package-again") && !xrtValueCount(snapshot.documents)); xideDiagnosticQueryDestroy(query);
 xideDiagnosticStoreDestroy(store); CHECK(xideDiagnosticPublicationCount(publication)==3 && xideDiagnosticPublicationFind(publication,manifest)==manifestIndex);
 xideDiagnosticPublicationDestroy(publication); xrtFree(manifest); xrtFree(api);
 xrtFree(first); xrtFree(second); xrtFree(common); xrtFree(single); xrtFree(unused); xrtFree(future); xrtFree(root); return 1;
}
int main(void)
{
 if(!run() || !dependencyScopes() || !dependencyParts()) return 1;
 printf("diagnostic_store_test passed: %u checks; real multi-entry diagnostics, exact collision-safe deduplication, version/limit failures, independent inputs/lifetimes, async ownership and4MiB disk sweep queue/cancel/shutdown.\n",checks); return 0;
}
