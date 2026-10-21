#include "ui/completion_view.h"
#include "ui/task_view.h"
#include "services/language_client.h"
#include "services/language_check.h"
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
typedef struct completion_input { xide_document* document; uint64_t id; uint32_t version; int wireVersion,externalState; char *path,*uri,*text,*language; size_t length; } completion_input;
typedef struct completion_binding { struct completion_binding* next; xide_app* app; xui_widget editor; xui_widget_event_proc handlers[5]; void* users[5]; } completion_binding;
struct xide_completion_view {
 xide_app* app; xide_language_client* client; char *compiler,*directory,*package;
 completion_input inputs[COMPLETION_DOCS]; size_t inputCount; int nextId,initialized,supported,syncKind; xls_sync* sync;
 xide_document* document; uint64_t id,due; uint32_t version; int offset,wanted,pending,request,ready,sawOpen,refreshing;
 xvalue* result; xui_code_completion_item_t* items; const char** nameIndex; int count,start,end,collectEligible;
 xui_widget repeatEditor; uint64_t repeatId; int repeatOffset;
 xui_code_text_edit_t edit; char editText[513]; completion_binding* bindings;
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
static int cvsyncEmit(void* user,const char* method,const xvalue* params)
{ return cvsend(user,method,xrtValueRetain((xvalue*)params),0); }
static void cvclearResult(xide_completion_view* v)
{
 if(v->document && v->document->editor) xuiCodeEditCancelCompletion(v->document->editor);
 free(v->items); free(v->nameIndex); v->items=NULL; v->nameIndex=NULL; v->count=v->collectEligible=0; xrtValueRelease(v->result); v->result=NULL; v->sawOpen=0;
}
static void cvdismiss(xide_completion_view* v)
{
 if(v->request && v->client && v->initialized) { xvalue* p=xrtValueObject(); cvput(p,"id",xrtValueInt(v->request)); cvsend(v,"$/cancelRequest",p,0); }
 cvclearResult(v); v->wanted=v->pending=v->ready=v->request=0; v->repeatEditor=NULL;
}
static void cvfreeInput(completion_input* b)
{ free(b->path); free(b->uri); free(b->text); free(b->language); memset(b,0,sizeof(*b)); }
static void cvfreeInputs(xide_completion_view* v)
{ for(size_t i=0;i<v->inputCount;++i) cvfreeInput(&v->inputs[i]); v->inputCount=0; }
static void cvstopClient(xide_completion_view* v)
{
 cvdismiss(v); xideLanguageClientStop(v->client); v->client=NULL; cvfreeInputs(v); xlsSyncDestroy(v->sync); v->sync=NULL; free(v->compiler); free(v->directory); free(v->package); v->compiler=v->directory=v->package=NULL; v->initialized=v->supported=v->syncKind=0;
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
 xlsSyncDestroy(v->sync); v->sync=NULL; free(v->compiler); free(v->directory); free(v->package);
 v->compiler=v->directory=v->package=NULL; v->initialized=v->supported=v->syncKind=0;
 const char* compiler=xideTaskViewCompiler(v->app); char* program=cvserverPath();
 v->compiler=compiler ? cvcopy(compiler) : NULL; v->directory=xidePathDirectory(v->document->path);
 int ok=program && v->compiler && v->directory && xideLanguageCheckPackagePath(&v->package,NULL);
 if(ok) v->client=xideLanguageClientStart(program,v->compiler,v->directory,NULL);
 free(program); if(!v->client) { cvstopClient(v); return 0; }
 xideLanguageClientSetWake(v->client,cvwake,v->app); v->nextId=1;
 xvalue *p=xrtValueObject(),*options=xrtValueObject(); cvput(p,"capabilities",xrtValueObject()); cvput(options,"checkOnSave",xrtValueBool(false)); cvput(p,"initializationOptions",options);
 if(!cvsend(v,"initialize",p,1)) { cvstopClient(v); return 0; } return 1;
}
static int cvcapture(xide_completion_view* v)
{
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
  b->path=cvcopy(d->path); b->uri=xideLspPathToUri(d->path,NULL); b->text=cvcopy(xuiCodeEditGetText(d->editor)); b->language=cvcopy(d->language);
  okay=b->path && b->uri && b->text && b->language;
  if(okay) b->length=strlen(b->text);
 }
 for(size_t i=0;i<count;++i) cvfreeInput(&previous[i]);
 if(!okay) cvfreeInputs(v);
 return okay;
}
static int cvbegin(xide_completion_view* v)
{
 if(!cvcontext(v,0) || !v->supported || !v->sync || !cvcapture(v) || !cvinputsCurrent(v)) return 0;
 completion_input* target=NULL; xls_sync_document documents[COMPLETION_DOCS];
 for(size_t i=0;i<v->inputCount;++i) { completion_input* b=&v->inputs[i];
  documents[i]=(xls_sync_document){b->uri,b->text,b->length,b->id,b->version}; if(b->document==v->document) target=b;
 }
 if(!target) return 0;
 if(!xlsSyncUpdate(v->sync,documents,v->inputCount,cvsyncEmit,v)) { cvstopClient(v); return 0; }
 for(size_t i=0;i<v->inputCount;++i) v->inputs[i].wireVersion=xlsSyncVersion(v->sync,v->inputs[i].uri);
 int line=0,character=0; if(!xideLspOffsetToPosition(target->text,v->offset,&line,&character)) return 0;
 xvalue *p=xrtValueObject(),*d=xrtValueObject(),*pos=xrtValueObject(); cvput(d,"uri",xrtValueString(cvsv(target->uri))); cvput(pos,"line",xrtValueInt(line)); cvput(pos,"character",xrtValueInt(character)); cvput(p,"textDocument",d); cvput(p,"position",pos); cvput(p,"version",xrtValueInt(target->wireVersion));
 if(v->nextId==INT_MAX) return 0;
 v->request=++v->nextId; return cvsend(v,"textDocument/completion",p,v->request);
}
void xideCompletionViewRequest(xide_app* app,xui_widget editor,int offset,int force)
{
 xide_completion_view* v=app ? app->completionView : NULL; xide_document* d=app ? app->activeDocument : NULL;
 if(!v || !d || d->editor!=editor || !cveligible(app,d) || xuiGetFocusWidget(app->context)!=editor) return;
 xui_code_selection_t s={0}; if(xuiCodeSelectionGetState(xuiCodeEditGetSelection(editor),&s)!=XUI_OK || s.iCaretOffset!=offset || s.iCaretOffset!=s.iAnchorOffset) return;
 const char* text=xuiCodeEditGetText(editor); int start=offset;
 while(start>0 && ((unsigned char)text[start-1]>=128 || isalnum((unsigned char)text[start-1]) || text[start-1]=='_')) --start;
 if(offset-start>255) return;
 if(offset>0) { xui_code_token_t tokens[8]; int count=0;
  if(xuiCodeTokenBufferGetTokensInRange(xuiCodeEditGetTokenBuffer(editor),xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(editor)),offset-1,offset,tokens,8,&count)!=XUI_OK) return;
  for(int i=0;i<count && i<8;++i) if(tokens[i].iKind==XUI_CODE_TOKEN_COMMENT || tokens[i].iKind==XUI_CODE_TOKEN_STRING || tokens[i].iKind==XUI_CODE_TOKEN_CHAR || tokens[i].iKind==XUI_CODE_TOKEN_ERROR) return;
 }
 uint32_t version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(editor));
 if(!force && v->wanted && v->document==d && v->id==d->id && v->version==version && v->offset==offset && (!v->inputCount || cvinputsCurrent(v))) return;
 cvdismiss(v); v->document=d; v->id=d->id; v->version=version; v->offset=offset; v->wanted=v->pending=1; v->due=GetTickCount64()+(force ? 0 : 80); xgeRenderRequest();
}
static int cvrange(const char* text,const xvalue* range,int* start,int* end)
{
 int sl=0,sc=0,el=0,ec=0; return cvnumber(cvget(cvget(range,"start"),"line"),&sl) && cvnumber(cvget(cvget(range,"start"),"character"),&sc) && cvnumber(cvget(cvget(range,"end"),"line"),&el) && cvnumber(cvget(cvget(range,"end"),"character"),&ec) && xideLspPositionToOffset(text,sl,sc,start) && xideLspPositionToOffset(text,el,ec,end) && *start<=*end;
}
static int cvsameRange(const xvalue* a,const xvalue* b)
{
 const char* points[]={"start","end"}; const char* axes[]={"line","character"};
 for(int i=0;i<2;++i) for(int j=0;j<2;++j) { int x=0,y=0; if(!cvnumber(cvget(cvget(a,points[i]),axes[j]),&x) || !cvnumber(cvget(cvget(b,points[i]),axes[j]),&y) || x!=y) return 0; }
 return 1;
}
static int cvname(const char* name)
{
 size_t n=name ? strlen(name) : 0; if(!n || n>512 || ((unsigned char)name[0]<128 && !isalpha((unsigned char)name[0]) && name[0]!='_')) return 0;
 for(size_t i=0;i<n;++i) if((unsigned char)name[i]<128 && !isalnum((unsigned char)name[i]) && name[i]!='_') return 0;
 return 1;
}
static int cvcompareNames(const void* a,const void* b) { return strcmp(*(const char* const*)a,*(const char* const*)b); }
static int cvapply(xide_completion_view* v,const xvalue* result)
{
 if(!cvcontext(v,0) || !cvinputsCurrent(v)) return 0;
 completion_input* target=NULL; for(size_t i=0;i<v->inputCount;++i) if(v->inputs[i].document==v->document) target=&v->inputs[i];
 int version=0,query=0; const char *scope=cvstring(cvget(result,"scope")),*guard=cvstring(cvget(result,"guardScope"));
 const xvalue* list=cvget(result,"items");
 if(!target || !cvnumber(cvget(result,"version"),&version) || version!=target->wireVersion || !cvnumber(cvget(result,"queryId"),&query) || query!=v->request || !scope || strcmp(scope,"cursor-query") || !guard || strcmp(guard,"background-poll") || !cvbool(cvget(result,"checkedCompilation"),0) || !cvbool(cvget(result,"isIncomplete"),0) || !list || xrtValueType(list)!=XVALUE_ARRAY || xrtValueCount(list)>COMPLETION_ITEMS) return 0;
 if(cvbool(cvget(result,"available"),0)) return xrtValueCount(list)==0 && cvbool(cvget(result,"ok"),0);
 const char* prefix=cvstring(cvget(result,"prefix")); int start=0,end=0;
 if(!cvbool(cvget(result,"available"),1) || !cvbool(cvget(result,"ok"),1) || !prefix || !cvrange(target->text,cvget(result,"replacementRange"),&start,&end) || start>v->offset || end<v->offset || strlen(prefix)!=(size_t)(v->offset-start) || memcmp(prefix,target->text+start,strlen(prefix))) return 0;
 if(start==end) { if(!start || target->text[start-1]!='.') return 0; }
 else {
  if(start>0 && ((unsigned char)target->text[start-1]>=128 || isalnum((unsigned char)target->text[start-1]) || target->text[start-1]=='_')) return 0;
  if((unsigned char)target->text[end]>=128 || isalnum((unsigned char)target->text[end]) || target->text[end]=='_') return 0;
  for(int at=start;at<end;++at) if((unsigned char)target->text[at]<128 && !isalnum((unsigned char)target->text[at]) && target->text[at]!='_') return 0;
 }
 size_t n=xrtValueCount(list); xui_code_completion_item_t* items=calloc(n ? n : 1,sizeof(*items)); if(!items) return 0;
 for(size_t i=0;i<n;++i) { const xvalue *item=xrtValueArrayGet(list,i),*edit=cvget(item,"textEdit"); int format=0,kind=0; int64 symbol=0;
  const char *name=cvstring(cvget(item,"label")),*insert=cvstring(cvget(edit,"newText")),*detail=cvstring(cvget(item,"detail")),*filter=cvstring(cvget(item,"filterText")),*sort=cvstring(cvget(item,"sortText"));
  if(!cvname(name) || !insert || strcmp(name,insert) || !filter || strcmp(filter,name) || !sort || strlen(sort)>32 || !cvnumber(cvget(item,"insertTextFormat"),&format) || format!=1 || !cvnumber(cvget(item,"kind"),&kind) || (kind!=2 && kind!=3 && kind!=5 && kind!=6 && kind!=7 && kind!=21) || !xrtValueGetInt(cvget(cvget(item,"data"),"symbolId"),&symbol) || symbol<0 || (uint64_t)symbol>UINT32_MAX || !cvsameRange(cvget(edit,"range"),cvget(result,"replacementRange")) || (cvget(item,"detail") && (!detail || strlen(detail)>8192))) { free(items); return 0; }
  items[i].iSize=sizeof(items[i]); items[i].sLabel=name; items[i].sInsertText=name; items[i].sFilterText=name; items[i].sDetail=detail ? detail : ""; items[i].sSortText=sort; items[i].iSortOrder=(int)i; items[i].iUserData=(intptr_t)symbol;
 }
 const char** names=malloc((n ? n : 1)*sizeof(*names)); if(!names) { free(items); return 0; }
 for(size_t i=0;i<n;++i) names[i]=items[i].sLabel;
 qsort(names,n,sizeof(*names),cvcompareNames);
 v->nameIndex=names; v->result=xrtValueRetain((xvalue*)result); v->items=items; v->count=(int)n; v->start=start; v->end=end; return 1;
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
static void cvread(xide_completion_view* v,int refresh)
{
 xvalue* m=NULL;
 while(v->client && (m=xideLanguageClientNext(v->client))) {
  int id=0; const char* method=cvstring(cvget(m,"method"));
  if(method && !strcmp(method,"xlang/completionInvalidated")) { int query=0; if(cvnumber(cvget(cvget(m,"params"),"queryId"),&query) && query==v->request) cvdismiss(v); }
  else if(cvnumber(cvget(m,"id"),&id) && id==1 && !v->initialized) {
   const xvalue* caps=cvget(cvget(m,"result"),"capabilities"); v->initialized=1; v->supported=cvbool(cvget(cvget(caps,"experimental"),"cursorCompletion"),1) && xrtValueType(cvget(caps,"completionProvider"))==XVALUE_OBJECT;
   int64 kind=1; const xvalue* sync=cvget(caps,"textDocumentSync");
   if(xrtValueType(sync)==XVALUE_OBJECT) sync=cvget(sync,"change");
   if(sync && !xrtValueGetInt(sync,&kind)) kind=0;
   if(kind!=1 && kind!=2) kind=0;
   v->syncKind=(int)kind;
   if(v->supported) v->sync=xlsSyncCreate((int)kind);
   v->supported=v->supported && v->sync!=NULL;
   if(v->supported) cvsend(v,"initialized",xrtValueObject(),0); else cvdismiss(v);
  } else if(id>1 && id==v->request && v->wanted && !v->ready) {
   v->ready=1; const xvalue* result=cvget(m,"result");
   if(!cvget(m,"error") && cvapply(v,result) && refresh && v->count) {
    v->refreshing=1; xuiCodeEditShowCompletion(v->document->editor); v->refreshing=0; cvreveal(v,xuiCodeEditGetCompletionPopupWidget(v->document->editor));
    if(v->end>v->offset) xuiCodeEditClearInlineCompletion(v->document->editor);
    v->sawOpen=xuiCodeEditIsCompletionOpen(v->document->editor); xgeRenderRequest();
   }
  }
  xrtValueRelease(m);
 }
}
void xideCompletionViewPoll(xide_app* app)
{
 xide_completion_view* v=app ? app->completionView : NULL; if(!v) return;
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
 int open=xuiCodeEditIsCompletionOpen(v->document->editor);
 if(v->sawOpen && !open) { cvdismiss(v); return; } if(open) v->sawOpen=1;
 if(!v->pending) return;
 if(GetTickCount64()<v->due) { xgeRenderRequest(); return; }
 if(!cvstartClient(v)) return;
 if(!v->initialized) return;
 v->pending=0; if(!cvbegin(v)) cvdismiss(v);
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
  if(v->result && v->client && xideLanguageClientState(v->client,NULL,0)==XIDE_LS_RUNNING && cvcontext(v,1) && cvinputsCurrent(v) && cvconnectionCurrent(v) && event->iDocumentVersion==v->version && event->tSelectionBefore.iAnchorOffset==v->start && event->tSelectionBefore.iCaretOffset==v->offset) {
   v->edit=(xui_code_text_edit_t){sizeof(v->edit),{v->start,v->end},v->editText}; action->pEdits=&v->edit; action->iEditCount=1; action->iFlags|=XUI_CODE_INPUT_ACTION_APPLY_EDITS; action->iSelectionAnchor=action->iSelectionCaret=v->start+event->iTextSize;
  } return XUI_OK;
 }
 if(event->iPhase==XUI_CODE_INPUT_COMMITTED && (v->wanted || v->repeatEditor==editor) && v->document && v->document->editor==editor) {
  int repeat=event->iSource!=XUI_CODE_INPUT_SOURCE_COMPLETION && event->iSource!=XUI_CODE_INPUT_SOURCE_PASTE;
  cvdismiss(v); if(repeat) { v->repeatEditor=editor; v->repeatId=v->document->id; v->repeatOffset=event->tSelectionAfter.iCaretOffset; xgeRenderRequest(); }
 } return XUI_OK;
}
static const int cvEvents[]={XUI_EVENT_KEY_DOWN,XUI_EVENT_BLUR,XUI_EVENT_POINTER_DOWN,XUI_EVENT_POINTER_WHEEL,XUI_EVENT_IME_COMPOSITION};
static int cvEvent(xui_widget widget,const xui_event_t* event,void* user)
{
 completion_binding* b=user; xide_completion_view* v=b->app->completionView; int index=-1;
 for(int i=0;i<5;++i) if(cvEvents[i]==event->iType) index=i;
 int dismiss=event->iType!=XUI_EVENT_KEY_DOWN;
 if(event->iType==XUI_EVENT_KEY_DOWN) dismiss=event->iKey==XUI_KEY_ESCAPE || event->iKey==XUI_KEY_LEFT || event->iKey==XUI_KEY_RIGHT || event->iKey==XUI_KEY_HOME || event->iKey==XUI_KEY_END || ((event->iModifiers&XUI_MOD_CTRL) && event->iKey!=XUI_KEY_SPACE);
 if(v && dismiss && v->document && v->document->editor==widget) cvdismiss(v);
 return index>=0 && b->handlers[index] ? b->handlers[index](widget,event,b->users[index]) : XUI_OK;
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
int xideCompletionViewCreate(xide_app* app) { app->completionView=calloc(1,sizeof(*app->completionView)); if(app->completionView) app->completionView->app=app; return app->completionView ? XUI_OK : XUI_ERROR_OUT_OF_MEMORY; }
void xideCompletionViewStop(xide_app* app) { if(app && app->completionView) cvstopClient(app->completionView); }
void xideCompletionViewDestroy(xide_app* app)
{
 xide_completion_view* v=app ? app->completionView : NULL; if(!v) return;
 /* Stop runs before the context/documents are destroyed. No widget access here. */
 xideLanguageClientStop(v->client); cvfreeInputs(v); xlsSyncDestroy(v->sync); free(v->compiler); free(v->directory); free(v->package); free(v->items); free(v->nameIndex); xrtValueRelease(v->result);
 while(v->bindings) { completion_binding* next=v->bindings->next; free(v->bindings); v->bindings=next; } free(v); app->completionView=NULL;
}
