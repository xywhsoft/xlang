#include "ui/goto_view.h"
#include "ui/editor_view.h"
#include "services/ui_font.h"
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#define TRY(call) do { int result_=(call); if(result_!=XUI_OK) return result_; } while(0)
struct xide_goto_view {
 xui_widget window,content,message,detail,controls[XIDE_GOTO_CONTROL_COUNT];
 xide_ui_layout layout; float scale; uint64_t document; uint32_t version;
 xui_widget_event_proc inputKey,inputIme; void *inputKeyUser,*inputImeUser; int composing; char messageText[512];
};
static xide_document* current(xide_app* a)
{
 for(size_t i=0;i<a->documents.count;++i) { xide_document* d=a->documents.items[i];if(d->open && d->id==a->gotoView->document) return d; } return NULL;
}
static int fail(xide_app* a,xide_model_error* e,const char* text)
{
 snprintf(a->gotoView->messageText,sizeof(a->gotoView->messageText),"%s",text);xuiLabelSetText(a->gotoView->message,a->gotoView->messageText);
 if(e) { e->code=XIDE_MODEL_INVALID;snprintf(e->message,sizeof(e->message),"%s",text); } xgeRenderRequest();return 0;
}
static const char* spaces(const char* p) { while(*p==' ' || *p=='\t') ++p;return p; }
static int number(const char** cursor,int* value)
{
 const char* p=spaces(*cursor);int n=0;if(*p<'0' || *p>'9') return 0;
 do { int digit=*p-'0';if(n>(INT_MAX-digit)/10) return 0;n=n*10+digit;++p; } while(*p>='0' && *p<='9');
 if(!n) return 0;
 *cursor=spaces(p);*value=n;return 1;
}
/* Expand only ranges containing the requested line; leave other folds intact.
   Prepare all ranges before changing selection, and preserve the real SDK's
   flags, levels and primary document version. */
