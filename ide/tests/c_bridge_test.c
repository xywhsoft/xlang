#define _WIN32_WINNT 0x0601
#include "platform/path.h"
#include "platform/text_file.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Inspect the actual prepared database and owned leases, without a fake server. */
#include "../src/services/c_completion_job.c"
static int assertions,failed;
#define CHECK(t) do { if(!(t)) { fprintf(stderr,"c_bridge_test:%d: %s; %s; %s\n",__LINE__,#t,error.message,message); failed=1; goto cleanup; } ++assertions; } while(0)
static xvalue* get(const xvalue* v,const char* k) { return v ? xrtValueObjectGet(v,cjsv(k)) : NULL; }
static const char* btstr(const xvalue* v) { xstrview s={0}; return xrtValueGetString(v,&s) ? s.Data : ""; }
static int put(xvalue* v,const char* k,xvalue* x) { return x && xrtValueObjectSetNew(v,cjsv(k),x); }
static int writeText(const char* p,const char* s) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static int record(const char* root,const char* name,const xvalue* v) { char* p=xidePathResolve(root,name); size_t n=0; char* text=xrtJsonStringify(v,true,&n); int okay=p && text && xrtFileWriteAtomic(p,(xbytesview){(unsigned char*)text,n}); free(p); xrtFree(text); return okay; }
static int waitState(xide_c_completion_job* j,enum xide_c_job_state wanted,char* message) { uint64_t until=GetTickCount64()+60000; do { enum xide_c_job_state s=xideCCompletionJobState(j,message,768); if(s==wanted) return 1; if(s>=XIDE_C_JOB_FAILED && s!=wanted) return 0; Sleep(5); } while(GetTickCount64()<until); return 0; }
static int waitDiagnostics(xide_c_completion_job* j,const char* root,const char* name,char* message) { uint64_t sequence=0,until=GetTickCount64()+30000; xide_c_event events[64]={0}; size_t count=0; int received=0,okay=1; int seen[64]={0}; do { if(xideCAnalysisJobDiagnostics(j,&sequence,events,64,&count)) { for(size_t i=0;i<count;++i) { const xvalue* diagnostics=get(get(events[i].message,"params"),"diagnostics"); if(!diagnostics || xrtValueCount(diagnostics)) okay=0; char detail[256]; snprintf(detail,sizeof(detail),"%s-%llu-%llu.json",name,(unsigned long long)events[i].documentId,(unsigned long long)sequence); if(!record(root,detail,events[i].message) || !record(root,name,events[i].message)) okay=0; for(size_t k=0;k<j->count;++k) if(events[i].documentId==j->documents[k].id && !seen[k]) { seen[k]=1; received++; } xideCEventClear(&events[i]); } } if(xideCCompletionJobState(j,message,768)!=XIDE_C_JOB_READY) return 0; if(received>=(int)j->count) return okay; Sleep(5); } while(GetTickCount64()<until); return 0; }
static xvalue* query(xide_c_completion_job* j,enum xide_c_query kind,uint64_t id,int offset) { uint64_t token=xideCAnalysisJobRequest(j,kind,id,offset),until=GetTickCount64()+30000; if(!token) return NULL; do { xide_c_event e={0}; if(xideCAnalysisJobTake(j,token,&e)) { xvalue* wire=e.message;e.message=NULL;xideCEventClear(&e);return wire; } if(xideCCompletionJobState(j,NULL,0)!=XIDE_C_JOB_READY) break; Sleep(5); } while(GetTickCount64()<until); xideCAnalysisJobCancelRequest(j,token); return NULL; }
static xvalue* project(const char* dir,const char* entry,const char* c) { xvalue *p=xrtValueObject(),*files=xrtValueArray(),*sources=xrtValueArray(); int okay=p && files && sources && xrtValueArrayAppendNew(files,xrtValueString(cjsv(c))) && xrtValueArrayAppendNew(sources,xrtValueString(cjsv(c))) && put(p,"directory",xrtValueString(cjsv(dir))) && put(p,"entry",xrtValueString(cjsv(entry))) && put(p,"shared",xrtValueBool(false)) && put(p,"includes",xrtValueArray()) && xrtValueObjectSet(p,cjsv("files"),files) && xrtValueObjectSet(p,cjsv("cSources"),sources); xrtValueRelease(files); xrtValueRelease(sources); if(!okay) { xrtValueRelease(p); return NULL; } return p; }
static int command(xvalue* db,const char* dir,const char* c) { xvalue *row=xrtValueObject(),*args=xrtValueArray(); const char* list[]={"clang","-x","c","-std=c11","-target","x86_64-w64-windows-gnu","-c",c}; int okay=row && args; for(size_t i=0;okay && i<sizeof(list)/sizeof(*list);++i) okay=xrtValueArrayAppendNew(args,xrtValueString(cjsv(list[i]))); if(okay) okay=put(row,"directory",xrtValueString(cjsv(dir))) && put(row,"file",xrtValueString(cjsv(c))) && xrtValueObjectSet(row,cjsv("arguments"),args) && xrtValueArrayAppend(db,row); xrtValueRelease(args); xrtValueRelease(row); return okay; }
static const char* operand(const xvalue* row,const char* option) { const xvalue* args=get(row,"arguments"); for(size_t i=0;i+1<xrtValueCount(args);++i) if(!strcmp(btstr(xrtValueArrayGet(args,i)),option)) return btstr(xrtValueArrayGet(args,i+1)); return NULL; }
static const char* mapped(const xvalue* items,const char* path) { const char* slash=strchr(path,'/'); size_t n=slash ? (size_t)(slash-path) : strlen(path); for(size_t i=0;i<xrtValueCount(items);++i) { const xvalue* row=xrtValueArrayGet(items,i); const char* name=btstr(get(row,"name")); if(strlen(name)==n && !strncmp(name,path,n)) return slash ? mapped(get(row,"contents"),slash+1) : btstr(get(row,"external-contents")); } return NULL; }
int main(int argc,char** argv)
{
 if(argc!=5 && argc!=6) return 2;
 xide_model_error error={0}; char message[768]={0}; char *root=NULL,*dirs[2]={0},*entries[2]={0},*cs[2]={0},*owned=NULL,*realHeader=NULL,*realDirectory=NULL,*customC=NULL; xvalue *projects=NULL,*commands=NULL,*reply=NULL,*vfs=NULL,*bad=NULL,*source=NULL; xide_c_completion_job* job=NULL; unsigned char* bytes=NULL; size_t length=0;
 char unique[80]; snprintf(unique,sizeof(unique),"bridge-native-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64()); root=xidePathResolve(argv[1],unique);
 const char* intXL="export \"xl_add\" func add(c_int a,c_int b) as c_int\n return a+b\nend\n";
 const char* doubleXL="export \"xl_add\" func add(c_double a,c_double b) as c_double\n return a+b\nend\n";
 const char* firstC="_Static_assert(__builtin_types_compatible_p(__typeof__(xl_add(1,2)), double), \"draft ABI\");\ndouble answer(void) { return xl_add(1,2); }\n";
 const char* secondC="_Static_assert(__builtin_types_compatible_p(__typeof__(xl_add(1,2)), int), \"other project ABI\");\nint answer(void) { return xl_add(1,2); }\n";
 if(argc==6) {
  bytes=xrtFileReadAllLimit(argv[5],65536,&length); CHECK(bytes && length && !memchr(bytes,0,length));
  customC=malloc(length+1); CHECK(customC); memcpy(customC,bytes,length);customC[length]=0;
  xrtFree(bytes);bytes=NULL; firstC=customC;
  CHECK(strstr(firstC,"xl_add(1,2); }") && strstr(firstC,"native_size_11") && strstr(firstC,"native___TINYC__"));
 }
 CHECK(root && xrtDirCreateAll(root)); projects=xrtValueArray(); commands=xrtValueArray(); CHECK(projects && commands);
 for(int i=0;i<2;++i) { dirs[i]=xidePathResolve(root,i ? "second" : "first"); entries[i]=xidePathResolve(dirs[i],"main.xl"); cs[i]=xidePathResolve(dirs[i],"native.c"); CHECK(dirs[i] && entries[i] && cs[i] && xrtDirCreateAll(dirs[i]) && writeText(entries[i],intXL) && writeText(cs[i],i ? secondC : firstC)); xvalue* p=project(dirs[i],entries[i],cs[i]); CHECK(p && xrtValueArrayAppendNew(projects,p) && command(commands,dirs[i],cs[i])); }
 xide_language_snapshot xl={entries[0],doubleXL,17}; xide_c_document docs[]={{101,cs[0],firstC,11},{102,cs[1],secondC,12}};
 xide_c_bridge_spec bridge={argv[3],projects,&xl,1}; xide_c_job_spec spec={.server=argv[2],.directory=root,.commands=commands,.documents=docs,.count=2,.documentId=101,.bridge=&bridge};
 job=xideCAnalysisJobStart(&spec,&error); CHECK(job); CHECK(put(xrtValueArrayGet(projects,0),"entry",xrtValueString(cjsv("caller-mutated.xl")))); xl.text="broken caller mutation";
 CHECK(waitState(job,XIDE_C_JOB_READY,message) && record(root,"prepared-commands.json",job->commands) && waitDiagnostics(job,root,"diagnostics.json",message));
 const char* header0=operand(xrtValueArrayGet(job->commands,0),"-include"),*header1=operand(xrtValueArrayGet(job->commands,1),"-include"); CHECK(header0 && header1 && strcmp(header0,header1)); owned=cjcopy(header0); CHECK(owned);
 reply=query(job,XIDE_C_HOVER,101,(int)(strstr(firstC,"xl_add(1,2); }")-firstC)+2); CHECK(reply && !get(reply,"error") && strstr(btstr(get(get(get(reply,"result"),"contents"),"value")),"double") && record(root,"draft-hover.json",reply)); xrtValueRelease(reply);reply=NULL;
 reply=query(job,XIDE_C_HOVER,102,(int)(strstr(secondC,"xl_add(1,2); }")-secondC)+2); CHECK(reply && strstr(btstr(get(get(get(reply,"result"),"contents"),"value")),"int") && record(root,"second-project-hover.json",reply)); xrtValueRelease(reply);reply=NULL;
 /* Real server definition replies preserve their wire form; local mapping
    also requires the querying project and the exact declaration interval. */
 reply=query(job,XIDE_C_DEFINITION,101,(int)(strstr(firstC,"xl_add(1,2); }")-firstC)+2);CHECK(reply && !get(reply,"error") && record(root,"draft-definition-wire.json",reply));
 source=xideCBridgeDefinition(job->bridge,cs[0],reply);
 if(source) {
  int64 version=0;CHECK(xidePathLexicalEqual(btstr(get(source,"path")),entries[0]) && xrtValueGetInt(get(source,"version"),&version) && version==17 && record(root,"draft-definition-source.json",source));
  xvalue* wrong=xideCBridgeDefinition(job->bridge,cs[1],reply);CHECK(!wrong);xrtValueRelease(wrong);
  bad=xrtValueDeepClone(reply);CHECK(bad);xvalue* result=get(bad,"result");xvalue* item=xrtValueType(result)==XVALUE_ARRAY ? xrtValueArrayGet(result,0) : result;xvalue* start=get(get(item,"range"),"start");int64 character=0;CHECK(xrtValueGetInt(get(start,"character"),&character) && put(start,"character",xrtValueInt(character+1)));wrong=xideCBridgeDefinition(job->bridge,cs[0],bad);CHECK(!wrong);xrtValueRelease(wrong);xrtValueRelease(bad);bad=NULL;
  xvalue *link=xrtValueObject(),*target=xrtValueObject();CHECK(link && target && xrtValueObjectSet(target,cjsv("targetUri"),get(xrtValueArrayGet(get(reply,"result"),0),"uri")) && xrtValueObjectSet(target,cjsv("targetSelectionRange"),get(xrtValueArrayGet(get(reply,"result"),0),"range")) && xrtValueObjectSet(target,cjsv("targetRange"),get(xrtValueArrayGet(get(reply,"result"),0),"range")) && xrtValueObjectSet(link,cjsv("result"),target));xrtValueRelease(target);wrong=xideCBridgeDefinition(job->bridge,cs[0],link);CHECK(wrong && xidePathLexicalEqual(btstr(get(wrong,"path")),entries[0]));xrtValueRelease(wrong);xrtValueRelease(link);
  printf("Verified real definition metadata, wrong project/range rejection and equivalent LocationLink.\n");
 } else puts("Optional source mapping unavailable; verified ordinary definition wire only.");
 xrtValueRelease(source);source=NULL;xrtValueRelease(reply);reply=NULL;
 xide_file_snapshot stamp={0}; CHECK(!xideCAnalysisJobGuard(job,header0,&stamp));
 const char* overlay=operand(xrtValueArrayGet(job->commands,0),"-ivfsoverlay"); CHECK(overlay); bytes=xrtFileReadAllLimit(overlay,1024*1024,&length); CHECK(bytes); vfs=xrtJsonParse((xstrview){(char*)bytes,length}); xrtFree(bytes);bytes=NULL; CHECK(vfs && !strcmp(btstr(get(vfs,"redirecting-with")),"fallback")); const char* builtin=mapped(get(xrtValueArrayGet(get(vfs,"roots"),0),"contents"),"include/__xl_runtime/xruntime_decl.h"); CHECK(builtin); bytes=xrtFileReadAllLimit(builtin,16*1024*1024,&length); CHECK(bytes);
 realHeader=xidePathResolve(dirs[0],"include/__xl_runtime/xruntime_decl.h"); realDirectory=xidePathDirectory(realHeader); CHECK(realHeader && realDirectory && xrtDirCreateAll(realDirectory));
 CHECK(xrtFileWriteAtomic(realHeader,(xbytesview){bytes,length}) && waitState(job,XIDE_C_JOB_INVALIDATED,message) && !xideCAnalysisJobSupports(job,XIDE_C_HOVER));
 xideCCompletionJobDestroy(job);job=NULL; CHECK(!xrtFileExists(owned)); xrtFree(bytes);bytes=NULL; free(owned);owned=NULL; xrtValueRelease(vfs);vfs=NULL;
 /* Ordinary disk files override the embedded fallback, preserving virtual names. */
 bytes=xrtFileReadAllLimit(realHeader,16*1024*1024,&length); CHECK(bytes); const char* marker="#define REAL_HEADER_MARK 91\n"; size_t mn=strlen(marker); char* marked=malloc(mn+length); CHECK(marked); memcpy(marked,marker,mn);memcpy(marked+mn,bytes,length); CHECK(xrtFileWriteAtomic(realHeader,(xbytesview){(unsigned char*)marked,mn+length})); free(marked); xrtFree(bytes);bytes=NULL;
 const char* realC="_Static_assert(REAL_HEADER_MARK==91, \"disk header priority\");\nint answer(void) { return xl_add(1,2); }\n"; docs[0].text=realC;docs[0].version++; xl.text=intXL;xl.version++; CHECK(put(xrtValueArrayGet(projects,0),"entry",xrtValueString(cjsv(entries[0]))));
 job=xideCAnalysisJobStart(&spec,&error); CHECK(job && waitState(job,XIDE_C_JOB_READY,message) && waitDiagnostics(job,root,"disk-priority-diagnostics.json",message));
 CHECK(writeText(entries[1],doubleXL) && waitState(job,XIDE_C_JOB_INVALIDATED,message) && !xideCAnalysisJobRequest(job,XIDE_C_HOVER,101,0)); xideCCompletionJobDestroy(job);job=NULL;
 CHECK(writeText(entries[1],intXL)); xl.text="func broken( as\n"; xl.version++; job=xideCAnalysisJobStart(&spec,&error); CHECK(job && waitState(job,XIDE_C_JOB_INVALIDATED,message) && !xideCAnalysisJobSupports(job,XIDE_C_HOVER)); xideCCompletionJobDestroy(job);job=NULL;
 /* Older compiler support is reported explicitly, while ordinary C remains usable. */
 bridge.compiler=argv[4]; docs[0].text="int answer(void) { return 42; }\n";docs[1].text=docs[0].text; docs[0].version++;docs[1].version++; job=xideCAnalysisJobStart(&spec,&error); CHECK(job && waitState(job,XIDE_C_JOB_READY,message) && strstr(message,"lacks automatic") && waitDiagnostics(job,root,"legacy-diagnostics.json",message)); xideCCompletionJobDestroy(job);job=NULL;
 bridge.compiler=argv[3];xl.text=intXL;xl.version++; bad=xrtValueDeepClone(projects); CHECK(bad && xrtValueArrayAppendNew((xvalue*)get(xrtValueArrayGet(bad,1),"files"),xrtValueString(cjsv(cs[0])))); bridge.projects=bad; CHECK(!xideCAnalysisJobStart(&spec,&error)); bridge.projects=projects; xrtValueRelease(bad);bad=NULL;
 job=xideCAnalysisJobStart(&spec,&error); CHECK(job); xideCCompletionJobCancel(job); CHECK(waitState(job,XIDE_C_JOB_CANCELLED,message)); xideCCompletionJobDestroy(job);job=NULL;
 if(argc==6) {
  /* A verified Win64 profile must not be applied to an incompatible target.
     The real compiler query runs; failed preparation leaves commands intact. */
  bad=xrtValueDeepClone(commands); CHECK(bad);xvalue* args=(xvalue*)get(xrtValueArrayGet(bad,0),"arguments");
  CHECK(xrtValueArraySetNew(args,5,xrtValueString(cjsv("i686-w64-windows-gnu"))));
  reply=xrtValueDeepClone(bad); CHECK(reply);
  xide_c_bridge_batch* batch=xideCBridgeCopy(&bridge,&error); CHECK(batch);
  int rejected=!xideCBridgePrepare(batch,&bad,NULL,NULL,&error);xideCBridgeDestroy(batch);
  CHECK(rejected && xrtValueEqual(bad,reply) && record(root,"unsupported-target-preserved.json",bad));
  xrtValueRelease(bad);bad=NULL;xrtValueRelease(reply);reply=NULL;
  puts("Verified measured native scalar/identity assertions and incompatible-target transaction rejection.");
 }
 bytes=xrtFileReadAllLimit(entries[0],1024,&length); CHECK(bytes && length==strlen(intXL) && !memcmp(bytes,intXL,length)); xrtFree(bytes);bytes=NULL;
 printf("c_bridge_test passed: %d assertions; real compiler/clangd, copied double draft, two project ABIs, exact builtin fallback and real disk priority, source/header invalidation, malformed draft withdrawal, explicit legacy fallback, duplicate ownership and cancellation; evidence: %s.\n",assertions,root);
cleanup:
 xideCCompletionJobDestroy(job); xrtFree(bytes);xrtValueRelease(reply);xrtValueRelease(source);xrtValueRelease(vfs);xrtValueRelease(bad);xrtValueRelease(projects);xrtValueRelease(commands); free(customC);free(owned);free(realHeader);free(realDirectory);for(int i=0;i<2;++i) { free(dirs[i]);free(entries[i]);free(cs[i]); } free(root);return failed;
}
