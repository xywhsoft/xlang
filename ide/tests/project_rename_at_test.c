#define XIDE_RENAME_PLAN_TEST_NO_MAIN
#include "project_rename_plan_test.c"
#undef CHECK
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"project_rename_at_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static int cursorResult(xide_reference_query* q,int ready,const char* reason,xvalue** keep)
{
 CHECK(q);const xvalue* rows=NULL;char error[512]={0};xide_project_reference_stats stats;
 enum xide_reference_query_state state;
 for(unsigned i=0;;++i){state=xideProjectReferencesResult(q,&rows,&stats,error,sizeof(error));if(state!=XIDE_REFERENCES_PENDING)break;if(i==10000){CHECK(0);}xrtSleep(1);}
 if(!ready){
  if(state!=XIDE_REFERENCES_FAILED || !strstr(error,reason))fprintf(stderr,"cursor state=%d error=%s\n",state,error);
  CHECK(state==XIDE_REFERENCES_FAILED && !rows && strstr(error,reason) && !xideProjectReferencesRetainRenamePlan(q));
 }else{
  if(state!=XIDE_REFERENCES_READY)fprintf(stderr,"cursor state=%d error=%s\n",state,error);
  CHECK(state==XIDE_REFERENCES_READY && rows && xrtValueCount(rows)==5);
  CHECK(stats.workspaceScopes==4 && stats.workspaceCompleted==4 && stats.thread!=xrtThreadCurrentId());
  CHECK(stats.renameRechecks>=4 && stats.renameUnchanged>=1 && stats.renameReferences>=5);
  xide_rename_plan* plan=xideProjectReferencesRetainRenamePlan(q);xide_rename_plan_info info;
  CHECK(plan && xideRenamePlanInfo(plan,&info) && info.documents==5 && info.edits==5);
  size_t versions=0,boms=0;
  for(size_t i=0;i<info.documents;++i){xide_rename_plan_document d;CHECK(xideRenamePlanDocumentAt(plan,i,&d) && d.count==1 && strstr(d.before.Data,"compute") && strstr(d.after.Data,"renamed_😀"));
   if(d.hasVersion){++versions;CHECK(d.version==INT_MAX && d.coordinatePrefix==1);}
   if(d.before.Size>=3 && !memcmp(d.before.Data,"\xef\xbb\xbf",3)){++boms;CHECK(d.after.Size>=3 && !memcmp(d.after.Data,"\xef\xbb\xbf",3));}
  }
  CHECK(versions==1 && boms==2);xideRenamePlanRelease(plan);
  xide_model_error why={0};xvalue* candidate=xideProjectReferencesRenameResult(q,0,NULL,NULL,&why);
  CHECK(candidate && xrtValueCount(candidate)==4 && xrtValueCount(testGet(candidate,"inputGuards"))>=5 && xrtValueCount(testGet(testGet(candidate,"workspaceEdit"),"documentChanges"))==5);
  if(keep)*keep=candidate;else xrtValueRelease(candidate);
 }
 xideProjectReferencesQueryDestroy(q);return 1;
}
static int cursorRun(void)
{
 char* dir=xrtPathAbs("build/rename-cursor/源码😀");CHECK(dir && xrtDirCreateAll(dir));
 const char* names[]={"main.xl","second.xl","unrelated.xl","standalone.xl","shared.xl","unopened.xl","isolated.xl","A.xlp","B.xlp","C.xlp","absent-xl.exe"};char* paths[11];
 for(size_t i=0;i<11;++i){paths[i]=xrtPathJoin(dir,names[i]);CHECK(paths[i]);}
 const char* raw="\xef\xbb\xbf" "import shared\r\nfunc main(...) as int\r\n return shared.compute(1)\r\nend\r\n";
 const char* dep="\xef\xbb\xbf" "export func compute(int value) as int\r\n return value\r\nend\r\n";
 const char *consumer="import shared\nfunc main(...) as int\n return shared.compute(2)\nend\n",*unrelated="func compute(int value) as int\n return value\nend\nfunc main(...) as int\n return compute(3)\nend\n";
 CHECK(writeText(paths[0],raw) && writeText(paths[1],consumer) && writeText(paths[2],unrelated) && writeText(paths[3],consumer) && writeText(paths[4],dep) && writeText(paths[5],consumer) && writeText(paths[6],unrelated));
 const char *am[]={paths[0],paths[4]},*bm[]={paths[1],paths[4],paths[5]},*cm[]={paths[6]};
 xls_context* contexts[]={context(paths[7],dir,paths[0],am,2),context(paths[8],dir,paths[1],bm,3),context(paths[9],dir,paths[2],cm,1)};
 xls_context* single=context(NULL,dir,paths[3],NULL,0);CHECK(contexts[0] && contexts[1] && contexts[2] && single);
 xide_rename_scope* scope=membership(contexts,single);xls_text* text=xlsTextCreate(dep,strlen(dep),strlen(dep),NULL);
 xls_analysis_document docs[]={{"dependency",paths[4],text,41,UINT_MAX}};
 xide_toolchains* cache=xideToolchainsCreate();xide_project_references* store=xideProjectReferencesCreate(cache,paths[10],NULL);CHECK(scope && text && cache && store);
 int line,column;CHECK(xideLspOffsetToPosition(dep,(int)(strstr(dep,"compute")-dep+2),&line,&column));
 xstrview name=xrtStrView("renamed_😀");
 CHECK(!xideProjectReferencesSubmitRenameAt(store,scope,contexts[0],docs,1,"dependency",paths[4],line,column,xrtStrView("return")));
 CHECK(!xideProjectReferencesSubmitRenameAt(store,scope,contexts[2],docs,1,"dependency",paths[4],line,column,name));
 CHECK(!xideProjectReferencesSubmitRenameAt(store,scope,contexts[0],docs,1,NULL,paths[4],line,column,name));
 CHECK(cursorResult(xideProjectReferencesSubmitRenameAt(store,scope,contexts[0],docs,1,"dependency",paths[4],line,column,name),1,NULL,NULL));
 CHECK(cursorResult(xideProjectReferencesSubmitRenameAt(store,scope,contexts[0],docs,1,"dependency",paths[4],line,column,name),1,NULL,NULL));
 /* A qualified use resolves the same declaration without a spelling search. */
 xls_text* entry=xlsTextCreate(raw,strlen(raw),strlen(raw),NULL);int useLine,useColumn;
 CHECK(entry && xideLspOffsetToPosition(raw,(int)(strstr(raw,"compute(1)")-raw+1),&useLine,&useColumn));
 xls_analysis_document use[]={ {"entry",paths[0],entry,42,UINT_MAX} };
 CHECK(cursorResult(xideProjectReferencesSubmitRenameAt(store,scope,contexts[0],use,1,"entry",paths[0],useLine,useColumn,name),1,NULL,NULL));
 const char *onlyA[]={paths[0]},*onlyB[]={paths[1],paths[5]};
 xls_context* restricted[]={context(paths[7],dir,paths[0],onlyA,1),context(paths[8],dir,paths[1],onlyB,2),xlsContextRetain(contexts[2])};
 CHECK(restricted[0] && restricted[1] && restricted[2]);xide_rename_scope* missing=membership(restricted,single);CHECK(missing);
 CHECK(cursorResult(xideProjectReferencesSubmitRenameAt(store,missing,restricted[0],use,1,"entry",paths[0],useLine,useColumn,name),0,"outside explicitly configured membership",NULL));
 xideRenameScopeRelease(missing);for(size_t i=0;i<3;++i)xlsContextRelease(restricted[i]);xlsTextRelease(entry);
 /* Reserved words/whitespace are not a semantic target. */
 CHECK(cursorResult(xideProjectReferencesSubmitRenameAt(store,scope,contexts[0],docs,1,"dependency",paths[4],0,1,name),0,"complete verified reference",NULL));
 CHECK(writeText(paths[6],"func broken(\n"));
 CHECK(cursorResult(xideProjectReferencesSubmitRenameAt(store,scope,contexts[0],docs,1,"dependency",paths[4],line,column,name),0,"configured member",NULL));CHECK(writeText(paths[6],unrelated));
 fenceEnabled=1;fenceAt=SIZE_MAX;fenceFired=0;fencePath=paths[6];fenceBody="func main(...) as int\n return 5\nend\n";
 CHECK(cursorResult(xideProjectReferencesSubmitRenameAt(store,scope,contexts[0],docs,1,"dependency",paths[4],line,column,name),0,"after declaration composition",NULL));CHECK(fenceFired && fenceOkay);fenceEnabled=0;CHECK(writeText(paths[6],unrelated));
 fenceEnabled=1;fenceAt=SIZE_MAX;fenceFired=0;fencePath=NULL;fenceReached=xrtEventCreate(false,false);fenceResume=xrtEventCreate(false,false);CHECK(fenceReached && fenceResume);
 xide_reference_query* q=xideProjectReferencesSubmitRenameAt(store,scope,contexts[0],docs,1,"dependency",paths[4],line,column,name);
 CHECK(q && xrtEventWaitFor(fenceReached,XIDE_XRT_TIMEOUT_MS(10000))==XWAIT_OK);xideProjectReferencesCancel(q);xrtEventSet(fenceResume);
 enum xide_reference_query_state state;
 for(unsigned i=0;;++i){state=xideProjectReferencesResult(q,NULL,NULL,NULL,0);if(state!=XIDE_REFERENCES_PENDING)break;if(i==10000){CHECK(0);}xrtSleep(1);}
 CHECK(state==XIDE_REFERENCES_CANCELLED && !xideProjectReferencesRetainRenamePlan(q));xideProjectReferencesQueryDestroy(q);
 fenceEnabled=0;xrtEventDestroy(fenceReached);xrtEventDestroy(fenceResume);fenceReached=fenceResume=NULL;
 /* Captured inputs outlive their caller and no draft mutates source bytes. */
 q=xideProjectReferencesSubmitRenameAt(store,scope,contexts[0],docs,1,"dependency",paths[4],line,column,name);CHECK(q);
 xideRenameScopeRelease(scope);for(size_t i=0;i<3;++i)xlsContextRelease(contexts[i]);xlsContextRelease(single);xlsTextRelease(text);
 xvalue* kept=NULL;CHECK(cursorResult(q,1,NULL,&kept));xideProjectReferencesDestroy(store);xideToolchainsDestroy(cache);
 char* encoded=xrtJsonStringify(kept,false,NULL);CHECK(encoded && strstr(encoded,"renamed_"));xrtFree(encoded);xrtValueRelease(kept);
 size_t size=0;unsigned char* disk=xrtFileReadAll(paths[4],&size);CHECK(disk && size==strlen(dep) && !memcmp(disk,dep,size));xrtFree(disk);
 for(size_t i=0;i<11;++i)xrtFree(paths[i]);
 xrtFree(dir);return 1;
}
static int standaloneCursor(void)
{
 char* dir=xrtPathAbs("build/rename-cursor/single");CHECK(dir && xrtDirCreateAll(dir));
 char *path=xrtPathJoin(dir,"single.xl"),*compiler=xrtPathJoin(dir,"absent-xl.exe");
 const char* body="func compute(int value) as int\n return value\nend\nfunc main(...) as int\n return compute(3)\nend\n";
 CHECK(path && compiler && writeText(path,body));xvalue* wire=xrtJsonParse(xrtStrView("{\"schema\":1,\"selectedProject\":\"\",\"packagePath\":null,\"projects\":[]}"));
 xide_analysis_catalog* catalog=xideAnalysisCatalogCreate(wire,NULL,0);xrtValueRelease(wire);xls_context* file=xideAnalysisCatalogFile(catalog,path);
 xide_rename_scope* scope=xideRenameScopeCreate(catalog,&file,1,0,NULL,NULL,NULL);xls_text* text=xlsTextCreate(body,strlen(body),strlen(body),NULL);
 xls_analysis_document docs[]={{"single",path,text,19,0}};xide_toolchains* cache=xideToolchainsCreate();xide_project_references* store=xideProjectReferencesCreate(cache,compiler,NULL);
 CHECK(catalog && file && scope && text && cache && store);int line,column;CHECK(xideLspOffsetToPosition(body,(int)(strstr(body,"compute(3)")-body+1),&line,&column));
 xide_reference_query* q=xideProjectReferencesSubmitRenameAt(store,scope,file,docs,1,"single",path,line,column,xrtStrView("renamed"));CHECK(q);
 const xvalue* rows=NULL;xide_project_reference_stats stats;char error[512]={0};enum xide_reference_query_state state;
 for(unsigned i=0;;++i){state=xideProjectReferencesResult(q,&rows,&stats,error,sizeof(error));if(state!=XIDE_REFERENCES_PENDING)break;if(i==10000){CHECK(0);}xrtSleep(1);}
 if(state!=XIDE_REFERENCES_READY)fprintf(stderr,"standalone cursor state=%d error=%s\n",state,error);
 CHECK(state==XIDE_REFERENCES_READY && rows && xrtValueCount(rows)==2 && stats.workspaceScopes==1 && stats.workspaceCompleted==1);
 xide_rename_plan* plan=xideProjectReferencesRetainRenamePlan(q);xide_rename_plan_info info;xide_rename_plan_document document;
 CHECK(plan && xideRenamePlanInfo(plan,&info) && info.documents==1 && info.edits==2 && xideRenamePlanDocumentAt(plan,0,&document));
 CHECK(document.hasVersion && document.version==INT_MIN && !document.coordinatePrefix && !strstr(document.after.Data,"compute") && strstr(document.after.Data,"renamed(3)"));
 xvalue* candidate=xideProjectReferencesRenameResult(q,0,NULL,NULL,NULL);CHECK(candidate && xrtValueCount(testGet(candidate,"inputGuards"))>=2);
 xrtValueRelease(candidate);xideRenamePlanRelease(plan);xideProjectReferencesQueryDestroy(q);xideProjectReferencesDestroy(store);xideToolchainsDestroy(cache);
 xlsTextRelease(text);xideRenameScopeRelease(scope);xlsContextRelease(file);xideAnalysisCatalogDestroy(catalog);xrtFree(path);xrtFree(compiler);xrtFree(dir);return 1;
}
int main(void)
{if(!runDraft() || !cursorRun() || !standaloneCursor())return 1;printf("project_rename_at_test passed: %u checks; real imported/standalone cursors, original native declaration, four scopes, complete rechecks/guards, Unicode BOM CRLF, stale/unsupported/cancelled rejection and detached read-only candidate\n",checks);return 0;}
