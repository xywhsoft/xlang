/* stdio server. Native analysis and compatibility processes run off-owner. */
#include "protocol/lsp_wire.h"
#include "protocol/input_state.h"
#include "services/language_check.h"
#include "services/diagnostic_store.h"
#include "services/analysis_catalog.h"
#include "platform/path.h"
#include "analysis/worker.h"
#ifdef XLS_COMPILER_XRT
#include "analysis/worker_modules.h"
#include "analysis/entity_store.h"
#include "services/project_references.h"
#endif
#include "protocol/signature_help.h"
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
#define INVALIDATION_LIMIT 128u
typedef struct message { struct message* next; xvalue* value; size_t size; int input; } message;
typedef struct document { char *uri,*path; const char* text; xls_text* content; int version; unsigned long long generation,identity; unsigned char uriHash[32];xls_context* context;int binding; } document;
typedef struct native_request { xvalue* id; xls_query* query; int document,version; unsigned long long identity,generation; } native_request;
typedef struct semantic_request {
 xvalue* id; int document,version,line,character,kind,started,bound;
 unsigned long long identity,generation,order;
} semantic_request;
typedef struct server {
 CRITICAL_SECTION lock; HANDLE reader,event; volatile LONG stop; message *head,*tail; size_t queueBytes,queueCount,queueInputs; int eof,readFailed;
 int initialized,shutdown,exitRequested,exitCode,writeFailed,checkOnSave,cliCompletion,cliDiagnostics,definitionInputProof,referenceInputProof; char *compiler,*directory; xide_toolchains* cache;
 document docs[DOC_LIMIT]; size_t textBytes; unsigned long long generation; xide_language_check* check; xvalue* checkId;
 int checkDocument,checkVersion,cancelCode,pending; unsigned long long checkGeneration; const char* cancelReason;
 int snapshotMode; unsigned long long checkBatchGeneration; xvalue* publishedGroups;
 xide_language_check* guard; xvalue* guardId; char* guardUri; int guardVersion,guardCancelled;
 xide_language_check* completion; xvalue *completionId,*completionPendingId; char* completionUri;
 int completionDocument,completionVersion,completionResponded,completionPublished,completionCancelled;
 unsigned long long completionGeneration,completionPendingGeneration;
 int completionPendingDocument,completionPendingEntry,completionPendingVersion,completionPendingLine,completionPendingCharacter;
 int completionContextDocument;
 xide_language_check* signature; xvalue *signatureId,*signaturePendingId; char* signatureUri;
 int signatureDocument,signatureVersion,signatureResponded,signaturePublished,signatureCancelled;
 unsigned long long signatureGeneration,signaturePendingGeneration;
 int signaturePendingDocument,signaturePendingEntry,signaturePendingVersion,signaturePendingLine,signaturePendingCharacter;
 int signatureContextDocument;
 unsigned signatureFlags;
 xls_worker *analysis,*semanticAnalysis,*cursorAnalysis,*signatureAnalysis; native_request native[DOC_LIMIT]; size_t foldingLimit;
 xls_session_stats analysisStats; uint64_t analysisThread;
 semantic_request semantic[DOC_LIMIT]; unsigned long long semanticOrder; int checkForSemantic;
 xide_diagnostic_store* diagnostics; xide_diagnostic_query* diagnosticQuery; xide_diagnostic_publication* diagnosticPublication;
 int automaticDiagnostics,diagnosticVersions,diagnosticInputProof,diagnosticDependencyProof,diagnosticOverlayProof,diagnosticDelay,checkForDiagnostics,diagnosticKind,diagnosticRoot;
 xvalue* diagnosticOverlayState;unsigned long long diagnosticOverlayGeneration,inputSerial;
 int diagnosticNeedsSnapshot,diagnosticReset,diagnosticBindingsChanged,diagnosticFailed[DOC_LIMIT];
 unsigned long long diagnosticJobGeneration,diagnosticJobIdentity,diagnosticOrder[DOC_LIMIT],diagnosticNextOrder;
 uint64_t diagnosticDue[DOC_LIMIT],diagnosticSweepAt,diagnosticRetryAt,diagnosticLogAt;
 char* diagnosticPaths[INVALIDATION_LIMIT]; size_t diagnosticPathCount,diagnosticPathBytes;
 xvalue *diagnosticResult,*diagnosticAnalysis; xide_language_input_guard* diagnosticProof;
 int diagnosticResultRoot; unsigned long long diagnosticResultIdentity,diagnosticResultGeneration;
 uint64_t diagnosticChecks,diagnosticOperations,diagnosticWorkerThread; size_t diagnosticRecords; char diagnosticError[512];
#ifdef XLS_COMPILER_XRT
 xls_dependency_stats diagnosticGraph;uint64_t diagnosticGraphFallbacks;int diagnosticGraphIndexed;
 xls_module_store* modules;
 xide_project_references* references;
 struct {xide_reference_query* query;xvalue* id;int document,version;unsigned long long identity,serial;int64 epoch;} referenceRequests[8];
 xide_project_reference_stats referenceStats;
#endif
 xide_analysis_catalog* catalog;int64 configurationEpoch;
} server;
static void cancelSemantic(server*,int,int,const char*);
static void semanticCheckFinished(server*,int,unsigned long long,int,int,const char*,int);
static void drainSemantic(server*);
static void diagnosticInput(server*,int,const char*,int);
static int diagnosticCapture(server*,int,xide_language_check*,xvalue**,int);
static void diagnosticCheckFailed(server*,int,int,const char*);
static void diagnosticStop(server*);
static int configureAnalysis(server*,const xvalue*,xide_model_error*);
static xvalue* configurationStamp(const server*,int);
static xvalue* diagnosticEnvelope(server*,const xvalue*,int,const char*);
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* o,const char* key) { return o && xrtValueType(o)==XVALUE_OBJECT ? xrtValueObjectGet(o,sv(key)) : NULL; }
static const char* string(const xvalue* v) { xstrview s={0}; return v && xrtValueGetString(v,&s) && !memchr(s.Data,0,s.Size) && s.Size<=INT_MAX && (!s.Size || MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.Data,(int)s.Size,NULL,0)) ? s.Data : NULL; }
static int put(xvalue* o,const char* key,xvalue* v) { return v && xrtValueObjectSetNew(o,sv(key),v); }
static xls_context* bindAnalysis(const xide_analysis_catalog* catalog,const char* path,int* state)
{
 *state=XIDE_ANALYSIS_UNOWNED;if(!catalog) return NULL;
 const xls_context* context=NULL;*state=xideAnalysisCatalogFind(catalog,path,&context);
 if(*state==XIDE_ANALYSIS_PROJECT) return xlsContextRetain((xls_context*)context);
 if(*state==XIDE_ANALYSIS_UNOWNED) {xls_context* file=xideAnalysisCatalogFile(catalog,path);if(!file) *state=XIDE_ANALYSIS_ERROR;return file;}
 return NULL;
}
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
 put(p,"uri",xrtValueString(sv(uri))); if(version && (!s->automaticDiagnostics || s->diagnosticVersions)) put(p,"version",xrtValueDeepClone(version)); put(p,"diagnostics",diagnostics ? diagnostics : xrtValueArray());
 if(s->automaticDiagnostics && s->diagnosticInputProof && !put(p,"xlang",diagnosticEnvelope(s,NULL,1,NULL))) s->writeFailed=1;
 put(out,"params",p); if(!s->writeFailed) sendMessage(s,out); else xrtValueRelease(out);
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
{ document* d=&s->docs[i]; s->textBytes-=xlsTextSize(d->content); free(d->uri); free(d->path); xlsTextRelease(d->content);xlsContextRelease(d->context); memset(d,0,sizeof(*d)); }
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
 put(result,"fullParses",xrtValueInt((int64)s->analysisStats.fullParses)); put(result,"fragmentParses",xrtValueInt((int64)s->analysisStats.fragmentParses));
 put(result,"reusedDeclarations",xrtValueInt((int64)s->analysisStats.reusedDeclarations)); put(result,"parsedBytes",xrtValueInt((int64)s->analysisStats.parsedBytes));
 put(result,"workerThreadId",xrtValueInt((int64)s->analysisThread)); put(result,"ownerThreadId",xrtValueInt((int64)xrtThreadCurrentId()));
 put(result,"queued",xrtValueInt((int64)load.queued)); put(result,"running",xrtValueInt((int64)load.running)); put(result,"submitted",xrtValueInt((int64)load.submitted)); put(result,"completed",xrtValueInt((int64)load.completed)); put(result,"rejected",xrtValueInt((int64)load.rejected));
#ifdef XLS_COMPILER_XRT
 xls_module_stats stats;xlsModuleStoreStats(s->modules,&stats);xvalue* modules=xrtValueObject();put(modules,"enabled",xrtValueBool(s->modules!=NULL));
