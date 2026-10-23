#include "ui/language_view.h"
#include "editor/lsp_coordinates.h"
#include "ui/c_language_view.h"
#include "ui/editor_view.h"
#include "ui/task_view.h"
#include "services/language_client.h"
#include "services/language_check.h"
#include "services/highlight_result.h"
#include "services/sync_plan.h"
#include "services/diagnostic_stream.h"
#include "platform/file_watch.h"
#include "platform/path.h"
#include "platform/file_io.h"
#include "platform/startup_alias.h"
#include "protocol/document_sync.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#define SNAPSHOT_DOCS 64u
#define SNAPSHOT_GROUPS 576u
#define TEXT_LIMIT (8u*1024u*1024u)
typedef struct document_binding { xide_document* document; uint64_t id; uint32_t version; int wireVersion,externalState; char *path,*uri,*language;wchar_t* key; const char* text; xvalue* textValue; size_t length; } document_binding;
typedef struct xide_automatic_diagnostics xide_automatic_diagnostics;
typedef struct annotation_binding { xide_document* document; uint64_t id; uint32_t version; int externalState; } annotation_binding;
typedef struct semantic_editor_binding { struct semantic_editor_binding* next; xide_app* app; xui_widget editor; xui_widget_event_proc handlers[8]; void* users[8]; int composing; } semantic_editor_binding;
typedef struct highlight_intent { xide_document* document; uint64_t id,stamp; uint32_t version; int offset,check,wire; } highlight_intent;
typedef struct outline_item { const xvalue* symbol; int parent; char* title; } outline_item;
struct xide_language_view {
 xide_app* app; document_binding inputs[SNAPSHOT_DOCS]; size_t inputCount;
 annotation_binding annotations[SNAPSHOT_GROUPS]; size_t annotationCount;
 char* packagePath; int packageCaptured,lastSnapshotId,retryInputs,retryFailures;
 xvalue* snapshotResult; int snapshotMode,snapshotSupported,syncKind,nativeBackend; xls_sync* sync; uint64_t recheckAt,observedEdits;
 int onDemandMode,onDemandSupported,receiptRequest,receiptReady,checkRetries; char receiptDigest[65]; size_t receiptDocuments; uint64_t semanticRefreshAt,checkRetryAt,preemptDeadline;
 xide_language_client* client; char *compiler,*directory,*uri,*path; xide_document *document,*pendingSave;
 xvalue* diagnostics; uint64_t documentId; uint32_t version; int wireVersion,requestId,nextId,initialized,busy,waitingInitialize,invalidated,automatic;
 int definitionRequest,definitionPending,definitionCheck,definitionSupported,definitionProofSupported; xide_document* definitionDocument; uint64_t definitionDocumentId; uint32_t definitionDocumentVersion; int definitionCaret;
 xui_widget outline; char* outlineTitle; xvalue *outlineResult,*hoverResult; outline_item* outlineItems; size_t outlineCount;
 xide_document *outlineDocument,*hoverDocument; uint64_t outlineId,hoverId; uint32_t outlineVersion,hoverVersion;
 int outlineRequest,outlineCheck,outlineSupported,hoverRequest,hoverCheck,hoverSupported,hoverOffset,hoverPointer,hoverX,hoverY; uint64_t hoverAt;
 semantic_editor_binding* bindings;
 xide_highlight_decoder* highlightDecoder; xide_highlight_result* highlightResult;
 highlight_intent highlight,highlightSuppressed; char highlightReceipt[65];
 uint64_t highlightAt,highlightToken,highlightSerial,highlightReplyUntil;
 int highlightSupported,highlightPreparing,highlightRequest,highlightDecoding;
 size_t highlightApplied; LARGE_INTEGER highlightClock;
 /* A single owner-held response bounds backpressure while the worker retires
    a large message. UI clones share only read-only atomic-refcounted leaves. */
 xvalue* deferredMessage; int deferredDiscard; uint64_t highlightSession,deferredSession;
 xide_sync_planner* syncPlanner;xide_sync_result* syncReady;
 uint64_t syncToken,syncSerial,syncSession,syncDeadline;int syncMode,syncSubmitted;
 xide_automatic_diagnostics* autoView;int autoEnabled,autoProbed,autoManual;
 char message[768];
};
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* o,const char* key) { return o && xrtValueType(o)==XVALUE_OBJECT ? xrtValueObjectGet(o,sv(key)) : NULL; }
static const char* string(const xvalue* v) { xstrview s={0}; return v && xrtValueGetString(v,&s) && !memchr(s.Data,0,s.Size) && s.Size<=INT_MAX && (!s.Size || MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.Data,(int)s.Size,NULL,0)) ? s.Data : NULL; }
static const char* boundedString(const xvalue* value,size_t limit)
{xstrview text={0};return xrtValueGetString(value,&text) && text.Size<=limit ? string(value) : NULL;}
static int put(xvalue* o,const char* key,xvalue* v) { return v && xrtValueObjectSetNew(o,sv(key),v); }
static char* copy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static int number(const xvalue* value,int* n) { int64 x=0; if(!value || !xrtValueGetInt(value,&x) || x<0 || x>INT_MAX) return 0; *n=(int)x; return 1; }
static int eligible(xide_document* d) { return d && d->open && d->path && !d->dirty && !d->externalState && !strcmp(d->language,"xlang") && xidePathHasExtension(d->path,".xl") && xuiCodeDocumentGetLength(xuiCodeEditGetDocument(d->editor))>=0 && xuiCodeDocumentGetLength(xuiCodeEditGetDocument(d->editor))<=8*1024*1024; }
static int namedXlang(xide_document* d) { return d && d->open && d->path && xidePathHasExtension(d->path,".xl"); }
static int snapshotConflict(xide_document* d) { return d->externalState && d->externalState!=XIDE_WATCH_MISSING; }
static int canSnapshot(xide_app* app,xide_document* root)
{
 if(!namedXlang(root) || strcmp(root->language,"xlang") || snapshotConflict(root)) return 0;
 size_t count=0,bytes=0;
 for(size_t i=0;i<app->documents.count;++i) { xide_document* d=app->documents.items[i]; if(!namedXlang(d)) continue;
  int length=xuiCodeDocumentGetLength(xuiCodeEditGetDocument(d->editor));
  if(snapshotConflict(d) || ++count>SNAPSHOT_DOCS || length<0 || (size_t)length>TEXT_LIMIT || (size_t)length>32u*1024u*1024u-bytes) return 0;
  bytes+=(size_t)length;
 } return count>0;
}
/* Scheduling hint only; acceptance still compares every document exactly. */
static uint64_t editStamp(xide_app* app)
{
 uint64_t stamp=UINT64_C(14695981039346656037);
 for(size_t i=0;i<app->documents.count;++i) { xide_document* d=app->documents.items[i]; if(!namedXlang(d)) continue;
  stamp=(stamp^d->id)*UINT64_C(1099511628211); stamp=(stamp^xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor)))*UINT64_C(1099511628211); stamp=(stamp^(unsigned)d->externalState)*UINT64_C(1099511628211);
  for(const unsigned char* p=(const unsigned char*)d->path;*p;++p) stamp=(stamp^*p)*UINT64_C(1099511628211);
  for(const unsigned char* p=(const unsigned char*)d->language;*p;++p) stamp=(stamp^*p)*UINT64_C(1099511628211);
 } return stamp;
}
static int automaticInputsCurrent(xide_language_view* v);
static int snapshotCurrent(xide_language_view* v)
{
 if(v->autoEnabled) return automaticInputsCurrent(v);
 if(!canSnapshot(v->app,v->document)) return 0;
 size_t count=0;
 for(size_t i=0;i<v->app->documents.count;++i) if(namedXlang(v->app->documents.items[i])) ++count;
 if(count!=v->inputCount) return 0;
 for(size_t i=0;i<v->inputCount;++i) { document_binding* b=&v->inputs[i]; xide_document* d=b->document;
  if(!namedXlang(d) || d->id!=b->id || d->externalState!=b->externalState || strcmp(d->path,b->path) || strcmp(d->language,b->language) || b->version!=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor))) return 0;
 }
 for(size_t i=0;i<v->annotationCount;++i) { annotation_binding* b=&v->annotations[i]; xide_document* d=b->document;
  if(!d->open || d->id!=b->id || d->externalState!=b->externalState || b->version!=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor))) return 0;
 } return 1;
}
static int current(xide_language_view* v) { return !v->invalidated && (v->snapshotMode ? snapshotCurrent(v) : eligible(v->document)) && v->document->id==v->documentId && v->path && !strcmp(v->path,v->document->path) && v->version==xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(v->document->editor)); }
static int sendMessage(xide_language_view* v,const char* method,xvalue* params,int id)
{
 xvalue* m=xrtValueObject(); int okay=m && put(m,"jsonrpc",xrtValueString(sv("2.0"))) && put(m,"method",xrtValueString(sv(method)));
 if(okay && params) { okay=put(m,"params",params); params=NULL; } if(okay && id) okay=put(m,"id",xrtValueInt(id));
 xide_model_error e={0}; if(okay) okay=xideLanguageClientSend(v->client,m,&e); xrtValueRelease(m); xrtValueRelease(params);
 if(!okay) { snprintf(v->message,sizeof(v->message),"%s",*e.message ? e.message : "无法发送语言服务请求。"); } return okay;
}
static void cancelQuery(xide_language_view* v,int id)
{ if(id && v->client && v->initialized) { xvalue* p=xrtValueObject(); if(p && put(p,"id",xrtValueInt(id))) sendMessage(v,"$/cancelRequest",p,0); else xrtValueRelease(p); } }
static void clearSemantic(xide_app* app);
static void clearHighlights(xide_language_view* v);
static void suppressHighlights(xide_language_view* v);
static void pollHighlights(xide_app* app);
static int takeHighlights(xide_language_view* v,xvalue* message);
static void cancelSyncPlan(xide_language_view* v);
static void pollSyncPlan(xide_app* app);
static int beginSyncPlan(xide_app* app,int mode);
static int createOutline(xide_app* app);
static void pollSemantic(xide_app* app);
static void applyOutline(xide_app* app,const xvalue* result,const xvalue* error);
static void applyHover(xide_app* app,const xvalue* result,const xvalue* error);
static int automaticCreate(xide_language_view* v);
static void automaticDestroy(xide_language_view* v);
static void automaticReset(xide_language_view* v);
static void automaticPoll(xide_app* app);
static int automaticBlocked(xide_language_view* v);
static int automaticReceive(xide_language_view* v,xvalue* message,size_t bytes);
static int automaticInputsCurrent(xide_language_view* v);
static int automaticPrepare(xide_app* app,int manual);
static void automaticSynced(xide_language_view* v,const xide_sync_result* result);
static void automaticReceipt(xide_language_view* v,uint64_t* serial);
static int automaticReceiptMatches(xide_language_view* v,const xvalue* result);
static void automaticPause(xide_language_view* v);
static void automaticResume(xide_language_view* v);
static void automaticSaved(xide_language_view* v,xide_document* document);
static void automaticManualFinished(xide_language_view* v);
static void automaticGuard(xide_language_view* v,const xvalue* params);
static size_t automaticCount(xide_language_view* v);
static int automaticJump(xide_app* app,size_t token);
static void clearDiagnostics(xide_app* app)
{
 cancelSyncPlan(app->languageView);
 xide_language_view* v=app->languageView; clearSemantic(app); if(!v->autoEnabled && v->document && v->document->id==v->documentId && v->document->editor) xuiCodeAnnotationClearDiagnostics(xuiCodeEditGetAnnotations(v->document->editor));
 for(size_t i=0;!v->autoEnabled && i<v->annotationCount;++i) if(v->annotations[i].document->id==v->annotations[i].id && v->annotations[i].document->editor) xuiCodeAnnotationClearDiagnostics(xuiCodeEditGetAnnotations(v->annotations[i].document->editor));
 v->annotationCount=0; xrtValueRelease(v->snapshotResult); v->snapshotResult=NULL;
 cancelQuery(v,v->definitionRequest); v->definitionRequest=v->definitionPending=0;
 cancelQuery(v,v->receiptRequest); v->receiptRequest=v->receiptReady=0;
 xrtValueRelease(v->diagnostics); v->diagnostics=NULL; xideTaskViewClearLanguageProblems(app); xgeRenderRequest();
}
static void freeInput(document_binding* b)
{ free(b->path); free(b->uri);free(b->key); xrtValueRelease(b->textValue); free(b->language); memset(b,0,sizeof(*b)); }
static void freeInputs(xide_language_view* v)
{ for(size_t i=0;i<v->inputCount;++i) freeInput(&v->inputs[i]); v->inputCount=0; }
static int syncEmit(void* user,const char* method,const xvalue* params)
{ return sendMessage(user,method,xrtValueRetain((xvalue*)params),0); }
static void closeDocument(xide_language_view* v)
{
 if(v->sync && v->client && v->initialized) xlsSyncUpdate(v->sync,NULL,0,syncEmit,v);
 free(v->uri); v->uri=NULL;
}
static void reset(xide_app* app)
{
 ++app->languageView->highlightSession;
 xide_language_view* v=app->languageView;automaticReset(v); clearDiagnostics(app); xideLanguageClientStop(v->client); v->client=NULL; free(v->compiler); free(v->directory); free(v->uri); free(v->path);
 v->compiler=v->directory=v->uri=v->path=NULL; free(v->packagePath); v->packagePath=NULL; v->packageCaptured=0; freeInputs(v); xlsSyncDestroy(v->sync); v->sync=NULL; v->snapshotSupported=v->syncKind=v->nativeBackend=0; v->onDemandMode=v->onDemandSupported=v->definitionProofSupported=0; v->semanticRefreshAt=v->checkRetryAt=v->preemptDeadline=0; v->initialized=v->busy=v->waitingInitialize=0; v->invalidated=1; v->document=NULL; v->requestId=0; v->automatic=0; v->recheckAt=0; v->highlightSupported=v->highlightPreparing=0;
 v->autoEnabled=v->autoProbed=v->autoManual=0;
}
static char* serverPath(void)
{
 wchar_t module[32768]; DWORD n=GetModuleFileNameW(NULL,module,32768); if(!n || n>=32768) return NULL;
 char path[131072]; if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,xideStartupResourceModule(module),-1,path,sizeof(path),NULL,NULL)) return NULL;
 char* directory=xidePathDirectory(path); char* server=directory ? xidePathResolveLexical(directory,"xlang-ls.exe") : NULL; free(directory); return server;
}
static int beginSnapshots(xide_app* app);
static int beginCheck(xide_app* app)
{
 if(app->languageView->snapshotMode) return beginSnapshots(app);
 return beginSyncPlan(app,3);
}
static int finishSavedSync(xide_app* app)
{
 xide_language_view* v=app->languageView;if(!current(v)) return 0;document_binding* root=NULL;
 for(size_t i=0;i<v->inputCount;++i) if(v->inputs[i].document==v->document && v->inputs[i].id==v->documentId) root=v->inputs+i;
 if(!root) return 0;
 free(v->uri);v->uri=copy(root->uri);if(!v->uri) return 0;v->wireVersion=root->wireVersion;
 xvalue *p=xrtValueObject(),*d=xrtValueObject(); put(d,"uri",xrtValueString(sv(v->uri))); put(p,"textDocument",d); put(p,"version",xrtValueInt(v->wireVersion)); if(v->nextId==INT_MAX) return 0; v->requestId=++v->nextId;
 if(!sendMessage(v,"xlang/checkSaved",p,v->requestId)) return 0;
 v->waitingInitialize=0; snprintf(v->message,sizeof(v->message),"后台检查保存后的 xlang 单入口源码…"); return 1;
}
static int captureInput(xide_document* d,document_binding* b)
{
 b->document=d;b->id=d->id;b->externalState=d->externalState;b->version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor));
 b->path=copy(d->path);b->uri=xideLspPathToUri(d->path,NULL);b->language=copy(d->language);b->key=xidePathNativeWide(d->path);
 const char* text=xuiCodeEditGetText(d->editor);b->length=strlen(text);
 if(b->length!=(size_t)xuiCodeDocumentGetLength(xuiCodeEditGetDocument(d->editor))) return 0;
 b->textValue=xrtValueString((xstrview){text,b->length});xstrview value={0};
 if(!b->textValue || !xrtValueGetString(b->textValue,&value)) return 0;
 b->text=value.Data ? value.Data : "";return b->path && b->uri && b->language && b->key;
}
static int captureInputs(xide_app* app)
{
 cancelSyncPlan(app->languageView);
 xide_language_view* v=app->languageView; document_binding previous[SNAPSHOT_DOCS]={0}; size_t count=v->inputCount;
 memcpy(previous,v->inputs,count*sizeof(*previous)); memset(v->inputs,0,sizeof(v->inputs)); v->inputCount=0;
 int okay=1;
 for(size_t i=0;i<app->documents.count && okay;++i) { xide_document* d=app->documents.items[i]; if(!namedXlang(d) || (v->autoEnabled && snapshotConflict(d))) continue;
  if(v->inputCount>=SNAPSHOT_DOCS) { okay=0; break; }
  document_binding* b=&v->inputs[v->inputCount++]; uint32_t version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor));
  for(size_t j=0;j<count;++j) if(previous[j].document==d && previous[j].id==d->id && previous[j].version==version &&
      previous[j].externalState==d->externalState && !strcmp(previous[j].path,d->path) && !strcmp(previous[j].language,d->language)) {
   *b=previous[j]; memset(&previous[j],0,sizeof(previous[j])); break;
  }
  if(b->document) continue;
  okay=captureInput(d,b);
 }
 for(size_t i=0;i<count;++i) freeInput(&previous[i]);
 if(!okay) freeInputs(v);
 return okay && (v->autoEnabled || v->inputCount>0);
}
static int beginSnapshots(xide_app* app)
{
 xide_language_view* v=app->languageView;
 if(!v->snapshotSupported) { v->busy=v->waitingInitialize=0; v->automatic=0; snprintf(v->message,sizeof(v->message),"语言服务不支持可验证的文档快照；请重新构建 IDE。"); return 0; }
 if(!current(v)) { v->busy=v->waitingInitialize=0; return 0; }
 return beginSyncPlan(app,1);
}
static int finishSnapshotSync(xide_app* app)
{
 xide_language_view* v=app->languageView;document_binding* root=NULL;if(!current(v)) return 0;
 for(size_t i=0;i<v->inputCount;++i) if(v->inputs[i].document==v->document && v->inputs[i].id==v->documentId) root=v->inputs+i;
 if(!root) return 0;
 xvalue *p=xrtValueObject(),*d=xrtValueObject(); put(d,"uri",xrtValueString(sv(root->uri))); put(p,"textDocument",d); put(p,"version",xrtValueInt(root->wireVersion));
 if(v->nextId==INT_MAX) return 0;
 v->requestId=++v->nextId; v->lastSnapshotId=v->requestId; if(!sendMessage(v,"xlang/check",p,v->requestId)) return 0;
 v->waitingInitialize=0; snprintf(v->message,sizeof(v->message),"后台检查 xlang 文档及 %zu 份打开文档快照…",v->inputCount); return 1;
}
static void languageWake(void* user) { (void)user; xgeRenderRequest(); }
static int connectClient(xide_app* app,xide_document* d)
{
 xide_language_view* v=app->languageView;
 automaticResume(v);
 const char* compiler=xideTaskViewCompiler(app); char* directory=xidePathDirectory(d->path); if(!compiler || !directory) { free(directory); return 0; }
 char* package=NULL; xide_model_error environmentError={0};
 if(!xideLanguageCheckPackagePath(&package,&environmentError)) { free(directory); snprintf(v->message,sizeof(v->message),"包搜索环境无法读取；旧诊断已撤销。"); clearDiagnostics(app); return 0; }
 int samePackage=v->packageCaptured && (package && v->packagePath ? !strcmp(package,v->packagePath) : package==v->packagePath);
 char stateMessage[768]={0}; enum xide_language_client_state state=xideLanguageClientState(v->client,stateMessage,sizeof(stateMessage));
 if(!v->client || state>=XIDE_LS_STOPPED || !samePackage || !v->compiler || !xidePathEqual(compiler,v->compiler) || ((!v->autoView || app->disableAutomaticDiagnostics || (v->autoProbed && !v->autoEnabled)) && !xidePathEqual(directory,v->directory))) {
  reset(app); v->packagePath=package; package=NULL; v->packageCaptured=1; char* program=serverPath(); xide_model_error e={0}; v->compiler=copy(compiler); v->directory=copy(directory); v->nextId=1;
  v->client=program && v->compiler && v->directory ? xideLanguageClientStart(program,compiler,directory,&e) : NULL; free(program);
  if(!v->client) { snprintf(v->message,sizeof(v->message),"%s",*e.message ? e.message : "无法启动 xlang-ls；请重新构建 IDE。"); free(directory); return 0; }
  xideLanguageClientSetWake(v->client,languageWake,app);
  xvalue *p=xrtValueObject(),*options=xrtValueObject(),*capabilities=xrtValueObject(),*experimental=xrtValueObject();
  if(v->autoView && !app->disableAutomaticDiagnostics) {
   xvalue* td=xrtValueObject(),*diagnostics=xrtValueObject();put(diagnostics,"versionSupport",xrtValueBool(true));put(td,"publishDiagnostics",diagnostics);put(capabilities,"textDocument",td);
   put(experimental,"diagnosticInputProof",xrtValueBool(true));put(experimental,"diagnosticDependencyProof",xrtValueBool(true));put(experimental,"diagnosticOverlayProof",xrtValueBool(true));
   put(options,"automaticDiagnosticsIfSupported",xrtValueBool(true));
  }
  put(experimental,"definitionInputProof",xrtValueBool(true)); put(capabilities,"experimental",experimental); put(p,"capabilities",capabilities);
  put(options,"checkOnSave",xrtValueBool(false)); put(options,"automaticDiagnostics",xrtValueBool(false)); put(p,"initializationOptions",options);
  if(!sendMessage(v,"initialize",p,1)) { free(directory); return 0; } v->waitingInitialize=1;
 }
 free(package); free(directory); return 1;
}
static int checkMode(xide_app* app,int snapshots,xide_document* d)
{
 xide_language_view* v=app->languageView; int wasOnDemand=v->onDemandMode;
 if(!(snapshots ? canSnapshot(app,d) : eligible(d)) || v->busy || !connectClient(app,d)) return 0;
 if(v->autoEnabled) return automaticPrepare(app,snapshots ? 1 : 2);
 v->pendingSave=NULL; v->onDemandMode=0; v->semanticRefreshAt=v->checkRetryAt=0; v->checkRetries=0; v->preemptDeadline=wasOnDemand ? GetTickCount64()+3000 : 0;
 clearDiagnostics(app); free(v->uri); v->uri=NULL; if(!snapshots) freeInputs(v); v->snapshotMode=snapshots; v->recheckAt=0; v->observedEdits=editStamp(app); free(v->path); v->path=copy(d->path); v->document=d; v->documentId=d->id; v->version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor)); v->invalidated=0; v->busy=1; v->automatic=1;
 if(!v->path || (snapshots && !captureInputs(app)) || (!snapshots && !(v->inputCount=1,captureInput(d,&v->inputs[0]))) || (v->initialized && !beginCheck(app))) { reset(app); v->automatic=0; v->recheckAt=0; return 0; }
 snprintf(v->message,sizeof(v->message),"%s",snapshots ? "后台检查 xlang 文档与未保存依赖…" : "后台检查保存后的 xlang 单入口源码…"); xgeRenderRequest(); return 1;
}
static int beginOnDemand(xide_app* app)
{
 xide_language_view* v=app->languageView;
 v->waitingInitialize=0;
 if(!v->onDemandSupported || !v->hoverSupported || !v->outlineSupported) {
  v->waitingInitialize=0; v->invalidated=1; snprintf(v->message,sizeof(v->message),"当前语言服务需要先检查 xlang 文档，再查询符号。"); return 0;
 }
 return beginSyncPlan(app,2);
}
static int finishOnDemandSync(xide_app* app)
{
 xide_language_view* v=app->languageView;if(!current(v) || v->nextId==INT_MAX) return 0;
 v->receiptRequest=++v->nextId; v->receiptReady=0; v->waitingInitialize=0;
 automaticReceipt(v,NULL);
 if(!sendMessage(v,"xlang/inputState",xrtValueObject(),v->receiptRequest)) { v->receiptRequest=0; return 0; }
 return 1;
}
static void cancelSyncPlan(xide_language_view* v)
{
 if(!v) return;
 if(v->syncMode==1 || v->syncMode==3) v->busy=0;
 xideSyncPlannerCancel(v->syncPlanner);
 v->syncMode=v->syncSubmitted=0;v->syncToken=v->syncDeadline=0;
 /* A held large batch/snapshot is retired by pollSyncPlan, never freed here. */
}
static int beginSyncPlan(xide_app* app,int mode)
{
 xide_language_view* v=app->languageView;
 if(!v->syncPlanner || !v->sync || (mode==4 ? !automaticInputsCurrent(v) : !current(v)) || (!v->inputCount && mode!=4) || v->syncSerial==UINT64_MAX) return 0;
 int busy=v->busy;cancelSyncPlan(v);v->busy=busy;
 v->syncMode=mode;v->syncToken=++v->syncSerial;v->syncSession=v->highlightSession;
 v->syncDeadline=GetTickCount64()+5000;v->receiptReady=0;
 xgeRenderRequest();return 1;
}
static void failSyncPlan(xide_app* app)
{
 reset(app);
 snprintf(app->languageView->message,sizeof(app->languageView->message),"文档同步准备失败或发送队列持续拥塞；再次查询会重新连接。");
}
static void pollSyncPlan(xide_app* app)
{
 xide_language_view* v=app->languageView;
 if(v->syncReady && (!v->syncMode || v->syncReady->token!=v->syncToken)) {
  if(xideSyncPlannerRetire(v->syncPlanner,v->syncReady)) v->syncReady=NULL;
  else {xgeRenderRequest();return;}
 }
 if(!v->syncMode) return;
 if(v->syncSession!=v->highlightSession || (v->syncMode==4 ? !automaticInputsCurrent(v) : !current(v))) {cancelSyncPlan(v);return;}
 if(!v->syncSubmitted) {
  xide_sync_input inputs[SNAPSHOT_DOCS];
  for(size_t i=0;i<v->inputCount;++i) inputs[i]=(xide_sync_input){v->inputs[i].uri,v->inputs[i].textValue,v->inputs[i].id,v->inputs[i].version};
  int submitted=xideSyncPlannerSubmit(v->syncPlanner,v->syncToken,v->sync,inputs,v->inputCount);
  if(submitted<0 || (!submitted && GetTickCount64()>=v->syncDeadline)) {failSyncPlan(app);return;}
  if(!submitted) {xgeRenderRequest();return;}
  v->syncSubmitted=1;v->syncDeadline=0;
 }
 if(!v->syncReady) v->syncReady=xideSyncPlannerTake(v->syncPlanner);
 xide_sync_result* result=v->syncReady;
 if(!result) return; /* The worker wakes the owner after publishing. */
 if(result->token!=v->syncToken) {xgeRenderRequest();return;}
 if(!result->valid || result->count!=v->inputCount) {failSyncPlan(app);return;}
 if(!xideLanguageBatchSend(v->client,result->batch)) {
  if(!v->syncDeadline) v->syncDeadline=GetTickCount64()+1000;
  if(xideLanguageClientState(v->client,NULL,0)>=XIDE_LS_STOPPED || GetTickCount64()>=v->syncDeadline) failSyncPlan(app);
  else xgeRenderRequest();
  return;
 }
 /* Admission splices the complete encoded batch before the query. Only now
    does the owner adopt the corresponding wire versions and exact receipt. */
 result->batch=NULL;xls_sync* previous=v->sync;v->sync=result->sync;result->sync=previous;
 memcpy(v->receiptDigest,result->digest,sizeof(v->receiptDigest));v->receiptDocuments=result->count;
 for(size_t i=0;i<v->inputCount;++i) v->inputs[i].wireVersion=result->versions[i];
 automaticSynced(v,result);
 int mode=v->syncMode;v->syncMode=v->syncSubmitted=0;v->syncToken=v->syncDeadline=0;result->token=0;
 if(xideSyncPlannerRetire(v->syncPlanner,result)) v->syncReady=NULL;
 int okay=1;
 if(mode==4) {
  if(v->autoManual) {okay=v->autoManual==1 ? finishSnapshotSync(app) : finishSavedSync(app);v->autoManual=0;}
  else if(v->document && current(v)) okay=finishOnDemandSync(app);
 } else okay=mode==1 ? finishSnapshotSync(app) : mode==2 ? finishOnDemandSync(app) : finishSavedSync(app);
 if(!okay) failSyncPlan(app);
}
static int prepareSymbols(xide_app* app)
{
 xide_language_view* v=app ? app->languageView : NULL; xide_document* d=app ? app->activeDocument : NULL;
 if(!v || v->busy || !canSnapshot(app,d) || !xideTaskViewCompiler(app)) return 0;
 if(v->autoEnabled) return automaticPrepare(app,0);
 if(v->snapshotMode && current(v)) {
  bool indexed=false;
  if((v->onDemandMode && (v->receiptReady || v->receiptRequest || v->waitingInitialize)) ||
     v->syncMode || (!v->onDemandMode && xrtValueGetBool(get(v->snapshotResult,"semanticDefinitions"),&indexed) && indexed)) return 1;
 }
 if(v->automatic) return 0;
 if(!connectClient(app,d)) return 0;
 clearDiagnostics(app); v->pendingSave=NULL; v->snapshotMode=1; v->onDemandMode=1; v->lastSnapshotId=0; v->automatic=0; v->recheckAt=v->semanticRefreshAt=0;
 free(v->path); v->path=copy(d->path); v->document=d; v->documentId=d->id; v->version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor)); v->invalidated=0; v->observedEdits=editStamp(app);
 if(!v->path || !captureInputs(app) || (v->initialized && !beginOnDemand(app))) { v->invalidated=1; return 0; }
 snprintf(v->message,sizeof(v->message),"正在准备 xlang 悬停与大纲…");
 xgeRenderRequest(); return 1;
}
static int check(xide_app* app) { return checkMode(app,0,app->activeDocument); }
int xideLanguageViewCreate(xide_app* app)
{
 app->languageView=calloc(1,sizeof(*app->languageView)); xide_language_view* v=app->languageView;
 if(!v) return XUI_ERROR_OUT_OF_MEMORY;
 v->app=app; v->highlightDecoder=xideHighlightDecoderCreate(languageWake,app);v->syncPlanner=xideSyncPlannerCreate(languageWake,app);
 if(!v->highlightDecoder || !v->syncPlanner || !automaticCreate(v) || !QueryPerformanceFrequency(&v->highlightClock)) return XUI_ERROR_OUT_OF_MEMORY;
 return createOutline(app);
}
void xideLanguageViewStop(xide_app* app) { if(app->languageView) {automaticPause(app->languageView); suppressHighlights(app->languageView); reset(app); } }
void xideLanguageViewDestroy(xide_app* app)
{ xide_language_view* v=app->languageView; if(v) { xideLanguageClientStop(v->client);automaticDestroy(v); xideSyncPlannerDestroy(v->syncPlanner);xideSyncResultRelease(v->syncReady);xideHighlightDecoderDestroy(v->highlightDecoder); xrtValueRelease(v->deferredMessage); xideHighlightResultRelease(v->highlightResult); xlsSyncDestroy(v->sync); free(v->compiler); free(v->directory); free(v->uri); free(v->path); free(v->packagePath); xrtValueRelease(v->diagnostics); xrtValueRelease(v->snapshotResult); freeInputs(v); while(v->bindings) { semantic_editor_binding* next=v->bindings->next; free(v->bindings); v->bindings=next; } free(v); app->languageView=NULL; } }
int xideLanguageViewCanCheck(xide_app* app) { return app && app->languageView && !app->languageView->busy && eligible(app->activeDocument) && xideTaskViewCompiler(app); }
int xideLanguageViewCanCheckCurrent(xide_app* app) { if(app && app->activeDocument && !strcmp(app->activeDocument->language,"c")) return xideCLanguageViewCanCheck(app); return app && app->languageView && !app->languageView->busy && canSnapshot(app,app->activeDocument) && xideTaskViewCompiler(app); }
int xideLanguageViewCanDefine(xide_app* app)
{
 if(app && app->activeDocument && !strcmp(app->activeDocument->language,"c")) return xideCLanguageViewCanQuery(app,1);
 xide_language_view* v=app ? app->languageView : NULL; xide_document* d=app ? app->activeDocument : NULL; bool indexed=false;
 if(!v || !d || !d->open || !d->path || strcmp(d->language,"xlang") || v->busy || v->definitionRequest || v->definitionPending) return 0;
 if(v->definitionSupported && v->snapshotMode && current(v) && xrtValueGetBool(get(v->snapshotResult,"semanticDefinitions"),&indexed) && indexed)
  for(size_t i=0;i<v->inputCount;++i) if(v->inputs[i].document==d && v->inputs[i].id==d->id) return 1;
 return !v->automatic && (!v->initialized || (v->onDemandSupported && v->definitionSupported && v->definitionProofSupported)) && canSnapshot(app,d) && xideTaskViewCompiler(app);
}
static int definitionIntentCurrent(xide_app* app)
{
 xide_language_view* v=app->languageView; xide_document* d=v->definitionDocument; xui_code_selection_t selection={0};
 return current(v) && v->definitionCheck==v->lastSnapshotId && d && d==app->activeDocument && d->open && d->id==v->definitionDocumentId && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor))==v->definitionDocumentVersion && xuiCodeSelectionGetState(xuiCodeEditGetSelection(d->editor),&selection)==XUI_OK && selection.iCaretOffset==v->definitionCaret;
}
static int sendDefinition(xide_app* app)
{
 xide_language_view* v=app->languageView; xide_document* d=v->definitionDocument; document_binding* b=NULL;
 if(!definitionIntentCurrent(app) || !v->initialized || (v->onDemandMode && (!v->receiptReady || !v->definitionProofSupported)) || v->nextId==INT_MAX) return 0;
 for(size_t i=0;i<v->inputCount;++i) if(v->inputs[i].document==d) b=&v->inputs[i];
 int line=0,character=0;
 if(!b || !xideCodeDocumentOffsetToLsp(xuiCodeEditGetDocument(d->editor),v->definitionCaret,&line,&character)) return 0;
 xvalue *p=xrtValueObject(),*item=xrtValueObject(),*pos=xrtValueObject();
 int okay=p && item && pos && put(item,"uri",xrtValueString(sv(b->uri))) && put(pos,"line",xrtValueInt(line)) && put(pos,"character",xrtValueInt(character));
 if(okay) { okay=put(p,"textDocument",item); item=NULL; } if(okay) { okay=put(p,"position",pos); pos=NULL; }
 if(okay) okay=put(p,"version",xrtValueInt(b->wireVersion)) && (v->onDemandMode || put(p,"checkId",xrtValueInt(v->lastSnapshotId)));
 xrtValueRelease(item); xrtValueRelease(pos);
 if(!okay) { xrtValueRelease(p); return 0; }
 v->definitionPending=0; v->definitionRequest=++v->nextId;
 if(!sendMessage(v,"textDocument/definition",p,v->definitionRequest)) { v->definitionRequest=0; return 0; } return 1;
}
static int requestDefinition(xide_app* app)
{
 if(!xideLanguageViewCanDefine(app) || !prepareSymbols(app)) return 0;
 xide_language_view* v=app->languageView; xide_document* d=app->activeDocument; xui_code_selection_t selection={0};
 if(xuiCodeSelectionGetState(xuiCodeEditGetSelection(d->editor),&selection)!=XUI_OK) return 0;
 v->definitionPending=1; v->definitionCheck=v->lastSnapshotId; v->definitionDocument=d; v->definitionDocumentId=d->id;
 v->definitionDocumentVersion=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor)); v->definitionCaret=selection.iCaretOffset;
 if(v->initialized && (!v->onDemandMode || v->receiptReady)) { if(sendDefinition(app)) return 1; v->definitionPending=0; return 0; }
 xgeRenderRequest(); return 1;
}
static void pollDefinition(xide_app* app)
{
 xide_language_view* v=app->languageView;
 if(!v->definitionPending && !v->definitionRequest) return;
 if(!definitionIntentCurrent(app)) { cancelQuery(v,v->definitionRequest); v->definitionPending=v->definitionRequest=0; return; }
 if(v->definitionPending && v->initialized && !v->waitingInitialize && !v->receiptRequest && !v->syncMode) {
  if(v->onDemandMode && !v->definitionProofSupported) { v->definitionPending=0; snprintf(v->message,sizeof(v->message),"当前语言服务需要先检查 xlang 文档，再转到定义。"); }
  else if(!sendDefinition(app)) v->definitionPending=0;
 }
}
int xideLanguageViewCanCancel(xide_app* app) { xide_language_view* v=app ? app->languageView : NULL; return xideCLanguageViewCanCancel(app) || (v && (v->autoEnabled || v->syncMode || v->highlightRequest || v->highlightDecoding || v->highlightApplied || v->highlightResult || v->busy || (v->snapshotMode && v->automatic) || v->recheckAt || v->semanticRefreshAt || (v->onDemandMode && (v->waitingInitialize || v->receiptRequest || v->definitionPending || v->definitionRequest || v->hoverRequest || v->outlineRequest)))); }
int xideLanguageViewBusy(xide_app* app) { return app && app->languageView && app->languageView->busy; }
static int showOutline(xide_app* app);
static int requestCaretHover(xide_app* app);
int xideLanguageViewAction(xide_app* app,int action)
{
 if(action==XIDE_ACTION_CANCEL_CHECK) xideCLanguageViewAction(app,action);
 if(app && app->activeDocument && !strcmp(app->activeDocument->language,"c") && action!=XIDE_ACTION_CHECK_SAVED && action!=XIDE_ACTION_CANCEL_CHECK) return xideCLanguageViewAction(app,action);
 if(action==XIDE_ACTION_OUTLINE) return showOutline(app);
 if(action==XIDE_ACTION_HOVER) return requestCaretHover(app);
 if(action==XIDE_ACTION_DEFINITION) return requestDefinition(app);
 if(action==XIDE_ACTION_CHECK_CURRENT) { if(app->languageView) app->languageView->retryInputs=app->languageView->retryFailures=0; return checkMode(app,1,app->activeDocument); }
 if(action==XIDE_ACTION_CHECK_SAVED) return check(app);
 if(action==XIDE_ACTION_CANCEL_CHECK && app->languageView) { xide_language_view* v=app->languageView; v->pendingSave=NULL; v->automatic=0; v->recheckAt=v->semanticRefreshAt=v->preemptDeadline=0;
  if(v->autoEnabled) {automaticPause(v);reset(app);snprintf(v->message,sizeof(v->message),"已暂停 xlang 自动检查；修改源码或再次查询可恢复。");return 1;}
  suppressHighlights(v);
  if(v->checkRetryAt) { v->checkRetryAt=0; v->busy=0; }
  if(v->requestId && v->busy) { xvalue* p=xrtValueObject(); put(p,"id",xrtValueInt(v->requestId)); sendMessage(v,"$/cancelRequest",p,0); }
  v->invalidated=1; clearDiagnostics(app); closeDocument(v); if(v->waitingInitialize) v->busy=0; snprintf(v->message,sizeof(v->message),"已请求取消源码检查。"); return 1;
 } return 0;
}
void xideLanguageViewSaved(xide_app* app,xide_document* document,int saved)
{ xide_language_view* v=app->languageView; if(v && saved && v->autoEnabled) automaticSaved(v,document);else if(v && saved && v->automatic && document==v->document && document->id==v->documentId) v->pendingSave=document; }
static int apply(xide_app* app,const xvalue* params)
{
 xide_language_view* v=app->languageView; const char* uri=string(get(params,"uri")); int version=0; const xvalue* list=get(params,"diagnostics");
 if(!current(v) || !uri || !v->uri || strcmp(uri,v->uri) || !number(get(params,"version"),&version) || version!=v->wireVersion) return 1;
 if(!list || xrtValueType(list)!=XVALUE_ARRAY || xrtValueCount(list)>4096) return 0;
 size_t n=xrtValueCount(list); xui_code_diagnostic_t* annotations=calloc(n+1,sizeof(*annotations)); char** titles=calloc(n+1,sizeof(*titles)); xide_lsp_position* positions=calloc(n*2+1,sizeof(*positions)); xvalue* owned=xrtValueDeepClone(list); int okay=annotations && titles && positions && owned; const char* text=xuiCodeEditGetText(v->document->editor);
 for(size_t i=0;okay && i<n;++i) {
  const xvalue* d=xrtValueArrayGet(owned,i),*range=get(d,"range"); const char* message=string(get(d,"message")); int sl=0,sc=0,el=0,ec=0,severity=0;
  okay=message && strlen(message)<=65535 && number(get(d,"severity"),&severity) && severity==1 && number(get(get(range,"start"),"line"),&sl) && number(get(get(range,"start"),"character"),&sc) && number(get(get(range,"end"),"line"),&el) && number(get(get(range,"end"),"character"),&ec);
  if(okay) { positions[i*2]=(xide_lsp_position){.line=sl,.character=sc}; positions[i*2+1]=(xide_lsp_position){.line=el,.character=ec}; }
 }
 if(okay) okay=xideLspPositionsToOffsets(text,positions,n*2);
 for(size_t i=0;okay && i<n;++i) {
  const char* message=string(get(xrtValueArrayGet(owned,i),"message")); int start=positions[i*2].offset,end=positions[i*2+1].offset;
  if(end<start) { okay=0; break; }
  annotations[i]=(xui_code_diagnostic_t){.iSize=sizeof(*annotations),.tRange={start,end},.iSeverity=XUI_CODE_DIAGNOSTIC_ERROR,.sMessage=message,.sSource="xlang saved entry",.iUserData=i};
  size_t length=strlen(v->path)+strlen(message)+100; titles[i]=malloc(length); if(titles[i]) snprintf(titles[i],length,"%s:%d:%d · %s",v->path,positions[i*2].line+1,positions[i*2].character+1,message); else okay=0;
 }
 if(okay) okay=xuiCodeAnnotationSetDiagnostics(xuiCodeEditGetAnnotations(v->document->editor),annotations,(int)n)==XUI_OK;
 if(okay) { xrtValueRelease(v->diagnostics); v->diagnostics=owned; owned=NULL; okay=xideTaskViewShowLanguageProblems(app,(const char* const*)titles,n,1)==XUI_OK; }
 for(size_t i=0;i<n;++i) free(titles ? titles[i] : NULL);
 free(titles); free(annotations); free(positions); xrtValueRelease(owned); xgeRenderRequest(); return okay;
}

