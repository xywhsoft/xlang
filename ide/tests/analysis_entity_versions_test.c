#include "analysis/module_store.h"
#include "analysis/check_session.h"
#include "analysis/check_ir.h"
#include "analysis/index_entities.h"
#define main entityVersionsBaseMain
#include "analysis_worker_cache_test.c"
#undef main

typedef struct version_value {uint32_t native;uint64_t identity,version;} version_value;
static version_value versionFunction(const xls_semantic_check* check,const char* name,int group)
{
 const XL6_AstNode* root=xlsSemanticCheckAst(check);const xls_entity_view* view=xlsSemanticIndexEntities(xlsSemanticCheckTypedIndex(check));
 if(root)for(size_t i=0;i<root->childCount;++i){const XL6_AstNode* n=root->children[i];if((n->kind!=XL6_AST_FUNC && n->kind!=XL6_AST_EXPORT_FUNC) || !n->childCount || !n->children[0]->text || strcmp(n->children[0]->text,name))continue;
  uint32_t id=group ? n->children[0]->semanticDeclaredSymbolId : n->semanticDeclaredSymbolId;return (version_value){id,xlsEntityViewSymbol(view,id),xlsEntityViewSymbolVersion(view,id)};}
 if(root)for(size_t i=0;i<root->childCount;++i){const XL6_AstNode* n=root->children[i];fprintf(stderr,"missing %s: kind=%s name=%s symbol=%u declared=%u\n",name,xl6AstKindName(n->kind),n->childCount && n->children[0]->text ? n->children[0]->text : "",n->semanticSymbolId,n->semanticDeclaredSymbolId);}
 return (version_value){0};
}
static version_value versionType(const xls_semantic_check* check,const char* path)
{
 const xls_semantic_index* index=xlsSemanticCheckTypedIndex(check);const xls_entity_view* view=xlsSemanticIndexEntities(index);
 for(size_t i=0;i<xlsSemanticIndexCount(index);++i){xls_index_fact f;if(!xlsSemanticIndexFact(index,i,&f) || strcmp(f.kind,"class") || !f.declaredType)continue;
  xls_check_source row;if(f.selectionSource && xlsSemanticCheckSource(check,f.selectionSource-1,&row) && !strcmp(row.path,path))return (version_value){f.declaredType,xlsEntityViewType(view,f.declaredType),xlsEntityViewTypeVersion(view,f.declaredType)};}
 return (version_value){0};
}
static int versionParity(xls_semantic_check* r,xls_context* context,const xls_analysis_document* docs)
{
 xls_semantic_input in[2];for(size_t i=0;i<2;++i)in[i]=(xls_semantic_input){docs[i].path,xlsTextData(docs[i].text),xlsTextSize(docs[i].text)};
 xls_semantic_check* fresh=xlsSemanticCheckCreateInContext(context,docs[0].path,in,2,1,NULL,NULL,NULL,0);CHECK(fresh && xlsSemanticCheckInfo(fresh).accepted && sameAst(xlsSemanticCheckAst(r),xlsSemanticCheckAst(fresh)));
 const char *ia=xlsSemanticCheckIndex(r),*ib=xlsSemanticCheckIndex(fresh);CHECK(ia && ib && !strcmp(ia,ib));
 XL6_Output x,y;CHECK(xl6OutputOpenMemory(&x) && xl6OutputOpenMemory(&y));xl6IrDumpModuleJson(&x,xlsSemanticCheckIr(r));xl6IrDumpModuleJson(&y,xlsSemanticCheckIr(fresh));char *a=xl6OutputTakeMemoryText(&x,NULL),*b=xl6OutputTakeMemoryText(&y,NULL);CHECK(a && b && !strcmp(a,b));xrtFree(a);xrtFree(b);xl6OutputDiscard(&x);xl6OutputDiscard(&y);xlsSemanticCheckDestroy(fresh);return 1;
}
typedef struct version_reader {const xls_entity_view* view;version_value function,type;int okay;} version_reader;
static int32 versionRead(void* user)
{version_reader* r=user;r->okay=1;for(size_t i=0;i<128;++i)if(xlsEntityViewSymbolVersion(r->view,r->function.native)!=r->function.version || xlsEntityViewTypeVersion(r->view,r->type.native)!=r->type.version)r->okay=0;return 0;}
static int versionsRun(void)
{
 char folder[128];snprintf(folder,sizeof(folder),"build/entity-versions-%llu",(unsigned long long)xrtClock());char* directory=xrtPathAbs(folder);CHECK(directory && xrtDirCreateAll(directory));char *entry=xrtPathJoin(directory,"main.xl"),*dep=xrtPathJoin(directory,"dep.xl"),*project=xrtPathJoin(directory,"sample.xlp");CHECK(entry && dep && project);
 xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=project,.configuration="debug",.entry=entry,.directory=directory,.analysisProfile="typed-interface-versions",.targetKind="exe"};xls_context* context=xlsContextCreate(&spec,1024*1024,NULL);xls_module_store* store=xlsModuleStoreCreate(NULL);xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(context && store && session);xls_module_scope bank;xlsModuleScopeBegin(&bank,store);
 xls_analysis_document docs[2]={{"entry",entry,NULL,1,0},{"dep",dep,NULL,2,0}};version_value first={0},firstGroup={0},firstType={0};xls_entity_view* held=NULL;xls_semantic_check* heldCheck=NULL;
 for(unsigned v=0;v<8;++v){
  xstrbuf source={0},dependency={0};CHECK(xrtStrBufAppend(&source,xrtStrView("\xef\xbb\xbfimport dep\r\n")));
  if(v==1)CHECK(xrtStrBufAppend(&source,xrtStrView("func inserted(int input) as int\r\n return input\r\nend\r\n")));
  for(unsigned i=0;i<4;++i)CHECK(xrtStrBufAppendFormat(&source,"func warm_%u(int value) as int\r\n int local=dep.compute(value)\r\n if local > 0 then\r\n return local\r\n end\r\n return value\r\nend\r\n",i));
  CHECK(xrtStrBufAppendFormat(&source,"func caller(int value) as int\r\n int local=dep.compute(value)%s\r\n if local > 0 then\r\n return local\r\n end\r\n return value\r\nend\r\nfunc main(...) as int\r\n int unique_main=caller(7)\r\n return unique_main\r\nend\r\n",v==7 ? "+1" : ""));
  CHECK(xrtStrBufAppendFormat(&dependency,"\xef\xbb\xbf// 中文😀\r\nexport class Data\r\n %s field\r\nend\r\nexport func compute(int value) as int\r\n return value+%u\r\nend\r\n",v==4 ? "string" : "int",v==2 ? 2u : v==6 ? 3u : 1u));
  if(v==3)CHECK(xrtStrBufAppend(&dependency,xrtStrView("export func compute(string value) as int\r\n return 0\r\nend\r\n")));
  for(size_t i=0;i<2;++i){xlsTextRelease(docs[i].text);xstrbuf* s=i ? &dependency : &source;docs[i].text=xlsTextCreate(s->Data,s->Size,1024*1024,NULL);docs[i].revision=v+1;CHECK(docs[i].text);}
  xls_entity_version_control off,inner;if(v==5){xlsEntityVersionsBegin(&off,0);xlsEntityVersionsBegin(&inner,8u*1024u*1024u);}
  xls_semantic_check* r=xlsCheckSessionCheckInContext(session,context,docs,2,"entry",1,NULL,NULL,NULL,0);
  if(v==5){xlsEntityVersionsEnd(&inner);xlsEntityVersionsEnd(&off);}
  CHECK(r && xlsSemanticCheckInfo(r).accepted && versionParity(r,context,docs));const xls_entity_view* view=xlsSemanticIndexEntities(xlsSemanticCheckTypedIndex(r));CHECK(view);
  version_value function=versionFunction(r,"__xl6_import_dep_compute",0),group=versionFunction(r,"__xl6_import_dep_compute",1),type=versionType(r,dep),caller=versionFunction(r,"caller",0);xls_entity_version_info info=xlsEntityViewVersions(view);
  fprintf(stdout,"v%u symbols=%zu types=%zu declined=%zu function=%llu/%llu group=%llu/%llu type=%llu/%llu\n",v,info.symbols,info.types,info.declined,(unsigned long long)function.identity,(unsigned long long)function.version,(unsigned long long)group.identity,(unsigned long long)group.version,(unsigned long long)type.identity,(unsigned long long)type.version);
  CHECK(function.identity && group.identity && type.identity && caller.identity && function.identity!=group.identity && !xlsEntityViewSymbolVersion(view,UINT32_MAX) && !xlsEntityViewTypeVersion(view,UINT32_MAX));
  if(v==5){CHECK(!info.symbols && !info.types && !info.bytes && info.declined && !function.version && !type.version);}
  else{
   CHECK(function.version && group.version && type.version && info.symbols && info.types && info.bytes && info.bytes<=16u*1024u*1024u);
   if(!v){first=function;firstGroup=group;firstType=type;held=xlsEntityViewRetain((xls_entity_view*)view);heldCheck=xlsSemanticCheckRetain(r);}
   else{CHECK(function.identity==first.identity && group.identity==firstGroup.identity && (v==4 || type.identity==firstType.identity));
    CHECK(function.version==first.version);CHECK((group.version==firstGroup.version)==(v!=3));CHECK((type.version==firstType.version)==(v!=4));}
  }
  size_t deps=xlsSemanticCheckIrDependencies(r,caller.identity),withVersions=0;xls_check_ir_work work=xlsSemanticCheckIrWork(r);fprintf(stdout,"caller deps=%zu checked=%llu captured=%llu reused=%llu declined=%llu\n",deps,(unsigned long long)work.checked,(unsigned long long)work.captured,(unsigned long long)work.reused,(unsigned long long)work.declined);CHECK(deps>0 && deps<=65536);xls_check_ir_dependency row;
  for(size_t i=0;i<deps;++i){CHECK(xlsSemanticCheckIrDependency(r,caller.identity,i,&row) && row.declaration);uint32_t native=xlsEntityViewNativeSymbol(view,row.declaration);CHECK(native);uint64_t current=xlsEntityViewSymbolVersion(view,native);if(!v)fprintf(stdout,"captured dependency=%llu version=%llu group=%d\n",(unsigned long long)row.declaration,(unsigned long long)row.version,row.declaration==group.identity);if(row.version){++withVersions;CHECK(!current || row.version==current);}}
  CHECK(!xlsSemanticCheckIrDependency(r,caller.identity,deps,&row) && !row.declaration && !row.version && !xlsSemanticCheckIrDependencies(r,UINT64_MAX) && !xlsSemanticCheckIrDependency(r,caller.identity,0,NULL));
  if(v<5 || v==7)CHECK(withVersions>0);
  else CHECK(!withVersions); /* Immutable units captured with admission off retain0 even when later views have versions. */
  xlsSemanticCheckDestroy(r);xrtStrBufFree(&source);xrtStrBufFree(&dependency);
 }
 xls_check_session* tiny=xlsCheckSessionCreate(NULL);CHECK(tiny);xls_entity_version_control low;xlsEntityVersionsBegin(&low,1);xls_semantic_check* limited=xlsCheckSessionCheckInContext(tiny,context,docs,2,"entry",1,NULL,NULL,NULL,0);xlsEntityVersionsEnd(&low);CHECK(limited && xlsSemanticCheckInfo(limited).accepted && versionParity(limited,context,docs));xls_entity_version_info empty=xlsEntityViewVersions(xlsSemanticIndexEntities(xlsSemanticCheckTypedIndex(limited)));CHECK(!empty.symbols && !empty.types && !empty.bytes && empty.declined);xlsSemanticCheckDestroy(limited);xlsCheckSessionDestroy(tiny);
 spec.configuration="release";xls_context* release=xlsContextCreate(&spec,1024*1024,NULL);xls_check_session* separate=xlsCheckSessionCreate(NULL);CHECK(release && separate);xls_semantic_check* other=xlsCheckSessionCheckInContext(separate,release,docs,2,"entry",1,NULL,NULL,NULL,0);CHECK(other && xlsSemanticCheckInfo(other).accepted && versionParity(other,release,docs));version_value different=versionFunction(other,"__xl6_import_dep_compute",0),differentType=versionType(other,dep);CHECK(different.identity && different.version && different.identity!=first.identity && different.version!=first.version && differentType.version && differentType.version!=firstType.version);xlsSemanticCheckDestroy(other);xlsCheckSessionDestroy(separate);xlsContextRelease(release);
 /* Real consumer: a default changes in an UNSELECTED overload. The selected
  * int call's complete checked AST and original IR proofs stay eligible.
  * A versioned overload-group dependency must reject its older interface;
  * zero-admission control executes the original complete-proof fallback. */
 xls_check_ir_work finalWork[2];
 for(unsigned off=0;off<2;++off){xls_check_session* probe=xlsCheckSessionCreate(NULL);CHECK(probe);uint64_t beforeGroup=0;
  for(unsigned edit=0;edit<2;++edit){xstrbuf text={0};CHECK(xrtStrBufAppendFormat(&text,"export func compute(int value) as int\r\n return value+1\r\nend\r\nexport func compute(string value,int pad=%u) as int\r\n return pad\r\nend\r\n",edit+1));xlsTextRelease(docs[1].text);docs[1].text=xlsTextCreate(text.Data,text.Size,1024*1024,NULL);++docs[1].revision;CHECK(docs[1].text);xrtStrBufFree(&text);
   xls_entity_version_control control;if(off)xlsEntityVersionsBegin(&control,0);xls_semantic_check* r=xlsCheckSessionCheckInContext(probe,context,docs,2,"entry",1,NULL,NULL,NULL,0);if(off)xlsEntityVersionsEnd(&control);CHECK(r && xlsSemanticCheckInfo(r).accepted && versionParity(r,context,docs));
   version_value g=versionFunction(r,"__xl6_import_dep_compute",1),caller=versionFunction(r,"caller",0);CHECK(g.identity && caller.identity);
   if(!off){CHECK(g.version);if(!edit)beforeGroup=g.version;else CHECK(g.version!=beforeGroup);}else CHECK(!g.version);
   size_t count=xlsSemanticCheckIrDependencies(r,caller.identity);int found=0;for(size_t i=0;i<count;++i){xls_check_ir_dependency d;CHECK(xlsSemanticCheckIrDependency(r,caller.identity,i,&d));if(d.declaration==g.identity){CHECK(d.version==g.version);found=1;}}CHECK(found);
   if(edit)finalWork[off]=xlsSemanticCheckIrWork(r);
   xlsSemanticCheckDestroy(r);
  }xlsCheckSessionDestroy(probe);
 }
 fprintf(stdout,"actual changed overload:versioned checked=%llu reused=%llu;fallback checked=%llu reused=%llu\n",(unsigned long long)finalWork[0].checked,(unsigned long long)finalWork[0].reused,(unsigned long long)finalWork[1].checked,(unsigned long long)finalWork[1].reused);
 CHECK(finalWork[0].checked>finalWork[1].checked && finalWork[0].reused<finalWork[1].reused && finalWork[1].reused>=5);
 xlsCheckSessionDestroy(session);xlsModuleScopeEnd(&bank);xlsModuleStoreRelease(store);xlsContextRelease(context);xlsSemanticCheckDestroy(heldCheck);CHECK(held && xlsEntityViewSymbolVersion(held,first.native)==first.version && xlsEntityViewTypeVersion(held,firstType.native)==firstType.version);
 version_reader readers[4];xthread* threads[4];for(size_t i=0;i<4;++i){readers[i]=(version_reader){held,first,firstType,0};threads[i]=xrtThreadCreate(versionRead,&readers[i],0);CHECK(threads[i]);}for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}xlsEntityViewRelease(held);
 for(size_t i=0;i<2;++i)xlsTextRelease(docs[i].text);
 xrtFree(project);xrtFree(dep);xrtFree(entry);xrtFree(directory);return 1;
}
int main(void){checks=0;if(!versionsRun())return 1;printf("analysis_entity_versions_test passed: %u checks;typed interface identities/versions,overload and member changes,body/coordinate/globalmoves,actual IR declaration dependencies,disabled fallback,complete fresh parity and four readers after owners close\n",checks);return 0;}
