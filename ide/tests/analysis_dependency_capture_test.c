
#include "analysis/dependency_capture.h"
#include "analysis/frontend_entry_cache.h"
#include "analysis/frontend_sem.h"
#include "analysis/module_store.h"
#include "src/sem/xl6_sem.h"
#include "src/ir/xl6_ir.h"
#include <stdio.h>
#include <string.h>
static unsigned checks,semCalls,irCalls;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_dependency_capture_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
int __real_xl6SemCheckModuleWithIndex(XL6_AstNode*,XL6_DiagList*,const char*,int);
int __wrap_xl6SemCheckModuleWithIndex(XL6_AstNode* root,XL6_DiagList* diags,const char* module,int index)
{++semCalls;return __real_xl6SemCheckModuleWithIndex(root,diags,module,index);}
int __real_xl6IrBuildModuleFromAst(XL6_IrModule*,const XL6_AstNode*,XL6_DiagList*);
int __wrap_xl6IrBuildModuleFromAst(XL6_IrModule* module,const XL6_AstNode* root,XL6_DiagList* diags)
{++irCalls;return __real_xl6IrBuildModuleFromAst(module,root,diags);}
static int writeText(const char* p,const char* s){return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)});}
static int cancelled(void* user){return *(int*)user;}

typedef struct reader {xls_dependency_capture* capture;int okay;} reader;
static int32 readCapture(void* user)
{
 reader* r=user;r->okay=1;
 for(unsigned i=0;i<128;++i){xls_check_inputs inputs;xls_check_source source;
  if(!xlsDependencyCaptureInfo(r->capture).expanded || !xlsDependencyCaptureInputs(r->capture,&inputs) || inputs.sources!=2 || !xlsDependencyCaptureSource(r->capture,i%2,&source) || !source.path || !source.text)r->okay=0;
 }
 xlsDependencyCaptureRelease(r->capture);return 0;
}
static int run(void)
{
 char *directory=xrtPathAbs("inputs");CHECK(directory && xrtDirCreateAll(directory));char *entry=xrtPathJoin(directory,"main.xl"),*dep=xrtPathJoin(directory,"library.xl"),*bad=xrtPathJoin(directory,"bad.xl");CHECK(entry && dep && bad);
 const char *main="import library\nfunc main(...) as int\n return library.main(7)\nend\n",*body="export func main(int value) as int\n return value+1\nend\n",*changed="export func main(int value) as int\n return value+2\nend\n";CHECK(writeText(entry,main) && writeText(dep,body) && writeText(bad,"func incomplete(\n"));
 xls_semantic_input input={entry,main,strlen(main)};char error[256]={0};xls_dependency_capture* graph=xlsDependencyCaptureCreate(NULL,NULL,entry,&input,1,NULL,NULL,error,sizeof(error));CHECK(graph && xlsDependencyCaptureInfo(graph).expanded && !xlsDependencyCaptureInfo(graph).diagnostics && !strcmp(xlsDependencyCaptureInfo(graph).phase,"dependencies") && !semCalls && !irCalls);
 xls_check_inputs gi;CHECK(xlsDependencyCaptureInputs(graph,&gi) && gi.sources==2);xls_semantic_check* full=xlsSemanticCheckCreate(entry,&input,1,1,NULL,NULL,NULL,0);CHECK(full && xlsSemanticCheckInfo(full).accepted && semCalls==1 && irCalls==1);xls_check_inputs fi;CHECK(xlsSemanticCheckInputs(full,&fi) && gi.sources==fi.sources && gi.probes==fi.probes);
 for(size_t i=0;i<gi.sources;++i){xls_check_source a,b;CHECK(xlsDependencyCaptureSource(graph,i,&a) && xlsSemanticCheckSource(full,i,&b));CHECK(!strcmp(a.path,b.path) && a.size==b.size && !memcmp(a.text,b.text,a.size) && a.kind==b.kind && a.rawSize==b.rawSize && !strcmp(a.origin,b.origin) && !memcmp(a.rawSha256,b.rawSha256,32) && !memcmp(a.textSha256,b.textSha256,32));}
 for(size_t i=0;i<gi.probes;++i){xls_check_probe a,b;CHECK(xlsDependencyCaptureProbe(graph,i,&a) && xlsSemanticCheckProbe(full,i,&b));CHECK(!strcmp(a.path,b.path) && a.exists==b.exists);}
 xlsSemanticCheckDestroy(full);CHECK(xlsDependencyCaptureValidate(graph));xls_dependency_capture* held=xlsDependencyCaptureRetain(graph);xlsDependencyCaptureRelease(graph);graph=held;CHECK(xlsDependencyCaptureInfo(graph).expanded && xlsDependencyCaptureInputs(graph,&gi));
 /* Dependency success must never imply semantic success. The real compiler
    rejects this library as a program's export-main entry. */
 unsigned beforeSem=semCalls,beforeIr=irCalls;xls_dependency_capture* library=xlsDependencyCaptureCreate(NULL,NULL,dep,NULL,0,NULL,NULL,error,sizeof(error));CHECK(library && xlsDependencyCaptureInfo(library).expanded && semCalls==beforeSem && irCalls==beforeIr);full=xlsSemanticCheckCreate(dep,NULL,0,1,NULL,NULL,NULL,0);CHECK(full && !xlsSemanticCheckInfo(full).accepted && xlsSemanticCheckInfo(full).diagnostics && semCalls==beforeSem+1 && irCalls==beforeIr);xlsSemanticCheckDestroy(full);xlsDependencyCaptureRelease(library);
 CHECK(writeText(dep,changed) && !xlsDependencyCaptureValidate(graph) && !xlsDependencyCaptureInfo(graph).expanded && xlsDependencyCaptureInfo(graph).error);CHECK(writeText(dep,body) && !xlsDependencyCaptureValidate(graph));xlsDependencyCaptureRelease(graph);
 xls_semantic_input overlays[2]={{entry,main,strlen(main)},{dep,body,strlen(body)}};beforeSem=semCalls;beforeIr=irCalls;graph=xlsDependencyCaptureCreate(NULL,NULL,entry,overlays,2,NULL,NULL,error,sizeof(error));CHECK(graph && xlsDependencyCaptureInfo(graph).expanded && semCalls==beforeSem && irCalls==beforeIr);CHECK(writeText(dep,changed) && xlsDependencyCaptureValidate(graph));xlsDependencyCaptureRelease(graph);
 xls_check_inputs absent={0};CHECK(!xlsDependencyCaptureInfo(NULL).expanded && !xlsDependencyCaptureInputs(NULL,&absent));
 CHECK(!xlsDependencyCaptureCreate(NULL,NULL,NULL,NULL,0,NULL,NULL,error,sizeof(error)));
 CHECK(!xlsDependencyCaptureCreate(NULL,NULL,entry,overlays,65,NULL,NULL,error,sizeof(error)));
 CHECK(!xlsDependencyCaptureSource(NULL,0,NULL) && !xlsDependencyCaptureProbe(NULL,0,NULL) && !xlsDependencyCaptureValidate(NULL));
 int stop=1;CHECK(!xlsDependencyCaptureCreate(NULL,NULL,entry,&input,1,cancelled,&stop,error,sizeof(error)) && strstr(error,"cancelled"));
 graph=xlsDependencyCaptureCreate(NULL,NULL,bad,NULL,0,NULL,NULL,error,sizeof(error));CHECK(graph && !xlsDependencyCaptureInfo(graph).expanded && xlsDependencyCaptureInfo(graph).diagnostics);xls_check_diagnostic diagnostic;CHECK(xlsDependencyCaptureDiagnostic(graph,0,&diagnostic) && diagnostic.message);xlsDependencyCaptureRelease(graph);

 /* Existing neutral module/cache and graph assets are reused under their
    original worker-local scope. Every reader owns a reference independently. */
 xls_module_store* store=xlsModuleStoreCreate(NULL);xls_entry_cache* cache=xlsEntryCacheCreate(8u*1024u*1024u);char* project=xrtPathJoin(directory,"owner.xlp");const char* members[]={entry,dep};
 xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=project,.configuration="debug",.entry=entry,.directory=directory,.analysisProfile="xlang-ls/compiler-xrt-v1",.targetKind="exe"};spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,2};xls_context* context=xlsContextCreate(&spec,16u*1024u*1024u,NULL);CHECK(store && cache && project && context);
 xls_module_scope scope;xlsModuleScopeBegin(&scope,store);graph=xlsDependencyCaptureCreate(context,cache,entry,overlays,2,NULL,NULL,error,sizeof(error));CHECK(graph && xlsDependencyCaptureInfo(graph).expanded);const xls_module_graph* modules=xlsDependencyCaptureModules(graph);xls_module_graph_info mi=xlsModuleGraphInfo(modules);CHECK(modules && mi.modules==2 && mi.sources==2 && mi.edges==1 && mi.entry && xlsModuleGraphStore(modules)==store && xlsModuleGraphContext(modules)==context);
 xls_dependency_capture* again=xlsDependencyCaptureCreate(context,cache,entry,overlays,2,NULL,NULL,error,sizeof(error));CHECK(again && xlsDependencyCaptureInfo(again).expanded && xlsDependencyCaptureModules(again));for(size_t i=1;i<=2;++i)CHECK(xlsModuleGraphSource(modules,i)==xlsModuleGraphSource(xlsDependencyCaptureModules(again),i));
 xls_module_graph* retained=xlsModuleGraphRetain((xls_module_graph*)modules);xlsDependencyCaptureRelease(again);xlsModuleScopeEnd(&scope);xlsEntryCacheDestroy(cache);xlsModuleStoreRelease(store);xlsContextRelease(context);xrtFree(project);
 reader readers[4];xthread* threads[4];for(unsigned i=0;i<4;++i){readers[i]=(reader){xlsDependencyCaptureRetain(graph),0};threads[i]=xrtThreadCreate(readCapture,readers+i,0);CHECK(threads[i]);}xlsDependencyCaptureRelease(graph);for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}
 CHECK(xlsModuleGraphInfo(retained).entry==mi.entry && xlsModuleGraphInfo(retained).edges==1);xls_module_edge edge;CHECK(xlsModuleGraphEdge(retained,0,&edge) && edge.importer==mi.entry && edge.imported);xlsModuleGraphRelease(retained);
 xrtFree(directory);xrtFree(entry);xrtFree(dep);xrtFree(bad);printf("analysis_dependency_capture_test passed: %u checks;real original prefix/source/probe parity,zero semantic/IR calls,library/program distinction,independent ownership,disk invalidation,overlay authority,cancellation,source diagnostics,existing neutral caches/import graph,independent4reader release\n",checks);return 1;
}
int main(void){return run()?0:1;}