#define MODULE_STAT(field) put(modules,#field,xrtValueInt((int64)stats.field))
 MODULE_STAT(lookups);MODULE_STAT(hits);MODULE_STAT(misses);MODULE_STAT(published);MODULE_STAT(deduplicated);MODULE_STAT(evictions);MODULE_STAT(declined);MODULE_STAT(modules);MODULE_STAT(versions);MODULE_STAT(bytes);MODULE_STAT(registryBytes);MODULE_STAT(limit);
#undef MODULE_STAT
 put(result,"moduleStore",modules);
 xls_entity_stats entities;xvalue* registry=xrtValueObject();put(registry,"enabled",xrtValueBool(xlsModuleStoreEntityStats(s->modules,&entities)));
#define ENTITY_STAT(field) put(registry,#field,xrtValueInt((int64)entities.field))
 ENTITY_STAT(domains);ENTITY_STAT(entities);ENTITY_STAT(bytes);ENTITY_STAT(limit);ENTITY_STAT(hits);ENTITY_STAT(admitted);ENTITY_STAT(declined);
#undef ENTITY_STAT
 put(result,"entityStore",registry);
#endif
 response(s,id,result,0,NULL);
}
static void startCheck(server* s,int index,const xvalue* id,int snapshots)
{
 document* d=&s->docs[index]; xide_model_error e={0};
 if(s->check) { if(id) response(s,id,NULL,-32001,"A source check is still stopping; retry after its response."); else s->pending=index; return; }
 if(s->catalog && d->binding<0) {
  const char* reason=d->binding==XIDE_ANALYSIS_AMBIGUOUS ? "The source belongs to multiple projects; select its analysis project." : "Cannot resolve configured analysis inputs.";
  if(id) response(s,id,NULL,-32602,reason);else diagnosticCheckFailed(s,index,-32602,reason);return;
 }
 if(s->catalog && !s->cliDiagnostics) snapshots=1;
 if(snapshots) {
  xide_language_analysis_options options={!s->cliDiagnostics,nativeWake,s};
  if(!s->cliDiagnostics) {
   xls_analysis_document inputs[DOC_LIMIT]; size_t count=0;
   for(int i=0;i<DOC_LIMIT;++i) if(s->docs[i].uri) { document* in=&s->docs[i]; inputs[count++]=(xls_analysis_document){in->uri,in->path,in->content,in->identity,(uint64_t)((int64_t)in->version-INT_MIN)}; }
   s->check=s->catalog ? xideLanguageCheckStartConfigured(d->context,s->cache,s->compiler,s->semanticAnalysis,inputs,count,d->uri,NULL,0,0,&options,&e) :
    xideLanguageCheckStartRetained(s->cache,s->compiler,s->directory,s->semanticAnalysis,inputs,count,d->uri,NULL,0,0,&options,&e);
  } else {
   xide_language_snapshot inputs[DOC_LIMIT]; size_t count=0;
   for(int i=0;i<DOC_LIMIT;++i) if(s->docs[i].uri) inputs[count++]=(xide_language_snapshot){s->docs[i].path,s->docs[i].text,s->docs[i].version};
   s->check=xideLanguageCheckStartSnapshotsEx(s->cache,s->compiler,s->directory,d->path,inputs,count,&options,&e);
  }
 } else s->check=xideLanguageCheckStart(s->cache,s->compiler,s->directory,d->path,d->text,d->version,&e);
 if(!s->check) { if(id) response(s,id,NULL,-32602,*e.message ? e.message : "Unable to start source check."); else fprintf(stderr,"xlang-ls: %s\n",e.message); return; }
 s->snapshotMode=snapshots; s->checkBatchGeneration=s->generation; s->checkForSemantic=s->checkForDiagnostics=0; clearPublished(s);
 s->checkDocument=index; s->checkVersion=d->version; s->checkGeneration=d->generation; s->checkId=id ? xrtValueDeepClone(id) : NULL; s->cancelCode=0; s->cancelReason=NULL;
 if(snapshots && s->automaticDiagnostics) ++s->diagnosticChecks;
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
 int automatic=s->checkForSemantic || s->checkForDiagnostics,background=s->checkForDiagnostics,semanticCode=0,indexed=0;
 int semanticDocument=s->checkDocument; unsigned long long semanticGeneration=s->checkBatchGeneration;
 const char* semanticReason=NULL;
 if(s->cancelCode || !current || state==XIDE_CHECK_CANCELLED) {
  semanticCode=s->cancelCode ? s->cancelCode : -32801; semanticReason=s->cancelReason ? s->cancelReason : "Document content changed.";
  if(s->checkId) response(s,s->checkId,NULL,semanticCode,semanticReason);
 }
 else if(state==XIDE_CHECK_READY && result) {
  if(s->catalog && !put(result,"analysisContext",configurationStamp(s,s->checkDocument))) {xrtValueRelease(result);result=NULL;semanticCode=-32603;semanticReason="Unable to stamp analysis configuration.";if(s->checkId) response(s,s->checkId,NULL,semanticCode,semanticReason);goto checkFinished;}
  xvalue* list=get(result,"diagnostics"); int published=0;
  bool definitions=false; xrtValueGetBool(get(result,"semanticDefinitions"),&definitions); indexed=definitions;
  if(s->snapshotMode) {
   s->guardId=s->checkId ? xrtValueDeepClone(s->checkId) : xrtValueNull(); s->guardUri=copy(d->uri); s->guardVersion=d->version;
   published=s->guardId && s->guardUri && put(result,"checkId",xrtValueDeepClone(s->guardId));
   if(published) published=s->automaticDiagnostics ? diagnosticCapture(s,s->checkDocument,s->check,&result,s->checkId!=NULL) : publishSnapshot(s,result);
   if(published) { s->guard=s->check; s->check=NULL; s->guardCancelled=0; }
   else { xrtValueRelease(s->guardId); s->guardId=NULL; free(s->guardUri); s->guardUri=NULL; }
  } else { if(!s->automaticDiagnostics) publish(s,d,xrtValueDeepClone(list)); published=1; }
  if(!published) { semanticCode=-32603; semanticReason="Unable to publish verified snapshot groups."; if(s->checkId) response(s,s->checkId,NULL,semanticCode,semanticReason); xrtValueRelease(result); result=NULL; }
  if(s->checkId && result) { put(result,"count",xrtValueInt((int64)xrtValueCount(list))); response(s,s->checkId,result,0,NULL); result=NULL; }
 } else { semanticCode=-32603; semanticReason=*error ? error : "Unable to obtain verified diagnostics."; if(s->checkId) response(s,s->checkId,NULL,semanticCode,semanticReason); else fprintf(stderr,"xlang-ls: %s\n",error); }
checkFinished:
 if(background && current && semanticCode) diagnosticCheckFailed(s,semanticDocument,semanticCode,semanticReason);
 xrtValueRelease(result);
 if(s->check && !xideLanguageCheckWorkerFinished(s->check)) { xideLanguageCheckCancel(s->check); s->guard=s->check; s->check=NULL; s->guardCancelled=1; }
 xideLanguageCheckDestroy(s->check); s->check=NULL; xrtValueRelease(s->checkId); s->checkId=NULL;
 s->checkForSemantic=s->checkForDiagnostics=0;
 if(automatic) semanticCheckFinished(s,semanticDocument,semanticGeneration,current,semanticCode,semanticReason,indexed);
 else drainSemantic(s);
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
 if(s->catalog && s->docs[entry].binding<0) {free(uri);response(s,s->completionPendingId,NULL,-32602,"Completion needs an unambiguous configured project.");goto discard;}
 if(!s->cliCompletion) {
  xls_analysis_document inputs[DOC_LIMIT]; size_t count=0;
  for(int j=0;j<DOC_LIMIT;++j) if(s->docs[j].uri) { document* in=&s->docs[j]; inputs[count++]=(xls_analysis_document){in->uri,in->path,in->content,in->identity,(uint64_t)((int64_t)in->version-INT_MIN)}; }
  s->completion=s->catalog ? xideLanguageCheckStartConfigured(s->docs[entry].context,s->cache,s->compiler,s->cursorAnalysis,inputs,count,d->uri,d->path,s->completionPendingLine,s->completionPendingCharacter,&options,&e) :
   xideLanguageCheckStartRetained(s->cache,s->compiler,s->directory,s->cursorAnalysis,inputs,count,s->docs[entry].uri,d->path,s->completionPendingLine,s->completionPendingCharacter,&options,&e);
 } else {
  xide_language_snapshot inputs[DOC_LIMIT]; size_t count=0;
  for(int j=0;j<DOC_LIMIT;++j) if(s->docs[j].uri) inputs[count++]=(xide_language_snapshot){s->docs[j].path,s->docs[j].text,s->docs[j].version};
  s->completion=xideLanguageCheckStartCompletionSnapshotsEx(s->cache,s->compiler,s->directory,s->docs[entry].path,inputs,count,d->path,s->completionPendingLine,s->completionPendingCharacter,&options,&e);
 }
 if(!s->completion) { free(uri); response(s,s->completionPendingId,NULL,-32603,*e.message ? e.message : "Unable to start cursor query."); goto discard; }
 s->completionUri=uri; s->completionId=s->completionPendingId; s->completionPendingId=NULL;
 s->completionDocument=i;s->completionContextDocument=entry; s->completionVersion=d->version; s->completionGeneration=s->generation;
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
  char guardError[512]={0};
  if(!s->completionCancelled && xideLanguageCheckGuardState(s->completion,guardError,sizeof(guardError))==XIDE_GUARD_INVALID) cancelCompletion(s,-32801,*guardError ? guardError : "Cursor source guard invalidated.",0);
  if(!s->completionCancelled && !s->completionResponded && state!=XIDE_CHECK_RUNNING) {
   xvalue* result=xideLanguageCheckResult(s->completion);
   if(state==XIDE_CHECK_READY && result && (!s->catalog || put(result,"analysisContext",configurationStamp(s,s->completionContextDocument))) && put(result,"queryId",xrtValueDeepClone(s->completionId))) { response(s,s->completionId,result,0,NULL); result=NULL; s->completionPublished=1; }
   else { response(s,s->completionId,NULL,state==XIDE_CHECK_CANCELLED ? -32800 : -32603,*error ? error : "Unable to obtain verified cursor facts."); s->completionCancelled=1; xideLanguageCheckCancel(s->completion); }
   xrtValueRelease(result); s->completionResponded=1;
  }
  if(s->completionCancelled && xideLanguageCheckWorkerFinished(s->completion)) {
   xideLanguageCheckDestroy(s->completion); s->completion=NULL; xrtValueRelease(s->completionId); s->completionId=NULL; free(s->completionUri); s->completionUri=NULL; s->completionCancelled=s->completionResponded=s->completionPublished=0;
  }
 }
 startCompletion(s);
}
static void signatureInvalidated(server* s,const char* reason)
{
 xvalue *out=xrtValueObject(),*p=xrtValueObject();
 put(out,"jsonrpc",xrtValueString(sv("2.0"))); put(out,"method",xrtValueString(sv("xlang/signatureInvalidated")));
 put(p,"queryId",xrtValueDeepClone(s->signatureId)); put(p,"uri",xrtValueString(sv(s->signatureUri))); put(p,"version",xrtValueInt(s->signatureVersion)); put(p,"reason",xrtValueString(sv(reason))); put(out,"params",p); sendMessage(s,out);
}
static void cancelSignature(server* s,int code,const char* reason,int pending)
{
 if(s->signature && !s->signatureCancelled) {
  if(!s->signatureResponded) { response(s,s->signatureId,NULL,code,reason); s->signatureResponded=1; }
  else if(s->signaturePublished) signatureInvalidated(s,reason);
  s->signatureCancelled=1; xideLanguageCheckCancel(s->signature);
 }
 if(pending && s->signaturePendingId) { response(s,s->signaturePendingId,NULL,code,reason); xrtValueRelease(s->signaturePendingId); s->signaturePendingId=NULL; }
}
static void startSignature(server* s)
{
 if(s->signature || !s->signaturePendingId || s->shutdown) return;
 int i=s->signaturePendingDocument,entry=s->signaturePendingEntry; document* d=&s->docs[i]; xide_model_error e={0};
 if(!d->uri || !s->docs[entry].uri || s->signaturePendingGeneration!=s->generation || d->version!=s->signaturePendingVersion) { response(s,s->signaturePendingId,NULL,-32801,"Open-document cursor inputs changed before starting."); goto discard; }
 char* uri=copy(d->uri);
 if(!uri) { response(s,s->signaturePendingId,NULL,-32603,"Unable to retain cursor input identity."); goto discard; }
 xide_language_analysis_options options={1,nativeWake,s};
 if(s->catalog && s->docs[entry].binding<0){free(uri);response(s,s->signaturePendingId,NULL,-32602,"Signature query needs an unambiguous configured project.");goto discard;}
 xls_analysis_document inputs[DOC_LIMIT];size_t count=0;
 for(int j=0;j<DOC_LIMIT;++j)if(s->docs[j].uri){document* in=&s->docs[j];inputs[count++]=(xls_analysis_document){in->uri,in->path,in->content,in->identity,(uint64_t)((int64_t)in->version-INT_MIN)};}
 s->signature=xideLanguageCheckStartSignature(s->catalog ? s->docs[entry].context : NULL,s->catalog!=NULL,s->cache,s->compiler,
  s->catalog ? xlsContextSpec(s->docs[entry].context)->directory : s->directory,s->signatureAnalysis,inputs,count,
  s->catalog ? d->uri : s->docs[entry].uri,d->path,s->signaturePendingLine,s->signaturePendingCharacter,s->signatureFlags,&options,&e);
 if(!s->signature) { free(uri); response(s,s->signaturePendingId,NULL,-32603,*e.message ? e.message : "Unable to start cursor query."); goto discard; }
 s->signatureUri=uri; s->signatureId=s->signaturePendingId; s->signaturePendingId=NULL;
 s->signatureDocument=i;s->signatureContextDocument=entry; s->signatureVersion=d->version; s->signatureGeneration=s->generation;
 s->signatureResponded=s->signaturePublished=s->signatureCancelled=0; return;
discard: xrtValueRelease(s->signaturePendingId); s->signaturePendingId=NULL;
}
static void requestSignature(server* s,const xvalue* id,const xvalue* params)
{
 int i=find(s,string(get(get(params,"textDocument"),"uri"))),entry=i,line=0,character=0,version=i>=0 ? s->docs[i].version : 0;
 const xvalue *requested=get(params,"version"),*entryDocument=get(params,"entryDocument"),*context=get(params,"context");
 const char* requestName=string(id); if(requestName && strlen(requestName)>65535) { response(s,id,NULL,-32602,"Signature request ID exceeds 64 KiB."); return; }
 if(entryDocument) entry=find(s,string(get(entryDocument,"uri")));
 if(i<0 || entry<0 || (requested && !integer(requested,&version)) || version!=s->docs[i].version || !integer(get(get(params,"position"),"line"),&line) || !integer(get(get(params,"position"),"character"),&character) || !validPosition(&s->docs[i],line,character)) { response(s,id,NULL,-32602,"Signature requires an open xlang document, a current version and a valid UTF-16 scalar position."); return; }
 if(i>=0 && xlsTextSize(s->docs[i].content)>=3 && !memcmp(s->docs[i].text,"\xef\xbb\xbf",3) && line==0 && character==0) { response(s,id,NULL,-32602,"Position precedes the source BOM."); return; }
 if(!xlsSignatureHelpContextValid(context)){response(s,id,NULL,-32602,"Signature help context is invalid.");return;}
 xvalue* captured=xrtValueDeepClone(id); if(!captured) { response(s,id,NULL,-32603,"Unable to retain signature request."); return; }
 cancelSignature(s,-32800,"Cursor query superseded by a newer request.",1);
 s->signaturePendingId=captured; s->signaturePendingDocument=i; s->signaturePendingEntry=entry; s->signaturePendingVersion=version; s->signaturePendingLine=line; s->signaturePendingCharacter=character; s->signaturePendingGeneration=s->generation;
 startSignature(s);
}
static void pollSignature(server* s)
{
 if(s->signature) {
  char error[512]={0}; enum xide_check_state state=xideLanguageCheckState(s->signature,error,sizeof(error));
  if(!s->signatureCancelled && (s->signatureGeneration!=s->generation || !s->docs[s->signatureDocument].uri || s->docs[s->signatureDocument].version!=s->signatureVersion)) cancelSignature(s,-32801,"Cursor input set is no longer current.",0);
  char guardError[512]={0};
  if(!s->signatureCancelled && xideLanguageCheckGuardState(s->signature,guardError,sizeof(guardError))==XIDE_GUARD_INVALID) cancelSignature(s,-32801,*guardError ? guardError : "Cursor source guard invalidated.",0);
  if(!s->signatureCancelled && !s->signatureResponded && state!=XIDE_CHECK_RUNNING) {
   xvalue* result=xideLanguageCheckResult(s->signature);
   if(state==XIDE_CHECK_READY && result && (xrtValueType(result)==XVALUE_NULL || ((!s->catalog || put(result,"analysisContext",configurationStamp(s,s->signatureContextDocument))) && put(result,"queryId",xrtValueDeepClone(s->signatureId))))) {
    int empty=xrtValueType(result)==XVALUE_NULL;response(s,s->signatureId,result,0,NULL);result=NULL;s->signaturePublished=!empty;
    if(empty){s->signatureCancelled=1;xideLanguageCheckCancel(s->signature);}
   }
   else { response(s,s->signatureId,NULL,state==XIDE_CHECK_CANCELLED ? -32800 : -32603,*error ? error : "Unable to obtain verified cursor facts."); s->signatureCancelled=1; xideLanguageCheckCancel(s->signature); }
   xrtValueRelease(result); s->signatureResponded=1;
  }
  if(s->signatureCancelled && xideLanguageCheckWorkerFinished(s->signature)) {
   xideLanguageCheckDestroy(s->signature); s->signature=NULL; xrtValueRelease(s->signatureId); s->signatureId=NULL; free(s->signatureUri); s->signatureUri=NULL; s->signatureCancelled=s->signatureResponded=s->signaturePublished=0;
  }
 }
 startSignature(s);
}
static int receive(const xvalue* value,void* user)
{
 server* s=user; message* m=calloc(1,sizeof(*m)); size_t n=0; char* encoded=xrtJsonStringify(value,false,&n); xrtFree(encoded);
 if(!m || !n || !(m->value=xrtValueDeepClone(value))) { if(m) { xrtValueRelease(m->value); free(m); } return 0; } m->size=n;
 const char* method=string(get(m->value,"method"));
 m->input=method && (!strcmp(method,"textDocument/didOpen") || !strcmp(method,"textDocument/didChange") || !strcmp(method,"textDocument/didClose") || !strcmp(method,"textDocument/didSave") || !strcmp(method,"workspace/didChangeWatchedFiles") || !strcmp(method,"workspace/didChangeConfiguration") || !strcmp(method,"xlang/configureAnalysis"));
 EnterCriticalSection(&s->lock); int okay=s->queueCount<256 && n<=QUEUE_LIMIT-s->queueBytes;
 if(okay) { if(s->tail) s->tail->next=m; else s->head=m; s->tail=m; s->queueBytes+=n; ++s->queueCount; s->queueInputs+=(size_t)m->input; SetEvent(s->event); }
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
 if(s->inputSerial==ULLONG_MAX) {s->writeFailed=1;return;}
 const xvalue* item=get(params,"textDocument"); const char* uri=string(get(item,"uri")); int i=find(s,uri);
 if(!strcmp(method,"textDocument/didOpen")) {
  const char *text=string(get(item,"text")),*language=string(get(item,"languageId")); int version=0;
  if(!uri || i>=0 || !language || strcmp(language,"xlang") || !validText(text) || !integer(get(item,"version"),&version) || strlen(text)>TOTAL_LIMIT-s->textBytes) goto invalid;
  for(i=0;i<DOC_LIMIT && s->docs[i].uri;++i) {} if(i==DOC_LIMIT) goto invalid;
  xide_model_error e={0}; char* path=xideLspUriToPath(uri,&e); if(!path || !xidePathHasExtension(path,".xl")) { free(path); goto invalid; }
  for(int j=0;j<DOC_LIMIT;++j) if(s->docs[j].uri && (xidePathLexicalEqual(path,s->docs[j].path) || xidePathEqual(path,s->docs[j].path))) { free(path); goto invalid; }
  xls_text* content=xlsTextCreate(text,strlen(text),TEXT_LIMIT,NULL); char* name=copy(uri);
  unsigned char uriHash[32];
  if(!content || !name || !xlsInputStateUri(uri,uriHash)) { free(path); xlsTextRelease(content); free(name); goto invalid; }
  unsigned long long generation=++s->generation;
  s->docs[i]=(document){.uri=name,.path=path,.text=xlsTextData(content),.content=content,.version=version,.generation=generation,.identity=generation};s->docs[i].context=bindAnalysis(s->catalog,path,&s->docs[i].binding); memcpy(s->docs[i].uriHash,uriHash,32); s->textBytes+=xlsTextSize(content);++s->inputSerial; cancelCompletion(s,-32801,"Open-document cursor input set changed.",1);cancelSignature(s,-32801,"Open-document cursor input set changed.",1); if(s->check && s->snapshotMode) cancel(s,-32801,"Open-document snapshot set changed."); clearPublished(s); diagnosticInput(s,i,path,1); return;
 }
 if(i<0) goto invalid;
 document* d=&s->docs[i];
 if(!strcmp(method,"textDocument/didClose")) {++s->inputSerial; cancelNative(s,i,-32800,"Document closed."); cancelSemantic(s,i,-32800,"Document closed."); cancelCompletion(s,-32800,"Document closed.",1);cancelSignature(s,-32800,"Document closed.",1); if(s->check && (s->snapshotMode || s->checkDocument==i)) cancel(s,-32800,"Document closed."); if(s->pending==i) s->pending=-1; clearPublished(s); publish(s,d,NULL); ++s->generation; diagnosticInput(s,-1,d->path,1); clearDocument(s,i); s->diagnosticDue[i]=0; s->diagnosticFailed[i]=0; return; }
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
  cancelCompletion(s,-32801,"Open-document cursor content changed.",1);cancelSignature(s,-32801,"Open-document cursor content changed.",1);
  cancelNative(s,i,-32801,"Native analysis document changed.");
  cancelSemantic(s,i,-32801,"Semantic analysis document changed.");
  s->textBytes=s->textBytes-xlsTextSize(d->content)+xlsTextSize(content); xlsTextRelease(d->content); d->content=content; d->text=xlsTextData(content); d->version=version; d->generation=++s->generation;++s->inputSerial; clearPublished(s); publish(s,d,NULL); diagnosticInput(s,i,d->path,0); return;
 }
 if(!strcmp(method,"textDocument/didSave")) {
  const xvalue* supplied=get(params,"text"); const char* text=string(supplied); if(supplied && (!text || strcmp(text,d->text))) goto invalid;
  ++s->inputSerial;if(s->automaticDiagnostics) {
   ++s->generation; if(s->check && s->snapshotMode) cancel(s,-32801,"Saved inputs changed.");
   cancelCompletion(s,-32801,"Saved input visibility changed.",1);cancelSignature(s,-32801,"Saved input visibility changed.",1); clearPublished(s); diagnosticInput(s,i,d->path,0);
  } else if(s->checkOnSave) startCheck(s,i,NULL,0);
  return;
 }
