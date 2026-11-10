#include "services/project_references.h"
#include "protocol/lsp_wire.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static size_t fenceAt;
static int fenceEnabled,fenceFired,fenceOkay;
static const char *fencePath,*fenceBody;
static xevent *fenceReached,*fenceResume;
static void workspaceFence(size_t completed)
{
 if(!fenceEnabled || fenceFired || completed!=fenceAt)return;
 fenceFired=1;
 if(fencePath)fenceOkay=xrtFileWriteAtomic(fencePath,(xbytesview){(const unsigned char*)fenceBody,strlen(fenceBody)});
 if(fenceReached){xrtEventSet(fenceReached);xrtEventWait(fenceResume);}
}
#define XIDE_REFERENCE_WORKSPACE_FENCE workspaceFence
#include "services/project_references.c"
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"project_rename_workspace_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static xstrview testView(const char* s){return (xstrview){s,strlen(s)};}
static const xvalue* testGet(const xvalue* v,const char* key){return xrtValueObjectGet(v,testView(key));}
static int testPut(xvalue* v,const char* key,xvalue* x){return x && xrtValueObjectSetNew(v,testView(key),x);}
static int writeText(const char* path,const char* text){return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)});}
static xls_context* context(const char* project,const char* directory,const char* entry,const char* const* members,size_t count)
{
 xls_context_spec spec={.kind=project ? XLS_CONTEXT_PROJECT : XLS_CONTEXT_XLANG,.project=project,.configuration=project ? "debug" : "",.entry=entry,.directory=directory,.analysisProfile=XIDE_ANALYSIS_PROFILE,.targetKind="exe"};
 spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,count};return xlsContextCreate(&spec,16u*1024u*1024u,NULL);
}
static xide_rename_scope* membership(xls_context* const* contexts,xls_context* standalone)
{
 xvalue *root=xrtValueObject(),*array=xrtValueArray();int okay=root && array;
 for(size_t i=0;okay && i<3;++i){xvalue* row=xideAnalysisContextWrite(contexts[i]);okay=row && xrtValueArrayAppendNew(array,row);}
 okay=okay && testPut(root,"schema",xrtValueInt(1)) && testPut(root,"selectedProject",xrtValueString(testView(xlsContextSpec(contexts[0])->project))) && testPut(root,"packagePath",xrtValueNull());
 if(okay){okay=testPut(root,"projects",array);array=NULL;}xrtValueRelease(array);
 xide_analysis_catalog* catalog=okay ? xideAnalysisCatalogCreate(root,NULL,0) : NULL;xrtValueRelease(root);
 xide_rename_scope* scope=catalog ? xideRenameScopeCreate(catalog,&standalone,1,0,NULL,NULL,NULL) : NULL;xideAnalysisCatalogDestroy(catalog);return scope;
}
static xide_rename_target* target(const char* entry,const char* raw,const char* shared,const char* sharedText)
{
 xls_semantic_input inputs[]={{entry,raw,strlen(raw)},{shared,sharedText,strlen(sharedText)}};
 xls_semantic_check* check=xlsSemanticCheckCreate(entry,inputs,2,1,NULL,NULL,NULL,0);if(!check || !xlsSemanticCheckInfo(check).accepted){xlsSemanticCheckDestroy(check);return NULL;}
 xls_check_inputs info;xide_semantic_source sources[8];xide_rename_target* t=NULL;
 if(xlsSemanticCheckInputs(check,&info) && info.sources<=8){
  size_t i=0;for(;i<info.sources;++i){xls_check_source s;if(!xlsSemanticCheckSource(check,i,&s))break;sources[i]=(xide_semantic_source){s.path,s.text,s.kind,!strcmp(s.path,entry)};}
  const char* p=strstr(raw,"compute(1)");int line,column;
  if(i==info.sources && p && xideLspOffsetToPosition(raw,(int)(p-raw),&line,&column))t=xideRenameTargetCreate(NULL,check,sources,info.sources,entry,line,column,0,NULL,NULL,NULL);
 }
 xlsSemanticCheckDestroy(check);return t;
}
static int result(xide_reference_query* q,int ready,size_t count,const char* reason)
{
 CHECK(q);const xvalue* rows=NULL;char error[512]={0};enum xide_reference_query_state state;xide_project_reference_stats stats;
 for(unsigned i=0;;++i){state=xideProjectReferencesResult(q,&rows,&stats,error,sizeof(error));if(state!=XIDE_REFERENCES_PENDING)break;if(i==5000){CHECK(0);}xrtSleep(1);}
 if(ready){
  if(state!=XIDE_REFERENCES_READY)fprintf(stderr,"rename scope failure: %s\n",error);
  CHECK(state==XIDE_REFERENCES_READY && rows && xrtValueCount(rows)==count && stats.locations==count && stats.thread!=xrtThreadCurrentId());
  CHECK(stats.workspaceScopes==4 && stats.workspaceCompleted==4 && stats.workspaceDuplicates>=2 && stats.workspaceGuardBytes && stats.workspaceGuardBytes<=128u*1024u*1024u);
  reference_output* native=xrtFutureValue(q->future);CHECK(native && native->rows.Count==count && native->guards.Count>=stats.workspaceScopes);
  for(size_t i=0;i<count;++i){const xvalue* row=xrtValueArrayGet(rows,i);int64 schema=0;const xvalue* proof=testGet(row,"xlang");CHECK(xrtValueGetInt(testGet(proof,"schema"),&schema) && schema==1 && xrtValueType(testGet(proof,"document"))==XVALUE_OBJECT && testGet(row,"uri") && testGet(row,"range"));}
  xvalue* taken=xideProjectReferencesTake(q);CHECK(taken && !xideProjectReferencesTake(q));xideProjectReferencesQueryDestroy(q);CHECK(xrtValueCount(taken)==count);xrtValueRelease(taken);
 }else{
  CHECK(state==XIDE_REFERENCES_FAILED && !rows && strstr(error,reason) && !xideProjectReferencesTake(q));xideProjectReferencesQueryDestroy(q);
 }
 return 1;
}
static int run(void)
{
 char* directory=xrtPathAbs("build/project-rename-workspace");CHECK(directory && xrtDirCreateAll(directory));
 const char* names[]={"main.xl","second.xl","unrelated.xl","standalone.xl","shared.xl","unopened.xl","helper.xl","A.xlp","B.xlp","C.xlp","absent-xl.exe","isolated.xl","STANDALONE.XL"};char* paths[13];
 for(size_t i=0;i<13;++i){paths[i]=xrtPathJoin(directory,names[i]);CHECK(paths[i]);}
 const char *raw="\xef\xbb\xbf" "import shared\r\nfunc main(...) as int\r\n return shared.compute(1)\r\nend\r\n",*dep="export func compute(int value) as int\n return value\nend\n";
 const char *consumer="import shared\nfunc main(...) as int\n return shared.compute(2)\nend\n",*unrelated="func compute(int value) as int\n return value\nend\nfunc main(...) as int\n return compute(3)\nend\n";
 CHECK(writeText(paths[0],raw) && writeText(paths[1],consumer) && writeText(paths[2],unrelated) && writeText(paths[3],consumer) && writeText(paths[4],dep) && writeText(paths[5],consumer) && writeText(paths[11],unrelated));
 const char *aMembers[]={paths[0],paths[4]},*bMembers[]={paths[1],paths[4],paths[5],paths[3]},*cMembers[]={paths[11]};
 xls_context* contexts[]={context(paths[7],directory,paths[0],aMembers,2),context(paths[8],directory,paths[1],bMembers,4),context(paths[9],directory,paths[2],cMembers,1)};
 xls_context* standalone=context(NULL,directory,paths[12],NULL,0);CHECK(contexts[0] && contexts[1] && contexts[2] && standalone);
 xide_rename_scope* scope=membership(contexts,standalone);xide_rename_target* t=target(paths[0],raw,paths[4],dep);CHECK(scope && t);
 xls_text* text=xlsTextCreate(raw,strlen(raw),strlen(raw),NULL);CHECK(text);xls_analysis_document docs[]={{"entry",paths[0],text,1,1}};
 xide_toolchains* cache=xideToolchainsCreate();xide_project_references* store=xideProjectReferencesCreate(cache,paths[10],NULL);CHECK(cache && store);
 for(size_t i=0;i<2;++i)CHECK(result(xideProjectReferencesSubmitRenameWorkspace(store,scope,t,docs,1,"entry"),1,5,NULL));
 xls_context* reordered[]={contexts[2],contexts[0],contexts[1]};xide_rename_scope* emptyFirst=membership(reordered,standalone);
 CHECK(emptyFirst && result(xideProjectReferencesSubmitRenameWorkspace(store,emptyFirst,t,docs,1,"entry"),1,5,NULL));xideRenameScopeRelease(emptyFirst);
 /* The unrelated scope has zero references; all its unimported members still
    need successful checking before the whole workspace can be admitted. */
 CHECK(writeText(paths[11],"func broken(\n"));CHECK(result(xideProjectReferencesSubmitRenameWorkspace(store,scope,t,docs,1,"entry"),0,0,"configured member"));
 CHECK(writeText(paths[11],unrelated) && result(xideProjectReferencesSubmitRenameWorkspace(store,scope,t,docs,1,"entry"),1,5,NULL));
 /* Mutate an earlier scope with no references after it has completed. A
    validator that retained guards only for returned locations misses this. */
 fenceEnabled=1;fenceAt=3;fenceFired=0;fencePath=paths[11];fenceBody="func main(...) as int\n return 4\nend\n";
 CHECK(result(xideProjectReferencesSubmitRenameWorkspace(store,scope,t,docs,1,"entry"),0,0,"Workspace source/resolution"));CHECK(fenceFired && fenceOkay);fenceEnabled=0;CHECK(writeText(paths[11],unrelated));
 CHECK(result(xideProjectReferencesSubmitRenameWorkspace(store,scope,t,docs,1,"entry"),1,5,NULL));
 /* Same bytes with a replaced disk identity must not merge as one proof. */
 fenceEnabled=1;fenceAt=1;fenceFired=0;fencePath=paths[4];fenceBody=dep;
 CHECK(result(xideProjectReferencesSubmitRenameWorkspace(store,scope,t,docs,1,"entry"),0,0,"scopes disagree"));CHECK(fenceFired && fenceOkay);fenceEnabled=0;
 CHECK(result(xideProjectReferencesSubmitRenameWorkspace(store,scope,t,docs,1,"entry"),1,5,NULL));
 /* A final pre-publication mutation is also rejected without partial output. */
 fenceEnabled=1;fenceAt=0;fenceFired=0;fencePath=paths[4];fenceBody="export func compute(int value) as int\n return value+1\nend\n";
 CHECK(result(xideProjectReferencesSubmitRenameWorkspace(store,scope,t,docs,1,"entry"),0,0,"Workspace source/resolution"));CHECK(fenceFired && fenceOkay);fenceEnabled=0;CHECK(writeText(paths[4],dep));
 /* Cancellation at an actual inter-scope phase leaves no published prefix. */
 fenceReached=xrtEventCreate(false,false);fenceResume=xrtEventCreate(false,false);CHECK(fenceReached && fenceResume);
 fenceEnabled=1;fenceAt=1;fenceFired=0;fencePath=NULL;
 xide_reference_query* q=xideProjectReferencesSubmitRenameWorkspace(store,scope,t,docs,1,"entry");CHECK(q && xrtEventWaitFor(fenceReached,XIDE_XRT_TIMEOUT_MS(5000))==XWAIT_OK);
 xideProjectReferencesCancel(q);xrtEventSet(fenceResume);enum xide_reference_query_state state;
 for(unsigned i=0;;++i){state=xideProjectReferencesResult(q,NULL,NULL,NULL,0);if(state!=XIDE_REFERENCES_PENDING)break;CHECK(i<5000);xrtSleep(1);}CHECK(state==XIDE_REFERENCES_CANCELLED && !xideProjectReferencesTake(q));xideProjectReferencesQueryDestroy(q);
 fenceEnabled=0;xrtEventDestroy(fenceReached);xrtEventDestroy(fenceResume);fenceReached=fenceResume=NULL;
 CHECK(result(xideProjectReferencesSubmitRenameWorkspace(store,scope,t,docs,1,"entry"),1,5,NULL));
 q=xideProjectReferencesSubmitRenameWorkspace(store,scope,t,docs,1,"entry");CHECK(q);
 xideRenameTargetRelease(t);xideRenameScopeRelease(scope);for(size_t i=0;i<3;++i)xlsContextRelease(contexts[i]);xlsContextRelease(standalone);xlsTextRelease(text);
 CHECK(result(q,1,5,NULL));xideProjectReferencesDestroy(store);xideToolchainsDestroy(cache);for(size_t i=0;i<13;++i)xrtFree(paths[i]);xrtFree(directory);return 1;
}
int main(void){if(!run())return 1;printf("project_rename_workspace_test passed: %u checks; complete four-scope native collection, lexical alias/shared dedup, unrelated member/late guard failures, complete proof identity, publication mutation, deterministic cancellation and queued lifetime\n",checks);return 0;}