static int hexDigest(const xvalue* value)
{ const char* text=string(value); if(!text || strlen(text)!=64) return 0; for(int i=0;i<64;++i) if(!((text[i]>='0' && text[i]<='9') || (text[i]>='a' && text[i]<='f'))) return 0; return 1; }
static int matchesHash(const void* bytes,size_t size,const xvalue* value)
{ unsigned char digest[32]; char hex[65]; if(!hexDigest(value) || !xrtSha256(bytes,size,digest)) return 0; for(int i=0;i<32;++i) snprintf(hex+i*2,3,"%02x",digest[i]); return !strcmp(hex,string(value)); }
static int textMatches(const xvalue* group,xide_document* d)
{ int size=0; const char* text=xuiCodeEditGetText(d->editor); return number(get(group,"normalizedSize"),&size) && (size_t)size==strlen(text) && matchesHash(text,(size_t)size,get(group,"normalizedSha256")); }
static xide_document* openPath(xide_app* app,const char* path)
{ for(size_t i=0;i<app->documents.count;++i) { xide_document* d=app->documents.items[i]; if(d->open && d->path && xidePathLexicalEqual(path,d->path)) return d; } return NULL; }
static const xvalue* snapshotGroup(xide_language_view* v,const char* path)
{ const xvalue* groups=get(v->snapshotResult,"documents"); for(size_t i=0;i<xrtValueCount(groups);++i) { const xvalue* g=xrtValueArrayGet(groups,i); if(xidePathLexicalEqual(path,string(get(g,"path")))) return g; } return NULL; }
static document_binding* inputPath(xide_language_view* v,const char* path)
{ for(size_t i=0;i<v->inputCount;++i) if(xidePathLexicalEqual(path,v->inputs[i].path)) return &v->inputs[i]; return NULL; }
static int formatted(const xvalue* value,const char* text) { const char* s=string(value); return s && !strcmp(s,text); }
static int diskIdentity(const char* path,const xvalue* group)
{
 const xvalue* input=get(group,"input"),*m=get(input,"metadata"); xfileinfo info={0}; int size=0; int64 available=0,attributes=0; char text[32];
 if(!xideIoReceiptStat(path,&info) || info.Type!=XFILE_TYPE_FILE || !number(get(input,"rawSize"),&size) || (uint64)size!=info.Size || !xrtValueGetInt(get(m,"available"),&available) || available!=info.Available || !xrtValueGetInt(get(m,"attributes"),&attributes) || attributes!=info.Attributes) return 0;
 snprintf(text,sizeof(text),"%016llx",(unsigned long long)info.Device); if(!formatted(get(m,"device"),text)) return 0;
 snprintf(text,sizeof(text),"%016llx",(unsigned long long)info.Identity); if(!formatted(get(m,"identity"),text)) return 0;
 snprintf(text,sizeof(text),"%lld",(long long)info.Modified); if(!formatted(get(m,"modifiedUs"),text)) return 0;
 snprintf(text,sizeof(text),"%lld",(long long)info.Changed); return formatted(get(m,"changedUs"),text);
}
static int diagnosticPositions(const xvalue* diagnostic,xide_lsp_position* positions)
{
 const xvalue* range=get(diagnostic,"range"); int severity=0; const char* message=string(get(diagnostic,"message"));
 if(!message || !*message || strlen(message)>65535 || !number(get(diagnostic,"severity"),&severity) || severity!=1 || !number(get(get(range,"start"),"line"),&positions[0].line) || !number(get(get(range,"start"),"character"),&positions[0].character) || !number(get(get(range,"end"),"line"),&positions[1].line) || !number(get(get(range,"end"),"character"),&positions[1].character)) return 0;
 return positions[1].line>positions[0].line || (positions[1].line==positions[0].line && positions[1].character>=positions[0].character);
}
typedef struct annotation_batch { xide_document* document; xui_code_diagnostic_t* diagnostics; size_t count; } annotation_batch;
static int applySnapshots(xide_app* app,const xvalue* result)
{
 xide_language_view* v=app->languageView; if(!current(v)) return 1;
 const xvalue* incoming=get(result,"documents"); int count=0,version=0; document_binding* root=inputPath(v,v->path);
 if(!root || !number(get(result,"version"),&version) || version!=root->wireVersion || !formatted(get(result,"scope"),"entry-with-open-overlays") || !formatted(get(result,"guardScope"),"background-poll") || !number(get(result,"count"),&count) || count>4096 || !incoming || xrtValueType(incoming)!=XVALUE_ARRAY || xrtValueCount(incoming)<v->inputCount || xrtValueCount(incoming)>SNAPSHOT_GROUPS) return 0;
 int checkId=0; if(!number(get(result,"checkId"),&checkId) || checkId!=v->lastSnapshotId) return 0;
 size_t groupsCount=xrtValueCount(incoming),at=0,overlayCount=0; xvalue* owned=xrtValueDeepClone(result),*flat=xrtValueArray();
 annotation_batch* batches=calloc(groupsCount,sizeof(*batches)); char** titles=calloc((size_t)count+1,sizeof(*titles)); int okay=owned && flat && batches && titles;
 const xvalue* groups=get(owned,"documents");
 for(size_t i=0;okay && i<groupsCount;++i) {
  const xvalue* g=xrtValueArrayGet(groups,i); const char *path=string(get(g,"path")),*origin=string(get(g,"origin")); const xvalue* list=get(g,"diagnostics"); int size=0;
  char* uri=path ? xideLspPathToUri(path,NULL) : NULL;
  okay=uri && origin && number(get(g,"normalizedSize"),&size) && (size_t)size<=TEXT_LIMIT && hexDigest(get(g,"normalizedSha256")) && list && xrtValueType(list)==XVALUE_ARRAY && at<=(size_t)count && xrtValueCount(list)<=(size_t)count-at; free(uri);
  for(size_t j=0;okay && j<i;++j) if(xidePathLexicalEqual(path,string(get(xrtValueArrayGet(groups,j),"path")))) okay=0;
  if(!okay) break;
  document_binding* b=inputPath(v,path); xide_document* d=openPath(app,path);
  if(b) { int wire=0; okay=!strcmp(origin,"overlay") && number(get(g,"version"),&wire) && wire==b->wireVersion && d==b->document && d->id==b->id && textMatches(g,d); ++overlayCount; }
  else {
   const xvalue* input=get(g,"input"),*m=get(input,"metadata"); int rawSize=0; int64 available=0,attributes=0;
   okay=!strcmp(origin,"disk") && !get(g,"version") && formatted(get(input,"origin"),"disk") && xidePathLexicalEqual(path,string(get(input,"path"))) && number(get(input,"rawSize"),&rawSize) && (size_t)rawSize<=TEXT_LIMIT && hexDigest(get(input,"rawSha256")) && xrtValueGetInt(get(m,"available"),&available) && xrtValueGetInt(get(m,"attributes"),&attributes) && string(get(m,"device")) && string(get(m,"identity")) && string(get(m,"modifiedUs")) && string(get(m,"changedUs"));
   if(okay && d) okay=!d->externalState && textMatches(g,d) && matchesHash(d->baseline.raw,d->baseline.rawLength,get(input,"rawSha256")) && diskIdentity(path,g);
  }
  if(!okay) break;
  size_t n=xrtValueCount(list); xide_lsp_position* positions=calloc(n*2+1,sizeof(*positions)); batches[i].document=d; batches[i].count=n;
  if(d && n) batches[i].diagnostics=calloc(n,sizeof(*batches[i].diagnostics));
  okay=positions && (!d || !n || batches[i].diagnostics);
  for(size_t j=0;okay && j<n;++j) { const xvalue* diag=xrtValueArrayGet(list,j); okay=xidePathLexicalEqual(path,string(get(diag,"file"))) && diagnosticPositions(diag,positions+j*2); }
  if(okay && d) okay=xideLspPositionsToOffsets(xuiCodeEditGetText(d->editor),positions,n*2);
  for(size_t j=0;okay && j<n;++j) {
   const xvalue* diag=xrtValueArrayGet(list,j); const char* message=string(get(diag,"message")); size_t length=strlen(path)+strlen(message)+100;
   titles[at]=malloc(length); if(!titles[at]) { okay=0; break; }
   snprintf(titles[at],length,"%s:%d:%d · %s",path,positions[j*2].line+1,positions[j*2].character+1,message);
   if(d) batches[i].diagnostics[j]=(xui_code_diagnostic_t){.iSize=sizeof(xui_code_diagnostic_t),.tRange={positions[j*2].offset,positions[j*2+1].offset},.iSeverity=XUI_CODE_DIAGNOSTIC_ERROR,.sMessage=message,.sSource="xlang document snapshots",.iUserData=at};
   okay=xrtValueArrayAppend(flat,(xvalue*)diag); ++at;
  } free(positions);
 }
 okay=okay && at==(size_t)count && overlayCount==v->inputCount;
 if(okay) {
  clearDiagnostics(app); v->snapshotResult=owned; owned=NULL; v->diagnostics=flat; flat=NULL;
  for(size_t i=0;okay && i<groupsCount;++i) if(batches[i].document) {
   xide_document* d=batches[i].document; v->annotations[v->annotationCount++]=(annotation_binding){d,d->id,xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor)),d->externalState};
   okay=xuiCodeAnnotationSetDiagnostics(xuiCodeEditGetAnnotations(d->editor),batches[i].diagnostics,(int)batches[i].count)==XUI_OK;
  }
  if(okay) okay=xideTaskViewShowLanguageProblemsScoped(app,"xlang 文档快照检查（含打开文档的未保存依赖）",(const char* const*)titles,at,1)==XUI_OK;
  if(!okay) clearDiagnostics(app);
 }
 for(size_t i=0;i<groupsCount;++i) if(batches) free(batches[i].diagnostics);
 for(size_t i=0;i<(size_t)count+1;++i) if(titles) free(titles[i]);
 free(batches); free(titles); xrtValueRelease(owned); xrtValueRelease(flat); xgeRenderRequest(); return okay;
}
static void invalidateSnapshot(xide_app* app,const char* message)
{
 xide_language_view* v=app->languageView;
 int refresh=v->onDemandMode && v->outlineDocument!=NULL;
 if(v->checkRetryAt) { v->checkRetryAt=0; v->busy=0; }
 if(v->busy && v->requestId) { xvalue* p=xrtValueObject(); put(p,"id",xrtValueInt(v->requestId)); sendMessage(v,"$/cancelRequest",p,0); }
 v->invalidated=1; clearDiagnostics(app); v->retryInputs=1; v->retryFailures=0;
 if(v->automatic) v->recheckAt=GetTickCount64()+500;
 if(refresh) v->semanticRefreshAt=GetTickCount64()+75;
 snprintf(v->message,sizeof(v->message),"%s",message);
}

