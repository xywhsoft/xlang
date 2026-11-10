#define main originalWorkspaceMain
#include "project_rename_workspace_test.c"
#undef main
#undef CHECK
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"project_reference_family_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static xide_rename_target* familyTarget(const char* path,const char* raw,const char* dep,const char* body)
{
 xls_semantic_input input[]={{path,raw,strlen(raw)},{dep,body,strlen(body)}};xls_semantic_check* check=xlsSemanticCheckCreate(path,input,2,1,NULL,NULL,NULL,0);xide_rename_target* result=NULL;
 if(check && xlsSemanticCheckInfo(check).accepted){xls_check_inputs info;xide_semantic_source sources[8];
  if(xlsSemanticCheckInputs(check,&info) && info.sources<=8){size_t i=0;for(;i<info.sources;++i){xls_check_source s;if(!xlsSemanticCheckSource(check,i,&s))break;sources[i]=(xide_semantic_source){s.path,s.text,s.kind,0};}
   const char* at=strstr(raw,"read() as int");int line,column;if(i==info.sources && at && xideLspOffsetToPosition(raw,(int)(at-raw),&line,&column))result=xideRenameTargetCreate(NULL,check,sources,info.sources,path,line,column,0,NULL,NULL,NULL);
  }
 }
 xlsSemanticCheckDestroy(check);return result;
}
static int familyResult(xide_reference_query* q,int ready,const char* reason,int warm,xide_reference_family** retained)
{
 CHECK(q);const xvalue* rows=(void*)1;char error[512]={0};xide_project_reference_stats stats;enum xide_reference_query_state state;
 for(unsigned i=0;;++i){state=xideProjectReferencesResult(q,&rows,&stats,error,sizeof(error));if(state!=XIDE_REFERENCES_PENDING)break;CHECK(i<10000);xrtSleep(1);}
 if(!ready){if(state!=XIDE_REFERENCES_FAILED)fprintf(stderr,"unexpected state:%d error:%s\n",state,error);CHECK(state==XIDE_REFERENCES_FAILED && !rows && strstr(error,reason) && !xideProjectReferencesRetainFamily(q));}
 else{
  if(state!=XIDE_REFERENCES_READY)fprintf(stderr,"state:%d error:%s\n",state,error);
  CHECK(state==XIDE_REFERENCES_READY && !rows && stats.workspaceScopes==4 && stats.workspaceCompleted==4 && stats.workspaceGuardBytes && stats.thread!=xrtThreadCurrentId());
  if(warm)CHECK(!stats.captured && !stats.memberTasks && stats.reused>=4);
  reference_output* native=xrtFutureValue(q->future);CHECK(native && native->graphs.Count>=4 && native->guards.Count>=4 && !native->rows.Count);
  xide_reference_family* f=xideProjectReferencesRetainFamily(q);xide_reference_family_info info;CHECK(f && xideSemanticReferenceFamilyInfo(f,&info) && info.members==4 && info.sources==3 && info.kinds==3);
  for(size_t i=0;i<info.members;++i){xide_semantic_source source;size_t start,end;CHECK(xideSemanticReferenceFamilyMember(f,i,&source,&start,&end) && end-start==4 && !memcmp(source.text+start,"read",4));}
  CHECK(!xideProjectReferencesTake(q));if(retained)*retained=f;else xideSemanticReferenceFamilyRelease(f);
 }
 xideProjectReferencesQueryDestroy(q);return 1;
}
static int runFamilyWorkspace(void)
{
 char* dir=xrtPathAbs("build/project-reference-family");CHECK(dir && xrtDirCreateAll(dir));const char* names[]={"a.xl","b.xl","c.xl","single.xl","dep.xl","isolated.xl","A.xlp","B.xlp","C.xlp","absent-xl.exe"};char* paths[10];for(size_t i=0;i<10;++i){paths[i]=xrtPathJoin(dir,names[i]);CHECK(paths[i]);}
 const char* dep="export protocol Reader\n func read() as int\nend\nexport class Base\n virtual func read() as int\n return 1\n end\nend\n";
 const char* a="import dep\nclass Derived extends dep.Base\n override func read() as int\n return 2\n end\nend\nfunc through(dep.Reader value) as int\n return value.read()\nend\nfunc main(...) as int\n return through(Derived())\nend\n";
 const char* b="import dep\nclass Other extends dep.Base\n override func read() as int\n return 3\n end\nend\nfunc through(dep.Reader value) as int\n return value.read()\nend\nfunc main(...) as int\n return through(Other())\nend\n";
 const char* plain="func main(...) as int\n return 0\nend\n";CHECK(writeText(paths[0],a) && writeText(paths[1],b) && writeText(paths[2],plain) && writeText(paths[3],plain) && writeText(paths[4],dep) && writeText(paths[5],plain));
 const char *am[]={paths[0],paths[4]},*bm[]={paths[1],paths[4]},*cm[]={paths[5]};xls_context* contexts[]={context(paths[6],dir,paths[0],am,2),context(paths[7],dir,paths[1],bm,2),context(paths[8],dir,paths[2],cm,1)};xls_context* single=context(NULL,dir,paths[3],NULL,0);CHECK(contexts[0] && contexts[1] && contexts[2] && single);
 xide_rename_scope* scope=membership(contexts,single);xide_rename_target* seed=familyTarget(paths[0],a,paths[4],dep);xls_text* text=xlsTextCreate(a,strlen(a),strlen(a),NULL);CHECK(scope && seed && text);xls_analysis_document docs[]={{"entry",paths[0],text,1,1}};
 xide_toolchains* cache=xideToolchainsCreate();xide_project_references* store=xideProjectReferencesCreate(cache,paths[9],NULL);CHECK(cache && store);
 CHECK(familyResult(xideProjectReferencesSubmitFamilyWorkspace(store,scope,seed,docs,1,"entry"),1,NULL,0,NULL));
 CHECK(familyResult(xideProjectReferencesSubmitFamilyWorkspace(store,scope,seed,docs,1,"entry"),1,NULL,1,NULL));
 for(size_t i=0;i<store->records.Count;++i){reference_record* r=records(store)[i];CHECK(r->graph);xide_reference_graph_info info;xide_reference_shard* shard=xideSemanticReferenceGraphRetainShard(r->graph);CHECK(shard==r->shard && xideSemanticReferenceGraphInfo(r->graph,&info) && r->bytes>=info.bytes);xideSemanticReferenceShardRelease(shard);}
 xls_context* reordered[]={contexts[2],contexts[1],contexts[0]};xide_rename_scope* other=membership(reordered,single);CHECK(other && familyResult(xideProjectReferencesSubmitFamilyWorkspace(store,other,seed,docs,1,"entry"),1,NULL,1,NULL));xideRenameScopeRelease(other);
 /* A scope with no family declarations still proves all configured members. */
 CHECK(writeText(paths[5],"func broken(\n"));CHECK(familyResult(xideProjectReferencesSubmitFamilyWorkspace(store,scope,seed,docs,1,"entry"),0,"configured member",0,NULL));CHECK(writeText(paths[5],plain));
 CHECK(familyResult(xideProjectReferencesSubmitFamilyWorkspace(store,scope,seed,docs,1,"entry"),1,NULL,0,NULL));
 fenceEnabled=1;fenceAt=SIZE_MAX;fenceFired=0;fencePath=paths[5];fenceBody="func main(...) as int\n return 4\nend\n";
 CHECK(familyResult(xideProjectReferencesSubmitFamilyWorkspace(store,scope,seed,docs,1,"entry"),0,"after declaration composition",0,NULL));CHECK(fenceFired && fenceOkay);fenceEnabled=0;CHECK(writeText(paths[5],plain));
 /* Imports alone do not grant workspace membership to related declarations. */
 xls_context* restricted[]={context(paths[6],dir,paths[0],am,1),context(paths[7],dir,paths[1],bm,1),contexts[2]};other=membership(restricted,single);CHECK(other && familyResult(xideProjectReferencesSubmitFamilyWorkspace(store,other,seed,docs,1,"entry"),0,"outside explicitly configured",0,NULL));xideRenameScopeRelease(other);xlsContextRelease(restricted[0]);xlsContextRelease(restricted[1]);
 CHECK(familyResult(xideProjectReferencesSubmitFamilyWorkspace(store,scope,seed,docs,1,"entry"),1,NULL,0,NULL));
 fenceEnabled=1;fenceAt=SIZE_MAX;fenceFired=0;fencePath=NULL;fenceReached=xrtEventCreate(false,false);fenceResume=xrtEventCreate(false,false);CHECK(fenceReached && fenceResume);
 xide_reference_query* q=xideProjectReferencesSubmitFamilyWorkspace(store,scope,seed,docs,1,"entry");CHECK(q && xrtEventWaitFor(fenceReached,XIDE_XRT_TIMEOUT_MS(10000))==XWAIT_OK);xideProjectReferencesCancel(q);xrtEventSet(fenceResume);enum xide_reference_query_state state;
 for(unsigned i=0;;++i){state=xideProjectReferencesResult(q,NULL,NULL,NULL,0);if(state!=XIDE_REFERENCES_PENDING)break;CHECK(i<10000);xrtSleep(1);}CHECK(state==XIDE_REFERENCES_CANCELLED && !xideProjectReferencesRetainFamily(q));xideProjectReferencesQueryDestroy(q);fenceEnabled=0;xrtEventDestroy(fenceReached);xrtEventDestroy(fenceResume);fenceReached=fenceResume=NULL;
 q=xideProjectReferencesSubmitFamilyWorkspace(store,scope,seed,docs,1,"entry");CHECK(q);xideRenameTargetRelease(seed);xideRenameScopeRelease(scope);for(size_t i=0;i<3;++i)xlsContextRelease(contexts[i]);xlsContextRelease(single);xlsTextRelease(text);xide_reference_family* retained=NULL;
 CHECK(familyResult(q,1,NULL,0,&retained));xideProjectReferencesDestroy(store);xideToolchainsDestroy(cache);xide_reference_key* held=xideSemanticReferenceFamilyKey(retained,0);CHECK(held);xideSemanticReferenceFamilyRelease(retained);xstrview name;int units;CHECK(xideSemanticReferenceKeyIdentifier(held,&name,&units) && name.Size==4);xideSemanticReferenceKeyRelease(held);
 for(size_t i=0;i<10;++i)xrtFree(paths[i]);
 xrtFree(dir);return 1;
}
int main(void){if(!runFamilyWorkspace())return 1;printf("project_reference_family_test passed: %u checks; all-scope native family closure, graph cache reuse, zero-target member guards, membership rejection, final mutation/cancellation and detached output\n",checks);return 0;}
