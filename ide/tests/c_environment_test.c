#include "services/c_commands.h"
#include "services/c_completion_job.h"
#include "platform/path.h"
#include "platform/process.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int assertions,failed; static xide_model_error error;
#define CHECK(v) do { ++assertions; if(!(v)) { fprintf(stderr,"c_environment_test:%d: %s; %s\n",__LINE__,#v,error.message); failed=1; goto cleanup; } } while(0)
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* v,const char* k) { return v ? xrtValueObjectGet(v,sv(k)) : NULL; }
static int writeText(const char* p,const char* t) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)t,strlen(t)}); }
static int record(const char* root,const char* name,const xvalue* v)
{ char* p=xidePathResolve(root,name); size_t n=0; char* t=xrtJsonStringify(v,true,&n); int ok=p && t && xrtFileWriteAtomic(p,(xbytesview){(unsigned char*)t,n}); free(p); xrtFree(t); return ok; }
static int contains(const xvalue* a,const char* s)
{ for(size_t i=0;i<xrtValueCount(a);++i) { xstrview t={0}; if(xrtValueGetString(xrtValueArrayGet(a,i),&t) && t.Size==strlen(s) && !memcmp(t.Data,s,t.Size)) return 1; } return 0; }
static int setAnalysis(xide_project* p,const char* system,const char* forced,const char* macros,int remove)
{
 xvalue* configurations=xideProjectConfigurationsJson(p); xvalue* ca=xrtJsonParse(sv(macros)); int ok=configurations && ca;
 if(ok && system) ok=xrtValueArrayAppendNew(get(ca,"systemIncludes"),xrtValueString(sv(system)));
 if(ok && forced) ok=xrtValueArrayAppendNew(get(ca,"forcedIncludes"),xrtValueString(sv(forced)));
 if(ok) ok=remove ? xrtValueObjectRemove(xrtValueArrayGet(configurations,0),sv("cAnalysis")) : xrtValueObjectSet(xrtValueArrayGet(configurations,0),sv("cAnalysis"),ca);
 if(ok) ok=xideProjectApplyConfigurations(p,configurations,p->activeConfiguration,&error);
 xrtValueRelease(ca); xrtValueRelease(configurations); return ok;
}
static int diagnostics(xide_c_completion_job* job,uint64_t id,const char* root,const char* name,int* count)
{
 uint64_t sequence=0,until=GetTickCount64()+30000; xide_c_event events[64]={0}; size_t n=0; int found=0;
 do {
  if(xideCAnalysisJobDiagnostics(job,&sequence,events,64,&n)) for(size_t i=0;i<n;++i) if(events[i].documentId==id) { *count=(int)xrtValueCount(get(get(events[i].message,"params"),"diagnostics")); found=record(root,name,events[i].message); break; }
  for(size_t i=0;i<n;++i) xideCEventClear(&events[i]);
  n=0;
  if(found || xideCCompletionJobState(job,error.message,sizeof(error.message))>=XIDE_C_JOB_FAILED) break;
  Sleep(5);
 } while(GetTickCount64()<until); return found;
}
static xvalue* query(xide_c_completion_job* job,uint64_t id,int offset)
{
 uint64_t until=GetTickCount64()+30000,token=xideCAnalysisJobRequest(job,XIDE_C_HOVER,id,offset); xide_c_event event={0}; xvalue* result=NULL;
 while(token && GetTickCount64()<until) {
  if(xideCAnalysisJobTake(job,token,&event)) { result=xrtValueDeepClone(event.message); break; }
  if(xideCCompletionJobState(job,error.message,sizeof(error.message))>=XIDE_C_JOB_FAILED) break;
  Sleep(5);
 } xideCEventClear(&event); return result;
}
static int run(const char* program,const char* directory,const char* log,const char* const* args,size_t count)
{
 xide_process_spec spec={.program=program,.directory=directory,.logPath=log,.arguments=args,.argumentCount=count}; xide_process_info info={0};
 xide_process* p=xideProcessStart(&spec,error.message,sizeof(error.message)); if(!p) return 0;
 uint64_t until=GetTickCount64()+30000; char chunk[8192];
 do { while(xideProcessRead(p,chunk,sizeof(chunk))) {} xideProcessGetInfo(p,&info); if(info.state==XIDE_PROCESS_FINISHED) break;
  Sleep(5); } while(GetTickCount64()<until);
 int ok=info.state==XIDE_PROCESS_FINISHED && info.started && !info.ioError && !info.droppedBytes && info.exit.Kind==XPROCESS_EXIT_CODE && !info.exit.Code;
 if(!ok) snprintf(error.message,sizeof(error.message),"Process failed: exit=%d; %.350s; log %.100s",info.exit.Code,info.error,log);
 xideProcessDestroy(p); return ok;
}
int main(int argc,char** argv)
{
 xide_project p={0},copy={0}; xvalue *commands=NULL,*value=NULL,*before=NULL,*after=NULL; xide_c_completion_job* job=NULL; char* owned[24]={0}; size_t ownedCount=0; int count=0;
 #define OWN(v) (owned[ownedCount++]=(v))
 CHECK(argc>=2 && argc<=4); char unique[96]; snprintf(unique,sizeof(unique),"c-environment-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64());
 char *root=OWN(xidePathResolve(argv[1],unique)),*system=OWN(xidePathResolve(root,"系统 头😀")),*forced=OWN(xidePathResolve(root,"强制 声明.h")),*types=OWN(xidePathResolve(system,"bridge-types.h")),*manifest=OWN(xidePathResolve(root,"app.xlp")),*entry=OWN(xidePathResolve(root,"main.xl")),*c=OWN(xidePathResolve(root,"native.c")),*copyPath=OWN(xidePathResolve(root,"copy/app.xlp"));
 const char *prelude="#include <bridge-types.h>\n",*source="#ifdef _CRT_SECURE_NO_WARNINGS\n#error undefine did not apply\n#endif\n#ifndef C_ANALYSIS_ON\n#error define did not apply\n#endif\nBridgeValue native_answer(void) { return C_ANSWER; }\n";
 const char* analysis="{\"systemIncludes\":[],\"forcedIncludes\":[],\"defines\":[\"C_ANALYSIS_ON=1\",\"C_ANSWER=42\"],\"undefines\":[\"_CRT_SECURE_NO_WARNINGS\"],\"future\":{\"keep\":7}}";
 CHECK(root && system && forced && types && manifest && entry && c && copyPath && xrtDirCreateAll(system));
 CHECK(writeText(types,"typedef int BridgeValue;\n") && writeText(forced,prelude) && writeText(c,source) && writeText(entry,"func main(...) as int\n return 0\nend\n"));
 CHECK(xideProjectCreate(&p,manifest,"C analysis",entry,&error) && xideProjectAddFile(&p,c,"c",&error) && setAnalysis(&p,system,forced,analysis,0));
 CHECK(p.configurations[0].cAnalysis.systemIncludes.count==1 && p.configurations[0].cAnalysis.forcedIncludes.count==1 && p.configurations[0].cAnalysis.defines.count==2);
 commands=xideCCommandsCreate(&p,NULL,NULL,&error); CHECK(commands && xrtValueCount(commands)==1 && record(root,"compile-commands.json",commands));
 const xvalue* args=get(xrtValueArrayGet(commands,0),"arguments");
 CHECK(contains(args,"-isystem") && contains(args,system) && contains(args,"-include") && contains(args,forced) && contains(args,"-D") && contains(args,"C_ANSWER=42") && contains(args,"-U") && contains(args,"_CRT_SECURE_NO_WARNINGS"));
 CHECK(xideCAnalysisMacroValid("VALUE=\"中文 = value\"",1) && xideCAnalysisMacroValid("EMPTY=",1) && !xideCAnalysisMacroValid("FUNC(x)=x",1) && !xideCAnalysisMacroValid("BAD=value",0) && !xideCAnalysisMacroValid("NAME\n",1) && !xideCAnalysisMacroValid("9BAD",1));
 before=xideProjectConfigurationsJson(&p); CHECK(before);
 CHECK(!setAnalysis(&p,NULL,NULL,"{\"defines\":[\"BAD-NAME=1\"]}",0) && error.code==XIDE_MODEL_INVALID);
 after=xideProjectConfigurationsJson(&p); CHECK(after && xrtValueEqual(before,after)); xrtValueRelease(after); after=NULL;
 CHECK(!setAnalysis(&p,NULL,NULL,"{\"systemIncludes\":false}",0));
 after=xideProjectConfigurationsJson(&p); CHECK(after && xrtValueEqual(before,after)); xrtValueRelease(after); after=NULL;
 value=xrtValueDeepClone(before); CHECK(value);
 xvalue* invalid=get(xrtValueArrayGet(value,0),"cAnalysis"),*oversized=xrtValueArray(); CHECK(invalid && oversized);
 for(int i=0;i<129;++i) CHECK(xrtValueArrayAppendNew(oversized,xrtValueString(sv("LIMIT_TEST=1"))));
 CHECK(xrtValueObjectSetNew(invalid,sv("defines"),oversized) && !xideProjectApplyConfigurations(&p,value,p.activeConfiguration,&error) && error.code==XIDE_MODEL_INVALID);
 xrtValueRelease(value); value=NULL; after=xideProjectConfigurationsJson(&p); CHECK(after && xrtValueEqual(before,after)); xrtValueRelease(after); after=NULL;
 /* All four per-list limits may fit the model yet exceed one CDB command. */
 value=xrtValueDeepClone(before); CHECK(value); invalid=get(xrtValueArrayGet(value,0),"cAnalysis");
 const char* keys[]={"systemIncludes","forcedIncludes","defines","undefines"};
 for(int k=0;k<4;++k) {
  xvalue* many=xrtValueArray(); CHECK(many); for(int i=0;i<128;++i) { char item[64]; snprintf(item,sizeof(item),k<2 ? "capacity/%d" : "MACRO_%d",i); CHECK(xrtValueArrayAppendNew(many,xrtValueString(sv(item)))); }
  CHECK(xrtValueObjectSetNew(invalid,sv(keys[k]),many));
 }
 CHECK(xideProjectApplyConfigurations(&p,value,p.activeConfiguration,&error) && !xideCCommandsCreate(&p,NULL,NULL,&error));
 CHECK(xideProjectApplyConfigurations(&p,before,p.activeConfiguration,&error)); xrtValueRelease(value); value=NULL;
 CHECK(xideProjectSave(&p,NULL,&error)); char* directory=OWN(xidePathDirectory(copyPath)); CHECK(directory && xrtDirCreateAll(directory)); value=xideProjectExport(&p,&error); CHECK(value && xideProjectDecode(&copy,manifest,value,&error)); xrtValueRelease(value); value=NULL;
 CHECK(xideProjectSave(&copy,copyPath,&error) && xideProjectLoad(&copy,copyPath,&error) && xidePathLexicalEqual(copy.configurations[0].cAnalysis.systemIncludes.items[0],system) && xidePathLexicalEqual(copy.configurations[0].cAnalysis.forcedIncludes.items[0],forced));
 after=xideProjectConfigurationsJson(&copy); CHECK(after && get(get(xrtValueArrayGet(after,0),"cAnalysis"),"future")); xrtValueRelease(after); after=NULL;
 if(argc>=3) {
  xide_c_document doc={1,c,source,9}; xide_c_job_spec spec={.server=argv[2],.directory=root,.commands=commands,.documents=&doc,.count=1,.documentId=1};
  job=xideCAnalysisJobStart(&spec,&error); CHECK(job && diagnostics(job,1,root,"configured-diagnostics.json",&count) && count==0);
  int offset=(int)(strstr(source,"C_ANSWER")-source)+2; value=query(job,1,offset); CHECK(value && record(root,"configured-hover.json",value));
  xstrview text={0}; CHECK(xrtValueGetString(get(get(get(value,"result"),"contents"),"value"),&text) && strstr(text.Data,"42")); xrtValueRelease(value); value=NULL;
  CHECK(writeText(forced,"#include <bridge-types.h>\n#undef C_ANSWER\n#define C_ANSWER 91\n"));
  uint64_t until=GetTickCount64()+6000; while(xideCCompletionJobState(job,NULL,0)==XIDE_C_JOB_READY && GetTickCount64()<until) Sleep(5);
  CHECK(xideCCompletionJobState(job,NULL,0)==XIDE_C_JOB_INVALIDATED); xideCCompletionJobDestroy(job); job=NULL;
  const char* changed="#ifdef _CRT_SECURE_NO_WARNINGS\n#error undefine did not apply\n#endif\nBridgeValue native_answer(void) { return C_ANSWER; }\n_Static_assert(C_ANSWER == 91, \"forced header redefine missing\");\n";
  doc.text=changed; doc.version=10; offset=(int)(strstr(changed,"C_ANSWER")-changed)+2;
  job=xideCAnalysisJobStart(&spec,&error); CHECK(job && diagnostics(job,1,root,"changed-prelude-diagnostics.json",&count) && count==0);
  value=query(job,1,offset); CHECK(value && record(root,"changed-prelude-hover.json",value) && xrtValueGetString(get(get(get(value,"result"),"contents"),"value"),&text) && strstr(text.Data,"91")); xrtValueRelease(value); value=NULL; xideCCompletionJobDestroy(job); job=NULL;
  CHECK(writeText(forced,prelude));
  /* The forced header is unsaved and supplied through the same real overlay. */
  CHECK(xideProjectAddFile(&p,forced,"header",&error)); xrtValueRelease(commands); commands=xideCCommandsCreate(&p,NULL,NULL,&error); CHECK(commands);
  const char* unsaved="BridgeValue native_answer(void) { return C_ANSWER; }\n_Static_assert(C_ANSWER == 73, \"unsaved forced header redefine missing\");\n"; offset=(int)(strstr(unsaved,"C_ANSWER")-unsaved)+2;
  xide_c_document docs[]={{1,c,unsaved,11},{2,forced,"#include <bridge-types.h>\n#undef C_ANSWER\n#define C_ANSWER 73\n",10}}; spec.commands=commands; spec.documents=docs; spec.count=2;
  job=xideCAnalysisJobStart(&spec,&error); CHECK(job && diagnostics(job,1,root,"unsaved-prelude-diagnostics.json",&count) && count==0);
  value=query(job,1,offset); CHECK(value && record(root,"unsaved-prelude-hover.json",value) && xrtValueGetString(get(get(get(value,"result"),"contents"),"value"),&text) && strstr(text.Data,"73")); xrtValueRelease(value); value=NULL; xideCCompletionJobDestroy(job); job=NULL;
  size_t n=0; unsigned char* bytes=xrtFileReadAllLimit(forced,4096,&n); int unchanged=bytes && n==strlen(prelude) && !memcmp(bytes,prelude,n); xrtFree(bytes); CHECK(unchanged);
 }
 if(argc==4) {
  /* Exact compiler-generated C ABI declarations; no inferred prototypes. */
  const char* xl="export \"xl_add\" func add(c_int left, c_int right) as c_int\n return left + right\nend\nextern \"native_answer\" func nativeAnswer() as c_int\nfunc main(...) as int\n if nativeAnswer() != 42 then\n  return 1\n end\n return 0\nend\n";
  const char* native="int native_answer(void) { return xl_add(19,23); }\n";
  char *module=OWN(xidePathResolve(root,"module.h")),*output=OWN(xidePathResolve(root,"mixed.exe")),*compilerDir=OWN(xidePathDirectory(argv[3])),*runtime=OWN(xidePathResolve(compilerDir,"include")),*log=OWN(xidePathResolve(root,"compiler.log"));
  CHECK(module && output && compilerDir && runtime && log && writeText(entry,xl) && writeText(c,native));
  const char* generate[]={"cgen",entry,"--emit-header",module}; CHECK(run(argv[3],root,log,generate,4) && xidePathExists(module));
  CHECK(xideProjectAddFile(&p,module,"header",&error));
  CHECK(setAnalysis(&p,NULL,NULL,"{}",1));
  xrtValueRelease(commands); commands=xideCCommandsCreate(&p,NULL,NULL,&error); CHECK(commands);
  xide_c_document missingDoc={1,c,native,11}; xide_c_job_spec missingSpec={.server=argv[2],.directory=root,.commands=commands,.documents=&missingDoc,.count=1,.documentId=1};
  job=xideCAnalysisJobStart(&missingSpec,&error); CHECK(job && diagnostics(job,1,root,"real-bridge-missing-diagnostics.json",&count) && count>0); xideCCompletionJobDestroy(job); job=NULL;
  CHECK(setAnalysis(&p,runtime,forced,"{\"systemIncludes\":[],\"forcedIncludes\":[],\"defines\":[],\"undefines\":[]}",0));
  CHECK(writeText(forced,"#include <xruntime_decl.h>\n#include \"module.h\"\n") && xideProjectSave(&p,NULL,&error));
  xrtValueRelease(commands); commands=xideCCommandsCreate(&p,NULL,NULL,&error); CHECK(commands && record(root,"real-bridge-commands.json",commands));
  xide_c_document doc={1,c,native,11}; xide_c_job_spec spec={.server=argv[2],.directory=root,.commands=commands,.documents=&doc,.count=1,.documentId=1};
  job=xideCAnalysisJobStart(&spec,&error); CHECK(job && diagnostics(job,1,root,"real-bridge-diagnostics.json",&count) && count==0);
  value=query(job,1,(int)(strstr(native,"xl_add")-native)+2); CHECK(value && record(root,"real-bridge-hover.json",value)); xstrview text={0};
  CHECK(xrtValueGetString(get(get(get(value,"result"),"contents"),"value"),&text) && strstr(text.Data,"xl_add") && strstr(text.Data,"int")); xrtValueRelease(value); value=NULL; xideCCompletionJobDestroy(job); job=NULL;
  const char* build[]={"build",manifest,"-o",output}; CHECK(run(argv[3],root,log,build,4) && xidePathExists(output) && run(output,root,log,NULL,0));
 }
 printf("c_environment_test passed: %d assertions; active C inputs, validation transaction, unknown fields, save-as paths%s%s; evidence %s\n",assertions,argc>=3 ? ", real clangd macros/prelude/unsaved overlay and invalidation" : "; real C provider gate omitted",argc==4 ? ", exact xlang C ABI header and real mixed build/run" : "",root);
cleanup:
 xideCCompletionJobDestroy(job); xrtValueRelease(commands); xrtValueRelease(value); xrtValueRelease(before); xrtValueRelease(after); xideProjectClear(&p); xideProjectClear(&copy);
 for(size_t i=0;i<ownedCount;++i) free(owned[i]);
 return failed;
}