invalid: fprintf(stderr,"xlang-ls: rejected %s notification.\n",method);
}
static void releaseSemantic(semantic_request* request)
{ xrtValueRelease(request->id); memset(request,0,sizeof(*request)); }
static int semanticCurrent(server* s,const semantic_request* request)
{
 document* d=&s->docs[request->document];
 return d->uri && d->identity==request->identity && d->version==request->version && s->generation==request->generation;
}
static xvalue* semanticResult(server* s,const semantic_request* request)
{
 if(!s->guard || s->guardCancelled) return NULL;
 document* d=&s->docs[request->document];
 if(s->catalog && (d->binding<0 || ((!request->bound || d->binding==XIDE_ANALYSIS_PROJECT) && !xideLanguageCheckMatchesContext(s->guard,d->context)))) return NULL;
 return request->kind==3 ? xideLanguageCheckDocumentHighlights(s->guard,d->path,request->version,request->line,request->character) :
  request->kind==2 ? xideLanguageCheckDocumentSymbols(s->guard,d->path,request->version) :
  request->kind==1 ? xideLanguageCheckHover(s->guard,d->path,request->version,request->line,request->character) :
  s->definitionInputProof ? xideLanguageCheckDefinitionWithProof(s->guard,d->path,request->version,request->line,request->character) :
  xideLanguageCheckDefinition(s->guard,d->path,request->version,request->line,request->character);
}
static void cancelUnusedSemanticCheck(server* s)
{
 if(!s->check || !s->checkForSemantic || s->checkForDiagnostics || s->cancelCode) return;
 for(size_t i=0;i<DOC_LIMIT;++i) if(s->semantic[i].id && s->semantic[i].started) return;
 cancel(s,-32800,"No pending semantic request needs this analysis.");
}
static void cancelSemantic(server* s,int index,int code,const char* reason)
{
 for(size_t i=0;i<DOC_LIMIT;++i) if(s->semantic[i].id && (index<0 || s->semantic[i].document==index)) {
  response(s,s->semantic[i].id,NULL,code,reason); releaseSemantic(&s->semantic[i]);
 }
 cancelUnusedSemanticCheck(s);
}
static void drainSemantic(server* s)
{
 for(size_t i=0;i<DOC_LIMIT;++i) {
  semantic_request* request=&s->semantic[i]; if(!request->id) continue;
  if(!semanticCurrent(s,request)) { response(s,request->id,NULL,-32801,"Semantic request inputs changed."); releaseSemantic(request); continue; }
  xvalue* result=semanticResult(s,request);
  if(result) { response(s,request->id,result,0,NULL); releaseSemantic(request); }
 }
}
static void semanticCheckFinished(server* s,int index,unsigned long long generation,int current,int code,const char* reason,int indexed)
{
 for(size_t i=0;i<DOC_LIMIT;++i) {
  semantic_request* request=&s->semantic[i];
  if(!request->id || !request->started || request->document!=index || request->generation!=generation) continue;
  if(code || !current || !semanticCurrent(s,request)) response(s,request->id,NULL,code ? code : -32801,reason ? reason : "Semantic request inputs changed.");
  else if(!s->guard || s->guardCancelled || xideLanguageCheckGuardState(s->guard,NULL,0)==XIDE_GUARD_INVALID)
   response(s,request->id,NULL,-32801,"Analyzed semantic inputs are no longer current.");
  else {
   /* Source errors have diagnostics, but no complete semantic facts. Empty
      standard results do not conceal infrastructure or resource failures. */
   xvalue* result=indexed ? semanticResult(s,request) : request->kind==2 ? xrtValueArray() : xrtValueNull();
   response(s,request->id,result,result ? 0 : indexed ? -32801 : -32603,result ? NULL : "Semantic result is unavailable.");
  }
  releaseSemantic(request);
 }
 drainSemantic(s);
}
static void pollSemantic(server* s)
{
 drainSemantic(s); cancelUnusedSemanticCheck(s);
 if(s->check || s->shutdown) return;
 semantic_request* next=NULL;
 for(size_t i=0;i<DOC_LIMIT;++i) if(s->semantic[i].id && (!next || s->semantic[i].order<next->order)) next=&s->semantic[i];
 if(!next) return;
 int index=next->document; startCheck(s,index,NULL,1);
 if(!s->check) { cancelSemantic(s,index,-32603,"Unable to start current semantic analysis."); return; }
 s->checkForSemantic=1;
 s->checkForDiagnostics=s->automaticDiagnostics;
 for(size_t i=0;i<DOC_LIMIT;++i) if(s->semantic[i].id && s->semantic[i].document==index && semanticCurrent(s,&s->semantic[i])) s->semantic[i].started=1;
}
static void semanticQuery(server* s,const xvalue* id,const xvalue* params,int query)
{
 int i=find(s,string(get(get(params,"textDocument"),"uri"))),version=i>=0 ? s->docs[i].version : 0,line=0,character=0; const xvalue* batch=get(params,"checkId"),*suppliedVersion=get(params,"version");
 const char* namedId=string(id);
 if(namedId && strlen(namedId)>65535) { response(s,id,NULL,-32602,"Semantic request ID exceeds 64 KiB."); return; }
 if(i<0 || (suppliedVersion && !integer(suppliedVersion,&version)) || version!=s->docs[i].version || (query!=2 && (!integer(get(get(params,"position"),"line"),&line) || line<0 || !integer(get(get(params,"position"),"character"),&character) || character<0)) || (batch && !validId(batch))) { response(s,id,NULL,-32602,"Open document and valid UTF-16 position are required; supplied version/checkId must be current."); return; }
 if(query!=2 && !validPosition(&s->docs[i],line,character)) { response(s,id,NULL,-32602,"Position is not a UTF-16 scalar boundary."); return; }
 if(s->catalog && s->docs[i].binding<0) {response(s,id,NULL,-32602,"Semantic queries need an unambiguous configured analysis project.");return;}
 if(batch && (!s->guard || s->guardCancelled || !sameId(batch,s->guardId))) { response(s,id,NULL,-32801,"Checked compilation is no longer current."); return; }
 size_t slot=DOC_LIMIT;
 for(size_t j=0;j<DOC_LIMIT;++j) {
  if(s->semantic[j].id && sameId(s->semantic[j].id,id)) { releaseSemantic(&s->semantic[j]); response(s,id,NULL,-32600,"Request ID is already active."); cancelUnusedSemanticCheck(s); return; }
  if(!s->semantic[j].id && slot==DOC_LIMIT) slot=j;
 }
 semantic_request captured={.document=i,.version=version,.line=line,.character=character,.kind=query,
  .identity=s->docs[i].identity,.generation=s->generation,.order=++s->semanticOrder,.bound=batch!=NULL};
 xvalue* result=semanticResult(s,&captured);
 if(result) { response(s,id,result,0,NULL); return; }
 if(batch || s->cliDiagnostics) { response(s,id,NULL,-32801,"No current semantic index; run a successful source check first."); return; }
 if(slot==DOC_LIMIT) { response(s,id,NULL,-32001,"The semantic request queue is full."); return; }
 captured.id=xrtValueDeepClone(id);
 if(!captured.id) { response(s,id,NULL,-32603,"Unable to retain semantic request."); return; }
 captured.started=s->check && (s->checkForSemantic || s->checkForDiagnostics) && !s->cancelCode && s->checkDocument==i && s->checkBatchGeneration==s->generation;
 s->semantic[slot]=captured;
}
#include "server/automatic_diagnostics.inc"
static int configureAnalysis(server* s,const xvalue* value,xide_model_error* error)
{
 int64 epoch=0;
 if(xrtValueType(value)!=XVALUE_OBJECT || xrtValueCount(value)!=2 || !xrtValueGetInt(get(value,"epoch"),&epoch) ||
    epoch<=0 || (s->initialized && epoch<=s->configurationEpoch) || epoch==INT64_MAX || s->generation==ULLONG_MAX || s->inputSerial==ULLONG_MAX || s->cliCompletion || s->cliDiagnostics) {
  snprintf(error->message,sizeof(error->message),"Analysis configuration needs a newer positive epoch and native backends.");return 0;
 }
 xide_analysis_catalog* catalog=xideAnalysisCatalogCreate(get(value,"catalog"),error->message,sizeof(error->message));if(!catalog) return 0;
 xls_context* contexts[DOC_LIMIT]={0};int bindings[DOC_LIMIT]={0};int okay=1;
 for(size_t i=0;i<DOC_LIMIT && okay;++i) if(s->docs[i].uri) {contexts[i]=bindAnalysis(catalog,s->docs[i].path,bindings+i);okay=bindings[i]!=XIDE_ANALYSIS_ERROR;}
 if(!okay) {for(size_t i=0;i<DOC_LIMIT;++i) xlsContextRelease(contexts[i]);xideAnalysisCatalogDestroy(catalog);snprintf(error->message,sizeof(error->message),"Cannot prepare open-document analysis bindings.");return 0;}
 if(s->initialized) {
  ++s->generation;++s->inputSerial;s->pending=-1;
  cancel(s,-32801,"Analysis configuration changed.");cancelSemantic(s,-1,-32801,"Analysis configuration changed.");cancelCompletion(s,-32801,"Analysis configuration changed.",1);cancelSignature(s,-32801,"Analysis configuration changed.",1);clearPublished(s);
  if(s->automaticDiagnostics) diagnosticResetState(s,NULL);
 }
 for(size_t i=0;i<DOC_LIMIT;++i) {xlsContextRelease(s->docs[i].context);s->docs[i].context=contexts[i];s->docs[i].binding=bindings[i];}
 xideAnalysisCatalogDestroy(s->catalog);s->catalog=catalog;s->configurationEpoch=epoch;return 1;
}
static xvalue* configurationStamp(const server* s,int index)
{
 const xls_context* context=s->docs[index].context;if(!context) return NULL;
 const xls_context_spec* spec=xlsContextSpec(context);xvalue* value=xrtValueObject();
 int okay=value && put(value,"schema",xrtValueInt(1)) && put(value,"epoch",xrtValueInt(s->configurationEpoch)) &&
  put(value,"project",xrtValueString(sv(spec->project))) && put(value,"configuration",xrtValueString(sv(spec->configuration))) &&
  put(value,"entry",xrtValueString(sv(spec->entry))) && put(value,"directory",xrtValueString(sv(spec->directory)));
 if(!okay) {xrtValueRelease(value);value=NULL;}return value;
}
static void inputState(server* s,const xvalue* id,const xvalue* params)
{
 if(params && (xrtValueType(params)!=XVALUE_OBJECT || xrtValueCount(params))) { response(s,id,NULL,-32602,"Input state takes no parameters."); return; }
 xls_input_state_item items[DOC_LIMIT]; size_t count=0;
 for(size_t i=0;i<DOC_LIMIT;++i) if(s->docs[i].uri) { memcpy(items[count].uri,s->docs[i].uriHash,32); items[count++].version=s->docs[i].version; }
 char digest[65],generation[32];
 if(!xlsInputStateDigest(items,count,digest)) { response(s,id,NULL,-32603,"Unable to summarize online document bindings."); return; }
 snprintf(generation,sizeof(generation),"%llu",s->generation);
 xvalue* result=xrtValueObject();
 int okay=result && put(result,"schema",xrtValueInt(1)) && put(result,"generation",xrtValueString(sv(generation))) && put(result,"documents",xrtValueInt((int64)count)) && put(result,"sha256",xrtValueString(sv(digest)));
 if(okay && s->diagnosticOverlayProof) {char serial[32];snprintf(serial,sizeof(serial),"%llu",s->inputSerial);okay=put(result,"serial",xrtValueString(sv(serial)));}
 if(okay) response(s,id,result,0,NULL); else { xrtValueRelease(result); response(s,id,NULL,-32603,"Unable to encode online document bindings."); }
}

