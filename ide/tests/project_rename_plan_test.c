#define main originalWorkspaceMain
#include "project_rename_workspace_test.c"
#undef main
#undef CHECK
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"project_rename_plan_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static int draftResult(xide_reference_query* q,int ready,const char* reason,int warm,xide_rename_plan** keep)
{
 CHECK(q);const xvalue* rows=NULL;char error[512]={0};xide_project_reference_stats stats;enum xide_reference_query_state state;
 for(unsigned i=0;;++i){state=xideProjectReferencesResult(q,&rows,&stats,error,sizeof(error));if(state!=XIDE_REFERENCES_PENDING)break;if(i>=10000){CHECK(0);}xrtSleep(1);}
 if(!ready){if(state!=XIDE_REFERENCES_FAILED)fprintf(stderr,"state=%d error=%s\n",state,error);CHECK(state==XIDE_REFERENCES_FAILED && !rows && strstr(error,reason) && !xideProjectReferencesRetainRenamePlan(q));}
 else{
  if(state!=XIDE_REFERENCES_READY)fprintf(stderr,"state=%d error=%s\n",state,error);
  CHECK(state==XIDE_REFERENCES_READY && rows && xrtValueCount(rows)==5 && stats.workspaceCompleted==4);if(warm)CHECK(!stats.captured && !stats.memberTasks && stats.reused>=4);
  CHECK(stats.renameRechecks>=4 && stats.renameUnchanged>=1 && stats.renameSources>=5 && stats.renameReferences>=5);
  xide_rename_plan* plan=xideProjectReferencesRetainRenamePlan(q);xide_rename_plan_info info;CHECK(plan && xideRenamePlanInfo(plan,&info) && info.documents==5 && info.edits==5);size_t overlays=0,boms=0;
  for(size_t i=0;i<info.documents;++i){xide_rename_plan_document d;CHECK(xideRenamePlanDocumentAt(plan,i,&d) && d.count==1 && strstr(d.before.Data,"compute") && !strstr(d.after.Data,"compute") && strstr(d.after.Data,"renamed_😀"));if(d.hasVersion){++overlays;CHECK(d.version==INT_MAX && d.coordinatePrefix==1);}if(d.before.Size>=3 && !memcmp(d.before.Data,"\xef\xbb\xbf",3)){++boms;CHECK(d.after.Size>=3 && !memcmp(d.after.Data,"\xef\xbb\xbf",3));}xide_rename_plan_edit e;size_t mapped;CHECK(xideRenamePlanEditAt(plan,d.first,&e) && xideRenamePlanMapOffset(plan,i,(size_t)e.start.offset,&mapped) && mapped==e.afterStart);}
  CHECK(overlays==1 && boms==2);xvalue* wire=xideRenamePlanWorkspaceEdit(plan,1,NULL,NULL);CHECK(wire && xrtValueCount(testGet(wire,"documentChanges"))==5);xrtValueRelease(wire);
#ifdef XIDE_RENAME_PLAN_READY_TEST
  CHECK(XIDE_RENAME_PLAN_READY_TEST(q));
#endif
  if(keep)*keep=plan;else xideRenamePlanRelease(plan);
 }
 xideProjectReferencesQueryDestroy(q);return 1;
}
static int runDraft(void)
{
 char* dir=xrtPathAbs("build/project-rename-plan/源码😀");CHECK(dir && xrtDirCreateAll(dir));const char* names[]={"main.xl","second.xl","unrelated.xl","standalone.xl","shared.xl","unopened.xl","isolated.xl","A.xlp","B.xlp","C.xlp","absent-xl.exe"};char* paths[11];for(size_t i=0;i<11;++i){paths[i]=xrtPathJoin(dir,names[i]);CHECK(paths[i]);}
 const char* raw="\xef\xbb\xbf" "import shared\r\nfunc main(...) as int\r\n return shared.compute(1)\r\nend\r\n";
 const char* dep="\xef\xbb\xbf" "export func compute(int value) as int\r\n return value\r\nend\r\n";
 const char *consumer="import shared\nfunc main(...) as int\n return shared.compute(2)\nend\n",*unrelated="func compute(int value) as int\n return value\nend\nfunc main(...) as int\n return compute(3)\nend\n";
 CHECK(writeText(paths[0],raw) && writeText(paths[1],consumer) && writeText(paths[2],unrelated) && writeText(paths[3],consumer) && writeText(paths[4],dep) && writeText(paths[5],consumer) && writeText(paths[6],unrelated));
 const char *am[]={paths[0],paths[4]},*bm[]={paths[1],paths[4],paths[5],paths[3]},*cm[]={paths[6]};xls_context* contexts[]={context(paths[7],dir,paths[0],am,2),context(paths[8],dir,paths[1],bm,4),context(paths[9],dir,paths[2],cm,1)};xls_context* single=context(NULL,dir,paths[3],NULL,0);CHECK(contexts[0] && contexts[1] && contexts[2] && single);xide_rename_scope* scope=membership(contexts,single);xide_rename_target* seed=target(paths[0],raw,paths[4],dep);xls_text* text=xlsTextCreate(raw,strlen(raw),strlen(raw),NULL);CHECK(scope && seed && text);xls_analysis_document docs[]={{"entry",paths[0],text,41,UINT_MAX}};
 xide_toolchains* cache=xideToolchainsCreate();xide_project_references* store=xideProjectReferencesCreate(cache,paths[10],NULL);CHECK(cache && store);xstrview name=xrtStrView("renamed_😀");
#ifdef XIDE_RENAME_SCOPE_CAPTURE_TEST
 CHECK(XIDE_RENAME_SCOPE_CAPTURE_TEST(scope));
#endif
 CHECK(!xideProjectReferencesSubmitRenamePlan(store,scope,seed,docs,1,"entry",xrtStrView("return")));
 CHECK(draftResult(xideProjectReferencesSubmitRenamePlan(store,scope,seed,docs,1,"entry",name),1,NULL,0,NULL));CHECK(draftResult(xideProjectReferencesSubmitRenamePlan(store,scope,seed,docs,1,"entry",name),1,NULL,1,NULL));
 /* Draft construction writes neither source files nor editor snapshots. */
 size_t diskSize=0;unsigned char* disk=xrtFileReadAll(paths[4],&diskSize);CHECK(disk && diskSize==strlen(dep) && !memcmp(disk,dep,diskSize) && xlsTextSize(text)==strlen(raw) && !memcmp(xlsTextData(text),raw,strlen(raw)));xrtFree(disk);
 fenceEnabled=1;fenceAt=SIZE_MAX;fenceFired=0;fencePath=paths[6];fenceBody="func main(...) as int\n return 5\nend\n";CHECK(draftResult(xideProjectReferencesSubmitRenamePlan(store,scope,seed,docs,1,"entry",name),0,"after declaration composition",0,NULL));CHECK(fenceFired && fenceOkay);fenceEnabled=0;CHECK(writeText(paths[6],unrelated));
 fenceEnabled=1;fenceAt=SIZE_MAX;fenceFired=0;fencePath=NULL;fenceReached=xrtEventCreate(false,false);fenceResume=xrtEventCreate(false,false);CHECK(fenceReached && fenceResume);xide_reference_query* q=xideProjectReferencesSubmitRenamePlan(store,scope,seed,docs,1,"entry",name);CHECK(q && xrtEventWaitFor(fenceReached,XIDE_XRT_TIMEOUT_MS(10000))==XWAIT_OK);xideProjectReferencesCancel(q);xrtEventSet(fenceResume);enum xide_reference_query_state state;for(unsigned i=0;;++i){state=xideProjectReferencesResult(q,NULL,NULL,NULL,0);if(state!=XIDE_REFERENCES_PENDING)break;if(i>=10000){CHECK(0);}xrtSleep(1);}CHECK(state==XIDE_REFERENCES_CANCELLED && !xideProjectReferencesRetainRenamePlan(q));xideProjectReferencesQueryDestroy(q);fenceEnabled=0;xrtEventDestroy(fenceReached);xrtEventDestroy(fenceResume);fenceReached=fenceResume=NULL;
 q=xideProjectReferencesSubmitRenamePlan(store,scope,seed,docs,1,"entry",name);CHECK(q);xideRenameTargetRelease(seed);xideRenameScopeRelease(scope);for(size_t i=0;i<3;++i)xlsContextRelease(contexts[i]);xlsContextRelease(single);xlsTextRelease(text);xide_rename_plan* kept=NULL;CHECK(draftResult(q,1,NULL,0,&kept));xideProjectReferencesDestroy(store);xideToolchainsDestroy(cache);xide_rename_plan_info info;CHECK(xideRenamePlanInfo(kept,&info) && info.documents==5);xvalue* wire=xideRenamePlanWorkspaceEdit(kept,0,NULL,NULL);CHECK(wire && xrtValueCount(testGet(wire,"documentChanges"))==5);xrtValueRelease(wire);xideRenamePlanRelease(kept);
 for(size_t i=0;i<11;++i)xrtFree(paths[i]);
 xrtFree(dir);return 1;
}
#ifndef XIDE_RENAME_PLAN_TEST_NO_MAIN
int main(void){if(!runDraft())return 1;printf("project_rename_plan_test passed: %u checks; guarded all-scope versioned rename candidate, complete affected native rechecks and unchanged guarded owners, BOM/newlines/Unicode, warm cached collection, immutable inputs, final mutation/cancellation and detached ownership\n",checks);return 0;}
#endif