static void publishedInvalidation(xide_app* app,const xvalue* params)
{
 xide_language_view* v=app->languageView; int checkId=0,version=0,liveId=0;
 if(v->onDemandMode && v->receiptReady) {
  const char* uri=string(get(params,"uri")); const xvalue* batch=get(params,"checkId");
  if(!uri || !batch || xrtValueType(batch)!=XVALUE_NULL || !number(get(params,"version"),&version)) return;
  for(size_t i=0;i<v->inputCount;++i) if(!strcmp(uri,v->inputs[i].uri) && version==v->inputs[i].wireVersion) {
   invalidateSnapshot(app,"语义分析所用依赖已变化，正在更新文档大纲。"); return;
  }
  return;
 }
 if(!v->snapshotMode || !v->snapshotResult || !number(get(params,"checkId"),&checkId) || !number(get(v->snapshotResult,"checkId"),&liveId) || checkId!=liveId) return;
 const char* uri=string(get(params,"uri")); document_binding* root=NULL;
 for(size_t i=0;i<v->inputCount;++i) if(v->inputs[i].document==v->document && v->inputs[i].id==v->documentId) root=&v->inputs[i];
 if(!root || !uri || strcmp(uri,root->uri) || !number(get(params,"version"),&version) || version!=root->wireVersion) return;
 invalidateSnapshot(app,"检查所用磁盘输入、导入候选或工具链已变化；旧诊断已撤销，正在等待复查。");
}
static int jumpRange(xide_app* app,const char* path,const xvalue* range);
static int selectRange(xide_app* app,xide_document* target,const xvalue* range);
static int jumpDefinitionProof(xide_app* app,const char* path,const xvalue* range,const xvalue* proof)
{
 xide_language_view* v=app->languageView; int size=0,version=0;
 if(!v || !v->onDemandMode || !v->receiptReady || !current(v)) return 0;
 const char *declared=string(get(proof,"path")),*origin=string(get(proof,"origin"));
 if(!path || !declared || !origin || !xidePathLexicalEqual(path,declared) || !number(get(proof,"normalizedSize"),&size) || (size_t)size>TEXT_LIMIT || !hexDigest(get(proof,"normalizedSha256"))) return 0;
 document_binding* binding=inputPath(v,path); xide_document* target=openPath(app,path); int wasOpen=target!=NULL;
 if(!strcmp(origin,"overlay")) {
  if(!binding || !target || target!=binding->document || target->id!=binding->id || !number(get(proof,"version"),&version) || version!=binding->wireVersion || get(proof,"input") || !textMatches(proof,target)) return 0;
 } else if(!strcmp(origin,"disk")) {
  const xvalue* input=get(proof,"input"); const char* inputPathName=string(get(input,"path")); int rawSize=0;
  if(binding || get(proof,"version") || !formatted(get(input,"origin"),"disk") || !inputPathName || !xidePathLexicalEqual(path,inputPathName) || !number(get(input,"rawSize"),&rawSize) || (size_t)rawSize>TEXT_LIMIT || !hexDigest(get(input,"rawSha256")) || !diskIdentity(path,proof)) return 0;
  xide_model_error error={0};
  if(!target && !xideEditorOpen(app,path,&error)) return 0;
  if(!target) target=app->activeDocument;
  if(!target || !target->open || snapshotConflict(target) || !textMatches(proof,target) || !matchesHash(target->baseline.raw,target->baseline.rawLength,get(input,"rawSha256")) || !diskIdentity(path,proof)) return 0;
 } else return 0;
 if(!target || snapshotConflict(target) || !selectRange(app,target,range)) return 0;
 if(!wasOpen && namedXlang(target)) v->semanticRefreshAt=GetTickCount64()+75;
 return 1;
}
static void applyDefinition(xide_app* app,const xvalue* result,const xvalue* error)
{
 xide_language_view* v=app->languageView; v->definitionRequest=v->definitionPending=0;
 if(!definitionIntentCurrent(app) || (v->onDemandMode ? (!v->receiptReady || !v->definitionProofSupported) : !v->snapshotResult)) return;
 if(error) { const char* reason=string(get(error,"message")); snprintf(v->message,sizeof(v->message),"定义查询未完成：%.620s",reason ? reason : "服务错误"); return; }
 if(result && xrtValueType(result)==XVALUE_NULL) { snprintf(v->message,sizeof(v->message),"当前位置没有可验证的定义。"); return; }
 const char* uri=string(get(result,"uri")); char* path=uri ? xideLspUriToPath(uri,NULL) : NULL; int okay=0;
 if(v->onDemandMode) {
  int schema=0; const xvalue* extension=get(result,"xlang");
  if(number(get(extension,"schema"),&schema) && schema==1) okay=jumpDefinitionProof(app,path,get(result,"range"),get(extension,"document"));
 } else okay=path && snapshotGroup(v,path) && jumpRange(app,path,get(result,"range"));
 if(!okay) snprintf(v->message,sizeof(v->message),"定义位置与当前输入无法核对，未执行跳转。");
 else snprintf(v->message,sizeof(v->message),"已转到定义。");
 xuiStatusBarSetItemText(app->statusBar,0,v->message);
 free(path);
}
static int receiptGeneration(const xvalue* value)
{
 const char* text=string(value); if(!text || !*text) return 0;
 size_t n=strlen(text); if(n>20 || (n>1 && *text=='0') || (n==20 && strcmp(text,"18446744073709551615")>0)) return 0;
 for(size_t i=0;i<n;++i) if(text[i]<'0' || text[i]>'9') return 0;
 return 1;
}
static void applyInputState(xide_app* app,const xvalue* result,const xvalue* error)
{
 xide_language_view* v=app->languageView; v->receiptRequest=0;
 if(!v->onDemandMode || !current(v)) return;
 int schema=0,count=0;
 if(!error && number(get(result,"schema"),&schema) && schema==1 && number(get(result,"documents"),&count) && (size_t)count==v->receiptDocuments && formatted(get(result,"sha256"),v->receiptDigest) && receiptGeneration(get(result,"generation")) && automaticReceiptMatches(v,result)) v->receiptReady=1;
 else { reset(app); snprintf(v->message,sizeof(v->message),"语言服务的文档同步确认无效；请重试悬停或大纲。"); }
}
static int deferPreemptedCheck(xide_language_view* v,const xvalue* error)
{
 int64 code=0;
 if(!v->preemptDeadline || GetTickCount64()>=v->preemptDeadline || !current(v) || !xrtValueGetInt(get(error,"code"),&code) || code!=-32001) return 0;
 ++v->checkRetries; v->busy=1; v->checkRetryAt=GetTickCount64()+25;
 snprintf(v->message,sizeof(v->message),"正在切换到源码检查…"); return 1;
}
void xideLanguageViewPoll(xide_app* app)
{
 xide_language_view* v=app->languageView; if(!v) return;
 automaticPoll(app);
 if(!v->autoEnabled && v->snapshotMode && v->document && !v->invalidated && v->compiler) {
  const char* compiler=xideTaskViewCompiler(app); char* package=NULL; xide_model_error e={0}; int captured=xideLanguageCheckPackagePath(&package,&e);
  int same=captured && v->packageCaptured && (package && v->packagePath ? !strcmp(package,v->packagePath) : package==v->packagePath); free(package);
  if(!compiler || !xidePathLexicalEqual(compiler,v->compiler) || !same) invalidateSnapshot(app,"编译器配置或包搜索环境已变化；旧诊断已撤销，正在等待重新同步。");
 }
 uint64_t edited=editStamp(app); if(v->snapshotMode && v->automatic && edited!=v->observedEdits && v->invalidated) v->recheckAt=GetTickCount64()+500;
 if(v->onDemandMode && v->semanticRefreshAt && edited!=v->observedEdits) v->semanticRefreshAt=GetTickCount64()+75;
 v->observedEdits=edited;
 if(!v->autoEnabled && v->document && (!v->document->open || v->document->id!=v->documentId)) { if(v->checkRetryAt) { v->checkRetryAt=0; v->busy=0; } clearDiagnostics(app); closeDocument(v); v->document=NULL; v->pendingSave=NULL; v->automatic=0; v->recheckAt=v->semanticRefreshAt=0; v->invalidated=1; }
 if(!v->autoEnabled && v->document && !v->invalidated && !current(v)) {
  if(v->snapshotMode) invalidateSnapshot(app,"文档快照已变化，旧诊断已撤销；正在等待重新检查。");
  else { v->invalidated=1; clearDiagnostics(app); snprintf(v->message,sizeof(v->message),"文档已变化，旧诊断已撤销；保存后可重新检查。"); }
 }
 xvalue* m; int count=0;
 while(count++<128) {
  if(automaticBlocked(v)) break;
  if(v->deferredMessage && v->deferredSession!=v->highlightSession) v->deferredDiscard=1;
  if(v->deferredMessage && v->deferredDiscard) {
   if(!xideHighlightDecoderDiscard(v->highlightDecoder,v->deferredMessage)) break;
   v->deferredMessage=NULL;
  }
  m=v->deferredMessage; v->deferredMessage=NULL;
  size_t wireBytes=0;if(!m && !(m=xideLanguageClientNextSized(v->client,&wireBytes))) break;
  const char *rpc=boundedString(get(m,"jsonrpc"),16),*method=boundedString(get(m,"method"),128); int id=0;
  if(v->autoEnabled && method && (!strcmp(method,"xlang/diagnosticDependencies") || !strcmp(method,"textDocument/publishDiagnostics"))) {automaticReceive(v,m,wireBytes);m=NULL;}
  else if(!rpc || strcmp(rpc,"2.0")) { snprintf(v->message,sizeof(v->message),"语言服务返回了无效 JSON-RPC 响应。"); reset(app); }
  else if(method && !strcmp(method,"xlang/diagnosticsInvalidated")) {if(v->autoEnabled) automaticGuard(v,get(m,"params"));else publishedInvalidation(app,get(m,"params"));}
  else if(method && !strcmp(method,"textDocument/publishDiagnostics")) { if(!v->snapshotMode && !apply(app,get(m,"params"))) { snprintf(v->message,sizeof(v->message),"语言服务诊断范围无效；已撤销本次结果。"); reset(app); } }
  else if(v->autoEnabled && method && !strcmp(method,"window/logMessage")) {
   const char* message=boundedString(get(get(m,"params"),"message"),4096);
   if(message) {snprintf(v->message,sizeof(v->message),"xlang 后台分析：%.650s",message);if(app->activeDocument && !strcmp(app->activeDocument->language,"xlang")) xuiStatusBarSetItemText(app->statusBar,0,v->message);}
  }
  else if(number(get(m,"id"),&id)) {
   const xvalue* result=get(m,"result"),*error=get(m,"error"); const char* reason=boundedString(get(error,"message"),4096);
   if(id==1 && !v->initialized) {
    const xvalue* cap=get(result,"capabilities"); const char* encoding=string(get(cap,"positionEncoding")); bool supported=false;
    if(error || !encoding || strcmp(encoding,"utf-16") || !xrtValueGetBool(get(get(cap,"experimental"),"savedEntryDiagnostics"),&supported) || !supported) { snprintf(v->message,sizeof(v->message),"语言服务初始化失败：%s",reason ? reason : "诊断能力不兼容"); reset(app); }
    else {
     int64 kind=1; const xvalue* sync=get(cap,"textDocumentSync");
     if(xrtValueType(sync)==XVALUE_OBJECT) sync=get(sync,"change");
     if(sync && !xrtValueGetInt(sync,&kind)) kind=0;
     if(kind!=1 && kind!=2) kind=0;
     bool automatic=false,overlayProof=false,dependencyProof=false,inputProof=false;
     const xvalue* experimental=get(cap,"experimental");v->autoProbed=1;
     v->autoEnabled=v->autoView && xrtValueGetBool(get(experimental,"automaticDiagnostics"),&automatic) && automatic &&
      xrtValueGetBool(get(experimental,"diagnosticOverlayProof"),&overlayProof) && overlayProof &&
      xrtValueGetBool(get(experimental,"diagnosticDependencyProof"),&dependencyProof) && dependencyProof && xrtValueGetBool(get(experimental,"diagnosticInputProof"),&inputProof) && inputProof;
     v->syncKind=(int)kind; v->sync=v->autoEnabled ? xlsSyncCreateMonotonic((int)kind) : xlsSyncCreate((int)kind);
     if(!v->sync) { snprintf(v->message,sizeof(v->message),"语言服务初始化失败：文档同步能力不兼容。"); reset(app); }
     else {
      bool snapshots=false,fingerprint=false; v->snapshotSupported=xrtValueGetBool(get(get(cap,"experimental"),"snapshotDiagnostics"),&snapshots) && snapshots && xrtValueGetBool(get(get(cap,"experimental"),"snapshotDocumentFingerprint"),&fingerprint) && fingerprint;
      bool native=false; v->nativeBackend=xrtValueGetBool(get(get(cap,"experimental"),"nativeSnapshotDiagnostics"),&native) && native;
      bool definition=false; v->definitionSupported=xrtValueGetBool(get(get(cap,"experimental"),"snapshotDefinition"),&definition) && definition;
      bool definitionProof=false; v->definitionProofSupported=xrtValueGetBool(get(get(cap,"experimental"),"definitionInputProof"),&definitionProof) && definitionProof;
      bool hover=false,outline=false; v->hoverSupported=xrtValueGetBool(get(get(cap,"experimental"),"snapshotHover"),&hover) && hover; v->outlineSupported=xrtValueGetBool(get(get(cap,"experimental"),"snapshotDocumentSymbols"),&outline) && outline;
      bool onDemand=false,inputState=false; v->onDemandSupported=xrtValueGetBool(get(get(cap,"experimental"),"automaticSemanticQueries"),&onDemand) && onDemand && xrtValueGetBool(get(get(cap,"experimental"),"inputState"),&inputState) && inputState;
      bool highlights=false; const xvalue* highlight=get(cap,"documentHighlightProvider");
      v->highlightSupported=(xrtValueGetBool(highlight,&highlights) && highlights) || xrtValueType(highlight)==XVALUE_OBJECT;
      v->initialized=1;
      if(!v->busy && v->highlightPreparing && !v->highlightSupported) { v->onDemandMode=0; v->invalidated=1; v->highlightPreparing=0; }
      int manual=v->busy ? (v->snapshotMode ? 1 : 2) : 0;
      if(!sendMessage(v,"initialized",xrtValueObject(),0) || (v->autoEnabled ? !automaticPrepare(app,manual) : (v->busy && !beginCheck(app)) || (!v->busy && v->onDemandMode && !beginOnDemand(app)))) { v->busy=0; if(!v->invalidated) reset(app); }
      else v->waitingInitialize=0;
     }
    }
   } else if(v->receiptRequest && id==v->receiptRequest) applyInputState(app,result,error);
   else if(v->definitionRequest && id==v->definitionRequest) applyDefinition(app,result,error);
   else if(v->outlineRequest && id==v->outlineRequest) applyOutline(app,result,error);
   else if(v->hoverRequest && id==v->hoverRequest) applyHover(app,result,error);
   else if(v->highlightRequest && id==v->highlightRequest) {
    int accepted=takeHighlights(v,m);
    if(accepted>0) m=NULL;
    else if(accepted<0) { v->deferredMessage=m; v->deferredDiscard=0; v->deferredSession=v->highlightSession; m=NULL; }
   }
   else if(id==v->requestId) {
    v->busy=0; v->requestId=0;
    if(error) {
     if(!deferPreemptedCheck(v,error)) {
      v->preemptDeadline=0;
      if(!v->invalidated) clearDiagnostics(app);
      snprintf(v->message,sizeof(v->message),"源码检查结束：%s",reason ? reason : "语言服务错误");
     /* A temporarily unreadable/deleted input can fail without a new report.
        Keep retrying only the already invalidated automatic input batch, with
        bounded backoff; cancellation/root close still disables this schedule. */
     if(v->snapshotMode && v->automatic && v->retryInputs && !v->invalidated) {
      v->invalidated=1; if(v->retryFailures<5) ++v->retryFailures;
      unsigned delay=1000u << (v->retryFailures-1); if(delay>10000u) delay=10000u;
      v->recheckAt=GetTickCount64()+delay;
     }
     }
    }
    else if(current(v)) { int n=0; v->preemptDeadline=0;
     if(!number(get(result,"count"),&n) || (!v->autoEnabled && v->snapshotMode && !applySnapshots(app,result))) { snprintf(v->message,sizeof(v->message),"语言服务返回了无效快照结果；已撤销本次诊断。"); reset(app); }
     else if(v->autoEnabled) {automaticManualFinished(v);snprintf(v->message,sizeof(v->message),"xlang 检查完成：入口 %d 个问题；自动检查保留其他文件的有效结果。",n);}
     else if(v->snapshotMode) { v->retryInputs=v->retryFailures=0; snprintf(v->message,sizeof(v->message),"xlang 文档快照检查：%d 个错误，%zu 份打开文档；C 语义尚未接入。",n,v->inputCount); }
     else snprintf(v->message,sizeof(v->message),"保存后的 xlang 单入口检查：%d 个错误；未保存依赖和 C 语义尚未接入。",n);
    }
   }
  }
  if(m && !xideHighlightDecoderDiscard(v->highlightDecoder,m)) { v->deferredMessage=m; v->deferredDiscard=1; v->deferredSession=v->highlightSession; }
  if(v->deferredMessage) break;
 }
 if(v->client) { char error[768]={0}; enum xide_language_client_state state=xideLanguageClientState(v->client,error,sizeof(error)); if(state>=XIDE_LS_STOPPED) { int unexpected=v->busy || state==XIDE_LS_FAILED; reset(app); if(unexpected) snprintf(v->message,sizeof(v->message),"语言服务已结束：%.620s；再次检查会重新启动。",*error ? error : "进程退出"); } }
 if(!v->busy && v->pendingSave) { xide_document* d=v->pendingSave; v->pendingSave=NULL; if(v->snapshotMode) { if(v->automatic) v->recheckAt=GetTickCount64()+500; } else if(eligible(d)) checkMode(app,0,d); }
 if(v->checkRetryAt && GetTickCount64()>=v->checkRetryAt) { v->checkRetryAt=0; if(!current(v) || !beginCheck(app)) { v->busy=0; v->preemptDeadline=0; } }
 if(v->snapshotMode && v->automatic && v->recheckAt && !v->busy && GetTickCount64()>=v->recheckAt && v->document && v->document->open && v->document->id==v->documentId && canSnapshot(app,v->document)) checkMode(app,1,v->document);
 if(v->onDemandMode && v->semanticRefreshAt && !v->busy && GetTickCount64()>=v->semanticRefreshAt && canSnapshot(app,app->activeDocument)) { v->semanticRefreshAt=0; prepareSymbols(app); }
 pollSyncPlan(app);automaticPoll(app);pollDefinition(app); pollSemantic(app); pollHighlights(app);
 if(*v->message && (v->busy || app->activeDocument==v->document)) xuiStatusBarSetItemText(app->statusBar,0,v->message);
 if(v->busy || v->recheckAt || (v->semanticRefreshAt && canSnapshot(app,app->activeDocument))) xgeRenderRequest();
 xideActionsRefresh(app);
}
static int selectRange(xide_app* app,xide_document* target,const xvalue* range)
{
 int line=0,column=0,offset=0,endLine=0,endColumn=0,endOffset=0;
 if(!target || !number(get(get(range,"start"),"line"),&line) || !number(get(get(range,"start"),"character"),&column) || !number(get(get(range,"end"),"line"),&endLine) || !number(get(get(range,"end"),"character"),&endColumn) || !xideCodeDocumentLspToOffset(xuiCodeEditGetDocument(target->editor),line,column,&offset) || !xideCodeDocumentLspToOffset(xuiCodeEditGetDocument(target->editor),endLine,endColumn,&endOffset) || endOffset<offset || xideEditorShow(app,target)!=XUI_OK) return 0;
 xui_code_document document=xuiCodeEditGetDocument(target->editor); if(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(target->editor),document,offset,0)!=XUI_OK) return 0; xuiCodeEditEnsureCaretVisible(target->editor); xgeRenderRequest(); return 1;
}
static int jumpRange(xide_app* app,const char* path,const xvalue* range)
{
 xide_language_view* v=app->languageView; if(!v || !current(v)) return 0; xide_document* target=v->document;
 if(v->onDemandMode) {
  document_binding* binding=path ? inputPath(v,path) : NULL; target=path ? openPath(app,path) : NULL;
  if(!v->receiptReady || !binding || !target || target!=binding->document || target->id!=binding->id || snapshotConflict(target) || xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(target->editor))!=binding->version) return 0;
 } else if(v->snapshotMode) {
  const xvalue* group=path ? snapshotGroup(v,path) : NULL; const char* origin=string(get(group,"origin"));
  if(!group || !origin) return 0;
  if(!strcmp(origin,"disk") && !diskIdentity(path,group)) { invalidateSnapshot(app,"依赖文件已变化，旧诊断已撤销；请等待重新检查。"); return 0; }
  target=openPath(app,path); xide_model_error error={0};
  if(!target && !xideEditorOpen(app,path,&error)) { snprintf(v->message,sizeof(v->message),"无法打开诊断文件：%.620s",error.message); return 0; }
  if(!target) target=app->activeDocument;
  if(!target || !target->open || snapshotConflict(target) || !textMatches(group,target) || (!strcmp(origin,"disk") && (!matchesHash(target->baseline.raw,target->baseline.rawLength,get(get(group,"input"),"rawSha256")) || !diskIdentity(path,group)))) { invalidateSnapshot(app,"诊断文件与检查时的正文或身份不一致；未执行旧位置跳转。"); return 0; }
 }
 return selectRange(app,target,range);
}
int xideLanguageViewJump(xide_app* app,size_t index)
{
 int routed=automaticJump(app,index);if(routed>=0) return routed;
 xide_language_view* v=app->languageView; if(!v || !v->diagnostics || index>=xrtValueCount(v->diagnostics)) return 0;
 const xvalue* d=xrtValueArrayGet(v->diagnostics,index); return jumpRange(app,string(get(d,"file")),get(d,"range"));
}
size_t xideLanguageViewDiagnosticCount(xide_app* app) { return app && app->languageView ? app->languageView->autoEnabled ? automaticCount(app->languageView) : app->languageView->diagnostics ? xrtValueCount(app->languageView->diagnostics) : 0 : 0; }
const char* xideLanguageViewMessage(xide_app* app) { return app && app->languageView ? app->languageView->message : ""; }

