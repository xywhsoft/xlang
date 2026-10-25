#include "analysis/frontend_module_graph.h"
#include "analysis/frontend_cancel.h"
#include "analysis/index_modules.h"
#include "analysis/worker_modules.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#define main originalWorkerFixtureMain
#include "analysis_worker_cache_test.c"
#undef main
#pragma GCC diagnostic pop
static uint64_t named(const xls_module_graph* graph,const char* path,unsigned* roles)
{
 for(size_t i=0;i<xlsModuleGraphInfo(graph).modules;++i){xls_module_node n;if(xlsModuleGraphNode(graph,i,&n) && !strcmp(n.path,path)){if(roles)*roles=n.roles;return n.identity;}}return 0;
}
static int edge(const xls_module_graph* graph,uint64_t from,uint64_t to)
{for(size_t i=0;i<xlsModuleGraphInfo(graph).edges;++i){xls_module_edge e;if(xlsModuleGraphEdge(graph,i,&e) && e.importer==from && e.imported==to)return 1;}return 0;}
static void diamond(char** paths,int complete)
{
 char* a[]={paths[0]},*ab[]={paths[0],paths[1]},*abc[]={paths[0],paths[1],paths[2]},*ac[]={paths[0],paths[2]};
 xlsModuleGraphImport(a,1);xlsModuleGraphImport(ab,2);xlsModuleGraphImport(abc,3);xlsModuleGraphImport(ac,2);xlsModuleGraphImport(ab,2);xlsModuleGraphExpanded(a,1,complete);
}
typedef struct graph_reader {const xls_module_graph* graph;uint64_t root,leaf;int okay;} graph_reader;
static int32 readGraph(void* user)
{
 graph_reader* r=user;r->okay=1;
 for(size_t i=0;i<128;++i){xls_module_graph_info info=xlsModuleGraphInfo(r->graph);if(info.entry!=r->root || info.modules!=4 || !edge(r->graph,r->root,r->leaf) || !xlsModuleGraphSource(r->graph,4))r->okay=0;}
 return 0;
}
static int cancelGraph(void* user){(void)user;return 1;}
static int isolated(void)
{
 char* paths[]={xrtPathAbs("graph-根😀.xl"),xrtPathAbs("graph-child.xl"),xrtPathAbs("graph-leaf.xl"),xrtPathAbs("graph-package.json")};for(size_t i=0;i<4;++i)CHECK(paths[i]);
 char* project=xrtPathAbs("graph.xlp");char* directory=xrtPathAbs(".");const char* members[]={paths[0],paths[2]};
 xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=project,.configuration="debug",.entry=paths[0],.directory=directory,.analysisProfile="module-graph-fixture",.targetKind="exe"};spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,2};
 xls_context* context=xlsContextCreate(&spec,1024*1024,NULL);xls_module_store* store=xlsModuleStoreCreate(NULL);CHECK(project && directory && context && store);
 XL6_SourceReportItem rows[4]={0};for(size_t i=0;i<4;++i){rows[i].path=paths[i];rows[i].kind=i==3 ? XL6_SOURCE_REPORT_MANIFEST : XL6_SOURCE_REPORT_SOURCE;}
 XL6_SourceReport report={.items=rows,.count=4,.complete=1};xls_module_graph_scope scope,inner;
 xlsModuleGraphBegin(&scope,store,context,32768);diamond(paths,1);xls_module_graph* graph=xlsModuleGraphEnd(&scope,&report);CHECK(graph);
 xls_module_graph_info info=xlsModuleGraphInfo(graph);CHECK(info.modules==4 && info.edges==3 && info.sources==4 && info.bytes<=32768 && xlsModuleGraphStore(graph)==store && xlsModuleGraphContext(graph)==context);
 unsigned roles=0;uint64_t root=named(graph,paths[0],&roles),child=named(graph,paths[1],NULL),leaf=named(graph,paths[2],NULL);CHECK(root && root==info.entry && child && leaf && roles==(XLS_MODULE_SOURCE|XLS_MODULE_ENTRY|XLS_MODULE_DECLARED_MEMBER));
 CHECK(edge(graph,root,child) && edge(graph,child,leaf) && edge(graph,root,leaf));CHECK(named(graph,paths[3],&roles) && roles==XLS_MODULE_MANIFEST);
 for(size_t i=0;i<4;++i)CHECK(xlsModuleGraphSource(graph,i+1)==named(graph,paths[i],NULL));
 CHECK(!xlsModuleGraphSource(graph,0) && !xlsModuleGraphSource(graph,5));
 xls_module_node n={.identity=99};xls_module_edge e={99,99};CHECK(!xlsModuleGraphNode(graph,4,&n) && !n.identity && !xlsModuleGraphEdge(graph,3,&e) && !e.importer);
 size_t budgets[]={0,1,128,512,1024,2048,4096,8192};
 for(size_t i=0;i<sizeof(budgets)/sizeof(*budgets);++i){xlsModuleGraphBegin(&scope,store,context,budgets[i]);diamond(paths,1);xls_module_graph* small=xlsModuleGraphEnd(&scope,&report);CHECK(!small || (xlsModuleGraphInfo(small).bytes<=budgets[i] && xlsModuleGraphInfo(small).entry==root));xlsModuleGraphRelease(small);}
 xlsModuleGraphBegin(&scope,store,context,32768);diamond(paths,0);CHECK(!xlsModuleGraphEnd(&scope,&report));
 xlsModuleGraphBegin(&scope,store,context,32768);char* only[]={paths[0]};xlsModuleGraphImport(only,1);CHECK(!xlsModuleGraphEnd(&scope,&report));
 xlsModuleGraphBegin(&scope,store,context,32768);xlsModuleGraphBegin(&inner,NULL,NULL,0);diamond(paths,1);CHECK(!xlsModuleGraphEnd(&inner,&report));diamond(paths,1);xls_module_graph* nested=xlsModuleGraphEnd(&scope,&report);CHECK(nested && xlsModuleGraphInfo(nested).edges==3);xlsModuleGraphRelease(nested);
 report.complete=0;xlsModuleGraphBegin(&scope,store,context,32768);diamond(paths,1);CHECK(!xlsModuleGraphEnd(&scope,&report));report.complete=1;
 xlsModuleGraphBegin(&scope,store,context,32768);diamond(paths,1);xls_module_graph* second=xlsModuleGraphEnd(&scope,&report);CHECK(second && xlsModuleGraphInfo(second).entry==root && named(second,paths[2],NULL)==leaf);xlsModuleGraphRelease(second);
 xls_frontend_cancel_scope cancellation;xlsFrontendCancelBegin(&cancellation,cancelGraph,NULL);xlsModuleGraphBegin(&scope,store,context,32768);diamond(paths,1);CHECK(!xlsModuleGraphEnd(&scope,&report));xls_frontend_cancel_work work;xlsFrontendCancelEnd(&cancellation,&work);CHECK(work.cancelled);
 graph_reader readers[4];xthread* threads[4];for(size_t i=0;i<4;++i){readers[i]=(graph_reader){graph,root,leaf,0};threads[i]=xrtThreadCreate(readGraph,&readers[i],0);CHECK(threads[i]);}
 for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}
 xlsContextRelease(context);xlsModuleStoreRelease(store);CHECK(xlsModuleGraphInfo(graph).entry==root && !strcmp(xlsContextSpec(xlsModuleGraphContext(graph))->project,project));xlsModuleGraphRelease(graph);
 for(size_t i=0;i<4;++i)xrtFree(paths[i]);
 xrtFree(project);xrtFree(directory);return 1;
}
static const xls_module_graph* modules(const fixture_result* r,unsigned kind)
{return kind==CHECK_KIND ? xlsSemanticCheckModules(r->check) : xlsSemanticCompletionModules(kind==COMPLETION_KIND ? r->completion : xlsSignatureSemantic(r->signature));}
static int bindingParity(const xls_semantic_check* check,const xls_module_graph* graph)
{
 const xls_semantic_index* index=xlsSemanticCheckTypedIndex(check);CHECK(index && xlsSemanticIndexModules(index)==graph);xls_inputs captured;CHECK(xlsSemanticCheckInputs(check,&captured));
 for(size_t s=0;s<captured.sources;++s){xls_input_source row;CHECK(xlsSemanticCheckSource(check,s,&row) && xlsSemanticIndexModuleIdentity(index,s+1)==named(graph,row.path,NULL));}
 for(size_t i=0;i<xlsSemanticIndexCount(index);++i){xls_index_fact fact;CHECK(xlsSemanticIndexFact(index,i,&fact));CHECK(xlsSemanticIndexModuleIdentity(index,fact.source)==xlsModuleGraphSource(graph,fact.source) && xlsSemanticIndexModuleIdentity(index,fact.selectionSource)==xlsModuleGraphSource(graph,fact.selectionSource));}
 return 1;
}
static int nativeWorkers(void)
{
 char relative[128];snprintf(relative,sizeof(relative),"build/module-graph-%llu",(unsigned long long)xrtClock());char* root=xrtPathAbs(relative);CHECK(root && xrtDirCreateAll(root));
 const char* names[]={"main.xl","dep.xl","leaf.xl","other.xl","unread.xl"};char* paths[5];xls_analysis_document docs[5]={0};
 const char* source="\xef\xbb\xbfimport dep\r\nimport other\r\nfunc choose(int left,string right) as int\r\n return left\r\nend\r\nfunc main(...) as int\r\n int unique_main=choose(dep.value(),\"hi\")\r\n return unique_main\r\nend\r\n";
 const char* dep="import leaf\nexport func value() as int\n return leaf.base()\nend\n";const char* leaf="export func base() as int\n return 7\nend\n";
 for(size_t i=0;i<5;++i){paths[i]=xrtPathJoin(root,names[i]);const char* body=i==0 ? source : i==1 ? dep : i==2 ? leaf : i==3 ? "import leaf\nexport func second() as int\n return leaf.base()+1\nend\n" : "func incomplete(";docs[i]=(xls_analysis_document){names[i],paths[i],xlsTextCreate(body,strlen(body),1024*1024,NULL),i+1,1};CHECK(paths[i] && docs[i].text);}
 char* project=xrtPathJoin(root,"graph.xlp");const char* members[]={paths[0],paths[1],paths[2],paths[3],paths[4]};xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=project,.configuration="debug",.entry=paths[0],.directory=root,.analysisProfile="module-graph-native-fixture",.targetKind="exe"};spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,5};xls_context* context=xlsContextCreate(&spec,1024*1024,NULL);CHECK(project && context);
 xls_module_store* store=xlsModuleStoreCreate(NULL);CHECK(store);xls_worker* workers[3];fixture_result held[3]={0};uint64_t ids[4]={0};
 for(unsigned k=0;k<3;++k){
  workers[k]=xlsWorkerCreateWithModules(NULL,4,NULL,store);CHECK(workers[k]);held[k]=query(workers[k],k,context,docs,5,names[0],names[0]);CHECK(held[k].state==XLS_QUERY_READY && parity(&held[k],k,context,docs,5,names[0],names[0]));
  const xls_module_graph* graph=modules(&held[k],k);CHECK(graph && xlsModuleGraphInfo(graph).modules==4 && xlsModuleGraphInfo(graph).edges==4 && xlsContextEqual(xlsModuleGraphContext(graph),context));
  for(size_t i=0;i<4;++i){unsigned roles=0;uint64_t id=named(graph,paths[i],&roles);CHECK(id && (roles&XLS_MODULE_DECLARED_MEMBER) && (roles&XLS_MODULE_SOURCE));if(!k)ids[i]=id;else CHECK(id==ids[i]);}
  CHECK(edge(graph,ids[0],ids[1]) && edge(graph,ids[1],ids[2]) && edge(graph,ids[0],ids[3]) && edge(graph,ids[3],ids[2]) && !named(graph,paths[4],NULL));
  if(!k)CHECK(bindingParity(held[k].check,graph));
 }
 xlsTextRelease(docs[2].text);const char* changed="export func base() as int\n int extra=8\n return extra\nend\n";docs[2].text=xlsTextCreate(changed,strlen(changed),1024*1024,NULL);++docs[2].revision;CHECK(docs[2].text);
 for(unsigned k=0;k<3;++k){fixture_result result=query(workers[k],k,context,docs,4,names[0],names[0]);CHECK(result.state==XLS_QUERY_READY && parity(&result,k,context,docs,4,names[0],names[0]));for(size_t i=0;i<4;++i)CHECK(named(modules(&result,k),paths[i],NULL)==ids[i]);releaseResult(&result);}
 const char* reordered="\xef\xbb\xbfimport other\r\nimport dep\r\nfunc choose(int left,string right) as int\r\n return left\r\nend\r\nfunc main(...) as int\r\n int unique_main=choose(dep.value(),\"hi\")\r\n return unique_main\r\nend\r\n";xlsTextRelease(docs[0].text);docs[0].text=xlsTextCreate(reordered,strlen(reordered),1024*1024,NULL);++docs[0].revision;CHECK(docs[0].text);
 for(unsigned k=0;k<3;++k){fixture_result result=query(workers[k],k,context,docs,4,names[0],names[0]);CHECK(result.state==XLS_QUERY_READY && parity(&result,k,context,docs,4,names[0],names[0]));for(size_t i=0;i<4;++i)CHECK(named(modules(&result,k),paths[i],NULL)==ids[i]);if(!k)CHECK(bindingParity(result.check,modules(&result,k)));releaseResult(&result);}
 spec.configuration="release";xls_context* release=xlsContextCreate(&spec,1024*1024,NULL);CHECK(release);
 fixture_result result=query(workers[0],CHECK_KIND,release,docs,4,names[0],names[0]);CHECK(result.state==XLS_QUERY_READY && parity(&result,CHECK_KIND,release,docs,4,names[0],names[0]));CHECK(!xlsContextEqual(xlsModuleGraphContext(modules(&result,0)),context) && named(modules(&result,0),paths[2],NULL)==ids[2]);releaseResult(&result);
 const char* missing="import no_such_module_graph_dependency\nfunc main(...) as int\n int unique_main=7\n return unique_main\nend\n";xlsTextRelease(docs[0].text);docs[0].text=xlsTextCreate(missing,strlen(missing),1024*1024,NULL);++docs[0].revision;CHECK(docs[0].text);result=query(workers[0],CHECK_KIND,context,docs,4,names[0],names[0]);CHECK(result.state==XLS_QUERY_READY && parity(&result,CHECK_KIND,context,docs,4,names[0],names[0]) && !xlsSemanticCheckInfo(result.check).accepted && !modules(&result,0));releaseResult(&result);
 xls_module_graph* retained=xlsModuleGraphRetain((xls_module_graph*)modules(&held[0],0));CHECK(retained);
 for(unsigned k=0;k<3;++k){xlsWorkerDestroy(workers[k]);releaseResult(&held[k]);}xlsModuleStoreRelease(store);xlsContextRelease(context);xlsContextRelease(release);CHECK(xlsModuleGraphInfo(retained).entry==ids[0] && edge(retained,ids[1],ids[2]));xlsModuleGraphRelease(retained);
 for(size_t i=0;i<5;++i){xlsTextRelease(docs[i].text);xrtFree(paths[i]);}xrtFree(project);xrtFree(root);return 1;
}
int main(void)
{checks=0;if(!isolated() || !nativeWorkers())return 1;printf("analysis_module_graph_test passed: %u checks;original native import edges,retained project ownership/persistent module IDs,typed source bindings,three workers,fresh parity,budgets,nesting and independent concurrent lifetimes\n",checks);return 0;}
