#include "services/c_input_trace.h"
#include "services/c_commands.h"
#include "services/c_completion_job.h"
#include "platform/path.h"
#include "../../tools/clangd_input_trace/NativeInput.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int assertions,failed; static xide_model_error error;
#define CHECK(v) do { ++assertions; if(!(v)) { fprintf(stderr,"c_input_trace_test:%d: %s; %s\n",__LINE__,#v,error.message); failed=1; goto cleanup; } } while(0)
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* v,const char* k) { return v ? xrtValueObjectGet(v,sv(k)) : NULL; }
static int put(xvalue* o,const char* k,xvalue* v) { return xrtValueObjectSetNew(o,sv(k),v); }
static int text(const char* p,const char* s) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static wchar_t* wide(const char* s) { int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,NULL,0); wchar_t* p=n ? malloc((size_t)n*sizeof(*p)) : NULL; if(p) MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,p,n); return p; }
static int number(xvalue* o,const char* k,uint64_t n) { char s[32]; snprintf(s,sizeof(s),"%llu",(unsigned long long)n); return put(o,k,xrtValueString(sv(s))); }
static xvalue* fileRow(const char* path,int hash) {
 xide_file_snapshot s={0}; if(!xideFileSnapshotInspect(path,&s,&error)) return NULL; char digest[65]; for(int i=0;i<32;++i) snprintf(digest+i*2,3,"%02x",s.sha256[i]);
 xvalue* r=xrtValueObject(); int ok=r && put(r,"path",xrtValueString(sv(path))) && put(r,"kind",xrtValueString(sv("file"))) && number(r,"index",s.index) && number(r,"volume",s.volume) && number(r,"size",s.length) && number(r,"modified100ns",s.lastWrite) && number(r,"attributes",s.attributes) && (!hash || put(r,"sha256",xrtValueString(sv(digest))));
 if(!ok) { xrtValueRelease(r); return NULL; } return r;
}
static xvalue* message(xvalue* row,uint64_t sequence,int complete) {
 xvalue *m=xrtValueObject(),*p=xrtValueObject(),*rows=xrtValueArray(); int ok=m && p && rows && (!row || xrtValueArrayAppend(rows,row)) && put(m,"jsonrpc",xrtValueString(sv("2.0"))) && put(m,"method",xrtValueString(sv("xlang/inputTrace"))) && put(p,"version",xrtValueInt(1)) && number(p,"sequence",sequence) && number(p,"previous",sequence-1) && put(p,"stable",xrtValueBool(true)) && put(p,"complete",xrtValueBool(complete!=0)) && put(p,"error",xrtValueString(sv(""))) && xrtValueObjectSet(p,sv("records"),rows) && xrtValueObjectSet(m,sv("params"),p);
 xrtValueRelease(p); xrtValueRelease(rows); if(!ok) { xrtValueRelease(m); return NULL; } return m;
}
static int save(const char* root,const char* name,const xvalue* value) { char* directory=xidePathResolve(root,"evidence"); char* path=directory ? xidePathResolve(directory,name) : NULL; size_t n=0; char* bytes=xrtJsonStringify(value,true,&n); int ok=directory && xrtDirCreateAll(directory) && path && bytes && xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)bytes,n}); free(directory); free(path); xrtFree(bytes); return ok; }
static int cancelled(void* user) { return *(int*)user; }
static int waitJob(xide_c_completion_job* job,enum xide_c_job_state wanted) { uint64_t until=GetTickCount64()+30000; do { enum xide_c_job_state state=xideCCompletionJobState(job,error.message,sizeof(error.message)); if(state==wanted) return 1; if(state>=XIDE_C_JOB_FAILED) return 0; Sleep(10); } while(GetTickCount64()<until); return 0; }
static int completionType(const xvalue* reply,const char* name,const char* type) { const xvalue* result=get(reply,"result"),*items=xrtValueType(result)==XVALUE_ARRAY ? result : get(result,"items"); for(size_t i=0;i<xrtValueCount(items);++i) { const xvalue* item=xrtValueArrayGet(items,i); xstrview label={0},detail={0}; if(xrtValueGetString(get(item,"label"),&label) && !strcmp(label.Data,name) && xrtValueGetString(get(item,"detail"),&detail) && strstr(detail.Data,type)) return 1; } return 0; }
static xvalue* query(xide_c_completion_job* job,uint64_t id,int offset) { uint64_t token=xideCAnalysisJobRequest(job,XIDE_C_HOVER,id,offset),until=GetTickCount64()+30000; xide_c_event event={0}; xvalue* result=NULL; while(token && GetTickCount64()<until) { if(xideCAnalysisJobTake(job,token,&event)) { result=xrtValueDeepClone(event.message); break; } if(xideCCompletionJobState(job,error.message,sizeof(error.message))>=XIDE_C_JOB_FAILED) break; Sleep(5); } xideCEventClear(&event); return result; }
static int observeMissing(const xide_c_job_spec* spec,const char* missing) {
 xide_c_session* session=xideCSessionStart(spec->server,spec->directory,spec->commands,spec->documents,spec->count,&error);
 xide_c_input_trace* trace=xideCInputTraceCreate(); xvalue* observations=xrtValueArray(); xide_c_event event={0}; int request=0,found=0,replied=0,okay=session && trace && observations; uint64_t until=GetTickCount64()+30000;
 while(okay && !replied && GetTickCount64()<until) {
  if(xideCSessionNext(session,&event)) {
   if(event.kind==XIDE_C_INPUT_TRACE) {
    okay=xideCInputTraceApply(trace,event.message,NULL,NULL,&error) && xrtValueArrayAppend(observations,event.message);
    const xvalue* rows=get(get(event.message,"params"),"records");
    for(size_t i=0;i<xrtValueCount(rows);++i) { const xvalue* row=xrtValueArrayGet(rows,i); xstrview path={0},kind={0}; if(xrtValueGetString(get(row,"path"),&path) && xrtValueGetString(get(row,"kind"),&kind) && !strcmp(kind.Data,"missing") && xidePathLexicalEqual(path.Data,missing)) found=1; }
   } else if(event.kind==XIDE_C_INITIALIZED) { okay=xideCSessionTracing(session); if(okay) { request=xideCSessionRequest(session,XIDE_C_HOVER,spec->documentId,spec->offset,&error); okay=request>1; } }
   else if(event.kind==XIDE_C_RESULT && event.requestId==request) { xstrview content={0}; okay=xrtValueGetString(get(get(get(event.message,"result"),"contents"),"value"),&content) && strstr(content.Data,"double"); replied=1; }
   xideCEventClear(&event);
  } else if(xideCSessionState(session,error.message,sizeof(error.message))>=XIDE_C_FAILED) okay=0;
  if(!replied) Sleep(5);
 }
 okay=okay && replied && found && xideCInputTraceComplete(trace) && xideCInputTraceCheck(trace,NULL,NULL,&error) && save(spec->directory,"real-missing-probe-observations.json",observations);
 xideCEventClear(&event); xideCSessionDestroy(session); xideCInputTraceDestroy(trace); xrtValueRelease(observations); return okay;
}
static int hoverType(xide_c_completion_job* job,const xide_c_job_spec* spec,const char* root,const char* name,const char* type) {
 xvalue* value=query(job,spec->documentId,spec->offset); xstrview content={0}; int okay=value && xrtValueGetString(get(get(get(value,"result"),"contents"),"value"),&content) && strstr(content.Data,type) && save(root,name,value); xrtValueRelease(value); return okay;
}
static int searchMatrix(const xide_c_job_spec* original,const char* root) {
 int good=0; xide_c_completion_job* job=NULL; xvalue* commands=xrtValueDeepClone(original->commands);
 char *early=xidePathResolve(root,"first 空间😀"),*late=xidePathResolve(root,"second"),*earlyHeader=NULL,*lateHeader=NULL,*nextEarly=NULL,*nextLate=NULL,*nested=NULL,*response=NULL,*resource=NULL;
 xide_c_document document=original->documents[0]; xide_c_job_spec spec=*original; spec.documents=&document; spec.commands=commands; xide_file_snapshot stamp={0};
 CHECK(commands && early && late && xrtDirCreateAll(early) && xrtDirCreateAll(late)); earlyHeader=xidePathResolve(early,"choice.h"); lateHeader=xidePathResolve(late,"choice.h"); nextEarly=xidePathResolve(early,"chain.h"); nextLate=xidePathResolve(late,"chain.h"); nested=xidePathResolve(root,"nested 空间😀.rsp"); response=xidePathResolve(root,"outer-response.rsp"); CHECK(earlyHeader && lateHeader && nextEarly && nextLate && nested && response);
 xvalue* arguments=get(xrtValueArrayGet(commands,0),"arguments"); CHECK(xrtValueArrayAppendNew(arguments,xrtValueString(sv("-I"))) && xrtValueArrayAppendNew(arguments,xrtValueString(sv(early))) && xrtValueArrayAppendNew(arguments,xrtValueString(sv("-I"))) && xrtValueArrayAppendNew(arguments,xrtValueString(sv(late))));
 const char* source="#include <choice.h>\nSEARCH_TYPE probe(SEARCH_TYPE input) { return input; }\n"; document.text=source; document.version=100; spec.offset=(int)(strstr(source,"input;")-source);
 CHECK(text(document.path,source) && text(lateHeader,"#define SEARCH_TYPE double\n"));
 job=xideCAnalysisJobStart(&spec,&error); CHECK(job && waitJob(job,XIDE_C_JOB_READY) && hoverType(job,&spec,root,"real-search-fallback.json","double")); CHECK(xideCAnalysisJobGuard(job,lateHeader,&stamp));
 CHECK(text(earlyHeader,"#define SEARCH_TYPE int\n") && waitJob(job,XIDE_C_JOB_INVALIDATED)); xideCCompletionJobDestroy(job); job=NULL;
 job=xideCAnalysisJobStart(&spec,&error); CHECK(job && waitJob(job,XIDE_C_JOB_READY) && hoverType(job,&spec,root,"real-search-shadowed.json","int")); CHECK(xideCAnalysisJobGuard(job,earlyHeader,&stamp)); xideCCompletionJobDestroy(job); job=NULL;
 source="#include <chain.h>\nCHAIN_TYPE probe(CHAIN_TYPE input) { return input; }\n"; document.text=source; document.version=101; spec.offset=(int)(strstr(source,"input;")-source);
 CHECK(text(document.path,source) && text(nextEarly,"#include_next <chain.h>\n") && text(nextLate,"#define CHAIN_TYPE double\n"));
 job=xideCAnalysisJobStart(&spec,&error); CHECK(job && waitJob(job,XIDE_C_JOB_READY) && hoverType(job,&spec,root,"real-include-next.json","double")); CHECK(xideCAnalysisJobGuard(job,nextEarly,&stamp) && xideCAnalysisJobGuard(job,nextLate,&stamp));
 CHECK(text(nextLate,"#define CHAIN_TYPE int\n") && waitJob(job,XIDE_C_JOB_INVALIDATED)); xideCCompletionJobDestroy(job); job=NULL;
 job=xideCAnalysisJobStart(&spec,&error); CHECK(job && waitJob(job,XIDE_C_JOB_READY) && hoverType(job,&spec,root,"real-rebuilt-include-next.json","int")); xideCCompletionJobDestroy(job); job=NULL;
 source="NESTED_TYPE probe(NESTED_TYPE input) { return input; }\n"; document.text=source; document.version=102; spec.offset=(int)(strstr(source,"input;")-source); CHECK(text(document.path,source) && text(nested,"-DNESTED_TYPE=double\n"));
 char arg[32770]; snprintf(arg,sizeof(arg),"@\"%s\"\n",nested); CHECK(text(response,arg)); snprintf(arg,sizeof(arg),"@%s",response); CHECK(xrtValueArrayAppendNew(arguments,xrtValueString(sv(arg))));
 job=xideCAnalysisJobStart(&spec,&error); CHECK(job && waitJob(job,XIDE_C_JOB_READY) && hoverType(job,&spec,root,"real-nested-response.json","double")); CHECK(xideCAnalysisJobGuard(job,response,&stamp) && xideCAnalysisJobGuard(job,nested,&stamp));
 CHECK(text(nested,"-DNESTED_TYPE=int\n") && waitJob(job,XIDE_C_JOB_INVALIDATED)); xideCCompletionJobDestroy(job); job=NULL;
 job=xideCAnalysisJobStart(&spec,&error); CHECK(job && waitJob(job,XIDE_C_JOB_READY) && hoverType(job,&spec,root,"real-rebuilt-nested-response.json","int")); xideCCompletionJobDestroy(job); job=NULL;
 source="#include <stddef.h>\nsize_t probe(size_t input) { return input; }\n"; document.text=source; document.version=103; spec.offset=(int)(strstr(source,"input;")-source); CHECK(text(document.path,source));
 char* bin=xidePathDirectory(spec.server); resource=bin ? xidePathResolve(bin,"../lib/clang/23/include/stddef.h") : NULL; free(bin); CHECK(resource);
 job=xideCAnalysisJobStart(&spec,&error); CHECK(job && waitJob(job,XIDE_C_JOB_READY) && hoverType(job,&spec,root,"real-builtin-header.json","size_t")); CHECK(xideCAnalysisJobGuard(job,resource,&stamp)); good=1;
cleanup:
 xideCCompletionJobDestroy(job); xrtValueRelease(commands); free(early); free(late); free(earlyHeader); free(lateHeader); free(nextEarly); free(nextLate); free(nested); free(response); free(resource); return good;
}
int main(int argc,char** argv) {
 char *root=NULL,*leaf=NULL,*missing=NULL,*replacement=NULL,*entry=NULL,*outer=NULL,*xl=NULL,*manifest=NULL; xide_c_input_trace* trace=NULL; xvalue *row=NULL,*wire=NULL,*bad=NULL,*commands=NULL,*reply=NULL; xide_c_completion_job* job=NULL; xide_project project={0}; void* handle=NULL; char* bytes=NULL; size_t size=0; unsigned long code=0; xlang_native_stamp native={0}; xide_file_snapshot snapshot={0};
 CHECK(argc>=2); char unique[100]; snprintf(unique,sizeof(unique),"trace-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64()); root=xidePathResolve(argv[1],unique); leaf=xidePathResolve(root,"间接 空间😀.h"); missing=xidePathResolve(root,"later.h"); replacement=xidePathResolve(root,"replacement.h"); CHECK(root && leaf && missing && replacement && xrtDirCreateAll(root) && text(leaf,"#define TYPE double\n"));
 CHECK(xlangNativeOpen(leaf,1,&handle,&native,&code)); CHECK(xideFileSnapshotInspect(leaf,&snapshot,&error) && native.index==snapshot.index && native.volume==snapshot.volume && native.modified==snapshot.lastWrite && native.size==snapshot.length);
 CHECK(xlangNativeRead(handle,&bytes,&size,&native,&code) && size==20 && !memcmp(bytes,"#define TYPE double\n",20)); xlangNativeFree(bytes); bytes=NULL;
 CHECK(text(replacement,"#define TYPE int\n")); wchar_t* old=wide(leaf); wchar_t* next=wide(replacement); int moved=old && next && ReplaceFileW(old,next,NULL,REPLACEFILE_IGNORE_MERGE_ERRORS,NULL,NULL); DWORD moveError=GetLastError(); free(old); free(next); if(!moved) snprintf(error.message,sizeof(error.message),"Atomic replacement failed: Windows %lu; %s",moveError,leaf); CHECK(moved);
 CHECK(xlangNativeRead(handle,&bytes,&size,&native,&code) && size==20 && !memcmp(bytes,"#define TYPE double\n",20)); xlangNativeFree(bytes); bytes=NULL; xlangNativeClose(handle); handle=NULL;
 CHECK(xideFileSnapshotInspect(leaf,&snapshot,&error) && native.index!=snapshot.index);
 row=fileRow(leaf,0); wire=message(row,1,0); trace=xideCInputTraceCreate(); CHECK(row && wire && trace && xideCInputTraceApply(trace,wire,NULL,NULL,&error)); CHECK(!xideCInputTraceComplete(trace) && !xideCInputTraceSnapshot(trace,leaf,&snapshot)); xrtValueRelease(wire); xrtValueRelease(row); row=fileRow(leaf,1); wire=message(row,2,1); CHECK(wire && xideCInputTraceApply(trace,wire,NULL,NULL,&error) && xideCInputTraceComplete(trace) && xideCInputTraceSnapshot(trace,leaf,&snapshot)); CHECK(xideCInputTraceCheck(trace,NULL,NULL,&error) && save(root,"component-observation.json",wire));
 CHECK(text(leaf,"#define TYPE double\n") && !xideCInputTraceCheck(trace,NULL,NULL,&error)); xideCInputTraceDestroy(trace); trace=NULL; xrtValueRelease(wire); wire=NULL; xrtValueRelease(row); row=NULL;
 row=xrtValueObject(); CHECK(row && put(row,"path",xrtValueString(sv(missing))) && put(row,"kind",xrtValueString(sv("missing")))); wire=message(row,1,1); trace=xideCInputTraceCreate(); CHECK(wire && trace && xideCInputTraceApply(trace,wire,NULL,NULL,&error) && xideCInputTraceCheck(trace,NULL,NULL,&error)); CHECK(text(missing,"#define LATE 1\n") && !xideCInputTraceCheck(trace,NULL,NULL,&error)); xideCInputTraceDestroy(trace); trace=NULL; xrtValueRelease(wire); wire=NULL; xrtValueRelease(row); row=NULL;
 row=fileRow(leaf,1); wire=message(row,1,1); CHECK(row && wire); for(int variant=0;variant<12;++variant) {
  bad=xrtValueDeepClone(wire); trace=xideCInputTraceCreate(); CHECK(bad && trace); xvalue* p=get(bad,"params"),*r=xrtValueArrayGet(get(p,"records"),0);
  if(variant==0) CHECK(put(p,"version",xrtValueInt(2)));
  if(variant==1) CHECK(number(p,"sequence",2));
  if(variant==2) CHECK(put(p,"stable",xrtValueBool(false)));
  if(variant==3) CHECK(put(r,"path",xrtValueString(sv("relative.h"))));
  if(variant==4) CHECK(put(r,"sha256",xrtValueString(sv("bad"))));
  if(variant==5) CHECK(put(r,"index",xrtValueString(sv("18446744073709551616"))));
  if(variant==6) CHECK(number(r,"index",UINT64_MAX));
  if(variant==7) CHECK(put(r,"kind",xrtValueString(sv("directory"))));
  if(variant==8) CHECK(put(r,"sha256",xrtValueString(sv("0000000000000000000000000000000000000000000000000000000000000000"))));
  if(variant==9) CHECK(put(p,"error",xrtValueString(sv("Observation failed."))));
  if(variant==10) CHECK(put(p,"error",xrtValueInt(0)));
  if(variant==11) { size_t length=270000; char* extra=malloc(length+1); CHECK(extra); memset(extra,'x',length); extra[length]=0; int enlarged=put(r,"extra",xrtValueString(sv(extra))); free(extra); CHECK(enlarged); }
  CHECK(!xideCInputTraceApply(trace,bad,NULL,NULL,&error)); xideCInputTraceDestroy(trace); trace=NULL; xrtValueRelease(bad); bad=NULL;
 }
 trace=xideCInputTraceCreate(); int stop=1; CHECK(trace && !xideCInputTraceApply(trace,wire,cancelled,&stop,&error)); xideCInputTraceDestroy(trace); trace=NULL; xrtValueRelease(row); row=NULL; xrtValueRelease(wire); wire=NULL;
 if(argc>2) {
  entry=xidePathResolve(root,"entry.c"); outer=xidePathResolve(root,"outer.h"); xl=xidePathResolve(root,"main.xl"); manifest=xidePathResolve(root,"project.xlp"); const char* source="#include \"outer.h\"\nTYPE inspect(TYPE input) { return input; }\n";
  CHECK(entry && outer && xl && manifest && text(entry,source) && text(outer,"#include \"间接 空间😀.h\"\n") && text(xl,"func main(...) as int\n return 0\nend\n") && xideProjectCreate(&project,manifest,"C 输入图",xl,&error) && xideProjectAddFile(&project,entry,"c",&error)); commands=xideCCommandsCreate(&project,NULL,NULL,&error); CHECK(commands);
  xide_c_document document={1,entry,source,1}; xide_c_job_spec spec={.server=argv[2],.directory=root,.commands=commands,.documents=&document,.count=1,.documentId=1,.offset=(int)(strstr(source,"input;")-source)};
  job=xideCAnalysisJobStart(&spec,&error); CHECK(job && waitJob(job,XIDE_C_JOB_READY)); reply=query(job,1,spec.offset); xstrview hover={0}; CHECK(reply && xrtValueGetString(get(get(get(reply,"result"),"contents"),"value"),&hover) && strstr(hover.Data,"double") && save(root,"real-indirect-hover.json",reply)); CHECK(xideCAnalysisJobGuard(job,leaf,&snapshot));
  CHECK(text(leaf,"#define TYPE int\n") && waitJob(job,XIDE_C_JOB_INVALIDATED)); xideCCompletionJobDestroy(job); job=NULL; xrtValueRelease(reply); reply=NULL;
  job=xideCAnalysisJobStart(&spec,&error); CHECK(job && waitJob(job,XIDE_C_JOB_READY)); reply=query(job,1,spec.offset); CHECK(reply && xrtValueGetString(get(get(get(reply,"result"),"contents"),"value"),&hover) && strstr(hover.Data,"int") && save(root,"real-rebuilt-hover.json",reply)); xideCCompletionJobDestroy(job); job=NULL;
  xrtValueRelease(reply); reply=NULL;
  const char* optionalSource="#define PROBE_HEADER \"optional.h\"\n#if __has_include(PROBE_HEADER)\n#include PROBE_HEADER\n#else\n#define OPTIONAL_TYPE double\n#endif\nOPTIONAL_TYPE probe(OPTIONAL_TYPE input) { return input; }\n";
  CHECK(text(entry,optionalSource)); document.text=optionalSource; document.version=2; spec.offset=(int)(strstr(optionalSource,"input;")-optionalSource);
  char* optionalProbe=xidePathResolve(root,"optional.h"); int observed=optionalProbe && observeMissing(&spec,optionalProbe); free(optionalProbe); CHECK(observed);
  job=xideCAnalysisJobStart(&spec,&error); CHECK(job && waitJob(job,XIDE_C_JOB_READY)); reply=query(job,1,spec.offset); CHECK(reply && xrtValueGetString(get(get(get(reply,"result"),"contents"),"value"),&hover) && strstr(hover.Data,"double") && save(root,"real-missing-probe-hover.json",reply));
  char* optional=xidePathResolve(root,"optional.h"); int created=optional && text(optional,"#define OPTIONAL_TYPE int\n"); free(optional); CHECK(created && waitJob(job,XIDE_C_JOB_INVALIDATED)); xideCCompletionJobDestroy(job); job=NULL; xrtValueRelease(reply); reply=NULL;
  job=xideCAnalysisJobStart(&spec,&error); CHECK(job && waitJob(job,XIDE_C_JOB_READY)); reply=query(job,1,spec.offset); CHECK(reply && xrtValueGetString(get(get(get(reply,"result"),"contents"),"value"),&hover) && strstr(hover.Data,"int") && save(root,"real-present-probe-hover.json",reply)); xideCCompletionJobDestroy(job); job=NULL; xrtValueRelease(reply); reply=NULL;
  const char* completionSource="#include \"outer.h\"\nvoid use(Box value) { value.tra }\n";
  CHECK(text(entry,completionSource) && text(leaf,"typedef struct Box { double tracedField; } Box;\n")); document.text=completionSource; document.version=3; spec.offset=(int)(strstr(completionSource,".tra")-completionSource)+4;
  job=xideCCompletionJobStart(&spec,&error); CHECK(job && waitJob(job,XIDE_C_JOB_READY)); reply=xideCCompletionJobResult(job); CHECK(reply && completionType(reply,"tracedField","double") && save(root,"real-indirect-completion.json",reply));
  CHECK(text(leaf,"typedef struct Box { int tracedField; } Box;\n") && waitJob(job,XIDE_C_JOB_INVALIDATED)); CHECK(!xideCCompletionJobResult(job)); xideCCompletionJobDestroy(job); job=NULL; xrtValueRelease(reply); reply=NULL;
  job=xideCCompletionJobStart(&spec,&error); CHECK(job && waitJob(job,XIDE_C_JOB_READY)); reply=xideCCompletionJobResult(job); CHECK(reply && completionType(reply,"tracedField","int") && save(root,"real-rebuilt-completion.json",reply)); xideCCompletionJobDestroy(job); job=NULL;
  xrtValueRelease(reply); reply=NULL;
  CHECK(searchMatrix(&spec,root));
  const char* responseSource="RESPONSE_TYPE probe(RESPONSE_TYPE input) { return input; }\n";
  char* response=xidePathResolve(root,"flags 空间😀.rsp"); int responseReady=response && text(response,"-DRESPONSE_TYPE=double\n") && text(entry,responseSource); CHECK(responseReady);
  size_t responseLength=strlen(response); char* responseArgument=malloc(responseLength+2); CHECK(responseArgument); responseArgument[0]='@'; memcpy(responseArgument+1,response,responseLength+1);
  int appended=xrtValueArrayAppendNew(get(xrtValueArrayGet(commands,0),"arguments"),xrtValueString(sv(responseArgument))); free(responseArgument); CHECK(appended); document.text=responseSource; document.version=4; spec.offset=(int)(strstr(responseSource,"input;")-responseSource);
  job=xideCAnalysisJobStart(&spec,&error); CHECK(job && waitJob(job,XIDE_C_JOB_READY)); reply=query(job,1,spec.offset); CHECK(reply && xrtValueGetString(get(get(get(reply,"result"),"contents"),"value"),&hover) && strstr(hover.Data,"double") && save(root,"real-response-file-hover.json",reply));
  CHECK(xideCAnalysisJobGuard(job,response,&snapshot)); CHECK(text(response,"-DRESPONSE_TYPE=int\n") && waitJob(job,XIDE_C_JOB_INVALIDATED)); xideCCompletionJobDestroy(job); job=NULL; xrtValueRelease(reply); reply=NULL;
  job=xideCAnalysisJobStart(&spec,&error); CHECK(job && waitJob(job,XIDE_C_JOB_READY)); reply=query(job,1,spec.offset); CHECK(reply && xrtValueGetString(get(get(get(reply,"result"),"contents"),"value"),&hover) && strstr(hover.Data,"int") && save(root,"real-rebuilt-response-hover.json",reply)); xideCCompletionJobDestroy(job); job=NULL; free(response);
  printf("Real traced C dependency gate passed.\n");
 } else printf("Real traced C dependency gate omitted: experimental provider was not supplied.\n");
 printf("c_input_trace_test passed: %d assertions; %s\n",assertions,root);
cleanup:
 xlangNativeClose(handle); xlangNativeFree(bytes); xideCCompletionJobDestroy(job); xideCInputTraceDestroy(trace); xideProjectClear(&project); xrtValueRelease(row); xrtValueRelease(wire); xrtValueRelease(bad); xrtValueRelease(commands); xrtValueRelease(reply); free(root); free(leaf); free(missing); free(replacement); free(entry); free(outer); free(xl); free(manifest); return failed;
}
