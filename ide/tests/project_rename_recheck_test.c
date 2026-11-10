#define XIDE_RENAME_PLAN_TEST_NO_MAIN
#include "project_rename_plan_test.c"
#undef CHECK
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"project_rename_recheck_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static xide_rename_scope* recheckScope(xls_context* context)
{
 xvalue *root=xrtValueObject(),*projects=xrtValueArray(),*row=xideAnalysisContextWrite(context);int okay=root && projects && row && xrtValueArrayAppendNew(projects,row);
 okay=okay && testPut(root,"schema",xrtValueInt(1)) && testPut(root,"selectedProject",xrtValueString(testView(xlsContextSpec(context)->project))) && testPut(root,"packagePath",xrtValueNull());
 if(okay){okay=testPut(root,"projects",projects);projects=NULL;}xrtValueRelease(projects);
 xide_analysis_catalog* catalog=okay?xideAnalysisCatalogCreate(root,NULL,0):NULL;xrtValueRelease(root);
 xide_rename_scope* scope=catalog?xideRenameScopeCreate(catalog,NULL,0,0,NULL,NULL,NULL):NULL;xideAnalysisCatalogDestroy(catalog);return scope;
}
static xide_rename_target* recheckSeed(const char* entry,const char* raw,const char* needle)
{
 xls_semantic_input input={entry,raw,strlen(raw)};xls_semantic_check* check=xlsSemanticCheckCreate(entry,&input,1,1,NULL,NULL,NULL,0);if(!check || !xlsSemanticCheckInfo(check).accepted){xlsSemanticCheckDestroy(check);return NULL;}
 xls_check_inputs info;xlsSemanticCheckInputs(check,&info);xide_semantic_source* sources=xrtCalloc(info.sources,sizeof(*sources));if(!sources){xlsSemanticCheckDestroy(check);return NULL;}
 for(size_t i=0;i<info.sources;++i){xls_input_source s;xlsSemanticCheckSource(check,i,&s);sources[i]=(xide_semantic_source){s.path,s.text,s.kind,0};}
 int line,column;xide_rename_target* target=NULL;const char* at=strstr(raw,needle);
 if(at && xideLspOffsetToPosition(raw,(int)(at-raw),&line,&column))target=xideRenameTargetCreate(NULL,check,sources,info.sources,entry,line,column,0,NULL,NULL,NULL);
 xrtFree(sources);xlsSemanticCheckDestroy(check);return target;
}
static int checkedCandidate(xide_reference_query* query,int success,const char* expected,size_t sources)
{
 CHECK(query);enum xide_reference_query_state state;const xvalue* result=NULL;xide_project_reference_stats stats;char error[512]={0};
 for(size_t i=0;;++i){state=xideProjectReferencesResult(query,&result,&stats,error,sizeof(error));if(state!=XIDE_REFERENCES_PENDING)break;if(i>=20000)CHECK(0);xrtSleep(1);}
 if(success){if(state!=XIDE_REFERENCES_READY)fprintf(stderr,"recheck state=%d: %s\n",state,error);CHECK(state==XIDE_REFERENCES_READY && result && stats.renameRechecks==1 && stats.renameSources==sources && !stats.renameUnchanged && stats.renameReferences>=2);xide_rename_plan* plan=xideProjectReferencesRetainRenamePlan(query);CHECK(plan);xideRenamePlanRelease(plan);}
 else{if(state!=XIDE_REFERENCES_FAILED)fprintf(stderr,"recheck state=%d: %s\n",state,error);CHECK(state==XIDE_REFERENCES_FAILED && !result && strstr(error,expected) && !xideProjectReferencesRetainRenamePlan(query));}
 xideProjectReferencesQueryDestroy(query);return 1;
}
static int wideWorkspace(const char* base,int explicitPaths)
{
 char* dir=xrtPathJoin(base,explicitPaths?"wide-explicit":"wide");CHECK(dir && xrtDirCreateAll(dir));const size_t count=130;char** members=xrtCalloc(count,sizeof(*members));CHECK(members);char* raw=xrtMalloc(count*64+256);CHECK(raw);size_t used=0;
 for(size_t i=1;i<count;++i){if(explicitPaths)used+=(size_t)sprintf(raw+used,"import \"./dep%03zu.xl\" as dep%03zu\r\n",i,i);else used+=(size_t)sprintf(raw+used,"import dep%03zu\r\n",i);}
 used+=(size_t)sprintf(raw+used,"func main(...) as int\r\n return dep001.compute()\r\nend\r\n");
 for(size_t i=0;i<count;++i){char name[64];snprintf(name,sizeof(name),i?"dep%03zu.xl":"main%03zu.xl",i);members[i]=xrtPathJoin(dir,name);CHECK(members[i] && writeText(members[i],i?"export func compute() as int\r\n return 1\r\nend\r\n":raw));}
 char *project=xrtPathJoin(dir,"wide.xlp"),*compiler=xrtPathJoin(dir,"absent-xl.exe");CHECK(project && compiler);
 xls_context* ctx=context(project,dir,members[0],(const char*const*)members,count);xide_rename_scope* scope=ctx?recheckScope(ctx):NULL;xide_rename_target* seed=recheckSeed(members[0],raw,"compute()");CHECK(ctx && scope && seed);
 xide_toolchains* cache=xideToolchainsCreate();xide_project_references* store=xideProjectReferencesCreate(cache,compiler,NULL);CHECK(cache && store);
 CHECK(checkedCandidate(xideProjectReferencesSubmitRenamePlan(store,scope,seed,NULL,0,NULL,testView("新值😀")),1,NULL,count));
 CHECK(checkedCandidate(xideProjectReferencesSubmitRenamePlan(store,scope,seed,NULL,0,NULL,testView("新值😀")),1,NULL,count));
 const char* original="export func compute() as int\r\n return 1\r\nend\r\n";size_t size=0;unsigned char* disk=xrtFileReadAll(members[1],&size);CHECK(disk && size==strlen(original) && !memcmp(disk,original,size));xrtFree(disk);
 /* Mutation after complete native checking but before its original provider
  * guard is revalidated must discard the entire candidate. */
 fenceEnabled=1;fenceAt=SIZE_MAX-1;fenceFired=0;fencePath=members[0];fenceBody="func main(...) as int\n return 1\nend\n";
 CHECK(checkedCandidate(xideProjectReferencesSubmitRenamePlan(store,scope,seed,NULL,0,NULL,testView("新值😀")),0,"changed",count) && fenceFired && fenceOkay);fenceEnabled=0;
 CHECK(writeText(members[0],raw));
 xideProjectReferencesDestroy(store);xideToolchainsDestroy(cache);xideRenameTargetRelease(seed);xideRenameScopeRelease(scope);xlsContextRelease(ctx);
 for(size_t i=0;i<count;++i)xrtFree(members[i]);
 xrtFree(members);xrtFree(raw);xrtFree(project);xrtFree(compiler);xrtFree(dir);return 1;
}
static int conflictWorkspace(const char* base,const char* name,const char* raw,const char* needle,const char* replacement)
{
 char* dir=xrtPathJoin(base,name);CHECK(dir && xrtDirCreateAll(dir));char *entry=xrtPathJoin(dir,"main.xl"),*project=xrtPathJoin(dir,"project.xlp"),*compiler=xrtPathJoin(dir,"absent-xl.exe");CHECK(entry && project && compiler && writeText(entry,raw));
 const char* members[]={entry};xls_context* ctx=context(project,dir,entry,members,1);xide_rename_scope* scope=ctx?recheckScope(ctx):NULL;xide_rename_target* seed=recheckSeed(entry,raw,needle);CHECK(ctx && scope && seed);
 xide_toolchains* cache=xideToolchainsCreate();xide_project_references* store=xideProjectReferencesCreate(cache,compiler,NULL);CHECK(cache && store);
 CHECK(checkedCandidate(xideProjectReferencesSubmitRenamePlan(store,scope,seed,NULL,0,NULL,testView(replacement)),0,"bindings",1));
 xideProjectReferencesDestroy(store);xideToolchainsDestroy(cache);xideRenameTargetRelease(seed);xideRenameScopeRelease(scope);xlsContextRelease(ctx);xrtFree(entry);xrtFree(project);xrtFree(compiler);xrtFree(dir);return 1;
}
static int foreignWorkspace(const char* base)
{
 const char* raw="extern \"native_call\" func nativeCall(c_int number) as c_int\nfunc main(...) as int\n return int(nativeCall(c_int(1)))\nend\n";
 char* dir=xrtPathJoin(base,"foreign-policy");CHECK(dir && xrtDirCreateAll(dir));
 char *entry=xrtPathJoin(dir,"main.xl"),*project=xrtPathJoin(dir,"project.xlp"),*compiler=xrtPathJoin(dir,"absent-xl.exe");CHECK(entry && project && compiler && writeText(entry,raw));
 const char* members[]={entry};xls_context* ctx=context(project,dir,entry,members,1);xide_rename_scope* scope=ctx?recheckScope(ctx):NULL;xide_rename_target* seed=recheckSeed(entry,raw,"nativeCall(c_int");CHECK(ctx && scope && seed);
 xide_toolchains* cache=xideToolchainsCreate();xide_project_references* store=xideProjectReferencesCreate(cache,compiler,NULL);CHECK(cache && store);
 /* The same cached provider remains usable for references; policy is a rename
  * gate, before collecting edits or running hypothetical native checks. */
 xls_text* text=xlsTextCreate(raw,strlen(raw),strlen(raw),NULL);CHECK(text);xls_analysis_document document={"entry",entry,text,1,1};
 for(size_t pass=0;pass<2;++pass){
  xide_reference_query* q=xideProjectReferencesSubmitFamilyReferences(store,scope,seed,&document,1,"entry");CHECK(q);const xvalue* rows=NULL;char error[512]={0};xide_project_reference_stats stats;enum xide_reference_query_state state;
  for(size_t i=0;;++i){state=xideProjectReferencesResult(q,&rows,&stats,error,sizeof(error));if(state!=XIDE_REFERENCES_PENDING)break;if(i>=10000)CHECK(0);xrtSleep(1);}
  CHECK(state==XIDE_REFERENCES_READY && rows && xrtValueCount(rows)==2);xide_reference_family* family=xideProjectReferencesRetainFamily(q);CHECK(family && xideSemanticReferenceFamilyRenamePolicy(family)==XLS_RENAME_POLICY_FOREIGN);xideSemanticReferenceFamilyRelease(family);xideProjectReferencesQueryDestroy(q);
  q=xideProjectReferencesSubmitRenamePlan(store,scope,seed,NULL,0,NULL,testView("renamedCall"));CHECK(q);rows=NULL;memset(&stats,0,sizeof(stats));
  for(size_t i=0;;++i){state=xideProjectReferencesResult(q,&rows,&stats,error,sizeof(error));if(state!=XIDE_REFERENCES_PENDING)break;if(i>=10000)CHECK(0);xrtSleep(1);}
  CHECK(state==XIDE_REFERENCES_FAILED && !rows && strstr(error,"foreign ABI") && !stats.renameRechecks && !xideProjectReferencesRetainRenamePlan(q));xideProjectReferencesQueryDestroy(q);
 }
 size_t size=0;unsigned char* bytes=xrtFileReadAll(entry,&size);CHECK(bytes && size==strlen(raw) && !memcmp(bytes,raw,size));xrtFree(bytes);
 xideProjectReferencesDestroy(store);xideToolchainsDestroy(cache);xideRenameTargetRelease(seed);xideRenameScopeRelease(scope);xlsContextRelease(ctx);xlsTextRelease(text);xrtFree(entry);xrtFree(project);xrtFree(compiler);xrtFree(dir);return 1;
}
int main(void)
{
 char* dir=xrtPathAbs("build/project-recheck");if(!dir || !xrtDirCreateAll(dir) || !runDraft() || !foreignWorkspace(dir) || !wideWorkspace(dir,0) || !wideWorkspace(dir,1) ||
  !conflictWorkspace(dir,"capture","func main(...) as int\n int kept=7\n if true\n int value=1\n print(kept)\n print(value)\n end\n return kept\nend\n","value=","kept") ||
  !conflictWorkspace(dir,"overload","func first(int x) as int\n return x\nend\nfunc second(float x) as int\n return 2\nend\nfunc main(...) as int\n return second(1)\nend\n","first(","second"))return 1;
 xrtFree(dir);printf("project_rename_recheck_test passed: %u checks; actual130-file actor candidate checks, complete binding capture/overload conflicts, guarded unchanged providers, cache reuse, no source writes and post-check source mutation rejection\n",checks);return 0;
}
