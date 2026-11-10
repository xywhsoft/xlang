#define main originalWorkspaceMain
#include "project_rename_workspace_test.c"
#undef main
#undef CHECK
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"project_family_references_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static xide_rename_target* occurrenceTarget(const char* path,const char* raw,const char* dependency,const char* body)
{
 xls_semantic_input inputs[]={{path,raw,strlen(raw)},{dependency,body,strlen(body)}};xls_semantic_check* check=xlsSemanticCheckCreate(path,inputs,2,1,NULL,NULL,NULL,0);xide_rename_target* t=NULL;
 if(check && xlsSemanticCheckInfo(check).accepted){xls_check_inputs info;xide_semantic_source sources[8];
  if(xlsSemanticCheckInputs(check,&info) && info.sources<=8){size_t i=0;for(;i<info.sources;++i){xls_check_source source;if(!xlsSemanticCheckSource(check,i,&source))break;const char* original=!strcmp(source.path,path) ? raw : !strcmp(source.path,dependency) ? body : "";sources[i]=(xide_semantic_source){source.path,source.text,source.kind,!memcmp(original,"\xef\xbb\xbf",strlen(original)>=3 ? 3 : 0) && strlen(original)>=3};}
   const char* at=strstr(raw,"read() as int");int line,column;if(i==info.sources && at && xideLspOffsetToPosition(raw,(int)(at-raw),&line,&column))t=xideRenameTargetCreate(NULL,check,sources,info.sources,path,line,column,0,NULL,NULL,NULL);
  }
 }
 xlsSemanticCheckDestroy(check);return t;
}
static int missingDeclarationRejected(const reference_output* output)
{
 reference_output* probe=xrtCalloc(1,sizeof(*probe));CHECK(probe && xrtArrayInit(&probe->rows,sizeof(result_row)));probe->family=xideSemanticReferenceFamilyRetain(output->family);xide_semantic_source source;size_t start,end;CHECK(xideSemanticReferenceFamilyMember(probe->family,0,&source,&start,&end));wchar_t* key=xidePathNativeWide(source.path);CHECK(key);size_t removed=0;
 for(size_t i=0;i<output->rows.Count;++i){const result_row* original=(const result_row*)output->rows.Data+i;if(!workspaceFamilyDeclarationOrder(original,key,start,end)){++removed;continue;}result_row row=*original;row.path=xrtStrDup(original->path);row.key=xidePathNativeWide(original->path);row.proof=xrtValueRetain((xvalue*)original->proof);row.ownedProof=1;CHECK(row.path && row.key && row.proof && xrtArrayPush(&probe->rows,&row));}
 free(key);xcancel* cancel=xrtCancelCreate();CHECK(removed==1 && cancel && !workspaceFamilyCompact(probe,0,cancel) && strstr(probe->error,"no exact captured"));xrtCancelDestroy(cancel);freeOutput(probe,NULL);return 1;
}
static int occurrenceResult(xide_reference_query* q,int ready,const char* reason,int warm,char* const* paths,const char* const* bodies,xvalue** kept,xide_reference_family** family)
{
 CHECK(q);const xvalue* wire=NULL;char error[512]={0};xide_project_reference_stats stats;enum xide_reference_query_state state;
 for(unsigned i=0;;++i){state=xideProjectReferencesResult(q,&wire,&stats,error,sizeof(error));if(state!=XIDE_REFERENCES_PENDING)break;if(i>=10000){CHECK(0);}xrtSleep(1);}
 if(!ready){if(state!=XIDE_REFERENCES_FAILED)fprintf(stderr,"state=%d error=%s\n",state,error);CHECK(state==XIDE_REFERENCES_FAILED && !wire && strstr(error,reason) && !xideProjectReferencesRetainFamily(q));}
 else{
  if(state!=XIDE_REFERENCES_READY)fprintf(stderr,"state=%d error=%s\n",state,error);
  CHECK(state==XIDE_REFERENCES_READY && wire && xrtValueCount(wire)==10 && stats.locations==10 && stats.workspaceCompleted==4 && stats.workspaceDuplicates>=2);
  if(warm)CHECK(!stats.captured && !stats.memberTasks && stats.reused>=4);
  reference_output* output=xrtFutureValue(q->future);CHECK(output && output->rows.Count==10 && output->guards.Count>=4);size_t observed[6]={0};
  for(size_t i=0;i<output->rows.Count;++i){result_row* row=(result_row*)output->rows.Data+i;size_t file=0;while(file<6 && !same(paths[file],row->path))++file;CHECK(file<6 && file!=2 && row->start.offset>=0 && row->end.offset-row->start.offset==4);const char* text=bodies[file];if(strlen(text)>=3 && !memcmp(text,"\xef\xbb\xbf",3))text+=3;CHECK((size_t)row->end.offset<=strlen(text) && !memcmp(text+row->start.offset,"read",4) && strncmp(text+row->end.offset,"(int value)",11));++observed[file];int64 schema;const xvalue* proof=testGet(xrtValueArrayGet(wire,i),"xlang");CHECK(proof && xrtValueGetInt(testGet(proof,"schema"),&schema) && schema==1 && testGet(proof,"document"));}
  CHECK(observed[0]==3 && observed[1]==3 && !observed[2] && observed[3]==1 && observed[4]==2 && observed[5]==1);
  CHECK(missingDeclarationRejected(output));
  xide_reference_family* f=xideProjectReferencesRetainFamily(q);xide_reference_family_info info;CHECK(f && xideSemanticReferenceFamilyInfo(f,&info) && info.members==4);if(family)*family=f;else xideSemanticReferenceFamilyRelease(f);
  xvalue* take=xideProjectReferencesTake(q);CHECK(take && !xideProjectReferencesTake(q));if(kept)*kept=take;else xrtValueRelease(take);
 }
 xideProjectReferencesQueryDestroy(q);return 1;
}
static int runOccurrences(void)
{
 char* dir=xrtPathAbs("build/project-family-references/源码😀");CHECK(dir && xrtDirCreateAll(dir));const char* names[]={"a.xl","b.xl","c.xl","single.xl","dep.xl","isolated.xl","A.xlp","B.xlp","C.xlp","absent-xl.exe","helper.xl"};char* paths[11];for(size_t i=0;i<11;++i){paths[i]=xrtPathJoin(dir,names[i]);CHECK(paths[i]);}
 const char* dep="\xef\xbb\xbf" "export protocol Reader\r\n func read() as int\r\nend\r\nexport class Base\r\n virtual func read() as int\r\n return 1\r\n end\r\nend\r\n";
 const char* a="\xef\xbb\xbf" "import dep\r\nclass Derived extends dep.Base\r\n override func read() as int\r\n return 2\r\n end\r\nend\r\nfunc through(dep.Reader value) as int\r\n return value.read()\r\nend\r\nfunc main(...) as int\r\n auto derived=Derived()\r\n return through(derived)+derived.read()\r\nend\r\n";
 const char* b="import dep\nclass Other extends dep.Base\n override func read() as int\n return 3\n end\nend\nclass Wrong\n func read(int value) as int\n return value+4\n end\nend\nfunc through(dep.Reader value) as int\n return value.read()\nend\nfunc main(...) as int\n return through(Other())+Other().read()\nend\n";
 const char *plain="func main(...) as int\n return 0\nend\n",*consumer="import dep\nfunc main(...) as int\n return dep.Base().read()\nend\n";const char* bodies[]={a,b,plain,consumer,dep,consumer};for(size_t i=0;i<6;++i)CHECK(writeText(paths[i],bodies[i]));
 const char *am[]={paths[0],paths[4]},*bm[]={paths[1],paths[4]},*cm[]={paths[5]};xls_context* contexts[]={context(paths[6],dir,paths[0],am,2),context(paths[7],dir,paths[1],bm,2),context(paths[8],dir,paths[2],cm,1)};xls_context* single=context(NULL,dir,paths[3],NULL,0);CHECK(contexts[0] && contexts[1] && contexts[2] && single);
 xide_rename_scope* scope=membership(contexts,single);xide_rename_target* seed=occurrenceTarget(paths[0],a,paths[4],dep);xls_text* text=xlsTextCreate(a,strlen(a),strlen(a),NULL);CHECK(scope && seed && text);xls_analysis_document docs[]={{"entry",paths[0],text,1,1}};xide_toolchains* cache=xideToolchainsCreate();xide_project_references* store=xideProjectReferencesCreate(cache,paths[9],NULL);CHECK(cache && store);
 CHECK(occurrenceResult(xideProjectReferencesSubmitFamilyReferences(store,scope,seed,docs,1,"entry"),1,NULL,0,paths,bodies,NULL,NULL));
 CHECK(occurrenceResult(xideProjectReferencesSubmitFamilyReferences(store,scope,seed,docs,1,"entry"),1,NULL,1,paths,bodies,NULL,NULL));
 xls_context* reordered[]={contexts[2],contexts[1],contexts[0]};xide_rename_scope* other=membership(reordered,single);CHECK(other && occurrenceResult(xideProjectReferencesSubmitFamilyReferences(store,other,seed,docs,1,"entry"),1,NULL,1,paths,bodies,NULL,NULL));xideRenameScopeRelease(other);
 /* Complete declaration membership does not grant membership to an unlisted
  * importer containing a family occurrence. */
 const char* helper="import dep\nexport func invoke() as int\n return dep.Base().read()\nend\n";xstrbuf changed={0};CHECK(writeText(paths[10],helper) && xrtStrBufAppend(&changed,xrtStrView("import helper\n")) && xrtStrBufAppend(&changed,xrtStrView(b)) && writeText(paths[1],changed.Data));CHECK(occurrenceResult(xideProjectReferencesSubmitFamilyReferences(store,scope,seed,docs,1,"entry"),0,"outside explicitly configured",0,paths,bodies,NULL,NULL));xrtStrBufFree(&changed);CHECK(writeText(paths[1],b));
 CHECK(occurrenceResult(xideProjectReferencesSubmitFamilyReferences(store,scope,seed,docs,1,"entry"),1,NULL,0,paths,bodies,NULL,NULL));
 fenceEnabled=1;fenceAt=SIZE_MAX;fenceFired=0;fencePath=paths[2];fenceBody="func main(...) as int\n return 7\nend\n";CHECK(occurrenceResult(xideProjectReferencesSubmitFamilyReferences(store,scope,seed,docs,1,"entry"),0,"after declaration composition",0,paths,bodies,NULL,NULL));CHECK(fenceFired && fenceOkay);fenceEnabled=0;CHECK(writeText(paths[2],plain));
 fenceEnabled=1;fenceAt=SIZE_MAX;fenceFired=0;fencePath=NULL;fenceReached=xrtEventCreate(false,false);fenceResume=xrtEventCreate(false,false);CHECK(fenceReached && fenceResume);xide_reference_query* q=xideProjectReferencesSubmitFamilyReferences(store,scope,seed,docs,1,"entry");CHECK(q && xrtEventWaitFor(fenceReached,XIDE_XRT_TIMEOUT_MS(10000))==XWAIT_OK);xideProjectReferencesCancel(q);xrtEventSet(fenceResume);enum xide_reference_query_state state;
 for(unsigned i=0;;++i){state=xideProjectReferencesResult(q,NULL,NULL,NULL,0);if(state!=XIDE_REFERENCES_PENDING)break;if(i>=10000){CHECK(0);}xrtSleep(1);}CHECK(state==XIDE_REFERENCES_CANCELLED && !xideProjectReferencesTake(q) && !xideProjectReferencesRetainFamily(q));xideProjectReferencesQueryDestroy(q);fenceEnabled=0;xrtEventDestroy(fenceReached);xrtEventDestroy(fenceResume);fenceReached=fenceResume=NULL;
 q=xideProjectReferencesSubmitFamilyReferences(store,scope,seed,docs,1,"entry");CHECK(q);xideRenameTargetRelease(seed);xideRenameScopeRelease(scope);for(size_t i=0;i<3;++i)xlsContextRelease(contexts[i]);xlsContextRelease(single);xlsTextRelease(text);xvalue* kept=NULL;xide_reference_family* family=NULL;CHECK(occurrenceResult(q,1,NULL,0,paths,bodies,&kept,&family));xideProjectReferencesDestroy(store);xideToolchainsDestroy(cache);xide_reference_family_info info;CHECK(xrtValueCount(kept)==10 && xideSemanticReferenceFamilyInfo(family,&info) && info.members==4);xrtValueRelease(kept);xideSemanticReferenceFamilyRelease(family);
 for(size_t i=0;i<11;++i)xrtFree(paths[i]);
 xrtFree(dir);return 1;
}
int main(void){if(!runOccurrences())return 1;printf("project_family_references_test passed: %u checks; complete family IDENT rows across all scopes, unopened/standalone members, unrelated names excluded, guarded dedup, warm reuse, membership/final mutation/cancellation and detached output\n",checks);return 0;}
