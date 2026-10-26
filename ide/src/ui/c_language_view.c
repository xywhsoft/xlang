#include "ui/c_language_view.h"
#include "ui/c_analysis_inputs.h"
#include "ui/c_completion_view.h"
#include "ui/task_view.h"
#include "ui/editor_view.h"
#include "platform/path.h"
#include "platform/file_watch.h"
#include "theme/theme.h"
#include "services/ui_font.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
typedef struct cv_diag { xide_c_input* input; int start,end; } cv_diag;
typedef struct cv_symbol { char* title; int parent,start,end; } cv_symbol;
typedef struct cv_query { uint64_t token,id; uint32_t version; xide_document* document; int offset; } cv_query;
typedef struct cv_binding { struct cv_binding* next; xide_app* app; xui_widget editor; xui_widget_event_proc handlers[8]; void* users[8]; } cv_binding;
struct xide_c_language_view {
 xide_app* app; xide_c_inputs* inputs; xide_c_completion_job *job,*retired[4]; size_t retiredCount;
 uint64_t sequence,due,hoverDue; int paused,activate,ready,received,stopped; unsigned retryCount;
 xide_c_event diagnostics[64]; size_t eventCount; cv_diag* flat; size_t diagnosticCount;
 xui_widget outline,xlangOutline,hoverPopup,hoverText; cv_symbol* symbols; size_t symbolCount; xvalue *symbolResult,*hoverResult;
 char* outlineTitle; cv_query definition,outlineQuery,hover; int hoverPointer,hoverTheme; xui_font hoverFont; float hoverX,hoverY; cv_binding* bindings;
 char message[768];
};
static xstrview cvsv(const char* s) { return (xstrview){s,strlen(s)}; }
static const xvalue* cvget(const xvalue* v,const char* k) { return v && xrtValueType(v)==XVALUE_OBJECT ? xrtValueObjectGet(v,cvsv(k)) : NULL; }
static char* cvcopy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static const char* cvstr(const xvalue* v) { xstrview s={0}; return v && xrtValueGetString(v,&s) && s.Size<=131072 && !memchr(s.Data,0,s.Size) && (!s.Size || MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.Data,(int)s.Size,NULL,0)) ? s.Data : NULL; }
static int cvint(const xvalue* v,int* out) { int64 n=0; if(!v || !xrtValueGetInt(v,&n) || n<0 || n>INT_MAX) return 0; *out=(int)n; return 1; }
static int cvposition(const xvalue* v,xide_lsp_position* p) { return cvint(cvget(v,"line"),&p->line) && cvint(cvget(v,"character"),&p->character); }
static int cvrange(const char* text,const xvalue* range,int* start,int* end)
{ xide_lsp_position p[2]={0}; if(!cvposition(cvget(range,"start"),p) || !cvposition(cvget(range,"end"),p+1) || !xideLspPositionsToOffsets(text,p,2) || p[0].offset>p[1].offset) return 0; *start=p[0].offset; *end=p[1].offset; return 1; }
static int cveligible(xide_app* app,xide_document* d)
{ return xideCInputNamed(d) && !strcmp(d->language,"c") && !strcmp(xuiCodeEditGetLanguage(d->editor),"c") && (!d->externalState || d->externalState==XIDE_WATCH_MISSING) && xideCCompletionViewServer(app) && !xideActionsHasModal(app); }
static int cvcurrent(xide_c_language_view* v)
{ return v && v->ready && v->inputs && xideCInputsCurrent(v->inputs) && xideCCompletionJobState(v->job,NULL,0)==XIDE_C_JOB_READY; }
static void cvcancelQuery(xide_c_language_view* v,cv_query* q)
{ if(q->token) xideCAnalysisJobCancelRequest(v->job,q->token); memset(q,0,sizeof(*q)); }
static void cvclearHover(xide_c_language_view* v)
{ if(v->hoverPopup) xuiPopupSetOpen(v->hoverPopup,0); cvcancelQuery(v,&v->hover); xrtValueRelease(v->hoverResult); v->hoverResult=NULL; v->hoverDue=0; v->hoverPointer=0; }
static void cvclearOutline(xide_c_language_view* v)
{
 cvcancelQuery(v,&v->outlineQuery); xuiTreeViewSetNodes(v->outline,NULL,0);
 for(size_t i=0;i<v->symbolCount;++i) free(v->symbols[i].title);
 free(v->symbols); v->symbols=NULL; v->symbolCount=0; free(v->outlineTitle); v->outlineTitle=NULL; xrtValueRelease(v->symbolResult); v->symbolResult=NULL;
}
static void cvclearDiagnostics(xide_c_language_view* v)
{
 if(v->inputs) for(size_t i=0;i<v->inputs->count;++i) { xide_c_input* b=&v->inputs->items[i]; if(b->document->open && b->document->id==b->id) xuiCodeAnnotationClearDiagnostics(xuiCodeEditGetAnnotations(b->document->editor)); }
 xideTaskViewClearLanguageProblemsOwned(v->app,v); for(size_t i=0;i<v->eventCount;++i) xideCEventClear(&v->diagnostics[i]); v->eventCount=0; free(v->flat); v->flat=NULL; v->diagnosticCount=0; v->sequence=0; v->received=0;
}
static void cvreap(xide_c_language_view* v)
{ for(size_t i=0;i<v->retiredCount;) { if(xideCCompletionJobDone(v->retired[i])) { xideCCompletionJobDestroy(v->retired[i]); v->retired[i]=v->retired[--v->retiredCount]; } else ++i; } }
static int cvdismiss(xide_c_language_view* v)
{
 cvclearHover(v); cvclearOutline(v); cvclearDiagnostics(v); cvcancelQuery(v,&v->definition); v->ready=0;
 if(v->job) { xideCCompletionJobCancel(v->job); cvreap(v); if(v->retiredCount>=4) return 0; v->retired[v->retiredCount++]=v->job; v->job=NULL; }
 xideCInputsDestroy(v->inputs); v->inputs=NULL; return 1;
}
static void cvwake(void* user) { (void)user; xgeRenderRequest(); }
static int cvbegin(xide_c_language_view* v)
{
 xide_model_error e={0}; if(!cveligible(v->app,v->app->activeDocument) || !cvdismiss(v)) return 0;
 v->inputs=xideCInputsCapture(v->app,v->app->activeDocument,&e);
 if(v->inputs) { xide_c_job_spec spec=xideCInputsSpec(v->inputs,v->app->activeDocument,0,cvwake); v->job=xideCAnalysisJobStart(&spec,&e); }
 v->due=0; if(!v->job) { snprintf(v->message,sizeof(v->message),"%s",*e.message ? e.message : "Cannot start C analysis."); v->paused=1; cvdismiss(v); return 0; }
 snprintf(v->message,sizeof(v->message),"C 分析正在同步打开的源码与头文件…"); return 1;
}
int xideCLanguageViewCanCheck(xide_app* app)
{ return app && app->cLanguageView && cveligible(app,app->activeDocument); }
int xideCLanguageViewCanQuery(xide_app* app,int query)
{
 xide_c_language_view* v=app ? app->cLanguageView : NULL; xide_document* d=app ? app->activeDocument : NULL;
 return v && query>=XIDE_C_DEFINITION && query<=XIDE_C_SYMBOLS && cveligible(app,d) && cvcurrent(v) && xideCInputsPath(v->inputs,d->path) && xideCAnalysisJobSupports(v->job,(enum xide_c_query)query) && (query!=XIDE_C_DEFINITION || !v->definition.token);
}
int xideCLanguageViewCanCancel(xide_app* app) { return app && app->cLanguageView && !app->cLanguageView->paused && (app->cLanguageView->job || app->cLanguageView->due); }
static int cvqueryCurrent(xide_c_language_view* v,const cv_query* q,int caret)
{
 xide_document* d=q->document; xui_code_selection_t s={0};
 return cvcurrent(v) && d && d==v->app->activeDocument && d->open && d->id==q->id && cveligible(v->app,d) && q->version==xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor)) && (!caret || (xuiGetFocusWidget(v->app->context)==d->editor && xuiCodeSelectionGetState(xuiCodeEditGetSelection(d->editor),&s)==XUI_OK && s.iCaretOffset==q->offset));
}
static int cvrequest(xide_c_language_view* v,cv_query* q,enum xide_c_query kind,int offset)
{
 if(!xideCLanguageViewCanQuery(v->app,kind)) return 0;
 cvcancelQuery(v,q); xide_document* d=v->app->activeDocument;
 *q=(cv_query){.token=xideCAnalysisJobRequest(v->job,kind,d->id,offset),.id=d->id,.version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor)),.document=d,.offset=offset}; 
 return q->token!=0;
}
static int cvselect(xide_app* app,xide_document* d,int start,int end)
{
 if(xideEditorShow(app,d)!=XUI_OK) return 0;
 xui_code_selection_t s={.iAnchorOffset=start,.iCaretOffset=end};
 return xuiCodeSelectionSetState(xuiCodeEditGetSelection(d->editor),xuiCodeEditGetDocument(d->editor),&s)==XUI_OK && xuiCodeEditEnsureCaretVisible(d->editor)==XUI_OK && xuiSetFocusWidget(app->context,d->editor)==XUI_OK;
}
static int cvproblemJump(xide_app* app,size_t index,void* user)
{ xide_c_language_view* v=user; if(!v || v->app!=app || !cvcurrent(v) || index>=v->diagnosticCount) return 0; cv_diag* d=&v->flat[index]; return cvselect(app,d->input->document,d->start,d->end); }

