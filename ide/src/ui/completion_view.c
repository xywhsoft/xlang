#include "ui/completion_view.h"
#include "ui/task_view.h"
#include "services/language_client.h"
#include "services/language_check.h"
#include "services/sync_plan.h"
#include "services/completion_result.h"
#include "editor/lsp_coordinates.h"
#include "platform/file_watch.h"
#include "platform/path.h"
#include "platform/startup_alias.h"
#include "protocol/document_sync.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <math.h>
#define COMPLETION_DOCS 64u
#define COMPLETION_ITEMS 4096u
typedef struct completion_input { xide_document* document; uint64_t id; uint32_t version; int wireVersion,externalState; char *path,*uri,*language; const char* text; xvalue* textValue; size_t length; } completion_input;
typedef struct completion_binding { struct completion_binding* next; xide_app* app; xui_widget editor; xui_widget_event_proc handlers[5]; void* users[5]; } completion_binding;
struct xide_completion_view {
 xide_app* app; xide_language_client* client; char *compiler,*directory,*package;
 completion_input inputs[COMPLETION_DOCS]; size_t inputCount; int nextId,initialized,supported,syncKind,nativeBackend; xls_sync* sync;
 xide_document* document; uint64_t id,due; uint32_t version; int offset,wanted,pending,request,ready,sawOpen,refreshing;
 const xvalue* result; xui_code_completion_item_t* items; const char* const* nameIndex; int count,start,end,collectEligible;
 xui_widget repeatEditor; uint64_t repeatId; int repeatOffset;
 xui_code_text_edit_t edit; char editText[513]; completion_binding* bindings;
 xide_sync_planner* syncPlanner; xide_sync_result* syncReady;
 uint64_t syncToken,syncSerial,syncSession,session,syncDeadline; int syncSubmitted;
 xide_completion_decoder* decoder;xide_completion_result* decoded;
 uint64_t decodeToken,decodeSerial,decodeSession,decodeDeadline;
 int decoding; xvalue* deferredMessage;int deferredDiscard;uint64_t deferredSession;
};
static xstrview cvsv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* cvget(const xvalue* o,const char* key) { return o && xrtValueType(o)==XVALUE_OBJECT ? xrtValueObjectGet(o,cvsv(key)) : NULL; }
static int cvput(xvalue* o,const char* key,xvalue* v) { return v && xrtValueObjectSetNew(o,cvsv(key),v); }
static char* cvcopy(const char* s) { size_t n=strlen(s)+1; char* r=malloc(n); if(r) memcpy(r,s,n); return r; }
static const char* cvstring(const xvalue* v) { xstrview s={0}; return v && xrtValueGetString(v,&s) && s.Size<=INT_MAX && !memchr(s.Data,0,s.Size) && (!s.Size || MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.Data,(int)s.Size,NULL,0)) ? s.Data : NULL; }
static int cvnumber(const xvalue* v,int* n) { int64 value=0; if(!v || !xrtValueGetInt(v,&value) || value<0 || value>INT_MAX) return 0; *n=(int)value; return 1; }
static int cvbool(const xvalue* v,int expected) { bool value=false; return v && xrtValueGetBool(v,&value) && value==(bool)expected; }
static int cvnamed(xide_document* d) { return d && d->open && d->path && xidePathHasExtension(d->path,".xl"); }
static int cveligible(xide_app* app,xide_document* d)
{
 if(!cvnamed(d) || xideActionsHasModal(app) || strcmp(d->language,"xlang") || strcmp(xuiCodeEditGetLanguage(d->editor),"xlang") || xuiCodeSelectionGetCount(xuiCodeEditGetSelection(d->editor))!=1 || xuiCodeEditIsReadonly(d->editor) || (d->externalState && d->externalState!=XIDE_WATCH_MISSING) || !xideTaskViewCompiler(app)) return 0;
 size_t count=0,bytes=0;
 for(size_t i=0;i<app->documents.count;++i) { xide_document* item=app->documents.items[i]; if(!cvnamed(item)) continue;
  int n=xuiCodeDocumentGetLength(xuiCodeEditGetDocument(item->editor));
  if((item->externalState && item->externalState!=XIDE_WATCH_MISSING) || ++count>COMPLETION_DOCS || n<0 || (size_t)n>8u*1024u*1024u || (size_t)n>32u*1024u*1024u-bytes) return 0;
  bytes+=(size_t)n;
 } return count>0;
}
static int cvcontext(xide_completion_view* v,int accepting)
{
 xide_document* d=v->document; xui_code_selection_t s={0};
 if(!v->wanted || !d || v->app->activeDocument!=d || !cveligible(v->app,d) || d->id!=v->id || v->version!=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor)) || xuiGetFocusWidget(v->app->context)!=d->editor || xuiCodeSelectionGetState(xuiCodeEditGetSelection(d->editor),&s)!=XUI_OK || s.iCaretOffset!=v->offset) return 0;
 return s.iAnchorOffset==s.iCaretOffset || (accepting && s.iAnchorOffset==v->start);
}
static int cvinputsCurrent(xide_completion_view* v)
{
 size_t count=0; for(size_t i=0;i<v->app->documents.count;++i) if(cvnamed(v->app->documents.items[i])) ++count;
 if(count!=v->inputCount || !count) return 0;
 for(size_t i=0;i<v->inputCount;++i) { completion_input* b=&v->inputs[i]; xide_document* d=b->document;
  if(!cvnamed(d) || d->id!=b->id || d->externalState!=b->externalState || strcmp(d->path,b->path) || strcmp(d->language,b->language) || b->version!=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor))) return 0;
 } return 1;
}
static int cvsend(xide_completion_view* v,const char* method,xvalue* params,int id)
{
 xvalue* m=xrtValueObject(); int ok=m && cvput(m,"jsonrpc",xrtValueString(cvsv("2.0"))) && cvput(m,"method",xrtValueString(cvsv(method)));
 if(ok && params) { ok=cvput(m,"params",params); params=NULL; } if(ok && id) ok=cvput(m,"id",xrtValueInt(id));
 if(ok) ok=xideLanguageClientSend(v->client,m,NULL);
 xrtValueRelease(m); xrtValueRelease(params); return ok;
}
static void cvcancelSync(xide_completion_view* v)
{ xideSyncPlannerCancel(v->syncPlanner);v->syncToken=v->syncDeadline=0;v->syncSubmitted=0; }
static int cvretireSync(xide_completion_view* v)
{
 if(v->syncReady && (!v->syncToken || v->syncReady->token!=v->syncToken)) {
  if(!xideSyncPlannerRetire(v->syncPlanner,v->syncReady)) {xgeRenderRequestAfter(.02f);return 0;}
  v->syncReady=NULL;
 }
 return 1;
}
static int cvflushResult(xide_completion_view* v)
{
 if(v->decoded && !v->result) {
  if(!xideCompletionDecoderRetire(v->decoder,v->decoded)) {xgeRenderRequestAfter(.02f);return 0;}
  v->decoded=NULL;
 }
 return 1;
}
static void cvclearResult(xide_completion_view* v)
{
 if(v->document && v->document->editor) xuiCodeEditCancelCompletion(v->document->editor);
 /* Strings and name index belong to the worker-owned immutable result. Stop
    native use first; only the small UI adapter array is freed on this thread. */
 free(v->items);v->items=NULL;v->nameIndex=NULL;v->result=NULL;v->count=v->collectEligible=v->sawOpen=0;cvflushResult(v);
}
static void cvdismiss(xide_completion_view* v)
{
 cvcancelSync(v);xideCompletionDecoderCancel(v->decoder);v->decodeToken=v->decodeDeadline=0;v->decoding=0;
 if(v->request && v->client && v->initialized) { xvalue* p=xrtValueObject(); cvput(p,"id",xrtValueInt(v->request)); cvsend(v,"$/cancelRequest",p,0); }
 cvclearResult(v); v->wanted=v->pending=v->ready=v->request=0; v->repeatEditor=NULL;
}
static void cvqueueInput(xide_completion_view* v,xide_document* d,int offset)
{
 cvdismiss(v);v->document=d;v->id=d->id;v->repeatEditor=d->editor;v->repeatId=d->id;v->repeatOffset=offset;xgeRenderRequest();
}
static void cvfreeInput(completion_input* b)
{ free(b->path); free(b->uri); xrtValueRelease(b->textValue); free(b->language); memset(b,0,sizeof(*b)); }
static void cvfreeInputs(xide_completion_view* v)
{ for(size_t i=0;i<v->inputCount;++i) cvfreeInput(&v->inputs[i]); v->inputCount=0; }
static void cvstopClient(xide_completion_view* v)
{
 ++v->session;
 cvdismiss(v); xideLanguageClientStop(v->client); v->client=NULL; cvfreeInputs(v); xlsSyncDestroy(v->sync); v->sync=NULL; free(v->compiler); free(v->directory); free(v->package); v->compiler=v->directory=v->package=NULL; v->initialized=v->supported=v->syncKind=v->nativeBackend=0;
}
static void cvwake(void* user) { (void)user; xgeRenderRequest(); }
static char* cvserverPath(void)
{
 wchar_t module[32768]; char path[131072]; DWORD n=GetModuleFileNameW(NULL,module,32768);
 if(!n || n>=32768 || !WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,xideStartupResourceModule(module),-1,path,sizeof(path),NULL,NULL)) return NULL;
 char* directory=xidePathDirectory(path); char* result=directory ? xidePathResolveLexical(directory,"xlang-ls.exe") : NULL; free(directory); return result;
}
static int cvconnectionCurrent(xide_completion_view* v)
{
 const char* compiler=xideTaskViewCompiler(v->app); char* directory=v->document && v->document->path ? xidePathDirectory(v->document->path) : NULL; char* package=NULL;
 int ok=compiler && directory && v->compiler && v->directory && xidePathEqual(compiler,v->compiler) && xidePathEqual(directory,v->directory) && xideLanguageCheckPackagePath(&package,NULL) && (package && v->package ? !strcmp(package,v->package) : package==v->package);
 free(directory); free(package); return ok;
}
static int cvstartClient(xide_completion_view* v)
{
 if(v->client) return 1;
 cvcancelSync(v);++v->session;
 xlsSyncDestroy(v->sync); v->sync=NULL; free(v->compiler); free(v->directory); free(v->package);
 v->compiler=v->directory=v->package=NULL; v->initialized=v->supported=v->syncKind=v->nativeBackend=0;
 const char* compiler=xideTaskViewCompiler(v->app); char* program=cvserverPath();
 v->compiler=compiler ? cvcopy(compiler) : NULL; v->directory=xidePathDirectory(v->document->path);
 int ok=program && v->compiler && v->directory && xideLanguageCheckPackagePath(&v->package,NULL);
 if(ok) v->client=xideLanguageClientStart(program,v->compiler,v->directory,NULL);
 free(program); if(!v->client) { cvstopClient(v); return 0; }
 xideLanguageClientSetWake(v->client,cvwake,v->app); v->nextId=1;
 xvalue *p=xrtValueObject(),*options=xrtValueObject(); cvput(p,"capabilities",xrtValueObject()); cvput(options,"checkOnSave",xrtValueBool(false)); cvput(options,"automaticDiagnostics",xrtValueBool(false)); cvput(p,"initializationOptions",options);
 if(!cvsend(v,"initialize",p,1)) { cvstopClient(v); return 0; } return 1;
}
static int cvcapture(xide_completion_view* v)
{
 cvcancelSync(v);
 completion_input previous[COMPLETION_DOCS]={0}; size_t count=v->inputCount;
 memcpy(previous,v->inputs,count*sizeof(*previous)); memset(v->inputs,0,sizeof(v->inputs)); v->inputCount=0;
 int okay=1;
 for(size_t i=0;i<v->app->documents.count && okay;++i) {
  xide_document* d=v->app->documents.items[i]; if(!cvnamed(d)) continue;
  if(v->inputCount>=COMPLETION_DOCS) { okay=0; break; }
  completion_input* b=&v->inputs[v->inputCount++]; uint32_t version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor));
  for(size_t j=0;j<count;++j) if(previous[j].document==d && previous[j].id==d->id && previous[j].version==version &&
      previous[j].externalState==d->externalState && !strcmp(previous[j].path,d->path) && !strcmp(previous[j].language,d->language)) {
   *b=previous[j]; memset(&previous[j],0,sizeof(previous[j])); break;
  }
  if(b->document) continue;
  b->document=d; b->id=d->id; b->version=version; b->externalState=d->externalState;
  b->path=cvcopy(d->path); b->uri=xideLspPathToUri(d->path,NULL); b->language=cvcopy(d->language);
  const char* text=xuiCodeEditGetText(d->editor);b->length=strlen(text);xstrview owned={0};
  okay=b->length==(size_t)xuiCodeDocumentGetLength(xuiCodeEditGetDocument(d->editor));
  if(okay) b->textValue=xrtValueString((xstrview){text,b->length});
  okay=okay && b->path && b->uri && b->language && b->textValue && xrtValueGetString(b->textValue,&owned);
  if(okay) b->text=owned.Data ? owned.Data : "";
 }
 for(size_t i=0;i<count;++i) cvfreeInput(&previous[i]);
 if(!okay) cvfreeInputs(v);
 return okay;
}
static int cvbegin(xide_completion_view* v)
{
 if(!cvcontext(v,0) || !v->supported || !v->syncPlanner || !v->sync || v->syncSerial==UINT64_MAX || !cvcapture(v) || !cvinputsCurrent(v)) return 0;
 v->syncToken=++v->syncSerial;v->syncSession=v->session;v->syncDeadline=GetTickCount64()+5000;return 1;
}
static int cvfinishSync(xide_completion_view* v)
{
 completion_input* target=NULL;for(size_t i=0;i<v->inputCount;++i) if(v->inputs[i].document==v->document) target=v->inputs+i;
 int line=0,character=0;
 if(!target || v->nextId==INT_MAX || !xideCodeDocumentOffsetToLsp(xuiCodeEditGetDocument(v->document->editor),v->offset,&line,&character)) return 0;
 xvalue *p=xrtValueObject(),*d=xrtValueObject(),*pos=xrtValueObject();
 int okay=p && d && pos && cvput(d,"uri",xrtValueString(cvsv(target->uri))) && cvput(pos,"line",xrtValueInt(line)) && cvput(pos,"character",xrtValueInt(character));
 if(okay) {okay=cvput(p,"textDocument",d);d=NULL;}if(okay) {okay=cvput(p,"position",pos);pos=NULL;}
 okay=okay && cvput(p,"version",xrtValueInt(target->wireVersion));xrtValueRelease(d);xrtValueRelease(pos);
 if(!okay) {xrtValueRelease(p);return 0;}
 v->request=++v->nextId; return cvsend(v,"textDocument/completion",p,v->request);
}
static void cvfailSync(xide_completion_view* v)
{ cvstopClient(v); }
static void cvpollSync(xide_completion_view* v)
{
 if(!cvretireSync(v) || !v->syncToken) return;
 if(v->syncSession!=v->session || !cvcontext(v,0) || !cvinputsCurrent(v) || !cvconnectionCurrent(v)) {cvdismiss(v);return;}
 if(!v->syncSubmitted) {
  xide_sync_input inputs[COMPLETION_DOCS];for(size_t i=0;i<v->inputCount;++i) inputs[i]=(xide_sync_input){v->inputs[i].uri,v->inputs[i].textValue,v->inputs[i].id,v->inputs[i].version};
  int accepted=xideSyncPlannerSubmit(v->syncPlanner,v->syncToken,v->sync,inputs,v->inputCount);
  if(accepted<0 || (!accepted && GetTickCount64()>=v->syncDeadline)) {cvfailSync(v);return;}
  if(!accepted) {xgeRenderRequestAfter(.02f);return;}v->syncSubmitted=1;v->syncDeadline=0;
 }
 if(!v->syncReady) v->syncReady=xideSyncPlannerTake(v->syncPlanner);
 xide_sync_result* result=v->syncReady;if(!result) return;
 if(result->token!=v->syncToken) {xgeRenderRequest();return;}
 if(!result->valid || result->count!=v->inputCount) {cvfailSync(v);return;}
 if(!xideLanguageBatchSend(v->client,result->batch)) {
  if(!v->syncDeadline) v->syncDeadline=GetTickCount64()+1000;
  if(xideLanguageClientState(v->client,NULL,0)>=XIDE_LS_STOPPED || GetTickCount64()>=v->syncDeadline) cvfailSync(v);else xgeRenderRequestAfter(.02f);
  return;
 }
 /* The complete notification batch precedes the completion request. Adopt
    its versions only after admission, then retire the old state off-thread. */
 result->batch=NULL;xls_sync* old=v->sync;v->sync=result->sync;result->sync=old;
 for(size_t i=0;i<v->inputCount;++i) v->inputs[i].wireVersion=result->versions[i];
 v->syncToken=v->syncDeadline=0;v->syncSubmitted=0;result->token=0;
 cvretireSync(v);if(!cvfinishSync(v)) cvfailSync(v);
}
static int cvprefixAllowed(xui_code_document document,int offset)
{
 if(offset<0 || offset>xuiCodeDocumentGetLength(document)) return 0;
 int base=offset>256 ? offset-256 : 0,size=offset-base,read=-1;char bytes[257];
 if(xuiCodeDocumentCopyRange(document,base,offset,bytes,sizeof(bytes),&read)!=XUI_OK || read!=size) return 0;
 int start=size;while(start>0 && ((unsigned char)bytes[start-1]>=128 || isalnum((unsigned char)bytes[start-1]) || bytes[start-1]=='_')) {
  --start;if(size-start>255) return 0;
 }
 return 1;
}
void xideCompletionViewRequest(xide_app* app,xui_widget editor,int offset,int force)
{
 xide_completion_view* v=app ? app->completionView : NULL; xide_document* d=app ? app->activeDocument : NULL;
 if(!v || !d || d->editor!=editor || !cveligible(app,d) || xuiGetFocusWidget(app->context)!=editor) return;
 xui_code_selection_t s={0}; if(xuiCodeSelectionGetState(xuiCodeEditGetSelection(editor),&s)!=XUI_OK || s.iCaretOffset!=offset || s.iCaretOffset!=s.iAnchorOffset) return;
 uint32_t version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(editor));
 /* Pending capture still describes the previous batch. A provider refresh
    must preserve the current intent/deadline; cvbegin captures all fresh inputs. */
 if(!force && v->wanted && v->document==d && v->id==d->id && v->version==version && v->offset==offset && (v->pending || !v->inputCount || cvinputsCurrent(v))) return;
 if(!cvprefixAllowed(xuiCodeEditGetDocument(editor),offset)) return;
 if(offset>0) { xui_code_token_t tokens[8]; int count=0;
  if(xuiCodeTokenBufferGetTokensInRange(xuiCodeEditGetTokenBuffer(editor),xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(editor)),offset-1,offset,tokens,8,&count)!=XUI_OK) return;
  for(int i=0;i<count && i<8;++i) if(tokens[i].iKind==XUI_CODE_TOKEN_COMMENT || tokens[i].iKind==XUI_CODE_TOKEN_STRING || tokens[i].iKind==XUI_CODE_TOKEN_CHAR || tokens[i].iKind==XUI_CODE_TOKEN_ERROR) return;
 }
 /* Input commits are coalesced by the owner frame, so asynchronous language
    queries need no second debounce after the editor's assist scheduling. */
 cvdismiss(v); v->document=d; v->id=d->id; v->version=version; v->offset=offset; v->wanted=v->pending=1; v->due=GetTickCount64(); xgeRenderRequest();
}
static int cvcompareNames(const void* a,const void* b) { return strcmp(*(const char* const*)a,*(const char* const*)b); }
/* Takes a fully validated result only after rechecking the current UI/model.
   All candidate strings/name lookups remain backed by its owned envelope. */