/* The symbol pane belongs to the central bottom Dock. It never changes the
   side panels or makes the help tab cease being the default. */
static void clearHover(xide_language_view* v)
{
 cancelQuery(v,v->hoverRequest);
 if(v->hoverDocument && v->hoverDocument->editor) xuiCodeEditCloseHint(v->hoverDocument->editor);
 xrtValueRelease(v->hoverResult); v->hoverResult=NULL; v->hoverRequest=0; v->hoverAt=0; v->hoverDocument=NULL; v->hoverPointer=0;
}
static void clearOutline(xide_language_view* v)
{
 cancelQuery(v,v->outlineRequest);
 if(v->outline) { xui_tree_view_node_t node={.iId=0,.iParent=-1,.sText="打开 xlang 文档查看大纲。",.bEnabled=0}; xuiTreeViewSetNodes(v->outline,&node,1); }
 for(size_t i=0;i<v->outlineCount;++i) free(v->outlineItems[i].title);
 free(v->outlineTitle); v->outlineTitle=NULL; free(v->outlineItems); v->outlineItems=NULL; v->outlineCount=0;
 xrtValueRelease(v->outlineResult); v->outlineResult=NULL; v->outlineDocument=NULL; v->outlineRequest=0;
}
static void clearSemantic(xide_app* app)
{ xide_language_view* v=app->languageView; if(v) { clearHover(v); clearOutline(v); clearHighlights(v); } }
static int readySymbols(xide_app* app)
{
 xide_language_view* v=app->languageView; xide_document* d=app->activeDocument; bool indexed=false;
 return v && d && d->open && d->path && !strcmp(d->language,"xlang") && !v->busy && !v->syncMode && v->snapshotMode && current(v) &&
  (v->onDemandMode ? (v->initialized && v->onDemandSupported && v->receiptReady) : (xrtValueGetBool(get(v->snapshotResult,"semanticDefinitions"),&indexed) && indexed)) && inputPath(v,d->path)!=NULL;
}
int xideLanguageViewCanHover(xide_app* app)
{
 if(app && app->activeDocument && !strcmp(app->activeDocument->language,"c")) return xideCLanguageViewCanQuery(app,2);
 xide_language_view* v=app ? app->languageView : NULL;
 return v && ((v->hoverSupported && readySymbols(app)) || (!v->busy && !v->automatic && (!v->initialized || v->onDemandSupported) && canSnapshot(app,app->activeDocument) && xideTaskViewCompiler(app)));
}
static int outlineEvent(xui_widget widget,const xui_event_t* event,void* user)
{
 xide_app* app=user; xide_language_view* v=app->languageView;
 if(event->iType==XUI_EVENT_POINTER_DOUBLE_CLICK || (event->iType==XUI_EVENT_KEY_DOWN && event->iKey==XUI_KEY_ENTER)) {
  int id=xuiTreeViewGetSelected(widget);
  if(id>0 && (size_t)id<=v->outlineCount && readySymbols(app) && v->outlineDocument==app->activeDocument && v->outlineCheck==v->lastSnapshotId && v->outlineId==app->activeDocument->id && v->outlineVersion==xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(app->activeDocument->editor)) && jumpRange(app,v->outlineDocument->path,get(v->outlineItems[id-1].symbol,"selectionRange"))) return XUI_EVENT_DISPATCH_STOP;
 }
 return XUI_OK;
}
static int createOutline(xide_app* app)
{
 xide_language_view* v=app->languageView; xui_tree_view_desc_t tree={0}; tree.iSize=sizeof(tree); tree.pFont=app->uiFont; tree.fItemHeight=26; tree.fPadding=6; tree.fIndent=12;
 int r=xuiTreeViewCreate(app->context,&v->outline,&tree); if(r!=XUI_OK) return r;
 app->panels[XIDE_OUTLINE]=v->outline; clearOutline(v);
 r=xuiWidgetSetEventHandler(v->outline,XUI_EVENT_POINTER_DOUBLE_CLICK,outlineEvent,app);
 if(r==XUI_OK) r=xuiWidgetSetEventHandler(v->outline,XUI_EVENT_KEY_DOWN,outlineEvent,app);
 return r;
}
static int showOutline(xide_app* app)
{
 if(!app || !app->languageView || !app->dock) return 0;
 if(canSnapshot(app,app->activeDocument)) prepareSymbols(app);
 xui_dock_window_info_t info={.iSize=sizeof(info)}; if(xuiDockPanelGetWindowInfo(app->dock,app->windows[XIDE_OUTLINE],&info)!=XUI_OK) return 0;
 if(info.iState==XUI_DOCK_PANEL_WINDOW_FLOATING) { xuiSetFocusWidget(app->context,app->languageView->outline); return 1; }
 int pane=info.iState==XUI_DOCK_PANEL_WINDOW_DOCKED ? info.iPane : -1;
 for(int i=XIDE_HELP;i<=XIDE_TASKS && pane<0;++i) { xui_dock_window_info_t b={.iSize=sizeof(b)}; if(xuiDockPanelGetWindowInfo(app->dock,app->windows[i],&b)==XUI_OK && b.iState==XUI_DOCK_PANEL_WINDOW_DOCKED) pane=b.iPane; }
 if(pane<0) { int editor=app->activeDocument ? app->activeDocument->window : app->windows[XIDE_XLANG_DEMO]; if(xuiDockPanelGetWindowInfo(app->dock,editor,&info)!=XUI_OK || info.iState!=XUI_DOCK_PANEL_WINDOW_DOCKED || xuiDockPanelDockWindowToPaneSide(app->dock,app->windows[XIDE_OUTLINE],info.iPane,XUI_DOCK_PANEL_SIDE_BOTTOM,0.28f,&pane)!=XUI_OK) return 0; }
 else if(xuiDockPanelDockWindowToPane(app->dock,app->windows[XIDE_OUTLINE],pane)!=XUI_OK) return 0;
 return xuiDockPanelSetPaneActiveWindow(app->dock,pane,app->windows[XIDE_OUTLINE])==XUI_OK;
}
static xvalue* queryParams(xide_language_view* v,xide_document* d,int offset)
{
 document_binding* b=inputPath(v,d->path); if(!b || b->document!=d || b->id!=d->id) return NULL;
 xvalue *p=xrtValueObject(),*item=xrtValueObject(),*position=NULL;
 int okay=p && item && put(item,"uri",xrtValueString(sv(b->uri)));
 if(okay) { okay=put(p,"textDocument",item); item=NULL; }
 if(okay && offset>=0) { int line=0,character=0; position=xrtValueObject(); okay=position && xideCodeDocumentOffsetToLsp(xuiCodeEditGetDocument(d->editor),offset,&line,&character) && put(position,"line",xrtValueInt(line)) && put(position,"character",xrtValueInt(character)); if(okay) { okay=put(p,"position",position); position=NULL; } }
 if(okay) okay=put(p,"version",xrtValueInt(b->wireVersion)) && (v->onDemandMode || put(p,"checkId",xrtValueInt(v->lastSnapshotId)));
 xrtValueRelease(item); xrtValueRelease(position); if(!okay) { xrtValueRelease(p); return NULL; } return p;
}
static int requestHover(xide_app* app)
{
 xide_language_view* v=app->languageView; if(!xideLanguageViewCanHover(app) || !readySymbols(app) || v->hoverDocument!=app->activeDocument) return 0;
 xvalue* params=queryParams(v,v->hoverDocument,v->hoverOffset); if(!params) return 0;
 if(v->nextId==INT_MAX) return 0;
 v->hoverRequest=++v->nextId; v->hoverCheck=v->lastSnapshotId; v->hoverId=v->hoverDocument->id; v->hoverVersion=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(v->hoverDocument->editor)); v->hoverAt=0;
 if(!sendMessage(v,"textDocument/hover",params,v->hoverRequest)) { v->hoverRequest=0; return 0; } return 1;
}
static int requestCaretHover(xide_app* app)
{
 if(!xideLanguageViewCanHover(app) || !prepareSymbols(app)) return 0;
 xide_language_view* v=app->languageView; xui_code_selection_t s={0}; if(xuiCodeSelectionGetState(xuiCodeEditGetSelection(app->activeDocument->editor),&s)!=XUI_OK) return 0;
 clearHover(v); v->hoverDocument=app->activeDocument; v->hoverOffset=s.iCaretOffset; v->hoverId=v->hoverDocument->id; v->hoverVersion=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(v->hoverDocument->editor));
 if(readySymbols(app)) return requestHover(app);
 v->hoverAt=GetTickCount64(); if(!v->hoverAt) v->hoverAt=1; xgeRenderRequest(); return 1;
}
static const int semanticEvents[]={XUI_EVENT_POINTER_MOVE,XUI_EVENT_POINTER_LEAVE,XUI_EVENT_POINTER_DOWN,XUI_EVENT_POINTER_WHEEL,XUI_EVENT_BLUR,XUI_EVENT_KEY_DOWN,XUI_EVENT_TEXT,XUI_EVENT_IME_COMPOSITION};
static int hoverEvent(xui_widget widget,const xui_event_t* event,void* user)
{
 semantic_editor_binding* b=user; xide_language_view* v=b->app->languageView; int index=-1;
 for(int i=0;i<8;++i) if(semanticEvents[i]==event->iType) index=i;
 if(event->iType==XUI_EVENT_IME_COMPOSITION) b->composing=event->bCompositionActive;
 if(event->iType==XUI_EVENT_BLUR) b->composing=0;
 if(event->iType==XUI_EVENT_KEY_DOWN && event->iKey==XUI_KEY_ESCAPE) suppressHighlights(v);
 else if(event->iType==XUI_EVENT_KEY_DOWN || event->iType==XUI_EVENT_TEXT || event->iType==XUI_EVENT_POINTER_DOWN || event->iType==XUI_EVENT_BLUR || event->iType==XUI_EVENT_IME_COMPOSITION) clearHighlights(v);
 if(event->iType!=XUI_EVENT_POINTER_MOVE) clearHover(v);
 int result=index>=0 && b->handlers[index] ? b->handlers[index](widget,event,b->users[index]) : XUI_OK;
 if(event->iType!=XUI_EVENT_POINTER_MOVE) return result;
 if(event->iButtons || !xideLanguageViewCanHover(b->app) || !b->app->activeDocument || b->app->activeDocument->editor!=widget) { clearHover(v); return result; }
 xui_code_edit_text_hit_t hit={.iSize=sizeof(hit)};
 if(xuiCodeEditHitTestText(widget,(float)event->fX,(float)event->fY,&hit)!=XUI_OK || !hit.bInsideText || hit.iClusterStart==hit.iClusterEnd) { clearHover(v); return result; }
 if(!prepareSymbols(b->app)) { clearHover(v); return result; }
 int offset=hit.iClusterStart;
 if(v->hoverDocument!=b->app->activeDocument || !v->hoverPointer || offset!=v->hoverOffset) { clearHover(v); v->hoverDocument=b->app->activeDocument; v->hoverId=v->hoverDocument->id; v->hoverVersion=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(v->hoverDocument->editor)); v->hoverOffset=offset; v->hoverPointer=1; v->hoverAt=GetTickCount64()+350; xgeRenderRequest(); }
 v->hoverX=event->fX; v->hoverY=event->fY; return result;
}
int xideLanguageViewBindEditor(xide_app* app,xui_widget editor)
{
 xide_language_view* v=app ? app->languageView : NULL; if(!v || !editor) return XUI_ERROR_INVALID_ARGUMENT;
 for(semantic_editor_binding* b=v->bindings;b;b=b->next) if(b->editor==editor) return XUI_OK;
 semantic_editor_binding* b=calloc(1,sizeof(*b)); if(!b) return XUI_ERROR_OUT_OF_MEMORY; b->app=app; b->editor=editor;
 int r=XUI_OK,installed=0;
 for(int i=0;i<8 && r==XUI_OK;++i) r=xuiWidgetGetEventHandler(editor,semanticEvents[i],&b->handlers[i],&b->users[i]);
 for(int i=0;i<8 && r==XUI_OK;++i) { r=xuiWidgetSetEventHandler(editor,semanticEvents[i],hoverEvent,b); if(r==XUI_OK) ++installed; }
 if(r!=XUI_OK) { for(int i=0;i<installed;++i) xuiWidgetSetEventHandler(editor,semanticEvents[i],b->handlers[i],b->users[i]); free(b); return r; }
 b->next=v->bindings; v->bindings=b; return XUI_OK;
}
static int readPosition(const xvalue* value,xide_lsp_position* p)
{ return number(get(value,"line"),&p->line) && number(get(value,"character"),&p->character); }
static int flattenSymbols(const xvalue* array,outline_item* items,xide_lsp_position* positions,size_t* count,int parent,int depth)
{
 if(!array || xrtValueType(array)!=XVALUE_ARRAY || depth>64 || xrtValueCount(array)>4096-*count) return 0;
 for(size_t i=0;i<xrtValueCount(array);++i) {
  if(*count>=4096) return 0;
  size_t at=(*count)++; const xvalue *v=xrtValueArrayGet(array,i),*range=get(v,"range"),*selection=get(v,"selectionRange"); const char *name=string(get(v,"name")),*detail=string(get(v,"detail")); int kind=0;
  if(!name || !*name || strlen(name)>512 || (detail && strlen(detail)>2048) || !number(get(v,"kind"),&kind) || kind<1 || kind>26 || !readPosition(get(range,"start"),positions+at*4) || !readPosition(get(range,"end"),positions+at*4+1) || !readPosition(get(selection,"start"),positions+at*4+2) || !readPosition(get(selection,"end"),positions+at*4+3)) return 0;
  items[at]=(outline_item){v,parent,copy(detail && *detail ? detail : name)}; if(!items[at].title || !flattenSymbols(get(v,"children"),items,positions,count,(int)at,depth+1)) return 0;
 }
 return 1;
}
static int symbolReplyCurrent(xide_language_view* v,xide_document* d,uint64_t id,uint32_t version,int check)
{ return readySymbols(v->app) && d==v->app->activeDocument && d->open && d->id==id && version==xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor)) && check==v->lastSnapshotId; }
/* Scratch positions of the validated, LF-normalized current editor document.
   Preserve the original endpoint bound and reject every invalid endpoint
   before applying any outline/hint UI state. No full-document text scan. */
