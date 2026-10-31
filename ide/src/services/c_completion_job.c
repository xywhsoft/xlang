#include "app/features.h"
#if XIDE_ENABLE_C_LSP
#include "services/c_completion_job.h"
#include "services/c_input_trace.h"
#include "platform/path.h"
#include "platform/file_rename.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define CJ_DOCS 64u
#define CJ_GUARDS 65536u
typedef struct cj_request { uint64_t token,documentId,until; enum xide_c_query query; int offset,remote,state; xide_c_event event; } cj_request;
typedef struct cj_guard { char* path; int exists; xide_file_snapshot stamp; } cj_guard;
struct xide_c_completion_job {
 HANDLE thread; CRITICAL_SECTION lock; volatile LONG cancel;
 char *server,*directory; xvalue *commands,*result; xide_c_bridge_batch* bridge; xide_c_document documents[CJ_DOCS]; size_t count;
 cj_guard* guards; size_t guardCount; xide_c_input_trace* trace; uint64_t documentId; int offset;
 int analysis,supported[4]; uint64_t nextToken,diagnosticSequence;
 cj_request requests[64]; xide_c_event diagnostics[CJ_DOCS];
 enum xide_c_job_state state; char message[768],bridgeNotice[384]; void (*wake)(void*); void* user;
};
static xstrview cjsv(const char* s) { return (xstrview){s,strlen(s)}; }
static char* cjcopy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static int cjpathText(const char* s) { size_t n=s ? strlen(s) : 0; return n && n<=32768 && MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,(int)n,NULL,0); }
static int cjfail(xide_model_error* e,const char* message) { if(e) { e->code=XIDE_MODEL_INVALID; snprintf(e->message,sizeof(e->message),"%s",message); } return 0; }
static int cjcancelled(xide_c_completion_job* j) { return InterlockedCompareExchange(&j->cancel,0,0)!=0; }
static void cjpublish(xide_c_completion_job* j,enum xide_c_job_state state,const char* message,xvalue* result)
{
 EnterCriticalSection(&j->lock);
 j->state=cjcancelled(j) ? XIDE_C_JOB_CANCELLED : state; snprintf(j->message,sizeof(j->message),"%s",message ? message : "");
 if(result && j->state==XIDE_C_JOB_READY) j->result=xrtValueRetain(result);
 LeaveCriticalSection(&j->lock); if(j->wake) j->wake(j->user);
}
static int cjguardAdd(xide_c_completion_job* j,const char* path,int overlay)
{
 if(!path || !*path) return 0;
 if(overlay) for(size_t i=0;i<j->count;++i) if(xidePathLexicalEqual(path,j->documents[i].path)) return 1;
 for(size_t i=0;i<j->guardCount;++i) if(xidePathLexicalEqual(path,j->guards[i].path)) return 1;
 if(j->guardCount>=CJ_GUARDS) return 0;
 char* absolute=xidePathResolveLexical(NULL,path); if(!absolute) return 0;
 j->guards[j->guardCount++].path=absolute; return 1;
}
static int cjinspect(const char* path,int* exists,xide_file_snapshot* stamp,xide_model_error* e)
{
 int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,NULL,0); wchar_t* w=n ? malloc((size_t)n*sizeof(*w)) : NULL;
 if(!w) return cjfail(e,"Cannot inspect C guard path.");
 MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,w,n); DWORD attrs=GetFileAttributesW(w),code=GetLastError(); free(w);
 if(attrs==INVALID_FILE_ATTRIBUTES && (code==ERROR_FILE_NOT_FOUND || code==ERROR_PATH_NOT_FOUND)) { *exists=0; memset(stamp,0,sizeof(*stamp)); return 1; }
 if(attrs==INVALID_FILE_ATTRIBUTES || (attrs&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))) return cjfail(e,"C guard input is unreadable or not a regular file.");
 *exists=1; return xideFileSnapshotInspect(path,stamp,e);
}
static int cjguardRange(xide_c_completion_job* j,size_t first,size_t end,int capture,xide_model_error* e)
{
 uint64_t total=0;
 for(size_t i=first;i<end;++i) {
  if(cjcancelled(j)) return 0;
  cj_guard* g=&j->guards[i]; int exists=0; xide_file_snapshot now={0};
  if(!cjinspect(g->path,&exists,&now,e)) return 0;
  if(now.length>1024u*1024u*1024u-total) return cjfail(e,"Explicit C guard inputs exceed the 1 GiB streaming bound.");
  total+=now.length;
  if(capture) { g->exists=exists; g->stamp=now; }
  else if(exists!=g->exists || (exists && !xideFileSnapshotEqual(&g->stamp,&now))) return cjfail(e,"Observed C input content or physical identity changed.");
 }
 return 1;
}
static int cjbridgeCancelled(void* user) { return cjcancelled(user); }
static int cjguardCheck(xide_c_completion_job* j,int capture,xide_model_error* e)
{ return cjguardRange(j,0,j->guardCount,capture,e) && (capture || !j->trace || xideCInputTraceCheck(j->trace,cjbridgeCancelled,j,e)) && (!j->bridge || capture || xideCBridgeGuard(j->bridge,e)); }
static int cjtraceEvent(xide_c_completion_job* j,xide_c_session* s,const xide_c_event* event,xide_model_error* e)
{
 if(event->kind==XIDE_C_INPUT_TRACE) { if(!j->trace) { xide_c_input_trace* created=xideCInputTraceCreate(); EnterCriticalSection(&j->lock); j->trace=created; LeaveCriticalSection(&j->lock); } return j->trace ? xideCInputTraceApply(j->trace,event->message,cjbridgeCancelled,j,e) : cjfail(e,"Cannot allocate C compiler input observations."); }
 if(xideCSessionTracing(s) && (event->kind==XIDE_C_DIAGNOSTICS || event->kind==XIDE_C_RESULT))
  return xideCInputTraceComplete(j->trace) ? cjguardCheck(j,0,e) : cjfail(e,"C semantic result arrived before a complete input trace.");
 return 1;
}
static int cjprepare(xide_c_completion_job* j,xide_model_error* e)
{
 if(!cjguardCheck(j,1,e)) return 0;
 if(!j->bridge) return 1;
 size_t first=j->guardCount;
 if(!xideCBridgePrepare(j->bridge,&j->commands,cjbridgeCancelled,j,e)) {
  if(e->code!=XIDE_MODEL_LEGACY) return 0;
  snprintf(j->bridgeNotice,sizeof(j->bridgeNotice),"%s","Selected compiler lacks automatic C/xlang bridge support; C analysis uses its explicit profile.");
  xideCBridgeDestroy(j->bridge); j->bridge=NULL; memset(e,0,sizeof(*e)); return cjguardCheck(j,0,e);
 }
 if(!cjguardRange(j,0,first,0,e)) return 0;
 const xvalue* paths=xideCBridgeObservedPaths(j->bridge);
 for(size_t i=0;i<xrtValueCount(paths);++i) {
  xstrview path={0}; if(!xrtValueGetString(xrtValueArrayGet(paths,i),&path) || !path.Size || !cjguardAdd(j,path.Data,1)) return cjfail(e,"Cannot guard generated headers or builtin fallback candidates.");
 }
 return cjguardRange(j,first,j->guardCount,1,e) && cjguardCheck(j,0,e);
}
static void cjreleaseBridge(xide_c_completion_job* j)
{ xideCBridgeDestroy(j->bridge); j->bridge=NULL; }
static void cjeventCopy(xide_c_event* to,const xide_c_event* from)
{ *to=*from; to->message=xrtValueDeepClone(from->message);to->sourceDefinition=from->sourceDefinition ? xrtValueDeepClone(from->sourceDefinition) : NULL; }
static void cjanalyzeEvent(xide_c_completion_job* j,const xide_c_event* event)
{
 EnterCriticalSection(&j->lock);
 if(!cjcancelled(j)) {
  if(event->kind==XIDE_C_DIAGNOSTICS) for(size_t i=0;i<j->count;++i) if(j->documents[i].id==event->documentId && j->documents[i].version==event->version) {
   xideCEventClear(&j->diagnostics[i]); cjeventCopy(&j->diagnostics[i],event); ++j->diagnosticSequence; break;
  }
  if(event->kind==XIDE_C_RESULT) for(size_t i=0;i<64;++i) if(j->requests[i].state==2 && j->requests[i].remote==event->requestId) {
   cjeventCopy(&j->requests[i].event,event);
   if(event->query==XIDE_C_DEFINITION) for(size_t d=0;d<j->count;++d) if(j->documents[d].id==event->documentId) {
    xrtValueRelease(j->requests[i].event.sourceDefinition);j->requests[i].event.sourceDefinition=xideCBridgeDefinition(j->bridge,j->documents[d].path,event->message);break;
   }
   j->requests[i].state=3; break;
  }
 }
 LeaveCriticalSection(&j->lock); if(j->wake) j->wake(j->user);
}
static int cjanalyzeRequests(xide_c_completion_job* j,xide_c_session* s,xide_model_error* e)
{
 EnterCriticalSection(&j->lock);
 int okay=1;
 for(size_t i=0;okay && !cjcancelled(j) && i<64;++i) {
  cj_request* r=&j->requests[i];
  if(r->state==4) { if(r->remote>1) xideCSessionCancel(s,r->remote); xideCEventClear(&r->event); memset(r,0,sizeof(*r)); }
  else if(r->state==1) { r->remote=xideCSessionRequest(s,r->query,r->documentId,r->offset,e); okay=r->remote>1; if(okay) { r->state=2; r->until=GetTickCount64()+30000; } }
  else if(r->state==2 && GetTickCount64()>=r->until) { okay=cjfail(e,"C semantic request timed out."); }
 }
 LeaveCriticalSection(&j->lock); return okay;
}
static DWORD WINAPI cjanalyzeWorker(void* user)
{
 xide_c_completion_job* j=user; xide_model_error e={0}; xide_c_event event={0}; xide_c_session* s=NULL;
 int bridgeRequested=j->bridge!=NULL,okay=cjprepare(j,&e),initialized=0; uint64_t until=GetTickCount64()+30000,next=until;
 if(okay && !cjcancelled(j)) { s=xideCSessionStart(j->server,j->directory,j->commands,j->documents,j->count,&e); okay=s!=NULL; }
 while(okay && !cjcancelled(j)) {
  unsigned drained=0; uint64_t slice=GetTickCount64();
  while(!cjcancelled(j) && drained<32 && GetTickCount64()-slice<8 && xideCSessionNext(s,&event)) {
   okay=cjtraceEvent(j,s,&event,&e);
   if(okay && event.kind==XIDE_C_INITIALIZED) {
    okay=cjguardCheck(j,0,&e);
    if(okay) { EnterCriticalSection(&j->lock); for(int i=0;i<4;++i) j->supported[i]=xideCSessionSupports(s,(enum xide_c_query)i); LeaveCriticalSection(&j->lock); initialized=1; next=GetTickCount64()+1000; cjpublish(j,XIDE_C_JOB_READY,j->bridgeNotice,NULL); }
   } else if(okay && event.kind!=XIDE_C_INPUT_TRACE) cjanalyzeEvent(j,&event);
   xideCEventClear(&event); ++drained; if(!okay) break;
  }
  if(okay && xideCSessionState(s,e.message,sizeof(e.message))>=XIDE_C_FAILED) okay=0;
  if(okay && !initialized && GetTickCount64()>=until) okay=cjfail(&e,"C analysis initialization timed out.");
  if(okay && initialized) okay=cjanalyzeRequests(j,s,&e);
  if(okay && initialized && GetTickCount64()>=next) { okay=cjguardCheck(j,0,&e); next=GetTickCount64()+1000; }
  if(okay) Sleep(5);
 }
 cjpublish(j,cjcancelled(j) ? XIDE_C_JOB_CANCELLED : initialized || bridgeRequested ? XIDE_C_JOB_INVALIDATED : XIDE_C_JOB_FAILED,*e.message ? e.message : "C analysis stopped.",NULL);
 xideCEventClear(&event); xideCSessionDestroy(s); cjreleaseBridge(j); return 0;
}
/* Index the compiler's exact generated header as a private document. With an
   empty main-file preamble clangd omits forced-header globals from its dynamic
   index. This header has real source text and a real owned compilation row;
   user documents and all their coordinates remain unchanged. */