static int reveal(xui_widget editor,int line)
{
 xui_code_fold_state folds=xuiCodeEditGetFoldState(editor);int count=xuiCodeFoldStateGetCount(folds),needed=0;
 if(count<0) return XUI_ERROR;
 for(int i=0;i<count;++i) { xui_code_fold_range_t r={0};int ret=xuiCodeFoldStateGetRange(folds,i,&r);if(ret!=XUI_OK) return ret;if(r.iStartLine<line && r.iEndLine>=line && (r.iFlags&XUI_CODE_FOLD_COLLAPSED)) { needed=1;break; } }
 if(!needed) return XUI_OK;
 xui_code_fold_range_t* ranges=calloc((size_t)count,sizeof(*ranges));if(!ranges) return XUI_ERROR_OUT_OF_MEMORY;
 int got=0,ret=xuiCodeFoldStateGetRanges(folds,ranges,count,&got);
 if(ret==XUI_OK && got==count) {
  for(int i=0;i<count;++i) if(ranges[i].iStartLine<line && ranges[i].iEndLine>=line) ranges[i].iFlags&=~XUI_CODE_FOLD_COLLAPSED;
  ret=xuiCodeFoldStateSetRanges(folds,ranges,count);
 } else if(ret==XUI_OK) ret=XUI_ERROR;
 free(ranges);return ret;
}
static void readingMargin(xui_widget editor,int offset)
{
 xui_rect_t caret={0};float x=0,y=0;
 if(xuiCodeEditGetTextOffsetRect(editor,offset,&caret)!=XUI_OK || xuiCodeEditGetScroll(editor,&x,&y)!=XUI_OK) return;
 xui_rect_t viewport=xuiScrollModelGetViewport(xuiCodeEditGetScrollModel(editor)),world=xuiWidgetGetWorldRect(editor);
 /* The public viewport excludes scrollbars. Leave one row of context where
    possible; SDK scrolling clamps the beginning/end of the real document. */
 if(caret.fH>0 && viewport.fH>3*caret.fH) {
  float top=world.fY+viewport.fY+caret.fH,bottom=world.fY+viewport.fY+viewport.fH-caret.fH,shift=0;
  if(caret.fY<top) shift=caret.fY-top;
  else if(caret.fY+caret.fH>bottom) shift=caret.fY+caret.fH-bottom;
  if(shift) xuiCodeEditSetScroll(editor,x,y+shift);
 }
}
int xideGotoViewApply(xide_app* a,xide_model_error* e)
{
 if(!a || !a->gotoView || !xideGotoViewIsOpen(a)) return 0;
 xide_document* d=current(a);
 if(!d || d!=a->activeDocument || a->gotoView->version!=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor))) return fail(a,e,"文档已改变，请取消后重新打开跳转窗口。");
 const char* p=xuiEditGetText(a->gotoView->controls[XIDE_GOTO_POSITION]);int line=0,column=1;
 if(!p || !number(&p,&line) || (*p==':' && (++p,!number(&p,&column))) || *p) return fail(a,e,"请输入正整数行号，或“行:列”，例如 12:3。");
 xui_code_document doc=xuiCodeEditGetDocument(d->editor);int lines=xuiCodeDocumentGetLineCount(doc);
 if(line>lines) { char text[128];snprintf(text,sizeof(text),"行号应为 1 到 %d。",lines);return fail(a,e,text); }
 int start=0,end=0,maxColumn=0,offset=0;
 if(xuiCodeDocumentGetLineRange(doc,line-1,&start,&end)!=XUI_OK || xuiCodeDocumentOffsetToLineColumn(doc,end,NULL,&maxColumn)!=XUI_OK) return fail(a,e,"无法读取目标行，请取消后重新打开。");
 if(column-1>maxColumn) { char text[128];snprintf(text,sizeof(text),"第 %d 行的列号应为 1 到 %d（编辑器字符；含行尾）。",line,maxColumn+1);return fail(a,e,text); }
 if(xuiCodeDocumentLineColumnToOffset(doc,line-1,column-1,&offset)!=XUI_OK || reveal(d->editor,line-1)!=XUI_OK) return fail(a,e,"无法展开或定位目标行。");
 xuiCodeSelectionClear(xuiCodeEditGetSelection(d->editor));
 if(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(d->editor),doc,offset,0)!=XUI_OK) return fail(a,e,"无法移动文档光标。");
 xideDialogSetOpen(a->gotoView->window,0);xuiSetFocusWidget(a->context,d->editor);xuiCodeEditEnsureCaretVisible(d->editor);readingMargin(d->editor,offset);xideEditorPoll(a);xideActionsRefresh(a);xgeRenderRequest();return 1;
}
static void clicked(xui_widget widget,void* user)
{
 xide_app* a=user;xide_model_error e={0};if(widget==a->gotoView->controls[XIDE_GOTO_APPLY]) xideGotoViewApply(a,&e);
 else { xideDialogSetOpen(a->gotoView->window,0);xideActionsRefresh(a); } xgeRenderRequest();
}
static int inputKey(xui_widget widget,const xui_event_t* event,void* user)
{
 xide_app* a=user;xide_goto_view* v=a->gotoView;
 if(event->iPhase==XUI_EVENT_PHASE_TARGET && event->iKey==XUI_KEY_ENTER && !event->iModifiers && !v->composing) { xide_model_error e={0};xideGotoViewApply(a,&e);return XUI_EVENT_DISPATCH_STOP; }
 return v->inputKey ? v->inputKey(widget,event,v->inputKeyUser) : XUI_OK;
}
static int editEvent(xui_widget widget,const xui_edit_event_t* event,void* user)
{
 xide_app* a=user;if(event->iType==XUI_EDIT_EVENT_COMPOSITION_CHANGED && !event->iTextSize && !event->iCompositionStart && !event->iCompositionEnd) a->gotoView->composing=0;return XUI_OK;
}
static int inputIme(xui_widget widget,const xui_event_t* event,void* user)
{
 xide_app* a=user;xide_goto_view* v=a->gotoView;int ret=v->inputIme ? v->inputIme(widget,event,v->inputImeUser) : XUI_OK;
 /* Empty active compositions are valid; the raw event carries this distinction.
    Native edit notifications additionally clear state on blur/text commit. */
 if(event->iPhase==XUI_EVENT_PHASE_TARGET) v->composing=event->bCompositionActive;
 return ret;
}
static int label(xide_app* a,const char* text,xui_rect_t rect,xui_widget* out)
{
 xui_label_desc_t desc={0};desc.iSize=sizeof(desc);desc.pFont=a->uiFont;desc.sText=text;desc.iWrapMode=XUI_TEXT_WRAP_WORD;xui_widget w=NULL;
 TRY(xuiLabelCreate(a->context,&w,&desc));TRY(xuiWidgetSetRect(w,rect));TRY(xuiWidgetAddChild(a->gotoView->content,w));if(out) *out=w;return XUI_OK;
}
static void windowClosed(xui_widget widget,void* user) { xide_app* a=user; clicked(a->gotoView->controls[XIDE_GOTO_CANCEL],a); }
int xideGotoViewCreate(xide_app* a)
{
 xide_goto_view* v=calloc(1,sizeof(*v));if(!v) return XUI_ERROR_OUT_OF_MEMORY;a->gotoView=v;
 TRY(xideDialogCreate(a,&v->window,"跳转到行 / 列",480,226,14)); TRY(xideDialogSetCancel(v->window,windowClosed,a));v->content=xideDialogGetContentWidget(v->window);TRY(xuiWidgetAddChild(a->root,v->window));TRY(xuiWidgetSetLayoutType(v->content,XUI_LAYOUT_MANUAL));
 TRY(label(a,"跳转到行 / 列",(xui_rect_t){0,0,480,28},NULL));TRY(label(a,"",(xui_rect_t){0,38,480,48},&v->detail));
 xui_input_desc_t input={0};input.iSize=sizeof(input);input.pFont=a->uiFont;TRY(xuiInputCreate(a->context,&v->controls[XIDE_GOTO_POSITION],&input));TRY(xuiWidgetSetRect(v->controls[XIDE_GOTO_POSITION],(xui_rect_t){0,94,480,32}));TRY(xuiWidgetSetAccessibleName(v->controls[XIDE_GOTO_POSITION],"行号或行:列"));TRY(xuiWidgetAddChild(v->content,v->controls[XIDE_GOTO_POSITION]));
 TRY(xuiWidgetGetEventHandler(v->controls[XIDE_GOTO_POSITION],XUI_EVENT_KEY_DOWN,&v->inputKey,&v->inputKeyUser));TRY(xuiWidgetSetEventHandler(v->controls[XIDE_GOTO_POSITION],XUI_EVENT_KEY_DOWN,inputKey,a));
 TRY(xuiWidgetGetEventHandler(v->controls[XIDE_GOTO_POSITION],XUI_EVENT_IME_COMPOSITION,&v->inputIme,&v->inputImeUser));TRY(xuiWidgetSetEventHandler(v->controls[XIDE_GOTO_POSITION],XUI_EVENT_IME_COMPOSITION,inputIme,a));
 TRY(xuiEditSetEvent(v->controls[XIDE_GOTO_POSITION],editEvent,a));
 TRY(label(a,"",(xui_rect_t){0,136,480,44},&v->message));
 for(int i=XIDE_GOTO_APPLY;i<=XIDE_GOTO_CANCEL;++i) {
  xui_button_desc_t button={0};button.iSize=sizeof(button);button.pFont=a->uiFont;button.sText=i==XIDE_GOTO_APPLY ? "跳转" : "取消";TRY(xuiButtonCreate(a->context,&v->controls[i],&button));TRY(xuiWidgetSetRect(v->controls[i],(xui_rect_t){i==XIDE_GOTO_APPLY ? 264 : 378,192,102,32}));TRY(xuiButtonSetClick(v->controls[i],clicked,a));TRY(xuiWidgetAddChild(v->content,v->controls[i]));
 } return xideUiLayoutCapture(&v->layout,v->content);
}
int xideGotoViewOpen(xide_app* a)
{
 if(!a || !a->gotoView || !a->activeDocument || !a->activeDocument->open || xideActionsHasModal(a)) return 0;
 xide_goto_view* v=a->gotoView;xide_document* d=a->activeDocument;xui_code_document doc=xuiCodeEditGetDocument(d->editor);xui_code_selection_t selection={0};int line=0,column=0;
 if(xuiCodeSelectionGetState(xuiCodeEditGetSelection(d->editor),&selection)!=XUI_OK || xuiCodeDocumentOffsetToLineColumn(doc,selection.iCaretOffset,&line,&column)!=XUI_OK) return 0;
 v->document=d->id;v->version=xuiCodeDocumentGetVersion(doc);v->messageText[0]=0;v->composing=0;char position[64],detail[256];snprintf(position,sizeof(position),"%d:%d",line+1,column+1);snprintf(detail,sizeof(detail),"当前 %d:%d，共 %d 行。输入行号或行:列；列与状态栏一致，组合字符合为一列，Tab 占一列。",line+1,column+1,xuiCodeDocumentGetLineCount(doc));
 xuiLabelSetText(v->detail,detail);xuiLabelSetText(v->message,"");xuiEditSetText(v->controls[XIDE_GOTO_POSITION],position);xideGotoViewLayout(a);xideDialogSetScroll(v->window,0,0);xideDialogSetOpen(v->window,1);xuiSetFocusWidget(a->context,v->controls[XIDE_GOTO_POSITION]);xuiEditSelectAll(v->controls[XIDE_GOTO_POSITION]);xideActionsRefresh(a);xgeRenderRequest();return 1;
}
int xideGotoViewIsOpen(xide_app* a) { return a && a->gotoView && xideDialogIsOpen(a->gotoView->window); }
void xideGotoViewLayout(xide_app* a)
{
 if(!a->gotoView) return;
 xide_goto_view* v=a->gotoView;int width=a->viewportWidth-48,height=a->viewportHeight-48;if(width<200) width=200;if(height<200) height=200;xideDialogSetMaxSize(v->window,width,height);
 int cw=xideUiHeight(a,480),ch=xideUiHeight(a,226);float scale=xideUiFontScale(a);
 if(v->scale!=scale) { xideUiLayoutApply(a,&v->layout,1);xideDialogSetContentSize(v->window,cw,ch);v->scale=scale; }
 
}
void xideGotoViewDestroy(xide_app* a) { free(a->gotoView);a->gotoView=NULL; }
xui_widget xideGotoViewWindow(xide_app* a) { return a->gotoView ? a->gotoView->window : NULL; }
xui_widget xideGotoViewControl(xide_app* a,int control) { return a->gotoView && control>=0 && control<XIDE_GOTO_CONTROL_COUNT ? a->gotoView->controls[control] : NULL; }
const char* xideGotoViewMessage(xide_app* a) { return a->gotoView ? a->gotoView->messageText : ""; }