#ifdef XLS_COMPILER_XRT
static void cancelReferences(server* s,const xvalue* id,int code,const char* reason)
{
 xvalue* retained=xrtValueRetain(id);id=retained;
 for(size_t i=0;i<8;++i)if(s->referenceRequests[i].query && (!id || sameId(id,s->referenceRequests[i].id))){xideProjectReferencesCancel(s->referenceRequests[i].query);if(code)response(s,s->referenceRequests[i].id,NULL,code,reason);xideProjectReferencesQueryDestroy(s->referenceRequests[i].query);xrtValueRelease(s->referenceRequests[i].id);memset(s->referenceRequests+i,0,sizeof(s->referenceRequests[i]));}
 xrtValueRelease(retained);
}
static void referenceWatched(server* s,const xvalue* params)
{
 if(s->automaticDiagnostics)return;
 const xvalue* changes=get(params,"changes");if(!changes || xrtValueType(changes)!=XVALUE_ARRAY || !xrtValueCount(changes))return;
 for(size_t i=0;i<xrtValueCount(changes);++i){const xvalue* row=xrtValueArrayGet(changes,i);const char* uri=string(get(row,"uri"));int type=0;char* path=uri ? xideLspUriToPath(uri,NULL) : NULL;int okay=path && integer(get(row,"type"),&type) && type>=1 && type<=3;free(path);if(!okay)return;}
 if(s->inputSerial==ULLONG_MAX)s->writeFailed=1;else ++s->inputSerial;
}
static void requestReferences(server* s,const xvalue* id,const xvalue* params)
{
 int i=find(s,string(get(get(params,"textDocument"),"uri"))),line=0,character=0,version=i>=0?s->docs[i].version:0;bool include=false;const xvalue* supplied=get(params,"version");const char* namedId=string(id);
 if(s->cliCompletion || s->cliDiagnostics){response(s,id,NULL,-32601,"References require the native analysis backend.");return;}
 if(i<0 || (namedId && strlen(namedId)>65535) || (supplied && !integer(supplied,&version)) || version!=s->docs[i].version || !integer(get(get(params,"position"),"line"),&line) || !integer(get(get(params,"position"),"character"),&character) || !validPosition(&s->docs[i],line,character) || !xrtValueGetBool(get(get(params,"context"),"includeDeclaration"),&include)){response(s,id,NULL,-32602,"References require an open current xlang document,valid UTF16 position and boolean includeDeclaration.");return;}
 if(s->catalog && s->docs[i].binding<0){response(s,id,NULL,-32602,"References need an unambiguous configured project.");return;}
 size_t slot=8;for(size_t j=0;j<8;++j){if(s->referenceRequests[j].query && sameId(id,s->referenceRequests[j].id)){cancelReferences(s,id,-32600,"Reference request ID is already active.");return;}if(!s->referenceRequests[j].query && slot==8)slot=j;}if(slot==8){response(s,id,NULL,-32603,"Reference request queue is full.");return;}
 xls_context* context=xlsContextRetain(s->docs[i].context);char* package=NULL;
 if(!context){package=xrtEnvGet("XLANG_PACKAGE_PATH");xls_context_spec spec={.kind=XLS_CONTEXT_XLANG,.entry=s->docs[i].path,.directory=s->directory,.analysisProfile="xlang-ls/compiler-xrt-v1",.targetKind="exe",.packagePath=package};context=xlsContextCreate(&spec,16u*1024u*1024u,NULL);}xrtFree(package);
 xls_analysis_document docs[DOC_LIMIT];size_t count=0;for(size_t j=0;j<DOC_LIMIT;++j)if(s->docs[j].uri){document* d=s->docs+j;docs[count++]=(xls_analysis_document){d->uri,d->path,d->content,d->identity,(uint64_t)((int64_t)d->version-INT_MIN)};}
 if(!s->references)s->references=xideProjectReferencesCreate(s->cache,s->compiler,s->modules);
 xvalue* captured=xrtValueDeepClone(id);xide_reference_query* query=context && captured ? xideProjectReferencesSubmitWithProof(s->references,context,docs,count,s->docs[i].uri,s->docs[i].path,line,character,include,s->referenceInputProof) : NULL;xlsContextRelease(context);
 if(!query || !xideProjectReferencesSetWake(query,nativeWake,s)){xideProjectReferencesCancel(query);xideProjectReferencesQueryDestroy(query);xrtValueRelease(captured);response(s,id,NULL,-32603,"Cannot start background project references.");return;}
 s->referenceRequests[slot].query=query;s->referenceRequests[slot].id=captured;s->referenceRequests[slot].document=i;s->referenceRequests[slot].version=version;s->referenceRequests[slot].identity=s->docs[i].identity;s->referenceRequests[slot].serial=s->inputSerial;s->referenceRequests[slot].epoch=s->configurationEpoch;
}
static void pollReferences(server* s)
{
 for(size_t i=0;i<8;++i){xide_reference_query* q=s->referenceRequests[i].query;if(!q)continue;document* d=s->docs+s->referenceRequests[i].document;
  if(!d->uri || d->identity!=s->referenceRequests[i].identity || d->version!=s->referenceRequests[i].version || s->inputSerial!=s->referenceRequests[i].serial || s->configurationEpoch!=s->referenceRequests[i].epoch){cancelReferences(s,s->referenceRequests[i].id,-32801,"Project reference input/configuration snapshot changed.");continue;}
  EnterCriticalSection(&s->lock);size_t waiting=s->queueInputs;LeaveCriticalSection(&s->lock);if(waiting)continue;
  char error[512]={0};xide_project_reference_stats stats={0};enum xide_reference_query_state state=xideProjectReferencesResult(q,NULL,&stats,error,sizeof(error));if(state==XIDE_REFERENCES_PENDING)continue;
  s->referenceStats=stats;
  if(state==XIDE_REFERENCES_READY)response(s,s->referenceRequests[i].id,xideProjectReferencesTake(q),0,NULL);else response(s,s->referenceRequests[i].id,NULL,state==XIDE_REFERENCES_CANCELLED?-32800:-32803,*error?error:"Project reference request cancelled.");
  xideProjectReferencesQueryDestroy(q);xrtValueRelease(s->referenceRequests[i].id);memset(s->referenceRequests+i,0,sizeof(s->referenceRequests[i]));
 }
}
static void referenceStats(server* s,const xvalue* id)
{
 xide_project_reference_stats* stats=&s->referenceStats;xvalue* out=xrtValueObject();int okay=out!=NULL;
#define REF_STAT(field) okay=okay && put(out,#field,xrtValueInt((int64)stats->field))
 REF_STAT(roots);REF_STAT(captured);REF_STAT(reused);REF_STAT(locations);REF_STAT(records);REF_STAT(chargedBytes);REF_STAT(thread);
 REF_STAT(keyActorBuilds);REF_STAT(keyActorHits);REF_STAT(keyActorSessions);REF_STAT(keyNavigationBuilds);REF_STAT(keyNavigationHits);
 REF_STAT(memberWorkers);REF_STAT(memberTasks);REF_STAT(memberPeak);REF_STAT(memberThreads);
#undef REF_STAT
 if(okay)response(s,id,out,0,NULL);else{ xrtValueRelease(out);response(s,id,NULL,-32603,"Cannot encode reference work counters.");}
}
#endif
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
  if(!xlsSignatureHelpCapabilities(get(params,"capabilities"),&s->signatureFlags)){response(s,id,NULL,-32602,"Invalid signature help capabilities.");return;}
  s->cliCompletion=backend && !strcmp(backend,"cli");
  const xvalue* diagnosticsOption=get(options,"diagnosticsBackend"); const char* diagnostics=string(diagnosticsOption);
  if(diagnosticsOption && (!diagnostics || (strcmp(diagnostics,"native") && strcmp(diagnostics,"cli")))) { response(s,id,NULL,-32602,"diagnosticsBackend must be native or cli."); return; }
  s->cliDiagnostics=diagnostics && !strcmp(diagnostics,"cli");
  bool automatic=!s->cliDiagnostics,versions=false,inputProof=false,dependencyProof=false,definitionProof=false; const xvalue* autoOption=get(options,"automaticDiagnostics");
  int delay=75; const xvalue* delayOption=get(options,"diagnosticDelay");
  if((autoOption && !xrtValueGetBool(autoOption,&automatic)) || (automatic && s->cliDiagnostics) || (delayOption && (!integer(delayOption,&delay) || delay<0 || delay>2000))) { response(s,id,NULL,-32602,"automaticDiagnostics requires the native backend; diagnosticDelay must be 0..2000 milliseconds."); return; }
  const xvalue* versionSupport=get(get(get(get(params,"capabilities"),"textDocument"),"publishDiagnostics"),"versionSupport");
  if(versionSupport && !xrtValueGetBool(versionSupport,&versions)) { response(s,id,NULL,-32602,"Diagnostic versionSupport must be boolean."); return; }
  const xvalue* proofSupport=get(get(get(params,"capabilities"),"experimental"),"diagnosticInputProof");
  if(proofSupport && !xrtValueGetBool(proofSupport,&inputProof)) { response(s,id,NULL,-32602,"Diagnostic input proof capability must be boolean."); return; }
  const xvalue* dependencySupport=get(get(get(params,"capabilities"),"experimental"),"diagnosticDependencyProof");
  if(dependencySupport && !xrtValueGetBool(dependencySupport,&dependencyProof)) { response(s,id,NULL,-32602,"Diagnostic dependency proof capability must be boolean."); return; }
  if(dependencyProof && !inputProof) { response(s,id,NULL,-32602,"Diagnostic dependency proofs require diagnosticInputProof."); return; }
  bool overlayProof=false,conditional=false;
  const xvalue* overlaySupport=get(get(get(params,"capabilities"),"experimental"),"diagnosticOverlayProof");
  const xvalue* conditionalOption=get(options,"automaticDiagnosticsIfSupported");
  if((overlaySupport && !xrtValueGetBool(overlaySupport,&overlayProof)) || (conditionalOption && !xrtValueGetBool(conditionalOption,&conditional)) ||
     (overlayProof && (!dependencyProof || !versions)) || (conditional && (!overlayProof || s->cliDiagnostics))) {
   response(s,id,NULL,-32602,"Diagnostic overlay proofs require dependency proofs and version support; conditional automatic diagnostics require native overlay proofs.");return;
  }
  automatic=automatic || conditional;
  const xvalue* definitionProofSupport=get(get(get(params,"capabilities"),"experimental"),"definitionInputProof");
  const xvalue* referenceProofSupport=get(get(get(params,"capabilities"),"experimental"),"referenceInputProof");bool referenceProof=false;
  if(referenceProofSupport && !xrtValueGetBool(referenceProofSupport,&referenceProof)){response(s,id,NULL,-32602,"referenceInputProof must be boolean.");return;}
  if(definitionProofSupport && !xrtValueGetBool(definitionProofSupport,&definitionProof)) { response(s,id,NULL,-32602,"Definition input proof capability must be boolean."); return; }
  const xvalue* rangeLimit=get(get(get(get(params,"capabilities"),"textDocument"),"foldingRange"),"rangeLimit"); int64 limit=65536;
  if(rangeLimit && (!xrtValueGetInt(rangeLimit,&limit) || limit<0 || limit>UINT32_MAX)) { response(s,id,NULL,-32602,"Folding rangeLimit must be a nonnegative 32-bit integer."); return; }
  s->foldingLimit=(size_t)(limit>65536 ? 65536 : limit);
  const xvalue* configuration=get(options,"analysisConfiguration");xide_model_error configurationError={0};
  if(configuration && !configureAnalysis(s,configuration,&configurationError)) {response(s,id,NULL,-32602,configurationError.message);return;}
  if(automatic && !s->diagnostics && !(s->diagnostics=xideDiagnosticStoreCreateWithDependencies(NULL,dependencyProof))) { response(s,id,NULL,-32603,"Unable to create the automatic diagnostic actor."); return; }
  s->automaticDiagnostics=automatic; s->diagnosticVersions=versions; s->diagnosticInputProof=inputProof; s->diagnosticDependencyProof=dependencyProof;s->diagnosticOverlayProof=overlayProof; s->definitionInputProof=definitionProof;s->referenceInputProof=referenceProof; s->diagnosticDelay=delay; s->diagnosticSweepAt=GetTickCount64()+1000;
  xvalue *result=xrtValueObject(),*cap=xrtValueObject(),*sync=xrtValueObject(),*save=xrtValueObject(),*info=xrtValueObject(),*scope=xrtValueObject();
  put(cap,"positionEncoding",xrtValueString(sv("utf-16"))); put(sync,"openClose",xrtValueBool(true)); put(sync,"change",xrtValueInt(2)); put(save,"includeText",xrtValueBool(true)); put(sync,"save",save); put(cap,"textDocumentSync",sync);
  put(cap,"foldingRangeProvider",xrtValueBool(true)); put(scope,"residentSyntax",xrtValueBool(true)); put(scope,"analysisRuntime",xrtValueString(sv("compiler-xrt")));
  put(scope,"nativeCursorCompletion",xrtValueBool(!s->cliCompletion)); put(scope,"completionBackend",xrtValueString(sv(s->cliCompletion ? "cli" : "native")));
  put(scope,"analysisConfiguration",xrtValueInt(!s->cliCompletion && !s->cliDiagnostics ? 1 : 0));put(scope,"analysisProfile",xrtValueString(sv(XIDE_ANALYSIS_PROFILE)));
  put(scope,"retainedDiskEntry",xrtValueInt(!s->cliCompletion && !s->cliDiagnostics ? 1 : 0));
  put(scope,"retainedEntryParse",xrtValueInt(!s->cliCompletion ? 1 : 0));
  put(scope,"retainedModuleParse",xrtValueInt(!s->cliCompletion ? 1 : 0));
  put(scope,"retainedCursorCompletion",xrtValueBool(!s->cliCompletion));
  put(scope,"retainedCursorSyntax",xrtValueBool(!s->cliCompletion));
  put(scope,"automaticSemanticQueries",xrtValueBool(!s->cliDiagnostics));
  put(scope,"automaticDiagnostics",xrtValueBool(automatic)); put(scope,"diagnosticDelay",xrtValueInt(delay));
  put(scope,"diagnosticInputProof",xrtValueBool(true));
  put(scope,"diagnosticDependencyProof",xrtValueBool(true));
  put(scope,"diagnosticOverlayProof",xrtValueBool(true));
  put(scope,"inputState",xrtValueBool(true));
  put(scope,"definitionInputProof",xrtValueBool(true));
  put(scope,"referenceInputProof",xrtValueBool(!s->cliCompletion && !s->cliDiagnostics));
  put(scope,"nativeSnapshotDiagnostics",xrtValueBool(!s->cliDiagnostics)); put(scope,"diagnosticsBackend",xrtValueString(sv(s->cliDiagnostics ? "cli" : "native")));
  put(scope,"retainedSemanticChecks",xrtValueBool(!s->cliDiagnostics)); put(scope,"retainedNativeInputs",xrtValueBool(!s->cliDiagnostics || !s->cliCompletion));
  put(scope,"incrementalEntrySyntax",xrtValueBool(!s->cliDiagnostics));
  put(scope,"incrementalFunctionSemantics",xrtValueBool(!s->cliDiagnostics));
  put(scope,"savedDiagnosticsBackend",xrtValueString(sv("cli")));
  put(scope,"savedEntryDiagnostics",xrtValueBool(true)); put(scope,"unsavedDependencies",xrtValueBool(true)); put(scope,"snapshotDiagnostics",xrtValueBool(true)); put(scope,"snapshotDocumentFingerprint",xrtValueBool(true)); put(scope,"snapshotInputGuard",xrtValueString(sv("background-poll"))); put(scope,"cSemantics",xrtValueBool(false)); put(scope,"snapshotDefinition",xrtValueBool(true)); put(cap,"definitionProvider",xrtValueBool(true)); put(cap,"hoverProvider",xrtValueBool(true)); put(cap,"documentSymbolProvider",xrtValueBool(true)); xvalue *completion=xrtValueObject(),*triggers=xrtValueArray(),*dot=xrtValueString(sv(".")); xrtValueArrayAppend(triggers,dot); xrtValueRelease(dot); put(completion,"resolveProvider",xrtValueBool(false)); put(completion,"triggerCharacters",triggers); put(cap,"completionProvider",completion); put(scope,"cursorCompletion",xrtValueBool(true)); put(scope,"completionInputGuard",xrtValueString(sv("background-poll"))); put(scope,"snapshotHover",xrtValueBool(true)); put(scope,"snapshotDocumentSymbols",xrtValueBool(true)); put(cap,"experimental",scope); put(result,"capabilities",cap);
  xvalue* signature=xrtValueObject();xvalue* signatureTriggers=xrtValueArray();xvalue* retriggers=xrtValueArray();
  xvalue *open=xrtValueString(sv("(")),*comma=xrtValueString(sv(",")),*close=xrtValueString(sv(")"));
  xrtValueArrayAppend(signatureTriggers,open);xrtValueArrayAppend(signatureTriggers,comma);xrtValueArrayAppend(retriggers,close);xrtValueRelease(open);xrtValueRelease(comma);xrtValueRelease(close);
  put(signature,"triggerCharacters",signatureTriggers);put(signature,"retriggerCharacters",retriggers);put(cap,"signatureHelpProvider",signature);put(scope,"signatureInputGuard",xrtValueString(sv("background-poll")));
  put(cap,"documentHighlightProvider",xrtValueBool(true));
