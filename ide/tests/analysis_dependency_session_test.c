#include "analysis/dependency_session.h"
#include "analysis/dependency_graph.h"
#include "analysis/frontend_internal.h"
#include "analysis/input_batch.h"
#include "analysis/session_state.h"
#include <stdio.h>
#include <string.h>
static unsigned checks,semCalls,irCalls;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"dependency_session:%d:%s\n",__LINE__,#v);return 0;}}while(0)
int __real_xl6SemCheckModuleWithIndex(XL6_AstNode*,XL6_DiagList*,const char*,int);
int __wrap_xl6SemCheckModuleWithIndex(XL6_AstNode* root,XL6_DiagList* diags,const char* module,int index)
{++semCalls;return __real_xl6SemCheckModuleWithIndex(root,diags,module,index);}
int __real_xl6IrBuildModuleFromAst(XL6_IrModule*,const XL6_AstNode*,XL6_DiagList*);
int __wrap_xl6IrBuildModuleFromAst(XL6_IrModule* module,const XL6_AstNode* root,XL6_DiagList* diags)
{++irCalls;return __real_xl6IrBuildModuleFromAst(module,root,diags);}
static const char* entryBody="import library\nfunc main(...) as int\n return library.main(7)\nend\n";
static const char* libraryBody="export func main(int value) as int\n return value+1\nend\n";
static const char* libraryEdited="export func main(int value) as int\n return value+2\nend\n";
static int writeDependency(const char* path,const char* body)
{return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)body,strlen(body)});}
typedef struct fixture {char *directory,*entry,*library,*other,*project; xls_text *mainText,*libText,*editText,*otherText; xls_context *context,*libraryContext; xls_analysis_document docs[3];} fixture;
static int setup(fixture* f)
{
 char name[128];snprintf(name,sizeof(name),"dependency-inputs-%llu-%llu",(unsigned long long)xrtThreadCurrentId(),(unsigned long long)xrtClock());
 f->directory=xrtPathAbs(name);CHECK(f->directory && xrtDirCreateAll(f->directory));
 f->entry=xrtPathJoin(f->directory,"main.xl");f->library=xrtPathJoin(f->directory,"library.xl");f->other=xrtPathJoin(f->directory,"other.xl");f->project=xrtPathJoin(f->directory,"owner.xlp");CHECK(f->entry && f->library && f->other && f->project);
 CHECK(writeDependency(f->entry,entryBody) && writeDependency(f->library,libraryBody) && writeDependency(f->other,"func other() as int\n return 0\nend\n"));
 f->mainText=xlsTextCreate(entryBody,strlen(entryBody),1024*1024,NULL);f->libText=xlsTextCreate(libraryBody,strlen(libraryBody),1024*1024,NULL);f->editText=xlsTextCreate(libraryEdited,strlen(libraryEdited),1024*1024,NULL);f->otherText=xlsTextCreate("// unused\n",10,1024*1024,NULL);CHECK(f->mainText && f->libText && f->editText && f->otherText);
 f->docs[0]=(xls_analysis_document){"main",f->entry,f->mainText,1,1};f->docs[1]=(xls_analysis_document){"lib",f->library,f->libText,2,1};f->docs[2]=(xls_analysis_document){"other",f->other,f->otherText,3,1};
 const char* members[]={f->entry,f->library};xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=f->project,.configuration="debug",.entry=f->entry,.directory=f->directory,.targetKind="exe",.analysisProfile="xlang-ls/compiler-xrt-v1"};spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,2};
 f->context=xlsContextCreate(&spec,1024*1024,NULL);spec.entry=f->library;f->libraryContext=xlsContextCreate(&spec,1024*1024,NULL);CHECK(f->context && f->libraryContext);return 1;
}
static void clear(fixture* f)
{xlsContextRelease(f->context);xlsContextRelease(f->libraryContext);xlsTextRelease(f->mainText);xlsTextRelease(f->libText);xlsTextRelease(f->editText);xlsTextRelease(f->otherText);xrtFree(f->directory);xrtFree(f->entry);xrtFree(f->library);xrtFree(f->other);xrtFree(f->project);}
static int parity(xls_dependency_capture* value,xls_context* ctx,const xls_analysis_document* docs,size_t count,const char* entry)
{
 xls_semantic_input inputs[64];for(size_t i=0;i<count;++i)inputs[i]=(xls_semantic_input){docs[i].path,xlsTextData(docs[i].text),xlsTextSize(docs[i].text)};
 xls_dependency_capture* fresh=xlsDependencyCaptureCreate(ctx,NULL,entry,inputs,count,NULL,NULL,NULL,0);CHECK(fresh && value);
 xls_dependency_info a=xlsDependencyCaptureInfo(value),b=xlsDependencyCaptureInfo(fresh);CHECK(a.expanded==b.expanded && a.diagnostics==b.diagnostics && !strcmp(a.phase,b.phase));
 xls_check_inputs ai,bi;CHECK(xlsDependencyCaptureInputs(value,&ai) && xlsDependencyCaptureInputs(fresh,&bi) && ai.sources==bi.sources && ai.probes==bi.probes);
 for(size_t i=0;i<ai.sources;++i){xls_check_source x,y;CHECK(xlsDependencyCaptureSource(value,i,&x) && xlsDependencyCaptureSource(fresh,i,&y));CHECK(!strcmp(x.path,y.path) && x.kind==y.kind && x.size==y.size && x.rawSize==y.rawSize && !memcmp(x.text,y.text,x.size) && !memcmp(x.textSha256,y.textSha256,32) && !memcmp(x.rawSha256,y.rawSha256,32));}
 for(size_t i=0;i<ai.probes;++i){xls_check_probe x,y;CHECK(xlsDependencyCaptureProbe(value,i,&x) && xlsDependencyCaptureProbe(fresh,i,&y) && !strcmp(x.path,y.path) && x.exists==y.exists);}
 xlsDependencyCaptureRelease(fresh);return 1;
}
static int stopDependency(void* p){return *(int*)p;}
static int sessions(void)
{
 fixture f={0};CHECK(setup(&f));char error[256];xls_dependency_session_stats stats;
 CHECK(!xlsDependencySessionCreate(&(xls_session_limits){65,1024,1024}));
 xls_dependency_session* s=xlsDependencySessionCreate(NULL);CHECK(s);
 xls_dependency_capture* a=xlsDependencySessionCaptureInContext(s,f.context,f.docs,3,"main",NULL,NULL,error,sizeof(error));CHECK(a && xlsDependencyCaptureInfo(a).expanded && !semCalls && !irCalls);CHECK(parity(a,f.context,f.docs,3,f.entry));
 xls_dependency_capture* b=xlsDependencySessionCaptureInContext(s,f.context,f.docs,3,"main",NULL,NULL,error,sizeof(error));CHECK(b==a);xlsDependencyCaptureRelease(b);
 f.docs[2].text=f.editText;f.docs[2].revision=2;b=xlsDependencySessionCaptureInContext(s,f.context,f.docs,3,"main",NULL,NULL,error,sizeof(error));CHECK(b==a);xlsDependencyCaptureRelease(b);xlsDependencySessionStats(s,&stats);CHECK(stats.builds==1 && stats.hits==2 && stats.inputScopeHits==1 && stats.documents==3);
 /* Complete history rejects stale unused text even when it cannot affect imports. */
 f.docs[2].revision=1;CHECK(!xlsDependencySessionCaptureInContext(s,f.context,f.docs,3,"main",NULL,NULL,error,sizeof(error)) && *error);f.docs[2].revision=2;
 f.docs[1].text=f.editText;f.docs[1].revision=2;b=xlsDependencySessionCaptureInContext(s,f.context,f.docs,3,"main",NULL,NULL,error,sizeof(error));CHECK(b && b!=a && parity(b,f.context,f.docs,3,f.entry));xlsDependencyCaptureRelease(b);xlsDependencySessionStats(s,&stats);CHECK(stats.builds==2 && stats.entrySyntaxHits==1);
 /* An imported overlay's close restores real disk ownership;the negative and
    positive resolution facts are compared to a fresh original prefix. */
 xls_analysis_document one=f.docs[0];b=xlsDependencySessionCaptureInContext(s,f.context,&one,1,"main",NULL,NULL,error,sizeof(error));CHECK(b && parity(b,f.context,&one,1,f.entry));xlsDependencyCaptureRelease(b);
 CHECK(writeDependency(f.library,libraryEdited));b=xlsDependencySessionCaptureInContext(s,f.context,&one,1,"main",NULL,NULL,error,sizeof(error));CHECK(b && parity(b,f.context,&one,1,f.entry));xlsDependencyCaptureRelease(b);xlsDependencySessionStats(s,&stats);CHECK(stats.invalidations==1);
 /* Checkpoint restore is the worker's final cancellation transaction. */
 xls_semantic_checkpoint saved=xlsDependencySessionCheckpoint(s);one.revision=8;b=xlsDependencySessionCaptureInContext(s,f.context,&one,1,"main",NULL,NULL,error,sizeof(error));CHECK(b);xlsDependencyCaptureRelease(b);xlsDependencySessionRestore(s,&saved);one.revision=1;b=xlsDependencySessionCaptureInContext(s,f.context,&one,1,"main",NULL,NULL,error,sizeof(error));CHECK(b);xlsDependencyCaptureRelease(b);
 int stop=1;one.revision=9;CHECK(!xlsDependencySessionCaptureInContext(s,f.context,&one,1,"main",stopDependency,&stop,error,sizeof(error)) && strstr(error,"cancelled"));one.revision=1;b=xlsDependencySessionCaptureInContext(s,f.context,&one,1,"main",NULL,NULL,error,sizeof(error));CHECK(b);xlsDependencyCaptureRelease(b);
 const char* mainEdit="import library\nfunc main(...) as int\n return library.main(8)\nend\n";xls_text* mainEdited=xlsTextCreate(mainEdit,strlen(mainEdit),1024*1024,NULL);CHECK(mainEdited);one.text=mainEdited;one.revision=2;
 b=xlsDependencySessionCaptureInContext(s,f.context,&one,1,"main",NULL,NULL,error,sizeof(error));CHECK(b && parity(b,f.context,&one,1,f.entry));xlsDependencySessionStats(s,&stats);CHECK(stats.entryFragmentParses>0 && stats.entryReusedDeclarations>0);xlsDependencyCaptureRelease(b);xlsTextRelease(mainEdited);
 xlsDependencySessionDestroy(s);CHECK(xlsDependencyCaptureInfo(a).expanded);xlsDependencyCaptureRelease(a);
 /* Closed disk entry is opt-in;ordinary semantic batches keep their contract. */
 s=xlsDependencySessionCreate(NULL);CHECK(s);a=xlsDependencySessionCaptureInContext(s,f.context,NULL,0,NULL,NULL,NULL,error,sizeof(error));CHECK(a && xlsDependencyCaptureInfo(a).expanded && parity(a,f.context,NULL,0,f.entry));b=xlsDependencySessionCaptureInContext(s,f.context,NULL,0,NULL,NULL,NULL,error,sizeof(error));CHECK(a==b);xlsDependencyCaptureRelease(b);
 CHECK(!xlsInputBatchCreateInContext(f.context,NULL,NULL,NULL,0,NULL,NULL,NULL,error,sizeof(error)));CHECK(!xlsDependencySessionCaptureInContext(s,NULL,NULL,0,NULL,NULL,NULL,error,sizeof(error)));CHECK(!xlsDependencySessionCaptureInContext(s,f.context,NULL,0,"main",NULL,NULL,error,sizeof(error)));
 CHECK(writeDependency(f.entry,"import not_created_dependency\nfunc main(...) as int\n return 0\nend\n"));b=xlsDependencySessionCaptureInContext(s,f.context,NULL,0,NULL,NULL,NULL,error,sizeof(error));CHECK(b && !xlsDependencyCaptureInfo(b).expanded && xlsDependencyCaptureInfo(b).diagnostics);xlsDependencyCaptureRelease(b);
 char* missing=xrtPathJoin(f.directory,"not_created_dependency.xl");CHECK(missing && writeDependency(missing,libraryBody));b=xlsDependencySessionCaptureInContext(s,f.context,NULL,0,NULL,NULL,NULL,error,sizeof(error));CHECK(b && xlsDependencyCaptureInfo(b).expanded && parity(b,f.context,NULL,0,f.entry));xlsDependencyCaptureRelease(b);xrtFree(missing);
 /* Reopening uses a new identity,with its own revision stream. */
 f.docs[0].identity=99;f.docs[0].revision=0;b=xlsDependencySessionCaptureInContext(s,f.context,f.docs,3,"main",NULL,NULL,error,sizeof(error));CHECK(b && parity(b,f.context,f.docs,3,f.entry));xlsDependencyCaptureRelease(b);
 xlsDependencySessionDestroy(s);xlsDependencyCaptureRelease(a);CHECK(!semCalls && !irCalls);clear(&f);return 1;
}
int main(void){if(!sessions())return 1;printf("analysis_dependency_session_test passed: %u checks;fresh original source/probe parity,used inputs,version gates,close/reopen,disk and negative probes,closed entry,retained lifetime,checkpoint/cancel,zero observed semantic/IR builders\n",checks);return 0;}