static int cvapplyDiagnostics(xide_c_language_view* v,xide_c_event* events,size_t count,uint64_t sequence)
{
 if(!cvcurrent(v) || !count || count>v->inputs->count) return 0;
 size_t total=0; int okay=1; xide_c_input* inputs[64]={0};
 for(size_t i=0;okay && i<count;++i) {
  for(size_t j=0;j<v->inputs->count;++j) if(v->inputs->items[j].id==events[i].documentId) inputs[i]=&v->inputs->items[j];
  const xvalue* list=cvget(cvget(events[i].message,"params"),"diagnostics");
  okay=inputs[i] && (int)inputs[i]->version==events[i].version && xrtValueType(list)==XVALUE_ARRAY && xrtValueCount(list)<=4096-total;
  for(size_t j=0;okay && j<i;++j) if(inputs[j]==inputs[i]) okay=0;
  if(okay) total+=xrtValueCount(list);
 }
 cv_diag* flat=calloc(total+1,sizeof(*flat)); char** titles=calloc(total+1,sizeof(*titles)); xui_code_diagnostic_t* marks=calloc(total+1,sizeof(*marks));
 okay=okay && flat && titles && marks; size_t at=0,begin[64]={0},sizes[64]={0};
 for(size_t i=0;okay && i<count;++i) {
  const xvalue* list=cvget(cvget(events[i].message,"params"),"diagnostics"); begin[i]=at; sizes[i]=xrtValueCount(list);
  for(size_t j=0;okay && j<sizes[i];++j) {
   const xvalue* item=xrtValueArrayGet(list,j); const char* message=cvstr(cvget(item,"message")); int severity=1,start=0,end=0;
   okay=message && *message && strlen(message)<=131072 && cvrange(inputs[i]->text,cvget(item,"range"),&start,&end) && (!cvget(item,"severity") || (cvint(cvget(item,"severity"),&severity) && severity>=1 && severity<=4));
   if(!okay) break;
   const xvalue* position=cvget(cvget(item,"range"),"start"); int line=0,character=0; okay=cvint(cvget(position,"line"),&line) && cvint(cvget(position,"character"),&character);
   size_t length=strlen(inputs[i]->path)+strlen(message)+96; titles[at]=malloc(length); if(!titles[at]) { okay=0; break; }
   snprintf(titles[at],length,"%s:%d:%d · %s",inputs[i]->path,line+1,character+1,message); flat[at]=(cv_diag){inputs[i],start,end};
   marks[at]=(xui_code_diagnostic_t){.iSize=sizeof(*marks),.tRange={start,end},.iSeverity=severity,.sMessage=message,.sSource="clangd",.iUserData=at}; ++at;
  }
 }
 if(okay) {
  cvclearDiagnostics(v); v->diagnostics[0]=(xide_c_event){0};
  for(size_t i=0;i<count;++i) { v->diagnostics[i]=events[i]; memset(&events[i],0,sizeof(events[i])); }
  v->eventCount=count; v->sequence=sequence; v->received=(int)count; v->flat=flat; flat=NULL; v->diagnosticCount=total;
  for(size_t i=0;okay && i<count;++i) okay=xuiCodeAnnotationSetDiagnostics(xuiCodeEditGetAnnotations(inputs[i]->document->editor),marks+begin[i],(int)sizes[i])==XUI_OK;
  char scope[256]; snprintf(scope,sizeof(scope),"C / clangd · 打开文档含未保存内容 · 诊断 %zu/%zu 个文档 · 输入守卫",count,v->inputs->count);
  void* owner=xideTaskViewLanguageProblemsOwner(v->app);
  if(okay && (v->activate || owner==v || (!owner && !xideTaskViewSelected(v->app) && cveligible(v->app,v->app->activeDocument)))) okay=xideTaskViewShowLanguageProblemsRouted(v->app,scope,(const char* const*)titles,total,v->activate,v,cvproblemJump)==XUI_OK;
  if(okay) { v->activate=0; snprintf(v->message,sizeof(v->message),"C 诊断：%zu 项；已接收 %zu/%zu 个文档。",total,count,v->inputs->count); }
 }
 if(!okay) cvclearDiagnostics(v);
 for(size_t i=0;i<total;++i) free(titles ? titles[i] : NULL);
 free(titles); free(marks); free(flat); return okay;
}
static int cvflatten(const xvalue* list,cv_symbol* items,xide_lsp_position* p,size_t* count,int parent,int depth)
{
 if(!list || xrtValueType(list)!=XVALUE_ARRAY || depth>64 || xrtValueCount(list)>8192-*count) return 0;
 for(size_t i=0;i<xrtValueCount(list);++i) {
  if(*count>=8192) return 0;
  size_t at=(*count)++; const xvalue* item=xrtValueArrayGet(list,i); const char *name=cvstr(cvget(item,"name")),*detail=cvstr(cvget(item,"detail")); int kind=0;
  if(!name || !*name || strlen(name)>4096 || (cvget(item,"detail") && (!detail || strlen(detail)>8192)) || !cvint(cvget(item,"kind"),&kind) || kind<1 || kind>26) return 0;
  const xvalue *range=cvget(item,"range"),*selection=cvget(item,"selectionRange");
  if(!cvposition(cvget(range,"start"),p+4*at) || !cvposition(cvget(range,"end"),p+4*at+1) || !cvposition(cvget(selection,"start"),p+4*at+2) || !cvposition(cvget(selection,"end"),p+4*at+3)) return 0;
  size_t n=strlen(name)+(detail ? strlen(detail) : 0)+8; items[at].title=malloc(n); items[at].parent=parent; if(!items[at].title) return 0;
  snprintf(items[at].title,n,"%s%s%s",name,detail && *detail ? " · " : "",detail ? detail : "");
  if(cvget(item,"children") && !cvflatten(cvget(item,"children"),items,p,count,(int)at,depth+1)) return 0;
 } return 1;
}
static int cvapplyOutline(xide_c_language_view* v,const xvalue* message)
{
 if(!cvqueryCurrent(v,&v->outlineQuery,0) || cvget(message,"error")) return 0;
 xvalue* result=xrtValueDeepClone(cvget(message,"result")); cv_symbol* items=calloc(8192,sizeof(*items)); xide_lsp_position* p=calloc(8192*4,sizeof(*p)); xui_tree_view_node_t* nodes=calloc(8193,sizeof(*nodes)); char* title=cvcopy(v->outlineQuery.document->path); size_t count=0;
 int okay=result && items && p && nodes && title && cvflatten(result,items,p,&count,-1,0) && xideLspPositionsToOffsets(xuiCodeEditGetText(v->outlineQuery.document->editor),p,count*4);
 if(nodes) nodes[0]=(xui_tree_view_node_t){.iId=0,.iParent=-1,.sText=title,.bExpanded=1,.bEnabled=0};
 for(size_t i=0;okay && i<count;++i) {
  xide_lsp_position* q=p+4*i; int parent=items[i].parent; okay=q[0].offset<=q[2].offset && q[2].offset<q[3].offset && q[3].offset<=q[1].offset;
  if(okay && parent>=0) okay=q[0].offset>=p[4*(size_t)parent].offset && q[1].offset<=p[4*(size_t)parent+1].offset;
  items[i].start=q[2].offset; items[i].end=q[3].offset; nodes[i+1]=(xui_tree_view_node_t){.iId=(int)i+1,.iParent=parent+1,.sText=items[i].title,.bExpanded=1,.bEnabled=1};
 }
 if(okay) okay=xuiTreeViewSetNodes(v->outline,nodes,(int)count+1)==XUI_OK;
 if(okay) { v->symbols=items; items=NULL; v->symbolCount=count; v->symbolResult=result; result=NULL; v->outlineTitle=title; title=NULL; }
 if(items) for(size_t i=0;i<count;++i) free(items[i].title);
 free(items); free(p); free(nodes); free(title); xrtValueRelease(result); return okay;
}
static int cvinside(xui_widget child,xui_widget parent)
{ for(;child;child=xuiWidgetGetParent(child)) if(child==parent) return 1; return 0; }
static int cvhoverCurrent(xide_c_language_view* v)
{
 if(!cvqueryCurrent(v,&v->hover,0)) return 0;
 int inPopup=v->hoverPopup && xuiPopupIsOpen(v->hoverPopup) && cvinside(xuiGetHoverWidget(v->app->context),v->hoverPopup);
 if(!v->hoverPointer) {
  xui_code_selection_t selection={0}; xui_widget focus=xuiGetFocusWidget(v->app->context);
  return (focus==v->hover.document->editor || cvinside(focus,v->hoverPopup)) && xuiCodeSelectionGetState(xuiCodeEditGetSelection(v->hover.document->editor),&selection)==XUI_OK && selection.iCaretOffset==v->hover.offset;
 }
 if(inPopup) return 1;
 xui_code_edit_text_hit_t hit={.iSize=sizeof(hit)};
 return xuiGetHoverWidget(v->app->context)==v->hover.document->editor && xuiCodeEditHitTestText(v->hover.document->editor,v->hoverX,v->hoverY,&hit)==XUI_OK && hit.bInsideText && hit.iClusterStart==v->hover.offset;
}
static int cvshowHover(xide_c_language_view* v,const char* text,int offset)
{
 xui_rect_t anchor={0}; if(xuiCodeEditGetTextOffsetRect(v->hover.document->editor,offset,&anchor)!=XUI_OK) return 0;
 int lines=1; for(const char* p=text;*p;++p) if(*p=='\n' && lines<1000) ++lines;
 float width=540.0f*xideUiFontScale(v->app),height=(float)(lines+1)*xideUiHeight(v->app,22)+16; if(height<64) height=64; if(height>280) height=280;
 xuiTextEditSetFont(v->hoverText,v->app->uiFont);
 v->hoverTheme=v->app->theme; v->hoverFont=v->app->uiFont;
 xuiTextEditSetColors(v->hoverText,ideThemeColor(v->app->theme,IDE_PANEL),ideThemeColor(v->app->theme,IDE_TEXT),ideThemeColor(v->app->theme,IDE_BORDER),ideThemeColor(v->app->theme,IDE_BORDER));
 xuiPopupSetColors(v->hoverPopup,ideThemeColor(v->app->theme,IDE_PANEL),ideThemeColor(v->app->theme,IDE_BORDER),ideThemeColor(v->app->theme,IDE_SHADOW),0);
 return xuiTextEditSetText(v->hoverText,text)==XUI_OK && xuiTextEditSetScroll(v->hoverText,0,0)==XUI_OK && xuiPopupSetOwner(v->hoverPopup,v->hover.document->editor)==XUI_OK && xuiPopupSetAnchorRect(v->hoverPopup,anchor)==XUI_OK && xuiPopupSetContentSize(v->hoverPopup,width,height)==XUI_OK && xuiPopupSetOpen(v->hoverPopup,1)==XUI_OK;
}
static int cvapplyHover(xide_c_language_view* v,const xvalue* message)
{
 if(!cvhoverCurrent(v) || xideCCompletionViewActive(v->app) || cvget(message,"error")) return 0;
 const xvalue* result=cvget(message,"result"); if(!result || xrtValueType(result)==XVALUE_NULL) return 0;
 const xvalue* contents=cvget(result,"contents"); const char *kind=cvstr(cvget(contents,"kind")),*text=cvstr(cvget(contents,"value")); int start=0,end=0;
 /* This client negotiates plaintext; never display Markdown as plain content. */
 if(!kind || strcmp(kind,"plaintext") || !text || !*text || strlen(text)>12499 || !cvrange(xuiCodeEditGetText(v->hover.document->editor),cvget(result,"range"),&start,&end) || start>v->hover.offset || v->hover.offset>=end) return 0;
 xvalue* owned=xrtValueDeepClone(result); if(!owned) return 0;
 if(!cvshowHover(v,cvstr(cvget(cvget(owned,"contents"),"value")),start)) { xrtValueRelease(owned); return 0; }
 xrtValueRelease(v->hoverResult); v->hoverResult=owned; return 1;
}