#ifdef XLS_COMPILER_XRT
  if(!s->cliCompletion && !s->cliDiagnostics)put(cap,"referencesProvider",xrtValueBool(true));
#endif
  put(info,"name",xrtValueString(sv("xlang-ls"))); put(info,"version",xrtValueString(sv("0.85"))); put(result,"serverInfo",info); s->initialized=1; s->checkOnSave=check; response(s,id,result,0,NULL); return;
 }
 if(!s->initialized) { if(id) response(s,id,NULL,-32002,"Server not initialized."); return; }
 if(s->shutdown) { if(id) response(s,id,NULL,-32600,"Server has shut down."); return; }
 if(!strcmp(method,"shutdown")) { if(id) { s->shutdown=1; s->pending=-1;
#ifdef XLS_COMPILER_XRT
 cancelReferences(s,NULL,-32800,"Server shutting down.");
#endif
 diagnosticStop(s); cancelNative(s,-1,-32800,"Server shutting down."); clearPublished(s); cancel(s,-32800,"Server shutting down."); cancelSemantic(s,-1,-32800,"Server shutting down."); cancelCompletion(s,-32800,"Server shutting down.",1);cancelSignature(s,-32800,"Server shutting down.",1); response(s,id,NULL,0,NULL); } return; }
 if(!strcmp(method,"initialized")) { if(id) response(s,id,NULL,-32600,"initialized must be a notification."); return; }
 if(!strcmp(method,"$/cancelRequest")) {
  if(id) response(s,id,NULL,-32600,"Cancellation must be a notification.");
  else {
   const xvalue* cancelledId=get(params,"id");
#ifdef XLS_COMPILER_XRT
   if(cancelledId)cancelReferences(s,cancelledId,-32800,"Request cancelled.");
#endif
   for(size_t i=0;i<DOC_LIMIT;++i) if(s->native[i].query && sameId(s->native[i].id,cancelledId)) { xlsQueryCancel(s->native[i].query); response(s,s->native[i].id,NULL,-32800,"Request cancelled."); releaseNative(&s->native[i]); }
   for(size_t i=0;i<DOC_LIMIT;++i) if(s->semantic[i].id && sameId(s->semantic[i].id,cancelledId)) { response(s,s->semantic[i].id,NULL,-32800,"Request cancelled."); releaseSemantic(&s->semantic[i]); }
   cancelUnusedSemanticCheck(s);
   if(s->checkId && sameId(s->checkId,cancelledId)) cancel(s,-32800,"Request cancelled.");
   if(s->completionId && sameId(s->completionId,cancelledId)) cancelCompletion(s,-32800,"Request cancelled.",0);
   if(s->completionPendingId && sameId(s->completionPendingId,cancelledId)) { response(s,s->completionPendingId,NULL,-32800,"Request cancelled."); xrtValueRelease(s->completionPendingId); s->completionPendingId=NULL; }
   if(s->signatureId && sameId(s->signatureId,cancelledId)) cancelSignature(s,-32800,"Request cancelled.",0);
   if(s->signaturePendingId && sameId(s->signaturePendingId,cancelledId)) { response(s,s->signaturePendingId,NULL,-32800,"Request cancelled."); xrtValueRelease(s->signaturePendingId); s->signaturePendingId=NULL; }
  }
  return;
 }
 if(!strcmp(method,"textDocument/foldingRange")) { if(id) requestFolding(s,id,params); return; }
 if(!strcmp(method,"xlang/analysisStats")) { if(id) analysisStats(s,id); return; }
 if(!strcmp(method,"xlang/diagnosticStats")) { if(id) diagnosticStats(s,id); return; }
 if(!strcmp(method,"xlang/inputState")) { if(id) inputState(s,id,params); return; }
 if(!strcmp(method,"workspace/didChangeWatchedFiles")) { if(id) response(s,id,NULL,-32600,"Watched file changes must be a notification."); else {
#ifdef XLS_COMPILER_XRT
 referenceWatched(s,params);
#endif
 diagnosticWatched(s,params);} return; }
 if(!strcmp(method,"textDocument/signatureHelp")){if(id)requestSignature(s,id,params);return;}
 if(!strcmp(method,"textDocument/completion")) { if(id) requestCompletion(s,id,params); return; }
 if(!strcmp(method,"xlang/configureAnalysis") || !strcmp(method,"workspace/didChangeConfiguration")) {
  int standard=!strcmp(method,"workspace/didChangeConfiguration");xide_model_error error={0};
  if((standard && id) || (!standard && !id)) {if(id) response(s,id,NULL,-32600,"Invalid configuration request/notification form.");return;}
  const xvalue* configuration=standard ? get(get(get(params,"settings"),"xlang"),"analysisConfiguration") : params;
  if(standard && !configuration) return;
  if(!configureAnalysis(s,configuration,&error)) {if(id) response(s,id,NULL,-32602,error.message);else diagnosticLog(s,error.message);return;}
  if(id) {xvalue* value=xrtValueObject();put(value,"schema",xrtValueInt(1));put(value,"epoch",xrtValueInt(s->configurationEpoch));put(value,"projects",xrtValueInt((int64)xideAnalysisCatalogCount(s->catalog)));response(s,id,value,0,NULL);}return;
 }
 #ifdef XLS_COMPILER_XRT
 if(!strcmp(method,"textDocument/references")){if(id)requestReferences(s,id,params);return;}
 if(!strcmp(method,"xlang/referenceStats")){if(id)referenceStats(s,id);return;}
