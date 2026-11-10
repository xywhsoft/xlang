#define main referenceScopeOriginalMain
#include "project_rename_scope_test.c"
#undef main
#include "platform/path.h"
#include "rename_identifier_fixture.h"
#undef CHECK
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"project_rename_identifier_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static xide_rename_target* identifierTarget(const char* a,const char* b,const char* dep,const char* needle)
{
 xls_semantic_input inputs[]={{a,identifierEntry,strlen(identifierEntry)},{b,identifierOther,strlen(identifierOther)},{dep,identifierLibrary,strlen(identifierLibrary)}};
 xls_semantic_check* check=xlsSemanticCheckCreate(a,inputs,3,1,NULL,NULL,NULL,0);if(!check || !xlsSemanticCheckInfo(check).accepted){xlsSemanticCheckDestroy(check);return NULL;}
 xls_check_inputs info;xide_semantic_source sources[8];xide_rename_target* out=NULL;
 if(xlsSemanticCheckInputs(check,&info) && info.sources<=8){size_t i=0;for(;i<info.sources;++i){xls_check_source s;if(!xlsSemanticCheckSource(check,i,&s))break;sources[i]=(xide_semantic_source){s.path,s.text,s.kind,!strcmp(s.path,a) || !strcmp(s.path,dep)};}
  const char* p=strstr(identifierEntry,needle);int line,col;if(i==info.sources && p && xideLspOffsetToPosition(identifierEntry,(int)(p-identifierEntry),&line,&col))out=xideRenameTargetCreate(NULL,check,sources,info.sources,a,line,col,0,NULL,NULL,NULL);
 }
 xlsSemanticCheckDestroy(check);return out;
}
static int identifierResult(xide_reference_query* q,const char* a,const char* name,size_t count,int qualified)
{
 CHECK(q);const xvalue* rows=NULL;char error[512]={0};enum xide_reference_query_state state;
 for(unsigned i=0;;++i){state=xideProjectReferencesResult(q,&rows,NULL,error,sizeof(error));if(state!=XIDE_REFERENCES_PENDING)break;CHECK(i<5000);xrtSleep(1);}
 if(state!=XIDE_REFERENCES_READY)fprintf(stderr,"identifier workspace error: %s\n",error);
 CHECK(state==XIDE_REFERENCES_READY && rows && xrtValueCount(rows)==count);int found=0;
 for(size_t i=0;i<count;++i){const xvalue* row=xrtValueArrayGet(rows,i),*range=get(row,"range"),*start=get(range,"start"),*end=get(range,"end");int64 line=0,col=0,lastLine=0,lastCol=0;
  CHECK(xrtValueGetInt(get(start,"line"),&line) && xrtValueGetInt(get(start,"character"),&col) && xrtValueGetInt(get(end,"line"),&lastLine) && xrtValueGetInt(get(end,"character"),&lastCol));
  CHECK(line==lastLine && lastCol-col==(strstr(name,"😀") ? 4 : 2));
  if(qualified && line==1){xstrview uri;CHECK(xrtValueGetString(get(row,"uri"),&uri));char* path=xideLspUriToPath(uri.Data,NULL);CHECK(path);
   if(xidePathLexicalEqual(path,a)){const char* point=strstr(identifierEntry,"型号?");int expectedLine,expectedColumn;CHECK(point && xideLspOffsetToPosition(identifierEntry,(int)(point-identifierEntry),&expectedLine,&expectedColumn) && line==expectedLine && col==expectedColumn);found=1;}free(path);
  }
 }
 CHECK(!qualified || found);xvalue* taken=xideProjectReferencesTake(q);CHECK(taken);xideProjectReferencesQueryDestroy(q);CHECK(xrtValueCount(taken)==count);xrtValueRelease(taken);return 1;
}
static int identifierProjectRun(void)
{
 char* directory=xrtPathAbs("build/project-rename-identifiers");CHECK(directory && xrtDirCreateAll(directory));
 const char* names[]={"入口😀.xl","other.xl","unrelated.xl","dep.xl","a.xlp","b.xlp","c.xlp","absent-xl.exe"};char* paths[8];for(size_t i=0;i<8;++i){paths[i]=xrtPathJoin(directory,names[i]);CHECK(paths[i]);}
 CHECK(writeText(paths[0],identifierEntry) && writeText(paths[1],identifierOther) && writeText(paths[3],identifierLibrary) && writeText(paths[2],"func main(...) as int\n return 0\nend\n"));
 const char* members[]={paths[0],paths[1],paths[3]};xls_context* contexts[]={context(paths[4],directory,paths[0],members,3),context(paths[5],directory,paths[1],members,3),context(paths[6],directory,paths[2],NULL,0)};
 xls_context* file=context(NULL,directory,paths[1],NULL,0);CHECK(contexts[0] && contexts[1] && contexts[2] && file);xide_rename_scope* scope=membership(contexts,file);CHECK(scope);
 xide_toolchains* cache=xideToolchainsCreate();xide_project_references* store=xideProjectReferencesCreate(cache,paths[7],NULL);CHECK(cache && store);
 xls_text* text=xlsTextCreate(identifierEntry,strlen(identifierEntry),strlen(identifierEntry),NULL);CHECK(text);xls_analysis_document doc={"entry",paths[0],text,1,1};
 xide_rename_target* type=identifierTarget(paths[0],paths[1],paths[3],"型号?");xide_rename_target* field=identifierTarget(paths[0],paths[1],paths[3],"😀字段=3");CHECK(type && field);
 for(size_t i=0;i<2;++i){CHECK(identifierResult(xideProjectReferencesSubmitRenameWorkspace(store,scope,type,&doc,1,"entry"),paths[0],"型号",4,1));CHECK(identifierResult(xideProjectReferencesSubmitRenameWorkspace(store,scope,field,&doc,1,"entry"),paths[0],"😀字段",4,0));}
 xideRenameTargetRelease(type);xideRenameTargetRelease(field);xideRenameScopeRelease(scope);xideProjectReferencesDestroy(store);xideToolchainsDestroy(cache);xlsTextRelease(text);for(size_t i=0;i<3;++i)xlsContextRelease(contexts[i]);xlsContextRelease(file);for(size_t i=0;i<8;++i)xrtFree(paths[i]);xrtFree(directory);return 1;
}
int main(void){if(!identifierProjectRun())return 1;printf("project_rename_identifier_test passed: %u checks; exact bound Unicode class and emoji field positions through all workspace actors, qualified-name prefix preserved, same-spelling fields excluded, warm reuse and detached output\n",checks);return 0;}