static xide_c_session* cjcompletionSession(xide_c_completion_job* j,uint64_t* indexed,xide_model_error* e)
{
 *indexed=0;
 if(!j->bridge) return xideCSessionStartCompletion(j->server,j->directory,j->commands,j->documents,j->count,e);
 const char* target=NULL; for(size_t i=0;i<j->count;++i) if(j->documents[i].id==j->documentId) target=j->documents[i].path;
 const xvalue* original=NULL; for(size_t i=0;target && i<xrtValueCount(j->commands);++i) { xvalue* row=xrtValueArrayGet(j->commands,i); xstrview name={0}; if(xrtValueGetString(xrtValueObjectGet(row,cjsv("file")),&name) && xidePathLexicalEqual(target,name.Data)) original=row; }
 const xvalue* old=original ? xrtValueObjectGet(original,cjsv("arguments")) : NULL; const char* header=NULL; size_t remove=0;
 for(size_t i=1;old && i+1<xrtValueCount(old);++i) { xstrview flag={0},path={0}; if(xrtValueGetString(xrtValueArrayGet(old,i),&flag) && !strcmp(flag.Data,"-include") && xrtValueGetString(xrtValueArrayGet(old,i+1),&path) && xideCBridgePrivatePath(j->bridge,path.Data)) { header=path.Data;remove=i;break; } }
 size_t length=0; unsigned char* bytes=header ? xrtFileReadAllLimit(header,1024u*1024u+256,&length) : NULL;
 xvalue *database=xrtValueDeepClone(j->commands),*row=xrtValueDeepClone((xvalue*)original),*args=xrtValueArray(); int okay=bytes && database && row && args && xrtValueCount(database)<2049;
 for(size_t i=0;okay && i<xrtValueCount(old);++i) {
  if(i==remove) { ++i; continue; }
  xstrview arg={0}; okay=xrtValueGetString(xrtValueArrayGet(old,i),&arg);
  if(okay) okay=xrtValueArrayAppendNew(args,xrtValueString(cjsv(target && xidePathLexicalEqual(arg.Data,target) ? header : arg.Data)));
 }
 if(okay) okay=xrtValueObjectSetNew(row,cjsv("file"),xrtValueString(cjsv(header))) && xrtValueObjectSet(row,cjsv("arguments"),args) && xrtValueArrayAppend(database,row);
 uint64_t id=UINT64_MAX; int collision=1; while(collision) { collision=0;for(size_t i=0;i<j->count;++i) if(j->documents[i].id==id) { --id;collision=1;break; } }
 xide_c_document documents[CJ_DOCS+1];for(size_t i=0;i<j->count;++i) documents[i]=j->documents[i];documents[j->count]=(xide_c_document){id,header,(char*)bytes,1};
 xide_c_session* session=okay ? xideCSessionStartCompletion(j->server,j->directory,database,documents,j->count+1,e) : NULL;
 if(session) *indexed=id;else if(!*e->message) cjfail(e,"Cannot index the exact owned bridge declaration header.");
 xrtFree(bytes);xrtValueRelease(args);xrtValueRelease(row);xrtValueRelease(database);return session;
}
static DWORD WINAPI cjworker(void* user)
{
 xide_c_completion_job* j=user; xide_model_error e={0}; xide_c_session* s=NULL; xide_c_event event={0}; xvalue* result=NULL; uint64_t indexed=0; int initialized=0,request=0,okay=cjprepare(j,&e); uint64_t until=GetTickCount64()+30000;
 if(okay && !cjcancelled(j)) { s=cjcompletionSession(j,&indexed,&e); okay=s!=NULL; }
 while(okay && !cjcancelled(j) && GetTickCount64()<until && !result) {
  if(xideCSessionNext(s,&event)) {
   okay=cjtraceEvent(j,s,&event,&e);
   if(okay && event.kind==XIDE_C_INITIALIZED) initialized=1;
   if(okay && initialized && !request && (!indexed || (event.kind==XIDE_C_DIAGNOSTICS && event.documentId==indexed))) { request=xideCSessionRequest(s,XIDE_C_COMPLETION,j->documentId,j->offset,&e); okay=request>1; }
   else if(okay && event.kind==XIDE_C_RESULT && event.requestId==request) result=xrtValueDeepClone(event.message);
   xideCEventClear(&event);
  } else if(xideCSessionState(s,e.message,sizeof(e.message))>=XIDE_C_FAILED) okay=0;
  if(!result) Sleep(5);
 }
 if(okay && !cjcancelled(j) && result) okay=cjguardCheck(j,0,&e);
 if(!okay || cjcancelled(j) || !result) {
  cjpublish(j,cjcancelled(j) ? XIDE_C_JOB_CANCELLED : XIDE_C_JOB_FAILED,*e.message ? e.message : "C completion timed out or failed.",NULL);
 } else {
  cjpublish(j,XIDE_C_JOB_READY,j->bridgeNotice,result); uint64_t next=GetTickCount64()+1000;
  while(!cjcancelled(j)) {
   unsigned drained=0; uint64_t slice=GetTickCount64();
   while(!cjcancelled(j) && drained<32 && GetTickCount64()-slice<8 && xideCSessionNext(s,&event)) { okay=cjtraceEvent(j,s,&event,&e); xideCEventClear(&event); ++drained; if(!okay) break; }
   if(!okay) { cjpublish(j,XIDE_C_JOB_INVALIDATED,e.message,NULL); break; }
   if(xideCSessionState(s,e.message,sizeof(e.message))>=XIDE_C_FAILED) { cjpublish(j,XIDE_C_JOB_INVALIDATED,*e.message ? e.message : "C server stopped.",NULL); break; }
   if(GetTickCount64()>=next) { if(!cjguardCheck(j,0,&e)) { cjpublish(j,XIDE_C_JOB_INVALIDATED,e.message,NULL); break; } next=GetTickCount64()+1000; }
   Sleep(10);
  }
 }
 xideCEventClear(&event); xideCSessionDestroy(s); cjreleaseBridge(j); xrtValueRelease(result); return 0;
}
static xide_c_completion_job* cjstart(const xide_c_job_spec* spec,xide_model_error* e,int analysis)
{
 if(!spec || !cjpathText(spec->server) || !cjpathText(spec->directory) || !spec->commands || !spec->documents || !spec->count || spec->count>CJ_DOCS || spec->extraCount>128 || (spec->extraCount && !spec->extraFiles) || !spec->documentId || spec->offset<0 || xrtValueType(spec->commands)!=XVALUE_ARRAY || xrtValueCount(spec->commands)>2048) { cjfail(e,"Invalid C completion job inputs."); return NULL; }
 xide_c_completion_job* j=calloc(1,sizeof(*j)); if(!j) { cjfail(e,"Cannot allocate C completion job."); return NULL; }
 InitializeCriticalSection(&j->lock); j->analysis=analysis; j->server=cjcopy(spec->server); j->directory=cjcopy(spec->directory); j->commands=xrtValueDeepClone(spec->commands); j->guards=calloc(CJ_GUARDS,sizeof(*j->guards)); j->documentId=spec->documentId; j->offset=spec->offset; j->wake=spec->wake; j->user=spec->user; int okay=j->server && j->directory && j->commands && j->guards; size_t total=0;
 for(size_t i=0;okay && i<spec->count;++i) {
  const xide_c_document* d=&spec->documents[i]; size_t n=d->text ? strlen(d->text) : 0;
  okay=d->id && d->path && d->text && d->version>0 && n<=8u*1024u*1024u && n<=32u*1024u*1024u-total;
  if(!okay) break;
  xide_c_document* b=&j->documents[j->count++]; *b=*d; b->path=cjcopy(d->path); b->text=cjcopy(d->text); okay=b->path && b->text; total+=n;
 }
 if(okay) okay=cjguardAdd(j,j->server,0);
 for(size_t i=0;okay && i<xrtValueCount(j->commands);++i) { xvalue* row=xrtValueArrayGet(j->commands,i); xstrview file={0}; okay=xrtValueType(row)==XVALUE_OBJECT && xrtValueGetString(xrtValueObjectGet(row,cjsv("file")),&file) && file.Size && file.Size<=32768 && !memchr(file.Data,0,file.Size) && cjpathText(file.Data) && cjguardAdd(j,file.Data,1); }
 for(size_t i=0;okay && i<xrtValueCount(j->commands);++i) {
  const xvalue* row=xrtValueArrayGet(j->commands,i); xstrview base={0};
  const xvalue* args=xrtValueObjectGet(row,cjsv("arguments")); okay=args && xrtValueType(args)==XVALUE_ARRAY && xrtValueCount(args)<=1024 && xrtValueGetString(xrtValueObjectGet(row,cjsv("directory")),&base) && base.Size && base.Size<=32768 && !memchr(base.Data,0,base.Size) && cjpathText(base.Data);
  for(size_t k=1;okay && k<xrtValueCount(args);++k) {
   xstrview flag={0}; okay=xrtValueGetString(xrtValueArrayGet(args,k),&flag);
   if(okay && ((flag.Size==8 && !memcmp(flag.Data,"-include",8)) || (flag.Size==8 && !memcmp(flag.Data,"-imacros",8)))) {
    xstrview file={0}; okay=++k<xrtValueCount(args) && xrtValueGetString(xrtValueArrayGet(args,k),&file) && file.Size && file.Size<=32768 && !memchr(file.Data,0,file.Size) && cjpathText(file.Data);
    char* path=okay ? xidePathResolveLexical(base.Data,file.Data) : NULL; okay=path && cjguardAdd(j,path,1); free(path);
   }
  }
 }
 for(size_t i=0;okay && i<spec->extraCount;++i) okay=cjguardAdd(j,spec->extraFiles[i],1);
 if(okay && spec->bridge) { j->bridge=xideCBridgeCopy(spec->bridge,e); okay=j->bridge!=NULL;
  if(okay) { char* base=xidePathDirectory(spec->bridge->compiler); okay=base && cjguardAdd(j,spec->bridge->compiler,0);
   const char* siblings[]={"xlc.dll","xl.dll"}; for(size_t i=0;okay && i<2;++i) { char* path=xidePathResolveLexical(base,siblings[i]); okay=path && cjguardAdd(j,path,0); free(path); } free(base); } }
 if(okay) j->thread=CreateThread(NULL,0,analysis ? cjanalyzeWorker : cjworker,j,0,NULL);
 if(!j->thread) { xideCCompletionJobDestroy(j); cjfail(e,"Cannot prepare or start copied C completion job."); return NULL; } return j;
}
enum xide_c_job_state xideCCompletionJobState(xide_c_completion_job* j,char* message,size_t capacity)
{
 if(!j) return XIDE_C_JOB_CANCELLED;
 EnterCriticalSection(&j->lock); enum xide_c_job_state state=cjcancelled(j) ? XIDE_C_JOB_CANCELLED : j->state; if(message && capacity) snprintf(message,capacity,"%s",j->message); LeaveCriticalSection(&j->lock); return state;
}
xvalue* xideCCompletionJobResult(xide_c_completion_job* j)
{
 if(!j) return NULL;
 EnterCriticalSection(&j->lock); xvalue* result=!cjcancelled(j) && j->state==XIDE_C_JOB_READY ? xrtValueDeepClone(j->result) : NULL; LeaveCriticalSection(&j->lock); return result;
}
void xideCCompletionJobCancel(xide_c_completion_job* j) { if(j) InterlockedExchange(&j->cancel,1); }
int xideCCompletionJobDone(xide_c_completion_job* j) { return !j || !j->thread || WaitForSingleObject(j->thread,0)==WAIT_OBJECT_0; }
void xideCCompletionJobDestroy(xide_c_completion_job* j)
{
 if(!j) return;
 xideCCompletionJobCancel(j); if(j->thread) { WaitForSingleObject(j->thread,INFINITE); CloseHandle(j->thread); }
 for(size_t i=0;i<j->count;++i) { free((char*)j->documents[i].path); free((char*)j->documents[i].text); }
 for(size_t i=0;i<j->guardCount;++i) free(j->guards[i].path);
 for(size_t i=0;i<CJ_DOCS;++i) xideCEventClear(&j->diagnostics[i]);
 for(size_t i=0;i<64;++i) xideCEventClear(&j->requests[i].event);
 cjreleaseBridge(j); xideCInputTraceDestroy(j->trace); free(j->guards); free(j->server); free(j->directory); xrtValueRelease(j->commands); xrtValueRelease(j->result); DeleteCriticalSection(&j->lock); free(j);
}

