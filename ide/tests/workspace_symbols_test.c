#include "services/workspace_symbols.h"
#include "platform/path.h"
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
static size_t operations,faultAt;static volatile LONG64 nativeKeys;static int tracking;
static int operation(void){return !tracking || ++operations!=faultAt;}
static void* actorMalloc(size_t n){return operation()?xrtMalloc(n):NULL;}
static void* actorCalloc(size_t n,size_t size){return operation()?xrtCalloc(n,size):NULL;}
static char* actorDup(const char* p){return operation()?xrtStrDup(p):NULL;}
static bool actorSha(const void* p,size_t n,void* out){return operation() && xrtSha256(p,n,out);}
static bool actorArrayInit(xarray* a,size_t size){return operation() && xrtArrayInit(a,size);}
static bool actorArrayPush(xarray* a,const void* p){return operation() && xrtArrayPush(a,p);}
static bool actorArraySort(xarray* a,xarraycompare compare){return operation() && xrtArraySort(a,compare);}
static bool actorMapInit(xmap* a,size_t size){return operation() && xrtMapInit(a,size);}
static bool actorMapSet(xmap* a,xbytesview key,const void* p){return operation() && xrtMapSet(a,key,p);}
static wchar_t* actorPathKey(const char* p){wchar_t* key=operation()?xidePathLexicalWide(p):NULL;if(key)InterlockedIncrement64(&nativeKeys);return key;}
static char* actorResolve(const char* directory,const char* p){char* path=operation()?xidePathResolveLexical(directory,p):NULL;if(path)InterlockedIncrement64(&nativeKeys);return path;}
static void actorFree(void* p){if(p)InterlockedDecrement64(&nativeKeys);free(p);}
static const char *mutatePath,*mutateBody;static int mutationArmed,mutationObserved;
static xide_workspace_symbol_result* actorQuery(xide_workspace_symbol_snapshot* s,const char* query,size_t limit,int (*stop)(void*),void* user,xide_model_error* e)
{xide_workspace_symbol_result* result=xideWorkspaceSymbolSnapshotQuery(s,query,limit,stop,user,e);if(result && mutationArmed){mutationArmed=0;mutationObserved=xrtFileWriteAtomic(mutatePath,(xbytesview){(const unsigned char*)mutateBody,strlen(mutateBody)});}return result;}
#define xrtMalloc actorMalloc
#define xrtCalloc actorCalloc
#define xrtStrDup actorDup
#define xrtSha256 actorSha
#define xrtArrayInit actorArrayInit
#define xrtArrayPush actorArrayPush
#define xrtArraySort actorArraySort
#define xrtMapInit actorMapInit
#define xrtMapSet actorMapSet
#define xidePathLexicalWide actorPathKey
#define xidePathResolveLexical actorResolve
#define xideWorkspaceSymbolSnapshotQuery actorQuery
#define free actorFree
#include "../src/services/workspace_symbols.c"
#undef xrtMalloc
#undef xrtCalloc
#undef xrtStrDup
#undef xrtSha256
#undef xrtArrayInit
#undef xrtArrayPush
#undef xrtArraySort
#undef xrtMapInit
#undef xrtMapSet
#undef xidePathLexicalWide
#undef xidePathResolveLexical
#undef xideWorkspaceSymbolSnapshotQuery
#undef free
static size_t checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"workspace_symbols_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static int writeText(const char* path,const char* body){return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)body,strlen(body)});}
typedef struct fixture {char *directory,*entry,*library,*orphan,*other,*project,*compiler,*absent;xide_toolchains* cache;xls_context *scope,*release;} fixture;
static const char* mainBody="import a_library as api\nfunc main(...) as int\n return api.compute(7)\nend\n";
static const char* libraryBody="/* 中文😀 */ class Payload\n int number\nend\nexport func compute(int value) as int\n return value+1\nend\n";
static const char* dirtyBody="/* 中文😀 */ class Payload\n int total\nend\nexport func compute(int value) as int\n return value+2\nend\n";
static const char* orphanBody="func orphanValue() as int\n return 3\nend\nfunc main(...) as int\n return orphanValue()\nend\n";
static int setup(fixture* f)
{
 f->directory=xrtPathAbs("build/workspace-symbols");CHECK(f->directory && xrtDirCreateAll(f->directory));f->entry=xrtPathJoin(f->directory,"main.xl");f->library=xrtPathJoin(f->directory,"a_library.xl");f->orphan=xrtPathJoin(f->directory,"unopened.xl");f->other=xrtPathJoin(f->directory,"z_importer.xl");f->project=xrtPathJoin(f->directory,"项目😀.xlp");f->compiler=xrtPathJoin(f->directory,"absent-xl.exe");f->absent=xrtPathJoin(f->directory,"unused.xl");CHECK(f->entry && f->library && f->orphan && f->other && f->project && f->compiler && f->absent);
 CHECK(writeText(f->entry,mainBody) && writeText(f->library,libraryBody) && writeText(f->orphan,orphanBody) && writeText(f->other,mainBody));
 const char* members[]={f->library,f->other,f->orphan,f->entry};xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=f->project,.configuration="Debug",.entry=f->entry,.directory=f->directory,.analysisProfile="xlang-ls/compiler-xrt-v1",.targetKind="exe"};spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,4};f->scope=xlsContextCreate(&spec,16u*1024u*1024u,NULL);spec.configuration="Release";f->release=xlsContextCreate(&spec,16u*1024u*1024u,NULL);f->cache=xideToolchainsCreate();CHECK(f->scope && f->release && f->cache);return 1;
}
static void clear(fixture* f){xlsContextRelease(f->scope);xlsContextRelease(f->release);xideToolchainsDestroy(f->cache);xrtFree(f->directory);xrtFree(f->entry);xrtFree(f->library);xrtFree(f->orphan);xrtFree(f->other);xrtFree(f->project);xrtFree(f->compiler);xrtFree(f->absent);}
typedef struct observed {enum xide_workspace_symbol_state state;xide_workspace_symbol_result* result;xide_workspace_symbol_work work;char error[512];} observed;
static void signalWake(void* p){xrtEventSet(p);}
static observed waitQuery(xide_workspace_symbol_query* query,int wake)
{
 observed result={0};if(!query){result.state=XIDE_WORKSPACE_SYMBOL_FAILED;return result;}xevent* event=wake?xrtEventCreate(false,false):NULL;if(event && !xideWorkspaceSymbolsSetWake(query,signalWake,event))abort();
 for(unsigned i=0;;++i){result.state=xideWorkspaceSymbolsResult(query,NULL,&result.work,result.error,sizeof(result.error));if(result.state!=XIDE_WORKSPACE_SYMBOL_PENDING)break;if(i==30000)abort();if(event)xrtEventWaitFor(event,XIDE_XRT_TIMEOUT_MS(1));else xrtSleep(1);}
 if(result.state==XIDE_WORKSPACE_SYMBOL_READY)result.result=xideWorkspaceSymbolsTake(query);
 xideWorkspaceSymbolsQueryDestroy(query);if(event)xrtEventDestroy(event);return result;
}
static observed query(xide_workspace_symbols* store,xls_context* const* contexts,size_t contextCount,const xls_analysis_document* docs,size_t count,const char* text)
{return waitQuery(xideWorkspaceSymbolsSubmit(store,contexts,contextCount,docs,count,text,4096),1);}
static size_t named(const xide_workspace_symbol_result* result,const char* name)
{size_t found=0;for(size_t i=0;i<xideWorkspaceSymbolResultCount(result);++i){xide_workspace_symbol_view row;if(xideWorkspaceSymbolResultAt(result,i,&row) && !strcmp(row.declaration.name,name))++found;}return found;}
static int scopesCurrent(const xide_workspace_symbol_result* result,xls_context* const* scopes,size_t count)
{for(size_t i=0;i<xideWorkspaceSymbolResultCount(result);++i){xide_workspace_symbol_view row;if(!xideWorkspaceSymbolResultAt(result,i,&row))return 0;int found=0;for(size_t j=0;j<count;++j)found|=xlsContextEqual(row.scope,scopes[j]);if(!found)return 0;}return 1;}
static int sameRow(const xide_semantic_symbol_view* a,const xide_semantic_symbol_view* b)
{return xidePathLexicalEqual(a->path,b->path) && !strcmp(a->name,b->name) && a->kind==b->kind && !memcmp(&a->start,&b->start,sizeof(a->start)) && !memcmp(&a->end,&b->end,sizeof(a->end)) && !memcmp(&a->selectionStart,&b->selectionStart,sizeof(a->selectionStart)) && !memcmp(&a->selectionEnd,&b->selectionEnd,sizeof(a->selectionEnd));}
static int oracle(fixture* f,const observed* actual,const char* const* entries,size_t count,const xls_analysis_document* docs,size_t documentCount)
{
 xls_worker* actor=xlsWorkerCreate(NULL,8);CHECK(actor);xide_semantic_symbols* captures[8]={0};xide_semantic_symbol_view expected[128];size_t n=0;
 for(size_t i=0;i<count;++i){xls_context_spec spec=*xlsContextSpec(f->scope);spec.entry=entries[i];spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){0};xls_context* context=xlsContextCreate(&spec,16u*1024u*1024u,NULL);xide_language_analysis_options options={.nativeBackend=1};xide_language_check* check=xideLanguageCheckStartDeclarationCapture(context,f->cache,f->compiler,actor,docs,documentCount,&options,NULL);xlsContextRelease(context);CHECK(check);
  for(unsigned spin=0;xideLanguageCheckState(check,NULL,0)==XIDE_CHECK_RUNNING;++spin){if(spin>=30000)CHECK(0);xrtSleep(1);}CHECK(xideLanguageCheckCaptureDeclarations(check,captures+i,NULL));xideLanguageCheckDestroy(check);
  for(size_t j=0;j<xideSemanticSymbolsCount(captures[i]);++j){xide_semantic_symbol_view row;CHECK(xideSemanticSymbolAt(captures[i],j,&row));int exists=0;for(size_t k=0;k<n;++k)exists|=sameRow(expected+k,&row);if(!exists){CHECK(n<128);expected[n++]=row;}}
 }
 CHECK(actual->state==XIDE_WORKSPACE_SYMBOL_READY && xideWorkspaceSymbolResultCount(actual->result)==n && xideWorkspaceSymbolResultMatches(actual->result)==n);
 for(size_t i=0;i<n;++i){size_t found=0;for(size_t j=0;j<n;++j){xide_workspace_symbol_view row;CHECK(xideWorkspaceSymbolResultAt(actual->result,j,&row));found+=sameRow(expected+i,&row.declaration);}CHECK(found==1);}
 for(size_t i=0;i<count;++i)xideSemanticSymbolsDestroy(captures[i]);
 xlsWorkerDestroy(actor);return 1;
}
static int basic(fixture* f)
{
 xide_workspace_symbols* store=xideWorkspaceSymbolsCreate(f->cache,f->compiler,NULL);CHECK(store);xls_context* scopes[]={f->scope};const char* entries[]={f->entry,f->orphan,f->other};observed cold=query(store,scopes,1,NULL,0,"");if(cold.state!=XIDE_WORKSPACE_SYMBOL_READY)fprintf(stderr,"cold:%s\n",cold.error);CHECK(oracle(f,&cold,entries,3,NULL,0));CHECK(cold.work.captured==3 && cold.work.reused==0 && cold.work.covered==1 && cold.work.roots==4 && cold.work.analysisThread && cold.work.analysisThread!=xrtThreadCurrentId() && cold.work.coordinatorThread!=xrtThreadCurrentId() && cold.work.coordinatorThread!=cold.work.analysisThread);
 observed warm=query(store,scopes,1,NULL,0,"");CHECK(warm.state==XIDE_WORKSPACE_SYMBOL_READY && !warm.work.captured && warm.work.reused==3 && warm.work.covered==1 && warm.work.index.generation==2 && named(warm.result,"number")==1);xideWorkspaceSymbolResultDestroy(warm.result);
 xls_text* dirty=xlsTextCreate(dirtyBody,strlen(dirtyBody),1024*1024,NULL);xls_analysis_document docs[]={{"dependency",f->library,dirty,1,1}};CHECK(dirty);observed edited=query(store,scopes,1,docs,1,"");CHECK(oracle(f,&edited,entries,3,docs,1));CHECK(edited.work.captured==2 && edited.work.reused==1 && named(edited.result,"total")==1 && !named(edited.result,"number") && named(cold.result,"number")==1 && edited.work.overlayHashBytes==strlen(dirtyBody));xideWorkspaceSymbolResultDestroy(edited.result);
 warm=query(store,scopes,1,docs,1,"");CHECK(warm.state==XIDE_WORKSPACE_SYMBOL_READY && warm.work.reused==3 && !warm.work.captured);xideWorkspaceSymbolResultDestroy(warm.result);
 xls_text* original=xlsTextCreate(libraryBody,strlen(libraryBody),1024*1024,NULL);CHECK(original);docs[0].text=original;observed stale=query(store,scopes,1,docs,1,"");CHECK(stale.state==XIDE_WORKSPACE_SYMBOL_FAILED && !stale.result);docs[0].revision=2;observed restored=query(store,scopes,1,docs,1,"");CHECK(restored.state==XIDE_WORKSPACE_SYMBOL_READY && named(restored.result,"number")==1);xideWorkspaceSymbolResultDestroy(restored.result);
 docs[0].revision=1;stale=query(store,scopes,1,docs,1,"");CHECK(stale.state==XIDE_WORKSPACE_SYMBOL_FAILED);docs[0].identity=2;restored=query(store,scopes,1,docs,1,"");CHECK(restored.state==XIDE_WORKSPACE_SYMBOL_READY);xideWorkspaceSymbolResultDestroy(restored.result);
 restored=query(store,scopes,1,NULL,0,"");CHECK(restored.state==XIDE_WORKSPACE_SYMBOL_READY && named(restored.result,"number")==1);xideWorkspaceSymbolResultDestroy(restored.result);
 CHECK(writeText(f->library,dirtyBody));edited=query(store,scopes,1,NULL,0,"");CHECK(edited.state==XIDE_WORKSPACE_SYMBOL_READY && named(edited.result,"total")==1 && edited.work.captured==2);xideWorkspaceSymbolResultDestroy(edited.result);CHECK(writeText(f->library,libraryBody));
 xls_context_spec spec=*xlsContextSpec(f->scope);char* otherProject=xrtPathJoin(f->directory,"second.xlp");CHECK(otherProject);spec.project=otherProject;xls_context* secondProject=xlsContextCreate(&spec,16u*1024u*1024u,NULL);xls_context* both[]={f->scope,f->release,secondProject};CHECK(secondProject);observed multi=query(store,both,3,NULL,0,"Payload number");CHECK(multi.state==XIDE_WORKSPACE_SYMBOL_READY && named(multi.result,"number")==3 && scopesCurrent(multi.result,both,3));xideWorkspaceSymbolResultDestroy(multi.result);xlsContextRelease(secondProject);xrtFree(otherProject);
 spec=*xlsContextSpec(f->scope);const char* only[]={f->library};spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){only,1};xls_context* narrow=xlsContextCreate(&spec,16u*1024u*1024u,NULL);xls_context* selected[]={narrow};CHECK(narrow);restored=query(store,selected,1,NULL,0,"");CHECK(restored.state==XIDE_WORKSPACE_SYMBOL_READY && !named(restored.result,"orphanValue") && named(restored.result,"Payload")==1 && scopesCurrent(restored.result,selected,1));xideWorkspaceSymbolResultDestroy(restored.result);xlsContextRelease(narrow);
 observed empty=query(store,NULL,0,NULL,0,"");CHECK(empty.state==XIDE_WORKSPACE_SYMBOL_READY && !xideWorkspaceSymbolResultCount(empty.result) && !empty.work.index.parts);xideWorkspaceSymbolResultDestroy(empty.result);
 xideWorkspaceSymbolsDestroy(store);CHECK(named(cold.result,"number")==1);xideWorkspaceSymbolResultDestroy(cold.result);xlsTextRelease(dirty);xlsTextRelease(original);return 1;
}
static int failures(fixture* f)
{
 xide_workspace_symbols* store=xideWorkspaceSymbolsCreate(f->cache,f->compiler,NULL);xls_context* scopes[]={f->scope};CHECK(store);observed baseline=query(store,scopes,1,NULL,0,"");CHECK(baseline.state==XIDE_WORKSPACE_SYMBOL_READY);uint64_t generation=baseline.work.index.generation;xideWorkspaceSymbolResultDestroy(baseline.result);
 CHECK(writeText(f->orphan,"func main(...) as int\n return \"bad\"\nend\n"));observed invalid=query(store,scopes,1,NULL,0,"");CHECK(invalid.state==XIDE_WORKSPACE_SYMBOL_FAILED && !invalid.result);xide_workspace_symbol_snapshot* snapshot=xideWorkspaceSymbolIndexAcquire(store->index);CHECK(xideWorkspaceSymbolSnapshotStats(snapshot).generation==generation);xideWorkspaceSymbolSnapshotRelease(snapshot);CHECK(writeText(f->orphan,orphanBody));
 mutatePath=f->library;mutateBody=dirtyBody;mutationArmed=1;mutationObserved=0;invalid=query(store,scopes,1,NULL,0,"");CHECK(mutationObserved && invalid.state==XIDE_WORKSPACE_SYMBOL_FAILED && !invalid.result);CHECK(writeText(f->library,libraryBody));
 /* Library program-role rejection is deferred only until a real importer
    checks it;the configured primary can never use this escape. */
 const char* lib="export func main(int value) as int\n return value+1\nend\n";const char* importer="import a_library as api\nfunc main(...) as int\n return api.main(7)\nend\n";CHECK(writeText(f->entry,"func main(...) as int\n return 0\nend\n") && writeText(f->library,lib) && writeText(f->other,importer));observed covered=query(store,scopes,1,NULL,0,"");CHECK(covered.state==XIDE_WORKSPACE_SYMBOL_READY && covered.work.sourceRejections==1 && named(covered.result,"main")==4);xideWorkspaceSymbolResultDestroy(covered.result);
 xls_context_spec spec=*xlsContextSpec(f->scope);spec.entry=f->library;xls_context* wrong=xlsContextCreate(&spec,16u*1024u*1024u,NULL);xls_context* badScopes[]={wrong};invalid=query(store,badScopes,1,NULL,0,"");CHECK(invalid.state==XIDE_WORKSPACE_SYMBOL_FAILED && !invalid.result);xlsContextRelease(wrong);
 CHECK(writeText(f->entry,mainBody) && writeText(f->library,libraryBody) && writeText(f->other,mainBody));
 xide_workspace_symbol_query* queued[40]={0};size_t admitted=0,refused=0;for(size_t i=0;i<40;++i){queued[i]=xideWorkspaceSymbolsSubmit(store,scopes,1,NULL,0,"",4096);if(queued[i])++admitted;else ++refused;}CHECK(admitted && refused);for(size_t i=0;i<40;++i){xideWorkspaceSymbolsCancel(queued[i]);xideWorkspaceSymbolsQueryDestroy(queued[i]);}
 xideWorkspaceSymbolsDestroy(store);CHECK(!nativeKeys);return 1;
}
static int overlayTopology(fixture* f)
{
 xide_workspace_symbols* store=xideWorkspaceSymbolsCreate(f->cache,f->compiler,NULL);xls_context* scopes[]={f->scope};CHECK(store);observed base=query(store,scopes,1,NULL,0,"");CHECK(base.state==XIDE_WORKSPACE_SYMBOL_READY);xideWorkspaceSymbolResultDestroy(base.result);
 xls_text* unused=xlsTextCreate("broken unused",13,1024,NULL);CHECK(unused);xls_analysis_document docs[2]={{"unrelated",f->absent,unused,55,1}};observed result=query(store,scopes,1,docs,1,"");CHECK(result.state==XIDE_WORKSPACE_SYMBOL_READY && !result.work.captured && result.work.reused==3 && !result.work.overlayHashBytes);xideWorkspaceSymbolResultDestroy(result.result);
 xls_text* changed=xlsTextCreate("other invalid",13,1024,NULL);CHECK(changed);docs[0].text=changed;result=query(store,scopes,1,docs,1,"");CHECK(result.state==XIDE_WORKSPACE_SYMBOL_FAILED && !result.result);docs[0].revision=2;result=query(store,scopes,1,docs,1,"");CHECK(result.state==XIDE_WORKSPACE_SYMBOL_READY && result.work.reused==3 && !result.work.overlayHashBytes);xideWorkspaceSymbolResultDestroy(result.result);
 char* sub=xrtPathJoin(f->directory,"fallback-program"),*entry=sub?xrtPathJoin(sub,"main.xl"):NULL;CHECK(sub && entry && xrtDirCreateAll(sub) && writeText(entry,mainBody));char* package=xrtPathJoin(sub,"packages/a_library");char *manifest=package?xrtPathJoin(package,"xlang.package.json"):NULL,*api=package?xrtPathJoin(package,"api.xl"):NULL;const char* json="{\"format\":\"xlang-package-1\",\"name\":\"a_library\",\"version\":\"1.0.0\",\"abiVersion\":\"1\",\"kind\":\"source\",\"artifact\":{\"kind\":\"source\",\"xlSources\":[\"api.xl\"]},\"dependencies\":[],\"links\":[],\"loadlibs\":[]}";CHECK(package && manifest && api && xrtDirCreateAll(package) && writeText(manifest,json) && writeText(api,libraryBody));xls_context_spec spec=*xlsContextSpec(f->scope);spec.entry=entry;spec.directory=sub;spec.packagePath="packages";spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){0};xls_context* fallback=xlsContextCreate(&spec,16u*1024u*1024u,NULL);xls_context* fallbackScopes[]={fallback};CHECK(fallback);result=query(store,fallbackScopes,1,NULL,0,"");CHECK(result.state==XIDE_WORKSPACE_SYMBOL_READY);xideWorkspaceSymbolResultDestroy(result.result);
 char* negative=NULL;
 for(size_t i=0;i<store->records.Count && !negative;++i){symbol_record* record=((symbol_record**)store->records.Data)[i];const xvalue* probes=xideLanguageInputGuardProbes(record->guard);for(size_t j=0;j<xrtValueCount(probes);++j){const xvalue* p=xrtValueArrayGet(probes,j);bool exists=true;const char* path=string(field(p,"path"));if(path && xrtValueGetBool(field(p,"exists"),&exists) && !exists){negative=xrtStrDup(path);break;}}}
 CHECK(negative);printf("Negative topology candidate: %s\n",negative);docs[1]=(xls_analysis_document){"negative",negative,unused,56,1};result=query(store,fallbackScopes,1,docs,2,"");CHECK(result.state!=XIDE_WORKSPACE_SYMBOL_READY || result.work.captured);xideWorkspaceSymbolResultDestroy(result.result);xrtFree(negative);xlsContextRelease(fallback);xrtFree(sub);xrtFree(entry);xrtFree(package);xrtFree(manifest);xrtFree(api);
 result=query(store,scopes,1,NULL,0,"");CHECK(result.state==XIDE_WORKSPACE_SYMBOL_READY);xideWorkspaceSymbolResultDestroy(result.result);
 xstrbuf body={0};CHECK(xrtStrBufAppend(&body,xrtStrView("\xef\xbb\xbf")) && xrtStrBufAppend(&body,xrtStrView(libraryBody)));xls_text* withBom=xlsTextCreate(body.Data,body.Size,1024*1024,NULL);xrtStrBufFree(&body);CHECK(withBom);docs[0]=(xls_analysis_document){"dependency",f->library,withBom,57,1};result=query(store,scopes,1,docs,1,"");const char* entries[]={f->entry,f->orphan,f->other};CHECK(oracle(f,&result,entries,3,docs,1));xideWorkspaceSymbolResultDestroy(result.result);
 xls_text* noBom=xlsTextCreate(libraryBody,strlen(libraryBody),1024*1024,NULL);CHECK(noBom);docs[0].text=noBom;docs[0].revision=2;result=query(store,scopes,1,docs,1,"");CHECK(result.state==XIDE_WORKSPACE_SYMBOL_READY && result.work.captured==2 && oracle(f,&result,entries,3,docs,1));xideWorkspaceSymbolResultDestroy(result.result);
 char* equalBody=xrtStrDup(libraryBody);CHECK(equalBody && strstr(equalBody,"number"));memcpy(strstr(equalBody,"number"),"amount",6);xls_text* sameSize=xlsTextCreate(equalBody,strlen(equalBody),1024*1024,NULL);xrtFree(equalBody);CHECK(sameSize && xlsTextSize(sameSize)==xlsTextSize(noBom));docs[0].text=sameSize;docs[0].revision=3;result=query(store,scopes,1,docs,1,"");CHECK(result.state==XIDE_WORKSPACE_SYMBOL_READY && result.work.captured==2 && named(result.result,"amount")==1 && !named(result.result,"number"));xideWorkspaceSymbolResultDestroy(result.result);
 xlsTextRelease(sameSize);xlsTextRelease(unused);xlsTextRelease(changed);xlsTextRelease(withBom);xlsTextRelease(noBom);xideWorkspaceSymbolsDestroy(store);CHECK(!nativeKeys);return 1;
}
static int faults(fixture* f)
{
 xls_context* scopes[]={f->scope};xls_text* text=xlsTextCreate(dirtyBody,strlen(dirtyBody),1024*1024,NULL);xls_analysis_document doc={"dependency",f->library,text,1,1};CHECK(text);size_t total=0,successes=0,failures=0,constructor=0;
 for(size_t at=0;;++at){tracking=1;operations=0;faultAt=at;xide_workspace_symbols* store=xideWorkspaceSymbolsCreate(f->cache,f->compiler,NULL);tracking=0;if(!at){CHECK(store);constructor=operations;}else CHECK(!store);xideWorkspaceSymbolsDestroy(store);CHECK(!nativeKeys);if(at==constructor)break;}
 for(unsigned mode=0;mode<2;++mode){total=successes=failures=0;
 for(size_t at=0;;++at){xide_workspace_symbols* store=xideWorkspaceSymbolsCreate(f->cache,f->compiler,NULL);CHECK(store);tracking=1;operations=0;faultAt=at;observed result=query(store,scopes,1,mode?&doc:NULL,mode,"");tracking=0;if(!at){CHECK(result.state==XIDE_WORKSPACE_SYMBOL_READY);total=operations;CHECK(total>20);}else CHECK(operations>=at);
  if(result.state==XIDE_WORKSPACE_SYMBOL_READY){CHECK(named(result.result,mode?"total":"number")==1 && named(result.result,"orphanValue")==1);++successes;}else{CHECK(!result.result);++failures;}
  xideWorkspaceSymbolResultDestroy(result.result);xideWorkspaceSymbolsDestroy(store);CHECK(!nativeKeys);if(at==total)break;
 }
 printf("Workspace actor boundary refusals mode%u=%zu;successful optional fallbacks=%zu;failed/cancelled=%zu;all native path owners balanced.\n",mode,total,successes-1,failures);
 }xlsTextRelease(text);printf("Constructor refusals=%zu.\n",constructor);return 1;
}
static int wide(fixture* f)
{
 xide_workspace_symbols* store=xideWorkspaceSymbolsCreate(f->cache,f->compiler,NULL);CHECK(store);const char* members[52];const xls_context_spec* original=xlsContextSpec(f->scope);for(size_t i=0;i<4;++i)members[i]=original->lists[XLS_CONTEXT_MEMBERS].items[i];char* paths[48]={0};
 for(unsigned i=0;i<48;++i){char name[32],body[160];snprintf(name,sizeof(name),"extra-%02u.xl",i);snprintf(body,sizeof(body),"func extraValue%02u() as int\n return %u\nend\nfunc main(...) as int\n return extraValue%02u()\nend\n",i,i,i);paths[i]=xrtPathJoin(f->directory,name);CHECK(paths[i] && writeText(paths[i],body));members[4+i]=paths[i];}
 xls_context_spec spec=*original;spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,52};xls_context* context=xlsContextCreate(&spec,16u*1024u*1024u,NULL);xls_context* scopes[]={context};CHECK(context);observed cold=query(store,scopes,1,NULL,0,"");CHECK(cold.state==XIDE_WORKSPACE_SYMBOL_READY && cold.work.roots==52 && cold.work.captured==51 && cold.work.covered==1 && cold.work.index.parts==51 && named(cold.result,"main")==51);
 for(unsigned i=0;i<48;++i){char name[32];snprintf(name,sizeof(name),"extraValue%02u",i);CHECK(named(cold.result,name)==1);}
 observed warm=query(store,scopes,1,NULL,0,"");CHECK(warm.state==XIDE_WORKSPACE_SYMBOL_READY && !warm.work.captured && warm.work.reused==51 && !warm.work.analysisThread && xideWorkspaceSymbolResultCount(warm.result)==xideWorkspaceSymbolResultCount(cold.result));
 printf("Workspace52 roots:51 checked owner shards;warm refresh reused all51 despite8-session analysis LRU;rows=%zu,indexBytes=%zu,guardBytes=%zu.\n",warm.work.index.rows,warm.work.index.bytes,warm.work.guardBytes);xideWorkspaceSymbolResultDestroy(cold.result);xideWorkspaceSymbolResultDestroy(warm.result);xideWorkspaceSymbolsDestroy(store);xlsContextRelease(context);for(size_t i=0;i<48;++i)xrtFree(paths[i]);CHECK(!nativeKeys);return 1;
}
int main(void)
{fixture f={0};if(!setup(&f) || !basic(&f) || !failures(&f) || !overlayTopology(&f) || !faults(&f) || !wide(&f))return 1;clear(&f);printf("workspace_symbols_test passed: %zu checks;actual unopened/imported/multiple-scope symbols,fresh capture union,guarded reuse,overlay/disk/history/config/membership/BOM/negative-candidate changes,rejection coverage,late mutation,queue,cancellation,allocation refusal,52 roots and detached result lifetime.\n",checks);return 0;}
