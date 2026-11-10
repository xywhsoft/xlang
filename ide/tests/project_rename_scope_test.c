#include "services/project_references.h"
#include "protocol/lsp_wire.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"project_rename_scope_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static xstrview sv(const char* s){return (xstrview){s,strlen(s)};}
static const xvalue* get(const xvalue* v,const char* key){return xrtValueObjectGet(v,sv(key));}
static int put(xvalue* v,const char* key,xvalue* x){return x && xrtValueObjectSetNew(v,sv(key),x);}
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
 okay=okay && put(root,"schema",xrtValueInt(1)) && put(root,"selectedProject",xrtValueString(sv(xlsContextSpec(contexts[0])->project))) && put(root,"packagePath",xrtValueNull());
 if(okay){okay=put(root,"projects",array);array=NULL;}xrtValueRelease(array);
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
  for(size_t i=0;i<count;++i){const xvalue* row=xrtValueArrayGet(rows,i);int64 schema=0;const xvalue* proof=get(row,"xlang");CHECK(xrtValueGetInt(get(proof,"schema"),&schema) && schema==1 && xrtValueType(get(proof,"document"))==XVALUE_OBJECT && get(row,"uri") && get(row,"range"));}
  xvalue* taken=xideProjectReferencesTake(q);CHECK(taken && !xideProjectReferencesTake(q));xideProjectReferencesQueryDestroy(q);CHECK(xrtValueCount(taken)==count);xrtValueRelease(taken);
 }else{
  CHECK(state==XIDE_REFERENCES_FAILED && !rows && strstr(error,reason) && !xideProjectReferencesTake(q));xideProjectReferencesQueryDestroy(q);
 }
 return 1;
}
static int run(void)
{
 char* directory=xrtPathAbs("build/project-rename-scope");CHECK(directory && xrtDirCreateAll(directory));
 const char* names[]={"main.xl","second.xl","unrelated.xl","standalone.xl","shared.xl","unopened.xl","helper.xl","A.xlp","B.xlp","C.xlp","absent-xl.exe"};char* paths[11];
 for(size_t i=0;i<11;++i){paths[i]=xrtPathJoin(directory,names[i]);CHECK(paths[i]);}
 const char *raw="\xef\xbb\xbf" "import shared\r\nfunc main(...) as int\r\n return shared.compute(1)\r\nend\r\n",*dep="export func compute(int value) as int\n return value\nend\n";
 const char *consumer="import shared\nfunc main(...) as int\n return shared.compute(2)\nend\n",*unrelated="func compute(int value) as int\n return value\nend\nfunc main(...) as int\n return compute(3)\nend\n";
 CHECK(writeText(paths[0],raw) && writeText(paths[1],consumer) && writeText(paths[2],unrelated) && writeText(paths[3],consumer) && writeText(paths[4],dep) && writeText(paths[5],consumer));
 const char *aMembers[]={paths[0],paths[4],paths[4]},*bMembers[]={paths[1],paths[4],paths[5]};
 xls_context* contexts[]={context(paths[7],directory,paths[0],aMembers,3),context(paths[8],directory,paths[1],bMembers,3),context(paths[9],directory,paths[2],NULL,0)};
 xls_context* standalone=context(NULL,directory,paths[3],NULL,0);CHECK(contexts[0] && contexts[1] && contexts[2] && standalone);
 xide_rename_scope* scope=membership(contexts,standalone);xide_rename_target* t=target(paths[0],raw,paths[4],dep);CHECK(scope && t);
 xls_text* text=xlsTextCreate(raw,strlen(raw),strlen(raw),NULL);CHECK(text);xls_analysis_document docs[]={{"entry",paths[0],text,1,1}};
 xide_toolchains* cache=xideToolchainsCreate();xide_project_references* store=xideProjectReferencesCreate(cache,paths[10],NULL);CHECK(cache && store);
 const size_t wanted[]={2,3,0,2};
 for(size_t pass=0;pass<2;++pass)for(size_t i=0;i<4;++i)CHECK(result(xideProjectReferencesSubmitRenameScope(store,(xls_context*)xideRenameScopeAt(scope,i),scope,t,docs,1,"entry"),1,wanted[i],NULL));
 /* A different configuration is not a member just because the path matches. */
 xls_context_spec changed=*xlsContextSpec(contexts[0]);changed.configuration="release";xls_context* other=xlsContextCreate(&changed,16u*1024u*1024u,NULL);CHECK(other && !xideProjectReferencesSubmitRenameScope(store,other,scope,t,docs,1,"entry"));xlsContextRelease(other);
 /* Implicitly imported callers cannot silently disappear from an edit. */
 const char* helper="import shared\nexport func helper() as int\n return shared.compute(9)\nend\n";
 const char* importing="import shared\nimport helper\nfunc main(...) as int\n return shared.compute(1)+helper.helper()\nend\n";
 CHECK(writeText(paths[6],helper));xls_text* updated=xlsTextCreate(importing,strlen(importing),strlen(importing),NULL);CHECK(updated);docs[0].text=updated;docs[0].revision=2;
 CHECK(result(xideProjectReferencesSubmitRenameScope(store,contexts[0],scope,t,docs,1,"entry"),0,0,"outside explicitly configured"));
 docs[0].text=text;docs[0].revision=3;CHECK(result(xideProjectReferencesSubmitRenameScope(store,contexts[0],scope,t,docs,1,"entry"),1,2,NULL));
 /* Every owner must match the exact complete declaration source, not just the
    same declaration name, line, source interval or compilation-local ID. */
 CHECK(writeText(paths[4],"export func compute(int value) as int\n return value+1\nend\n"));
 CHECK(result(xideProjectReferencesSubmitRenameScope(store,contexts[1],scope,t,docs,1,"entry"),0,0,"Declaration source changed"));
 CHECK(writeText(paths[4],dep) && result(xideProjectReferencesSubmitRenameScope(store,contexts[1],scope,t,docs,1,"entry"),1,3,NULL));
 /* A library-only declaration is not made editable by successful checking. */
 xls_context *unlisted[]={context(paths[7],directory,paths[0],NULL,0),context(paths[8],directory,paths[1],NULL,0),contexts[2]};
 xide_rename_scope* noLibrary=membership(unlisted,standalone);CHECK(noLibrary && result(xideProjectReferencesSubmitRenameScope(store,unlisted[0],noLibrary,t,docs,1,"entry"),0,0,"declaration is outside"));
 xideRenameScopeRelease(noLibrary);xlsContextRelease(unlisted[0]);xlsContextRelease(unlisted[1]);
 /* An uncovered member error still fails the whole configured scope. */
 CHECK(writeText(paths[5],"func broken(\n"));CHECK(result(xideProjectReferencesSubmitRenameScope(store,contexts[1],scope,t,docs,1,"entry"),0,0,"configured member"));CHECK(writeText(paths[5],consumer));
 xide_reference_query* q=xideProjectReferencesSubmitRenameScope(store,contexts[1],scope,t,docs,1,"entry");CHECK(q);xideProjectReferencesCancel(q);enum xide_reference_query_state state;
 for(unsigned i=0;;++i){state=xideProjectReferencesResult(q,NULL,NULL,NULL,0);if(state!=XIDE_REFERENCES_PENDING)break;CHECK(i<5000);xrtSleep(1);}CHECK(state==XIDE_REFERENCES_CANCELLED || state==XIDE_REFERENCES_READY);xideProjectReferencesQueryDestroy(q);
 /* Queued work owns target, scope, context and input text independently. */
 q=xideProjectReferencesSubmitRenameScope(store,contexts[1],scope,t,docs,1,"entry");CHECK(q);
 xideRenameTargetRelease(t);xideRenameScopeRelease(scope);for(size_t i=0;i<3;++i)xlsContextRelease(contexts[i]);xlsContextRelease(standalone);xlsTextRelease(text);xlsTextRelease(updated);
 CHECK(result(q,1,3,NULL));xideProjectReferencesDestroy(store);xideToolchainsDestroy(cache);for(size_t i=0;i<11;++i)xrtFree(paths[i]);xrtFree(directory);return 1;
}
int main(void){if(!run())return 1;printf("project_rename_scope_test passed: %u checks; same native declaration across all project/file scopes, unrelated scope, unopened members, exact source/proofs, unconfigured import rejection, failure recovery and retained queued lifetime\n",checks);return 0;}