static int mapEditorPositions(xide_document* d,xide_lsp_position* positions,size_t count)
{
 if(count>8192) return 0;
 for(size_t i=0;i<count;++i)
  if(!xideCodeDocumentLspToOffset(xuiCodeEditGetDocument(d->editor),positions[i].line,positions[i].character,&positions[i].offset)) return 0;
 return 1;
}
static void applyOutline(xide_app* app,const xvalue* result,const xvalue* error)
{
 xide_language_view* v=app->languageView; v->outlineRequest=0;
 if(!symbolReplyCurrent(v,v->outlineDocument,v->outlineId,v->outlineVersion,v->outlineCheck)) { clearOutline(v); return; }
 if(error) return;
 char* title=copy(v->outlineDocument->path); xvalue* owned=xrtValueDeepClone(result); outline_item* items=calloc(4096,sizeof(*items)); xide_lsp_position* positions=calloc(4096*4,sizeof(*positions)); xui_tree_view_node_t* nodes=calloc(4097,sizeof(*nodes)); size_t count=0;
 int okay=title && owned && items && positions && nodes && flattenSymbols(owned,items,positions,&count,-1,0) && mapEditorPositions(v->outlineDocument,positions,count*4);
 if(nodes) nodes[0]=(xui_tree_view_node_t){.iId=0,.iParent=-1,.sText=title,.bExpanded=1,.bEnabled=0};
 for(size_t i=0;okay && i<count;++i) {
  xide_lsp_position* p=positions+i*4; int parent=items[i].parent; okay=p[0].offset<=p[2].offset && p[2].offset<p[3].offset && p[3].offset<=p[1].offset;
  if(okay && parent>=0) okay=p[0].offset>=positions[(size_t)parent*4].offset && p[1].offset<=positions[(size_t)parent*4+1].offset;
  nodes[i+1]=(xui_tree_view_node_t){.iId=(int)i+1,.iParent=parent<0 ? 0 : parent+1,.sText=items[i].title,.bExpanded=1,.bEnabled=1};
 }
 if(okay) okay=xuiTreeViewSetNodes(v->outline,nodes,(int)count+1)==XUI_OK;
 if(okay) { v->outlineTitle=title; title=NULL; v->outlineResult=owned; owned=NULL; v->outlineItems=items; items=NULL; v->outlineCount=count; if(v->onDemandMode) snprintf(v->message,sizeof(v->message),"xlang 文档大纲：%zu 项。",count); }
 else { clearOutline(v); snprintf(v->message,sizeof(v->message),"大纲范围无法验证，已撤销本次大纲。"); }
 if(items) for(size_t i=0;i<count;++i) free(items[i].title);
 free(title); free(items); free(positions); free(nodes); xrtValueRelease(owned); xgeRenderRequest();
}
static void applyHover(xide_app* app,const xvalue* result,const xvalue* error)
{
 xide_language_view* v=app->languageView; v->hoverRequest=0;
 if(!symbolReplyCurrent(v,v->hoverDocument,v->hoverId,v->hoverVersion,v->hoverCheck) || error || !result || xrtValueType(result)==XVALUE_NULL) { clearHover(v); return; }
 if(!v->hoverPointer) { xui_code_selection_t selection={0}; if(xuiCodeSelectionGetState(xuiCodeEditGetSelection(v->hoverDocument->editor),&selection)!=XUI_OK || selection.iCaretOffset!=v->hoverOffset) { clearHover(v); return; } }
 xvalue* owned=xrtValueDeepClone(result); const xvalue *contents=get(owned,"contents"),*range=get(owned,"range"); const char *kind=string(get(contents,"kind")),*text=string(get(contents,"value")); xide_lsp_position p[2]={0};
 int okay=owned && kind && !strcmp(kind,"plaintext") && text && *text && strlen(text)<=12499 && readPosition(get(range,"start"),p) && readPosition(get(range,"end"),p+1) && mapEditorPositions(v->hoverDocument,p,2) && p[0].offset<=v->hoverOffset && v->hoverOffset<p[1].offset;
 if(okay) { xui_code_hover_t hint={.iSize=sizeof(hint),.tRange={p[0].offset,p[1].offset},.sText=text,.sContentType="plaintext"}; okay=xuiCodeEditShowHint(v->hoverDocument->editor,&hint)==XUI_OK; }
 if(okay) { xrtValueRelease(v->hoverResult); v->hoverResult=owned; owned=NULL; }
 else clearHover(v);
 xrtValueRelease(owned); xgeRenderRequest();
}
static void pollSemantic(xide_app* app)
{
 xide_language_view* v=app->languageView; int ready=readySymbols(app);
 if(v->outlineDocument && (!ready || v->outlineDocument!=app->activeDocument || v->outlineId!=app->activeDocument->id || v->outlineVersion!=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(app->activeDocument->editor)) || v->outlineCheck!=v->lastSnapshotId)) clearOutline(v);
 if(ready && v->outlineSupported && !v->outlineDocument && v->nextId<INT_MAX) {
  xvalue* params=queryParams(v,app->activeDocument,-1);
  if(params) { v->outlineDocument=app->activeDocument; v->outlineId=app->activeDocument->id; v->outlineVersion=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(app->activeDocument->editor)); v->outlineCheck=v->lastSnapshotId; v->outlineRequest=++v->nextId; if(!sendMessage(v,"textDocument/documentSymbol",params,v->outlineRequest)) clearOutline(v); }
 }
 if(v->hoverDocument) {
  int pending=v->onDemandMode && current(v) && (v->waitingInitialize || v->receiptRequest || v->syncMode);
  if((!ready && !pending) || app->activeDocument!=v->hoverDocument || !v->hoverDocument->open || v->hoverId!=v->hoverDocument->id || v->hoverVersion!=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(v->hoverDocument->editor))) { clearHover(v); return; }
  if(v->hoverPointer) { xui_code_edit_text_hit_t hit={.iSize=sizeof(hit)}; if(xuiGetHoverWidget(app->context)!=v->hoverDocument->editor || xuiCodeEditHitTestText(v->hoverDocument->editor,(float)v->hoverX,(float)v->hoverY,&hit)!=XUI_OK || !hit.bInsideText || hit.iClusterStart!=v->hoverOffset) { clearHover(v); return; } }
  else { xui_code_selection_t selection={0}; if(xuiCodeSelectionGetState(xuiCodeEditGetSelection(v->hoverDocument->editor),&selection)!=XUI_OK || selection.iCaretOffset!=v->hoverOffset) { clearHover(v); return; } }
  if(ready && v->hoverAt && GetTickCount64()>=v->hoverAt) requestHover(app);
 }
 if(v->hoverAt || v->hoverRequest || v->outlineRequest) xgeRenderRequest();
}