static int cvsourceHash(const xvalue* target,const unsigned char* bytes,size_t size)
{
 const char* expected=cvstr(cvget(target,"textSha256"));int length=0;unsigned char digest[32];char hex[65];
 size_t bom=size>=3 && bytes[0]==0xef && bytes[1]==0xbb && bytes[2]==0xbf ? 3 : 0;
 if(!expected || strlen(expected)!=64 || !cvint(cvget(target,"textSize"),&length) || size-bom!=(size_t)length || !xrtSha256(bytes+bom,size-bom,digest)) return 0;
 for(int i=0;i<32;++i) snprintf(hex+2*i,3,"%02x",digest[i]);
 return !strcmp(expected,hex);
}
static int cvapplySourceDefinition(xide_c_language_view* v,const xvalue* source)
{
 if(!cvqueryCurrent(v,&v->definition,1)) return 0;
 const char* path=cvstr(cvget(source,"path"));int version=0,start=0,end=0,outerStart=0,outerEnd=0;
 if(!path || strlen(path)>32767 || !xidePathHasExtension(path,".xl") || !cvint(cvget(source,"version"),&version)) return 0;
 xide_c_input* input=xideCInputsXlangPath(v->inputs,path);xide_document* target=input ? input->document : NULL;
 xide_text_file disk={0};xide_file_snapshot expected={0},before={0},after={0};xide_model_error error={0};const char* text=input ? input->text : NULL;int okay=1;
 if(input) {
  okay=version>0 && input->version==(uint32_t)version && target->open && target->id==input->id && !strcmp(target->language,"xlang") && !strcmp(xuiCodeEditGetLanguage(target->editor),"xlang") && cvsourceHash(source,(const unsigned char*)text,strlen(text));
 } else {
  /* An existing unbound/open document must never be replaced by disk text. */
  for(size_t i=0;i<v->app->documents.count;++i) { xide_document* d=v->app->documents.items[i];if(d->open && d->path && xidePathLexicalEqual(d->path,path)) okay=0; }
  okay=okay && !version && xideCAnalysisJobGuard(v->job,path,&expected) && expected.length<=8u*1024u*1024u && xideFileSnapshotInspect(path,&before,&error) && xideFileSnapshotEqual(&expected,&before) && xideTextFileRead(path,&disk,&error) && xideFileSnapshotInspect(path,&after,&error) && xideFileSnapshotEqual(&expected,&after) && cvsourceHash(source,disk.raw,disk.rawLength);
  text=disk.text;
 }
 okay=okay && text && cvrange(text,cvget(source,"selectionRange"),&start,&end) && start<end && cvrange(text,cvget(source,"range"),&outerStart,&outerEnd) && outerStart<=start && end<=outerEnd && cvqueryCurrent(v,&v->definition,1);
 if(okay && !input) {
  okay=xideEditorOpen(v->app,path,&error);target=v->app->activeDocument;
  if(okay) okay=target && !strcmp(target->language,"xlang") && !strcmp(xuiCodeEditGetLanguage(target->editor),"xlang") && target->baseline.rawLength==disk.rawLength && !memcmp(target->baseline.raw,disk.raw,disk.rawLength) && !strcmp(xuiCodeEditGetText(target->editor),text) && xideFileSnapshotInspect(path,&after,&error) && xideFileSnapshotEqual(&expected,&after);
 }
 if(okay) okay=cvselect(v->app,target,start,end);
 else snprintf(v->message,sizeof(v->message),"xlang 定义来源的文档版本、正文或磁盘身份已变化；已拒绝跳转。");
 xideTextFileClear(&disk);return okay;
}
static int cvapplyDefinition(xide_c_language_view* v,const xvalue* message)
{
 if(!cvqueryCurrent(v,&v->definition,1) || cvget(message,"error")) return 0;
 const xvalue* result=cvget(message,"result"); if(!result || xrtValueType(result)==XVALUE_NULL) return 0;
 const xvalue* item=result; if(xrtValueType(result)==XVALUE_ARRAY) { if(!xrtValueCount(result)) return 0; item=xrtValueArrayGet(result,0); }
 const char* uri=cvstr(cvget(item,"uri")); const xvalue* range=cvget(item,"range"); int link=0;
 if(!uri) { uri=cvstr(cvget(item,"targetUri")); range=cvget(item,"targetSelectionRange"); link=1; }
 char* path=uri ? xideLspUriToPath(uri,NULL) : NULL; if(!path) return 0;
 xide_c_input* input=xideCInputsPath(v->inputs,path); xide_document* target=input ? input->document : NULL; xide_text_file disk={0}; xide_file_snapshot expected={0},before={0},after={0}; xide_model_error e={0};
 const char* text=input ? input->text : NULL; int start=0,end=0,okay=1;
 if(!input) {
  okay=xideCAnalysisJobGuard(v->job,path,&expected) && expected.length<=8u*1024u*1024u && xideFileSnapshotInspect(path,&before,&e) && xideFileSnapshotEqual(&expected,&before) && xideTextFileRead(path,&disk,&e) && xideFileSnapshotInspect(path,&after,&e) && xideFileSnapshotEqual(&expected,&after); text=disk.text;
  if(!okay) snprintf(v->message,sizeof(v->message),"C 定义目标未被本会话观察，或磁盘内容/身份已改变；已拒绝跳转。");
 }
 okay=okay && text && cvrange(text,range,&start,&end);
 if(okay && link) { int outerStart=0,outerEnd=0; okay=cvrange(text,cvget(item,"targetRange"),&outerStart,&outerEnd) && outerStart<=start && end<=outerEnd; }
 xvalue* context=NULL;
 okay=okay && cvqueryCurrent(v,&v->definition,1) && xideCInputsPrepareNavigation(v->inputs,v->definition.document,path,&context);
 if(okay && !input) {
  /* User-invoked navigation checks at most 8 MiB on this thread, then compares
     the actual open baseline; ordinary guard polling stays in the worker. */
  okay=xideEditorOpen(v->app,path,&e); target=v->app->activeDocument;
  if(okay) okay=target && target->baseline.rawLength==disk.rawLength && !memcmp(target->baseline.raw,disk.raw,disk.rawLength) && !strcmp(xuiCodeEditGetText(target->editor),text) && xideFileSnapshotInspect(path,&after,&e) && xideFileSnapshotEqual(&expected,&after);
 }
 if(okay) okay=cvselect(v->app,target,start,end);
 if(okay) { xrtValueRelease(target->cAnalysisContext); target->cAnalysisContext=context; context=NULL; }
 xrtValueRelease(context); xideTextFileClear(&disk); free(path); return okay;
}
static int cvoutlineEvent(xui_widget widget,const xui_event_t* event,void* user)
{
 xide_c_language_view* v=((xide_app*)user)->cLanguageView;
 if((event->iType==XUI_EVENT_POINTER_DOUBLE_CLICK || (event->iType==XUI_EVENT_KEY_DOWN && event->iKey==XUI_KEY_ENTER)) && v && cvqueryCurrent(v,&v->outlineQuery,0)) {
  int id=xuiTreeViewGetSelected(widget); if(id>0 && (size_t)id<=v->symbolCount && cvselect(v->app,v->outlineQuery.document,v->symbols[id-1].start,v->symbols[id-1].end)) return XUI_EVENT_DISPATCH_STOP;
 } return XUI_OK;
}
static int cvuseOutline(xide_c_language_view* v,int c)
{
 xui_widget wanted=c ? v->outline : v->xlangOutline,other=c ? v->xlangOutline : v->outline;
 if(xuiDockPanelGetWindowClient(v->app->dock,v->app->windows[XIDE_OUTLINE])!=wanted) {
  if(xuiDockPanelSetWindowClient(v->app->dock,v->app->windows[XIDE_OUTLINE],wanted)!=XUI_OK) return 0;
  if(xuiWidgetAddChild(v->app->root,other)!=XUI_OK || xuiWidgetSetVisible(other,0)!=XUI_OK) return 0;
  v->app->panels[XIDE_OUTLINE]=wanted;
 }
 return xuiWidgetSetVisible(wanted,1)==XUI_OK;
}
static int cvshowOutline(xide_c_language_view* v)
{
 if(!cvuseOutline(v,1)) return 0;
 xide_app* app=v->app; xui_dock_window_info_t info={.iSize=sizeof(info)};
 if(xuiDockPanelGetWindowInfo(app->dock,app->windows[XIDE_OUTLINE],&info)!=XUI_OK) return 0;
 if(info.iState==XUI_DOCK_PANEL_WINDOW_FLOATING) return xuiSetFocusWidget(app->context,v->outline)==XUI_OK;
 int pane=info.iState==XUI_DOCK_PANEL_WINDOW_DOCKED ? info.iPane : -1;
 for(int i=XIDE_HELP;i<=XIDE_USAGES && pane<0;++i) { xui_dock_window_info_t b={.iSize=sizeof(b)}; if(xuiDockPanelGetWindowInfo(app->dock,app->windows[i],&b)==XUI_OK && b.iState==XUI_DOCK_PANEL_WINDOW_DOCKED) pane=b.iPane; }
 if(pane<0) { int editor=app->activeDocument ? app->activeDocument->window : app->windows[XIDE_XLANG_DEMO]; if(xuiDockPanelGetWindowInfo(app->dock,editor,&info)!=XUI_OK || info.iState!=XUI_DOCK_PANEL_WINDOW_DOCKED || xuiDockPanelDockWindowToPaneSide(app->dock,app->windows[XIDE_OUTLINE],info.iPane,XUI_DOCK_PANEL_SIDE_BOTTOM,0.28f,&pane)!=XUI_OK) return 0; }
 else if(xuiDockPanelDockWindowToPane(app->dock,app->windows[XIDE_OUTLINE],pane)!=XUI_OK) return 0;
 return xuiDockPanelSetPaneActiveWindow(app->dock,pane,app->windows[XIDE_OUTLINE])==XUI_OK;
}
int xideCLanguageViewAction(xide_app* app,int action)
{
 xide_c_language_view* v=app ? app->cLanguageView : NULL; if(!v || v->stopped) return 0;
 if(action==XIDE_ACTION_CANCEL_CHECK) { cvdismiss(v); v->paused=1; v->due=0; snprintf(v->message,sizeof(v->message),"C 分析已取消；检查当前文档可重新启动。"); return 1; }
 if(action==XIDE_ACTION_CHECK_CURRENT && xideCLanguageViewCanCheck(app)) { v->paused=0; v->activate=1; return cvbegin(v); }
 if(action==XIDE_ACTION_OUTLINE && xideCInputNamed(app->activeDocument) && !strcmp(app->activeDocument->language,"c")) return cvshowOutline(v);
 cv_query* q=action==XIDE_ACTION_DEFINITION ? &v->definition : action==XIDE_ACTION_HOVER ? &v->hover : NULL;
 if(q) { xui_code_selection_t s={0}; if(!app->activeDocument || xuiCodeSelectionGetState(xuiCodeEditGetSelection(app->activeDocument->editor),&s)!=XUI_OK) return 0; if(q==&v->hover) cvclearHover(v); return cvrequest(v,q,q==&v->definition ? XIDE_C_DEFINITION : XIDE_C_HOVER,s.iCaretOffset); }
 return 0;
}
static const int cvEvents[]={XUI_EVENT_POINTER_MOVE,XUI_EVENT_POINTER_LEAVE,XUI_EVENT_POINTER_DOWN,XUI_EVENT_POINTER_WHEEL,XUI_EVENT_BLUR,XUI_EVENT_KEY_DOWN,XUI_EVENT_TEXT,XUI_EVENT_IME_COMPOSITION};
static int cvhoverEvent(xui_widget widget,const xui_event_t* event,void* user)
{
 cv_binding* b=user; xide_c_language_view* v=b->app->cLanguageView; int index=-1;
 for(int i=0;i<8;++i) if(cvEvents[i]==event->iType) index=i;
 if(event->iType!=XUI_EVENT_POINTER_MOVE && !(event->iType==XUI_EVENT_POINTER_LEAVE && xuiPopupIsOpen(v->hoverPopup) && cvinside(xuiGetHoverWidget(b->app->context),v->hoverPopup)) && !(event->iType==XUI_EVENT_BLUR && cvinside(xuiGetFocusWidget(b->app->context),v->hoverPopup))) {
 cvclearHover(v); if(event->iType==XUI_EVENT_BLUR || event->iType==XUI_EVENT_KEY_DOWN || event->iType==XUI_EVENT_TEXT || event->iType==XUI_EVENT_IME_COMPOSITION) cvcancelQuery(v,&v->definition); }
 int result=index>=0 && b->handlers[index] ? b->handlers[index](widget,event,b->users[index]) : XUI_OK;
 if(event->iType!=XUI_EVENT_POINTER_MOVE) return result;
 if(event->iButtons || !xideCLanguageViewCanQuery(b->app,XIDE_C_HOVER) || b->app->activeDocument->editor!=widget || xideCCompletionViewActive(b->app)) { cvclearHover(v); return result; }
 xui_code_edit_text_hit_t hit={.iSize=sizeof(hit)};
 if(xuiCodeEditHitTestText(widget,(float)event->fX,(float)event->fY,&hit)!=XUI_OK || !hit.bInsideText || hit.iClusterStart==hit.iClusterEnd) { cvclearHover(v); return result; }
 if(v->hover.document!=b->app->activeDocument || !v->hoverPointer || v->hover.offset!=hit.iClusterStart) {
  cvclearHover(v); xide_document* d=b->app->activeDocument; v->hover=(cv_query){.document=d,.id=d->id,.version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor)),.offset=hit.iClusterStart}; v->hoverPointer=1; v->hoverDue=GetTickCount64()+350; xgeRenderRequest();
 } v->hoverX=event->fX; v->hoverY=event->fY; return result;
}
int xideCLanguageViewBindEditor(xide_app* app,xui_widget editor)
{
 xide_c_language_view* v=app ? app->cLanguageView : NULL; if(!v || !editor) return XUI_ERROR_INVALID_ARGUMENT;
 for(cv_binding* b=v->bindings;b;b=b->next) if(b->editor==editor) return XUI_OK;
 cv_binding* b=calloc(1,sizeof(*b)); if(!b) return XUI_ERROR_OUT_OF_MEMORY; b->app=app; b->editor=editor; int r=XUI_OK,installed=0;
 for(int i=0;i<8 && r==XUI_OK;++i) r=xuiWidgetGetEventHandler(editor,cvEvents[i],&b->handlers[i],&b->users[i]);
 for(int i=0;i<8 && r==XUI_OK;++i) { r=xuiWidgetSetEventHandler(editor,cvEvents[i],cvhoverEvent,b); if(r==XUI_OK) ++installed; }
 if(r!=XUI_OK) { for(int i=0;i<installed;++i) xuiWidgetSetEventHandler(editor,cvEvents[i],b->handlers[i],b->users[i]); free(b); return r; }
 b->next=v->bindings; v->bindings=b; return XUI_OK;
}

