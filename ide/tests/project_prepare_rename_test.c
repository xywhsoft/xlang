#include "services/project_references.h"
#include "analysis/dependency_capture.h"
#include "protocol/lsp_wire.h"
#include "platform/path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"prepare-native:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static uint64_t observeThread;static size_t dependencies,guards;
int __real_xlsDependencyCaptureValidate(xls_dependency_capture*);
int __wrap_xlsDependencyCaptureValidate(xls_dependency_capture* p)
{if(observeThread==xrtThreadCurrentId())++dependencies;return __real_xlsDependencyCaptureValidate(p);}
int __real_xideLanguageInputGuardValidate(xide_language_input_guard*,int (*)(void*),void*,xide_model_error*);
int __wrap_xideLanguageInputGuardValidate(xide_language_input_guard* p,int (*stop)(void*),void* user,xide_model_error* e)
{if(observeThread==xrtThreadCurrentId())++guards;return __real_xideLanguageInputGuardValidate(p,stop,user,e);}
static int put(xvalue* v,const char* key,xvalue* child){return child && xrtValueObjectSetNew(v,xrtStrView(key),child);}
static int writeText(const char* path,const char* body){return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)body,strlen(body)});}
static xls_context* project(const char* manifest,const char* dir,const char* entry,const char* const* members,size_t count)
{
 xls_context_spec s={.kind=XLS_CONTEXT_PROJECT,.project=manifest,.configuration="debug",.entry=entry,.directory=dir,.analysisProfile=XIDE_ANALYSIS_PROFILE,.targetKind="exe"};
 s.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,count};return xlsContextCreate(&s,16u*1024u*1024u,NULL);
}
static xide_rename_scope* scopeFor(xls_context* a,xls_context* b)
{
 xvalue *value=xrtValueObject(),*projects=xrtValueArray();int okay=value && projects;
 xvalue* row=xideAnalysisContextWrite(a);okay=okay && row && xrtValueArrayAppendNew(projects,row);
 row=xideAnalysisContextWrite(b);okay=okay && row && xrtValueArrayAppendNew(projects,row);
 okay=okay && put(value,"schema",xrtValueInt(1)) && put(value,"selectedProject",xrtValueString(xrtStrView(xlsContextSpec(a)->project))) && put(value,"packagePath",xrtValueNull());
 if(okay){okay=put(value,"projects",projects);projects=NULL;}xrtValueRelease(projects);
 xide_analysis_catalog* catalog=okay ? xideAnalysisCatalogCreate(value,NULL,0) : NULL;xrtValueRelease(value);
 xide_rename_scope* scope=catalog ? xideRenameScopeCreate(catalog,NULL,0,0,NULL,NULL,NULL) : NULL;xideAnalysisCatalogDestroy(catalog);return scope;
}
static int ready(xide_reference_query* q,xide_project_reference_stats* stats,int okay)
{
 CHECK(q);enum xide_reference_query_state state;char error[512]={0};
 for(size_t i=0;;++i){state=xideProjectReferencesResult(q,NULL,stats,error,sizeof(error));if(state!=XIDE_REFERENCES_PENDING)break;if(i==10000)CHECK(0);xrtSleep(1);}
 if(state!=(okay ? XIDE_REFERENCES_READY : XIDE_REFERENCES_FAILED))fprintf(stderr,"state=%d error=%s\n",state,error);
 CHECK(state==(okay ? XIDE_REFERENCES_READY : XIDE_REFERENCES_FAILED));return 1;
}
static int fresh(xide_reference_query* q)
{
 dependencies=guards=0;observeThread=xrtThreadCurrentId();xide_model_error e={0};int result=xideProjectReferencesPrepareRenameValidate(q,NULL,NULL,&e);observeThread=0;return result;
}
static int run(void)
{
 char* dir=xrtPathAbs("build/prepare-native/源码😀");CHECK(dir && xrtDirCreateAll(dir));
 char *primary=xrtPathJoin(dir,"primary.xl"),*member=xrtPathJoin(dir,"member.xl"),*broken=xrtPathJoin(dir,"broken.xl"),*ap=xrtPathJoin(dir,"A.xlp"),*bp=xrtPathJoin(dir,"B.xlp"),*compiler=xrtPathJoin(dir,"absent-xl.exe");
 const char *entry="func main(...) as int\n int other = 0\n return other\nend\n",*body="func main(...) as int\n int amount = 3\n return amount\nend\n",*bad="func broken(\n";
 CHECK(primary && member && broken && ap && bp && compiler && writeText(primary,entry) && writeText(member,body) && writeText(broken,bad));
 const char *am[]={primary,member},*bm[]={broken};xls_context *a=project(ap,dir,primary,am,2),*b=project(bp,dir,broken,bm,1);xide_rename_scope* scope=a && b ? scopeFor(a,b) : NULL;
 xls_text* text=xlsTextCreate(body,strlen(body),strlen(body),NULL);xls_analysis_document docs[]={{"member",member,text,41,0}};
 xide_toolchains* cache=xideToolchainsCreate();xide_project_references* store=xideProjectReferencesCreate(cache,compiler,NULL);CHECK(a && b && scope && text && cache && store);
 CHECK(!xideProjectReferencesSubmitPrepareRenameAt(store,scope,b,docs,1,"member",member,1,6));
 xide_reference_query* q=xideProjectReferencesSubmitPrepareRenameAt(store,scope,a,docs,1,"member",member,1,6);xide_project_reference_stats cold,warm;
 CHECK(ready(q,&cold,1) && cold.thread!=xrtThreadCurrentId() && cold.roots==1 && cold.captured==1 && cold.ownerProbes && !cold.memberTasks && !cold.workspaceScopes && !cold.workspaceCompleted && !cold.renameRechecks && !xideProjectReferencesRetainRenamePlan(q));
 xide_rename_target* target=xideProjectReferencesRetainPreparedRenameTarget(q);xide_rename_target_info info;
 CHECK(target && xideRenameTargetInfo(target,&info));
 fprintf(stderr,"prepared path=%s input=%s name=%.*s range=%d:%d-%d:%d\n",info.path,member,(int)info.name.Size,info.name.Data,info.start.line,info.start.character,info.end.line,info.end.character);
 CHECK(xidePathLexicalEqual(info.path,member) && info.name.Size==6 && !memcmp(info.name.Data,"amount",6) && info.start.line==1 && info.start.character==5 && info.end.character==11);
 CHECK(fresh(q) && dependencies && guards);
 xide_reference_query* again=xideProjectReferencesSubmitPrepareRenameAt(store,scope,a,docs,1,"member",member,1,6);
 CHECK(ready(again,&warm,1));
 fprintf(stderr,"cold builds=%zu hits=%zu sessions=%zu warm builds=%zu hits=%zu sessions=%zu\n",cold.keyActorBuilds,cold.keyActorHits,cold.keyActorSessions,warm.keyActorBuilds,warm.keyActorHits,warm.keyActorSessions);
 CHECK(!warm.renameRechecks && !warm.workspaceScopes && !warm.memberTasks && warm.keyActorBuilds==cold.keyActorBuilds && warm.keyActorHits>cold.keyActorHits);
 xideProjectReferencesQueryDestroy(again);
 /* Even a same-byte replacement of an unsuccessful importer probe invalidates
  * the independent preparation. The selected member's own source is unchanged. */
 CHECK(writeText(primary,entry) && !fresh(q));xideProjectReferencesQueryDestroy(q);xideRenameTargetRelease(target);
 /* Populate the original reference record for a negative route. Preparation
  * must retain its guard rather than relying on the dependency-capture path. */
 q=xideProjectReferencesSubmitPrepareRenameAt(store,scope,a,docs,1,"member",primary,1,6);CHECK(ready(q,NULL,1));xideProjectReferencesQueryDestroy(q);
 q=xideProjectReferencesSubmitPrepareRenameAt(store,scope,a,docs,1,"member",member,1,6);CHECK(ready(q,&warm,1) && warm.ownerRouteHits);
 CHECK(fresh(q) && !dependencies && guards>=2);
 target=xideProjectReferencesRetainPreparedRenameTarget(q);CHECK(target && writeText(primary,entry) && !fresh(q));xideProjectReferencesQueryDestroy(q);
 /* The preparation intentionally ignores the unrelated broken project. The
  * full rename still checks every scope and cannot produce an edit. */
 q=xideProjectReferencesSubmitRenameAt(store,scope,a,docs,1,"member",member,1,6,xrtStrView("changed"));CHECK(ready(q,NULL,0) && !xideProjectReferencesRetainRenamePlan(q));xideProjectReferencesQueryDestroy(q);
 q=xideProjectReferencesSubmitPrepareRenameAt(store,scope,a,docs,1,"member",member,0,1);CHECK(ready(q,NULL,0) && !xideProjectReferencesRetainPreparedRenameTarget(q));xideProjectReferencesQueryDestroy(q);
 xideProjectReferencesDestroy(store);xideToolchainsDestroy(cache);xlsTextRelease(text);xideRenameScopeRelease(scope);xlsContextRelease(a);xlsContextRelease(b);
 CHECK(xideRenameTargetInfo(target,&info) && info.name.Size==6 && !memcmp(info.name.Data,"amount",6));xideRenameTargetRelease(target);
 size_t size=0;unsigned char* actual=xrtFileReadAll(member,&size);CHECK(actual && size==strlen(body) && !memcmp(actual,body,size));xrtFree(actual);
 xrtFree(primary);xrtFree(member);xrtFree(broken);xrtFree(ap);xrtFree(bp);xrtFree(compiler);xrtFree(dir);return 1;
}
int main(void){if(!run())return 1;printf("PASS prepare rename native: %u checks; exact source identity/range, one provider and warm native cache, no workspace/hypothetical checks, cold/cached negative routing revalidation and detached target\n",checks);return 0;}