/* Standard document highlights use a distinct owner ID. Native BACKGROUND
 * drawing is indexed by visible bytes; no UI hit tests/full-text conversions
 * are needed when scrolling, wrapping or folding installed ranges. */
#define HIGHLIGHT_INDICATOR 1005
static int highlightCaret(xide_language_view* v,highlight_intent* intent)
{
 xide_document* d=v->app->activeDocument; xui_code_selection_t selection={0};
 if(!d || !d->editor || !canSnapshot(v->app,d) || v->busy || xideActionsHasModal(v->app) ||
    strcmp(d->language,"xlang") || strcmp(xuiCodeEditGetLanguage(d->editor),"xlang") ||
    xuiGetFocusWidget(v->app->context)!=d->editor || !xideTaskViewCompiler(v->app) ||
    xuiCodeSelectionGetCount(xuiCodeEditGetSelection(d->editor))!=1 ||
    xuiCodeSelectionGetState(xuiCodeEditGetSelection(d->editor),&selection)!=XUI_OK ||
    selection.iCaretOffset!=selection.iAnchorOffset) return 0;
 for(semantic_editor_binding* b=v->bindings;b;b=b->next) if(b->editor==d->editor && b->composing) return 0;
 *intent=(highlight_intent){.document=d,.id=d->id,.stamp=editStamp(v->app),
  .version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor)),.offset=selection.iCaretOffset};
 return 1;
}
static int sameHighlight(const highlight_intent* a,const highlight_intent* b)
{ return a->document==b->document && a->id==b->id && a->stamp==b->stamp && a->version==b->version && a->offset==b->offset; }
static void clearHighlights(xide_language_view* v)
{
 if(!v || (!v->highlight.document && !v->highlightRequest && !v->highlightDecoding && !v->highlightResult)) return;
 cancelQuery(v,v->highlightRequest); xideHighlightDecoderCancel(v->highlightDecoder);
 if(v->highlight.document && v->highlight.document->editor) {
  xui_widget editor=v->highlight.document->editor;
  xui_code_annotation_store annotations=xuiCodeEditGetAnnotations(editor);
  if(annotations && xuiCodeAnnotationClearIndicators(annotations,HIGHLIGHT_INDICATOR)==XUI_OK)
   xuiWidgetInvalidate(editor,XUI_WIDGET_DIRTY_CACHE|XUI_WIDGET_DIRTY_RENDER);
 }
 xideHighlightResultRelease(v->highlightResult); v->highlightResult=NULL;
 memset(&v->highlight,0,sizeof(v->highlight)); v->highlightAt=v->highlightToken=v->highlightReplyUntil=0;
 v->highlightRequest=v->highlightDecoding=v->highlightPreparing=0; v->highlightApplied=0; v->highlightReceipt[0]=0;
}
static void suppressHighlights(xide_language_view* v)
{
 xide_document* d=v->app->activeDocument; xui_code_selection_t selection={0};
 if(d && d->open && d->editor && namedXlang(d) &&
    xuiCodeSelectionGetState(xuiCodeEditGetSelection(d->editor),&selection)==XUI_OK)
  v->highlightSuppressed=(highlight_intent){.document=d,.id=d->id,.stamp=editStamp(v->app),
   .version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor)),.offset=selection.iCaretOffset};
 clearHighlights(v);
}
static int highlightCurrent(xide_language_view* v)
{
 highlight_intent intent={0};
 if(!highlightCaret(v,&intent) || !sameHighlight(&intent,&v->highlight) || !readySymbols(v->app) ||
    !v->highlightSupported || v->highlight.check!=v->lastSnapshotId ||
    (v->onDemandMode && strcmp(v->highlightReceipt,v->receiptDigest))) return 0;
 document_binding* input=inputPath(v,intent.document->path);
 return input && input->document==intent.document && input->id==intent.id && input->version==intent.version &&
  input->wireVersion==v->highlight.wire && xlsSyncVersion(v->sync,input->uri)==v->highlight.wire;
}
/* >0 transfers the entire response; <0 retains it in the single retry slot. */
static int takeHighlights(xide_language_view* v,xvalue* message)
{
 if(!highlightCurrent(v)) { clearHighlights(v); return 0; }
 document_binding* input=inputPath(v,v->highlight.document->path);
 xls_text* text=xlsSyncRetainText(v->sync,input->uri,v->highlight.wire);
 if(!text) { v->highlightRequest=0; return 0; }
 if(xideHighlightDecoderSubmit(v->highlightDecoder,v->highlightToken,message,text)) {
  v->highlightRequest=0; v->highlightDecoding=1; v->highlightReplyUntil=0; return 1;
 }
 xlsTextRelease(text);
 uint64_t now=GetTickCount64(); if(!v->highlightReplyUntil) v->highlightReplyUntil=now+1000;
 if(now<v->highlightReplyUntil) { xgeRenderRequestAfter(0.02f); return -1; }
 v->highlightRequest=0; v->highlightReplyUntil=0; return 0;
}
static void installHighlights(xide_language_view* v)
{
 if(!v->highlightResult) return;
 if(!highlightCurrent(v)) { clearHighlights(v); return; }
 xui_widget editor=v->highlight.document->editor; xui_code_annotation_store annotations=xuiCodeEditGetAnnotations(editor);
 LARGE_INTEGER began,now; QueryPerformanceCounter(&began); size_t before=v->highlightApplied;
 while(v->highlightApplied<v->highlightResult->count && v->highlightApplied-before<8192) {
  const xide_highlight_range* range=v->highlightResult->ranges+v->highlightApplied;
  if(!annotations || xuiCodeAnnotationSetIndicator(annotations,HIGHLIGHT_INDICATOR,XUI_CODE_INDICATOR_BACKGROUND,
      range->start,range->end,0,0)!=XUI_OK) { suppressHighlights(v); return; }
  ++v->highlightApplied;
  if((v->highlightApplied-before)%16==0) { QueryPerformanceCounter(&now);
   if((now.QuadPart-began.QuadPart)*2000>=v->highlightClock.QuadPart) break;
  }
 }
 if(v->highlightApplied!=before) { xuiWidgetInvalidate(editor,XUI_WIDGET_DIRTY_CACHE|XUI_WIDGET_DIRTY_RENDER); xgeRenderRequest(); }
 if(v->highlightApplied==v->highlightResult->count) { xideHighlightResultRelease(v->highlightResult); v->highlightResult=NULL; }
 else xgeRenderRequest();
}
static void pollHighlights(xide_app* app)
{
 xide_language_view* v=app->languageView; highlight_intent intent={0};
 if(!highlightCaret(v,&intent) || (v->highlightSuppressed.document && sameHighlight(&intent,&v->highlightSuppressed))) { clearHighlights(v); return; }
 if(v->highlightSuppressed.document) memset(&v->highlightSuppressed,0,sizeof(v->highlightSuppressed));
 if(v->highlight.document && !sameHighlight(&intent,&v->highlight)) clearHighlights(v);
 if(!v->highlight.document) { v->highlight=intent; v->highlightAt=GetTickCount64()+120; }
 xide_highlight_result* result=xideHighlightDecoderTake(v->highlightDecoder);
 if(result) {
  if(v->highlightDecoding && result->token==v->highlightToken) {
   if(!highlightCurrent(v)) clearHighlights(v);
   else {
    v->highlightDecoding=0;
    if(result->valid) { v->highlightResult=result; result=NULL; }
   }
  }
  xideHighlightResultRelease(result);
 }
 installHighlights(v);
 uint64_t now=GetTickCount64();
 if(v->highlightAt && now>=v->highlightAt) {
  if(v->initialized && !v->highlightSupported) v->highlightAt=0;
  else {
   if(!readySymbols(app) && !v->waitingInitialize && !v->receiptRequest && !v->syncMode) {
    int prepared=prepareSymbols(app); v->highlight=intent; v->highlightAt=prepared ? now : 0;
    v->highlightPreparing=prepared;
   }
   if(v->highlightAt && readySymbols(app)) {
    v->highlightAt=0; v->highlightPreparing=0;
    document_binding* input=inputPath(v,intent.document->path);
    xvalue* params=v->highlightSupported && input && v->nextId<INT_MAX && v->highlightSerial<UINT64_MAX ? queryParams(v,intent.document,intent.offset) : NULL;
    if(params) {
     v->highlight.check=v->lastSnapshotId; v->highlight.wire=input->wireVersion;
     memcpy(v->highlightReceipt,v->receiptDigest,sizeof(v->highlightReceipt));
     v->highlightToken=++v->highlightSerial; v->highlightRequest=++v->nextId;
     if(!sendMessage(v,"textDocument/documentHighlight",params,v->highlightRequest)) v->highlightRequest=0;
    }
   }
  }
 }
 if(v->highlightAt || v->deferredMessage) xgeRenderRequestAfter(0.02f);
}

#include "automatic_diagnostics.inc"
