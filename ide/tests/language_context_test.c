#include "analysis_test_clock.h"
#include "services/language_check.h"
#include "services/diagnostic_store.h"
#include "platform/path.h"
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(e) do { ++checks; if(!(e)) { fprintf(stderr,"language_context_test:%d %s\n",__LINE__,#e); return 0; } } while(0)
static const char* body="import context_package\nfunc main(...) as int\n return context_package.value()\nend\n";
static const char* cursor="import context_package as cp\nfunc main(...) as int\n cp.\n return 0\nend\n";
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static const xvalue* get(const xvalue* o,const char* key) { return xrtValueObjectGet(o,sv(key)); }
static int textIs(const xvalue* value,const char* expected)
{ xstrview s={0}; return xrtValueGetString(value,&s) && s.Size==strlen(expected) && !memcmp(s.Data,expected,s.Size); }
static int pathIs(const xvalue* value,const char* expected)
{ xstrview s={0}; return xrtValueGetString(value,&s) && s.Data && xidePathLexicalEqual(s.Data,expected); }
static int writeText(const char* path,const char* text)
{ return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)}); }
static xls_context* context(const char* entry,const char* directory,const char* configuration,const char* package)
{
 xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=directory,.configuration=configuration,.entry=entry,
  .directory=directory,.analysisProfile=XIDE_ANALYSIS_PROFILE,.targetKind="exe",.packagePath=package};
 return xlsContextCreate(&spec,1024*1024,NULL);
}
static char* package(const char* directory,int stringType)
{
 char* folder=xrtPathJoin(directory,"packages/context_package");
 char* manifest=folder ? xrtPathJoin(folder,"xlang.package.json") : NULL;
 char* api=folder ? xrtPathJoin(folder,"api.xl") : NULL;
 const char* json="{\"format\":\"xlang-package-1\",\"name\":\"context_package\",\"version\":\"1.0.0\",\"abiVersion\":\"1\",\"kind\":\"source\",\"artifact\":{\"kind\":\"source\",\"xlSources\":[\"api.xl\"]},\"dependencies\":[],\"links\":[],\"loadlibs\":[]}";
 int okay=folder && manifest && api && xrtDirCreateAll(folder) && writeText(manifest,json) && writeText(api,stringType ?
  "export func value() as string\n return \"B\"\nend\nexport func beta() as int\n return 2\nend\n" :
  "export func value() as int\n return 7\nend\nexport func alpha() as int\n return 1\nend\n");
 xrtFree(folder);xrtFree(manifest); if(!okay) {xrtFree(api);api=NULL;} return api;
}
static xide_language_check* start(xls_context* context,xide_toolchains* cache,const char* compiler,
 const char* directory,xls_worker* actor,const char* entry,xls_text* text,int completion)
{
 xide_language_analysis_options options={.nativeBackend=1};xide_model_error error={0};
 xls_analysis_document d={"same-entry",entry,text,17,73u-(int64_t)INT32_MIN};
 xide_language_check* result=xideLanguageCheckStartRetainedInContext(context,cache,compiler,directory,actor,&d,1,d.key,
  completion ? entry : NULL,completion ? 2 : 0,completion ? 4 : 0,&options,&error);
 if(!result) fprintf(stderr,"host context start: %s\n",error.message);
 return result;
}
static xvalue* ready(xide_language_check* check)
{
 char error[512]={0};uint64_t began=xlsTestClockMicros();enum xide_check_state state;
 while(check && (state=xideLanguageCheckState(check,error,sizeof(error)))==XIDE_CHECK_RUNNING && xlsTestClockMicros()-began<15000000) xrtSleep(1);
 xvalue* out=xideLanguageCheckResult(check);
 if(!out) fprintf(stderr,"host context result: %s\n",error);
 return out;
}
static int report(xvalue* value,int accepted,const char* directory)
{
 bool okay=false;int64_t version=0;CHECK(value && xrtValueGetBool(get(value,"ok"),&okay) && okay==accepted);
 CHECK(xrtValueGetInt(get(value,"version"),&version) && version==73);
 const xvalue* documents=get(value,"documents");char* api=xrtPathJoin(directory,"packages/context_package/api.xl");int found=0;
 for(size_t i=0;i<xrtValueCount(documents);++i) {
  const xvalue* row=xrtValueArrayGet(documents,i);
  if(pathIs(get(row,"path"),api)) {CHECK(textIs(get(row,"origin"),"disk"));++found;}
 }
 xrtFree(api);CHECK(found==1);return 1;
}
static int items(const xvalue* value,const char* wanted,const char* excluded)
{
 bool available=false;CHECK(value && xrtValueGetBool(get(value,"available"),&available) && available);
 const xvalue* list=get(value,"items");int found=0;
 for(size_t i=0;i<xrtValueCount(list);++i) {
  const xvalue* row=xrtValueArrayGet(list,i);if(textIs(get(row,"label"),wanted)) ++found;
  CHECK(!textIs(get(row,"label"),excluded));
 }
 CHECK(found==1);return 1;
}
static int stopped(void* user) { return *(int*)user; }
static int storeResult(xide_diagnostic_query* query,int expected,xide_diagnostic_snapshot* value)
{
 CHECK(query);uint64_t began=xlsTestClockMicros();enum xide_diagnostic_state state;
 while((state=xideDiagnosticQueryResult(query,value))==XIDE_DIAGNOSTIC_PENDING && xlsTestClockMicros()-began<15000000) xrtSleep(1);
 CHECK(state==(enum xide_diagnostic_state)expected);return 1;
}
static xide_analysis_catalog* selectedCatalog(xls_context* a,xls_context* b,int selected)
{
 xvalue *root=xrtValueObject(),*list=xrtValueArray();xls_context* contexts[]={a,b};int okay=root && list;
 for(size_t i=0;okay && i<2;++i) {xvalue* value=xideAnalysisContextWrite(contexts[i]);okay=value && xrtValueArrayAppend(list,value);xrtValueRelease(value);}
 okay=okay && xrtValueObjectSetNew(root,sv("schema"),xrtValueInt(1)) && xrtValueObjectSetNew(root,sv("selectedProject"),xrtValueString(sv(xlsContextSpec(contexts[selected])->project))) && xrtValueObjectSetNew(root,sv("packagePath"),xrtValueString(sv("packages")));
 if(okay) {okay=xrtValueObjectSetNew(root,sv("projects"),list);list=NULL;}
 xide_analysis_catalog* catalog=okay ? xideAnalysisCatalogCreate(root,NULL,0) : NULL;xrtValueRelease(list);xrtValueRelease(root);return catalog;
}
static int selectedPublication(xide_diagnostic_query* query,const char* entry,const char* included,const char* excluded,int errors)
{
 xide_diagnostic_publication* publication=NULL;xvalue* invalidated=NULL;CHECK(xideDiagnosticQueryTake(query,&publication,&invalidated));
 size_t index=xideDiagnosticPublicationFind(publication,entry);CHECK(index!=SIZE_MAX);
 CHECK((xrtValueCount(get(xideDiagnosticPublicationGroup(publication,index),"diagnostics"))>0)==errors);
 const char* id=xideDiagnosticPublicationGroupDependency(publication,index);CHECK(id);
 const xvalue* chunks=xideDiagnosticPublicationDependencyChunks(publication,xideDiagnosticPublicationFindDependency(publication,id));CHECK(chunks);
 int found=0,foreign=0;
 for(size_t i=0;i<xrtValueCount(chunks);++i) {const xvalue* paths=get(xrtValueArrayGet(chunks,i),"paths");
  for(size_t j=0;j<xrtValueCount(paths);++j) {const xvalue* value=xrtValueArrayGet(paths,j);if(pathIs(value,included)) found=1;if(pathIs(value,excluded)) foreign=1;}
 }
 CHECK(found && !foreign);xideDiagnosticPublicationDestroy(publication);xrtValueRelease(invalidated);return 1;
}
static int run(void)
{
 char relative[160];snprintf(relative,sizeof(relative),"build/host-context-%llu-%llu",(unsigned long long)xrtThreadCurrentId(),(unsigned long long)xlsTestClockMicros());
 CHECK(xrtDirCreateAll(relative));char* root=xrtPathAbs(relative);char *a=xrtPathJoin(root,"工程 A"),*b=xrtPathJoin(root,"工程 B"),*entry=xrtPathJoin(root,"same-entry.xl"),*compiler=xrtPathJoin(root,"absent/xl.exe");
 CHECK(root && a && b && entry && compiler && writeText(entry,"// original entry is not an overlay file write\n"));
 char* apiA=package(a,0);char* apiB=package(b,1);CHECK(apiA && apiB);
 xide_model_error error={0};char* previous=NULL;CHECK(xideLanguageCheckPackagePath(&previous,&error));char* global=xrtPathJoin(b,"packages");CHECK(global && xrtEnvSet("XLANG_PACKAGE_PATH",global));
 xls_context *ca=context(entry,a,"debug","packages"),*cb=context(entry,b,"release","packages");CHECK(ca && cb);
 xls_text* text=xlsTextCreate(body,strlen(body),4096,NULL);xls_text* queryText=xlsTextCreate(cursor,strlen(cursor),4096,NULL);CHECK(text && queryText);
 xide_toolchains* cache=xideToolchainsCreate();xls_worker *actor=xlsWorkerCreate(NULL,8),*other=xlsWorkerCreate(NULL,8);CHECK(cache && actor && other);
 xide_language_check *first=start(ca,cache,compiler,a,actor,entry,text,0),*second=start(cb,cache,compiler,b,other,entry,text,0);CHECK(first && second);
 xvalue *ra=ready(first),*rb=ready(second);CHECK(report(ra,1,a) && report(rb,0,b));
 xide_language_input_guard *ga=xideLanguageCheckCaptureInputGuard(first,&error),*gb=xideLanguageCheckCaptureInputGuard(second,&error);CHECK(ga && gb);
 CHECK(xideLanguageInputGuardSameContext(ga,ga) && !xideLanguageInputGuardSameContext(ga,gb));
 CHECK(!xideLanguageInputGuardSameContext(NULL,ga) && !xideLanguageInputGuardSameContext(ga,NULL));
 CHECK(xideLanguageInputGuardTouches(ga,apiA) && !xideLanguageInputGuardTouches(ga,apiB) && xideLanguageInputGuardTouches(gb,apiB));
 CHECK(xideLanguageInputGuardReportBytes(ga)>xlsContextBytes(ca));
 xideLanguageCheckDestroy(first);xideLanguageCheckDestroy(second);
 first=start(ca,cache,compiler,a,actor,entry,queryText,1);second=start(cb,cache,compiler,b,other,entry,queryText,1);CHECK(first && second);
 xvalue *ac=ready(first),*bc=ready(second);CHECK(items(ac,"alpha","beta") && items(bc,"beta","alpha"));xrtValueRelease(ac);xrtValueRelease(bc);
 xideLanguageCheckDestroy(first);xideLanguageCheckDestroy(second);
 /* A cursor without a retained actor still uses the same explicit context. */
 first=start(ca,cache,compiler,a,NULL,entry,queryText,1);CHECK(first);ac=ready(first);CHECK(items(ac,"alpha","beta"));xrtValueRelease(ac);xideLanguageCheckDestroy(first);
 CHECK(!start(ca,cache,compiler,b,actor,entry,text,0));
 xls_context_spec unsupported=*xlsContextSpec(ca);const char* include="C:/unsupported";unsupported.lists[XLS_CONTEXT_INCLUDES]=(xls_context_strings){&include,1};
 xls_context* invalid=xlsContextCreate(&unsupported,4096,NULL);CHECK(invalid && !start(invalid,cache,compiler,a,actor,entry,text,0));xlsContextRelease(invalid);
 /* NULL is the legacy host contract, not inherited explicit package state. */
 first=start(NULL,cache,compiler,root,actor,entry,text,0);CHECK(first);ac=ready(first);CHECK(report(ac,0,b));
 xide_language_input_guard* legacy=xideLanguageCheckCaptureInputGuard(first,&error);CHECK(legacy);xrtValueRelease(ac);xideLanguageCheckDestroy(first);
 /* Same actual files and raw environment, so only the retained context is
    an additional metadata charge. Do not infer accounting from a lower bound. */
 char* canonicalEntry=xidePathResolveLexical(NULL,entry);char* processDirectory=xrtPathAbs(".");CHECK(canonicalEntry && processDirectory);
 xls_context* charged=context(canonicalEntry,processDirectory,"byte-accounting",global);CHECK(charged);
 first=start(charged,cache,compiler,processDirectory,actor,entry,text,0);CHECK(first);ac=ready(first);CHECK(report(ac,0,b));
 xide_language_input_guard* chargedGuard=xideLanguageCheckCaptureInputGuard(first,&error);CHECK(chargedGuard);
 CHECK(xideLanguageInputGuardReportBytes(chargedGuard)==xideLanguageInputGuardReportBytes(legacy)+xlsContextBytes(charged));
 xrtValueRelease(ac);xideLanguageCheckDestroy(first);xlsContextRelease(charged);free(canonicalEntry);xrtFree(processDirectory);
 CHECK(xrtEnvSet("XLANG_PACKAGE_PATH",a));
 CHECK(!xideLanguageInputGuardValidate(legacy,NULL,NULL,&error) && xideLanguageInputGuardValidate(chargedGuard,NULL,NULL,&error));
 xideLanguageInputGuardRelease(legacy);xideLanguageInputGuardRelease(chargedGuard);
 int cancel=1;CHECK(!xideLanguageInputGuardValidate(ga,stopped,&cancel,&error) && xideLanguageInputGuardCurrent(ga));
 CHECK(xideLanguageInputGuardValidate(ga,NULL,NULL,&error) && xideLanguageInputGuardValidate(gb,NULL,NULL,&error));
 xide_diagnostic_store* store=xideDiagnosticStoreCreate(NULL);CHECK(store);
 xide_diagnostic_query* put=xideDiagnosticStorePutTake(store,"context-A",ga,ra);xideLanguageInputGuardRelease(ga);CHECK(put);ga=NULL;ra=NULL;
 xide_diagnostic_snapshot state={0};CHECK(storeResult(put,XIDE_DIAGNOSTIC_READY,&state));xideDiagnosticQueryDestroy(put);
 xls_context* equal=context(entry,a,"debug","packages");CHECK(equal);
 first=start(equal,cache,compiler,a,actor,entry,text,0);CHECK(first);ac=ready(first);CHECK(report(ac,1,a));
 xide_language_input_guard* equivalent=xideLanguageCheckCaptureInputGuard(first,&error);CHECK(equivalent && !xideLanguageInputGuardSameContext(equivalent,gb));
 xideLanguageCheckDestroy(first);xlsContextRelease(equal);
 put=xideDiagnosticStorePutTake(store,"context-A-copy",equivalent,ac);xideLanguageInputGuardRelease(equivalent);
 CHECK(storeResult(put,XIDE_DIAGNOSTIC_READY,&state) && state.records==2);xideDiagnosticQueryDestroy(put);
 put=xideDiagnosticStorePutTake(store,"context-B-conflict",gb,xrtValueDeepClone(rb));CHECK(storeResult(put,XIDE_DIAGNOSTIC_FAILED,&state));
 CHECK(state.conflictPath && xidePathLexicalEqual(state.conflictPath,entry) && strstr(state.error,"contexts"));xideDiagnosticQueryDestroy(put);
 put=xideDiagnosticStoreSnapshot(store);CHECK(storeResult(put,XIDE_DIAGNOSTIC_READY,&state) && state.records==2);xideDiagnosticQueryDestroy(put);
 put=xideDiagnosticStoreRemove(store,"context-A-copy");CHECK(storeResult(put,XIDE_DIAGNOSTIC_READY,&state) && state.records==1);xideDiagnosticQueryDestroy(put);
 char* separate=xrtPathJoin(root,"separate-entry.xl");CHECK(separate && writeText(separate,"// distinct disk entry\n"));
 xls_context* disjoint=context(separate,b,"release","packages");CHECK(disjoint);
 first=start(disjoint,cache,compiler,b,other,separate,text,0);CHECK(first);ac=ready(first);CHECK(report(ac,0,b));
 xide_language_input_guard* disjointGuard=xideLanguageCheckCaptureInputGuard(first,&error);CHECK(disjointGuard && !xideLanguageInputGuardSameContext(disjointGuard,gb));
 xideLanguageCheckDestroy(first);xlsContextRelease(disjoint);
 put=xideDiagnosticStorePutTake(store,"context-B-disjoint",disjointGuard,ac);xideLanguageInputGuardRelease(disjointGuard);
 CHECK(storeResult(put,XIDE_DIAGNOSTIC_READY,&state) && state.records==2);xideDiagnosticQueryDestroy(put);
 put=xideDiagnosticStoreRemove(store,"context-B-disjoint");CHECK(storeResult(put,XIDE_DIAGNOSTIC_READY,&state) && state.records==1);xideDiagnosticQueryDestroy(put);xrtFree(separate);
 /* A bounded store charges retained context bytes; failure retains no record. */
 xide_diagnostic_limits tiny={.records=2,.retainedReportBytes=1,.publishedReportBytes=1024,.queue=2};xide_diagnostic_store* small=xideDiagnosticStoreCreate(&tiny);CHECK(small);
 put=xideDiagnosticStorePutTake(small,"too-small",gb,xrtValueDeepClone(rb));CHECK(storeResult(put,XIDE_DIAGNOSTIC_FAILED,&state));xideDiagnosticQueryDestroy(put);xideDiagnosticStoreDestroy(small);
 /* Configured publication selects one existing checked context per file,
    including only that context's actual source/probe dependency proofs. */
 first=start(ca,cache,compiler,a,actor,entry,text,0);CHECK(first);ac=ready(first);CHECK(report(ac,1,a));
 xide_language_input_guard* selectedGuard=xideLanguageCheckCaptureInputGuard(first,&error);CHECK(selectedGuard);xideLanguageCheckDestroy(first);
 xide_analysis_catalog *catalogA=selectedCatalog(ca,cb,0),*catalogB=selectedCatalog(ca,cb,1);CHECK(catalogA && catalogB);
 xide_diagnostic_store* scoped=xideDiagnosticStoreCreateWithDependencies(NULL,1);CHECK(scoped);
 put=xideDiagnosticStorePutConfigured(scoped,"A",selectedGuard,ac,catalogA);CHECK(storeResult(put,XIDE_DIAGNOSTIC_READY,&state));xideDiagnosticQueryDestroy(put);
 put=xideDiagnosticStorePutConfigured(scoped,"B",gb,xrtValueDeepClone(rb),catalogA);CHECK(storeResult(put,XIDE_DIAGNOSTIC_READY,&state) && state.records==2);
 CHECK(selectedPublication(put,entry,apiA,apiB,0));xideDiagnosticQueryDestroy(put);
 put=xideDiagnosticStorePutConfigured(scoped,"B",gb,xrtValueDeepClone(rb),catalogB);CHECK(storeResult(put,XIDE_DIAGNOSTIC_READY,&state) && state.records==2);
 CHECK(selectedPublication(put,entry,apiB,apiA,1));xideDiagnosticQueryDestroy(put);
 xideAnalysisCatalogDestroy(catalogA);xideAnalysisCatalogDestroy(catalogB);xideLanguageInputGuardRelease(selectedGuard);
 xlsContextRelease(ca);xlsContextRelease(cb);xlsWorkerDestroy(actor);xlsWorkerDestroy(other);xideToolchainsDestroy(cache);xlsTextRelease(text);xlsTextRelease(queryText);
 CHECK(xideLanguageInputGuardValidate(gb,NULL,NULL,&error));
 put=xideDiagnosticStoreSweep(store);CHECK(storeResult(put,XIDE_DIAGNOSTIC_READY,&state) && state.records==1);xideDiagnosticQueryDestroy(put);
 CHECK(writeText(apiA,"export func value() as int\n return 8\nend\n"));put=xideDiagnosticStoreSweep(store);
 CHECK(storeResult(put,XIDE_DIAGNOSTIC_READY,&state) && !state.records && xrtValueCount(state.invalidated)==1);xideDiagnosticQueryDestroy(put);xideDiagnosticStoreDestroy(store);
 put=xideDiagnosticStoreSweep(scoped);CHECK(storeResult(put,XIDE_DIAGNOSTIC_READY,&state) && state.records==1 && selectedPublication(put,entry,apiB,apiA,1));xideDiagnosticQueryDestroy(put);
 CHECK(xideLanguageInputGuardValidate(gb,NULL,NULL,&error) && writeText(apiB,"export func value() as string\n return \"changed\"\nend\n"));
 CHECK(!xideLanguageInputGuardValidate(gb,NULL,NULL,&error) && !xideLanguageInputGuardCurrent(gb));xideLanguageInputGuardRelease(gb);xrtValueRelease(rb);
 put=xideDiagnosticStoreSweep(scoped);CHECK(storeResult(put,XIDE_DIAGNOSTIC_READY,&state) && !state.records);xideDiagnosticQueryDestroy(put);xideDiagnosticStoreDestroy(scoped);
 CHECK(previous ? xrtEnvSet("XLANG_PACKAGE_PATH",previous) : xrtEnvRemove("XLANG_PACKAGE_PATH"));free(previous);
 xrtFree(global);xrtFree(apiA);xrtFree(apiB);xrtFree(compiler);xrtFree(entry);xrtFree(a);xrtFree(b);xrtFree(root);return 1;
}
int main(void)
{
 if(!run()) return 1;
 printf("language_context_test passed: %u checks; real host directory/package isolation, retained/copy cursor, detached guards, cancellation, actor/store ownership and disk invalidation.\n",checks);
 return 0;
}
