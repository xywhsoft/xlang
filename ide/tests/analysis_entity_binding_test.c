#include "analysis/index_entities.h"
#include "analysis/index_modules.h"
#include "analysis/worker_modules.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#define main originalWorkerFixtureMain
#include "analysis_worker_cache_test.c"
#undef main
#pragma GCC diagnostic pop
typedef struct identity {uint32_t symbol,type;uint64_t value,globalType;unsigned source;} identity;
static identity declaration(const xls_semantic_check* check,const char* path,const char* source,const char* marker,const char* kind)
{
 identity out={0};const char* at=strstr(source,marker);if(!at)return out;size_t offset=(size_t)(at-source);if(strlen(source)>=3 && !memcmp(source,"\xef\xbb\xbf",3))offset-=3;
 const xls_semantic_index* index=xlsSemanticCheckTypedIndex(check);
 for(size_t i=0;i<xlsSemanticIndexCount(index);++i){xls_index_fact f;xls_entity_fact e;if(!xlsSemanticIndexFact(index,i,&f) || !xlsSemanticIndexEntityFact(index,i,&e) || strcmp(f.kind,kind) || !f.selectionRange.known || f.selectionRange.startOffset!=offset || !f.selectionSource)continue;xls_input_source row;if(!xlsSemanticCheckSource(check,f.selectionSource-1,&row) || strcmp(row.path,path))continue;out=(identity){f.declaredSymbol,f.declaredType,e.declaredSymbol,e.declaredType,f.source};break;}return out;
}
static int selectedCall(const xls_semantic_check* check,uint32_t native,uint64_t stable)
{
 size_t count=0;const xls_semantic_index* index=xlsSemanticCheckTypedIndex(check);
 for(size_t i=0;i<xlsSemanticIndexCount(index);++i){xls_index_fact f;xls_entity_fact e;CHECK(xlsSemanticIndexFact(index,i,&f) && xlsSemanticIndexEntityFact(index,i,&e));if(!strcmp(f.kind,"call") && f.targetSymbol==native){CHECK(e.targetSymbol==stable);++count;}}
 CHECK(count>0);return 1;
}
typedef struct entity_reader {const xls_entity_view* view;uint32_t native;uint64_t stable;int okay;} entity_reader;
static int32 readEntities(void* user)
{entity_reader* r=user;r->okay=1;for(size_t i=0;i<256;++i)if(xlsEntityViewSymbol(r->view,r->native)!=r->stable || xlsEntityViewNativeSymbol(r->view,r->stable)!=r->native || !xlsEntityViewInfo(r->view).domain)r->okay=0;return 0;}
static int run(void)
{
 char relative[128];snprintf(relative,sizeof(relative),"build/entity-binding-%llu",(unsigned long long)xrtClock());char* root=xrtPathAbs(relative);CHECK(root && xrtDirCreateAll(root));char *paths[]={xrtPathJoin(root,"main.xl"),xrtPathJoin(root,"dep.xl"),xrtPathJoin(root,"other.xl")},*project=xrtPathJoin(root,"sample.xlp");CHECK(paths[0] && paths[1] && paths[2] && project);
 const char* header="class Local<T>\n T field\n func accept(T input) as int\n return 0\n end\nend\nfunc consume(Local<int> value) as int\n return 0\nend\nfunc keep<T>(T value) as T\n return value\nend\nfunc same(int value) as int\n return value\nend\nfunc same(string value) as int\n return 1\nend\nfunc token() as int\n return 3\nend\n";
 const char* tail="func main(...) as int\n int token = 7\n int unique_main=same(dep.compute(7))\n return unique_main\nend\n";
 const char* dependency="export class Box<T>\n T field\nend\nexport func compute(int value) as int\n return value+1\nend\nexport func compute(string value) as int\n return 0\nend\n";
 const char* other="export func otherValue() as int\n return 1\nend\n";const char* members[]={paths[0],paths[1],paths[2]};xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=project,.configuration="debug",.entry=paths[0],.directory=root,.analysisProfile="entity-binding-fixture",.targetKind="exe"};spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,3};xls_context *context=xlsContextCreate(&spec,1024*1024,NULL),*twin=xlsContextCreate(&spec,1024*1024,NULL);CHECK(context && twin);
 xls_module_store* store=xlsModuleStoreCreate(NULL);xls_worker* workers[3];CHECK(store);for(size_t i=0;i<3;++i){workers[i]=xlsWorkerCreateWithModules(NULL,4,NULL,store);CHECK(workers[i]);}
 xls_analysis_document docs[3]={{"entry",paths[0],NULL,1,1},{"dep",paths[1],xlsTextCreate(dependency,strlen(dependency),1024*1024,NULL),2,1},{"other",paths[2],xlsTextCreate(other,strlen(other),1024*1024,NULL),3,1}};CHECK(docs[1].text && docs[2].text);
 identity firstInt={0},firstString={0},firstRoot={0},firstType={0},firstGeneric={0},firstParameter={0},firstMember={0},firstTyped={0};fixture_result held={0};xls_entity_view* retained=NULL;
 for(unsigned version=0;version<5;++version){
  xstrbuf source={0};CHECK(xrtStrBufAppend(&source,xrtStrView(version>=3 ? "import other\nimport dep\n" : "import dep\nimport other\n")));
  if(version)CHECK(xrtStrBufAppend(&source,xrtStrView("func inserted() as int\n return 0\nend\n")));
  CHECK(xrtStrBufAppend(&source,xrtStrView(header)));if(version>=2)CHECK(xrtStrBufAppend(&source,xrtStrView("// shifted declarations 中文😀\n")));CHECK(xrtStrBufAppend(&source,xrtStrView(tail)));
  if(version==4){char* marker=strstr(source.Data," int token = 7");CHECK(marker);size_t at=(size_t)(marker-source.Data);xstrbuf changed={0};CHECK(xrtStrBufAppend(&changed,(xstrview){source.Data,at}) && xrtStrBufAppend(&changed,xrtStrView(" int newLocal=5\n")) && xrtStrBufAppend(&changed,xrtStrView(source.Data+at)));xrtStrBufFree(&source);source=changed;}
  xlsTextRelease(docs[0].text);docs[0].text=xlsTextCreate(source.Data,source.Size,1024*1024,NULL);docs[0].revision=version+1;CHECK(docs[0].text);
  for(size_t k=0;k<3;++k){
   fixture_result r=query(workers[k],CHECK_KIND,k==1 ? twin : context,docs,3,"entry","entry");CHECK(r.state==XLS_QUERY_READY && parity(&r,CHECK_KIND,context,docs,3,"entry","entry"));const xls_semantic_index* index=xlsSemanticCheckTypedIndex(r.check);const xls_entity_view* view=xlsSemanticIndexEntities(index);CHECK(view && xlsEntityViewInfo(view).symbols>0 && xlsEntityViewInfo(view).domain && xlsEntityViewInfo(view).bytes<=16u*1024u*1024u);
   identity integer=declaration(r.check,paths[1],dependency,"compute(int","export_func"),string=declaration(r.check,paths[1],dependency,"compute(string","export_func"),function=declaration(r.check,paths[0],source.Data,"same(int","func"),type=declaration(r.check,paths[0],source.Data,"Local<T>","class"),generic=declaration(r.check,paths[0],source.Data,"T>(T value)","name"),parameter=declaration(r.check,paths[0],source.Data,"value) as T","param"),member=declaration(r.check,paths[0],source.Data,"accept(T input)","func"),typed=declaration(r.check,paths[0],source.Data,"consume(Local<int>","func");
   if(!integer.value || !string.value || !function.value || !type.globalType || !generic.globalType || !parameter.value)fprintf(stderr,"identity capture v%u worker%zu: int=%llu string=%llu func=%llu class=%llu generic=%llu param=%llu;symbols=%zu types=%zu ambiguous=%zu\n",version,k,(unsigned long long)integer.value,(unsigned long long)string.value,(unsigned long long)function.value,(unsigned long long)type.globalType,(unsigned long long)generic.globalType,(unsigned long long)parameter.value,xlsEntityViewInfo(view).symbols,xlsEntityViewInfo(view).types,xlsEntityViewInfo(view).ambiguous);
   if(!member.value || !typed.value)fprintf(stderr,"complex header mapping: member=%llu typed=%llu\n",(unsigned long long)member.value,(unsigned long long)typed.value);
   identity classGeneric=declaration(r.check,paths[0],source.Data,"T>\n T field","name");
   CHECK(classGeneric.globalType && classGeneric.globalType!=generic.globalType);
   CHECK(member.value && typed.value);
   CHECK(integer.value && string.value && integer.value!=string.value && function.value && type.globalType && generic.globalType && parameter.value);
   CHECK(selectedCall(r.check,integer.symbol,integer.value) && selectedCall(r.check,function.symbol,function.value));
   identity local=declaration(r.check,paths[0],source.Data,"token = 7","var_decl");CHECK(local.symbol && !local.value);
   if(!version && !k){firstInt=integer;firstString=string;firstRoot=function;firstType=type;firstGeneric=generic;firstParameter=parameter;firstMember=member;firstTyped=typed;held=r;retained=xlsEntityViewRetain((xls_entity_view*)view);}
   else{CHECK(integer.value==firstInt.value && string.value==firstString.value && function.value==firstRoot.value && type.globalType==firstType.globalType && generic.globalType==firstGeneric.globalType && parameter.value==firstParameter.value && member.value==firstMember.value && typed.value==firstTyped.value);if(version==1)CHECK(function.symbol!=firstRoot.symbol);if(version>=3)CHECK(integer.source!=firstInt.source);releaseResult(&r);}
  }
  xrtStrBufFree(&source);
 }
 const char* changedSignature="export class Box<T>\n T field\nend\nexport func compute(int value) as string\n return \"changed\"\nend\nexport func compute(string value) as int\n return 0\nend\n";
 xlsTextRelease(docs[1].text);docs[1].text=xlsTextCreate(changedSignature,strlen(changedSignature),1024*1024,NULL);++docs[1].revision;CHECK(docs[1].text);fixture_result changed=query(workers[0],CHECK_KIND,context,docs,3,"entry","entry");CHECK(changed.state==XLS_QUERY_READY && parity(&changed,CHECK_KIND,context,docs,3,"entry","entry"));identity signature=declaration(changed.check,paths[1],changedSignature,"compute(int","export_func"),selected=declaration(changed.check,paths[0],xlsTextData(docs[0].text),"same(string","func");CHECK(signature.value && signature.value!=firstInt.value && selected.value && selectedCall(changed.check,selected.symbol,selected.value));releaseResult(&changed);
 xlsTextRelease(docs[1].text);docs[1].text=xlsTextCreate(dependency,strlen(dependency),1024*1024,NULL);++docs[1].revision;CHECK(docs[1].text);changed=query(workers[0],CHECK_KIND,context,docs,3,"entry","entry");CHECK(changed.state==XLS_QUERY_READY && parity(&changed,CHECK_KIND,context,docs,3,"entry","entry"));signature=declaration(changed.check,paths[1],dependency,"compute(int","export_func");CHECK(signature.value==firstInt.value && selectedCall(changed.check,signature.symbol,signature.value));releaseResult(&changed);
 spec.configuration="release";xls_context* release=xlsContextCreate(&spec,1024*1024,NULL);CHECK(release);fixture_result r=query(workers[0],CHECK_KIND,release,docs,3,"entry","entry");CHECK(r.state==XLS_QUERY_READY && parity(&r,CHECK_KIND,release,docs,3,"entry","entry"));identity different=declaration(r.check,paths[1],dependency,"compute(int","export_func");CHECK(different.value && different.value!=firstInt.value);releaseResult(&r);xlsContextRelease(release);
 xls_entity_stats stats;CHECK(xlsModuleStoreEntityStats(store,&stats) && stats.domains==2 && stats.entities>10 && stats.hits>10 && stats.bytes<=stats.limit);
 for(size_t k=0;k<3;++k)xlsWorkerDestroy(workers[k]);
 releaseResult(&held);xlsModuleStoreRelease(store);xlsContextRelease(context);xlsContextRelease(twin);CHECK(retained && xlsEntityViewSymbol(retained,firstInt.symbol)==firstInt.value);
 entity_reader readers[4];xthread* threads[4];for(size_t i=0;i<4;++i){readers[i]=(entity_reader){retained,firstInt.symbol,firstInt.value,0};threads[i]=xrtThreadCreate(readEntities,&readers[i],0);CHECK(threads[i]);}for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}xlsEntityViewRelease(retained);
 for(size_t i=0;i<3;++i){xlsTextRelease(docs[i].text);xrtFree(paths[i]);}xrtFree(project);xrtFree(root);return 1;
}
int main(void){checks=0;if(!run())return 1;printf("analysis_entity_binding_test passed: %u checks;current compiler references,overloads,type/generic/parameter IDs,global allocation/source ordinal shifts,equal contexts,configuration isolation,unmapped shadowed locals,fresh parity and four readers after all original owners close\n",checks);return 0;}
