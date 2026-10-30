#include "analysis_test_clock.h"
#include "analysis/index_entities.h"
#include "analysis/check_ir.h"
#include "analysis/frontend_ir_memo.h"
#include "analysis/module_store.h"
#include "analysis/worker_modules.h"
#define main irEntityBaseMain
#include "analysis_worker_cache_test.c"
#undef main

static char* program(unsigned version)
{
 xstrbuf s={0};int okay=xrtStrBufAppend(&s,xrtStrView("\xef\xbb\xbfimport dep\r\n"));
 if(version>=1 && version<=4)okay=okay && xrtStrBufAppend(&s,xrtStrView("func inserted(int value) as int\r\n return value\r\nend\r\n"));
 okay=okay && xrtStrBufAppend(&s,xrtStrView("func caller(int value) as int\r\n int local=dep.compute(value)\r\n"));
 if(version>=2)okay=okay && xrtStrBufAppend(&s,xrtStrView(" int extra=value+1\r\n // shifted locals 中文😀\r\n"));
 okay=okay && xrtStrBufAppend(&s,xrtStrView(" if local > 0 then\r\n return local\r\n end\r\n return value\r\nend\r\n"));
 for(unsigned i=0;okay && i<40;++i){
  if(i==0 || i==39)okay=xrtStrBufAppendFormat(&s,"func helper_%u(int value) as int\r\n if value > 0 then\r\n return helper_1(value)\r\n end\r\n return 0\r\nend\r\n",i);
  else okay=xrtStrBufAppendFormat(&s,"func helper_%u(int value) as int\r\n if value > 0 then\r\n return value+%u\r\n end\r\n return 0\r\nend\r\n",i,i);
 }
 okay=okay && xrtStrBufAppend(&s,xrtStrView("func main(...) as int\r\n int unique_main=caller(7)\r\n return unique_main\r\nend\r\n"));
 char* text=okay ? xrtStrDup(s.Data) : NULL;xrtStrBufFree(&s);return text;
}
static uint32_t named(const xls_semantic_check* check,const char* name,uint64_t* identity)
{
 const XL6_AstNode* root=xlsSemanticCheckAst(check);const xls_entity_view* view=xlsSemanticIndexEntities(xlsSemanticCheckTypedIndex(check));
 for(size_t i=0;i<root->childCount;++i){const XL6_AstNode* n=root->children[i];if(n->kind!=XL6_AST_FUNC || !n->childCount || !n->children[0]->text || strcmp(n->children[0]->text,name))continue;*identity=xlsEntityViewSymbol(view,n->semanticDeclaredSymbolId);return n->semanticDeclaredSymbolId;}
 *identity=0;return 0;
}
static char* irDump(const XL6_IrModule* ir)
{XL6_Output out;if(!xl6OutputOpenMemory(&out))return NULL;xl6IrDumpModuleJson(&out,ir);char* value=xl6OutputHasFailed(&out) ? NULL : xl6OutputTakeMemoryText(&out,NULL);xl6OutputDiscard(&out);return value;}
static int freshIr(const xls_semantic_check* result,xls_context* context,const xls_analysis_document* docs)
{
 xls_semantic_input inputs[2];for(size_t i=0;i<2;++i)inputs[i]=(xls_semantic_input){docs[i].path,xlsTextData(docs[i].text),xlsTextSize(docs[i].text)};
 xls_semantic_check* fresh=xlsSemanticCheckCreateInContext(context,docs[0].path,inputs,2,1,NULL,NULL,NULL,0);CHECK(fresh && xlsSemanticCheckInfo(fresh).accepted);
 char *a=irDump(xlsSemanticCheckIr(result)),*b=irDump(xlsSemanticCheckIr(fresh));CHECK(a && b && !strcmp(a,b));xrtFree(a);xrtFree(b);xlsSemanticCheckDestroy(fresh);return 1;
}
typedef struct ir_reader {const xls_ir_result* result;const char* expected;int okay;} ir_reader;
static int32 readIr(void* user)
{
 ir_reader* r=user;r->okay=1;for(unsigned i=0;i<8;++i){char* text=irDump(xlsFrontendIrResultModule(r->result));if(!text || strcmp(text,r->expected))r->okay=0;xrtFree(text);}return 0;
}
static int run(void)
{
 char folder[128];snprintf(folder,sizeof(folder),"build/ir-entities-%llu",(unsigned long long)xlsTestClockMicros());char* directory=xrtPathAbs(folder);CHECK(directory && xrtDirCreateAll(directory));char *entry=xrtPathJoin(directory,"main.xl"),*dep=xrtPathJoin(directory,"dep.xl"),*project=xrtPathJoin(directory,"sample.xlp");CHECK(entry && dep && project);
 const char* members[]={entry,dep};xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=project,.configuration="debug",.entry=entry,.directory=directory,.analysisProfile="ir-entity-fixture",.targetKind="exe"};spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,2};xls_context* context=xlsContextCreate(&spec,1024*1024,NULL);xls_module_store* store=xlsModuleStoreCreate(NULL);xls_worker* worker=xlsWorkerCreateWithModules(NULL,4,NULL,store);CHECK(context && store && worker);
 xls_analysis_document docs[2]={{"entry",entry,NULL,1,0},{"dep",dep,NULL,2,0}};fixture_result held={0};xls_text *heldEntry=NULL,*heldDep=NULL;uint32_t originalSymbol=0;uint64_t originalIdentity=0;unsigned originalVersion=0;
 for(unsigned v=0;v<7;++v){
  char* body=program(v);const char* dependency=v==3 ? "export func compute(int input) as int\r\n return input+1\r\nend\r\n" : v==6 ? "export func compute(int value) as int\r\n return value+2\r\nend\r\n" : "export func compute(int value) as int\r\n return value+1\r\nend\r\n";CHECK(body);
  xlsTextRelease(docs[0].text);xlsTextRelease(docs[1].text);docs[0].text=xlsTextCreate(body,strlen(body),1024*1024,NULL);docs[1].text=xlsTextCreate(dependency,strlen(dependency),1024*1024,NULL);docs[0].revision=docs[1].revision=v+1;CHECK(docs[0].text && docs[1].text);
  fixture_result r=query(worker,CHECK_KIND,context,docs,2,"entry","entry");CHECK(r.state==XLS_QUERY_READY && parity(&r,CHECK_KIND,context,docs,2,"entry","entry") && freshIr(r.check,context,docs));
  const xls_entity_view* view=xlsSemanticIndexEntities(xlsSemanticCheckTypedIndex(r.check));uint64_t identity=0;uint32_t symbol=named(r.check,"helper_0",&identity);CHECK(view && symbol && identity && xlsEntityViewNativeSymbol(view,identity)==symbol && !xlsEntityViewNativeSymbol(view,UINT64_MAX));
  if(!v){originalSymbol=symbol;originalIdentity=identity;held=r;heldEntry=xlsTextRetain(docs[0].text);heldDep=xlsTextRetain(docs[1].text);originalVersion=v+1;}
  else{
   xls_check_ir_work work=xlsSemanticCheckIrWork(r.check);fprintf(stdout,"v%u native=%u stable=%llu IR check=%llu reuse=%llu\n",v,symbol,(unsigned long long)identity,(unsigned long long)work.checked,(unsigned long long)work.reused);
   CHECK(identity==originalIdentity);if(v==1)CHECK(symbol!=originalSymbol);
   CHECK(work.reused>=40 && work.checked>0);releaseResult(&r);
  }
  xrtFree(body);
 }
 /* A new registry can assign numerically equal external IDs. Store namespace
  * equality remains mandatory before any normalized key can be considered. */
 xls_ir_result* previous=xlsFrontendIrResultBuildEntities(xlsSemanticCheckAst(held.check),NULL,NULL,32u*1024u*1024u,xlsSemanticIndexEntities(xlsSemanticCheckTypedIndex(held.check)));CHECK(previous);
 xls_module_store* foreign=xlsModuleStoreCreate(NULL);xls_worker* other=xlsWorkerCreateWithModules(NULL,4,NULL,foreign);CHECK(foreign && other);
 xls_analysis_document oldDocs[2]={{"entry",entry,heldEntry,1,originalVersion},{"dep",dep,heldDep,2,originalVersion}};fixture_result otherResult=query(other,CHECK_KIND,context,oldDocs,2,"entry","entry");CHECK(otherResult.state==XLS_QUERY_READY && parity(&otherResult,CHECK_KIND,context,oldDocs,2,"entry","entry"));
 const xls_entity_view* otherView=xlsSemanticIndexEntities(xlsSemanticCheckTypedIndex(otherResult.check));CHECK(otherView && xlsEntityViewStore(otherView)!=xlsEntityViewStore(xlsSemanticIndexEntities(xlsSemanticCheckTypedIndex(held.check))));
 xls_ir_result* next=xlsFrontendIrResultBuildEntities(xlsSemanticCheckAst(otherResult.check),NULL,previous,32u*1024u*1024u,otherView);CHECK(next && !xlsFrontendIrResultWork(next).reused);
 char *a=irDump(xlsFrontendIrResultModule(next)),*b=irDump(xlsSemanticCheckIr(otherResult.check));CHECK(a && b && !strcmp(a,b));xrtFree(a);xrtFree(b);xlsFrontendIrResultDestroy(next);
 next=xlsFrontendIrResultBuildEntities(xlsSemanticCheckAst(otherResult.check),NULL,previous,0,otherView);CHECK(next && !xlsFrontendIrResultWork(next).reused);a=irDump(xlsFrontendIrResultModule(next));b=irDump(xlsSemanticCheckIr(otherResult.check));CHECK(a && b && !strcmp(a,b));xrtFree(a);xrtFree(b);xlsFrontendIrResultDestroy(next);
 releaseResult(&otherResult);xlsWorkerDestroy(other);xlsModuleStoreRelease(foreign);
 spec.configuration="release";xls_context* release=xlsContextCreate(&spec,1024*1024,NULL);other=xlsWorkerCreateWithModules(NULL,4,NULL,store);CHECK(release && other);otherResult=query(other,CHECK_KIND,release,oldDocs,2,"entry","entry");CHECK(otherResult.state==XLS_QUERY_READY && parity(&otherResult,CHECK_KIND,release,oldDocs,2,"entry","entry"));otherView=xlsSemanticIndexEntities(xlsSemanticCheckTypedIndex(otherResult.check));const xls_entity_view* oldView=xlsSemanticIndexEntities(xlsSemanticCheckTypedIndex(held.check));CHECK(otherView && xlsEntityViewStore(otherView)==xlsEntityViewStore(oldView) && xlsEntityViewInfo(otherView).domain!=xlsEntityViewInfo(oldView).domain);
 next=xlsFrontendIrResultBuildEntities(xlsSemanticCheckAst(otherResult.check),NULL,previous,32u*1024u*1024u,otherView);CHECK(next && !xlsFrontendIrResultWork(next).reused);a=irDump(xlsFrontendIrResultModule(next));b=irDump(xlsSemanticCheckIr(otherResult.check));CHECK(a && b && !strcmp(a,b));xrtFree(a);xrtFree(b);xlsFrontendIrResultDestroy(next);releaseResult(&otherResult);xlsWorkerDestroy(other);xlsContextRelease(release);
 a=irDump(xlsFrontendIrResultModule(previous));CHECK(a);releaseResult(&held);xlsTextRelease(heldEntry);xlsTextRelease(heldDep);xlsTextRelease(docs[0].text);xlsTextRelease(docs[1].text);xlsWorkerDestroy(worker);xlsModuleStoreRelease(store);xlsContextRelease(context);
 ir_reader readers[4];xthread* threads[4];for(size_t i=0;i<4;++i){readers[i]=(ir_reader){previous,a,0};threads[i]=xrtThreadCreate(readIr,&readers[i],0);CHECK(threads[i]);}for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}xrtFree(a);xlsFrontendIrResultDestroy(previous);
 xrtFree(entry);xrtFree(dep);xrtFree(project);xrtFree(directory);return 1;
}
int main(void)
{if(!run())return 1;printf("analysis_ir_entity_test passed: %u checks;actual named IDs shift across function insertion/removal,local moves and imported header/body changes;current dependency rebind,complete fresh parity,foreign store/configuration and zero-budget fallback,four readers after all original owners close\n",checks);return 0;}