#endif
 if(!strcmp(method,"textDocument/definition")) { if(id) semanticQuery(s,id,params,0); return; }
 if(!strcmp(method,"textDocument/hover")) { if(id) semanticQuery(s,id,params,1); return; }
 if(!strcmp(method,"textDocument/documentSymbol")) { if(id) semanticQuery(s,id,params,2); return; }
 if(!strcmp(method,"textDocument/documentHighlight")) { if(id) semanticQuery(s,id,params,3); return; }
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
 s.cache=xideToolchainsCreate(); s.analysis=xlsWorkerCreate(NULL,DOC_LIMIT);
#ifdef XLS_COMPILER_XRT
 s.modules=xlsModuleStoreCreate(NULL);s.semanticAnalysis=xlsWorkerCreateWithModules(NULL,8,NULL,s.modules);s.cursorAnalysis=xlsWorkerCreateWithModules(NULL,8,NULL,s.modules);s.signatureAnalysis=xlsWorkerCreateWithModules(NULL,8,NULL,s.modules);
#else
 s.semanticAnalysis=xlsWorkerCreate(NULL,8);s.cursorAnalysis=xlsWorkerCreate(NULL,8);s.signatureAnalysis=xlsWorkerCreate(NULL,8);
#endif
 InitializeCriticalSection(&s.lock); s.event=CreateEventW(NULL,FALSE,FALSE,NULL);
 if(s.compiler && s.directory && s.cache && s.analysis && s.semanticAnalysis && s.cursorAnalysis && s.signatureAnalysis && s.event) s.reader=CreateThread(NULL,0,reader,&s,0,NULL);
 if(s.reader) while(!s.exitRequested && !s.writeFailed) {
  EnterCriticalSection(&s.lock); message* m=s.head; if(m) { s.head=m->next; if(!s.head) s.tail=NULL; s.queueBytes-=m->size; --s.queueCount; s.queueInputs-=(size_t)m->input; } int eof=s.eof,failed=s.readFailed; LeaveCriticalSection(&s.lock);
  if(m) { dispatch(&s,m->value); xrtValueRelease(m->value); free(m); } else if(eof) { s.exitCode=failed ? 2 : 1; break; }
  pollGuard(&s); pollCheck(&s); pollCompletion(&s); pollSignature(&s); pollNative(&s); pollSemantic(&s); diagnosticPoll(&s);
#ifdef XLS_COMPILER_XRT
  pollReferences(&s);
#endif
  if(!m) {
   HANDLE waits[5]={s.event}; DWORD count=1;
   if(s.signatureCancelled){HANDLE h=xideLanguageCheckWorkerSignal(s.signature);if(h)waits[count++]=h;}
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
 for(size_t i=0;i<DOC_LIMIT;++i) if(s.semantic[i].id) releaseSemantic(&s.semantic[i]);
 #ifdef XLS_COMPILER_XRT
 cancelReferences(&s,NULL,0,"Server stopped.");xideProjectReferencesDestroy(s.references);
#endif
 diagnosticStop(&s); xideDiagnosticStoreDestroy(s.diagnostics); xrtValueRelease(s.diagnosticAnalysis);xrtValueRelease(s.diagnosticOverlayState);
 xlsWorkerDestroy(s.analysis);
 xideLanguageCheckDestroy(s.signature);xrtValueRelease(s.signatureId);xrtValueRelease(s.signaturePendingId);free(s.signatureUri);xlsWorkerDestroy(s.signatureAnalysis);
 xideLanguageCheckDestroy(s.completion); xrtValueRelease(s.completionId); xrtValueRelease(s.completionPendingId); free(s.completionUri); xideLanguageCheckDestroy(s.check); xideLanguageCheckDestroy(s.guard); xlsWorkerDestroy(s.semanticAnalysis); xlsWorkerDestroy(s.cursorAnalysis); xrtValueRelease(s.guardId); free(s.guardUri); xrtValueRelease(s.checkId); xrtValueRelease(s.publishedGroups); xideToolchainsDestroy(s.cache);
#ifdef XLS_COMPILER_XRT
 xlsModuleStoreRelease(s.modules);
#endif
 while(s.head) { message* m=s.head; s.head=m->next; xrtValueRelease(m->value); free(m); }
 for(int i=0;i<DOC_LIMIT;++i) clearDocument(&s,i);
 xideAnalysisCatalogDestroy(s.catalog);
 if(s.event) { CloseHandle(s.event); } DeleteCriticalSection(&s.lock); free(s.compiler); free(s.directory); return s.writeFailed ? 2 : s.exitCode;
}
