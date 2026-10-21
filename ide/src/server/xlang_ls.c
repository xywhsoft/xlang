/* stdio server. Native analysis and compatibility processes run off-owner. */
#include "protocol/lsp_wire.h"
#include "services/language_check.h"
#include "platform/path.h"
#include "analysis/worker.h"
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define DOC_LIMIT 64
#define TEXT_LIMIT (8u*1024u*1024u)
#define TOTAL_LIMIT (32u*1024u*1024u)
#define QUEUE_LIMIT (64u*1024u*1024u)
typedef struct message { struct message* next; xvalue* value; size_t size; } message;
typedef struct document { char *uri,*path; const char* text; xls_text* content; int version; unsigned long long generation,identity; } document;
typedef struct native_request { xvalue* id; xls_query* query; int document,version; unsigned long long identity,generation; } native_request;
typedef struct server {
 CRITICAL_SECTION lock; HANDLE reader,event; volatile LONG stop; message *head,*tail; size_t queueBytes,queueCount; int eof,readFailed;
 int initialized,shutdown,exitRequested,exitCode,writeFailed,checkOnSave,cliCompletion,cliDiagnostics; char *compiler,*directory; xide_toolchains* cache;
 document docs[DOC_LIMIT]; size_t textBytes; unsigned long long generation; xide_language_check* check; xvalue* checkId;
 int checkDocument,checkVersion,cancelCode,pending; unsigned long long checkGeneration; const char* cancelReason;
 int snapshotMode; unsigned long long checkBatchGeneration; xvalue* publishedGroups;
 xide_language_check* guard; xvalue* guardId; char* guardUri; int guardVersion,guardCancelled;
 xide_language_check* completion; xvalue *completionId,*completionPendingId; char* completionUri;
 int completionDocument,completionVersion,completionResponded,completionPublished,completionCancelled;
 unsigned long long completionGeneration,completionPendingGeneration;
 int completionPendingDocument,completionPendingEntry,completionPendingVersion,completionPendingLine,completionPendingCharacter;
 xls_worker *analysis,*semanticAnalysis; native_request native[DOC_LIMIT]; size_t foldingLimit;
 xls_session_stats analysisStats; uint64_t analysisThread;
} server;
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* o,const char* key) { return o && xrtValueType(o)==XVALUE_OBJECT ? xrtValueObjectGet(o,sv(key)) : NULL; }
static const char* string(const xvalue* v) { xstrview s={0}; return v && xrtValueGetString(v,&s) && !memchr(s.Data,0,s.Size) && s.Size<=INT_MAX && (!s.Size || MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.Data,(int)s.Size,NULL,0)) ? s.Data : NULL; }
static int put(xvalue* o,const char* key,xvalue* v) { return v && xrtValueObjectSetNew(o,sv(key),v); }
static char* copy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static int validText(const char* s) { size_t n=s ? strlen(s) : TEXT_LIMIT+1u; return n<=TEXT_LIMIT && (!n || MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,(int)n,NULL,0)); }
static int validPosition(const document* d,int line,int character)
{ size_t offset=0; return line>=0 && character>=0 && xlsTextOffset(d->content,(xls_position){(size_t)line,(size_t)character},&offset); }
static int validId(const xvalue* id) { return id && (xrtValueType(id)==XVALUE_INT || string(id)!=NULL); }
static int sameId(const xvalue* a,const xvalue* b) { int64 x=0,y=0; const char *s=string(a),*t=string(b); return s && t ? !strcmp(s,t) : xrtValueGetInt(a,&x) && xrtValueGetInt(b,&y) && x==y; }
static int integer(const xvalue* v,int* n) { int64 x=0; if(!v || !xrtValueGetInt(v,&x) || x<INT_MIN || x>INT_MAX) return 0; *n=(int)x; return 1; }
static int sendMessage(server* s,xvalue* value)
{
 size_t length=0; xide_model_error e={0}; char* bytes=xideLspFrame(value,&length,&e); xrtValueRelease(value);
 if(!bytes) { s->writeFailed=1; return 0; } size_t offset=0;
 while(offset<length) { DWORD written=0; size_t n=length-offset; if(n>16384) n=16384; if(!WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),bytes+offset,(DWORD)n,&written,NULL) || !written) { s->writeFailed=1; break; } offset+=written; }
 free(bytes); return !s->writeFailed;
}
static void response(server* s,const xvalue* id,xvalue* result,int code,const char* text)
{
 xvalue* out=xrtValueObject(); put(out,"jsonrpc",xrtValueString(sv("2.0"))); put(out,"id",id ? xrtValueDeepClone(id) : xrtValueNull());
 if(code) { xvalue* e=xrtValueObject(); put(e,"code",xrtValueInt(code)); put(e,"message",xrtValueString(sv(text))); put(out,"error",e); xrtValueRelease(result); }
 else put(out,"result",result ? result : xrtValueNull());
 sendMessage(s,out);
}
static void publishUri(server* s,const char* uri,const xvalue* version,xvalue* diagnostics)
{
 xvalue *out=xrtValueObject(),*p=xrtValueObject(); put(out,"jsonrpc",xrtValueString(sv("2.0"))); put(out,"method",xrtValueString(sv("textDocument/publishDiagnostics")));
 put(p,"uri",xrtValueString(sv(uri))); if(version) put(p,"version",xrtValueDeepClone(version)); put(p,"diagnostics",diagnostics ? diagnostics : xrtValueArray()); put(out,"params",p); sendMessage(s,out);
}
static void publish(server* s,document* d,xvalue* diagnostics)
{ xvalue* version=xrtValueInt(d->version); publishUri(s,d->uri,version,diagnostics); xrtValueRelease(version); }
static void clearPublished(server* s)
{
 if(s->guard) { s->guardCancelled=1; xideLanguageCheckCancel(s->guard); }
 if(!s->publishedGroups) return;
 for(size_t i=0;i<xrtValueCount(s->publishedGroups);++i) {
  const xvalue* group=xrtValueArrayGet(s->publishedGroups,i); const char* uri=string(get(group,"uri")); xvalue* version=NULL;
  for(int j=0;j<DOC_LIMIT;++j) if(s->docs[j].uri && !strcmp(s->docs[j].uri,uri)) { version=xrtValueInt(s->docs[j].version); break; }
  publishUri(s,uri,version,NULL); xrtValueRelease(version);
 }
 xrtValueRelease(s->publishedGroups); s->publishedGroups=NULL;
}
static int publishSnapshot(server* s,const xvalue* result)
{
 const xvalue* documents=get(result,"documents"); xvalue* groups=xrtValueArray(); if(!documents || !groups) { xrtValueRelease(groups); return 0; }
 for(size_t i=0;i<xrtValueCount(documents);++i) {
  const xvalue* d=xrtValueArrayGet(documents,i); const char* path=string(get(d,"path")); xide_model_error e={0}; char* uri=NULL;
  for(int j=0;j<DOC_LIMIT;++j) if(s->docs[j].uri && xidePathLexicalEqual(path,s->docs[j].path)) { uri=copy(s->docs[j].uri); break; }
  if(!uri) uri=xideLspPathToUri(path,&e);
  xvalue* g=xrtValueObject();
  int okay=uri && g && put(g,"uri",xrtValueString(sv(uri))) && (!get(d,"version") || put(g,"version",xrtValueDeepClone(get(d,"version")))) && xrtValueArrayAppend(groups,g); free(uri); xrtValueRelease(g);
  if(!okay) { xrtValueRelease(groups); return 0; }
 }
 clearPublished(s); s->publishedGroups=groups;
 for(size_t i=0;i<xrtValueCount(groups);++i) { const xvalue* g=xrtValueArrayGet(groups,i); publishUri(s,string(get(g,"uri")),get(g,"version"),xrtValueDeepClone(get(xrtValueArrayGet(documents,i),"diagnostics"))); }
 return !s->writeFailed;
}
static void cancel(server* s,int code,const char* reason)
{ if(s->check) { s->cancelCode=code; s->cancelReason=reason; xideLanguageCheckCancel(s->check); } }
static int find(server* s,const char* uri) { if(uri) for(int i=0;i<DOC_LIMIT;++i) if(s->docs[i].uri && !strcmp(s->docs[i].uri,uri)) return i; return -1; }
static void clearDocument(server* s,int i)
{ document* d=&s->docs[i]; s->textBytes-=xlsTextSize(d->content); free(d->uri); free(d->path); xlsTextRelease(d->content); memset(d,0,sizeof(*d)); }
static void releaseNative(native_request* request)
{ xlsQueryDestroy(request->query); xrtValueRelease(request->id); memset(request,0,sizeof(*request)); }
static void cancelNative(server* s,int documentIndex,int code,const char* reason)
{
 for(size_t i=0;i<DOC_LIMIT;++i) if(s->native[i].query && (documentIndex<0 || s->native[i].document==documentIndex)) {
  xlsQueryCancel(s->native[i].query); response(s,s->native[i].id,NULL,code,reason); releaseNative(&s->native[i]);
 }
}
static void nativeWake(void* user) { server* s=user; SetEvent(s->event); }
static void requestFolding(server* s,const xvalue* id,const xvalue* params)
{
 int index=find(s,string(get(get(params,"textDocument"),"uri"))),version=index>=0 ? s->docs[index].version : 0;
 const xvalue* supplied=get(params,"version"); const char* namedId=string(id);
 if(index<0 || (supplied && !integer(supplied,&version)) || version!=s->docs[index].version || (namedId && strlen(namedId)>65535)) { response(s,id,NULL,-32602,"Folding requires an open xlang document and a current supplied version."); return; }
 size_t slot=DOC_LIMIT;
 for(size_t i=0;i<DOC_LIMIT;++i) {
  if(s->native[i].query && sameId(s->native[i].id,id)) { xlsQueryCancel(s->native[i].query); releaseNative(&s->native[i]); response(s,id,NULL,-32600,"Request ID is already active."); return; }
  if(!s->native[i].query && slot==DOC_LIMIT) slot=i;
 }
 if(slot==DOC_LIMIT) { response(s,id,NULL,-32001,"The native analysis queue is full."); return; }
 xvalue* captured=xrtValueDeepClone(id); if(!captured) { response(s,id,NULL,-32603,"Unable to retain analysis request."); return; }
 xls_analysis_document inputs[DOC_LIMIT]; size_t count=0;
 for(size_t i=0;i<DOC_LIMIT;++i) if(s->docs[i].uri) { document* d=&s->docs[i]; inputs[count++]=(xls_analysis_document){d->uri,d->path,d->content,d->identity,(uint64_t)((int64_t)d->version-INT_MIN)}; }
 document* d=&s->docs[index]; xls_query* query=xlsWorkerFolding(s->analysis,inputs,count,d->uri,s->foldingLimit);
 if(!query || !xlsQuerySetWake(query,nativeWake,s)) { xlsQueryCancel(query); xlsQueryDestroy(query); xrtValueRelease(captured); response(s,id,NULL,-32001,"Unable to submit native analysis; its queue may be full."); return; }
 s->native[slot]=(native_request){captured,query,index,version,d->identity,d->generation};
}
static void pollNative(server* s)
{
 for(size_t i=0;i<DOC_LIMIT;++i) {
  native_request* request=&s->native[i]; if(!request->query) continue;
  xls_analysis_snapshot snapshot={0}; xls_session_stats stats={0}; uint64_t thread=0;
  enum xls_query_state state=xlsQueryResult(request->query,&snapshot,&stats,&thread);
  if(state==XLS_QUERY_PENDING) continue;
  document* document=&s->docs[request->document];
  if(state==XLS_QUERY_READY) { s->analysisStats=stats; s->analysisThread=thread; }
  if(!document->uri || document->identity!=request->identity || document->version!=request->version || document->generation!=request->generation) response(s,request->id,NULL,-32801,"Native analysis input is no longer current.");
  else if(state!=XLS_QUERY_READY) response(s,request->id,NULL,state==XLS_QUERY_CANCELLED ? -32800 : -32603,"Native analysis did not complete.");
  else {
   const xls_folding* folding=xlsQueryFolding(request->query); xvalue* result=xrtValueArray(); int okay=folding && result;
   const xls_fold_range* ranges=xlsFoldingRanges(folding);
   for(size_t j=0;okay && j<xlsFoldingCount(folding);++j) {
    xvalue* range=xrtValueObject(); okay=range && put(range,"startLine",xrtValueInt((int64)ranges[j].startLine)) && put(range,"endLine",xrtValueInt((int64)ranges[j].endLine)) && xrtValueArrayAppend(result,range); xrtValueRelease(range);
   }
   response(s,request->id,result,okay ? 0 : -32603,okay ? NULL : "Unable to encode folding ranges.");
  }
  xlsSyntaxRelease(snapshot.syntax); releaseNative(request);
 }
}
static void analysisStats(server* s,const xvalue* id)
{
 xls_worker_load load={0}; xlsWorkerLoad(s->analysis,&load); xvalue* result=xrtValueObject();
 put(result,"runtime",xrtValueString(sv("compiler-xrt"))); put(result,"parses",xrtValueInt((int64)s->analysisStats.parses)); put(result,"cacheHits",xrtValueInt((int64)s->analysisStats.cacheHits));
 put(result,"documents",xrtValueInt((int64)s->analysisStats.documents)); put(result,"textBytes",xrtValueInt((int64)s->analysisStats.textBytes));
 put(result,"workerThreadId",xrtValueInt((int64)s->analysisThread)); put(result,"ownerThreadId",xrtValueInt((int64)xrtThreadCurrentId()));
 put(result,"queued",xrtValueInt((int64)load.queued)); put(result,"running",xrtValueInt((int64)load.running)); put(result,"submitted",xrtValueInt((int64)load.submitted)); put(result,"completed",xrtValueInt((int64)load.completed)); put(result,"rejected",xrtValueInt((int64)load.rejected));
 response(s,id,result,0,NULL);
}
static void startCheck(server* s,int index,const xvalue* id,int snapshots)
{
 document* d=&s->docs[index]; xide_model_error e={0};
 if(s->check) { if(id) response(s,id,NULL,-32001,"A source check is still stopping; retry after its response."); else s->pending=index; return; }
 if(snapshots) {
  xide_language_analysis_options options={!s->cliDiagnostics,nativeWake,s};
  if(!s->cliDiagnostics) {
   xls_analysis_document inputs[DOC_LIMIT]; size_t count=0;
   for(int i=0;i<DOC_LIMIT;++i) if(s->docs[i].uri) { document* in=&s->docs[i]; inputs[count++]=(xls_analysis_document){in->uri,in->path,in->content,in->identity,(uint64_t)((int64_t)in->version-INT_MIN)}; }
   s->check=xideLanguageCheckStartRetained(s->cache,s->compiler,s->directory,s->semanticAnalysis,inputs,count,d->uri,NULL,0,0,&options,&e);
  } else {
   xide_language_snapshot inputs[DOC_LIMIT]; size_t count=0;
   for(int i=0;i<DOC_LIMIT;++i) if(s->docs[i].uri) inputs[count++]=(xide_language_snapshot){s->docs[i].path,s->docs[i].text,s->docs[i].version};
   s->check=xideLanguageCheckStartSnapshotsEx(s->cache,s->compiler,s->directory,d->path,inputs,count,&options,&e);
  }
 } else s->check=xideLanguageCheckStart(s->cache,s->compiler,s->directory,d->path,d->text,d->version,&e);
 if(!s->check) { if(id) response(s,id,NULL,-32602,*e.message ? e.message : "Unable to start source check."); else fprintf(stderr,"xlang-ls: %s\n",e.message); return; }
 s->snapshotMode=snapshots; s->checkBatchGeneration=s->generation; clearPublished(s);
 s->checkDocument=index; s->checkVersion=d->version; s->checkGeneration=d->generation; s->checkId=id ? xrtValueDeepClone(id) : NULL; s->cancelCode=0; s->cancelReason=NULL;
}
/* A cancelled guard is retired only once its worker has exited. At most one
   retained guard and one new check coexist; owner dispatch never waits on I/O. */
