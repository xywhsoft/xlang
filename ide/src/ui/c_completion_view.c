#include "ui/c_completion_view.h"
#include "services/c_completion_job.h"
#include "ui/c_analysis_inputs.h"
#include "services/c_commands.h"
#include "platform/path.h"
#include "platform/file_watch.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#define CCV_DOCS 64u
#define CCV_ITEMS 4096u
typedef xide_c_input ccv_input;
typedef struct ccv_binding { struct ccv_binding* next; xide_app* app; xui_widget editor; xui_widget_event_proc handlers[5]; void* users[5]; } ccv_binding;
struct xide_c_completion_view {
 xide_app* app; xide_c_completion_job *job,*retired[4]; size_t retiredCount;
 xide_c_inputs* batch; ccv_input* inputs; size_t inputCount; xvalue *model,*result;
 char *server,*override; char message[768]; xide_document* document; uint64_t id,due; uint32_t version;
 int offset,wanted,pending,ready,start,end,sawOpen,refreshing,collectEligible;
 xui_code_completion_item_t* items; const char** names; int count;
 xui_code_text_edit_t edit; char editText[513]; ccv_binding* bindings;
 xui_widget repeat; uint64_t repeatId; int repeatOffset;
};
static xstrview ccsv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* ccget(const xvalue* v,const char* k) { return v && xrtValueType(v)==XVALUE_OBJECT ? xrtValueObjectGet(v,ccsv(k)) : NULL; }
static const char* ccstr(const xvalue* v) { xstrview s={0}; return v && xrtValueGetString(v,&s) && s.Size<=131072 && !memchr(s.Data,0,s.Size) && (!s.Size || MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.Data,(int)s.Size,NULL,0)) ? s.Data : NULL; }
static int ccint(const xvalue* v,int* out) { int64 n=0; if(!v || !xrtValueGetInt(v,&n) || n<0 || n>INT_MAX) return 0; *out=(int)n; return 1; }
static int ccident(unsigned char c) { return c>=128 || isalnum(c) || c=='_'; }
static int ccnamed(xide_document* d) { return d && d->open && d->path && (xidePathHasExtension(d->path,".c") || xidePathHasExtension(d->path,".h")); }
const char* xideCCompletionViewServer(xide_app* app)
{ return app && app->cCompletionView && app->cCompletionView->override ? app->cCompletionView->override : app && app->cServerPath ? app->cServerPath : app ? app->preferences.cServer : NULL; }
static int ccusable(xide_app* app,xide_document* d)
{
 return ccnamed(d) && !strcmp(d->language,"c") && !strcmp(xuiCodeEditGetLanguage(d->editor),"c") && !xuiCodeEditIsReadonly(d->editor) && xuiCodeSelectionGetCount(xuiCodeEditGetSelection(d->editor))==1 && (!d->externalState || d->externalState==XIDE_WATCH_MISSING) && !xideActionsHasModal(app) && xideCCompletionViewServer(app);
}
static int cccontext(xide_c_completion_view* v,int accepting)
{
 xide_document* d=v->document; xui_code_selection_t s={0};
 return v->wanted && d && v->app->activeDocument==d && ccusable(v->app,d) && d->id==v->id && v->version==xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor)) && xuiGetFocusWidget(v->app->context)==d->editor && xuiCodeSelectionGetState(xuiCodeEditGetSelection(d->editor),&s)==XUI_OK && s.iCaretOffset==v->offset && (s.iAnchorOffset==s.iCaretOffset || (accepting && s.iAnchorOffset==v->start));
}
static int ccinputsCurrent(xide_c_completion_view* v)
{ return v->batch && xideCInputsCurrent(v->batch); }
static void ccreap(xide_c_completion_view* v)
{
 for(size_t i=0;i<v->retiredCount;) { if(xideCCompletionJobDone(v->retired[i])) { xideCCompletionJobDestroy(v->retired[i]); v->retired[i]=v->retired[--v->retiredCount]; } else ++i; }
}
static void ccclear(xide_c_completion_view* v)
{
 if(v->document && v->document->editor) xuiCodeEditCancelCompletion(v->document->editor);
 free(v->items); free(v->names); v->items=NULL; v->names=NULL; v->count=v->collectEligible=0; xrtValueRelease(v->result); v->result=NULL;
}
static int ccdismiss(xide_c_completion_view* v)
{
 ccclear(v); v->wanted=v->pending=v->ready=v->sawOpen=0; v->repeat=NULL;
 if(v->job) { xideCCompletionJobCancel(v->job); ccreap(v); if(v->retiredCount>=4) return 0; v->retired[v->retiredCount++]=v->job; v->job=NULL; }
 xideCInputsDestroy(v->batch); v->batch=NULL; v->inputs=NULL; v->inputCount=0; v->model=NULL; v->server=NULL; return 1;
}
static void ccwake(void* user) { (void)user; xgeRenderRequest(); }
static int ccbegin(xide_c_completion_view* v)
{
 if(!cccontext(v,0) || v->retiredCount>=4) return 0;
 xide_model_error e={0}; v->batch=xideCInputsCapture(v->app,v->document,&e);
 if(v->batch) {
  v->inputs=v->batch->items; v->inputCount=v->batch->count; v->model=v->batch->model; v->server=v->batch->server;
  xide_c_job_spec spec=xideCInputsSpec(v->batch,v->document,v->offset,ccwake); v->job=xideCCompletionJobStart(&spec,&e);
 }
 snprintf(v->message,sizeof(v->message),"%s",*e.message ? e.message : v->job ? "" : "C/xlang analysis inputs/configuration are ambiguous, conflicting or exceed capacity.");
 return v->job!=NULL;
}
void xideCCompletionViewRequest(xide_app* app,xui_widget editor,int offset,int force)
{
 xide_c_completion_view* v=app ? app->cCompletionView : NULL; xide_document* d=app ? app->activeDocument : NULL;
 if(!v || !d || d->editor!=editor || !ccusable(app,d) || xuiGetFocusWidget(app->context)!=editor) return;
 xui_code_selection_t s={0}; if(xuiCodeSelectionGetState(xuiCodeEditGetSelection(editor),&s)!=XUI_OK || s.iCaretOffset!=offset || s.iCaretOffset!=s.iAnchorOffset) return;
 const char* text=xuiCodeEditGetText(editor); int start=offset; while(start>0 && ccident((unsigned char)text[start-1])) --start; if(offset-start>255) return;
 if(offset>0) { xui_code_token_t tokens[8]; int count=0;
  if(xuiCodeTokenBufferGetTokensInRange(xuiCodeEditGetTokenBuffer(editor),xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(editor)),offset-1,offset,tokens,8,&count)!=XUI_OK) return;
  for(int i=0;i<count && i<8;++i) if(tokens[i].iKind==XUI_CODE_TOKEN_COMMENT || tokens[i].iKind==XUI_CODE_TOKEN_STRING || tokens[i].iKind==XUI_CODE_TOKEN_CHAR || tokens[i].iKind==XUI_CODE_TOKEN_ERROR) return;
 }
 uint32_t version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(editor));
 if(!force && v->wanted && v->document==d && v->id==d->id && v->version==version && v->offset==offset && (!v->inputCount || ccinputsCurrent(v))) return;
 if(!ccdismiss(v)) return;
 v->document=d; v->id=d->id; v->version=version; v->offset=offset; v->wanted=v->pending=1; v->due=GetTickCount64()+(force ? 0 : 80); xgeRenderRequest();
}
static int cccompare(const void* a,const void* b) { return strcmp(*(const char* const*)a,*(const char* const*)b); }
static int ccname(const char* text) { size_t n=text ? strlen(text) : 0; if(!n || n>512 || ((unsigned char)text[0]<128 && !isalpha((unsigned char)text[0]) && text[0]!='_')) return 0; for(size_t i=0;i<n;++i) if(!ccident((unsigned char)text[i])) return 0; return 1; }
static int ccapply(xide_c_completion_view* v,const xvalue* message)
{
 if(!cccontext(v,0) || !ccinputsCurrent(v) || ccget(message,"error")) return 0;
 const xvalue* result=ccget(message,"result"); const xvalue* list=xrtValueType(result)==XVALUE_ARRAY ? result : ccget(result,"items");
 if(!list || xrtValueType(list)!=XVALUE_ARRAY || xrtValueCount(list)>CCV_ITEMS) return 0;
 const char* text=xuiCodeEditGetText(v->document->editor); int start=v->offset,end=v->offset;
 while(start>0 && ccident((unsigned char)text[start-1])) --start;
 while(ccident((unsigned char)text[end])) ++end;
 size_t n=xrtValueCount(list); xui_code_completion_item_t* items=calloc(n ? n : 1,sizeof(*items)); const char** names=calloc(n ? n : 1,sizeof(*names)); xide_lsp_position* positions=calloc(n ? 2*n : 1,sizeof(*positions)); int okay=items && names && positions;
 for(size_t i=0;okay && i<n;++i) {
  const xvalue* item=xrtValueArrayGet(list,i); const xvalue* edit=ccget(item,"textEdit"); const char *label=ccstr(ccget(item,"label")),*insert=ccstr(ccget(edit,"newText")),*filter=ccstr(ccget(item,"filterText")),*detail=ccstr(ccget(item,"detail")),*sort=ccstr(ccget(item,"sortText")); int format=1;
  okay=label && *label && strlen(label)<=512 && insert && *insert && strlen(insert)<=512 && !strpbrk(insert,"\r\n") && filter && ccname(filter) && (!ccget(item,"detail") || (detail && strlen(detail)<=8192)) && (!ccget(item,"sortText") || (sort && strlen(sort)<=512)) && (!ccget(item,"insertTextFormat") || (ccint(ccget(item,"insertTextFormat"),&format) && format==1)) && (!ccget(item,"additionalTextEdits") || (xrtValueType(ccget(item,"additionalTextEdits"))==XVALUE_ARRAY && xrtValueCount(ccget(item,"additionalTextEdits"))==0));
  if(okay) okay=ccint(ccget(ccget(ccget(edit,"range"),"start"),"line"),&positions[2*i].line) && ccint(ccget(ccget(ccget(edit,"range"),"start"),"character"),&positions[2*i].character) && ccint(ccget(ccget(ccget(edit,"range"),"end"),"line"),&positions[2*i+1].line) && ccint(ccget(ccget(ccget(edit,"range"),"end"),"character"),&positions[2*i+1].character);
  if(okay) { items[i]=(xui_code_completion_item_t){0}; items[i].iSize=sizeof(items[i]); items[i].sLabel=label; items[i].sInsertText=insert; items[i].sFilterText=filter; items[i].sDetail=detail ? detail : ""; items[i].sSortText=sort ? sort : ""; items[i].iSortOrder=(int)i; items[i].iUserData=(intptr_t)i; names[i]=filter; }
 }
 if(okay) okay=xideLspPositionsToOffsets(text,positions,n*2);
 for(size_t i=0;okay && i<n;++i) okay=positions[2*i].offset==start && positions[2*i+1].offset>=v->offset && positions[2*i+1].offset<=end;
 free(positions); if(!okay) { free(items); free(names); return 0; }
 qsort(names,n,sizeof(*names),cccompare); v->items=items; v->names=names; v->count=(int)n; v->result=xrtValueRetain((xvalue*)message); v->start=start; v->end=end; return 1;
}
static void ccreveal(xide_c_completion_view* v,xui_widget widget)
{
 if(!widget) return;
 if(xuiWidgetGetType(widget)==xuiListViewGetType(v->app->context)) {
  float row=0,padding=0,border=0; xui_rect_t rect=xuiWidgetGetRect(widget);
  if(xuiListViewGetMetrics(widget,&row,&padding,&border)==XUI_OK && row>0 && row<=128 && border>=0 && border<=8 && rect.fH<(int)ceilf(row+2*border)) { rect.fH=(int)ceilf(row+2*border); xuiWidgetSetRect(widget,rect); xuiPopupSetContentSize(xuiCodeEditGetCompletionPopupWidget(v->document->editor),rect.fW,rect.fH); }
  xuiListViewSetScroll(widget,0); xuiListViewEnsureVisible(widget,xuiCodeEditGetCompletionSelected(v->document->editor)); return;
 }
 for(xui_widget child=xuiWidgetGetFirstChild(widget);child;child=xuiWidgetGetNextSibling(child)) ccreveal(v,child);
}
void xideCCompletionViewPoll(xide_app* app)
{
 xide_c_completion_view* v=app ? app->cCompletionView : NULL; if(!v) return; ccreap(v);
 if(v->repeat) { xui_widget editor=v->repeat; uint64_t id=v->repeatId; int offset=v->repeatOffset; v->repeat=NULL; if(app->activeDocument && app->activeDocument->id==id && app->activeDocument->editor==editor) xideCCompletionViewRequest(app,editor,offset,0); }
 if(!v->wanted) { if(v->job) ccdismiss(v); return; }
 if(!cccontext(v,0) || (v->inputCount && !ccinputsCurrent(v))) { ccdismiss(v); return; }
 int open=xuiCodeEditIsCompletionOpen(v->document->editor); if(v->sawOpen && !open) { ccdismiss(v); return; } if(open) v->sawOpen=1;
 if(v->pending) { if(GetTickCount64()<v->due || v->retiredCount>=4) { xgeRenderRequest(); return; } v->pending=0; if(!ccbegin(v)) { ccdismiss(v); return; } }
 if(!v->job) return;
 enum xide_c_job_state state=xideCCompletionJobState(v->job,v->message,sizeof(v->message));
 if(state>=XIDE_C_JOB_FAILED) { ccdismiss(v); return; }
 if(state==XIDE_C_JOB_READY && !v->ready) {
  v->ready=1; xvalue* result=xideCCompletionJobResult(v->job);
  if(result && ccapply(v,result) && v->count) {
   v->refreshing=1; xuiCodeEditShowCompletion(v->document->editor); v->refreshing=0; ccreveal(v,xuiCodeEditGetCompletionPopupWidget(v->document->editor));
   if(v->end>v->offset) xuiCodeEditClearInlineCompletion(v->document->editor);
   v->sawOpen=xuiCodeEditIsCompletionOpen(v->document->editor); xgeRenderRequest();
  } xrtValueRelease(result);
 }
}
int xideCCompletionViewCollect(xide_app* app,xui_widget editor,int offset,xui_code_completion_item_t* items,int capacity,int* count)
{
 xide_c_completion_view* v=app ? app->cCompletionView : NULL; if(!v || !count) return XUI_OK; v->collectEligible=0;
 if(!v->refreshing) xideCCompletionViewRequest(app,editor,offset,0);
 if(!v->result || !v->document || v->document->editor!=editor || offset!=v->offset || !cccontext(v,0) || !ccinputsCurrent(v) || xideCCompletionJobState(v->job,NULL,0)!=XIDE_C_JOB_READY) return XUI_OK;
 v->collectEligible=1; for(int i=0;i<v->count;++i) { if(items && *count<capacity) items[*count]=v->items[i]; if(!items || *count<capacity) ++*count; } return XUI_OK;
}
int xideCCompletionViewHasName(xide_app* app,xui_widget editor,const char* name)
{
 xide_c_completion_view* v=app ? app->cCompletionView : NULL;
 return v && v->collectEligible && v->result && v->document && v->document->editor==editor && v->count && bsearch(&name,v->names,(size_t)v->count,sizeof(*v->names),cccompare)!=NULL;
}
int xideCCompletionViewInput(xide_app* app,xui_widget editor,const xui_code_input_event_t* event,xui_code_input_action_t* action)
{
 xide_c_completion_view* v=app ? app->cCompletionView : NULL; if(!v || !event || !action) return XUI_OK;
 if(event->iPhase==XUI_CODE_INPUT_PREVIEW && event->iSource==XUI_CODE_INPUT_SOURCE_COMPLETION && event->sText && v->collectEligible && v->document && v->document->editor==editor) {
  int matched=0; for(int i=0;i<v->count;++i) if(event->iTextSize==(int)strlen(v->items[i].sInsertText) && !memcmp(event->sText,v->items[i].sInsertText,(size_t)event->iTextSize)) matched=1;
  if(!matched || event->iTextSize>512) return XUI_OK;
  memcpy(v->editText,event->sText,(size_t)event->iTextSize); v->editText[event->iTextSize]=0;
  action->iFlags=XUI_CODE_INPUT_ACTION_CONSUME|XUI_CODE_INPUT_ACTION_SET_SELECTION|XUI_CODE_INPUT_ACTION_CLOSE_ASSIST; action->iSelectionAnchor=action->iSelectionCaret=event->tSelectionBefore.iCaretOffset;
  if(v->result && xideCCompletionJobState(v->job,NULL,0)==XIDE_C_JOB_READY && cccontext(v,1) && ccinputsCurrent(v) && event->iDocumentVersion==v->version && event->tSelectionBefore.iAnchorOffset==v->start && event->tSelectionBefore.iCaretOffset==v->offset) {
   v->edit=(xui_code_text_edit_t){sizeof(v->edit),{v->start,v->end},v->editText}; action->pEdits=&v->edit; action->iEditCount=1; action->iFlags|=XUI_CODE_INPUT_ACTION_APPLY_EDITS; action->iSelectionAnchor=action->iSelectionCaret=v->start+event->iTextSize;
  } return XUI_OK;
 }
 if(event->iPhase==XUI_CODE_INPUT_COMMITTED && (v->wanted || v->repeat==editor) && v->document && v->document->editor==editor) {
  int repeat=event->iSource!=XUI_CODE_INPUT_SOURCE_COMPLETION && event->iSource!=XUI_CODE_INPUT_SOURCE_PASTE; uint64_t id=v->document->id;
  if(ccdismiss(v) && repeat) { v->repeat=editor; v->repeatId=id; v->repeatOffset=event->tSelectionAfter.iCaretOffset; xgeRenderRequest(); }
 } return XUI_OK;
}
static const int ccEvents[]={XUI_EVENT_KEY_DOWN,XUI_EVENT_BLUR,XUI_EVENT_POINTER_DOWN,XUI_EVENT_POINTER_WHEEL,XUI_EVENT_IME_COMPOSITION};
static int ccevent(xui_widget widget,const xui_event_t* event,void* user)
{
 ccv_binding* b=user; xide_c_completion_view* v=b->app->cCompletionView; int index=-1; for(int i=0;i<5;++i) if(ccEvents[i]==event->iType) index=i;
 int dismiss=event->iType!=XUI_EVENT_KEY_DOWN;
 if(event->iType==XUI_EVENT_KEY_DOWN) dismiss=event->iKey==XUI_KEY_ESCAPE || event->iKey==XUI_KEY_LEFT || event->iKey==XUI_KEY_RIGHT || event->iKey==XUI_KEY_HOME || event->iKey==XUI_KEY_END || ((event->iModifiers&XUI_MOD_CTRL) && event->iKey!=XUI_KEY_SPACE);
 if(v && dismiss && v->document && v->document->editor==widget) ccdismiss(v);
 return index>=0 && b->handlers[index] ? b->handlers[index](widget,event,b->users[index]) : XUI_OK;
}
int xideCCompletionViewBindEditor(xide_app* app,xui_widget editor)
{
 xide_c_completion_view* v=app->cCompletionView; if(!v) return XUI_OK;
 for(ccv_binding* b=v->bindings;b;b=b->next) if(b->editor==editor) return XUI_OK;
 ccv_binding* b=calloc(1,sizeof(*b)); if(!b) return XUI_ERROR_OUT_OF_MEMORY; b->app=app; b->editor=editor; int r=XUI_OK,installed=0;
 for(int i=0;i<5 && r==XUI_OK;++i) r=xuiWidgetGetEventHandler(editor,ccEvents[i],&b->handlers[i],&b->users[i]);
 for(int i=0;i<5 && r==XUI_OK;++i) { r=xuiWidgetSetEventHandler(editor,ccEvents[i],ccevent,b); if(r==XUI_OK) ++installed; }
 if(r!=XUI_OK) { for(int i=0;i<installed;++i) xuiWidgetSetEventHandler(editor,ccEvents[i],b->handlers[i],b->users[i]); free(b); return r; } b->next=v->bindings; v->bindings=b; return XUI_OK;
}
int xideCCompletionViewAction(xide_app* app,int action)
{
 (void)action;if(!app || !app->cCompletionView) return 0;
 char* server=xideMessageChoosePath(app,XUI_FILE_DIALOG_MODE_OPEN_FILE,"选择 clangd C 语言服务器","可执行程序|*.exe;*|所有文件|*.*",app->cCompletionView->override,NULL,UINT64_C(721),0);
 if(server) { xide_model_error e={0};if(xideSettingsSetCServer(&app->preferences,server,&e)) { ccdismiss(app->cCompletionView);free(app->cCompletionView->override);app->cCompletionView->override=server;server=NULL; } else xideMessageShow(app,"C 语言服务器设置",e.message,XUI_MSGBOX_ICON_WAR);free(server); }
 return 1;
}
int xideCCompletionViewCreate(xide_app* app) { app->cCompletionView=calloc(1,sizeof(*app->cCompletionView)); if(app->cCompletionView) app->cCompletionView->app=app; return app->cCompletionView ? XUI_OK : XUI_ERROR_OUT_OF_MEMORY; }
void xideCCompletionViewStop(xide_app* app)
{
 xide_c_completion_view* v=app ? app->cCompletionView : NULL; if(!v) return; ccdismiss(v); xideCCompletionJobDestroy(v->job); v->job=NULL;
 for(size_t i=0;i<v->retiredCount;++i) xideCCompletionJobDestroy(v->retired[i]);
 v->retiredCount=0;
}
void xideCCompletionViewDestroy(xide_app* app)
{
 xide_c_completion_view* v=app ? app->cCompletionView : NULL; if(!v) return;
 xideCCompletionJobDestroy(v->job); for(size_t i=0;i<v->retiredCount;++i) xideCCompletionJobDestroy(v->retired[i]);
 for(size_t i=0;i<v->inputCount;++i) { free(v->inputs[i].path); free(v->inputs[i].text); free(v->inputs[i].language); }
 free(v->server); free(v->override); free(v->items); free(v->names); xrtValueRelease(v->model); xrtValueRelease(v->result);
 while(v->bindings) { ccv_binding* next=v->bindings->next; free(v->bindings); v->bindings=next; } free(v); app->cCompletionView=NULL;
}

int xideCCompletionViewActive(xide_app* app) { xide_c_completion_view* v=app ? app->cCompletionView : NULL; return v && v->document && v->document->open && v->document->id==v->id && xuiCodeEditIsCompletionOpen(v->document->editor); }