int xideCLanguageViewCreate(xide_app* app)
{
 xide_c_language_view* v=calloc(1,sizeof(*v)); if(!v) return XUI_ERROR_OUT_OF_MEMORY; app->cLanguageView=v; v->app=app; v->xlangOutline=app->panels[XIDE_OUTLINE];
 xui_tree_view_desc_t tree={.iSize=sizeof(tree),.pFont=app->uiFont,.fItemHeight=26,.fPadding=6,.fIndent=12};
 int r=xuiTreeViewCreate(app->context,&v->outline,&tree);
 if(r==XUI_OK) r=xuiWidgetAddChild(app->root,v->outline);
 if(r==XUI_OK) r=xuiWidgetSetVisible(v->outline,0);
 if(r==XUI_OK) r=xuiWidgetSetEventHandler(v->outline,XUI_EVENT_POINTER_DOUBLE_CLICK,cvoutlineEvent,app);
 if(r==XUI_OK) r=xuiWidgetSetEventHandler(v->outline,XUI_EVENT_KEY_DOWN,cvoutlineEvent,app);
 xui_popup_desc_t popup={.iSize=sizeof(popup),.fContentWidth=540,.fContentHeight=150,.fMaxWidth=640,.fMaxHeight=320,.fPadding=4,.fBorderWidth=1,.fMargin=8,.iAnchor=XUI_POPUP_ANCHOR_BOTTOM_LEFT,.iDirection=XUI_POPUP_DIRECTION_RIGHT_DOWN,.iOutsidePolicy=XUI_POPUP_OUTSIDE_IGNORE,.iOwnerPolicy=XUI_POPUP_OWNER_PASSTHROUGH,.iEscapePolicy=XUI_POPUP_ESCAPE_CLOSE,.iFocusPolicy=XUI_POPUP_FOCUS_NONE};
 if(r==XUI_OK) r=xuiPopupCreate(app->context,&v->hoverPopup,&popup);
 if(r==XUI_OK) r=xuiWidgetAddChild(app->root,v->hoverPopup);
 if(r==XUI_OK) r=xuiWidgetSetLayoutType(xuiPopupGetContentWidget(v->hoverPopup),XUI_LAYOUT_OVERLAY);
 xui_text_edit_desc_t text={.iSize=sizeof(text),.pFont=app->uiFont,.sText="",.bReadonly=1,.bWordWrap=1,.iMaxLength=12500};
 if(r==XUI_OK) r=xuiTextEditCreate(app->context,&v->hoverText,&text);
 if(r==XUI_OK) r=xuiWidgetSetSizeMode(v->hoverText,XUI_SIZE_FILL,XUI_SIZE_FILL);
 if(r==XUI_OK) r=xuiWidgetAddChild(xuiPopupGetContentWidget(v->hoverPopup),v->hoverText);
 if(r==XUI_OK) r=xuiWidgetSetAccessibleName(v->hoverText,"C 符号信息，只读，可换行滚动");
 return r;
}
void xideCLanguageViewStop(xide_app* app)
{
 xide_c_language_view* v=app ? app->cLanguageView : NULL; if(!v || v->stopped) return;
 cvdismiss(v); if(v->job) { xideCCompletionJobDestroy(v->job); v->job=NULL; }
 for(size_t i=0;i<v->retiredCount;++i) xideCCompletionJobDestroy(v->retired[i]);
 v->retiredCount=0;
 xideCInputsDestroy(v->inputs); v->inputs=NULL; v->stopped=1;
}
void xideCLanguageViewDestroy(xide_app* app)
{
 xide_c_language_view* v=app ? app->cLanguageView : NULL; if(!v) return;
 /* Stop runs while widgets are alive; destruction owns only plain allocations. */
 while(v->bindings) { cv_binding* next=v->bindings->next; free(v->bindings); v->bindings=next; }
 free(v); app->cLanguageView=NULL;
}
size_t xideCLanguageViewDiagnosticCount(xide_app* app) { return app && app->cLanguageView ? app->cLanguageView->diagnosticCount : 0; }
const char* xideCLanguageViewMessage(xide_app* app) { return app && app->cLanguageView ? app->cLanguageView->message : ""; }
void xideCLanguageViewPoll(xide_app* app)
{
 xide_c_language_view* v=app ? app->cLanguageView : NULL; if(!v || v->stopped) return; cvreap(v);
 xide_document* active=app->activeDocument; int c=xideCInputNamed(active) && !strcmp(active->language,"c"); cvuseOutline(v,c);
 if(v->inputs && !xideCInputsCurrent(v->inputs)) { cvdismiss(v); v->due=v->paused ? 0 : GetTickCount64()+500; snprintf(v->message,sizeof(v->message),"C 输入已变化；旧诊断和语义结果已撤下。"); }
 if(v->job) {
  char workerMessage[768]={0}; enum xide_c_job_state state=xideCCompletionJobState(v->job,workerMessage,sizeof(workerMessage));
  if(state>=XIDE_C_JOB_FAILED) { snprintf(v->message,sizeof(v->message),"%s",workerMessage); int retry=state==XIDE_C_JOB_INVALIDATED; cvdismiss(v); v->paused=!retry; unsigned delay=1500u << (v->retryCount<3 ? v->retryCount : 3); if(delay>10000) delay=10000; if(retry && v->retryCount<3) ++v->retryCount; v->due=retry ? GetTickCount64()+delay : 0; }
  else if(state==XIDE_C_JOB_READY) { if(!v->ready && *workerMessage) snprintf(v->message,sizeof(v->message),"%s",workerMessage); v->ready=1; v->retryCount=0; }
 }
 if(!v->paused && !v->job && cveligible(app,active)) {
  if(!v->due) v->due=GetTickCount64()+500;
  if(GetTickCount64()>=v->due && v->retiredCount<4) cvbegin(v);
 }
 if(!cvcurrent(v)) return;
 uint64_t seq=v->sequence; xide_c_event events[64]={0}; size_t count=0;
 if(xideCAnalysisJobDiagnostics(v->job,&seq,events,64,&count)) {
  if(!cvapplyDiagnostics(v,events,count,seq)) { snprintf(v->message,sizeof(v->message),"C 诊断无法验证，已撤下本次诊断。"); }
  for(size_t i=0;i<count;++i) xideCEventClear(&events[i]);
 }
 if(v->outlineQuery.document && !cvqueryCurrent(v,&v->outlineQuery,0)) cvclearOutline(v);
 if(c && !v->outlineQuery.document && xideCLanguageViewCanQuery(app,XIDE_C_SYMBOLS)) cvrequest(v,&v->outlineQuery,XIDE_C_SYMBOLS,0);
 if(v->hover.document && (!cvhoverCurrent(v) || xideCCompletionViewActive(app))) cvclearHover(v);
 if(v->hoverResult && xuiPopupIsOpen(v->hoverPopup) && (v->hoverTheme!=app->theme || v->hoverFont!=app->uiFont)) cvshowHover(v,cvstr(cvget(cvget(v->hoverResult,"contents"),"value")),v->hover.offset);
 if(v->hoverDue && GetTickCount64()>=v->hoverDue) { int offset=v->hover.offset; v->hoverDue=0; cvrequest(v,&v->hover,XIDE_C_HOVER,offset); }
 if(v->definition.token && !cvqueryCurrent(v,&v->definition,1)) cvcancelQuery(v,&v->definition);
 xide_c_event event={0};
 if(v->outlineQuery.token && xideCAnalysisJobTake(v->job,v->outlineQuery.token,&event)) { int okay=cvapplyOutline(v,event.message); v->outlineQuery.token=0; if(!okay) { cvclearOutline(v); snprintf(v->message,sizeof(v->message),"C 大纲无法验证。"); } xideCEventClear(&event); }
 if(v->hover.token && xideCAnalysisJobTake(v->job,v->hover.token,&event)) { int okay=cvapplyHover(v,event.message); v->hover.token=0; if(!okay) cvclearHover(v); xideCEventClear(&event); }
 if(v->definition.token && xideCAnalysisJobTake(v->job,v->definition.token,&event)) { if(event.sourceDefinition) cvapplySourceDefinition(v,event.sourceDefinition); else cvapplyDefinition(v,event.message); memset(&v->definition,0,sizeof(v->definition)); xideCEventClear(&event); }
}

int xideCLanguageViewHintOpen(xide_app* app) { return app && app->cLanguageView && xuiPopupIsOpen(app->cLanguageView->hoverPopup); }