static void pollGuard(server* s)
{
 if(!s->guard) return;
 char reason[512]={0};
 if(!s->guardCancelled && xideLanguageCheckGuardState(s->guard,reason,sizeof(reason))==XIDE_GUARD_INVALID) {
  xvalue *out=xrtValueObject(),*p=xrtValueObject(); put(out,"jsonrpc",xrtValueString(sv("2.0"))); put(out,"method",xrtValueString(sv("xlang/diagnosticsInvalidated")));
  put(p,"checkId",xrtValueDeepClone(s->guardId)); put(p,"uri",xrtValueString(sv(s->guardUri))); put(p,"version",xrtValueInt(s->guardVersion)); put(p,"reason",xrtValueString(sv(reason))); put(out,"params",p); sendMessage(s,out); clearPublished(s);
 }
 if(s->guardCancelled && xideLanguageCheckWorkerFinished(s->guard)) {
  xideLanguageCheckDestroy(s->guard); s->guard=NULL; xrtValueRelease(s->guardId); s->guardId=NULL; free(s->guardUri); s->guardUri=NULL; s->guardCancelled=0;
 }
}
static void pollCheck(server* s)
{
 if(!s->check) return;
 char error[512]={0}; enum xide_check_state state=xideLanguageCheckState(s->check,error,sizeof(error)); if(state==XIDE_CHECK_RUNNING || s->guard) return;
 if(state!=XIDE_CHECK_READY && !xideLanguageCheckWorkerFinished(s->check)) return;
 if(state==XIDE_CHECK_READY && xideLanguageCheckGuardState(s->check,error,sizeof(error))==XIDE_GUARD_INVALID && !xideLanguageCheckWorkerFinished(s->check)) return;
 document* d=&s->docs[s->checkDocument]; int current=d->uri && d->generation==s->checkGeneration && d->version==s->checkVersion && (!s->snapshotMode || s->checkBatchGeneration==s->generation);
 if((s->cancelCode || !current) && !xideLanguageCheckWorkerFinished(s->check)) return;
 xvalue* result=xideLanguageCheckResult(s->check);
 if(s->cancelCode || !current || state==XIDE_CHECK_CANCELLED) { if(s->checkId) response(s,s->checkId,NULL,s->cancelCode ? s->cancelCode : -32801,s->cancelReason ? s->cancelReason : "Document content changed."); }
 else if(state==XIDE_CHECK_READY && result) {
  xvalue* list=get(result,"diagnostics"); int published=0;
  if(s->snapshotMode) {
   s->guardId=s->checkId ? xrtValueDeepClone(s->checkId) : xrtValueNull(); s->guardUri=copy(d->uri); s->guardVersion=d->version;
   published=s->guardId && s->guardUri && put(result,"checkId",xrtValueDeepClone(s->guardId)) && publishSnapshot(s,result);
   if(published) { s->guard=s->check; s->check=NULL; s->guardCancelled=0; }
   else { xrtValueRelease(s->guardId); s->guardId=NULL; free(s->guardUri); s->guardUri=NULL; }
  } else { publish(s,d,xrtValueDeepClone(list)); published=1; }
  if(!published) { if(s->checkId) response(s,s->checkId,NULL,-32603,"Unable to publish verified snapshot groups."); xrtValueRelease(result); result=NULL; }
  if(s->checkId && result) { put(result,"count",xrtValueInt((int64)xrtValueCount(list))); response(s,s->checkId,result,0,NULL); result=NULL; }
 } else { if(s->checkId) response(s,s->checkId,NULL,-32603,*error ? error : "Unable to obtain verified diagnostics."); else fprintf(stderr,"xlang-ls: %s\n",error); }
 xrtValueRelease(result);
 if(s->check && !xideLanguageCheckWorkerFinished(s->check)) { xideLanguageCheckCancel(s->check); s->guard=s->check; s->check=NULL; s->guardCancelled=1; }
 xideLanguageCheckDestroy(s->check); s->check=NULL; xrtValueRelease(s->checkId); s->checkId=NULL;
 if(s->pending>=0 && !s->shutdown && s->docs[s->pending].uri) { int i=s->pending; s->pending=-1; startCheck(s,i,NULL,0); } else s->pending=-1;
}
/* One cursor worker and one latest waiting request. Retire asynchronously. */
static void completionInvalidated(server* s,const char* reason)
{
 xvalue *out=xrtValueObject(),*p=xrtValueObject();
 put(out,"jsonrpc",xrtValueString(sv("2.0"))); put(out,"method",xrtValueString(sv("xlang/completionInvalidated")));
 put(p,"queryId",xrtValueDeepClone(s->completionId)); put(p,"uri",xrtValueString(sv(s->completionUri))); put(p,"version",xrtValueInt(s->completionVersion)); put(p,"reason",xrtValueString(sv(reason))); put(out,"params",p); sendMessage(s,out);
}
static void cancelCompletion(server* s,int code,const char* reason,int pending)
{
 if(s->completion && !s->completionCancelled) {
  if(!s->completionResponded) { response(s,s->completionId,NULL,code,reason); s->completionResponded=1; }
  else if(s->completionPublished) completionInvalidated(s,reason);
  s->completionCancelled=1; xideLanguageCheckCancel(s->completion);
 }
 if(pending && s->completionPendingId) { response(s,s->completionPendingId,NULL,code,reason); xrtValueRelease(s->completionPendingId); s->completionPendingId=NULL; }
}
static void startCompletion(server* s)
{
 if(s->completion || !s->completionPendingId || s->shutdown) return;
 int i=s->completionPendingDocument,entry=s->completionPendingEntry; document* d=&s->docs[i]; xide_model_error e={0};
 if(!d->uri || !s->docs[entry].uri || s->completionPendingGeneration!=s->generation || d->version!=s->completionPendingVersion) { response(s,s->completionPendingId,NULL,-32801,"Open-document cursor inputs changed before starting."); goto discard; }
 char* uri=copy(d->uri);
 if(!uri) { response(s,s->completionPendingId,NULL,-32603,"Unable to retain cursor input identity."); goto discard; }
 xide_language_completion_options options={!s->cliCompletion,nativeWake,s};
 if(!s->cliCompletion) {
  xls_analysis_document inputs[DOC_LIMIT]; size_t count=0;
  for(int j=0;j<DOC_LIMIT;++j) if(s->docs[j].uri) { document* in=&s->docs[j]; inputs[count++]=(xls_analysis_document){in->uri,in->path,in->content,in->identity,(uint64_t)((int64_t)in->version-INT_MIN)}; }
  s->completion=xideLanguageCheckStartRetained(s->cache,s->compiler,s->directory,NULL,inputs,count,s->docs[entry].uri,d->path,s->completionPendingLine,s->completionPendingCharacter,&options,&e);
 } else {
  xide_language_snapshot inputs[DOC_LIMIT]; size_t count=0;
  for(int j=0;j<DOC_LIMIT;++j) if(s->docs[j].uri) inputs[count++]=(xide_language_snapshot){s->docs[j].path,s->docs[j].text,s->docs[j].version};
  s->completion=xideLanguageCheckStartCompletionSnapshotsEx(s->cache,s->compiler,s->directory,s->docs[entry].path,inputs,count,d->path,s->completionPendingLine,s->completionPendingCharacter,&options,&e);
 }
 if(!s->completion) { free(uri); response(s,s->completionPendingId,NULL,-32603,*e.message ? e.message : "Unable to start cursor query."); goto discard; }
 s->completionUri=uri; s->completionId=s->completionPendingId; s->completionPendingId=NULL;
 s->completionDocument=i; s->completionVersion=d->version; s->completionGeneration=s->generation;
 s->completionResponded=s->completionPublished=s->completionCancelled=0; return;
discard: xrtValueRelease(s->completionPendingId); s->completionPendingId=NULL;
}
static void requestCompletion(server* s,const xvalue* id,const xvalue* params)
{
 int i=find(s,string(get(get(params,"textDocument"),"uri"))),entry=i,line=0,character=0,version=i>=0 ? s->docs[i].version : 0;
 const xvalue *requested=get(params,"version"),*entryDocument=get(params,"entryDocument"),*context=get(params,"context");
 const char* requestName=string(id); if(requestName && strlen(requestName)>65535) { response(s,id,NULL,-32602,"Completion request ID exceeds 64 KiB."); return; }
 if(entryDocument) entry=find(s,string(get(entryDocument,"uri")));
 if(i<0 || entry<0 || (requested && !integer(requested,&version)) || version!=s->docs[i].version || !integer(get(get(params,"position"),"line"),&line) || !integer(get(get(params,"position"),"character"),&character) || !validPosition(&s->docs[i],line,character)) { response(s,id,NULL,-32602,"Completion requires an open xlang document, a current version and a valid UTF-16 scalar position."); return; }
 if(i>=0 && xlsTextSize(s->docs[i].content)>=3 && !memcmp(s->docs[i].text,"\xef\xbb\xbf",3) && line==0 && character==0) { response(s,id,NULL,-32602,"Position precedes the source BOM."); return; }
 if(context) { int trigger=0; const xvalue* raw=get(context,"triggerCharacter"); const char* characterText=string(raw); if(!integer(get(context,"triggerKind"),&trigger) || trigger<1 || trigger>3 || (raw && (!characterText || !*characterText || strlen(characterText)>8)) || (trigger==2 && !characterText)) { response(s,id,NULL,-32602,"Completion context is invalid."); return; } }
 xvalue* captured=xrtValueDeepClone(id); if(!captured) { response(s,id,NULL,-32603,"Unable to retain completion request."); return; }
 cancelCompletion(s,-32800,"Cursor query superseded by a newer request.",1);
 s->completionPendingId=captured; s->completionPendingDocument=i; s->completionPendingEntry=entry; s->completionPendingVersion=version; s->completionPendingLine=line; s->completionPendingCharacter=character; s->completionPendingGeneration=s->generation;
 startCompletion(s);
}
static void pollCompletion(server* s)
{
 if(s->completion) {
  char error[512]={0}; enum xide_check_state state=xideLanguageCheckState(s->completion,error,sizeof(error));
  if(!s->completionCancelled && (s->completionGeneration!=s->generation || !s->docs[s->completionDocument].uri || s->docs[s->completionDocument].version!=s->completionVersion)) cancelCompletion(s,-32801,"Cursor input set is no longer current.",0);
  if(!s->completionCancelled && xideLanguageCheckGuardState(s->completion,error,sizeof(error))==XIDE_GUARD_INVALID) cancelCompletion(s,-32801,*error ? error : "Cursor source guard invalidated.",0);
  if(!s->completionCancelled && !s->completionResponded && state!=XIDE_CHECK_RUNNING) {
   xvalue* result=xideLanguageCheckResult(s->completion);
   if(state==XIDE_CHECK_READY && result && put(result,"queryId",xrtValueDeepClone(s->completionId))) { response(s,s->completionId,result,0,NULL); result=NULL; s->completionPublished=1; }
   else { response(s,s->completionId,NULL,state==XIDE_CHECK_CANCELLED ? -32800 : -32603,*error ? error : "Unable to obtain verified cursor facts."); s->completionCancelled=1; xideLanguageCheckCancel(s->completion); }
   xrtValueRelease(result); s->completionResponded=1;
  }
  if(s->completionCancelled && xideLanguageCheckWorkerFinished(s->completion)) {
   xideLanguageCheckDestroy(s->completion); s->completion=NULL; xrtValueRelease(s->completionId); s->completionId=NULL; free(s->completionUri); s->completionUri=NULL; s->completionCancelled=s->completionResponded=s->completionPublished=0;
  }
 }
 startCompletion(s);
}
static int receive(const xvalue* value,void* user)
{
 server* s=user; message* m=calloc(1,sizeof(*m)); size_t n=0; char* encoded=xrtJsonStringify(value,false,&n); xrtFree(encoded);
 if(!m || !n || !(m->value=xrtValueDeepClone(value))) { if(m) { xrtValueRelease(m->value); free(m); } return 0; } m->size=n;
 EnterCriticalSection(&s->lock); int okay=s->queueCount<256 && n<=QUEUE_LIMIT-s->queueBytes;
 if(okay) { if(s->tail) s->tail->next=m; else s->head=m; s->tail=m; s->queueBytes+=n; ++s->queueCount; SetEvent(s->event); }
 LeaveCriticalSection(&s->lock); if(!okay) { xrtValueRelease(m->value); free(m); } return okay;
}
static DWORD WINAPI reader(void* user)
{
 server* s=user; xide_lsp_stream stream={0}; xide_model_error e={0}; int okay=1; char bytes[16384]; DWORD n=0;
 while(!InterlockedCompareExchange(&s->stop,0,0) && ReadFile(GetStdHandle(STD_INPUT_HANDLE),bytes,sizeof(bytes),&n,NULL) && n) if(!xideLspStreamFeed(&stream,bytes,n,receive,s,&e)) { okay=0; break; }
 if(!InterlockedCompareExchange(&s->stop,0,0) && !xideLspStreamEnd(&stream,&e)) okay=0;
 xideLspStreamClear(&stream); if(!okay) fprintf(stderr,"xlang-ls: %s\n",e.message);
 EnterCriticalSection(&s->lock); s->eof=1; s->readFailed=!okay; SetEvent(s->event); LeaveCriticalSection(&s->lock); return 0;
}
static void documents(server* s,const char* method,const xvalue* params)
{
 const xvalue* item=get(params,"textDocument"); const char* uri=string(get(item,"uri")); int i=find(s,uri);
 if(!strcmp(method,"textDocument/didOpen")) {
  const char *text=string(get(item,"text")),*language=string(get(item,"languageId")); int version=0;
  if(!uri || i>=0 || !language || strcmp(language,"xlang") || !validText(text) || !integer(get(item,"version"),&version) || strlen(text)>TOTAL_LIMIT-s->textBytes) goto invalid;
  for(i=0;i<DOC_LIMIT && s->docs[i].uri;++i) {} if(i==DOC_LIMIT) goto invalid;
  xide_model_error e={0}; char* path=xideLspUriToPath(uri,&e); if(!path || !xidePathHasExtension(path,".xl")) { free(path); goto invalid; }
  for(int j=0;j<DOC_LIMIT;++j) if(s->docs[j].uri && (xidePathLexicalEqual(path,s->docs[j].path) || xidePathEqual(path,s->docs[j].path))) { free(path); goto invalid; }
  xls_text* content=xlsTextCreate(text,strlen(text),TEXT_LIMIT,NULL); char* name=copy(uri);
  if(!content || !name) { free(path); xlsTextRelease(content); free(name); goto invalid; }
  unsigned long long generation=++s->generation;
  s->docs[i]=(document){.uri=name,.path=path,.text=xlsTextData(content),.content=content,.version=version,.generation=generation,.identity=generation}; s->textBytes+=xlsTextSize(content); cancelCompletion(s,-32801,"Open-document cursor input set changed.",1); if(s->check && s->snapshotMode) cancel(s,-32801,"Open-document snapshot set changed."); clearPublished(s); return;
 }
 if(i<0) goto invalid;
 document* d=&s->docs[i];
 if(!strcmp(method,"textDocument/didClose")) { cancelNative(s,i,-32800,"Document closed."); cancelCompletion(s,-32800,"Document closed.",1); if(s->check && (s->snapshotMode || s->checkDocument==i)) cancel(s,-32800,"Document closed."); if(s->pending==i) s->pending=-1; clearPublished(s); publish(s,d,NULL); ++s->generation; clearDocument(s,i); return; }
 if(!strcmp(method,"textDocument/didChange")) {
  const xvalue* changes=get(params,"contentChanges"); int version=0;
  if(!integer(get(item,"version"),&version) || version<=d->version || !changes || xrtValueType(changes)!=XVALUE_ARRAY || xrtValueCount(changes)>1024) goto invalid;
  xarray edits; if(!xrtArrayInit(&edits,sizeof(xls_text_edit))) goto invalid;
  int okay=1;
  for(size_t j=0;j<xrtValueCount(changes) && okay;++j) {
   const xvalue* change=xrtValueArrayGet(changes,j); const char* text=string(get(change,"text"));
   const xvalue *range=get(change,"range"),*length=get(change,"rangeLength");
   xls_text_edit edit={.text=text,.length=text ? strlen(text) : 0,.hasRange=range!=NULL,.hasRangeLength=length!=NULL};
   if(!text) { okay=0; break; }
   if(range) {
    const xvalue *start=get(range,"start"),*end=get(range,"end"); int sl=0,sc=0,el=0,ec=0;
    okay=integer(get(start,"line"),&sl) && integer(get(start,"character"),&sc) && integer(get(end,"line"),&el) && integer(get(end,"character"),&ec) && sl>=0 && sc>=0 && el>=0 && ec>=0;
    if(okay) { edit.start=(xls_position){(size_t)sl,(size_t)sc}; edit.end=(xls_position){(size_t)el,(size_t)ec}; }
   }
   if(length) { int units=0; okay=okay && integer(length,&units) && units>=0; if(okay) edit.rangeLength=(size_t)units; }
   if(okay) okay=xrtArrayPush(&edits,&edit);
  }
  xls_text* content=okay ? xlsTextApply(d->content,(const xls_text_edit*)edits.Data,edits.Count,TEXT_LIMIT,NULL) : NULL;
  xrtArrayUnit(&edits);
  if(!content) goto invalid;
  if(xlsTextSize(content)>TOTAL_LIMIT-(s->textBytes-xlsTextSize(d->content))) { xlsTextRelease(content); goto invalid; }
  if(s->check && (s->snapshotMode || s->checkDocument==i)) cancel(s,-32801,"Document content changed.");
  cancelCompletion(s,-32801,"Open-document cursor content changed.",1);
  cancelNative(s,i,-32801,"Native analysis document changed.");
  s->textBytes=s->textBytes-xlsTextSize(d->content)+xlsTextSize(content); xlsTextRelease(d->content); d->content=content; d->text=xlsTextData(content); d->version=version; d->generation=++s->generation; clearPublished(s); publish(s,d,NULL); return;
 }
 if(!strcmp(method,"textDocument/didSave")) {
  const xvalue* supplied=get(params,"text"); const char* text=string(supplied); if(supplied && (!text || strcmp(text,d->text))) goto invalid;
  if(s->checkOnSave) startCheck(s,i,NULL,0);
  return;
 }
invalid: fprintf(stderr,"xlang-ls: rejected %s notification.\n",method);
}
static void semanticQuery(server* s,const xvalue* id,const xvalue* params,int query)
{
 int i=find(s,string(get(get(params,"textDocument"),"uri"))),version=i>=0 ? s->docs[i].version : 0,line=0,character=0; const xvalue* batch=get(params,"checkId"),*suppliedVersion=get(params,"version");
 if(i<0 || (suppliedVersion && !integer(suppliedVersion,&version)) || version!=s->docs[i].version || (query!=2 && (!integer(get(get(params,"position"),"line"),&line) || line<0 || !integer(get(get(params,"position"),"character"),&character) || character<0)) || (batch && !validId(batch))) { response(s,id,NULL,-32602,"Open document and valid UTF-16 position are required; supplied version/checkId must be current."); return; }
 if(query!=2 && !validPosition(&s->docs[i],line,character)) { response(s,id,NULL,-32602,"Position is not a UTF-16 scalar boundary."); return; }
 if(!s->guard || s->guardCancelled || (batch && !sameId(batch,s->guardId))) { response(s,id,NULL,-32801,"Checked compilation is no longer current."); return; }
 xvalue* result=query==2 ? xideLanguageCheckDocumentSymbols(s->guard,s->docs[i].path,version) : query==1 ? xideLanguageCheckHover(s->guard,s->docs[i].path,version,line,character) : xideLanguageCheckDefinition(s->guard,s->docs[i].path,version,line,character);
 if(!result) response(s,id,NULL,-32801,"No current semantic index; run a successful source check first.");
 else response(s,id,result,0,NULL);
}
static void dispatch(server* s,const xvalue* message)
{
 const xvalue* id=get(message,"id"); const char *rpc=string(get(message,"jsonrpc")),*method=string(get(message,"method")); const xvalue* params=get(message,"params");
 if(!rpc || strcmp(rpc,"2.0") || !method || (id && !validId(id))) { response(s,NULL,NULL,-32600,"Invalid JSON-RPC request."); return; }
 if(!strcmp(method,"exit")) { if(!id) { s->exitRequested=1; s->exitCode=s->shutdown ? 0 : 1; } else response(s,id,NULL,-32600,"exit must be a notification."); return; }
 if(!strcmp(method,"initialize")) {
  if(!id || s->initialized || s->shutdown || !params || xrtValueType(params)!=XVALUE_OBJECT) { if(id) response(s,id,NULL,-32600,"Invalid or duplicate initialize request."); return; }
  xvalue* enc=get(get(params,"capabilities"),"general"); enc=get(enc,"positionEncodings"); int utf16=!enc;
  if(enc && xrtValueType(enc)==XVALUE_ARRAY) for(size_t i=0;i<xrtValueCount(enc);++i) { const char* name=string(xrtValueArrayGet(enc,i)); if(name && !strcmp(name,"utf-16")) utf16=1; }
  if(!utf16) { response(s,id,NULL,-32602,"UTF-16 position encoding is required."); return; }
  const xvalue* options=get(params,"initializationOptions"); bool check=true; const xvalue* enabled=get(options,"checkOnSave"); if(enabled && !xrtValueGetBool(enabled,&check)) { response(s,id,NULL,-32602,"checkOnSave must be boolean."); return; }
  const xvalue* backendOption=get(options,"completionBackend"); const char* backend=string(backendOption);
  if(backendOption && (!backend || (strcmp(backend,"native") && strcmp(backend,"cli")))) { response(s,id,NULL,-32602,"completionBackend must be native or cli."); return; }
  s->cliCompletion=backend && !strcmp(backend,"cli");
  const xvalue* diagnosticsOption=get(options,"diagnosticsBackend"); const char* diagnostics=string(diagnosticsOption);
  if(diagnosticsOption && (!diagnostics || (strcmp(diagnostics,"native") && strcmp(diagnostics,"cli")))) { response(s,id,NULL,-32602,"diagnosticsBackend must be native or cli."); return; }
  s->cliDiagnostics=diagnostics && !strcmp(diagnostics,"cli");
  const xvalue* rangeLimit=get(get(get(get(params,"capabilities"),"textDocument"),"foldingRange"),"rangeLimit"); int64 limit=65536;
  if(rangeLimit && (!xrtValueGetInt(rangeLimit,&limit) || limit<0 || limit>UINT32_MAX)) { response(s,id,NULL,-32602,"Folding rangeLimit must be a nonnegative 32-bit integer."); return; }
  s->foldingLimit=(size_t)(limit>65536 ? 65536 : limit);
  xvalue *result=xrtValueObject(),*cap=xrtValueObject(),*sync=xrtValueObject(),*save=xrtValueObject(),*info=xrtValueObject(),*scope=xrtValueObject();
  put(cap,"positionEncoding",xrtValueString(sv("utf-16"))); put(sync,"openClose",xrtValueBool(true)); put(sync,"change",xrtValueInt(2)); put(save,"includeText",xrtValueBool(true)); put(sync,"save",save); put(cap,"textDocumentSync",sync);
  put(cap,"foldingRangeProvider",xrtValueBool(true)); put(scope,"residentSyntax",xrtValueBool(true)); put(scope,"analysisRuntime",xrtValueString(sv("compiler-xrt")));
  put(scope,"nativeCursorCompletion",xrtValueBool(!s->cliCompletion)); put(scope,"completionBackend",xrtValueString(sv(s->cliCompletion ? "cli" : "native")));
  put(scope,"nativeSnapshotDiagnostics",xrtValueBool(!s->cliDiagnostics)); put(scope,"diagnosticsBackend",xrtValueString(sv(s->cliDiagnostics ? "cli" : "native")));
  put(scope,"retainedSemanticChecks",xrtValueBool(!s->cliDiagnostics)); put(scope,"retainedNativeInputs",xrtValueBool(!s->cliDiagnostics || !s->cliCompletion));
  put(scope,"savedDiagnosticsBackend",xrtValueString(sv("cli")));
  put(scope,"savedEntryDiagnostics",xrtValueBool(true)); put(scope,"unsavedDependencies",xrtValueBool(true)); put(scope,"snapshotDiagnostics",xrtValueBool(true)); put(scope,"snapshotDocumentFingerprint",xrtValueBool(true)); put(scope,"snapshotInputGuard",xrtValueString(sv("background-poll"))); put(scope,"cSemantics",xrtValueBool(false)); put(scope,"snapshotDefinition",xrtValueBool(true)); put(cap,"definitionProvider",xrtValueBool(true)); put(cap,"hoverProvider",xrtValueBool(true)); put(cap,"documentSymbolProvider",xrtValueBool(true)); xvalue *completion=xrtValueObject(),*triggers=xrtValueArray(),*dot=xrtValueString(sv(".")); xrtValueArrayAppend(triggers,dot); xrtValueRelease(dot); put(completion,"resolveProvider",xrtValueBool(false)); put(completion,"triggerCharacters",triggers); put(cap,"completionProvider",completion); put(scope,"cursorCompletion",xrtValueBool(true)); put(scope,"completionInputGuard",xrtValueString(sv("background-poll"))); put(scope,"snapshotHover",xrtValueBool(true)); put(scope,"snapshotDocumentSymbols",xrtValueBool(true)); put(cap,"experimental",scope); put(result,"capabilities",cap);
  put(info,"name",xrtValueString(sv("xlang-ls"))); put(info,"version",xrtValueString(sv("0.9"))); put(result,"serverInfo",info); s->initialized=1; s->checkOnSave=check; response(s,id,result,0,NULL); return;
 }
 if(!s->initialized) { if(id) response(s,id,NULL,-32002,"Server not initialized."); return; }
 if(s->shutdown) { if(id) response(s,id,NULL,-32600,"Server has shut down."); return; }
 if(!strcmp(method,"shutdown")) { if(id) { s->shutdown=1; s->pending=-1; cancelNative(s,-1,-32800,"Server shutting down."); clearPublished(s); cancel(s,-32800,"Server shutting down."); cancelCompletion(s,-32800,"Server shutting down.",1); response(s,id,NULL,0,NULL); } return; }
 if(!strcmp(method,"initialized")) { if(id) response(s,id,NULL,-32600,"initialized must be a notification."); return; }
 if(!strcmp(method,"$/cancelRequest")) {
  if(id) response(s,id,NULL,-32600,"Cancellation must be a notification.");
  else {
   const xvalue* cancelledId=get(params,"id");
   for(size_t i=0;i<DOC_LIMIT;++i) if(s->native[i].query && sameId(s->native[i].id,cancelledId)) { xlsQueryCancel(s->native[i].query); response(s,s->native[i].id,NULL,-32800,"Request cancelled."); releaseNative(&s->native[i]); }
   if(s->checkId && sameId(s->checkId,cancelledId)) cancel(s,-32800,"Request cancelled.");
   if(s->completionId && sameId(s->completionId,cancelledId)) cancelCompletion(s,-32800,"Request cancelled.",0);
   if(s->completionPendingId && sameId(s->completionPendingId,cancelledId)) { response(s,s->completionPendingId,NULL,-32800,"Request cancelled."); xrtValueRelease(s->completionPendingId); s->completionPendingId=NULL; }
  }
  return;
 }
 if(!strcmp(method,"textDocument/foldingRange")) { if(id) requestFolding(s,id,params); return; }
 if(!strcmp(method,"xlang/analysisStats")) { if(id) analysisStats(s,id); return; }
 if(!strcmp(method,"textDocument/completion")) { if(id) requestCompletion(s,id,params); return; }
 if(!strcmp(method,"textDocument/definition")) { if(id) semanticQuery(s,id,params,0); return; }
 if(!strcmp(method,"textDocument/hover")) { if(id) semanticQuery(s,id,params,1); return; }
 if(!strcmp(method,"textDocument/documentSymbol")) { if(id) semanticQuery(s,id,params,2); return; }
 if(!strcmp(method,"xlang/checkSaved") || !strcmp(method,"xlang/check")) {
  int i=find(s,string(get(get(params,"textDocument"),"uri"))),version=0;
  if(!id) return;
  if(i<0 || !integer(get(params,"version"),&version) || version!=s->docs[i].version) { response(s,id,NULL,-32602,"Named open xlang document and its current version are required."); return; }
  startCheck(s,i,id,!strcmp(method,"xlang/check")); return;
 }
 if(!strcmp(method,"textDocument/didOpen") || !strcmp(method,"textDocument/didChange") || !strcmp(method,"textDocument/didSave") || !strcmp(method,"textDocument/didClose")) { if(id) response(s,id,NULL,-32600,"Document lifecycle messages must be notifications."); else documents(s,method,params); return; }
 if(id) response(s,id,NULL,-32601,"Method not implemented.");
}
static char* utf8(const wchar_t* value) { int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value,-1,NULL,0,NULL,NULL); char* p=n ? malloc((size_t)n) : NULL; if(p) WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value,-1,p,n,NULL,NULL); return p; }
int main(void)
{
 server s={0}; s.pending=-1; s.exitCode=1; int argc=0; wchar_t** argv=CommandLineToArgvW(GetCommandLineW(),&argc);
 if(argc!=3 || !argv || wcscmp(argv[1],L"--compiler")) { fputs("Usage: xlang-ls --compiler absolute-xl.exe\n",stderr); if(argv) LocalFree(argv); return 2; }
 char* compiler=utf8(argv[2]); LocalFree(argv); s.compiler=compiler ? xidePathResolveLexical(NULL,compiler) : NULL; free(compiler);
 wchar_t cwd[32768]; if(GetCurrentDirectoryW(32768,cwd)) s.directory=utf8(cwd);
 s.cache=xideToolchainsCreate(); s.analysis=xlsWorkerCreate(NULL,DOC_LIMIT); s.semanticAnalysis=xlsWorkerCreate(NULL,8); InitializeCriticalSection(&s.lock); s.event=CreateEventW(NULL,FALSE,FALSE,NULL);
 if(s.compiler && s.directory && s.cache && s.analysis && s.semanticAnalysis && s.event) s.reader=CreateThread(NULL,0,reader,&s,0,NULL);
 if(s.reader) while(!s.exitRequested && !s.writeFailed) {
  EnterCriticalSection(&s.lock); message* m=s.head; if(m) { s.head=m->next; if(!s.head) s.tail=NULL; s.queueBytes-=m->size; --s.queueCount; } int eof=s.eof,failed=s.readFailed; LeaveCriticalSection(&s.lock);
  if(m) { dispatch(&s,m->value); xrtValueRelease(m->value); free(m); } else if(eof) { s.exitCode=failed ? 2 : 1; break; }
  pollGuard(&s); pollCheck(&s); pollCompletion(&s); pollNative(&s);
  if(!m) {
   HANDLE waits[4]={s.event}; DWORD count=1;
   if(s.completionCancelled) { HANDLE h=xideLanguageCheckWorkerSignal(s.completion); if(h) waits[count++]=h; }
   if(s.guardCancelled) { HANDLE h=xideLanguageCheckWorkerSignal(s.guard); if(h) waits[count++]=h; }
   if(s.check) { HANDLE h=xideLanguageCheckWorkerSignal(s.check); if(h) waits[count++]=h; }
   /* A retired guard can exit after pollCompletion. Wake on the real thread
      signal, so a new query never waits for the 10 ms fallback timer. */
   WaitForMultipleObjects(count,waits,FALSE,10);
  }
 }
 InterlockedExchange(&s.stop,1);
 if(s.reader) {
  /* A read can begin after a one-shot CancelSynchronousIo sees no pending I/O.
   * Retry until the reader observes stop; clients need not close their stdin. */
  do { CancelSynchronousIo(s.reader); } while(WaitForSingleObject(s.reader,20)==WAIT_TIMEOUT);
  CloseHandle(s.reader);
 }
 for(size_t i=0;i<DOC_LIMIT;++i) if(s.native[i].query) { xlsQueryCancel(s.native[i].query); releaseNative(&s.native[i]); }
 xlsWorkerDestroy(s.analysis);
 xideLanguageCheckDestroy(s.completion); xrtValueRelease(s.completionId); xrtValueRelease(s.completionPendingId); free(s.completionUri); xideLanguageCheckDestroy(s.check); xideLanguageCheckDestroy(s.guard); xlsWorkerDestroy(s.semanticAnalysis); xrtValueRelease(s.guardId); free(s.guardUri); xrtValueRelease(s.checkId); xrtValueRelease(s.publishedGroups); xideToolchainsDestroy(s.cache);
 while(s.head) { message* m=s.head; s.head=m->next; xrtValueRelease(m->value); free(m); }
 for(int i=0;i<DOC_LIMIT;++i) clearDocument(&s,i);
 if(s.event) { CloseHandle(s.event); } DeleteCriticalSection(&s.lock); free(s.compiler); free(s.directory); return s.writeFailed ? 2 : s.exitCode;
}