static int cvapply(xide_completion_view* v,xide_completion_result* result)
{
 if(!result || !result->valid || !result->available || result->count>COMPLETION_ITEMS || (result->count && (!result->items || !result->names)) || v->decoded || v->syncToken || !cvcontext(v,0) || !cvinputsCurrent(v) || !cvconnectionCurrent(v)) return 0;
 size_t count=result->count;xui_code_completion_item_t* items=count ? calloc(count,sizeof(*items)) : NULL;
 if(count && !items) return 0;
 for(size_t i=0;i<count;++i) {
  const xide_completion_item* source=result->items+i;
  items[i]=(xui_code_completion_item_t){.iSize=sizeof(*items),.sLabel=source->label,.sInsertText=source->label,.sFilterText=source->label,
   .sDetail=source->detail,.sSortText=source->sort,.iSortOrder=(int)i,.iUserData=(intptr_t)source->symbol};
 }
 v->decoded=result;v->result=cvget(result->message,"result");v->items=items;v->nameIndex=result->names;v->count=(int)count;v->start=result->start;v->end=result->end;return 1;
}
static void cvreveal(xide_completion_view* v,xui_widget widget)
{
 if(!widget) return;
 if(xuiWidgetGetType(widget)==xuiListViewGetType(v->app->context)) {
  float row=0,padding=0,border=0; xui_rect_t rect=xuiWidgetGetRect(widget);
  if(xuiListViewGetMetrics(widget,&row,&padding,&border)==XUI_OK && row>0 && row<=128 && border>=0 && border<=8 && rect.fH<(int)ceilf(row+2*border)) {
   rect.fH=(int)ceilf(row+2*border); xuiWidgetSetRect(widget,rect); xuiPopupSetContentSize(xuiCodeEditGetCompletionPopupWidget(v->document->editor),rect.fW,rect.fH);
  }
  xuiListViewSetScroll(widget,0); xuiListViewEnsureVisible(widget,xuiCodeEditGetCompletionSelected(v->document->editor)); return;
 }
 for(xui_widget child=xuiWidgetGetFirstChild(widget);child;child=xuiWidgetGetNextSibling(child)) cvreveal(v,child);
}
static int cvdecodeCurrent(xide_completion_view* v)
{
 if(v->decodeSession!=v->session || v->syncToken || !cvcontext(v,0) || !cvinputsCurrent(v) || !cvconnectionCurrent(v)) return 0;
 for(size_t i=0;i<v->inputCount;++i) if(v->inputs[i].document==v->document)
  return v->inputs[i].wireVersion>0 && v->inputs[i].wireVersion==xlsSyncVersion(v->sync,v->inputs[i].uri);
 return 0;
}
/* >0 transfers message ownership; <0 keeps one bounded deferred message. */
static int cvtakeResponse(xide_completion_view* v,xvalue* message)
{
 if(!v->decodeToken) {
  if(v->decodeSerial==UINT64_MAX) {v->ready=1;return 0;}
  v->decodeToken=++v->decodeSerial;v->decodeSession=v->session;
 }
 if(!cvdecodeCurrent(v)) {cvdismiss(v);return 0;}
 completion_input* target=NULL;for(size_t i=0;i<v->inputCount;++i) if(v->inputs[i].document==v->document) target=v->inputs+i;
 xls_text* text=target ? xlsSyncRetainText(v->sync,target->uri,target->wireVersion) : NULL;
 if(!text) {cvdismiss(v);return 0;}
 xide_completion_expected expected={v->request,target->wireVersion,v->offset};
 if(xideCompletionDecoderSubmit(v->decoder,v->decodeToken,message,text,expected)) {v->decoding=1;v->decodeDeadline=0;return 1;}
 xlsTextRelease(text);if(!v->decodeDeadline) v->decodeDeadline=GetTickCount64()+1000;
 if(GetTickCount64()<v->decodeDeadline) {xgeRenderRequestAfter(.02f);return -1;}
 xideCompletionDecoderCancel(v->decoder);v->decodeToken=v->decodeDeadline=0;v->ready=1;return 0;
}
/* Caller owns result and has no previous held/adopted result. */
static void cvacceptDecoded(xide_completion_view* v,xide_completion_result* result,int refresh)
{
 int current=v->decoding && result->token==v->decodeToken && cvdecodeCurrent(v);
 if(v->decoding && result->token==v->decodeToken && !current) cvdismiss(v);
 if(current) {
  v->ready=1;v->decoding=0;v->decodeToken=0;
  if(cvapply(v,result)) {
   result=NULL;
   if(refresh && v->count) {
    v->refreshing=1;xuiCodeEditShowCompletion(v->document->editor);v->refreshing=0;cvreveal(v,xuiCodeEditGetCompletionPopupWidget(v->document->editor));
    if(v->end>v->offset) xuiCodeEditClearInlineCompletion(v->document->editor);
    v->sawOpen=xuiCodeEditIsCompletionOpen(v->document->editor);xgeRenderRequest();
   }
  }
 }
 if(result) {v->decoded=result;v->result=NULL;cvflushResult(v);}
}
static void cvpollDecoded(xide_completion_view* v,int refresh)
{
 if(!cvflushResult(v) || v->decoded || v->deferredMessage || xideLanguageClientHasPending(v->client)) return;
 xide_completion_result* result=xideCompletionDecoderTake(v->decoder);if(result) cvacceptDecoded(v,result,refresh);
}
static const char* cvmethod(const xvalue* message)
{ const xvalue* value=cvget(message,"method");xstrview s={0};return value && xrtValueGetString(value,&s) && s.Size<=128 ? cvstring(value) : NULL; }
static void cvread(xide_completion_view* v,int refresh)
{
 (void)refresh;int count=0;
 while(count++<128) {
  if(v->deferredMessage && v->deferredSession!=v->session) v->deferredDiscard=1;
  if(v->deferredMessage && v->deferredDiscard) {
   if(!xideCompletionDecoderDiscard(v->decoder,v->deferredMessage)) {xgeRenderRequestAfter(.02f);break;}
   v->deferredMessage=NULL;
  }
  xvalue* m=v->deferredMessage;v->deferredMessage=NULL;
  if(!m && !(m=xideLanguageClientNext(v->client))) break;
  int id=0;const char* method=cvmethod(m);
  if(method && !strcmp(method,"xlang/completionInvalidated")) {int query=0;if(cvnumber(cvget(cvget(m,"params"),"queryId"),&query) && query==v->request) cvdismiss(v);}
  else if(cvnumber(cvget(m,"id"),&id) && id==1 && !v->initialized) {
   const xvalue* caps=cvget(cvget(m,"result"),"capabilities");v->initialized=1;v->supported=cvbool(cvget(cvget(caps,"experimental"),"cursorCompletion"),1) && xrtValueType(cvget(caps,"completionProvider"))==XVALUE_OBJECT;
   v->nativeBackend=cvbool(cvget(cvget(caps,"experimental"),"nativeCursorCompletion"),1);
   int64 kind=1;const xvalue* sync=cvget(caps,"textDocumentSync");if(xrtValueType(sync)==XVALUE_OBJECT) sync=cvget(sync,"change");
   if(sync && !xrtValueGetInt(sync,&kind)) kind=0;
   if(kind!=1 && kind!=2) kind=0;
   v->syncKind=(int)kind;
   if(v->supported) v->sync=xlsSyncCreate((int)kind);
   v->supported=v->supported && v->sync!=NULL;
   if(v->supported) cvsend(v,"initialized",xrtValueObject(),0);else cvdismiss(v);
  } else if(id>1 && id==v->request && v->wanted && !v->ready && !v->decoding) {
   int accepted=cvtakeResponse(v,m);
   if(accepted>0) m=NULL;
   else if(accepted<0) {v->deferredMessage=m;v->deferredDiscard=0;v->deferredSession=v->session;m=NULL;}
  }
  if(m && !xideCompletionDecoderDiscard(v->decoder,m)) {v->deferredMessage=m;v->deferredDiscard=1;v->deferredSession=v->session;}
  if(v->deferredMessage) {xgeRenderRequestAfter(.02f);break;}
 }
 if(xideLanguageClientHasPending(v->client)) xgeRenderRequestAfter(.02f);
}
void xideCompletionViewPoll(xide_app* app)
{
 xide_completion_view* v=app ? app->completionView : NULL; if(!v) return;
 cvretireSync(v);if(!cvflushResult(v)) return;
 if(v->repeatEditor) { xui_widget editor=v->repeatEditor; uint64_t id=v->repeatId; int offset=v->repeatOffset; v->repeatEditor=NULL;
  if(app->activeDocument && app->activeDocument->id==id && app->activeDocument->editor==editor) xideCompletionViewRequest(app,editor,offset,0);
 }
 cvread(v,1); if(!v->wanted) return;
 if(!cvcontext(v,0) || (v->inputCount && !v->pending && !cvinputsCurrent(v))) { cvdismiss(v); return; }
 if((v->client && (!cvconnectionCurrent(v) || xideLanguageClientState(v->client,NULL,0)>=XIDE_LS_STOPPED)) || (!v->client && !v->pending)) {
  int retry=v->pending; xui_widget editor=v->document->editor; int offset=v->offset; cvstopClient(v);
  if(!retry) return;
  xideCompletionViewRequest(app,editor,offset,1); xuiCodeEditShowCompletion(editor);
 }
 cvpollDecoded(v,1);
 int open=xuiCodeEditIsCompletionOpen(v->document->editor);
 if(v->sawOpen && !open) { cvdismiss(v); return; } if(open) v->sawOpen=1;
 if(v->syncToken) {cvpollSync(v);return;}
 if(!v->pending) return;
 uint64_t now=GetTickCount64();if(now<v->due) { xgeRenderRequestAfter((float)(v->due-now)/1000.0f); return; }
 if(!cvstartClient(v)) return;
 if(!v->initialized) return;
 v->pending=0; if(!cvbegin(v)) cvdismiss(v);else cvpollSync(v);
}
int xideCompletionViewCollect(xide_app* app,xui_widget editor,int offset,xui_code_completion_item_t* items,int capacity,int* count)
{
 xide_completion_view* v=app ? app->completionView : NULL; if(!v || !count) return XUI_OK;
 v->collectEligible=0; if(!v->refreshing) xideCompletionViewRequest(app,editor,offset,0);
 if(!v->result || !v->document || v->document->editor!=editor || v->offset!=offset || !cvcontext(v,0) || !cvinputsCurrent(v)) return XUI_OK;
 v->collectEligible=1;
 for(int i=0;i<v->count;++i) { if(items && *count<capacity) items[*count]=v->items[i]; if(!items || *count<capacity) ++*count; } return XUI_OK;
}
int xideCompletionViewHasName(xide_app* app,xui_widget editor,const char* name)
{
 xide_completion_view* v=app ? app->completionView : NULL;
 /* Collect validates the batch once in this synchronous provider call. */
 if(!v || !v->collectEligible || !v->result || !v->document || v->document->editor!=editor) return 0;
 return v->count && bsearch(&name,v->nameIndex,(size_t)v->count,sizeof(*v->nameIndex),cvcompareNames)!=NULL;
}
int xideCompletionViewInput(xide_app* app,xui_widget editor,const xui_code_input_event_t* event,xui_code_input_action_t* action)
{
 xide_completion_view* v=app ? app->completionView : NULL; if(!v || !event || !action) return XUI_OK;
 if(event->iPhase==XUI_CODE_INPUT_PREVIEW && event->iSource==XUI_CODE_INPUT_SOURCE_COMPLETION && event->sText && v->collectEligible && v->document && v->document->editor==editor) {
  int matched=0; for(int i=0;i<v->count;++i) if(event->iTextSize==(int)strlen(v->items[i].sInsertText) && !memcmp(event->sText,v->items[i].sInsertText,(size_t)event->iTextSize)) matched=1;
  if(!matched) return XUI_OK;
  memcpy(v->editText,event->sText,(size_t)event->iTextSize); v->editText[event->iTextSize]=0;
  cvread(v,0); action->iFlags=XUI_CODE_INPUT_ACTION_CONSUME|XUI_CODE_INPUT_ACTION_SET_SELECTION|XUI_CODE_INPUT_ACTION_CLOSE_ASSIST;
  action->iSelectionAnchor=action->iSelectionCaret=event->tSelectionBefore.iCaretOffset;
  if(v->result && !v->deferredMessage && !xideLanguageClientHasPending(v->client) && v->client && xideLanguageClientState(v->client,NULL,0)==XIDE_LS_RUNNING && cvcontext(v,1) && cvinputsCurrent(v) && cvconnectionCurrent(v) && event->iDocumentVersion==v->version && event->tSelectionBefore.iAnchorOffset==v->start && event->tSelectionBefore.iCaretOffset==v->offset) {
   v->edit=(xui_code_text_edit_t){sizeof(v->edit),{v->start,v->end},v->editText}; action->pEdits=&v->edit; action->iEditCount=1; action->iFlags|=XUI_CODE_INPUT_ACTION_APPLY_EDITS; action->iSelectionAnchor=action->iSelectionCaret=v->start+event->iTextSize;
  } return XUI_OK;
 }
 if(event->iPhase==XUI_CODE_INPUT_COMMITTED) {
  int active=(v->wanted || v->repeatEditor==editor) && v->document && v->document->editor==editor;
  int repeat=event->iSource!=XUI_CODE_INPUT_SOURCE_COMPLETION && event->iSource!=XUI_CODE_INPUT_SOURCE_PASTE;
  unsigned char last=event->sText && event->iTextSize>0 ? (unsigned char)event->sText[event->iTextSize-1] : 0;
  int typed=(event->iSource==XUI_CODE_INPUT_SOURCE_KEYBOARD || event->iSource==XUI_CODE_INPUT_SOURCE_IME) &&
   (last>=128 || isalnum(last) || last=='_' || last=='.');
  xide_document* d=app->activeDocument;
  if(active || (typed && cvnamed(d) && d->editor==editor && !strcmp(d->language,"xlang"))) {
   if(repeat && d && d->editor==editor && cvnamed(d) && !strcmp(d->language,"xlang")) {
    /* One cheap latest-intent slot for the input batch. Eligibility, lexical
       context and all document capture stay in the regular owner poll. */
    cvqueueInput(v,d,event->tSelectionAfter.iCaretOffset);
   } else cvdismiss(v);
  }
 } return XUI_OK;
}
static const int cvEvents[]={XUI_EVENT_KEY_DOWN,XUI_EVENT_BLUR,XUI_EVENT_POINTER_DOWN,XUI_EVENT_POINTER_WHEEL,XUI_EVENT_IME_COMPOSITION};
static int cvEvent(xui_widget widget,const xui_event_t* event,void* user)
{
 completion_binding* b=user; xide_completion_view* v=b->app->completionView; int index=-1;
 for(int i=0;i<5;++i) if(cvEvents[i]==event->iType) index=i;
 xide_document* d=v ? v->document : NULL;
 int deletion=v && d && d->editor==widget && (v->wanted || v->repeatEditor==widget) && event->iType==XUI_EVENT_KEY_DOWN && !event->iModifiers && (event->iKey==XUI_KEY_BACKSPACE || event->iKey==XUI_KEY_DELETE);
 uint64_t id=deletion ? d->id : 0;uint32_t version=deletion ? xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(widget)) : 0;
 int dismiss=event->iType!=XUI_EVENT_KEY_DOWN;
 if(event->iType==XUI_EVENT_KEY_DOWN) dismiss=event->iKey==XUI_KEY_ESCAPE || event->iKey==XUI_KEY_LEFT || event->iKey==XUI_KEY_RIGHT || event->iKey==XUI_KEY_HOME || event->iKey==XUI_KEY_END || ((event->iModifiers&XUI_MOD_CTRL) && event->iKey!=XUI_KEY_SPACE);
 if(v && dismiss && v->document && v->document->editor==widget) cvdismiss(v);
 int result=index>=0 && b->handlers[index] ? b->handlers[index](widget,event,b->users[index]) : XUI_OK;
 /* Native deletion commands bypass the text-commit hook. Observe the result
    after the original handler, without reproducing its editing/selection rules. */
 if(deletion && result>=0 && d->open && d->id==id && b->app->activeDocument==d && d->editor==widget && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(widget))!=version) {
  xui_code_selection_t selection={0};
  if(xuiCodeSelectionGetState(xuiCodeEditGetSelection(widget),&selection)==XUI_OK && selection.iCaretOffset==selection.iAnchorOffset) cvqueueInput(v,d,selection.iCaretOffset);
 }
 return result;
}
int xideCompletionViewBindEditor(xide_app* app,xui_widget editor)
{
 xide_completion_view* v=app->completionView; if(!v) return XUI_OK;
 for(completion_binding* b=v->bindings;b;b=b->next) if(b->editor==editor) return XUI_OK;
 completion_binding* b=calloc(1,sizeof(*b)); if(!b) return XUI_ERROR_OUT_OF_MEMORY; b->app=app; b->editor=editor; int r=XUI_OK,installed=0;
 for(int i=0;i<5 && r==XUI_OK;++i) r=xuiWidgetGetEventHandler(editor,cvEvents[i],&b->handlers[i],&b->users[i]);
 for(int i=0;i<5 && r==XUI_OK;++i) { r=xuiWidgetSetEventHandler(editor,cvEvents[i],cvEvent,b); if(r==XUI_OK) ++installed; }
 if(r!=XUI_OK) { for(int i=0;i<installed;++i) xuiWidgetSetEventHandler(editor,cvEvents[i],b->handlers[i],b->users[i]); free(b); return r; }
 b->next=v->bindings; v->bindings=b; return XUI_OK;
}
int xideCompletionViewActive(xide_app* app)
{
 xide_completion_view* v=app ? app->completionView : NULL;
 return v && v->document && app->activeDocument==v->document && v->document->open && v->document->id==v->id && (v->wanted || v->repeatEditor) && xuiGetFocusWidget(app->context)==v->document->editor;
}
int xideCompletionViewCreate(xide_app* app)
{
 app->completionView=calloc(1,sizeof(*app->completionView));xide_completion_view* v=app->completionView;if(!v) return XUI_ERROR_OUT_OF_MEMORY;
 v->app=app;v->syncPlanner=xideSyncPlannerCreate(cvwake,app);v->decoder=xideCompletionDecoderCreate(cvwake,app);if(v->syncPlanner && v->decoder) return XUI_OK;
 xideSyncPlannerDestroy(v->syncPlanner);xideCompletionDecoderDestroy(v->decoder);free(v);app->completionView=NULL;return XUI_ERROR_OUT_OF_MEMORY;
}
void xideCompletionViewStop(xide_app* app) { if(app && app->completionView) cvstopClient(app->completionView); }
void xideCompletionViewDestroy(xide_app* app)
{
 xide_completion_view* v=app ? app->completionView : NULL; if(!v) return;
 /* Stop runs before the context/documents are destroyed. No widget access here. */
 xideLanguageClientStop(v->client);xideSyncPlannerDestroy(v->syncPlanner);xideSyncResultRelease(v->syncReady);xideCompletionDecoderDestroy(v->decoder);xideCompletionResultRelease(v->decoded);xrtValueRelease(v->deferredMessage); cvfreeInputs(v); xlsSyncDestroy(v->sync); free(v->compiler); free(v->directory); free(v->package); free(v->items);
 while(v->bindings) { completion_binding* next=v->bindings->next; free(v->bindings); v->bindings=next; } free(v); app->completionView=NULL;
}