xide_c_completion_job* xideCCompletionJobStart(const xide_c_job_spec* spec,xide_model_error* e) { return cjstart(spec,e,0); }
xide_c_completion_job* xideCAnalysisJobStart(const xide_c_job_spec* spec,xide_model_error* e) { return cjstart(spec,e,1); }
int xideCAnalysisJobSupports(xide_c_completion_job* j,enum xide_c_query query)
{ if(!j || query<0 || query>XIDE_C_SYMBOLS) return 0; EnterCriticalSection(&j->lock); int yes=j->analysis && !cjcancelled(j) && j->state==XIDE_C_JOB_READY && j->supported[query]; LeaveCriticalSection(&j->lock); return yes; }
uint64_t xideCAnalysisJobRequest(xide_c_completion_job* j,enum xide_c_query query,uint64_t id,int offset)
{
 if(!j || query<0 || query>XIDE_C_SYMBOLS || !id || offset<0) return 0;
 EnterCriticalSection(&j->lock); uint64_t token=0; int known=0;
 for(size_t i=0;i<j->count;++i) if(j->documents[i].id==id && (query==XIDE_C_SYMBOLS || (size_t)offset<=strlen(j->documents[i].text))) known=1;
 if(known && j->analysis && !cjcancelled(j) && j->state==XIDE_C_JOB_READY && j->supported[query] && j->nextToken<UINT64_MAX) for(size_t i=0;i<64;++i) if(!j->requests[i].state) {
  token=++j->nextToken; j->requests[i]=(cj_request){.token=token,.documentId=id,.query=query,.offset=offset,.state=1}; break;
 } LeaveCriticalSection(&j->lock); return token;
}
void xideCAnalysisJobCancelRequest(xide_c_completion_job* j,uint64_t token)
{ if(!j || !token) return; EnterCriticalSection(&j->lock); for(size_t i=0;i<64;++i) if(j->requests[i].token==token) j->requests[i].state=4; LeaveCriticalSection(&j->lock); }
int xideCAnalysisJobTake(xide_c_completion_job* j,uint64_t token,xide_c_event* event)
{
 if(!j || !event || !token) return 0;
 EnterCriticalSection(&j->lock); int yes=0;
 if(!cjcancelled(j) && j->state==XIDE_C_JOB_READY) for(size_t i=0;i<64;++i) if(j->requests[i].token==token && j->requests[i].state==3) {
  *event=j->requests[i].event; memset(&j->requests[i],0,sizeof(j->requests[i])); yes=1; break;
 } LeaveCriticalSection(&j->lock); return yes;
}
int xideCAnalysisJobDiagnostics(xide_c_completion_job* j,uint64_t* seq,xide_c_event* events,size_t cap,size_t* count)
{
 if(!j || !seq || !events || !count || cap<j->count) return 0;
 EnterCriticalSection(&j->lock); int yes=0; *count=0;
 if(!cjcancelled(j) && j->state==XIDE_C_JOB_READY && *seq!=j->diagnosticSequence) {
  int okay=1; for(size_t i=0;i<j->count;++i) if(j->diagnostics[i].message) { cjeventCopy(&events[*count],&j->diagnostics[i]); if(!events[*count].message) okay=0; ++*count; }
  if(okay) { *seq=j->diagnosticSequence; yes=1; } else { for(size_t i=0;i<*count;++i) xideCEventClear(&events[i]); *count=0; }
 } LeaveCriticalSection(&j->lock); return yes;
}
int xideCAnalysisJobGuard(xide_c_completion_job* j,const char* path,xide_file_snapshot* out)
{
 if(!j || !path || !out) return 0;
 EnterCriticalSection(&j->lock); int yes=0;
 if(!cjcancelled(j) && j->state==XIDE_C_JOB_READY && !xideCBridgePrivatePath(j->bridge,path)) for(size_t i=0;i<j->guardCount;++i) if(j->guards[i].exists && xidePathLexicalEqual(path,j->guards[i].path)) { *out=j->guards[i].stamp; yes=1; break; }
 if(!yes && !cjcancelled(j) && j->state==XIDE_C_JOB_READY && !xideCBridgePrivatePath(j->bridge,path)) yes=xideCInputTraceSnapshot(j->trace,path,out);
 LeaveCriticalSection(&j->lock); return yes;
}

#endif /* XIDE_ENABLE_C_LSP */
