#include "ui/find_view.h"
#include "ui/editor_view.h"
#include "services/ui_font.h"
#include "theme/theme.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#define TRY(call) do {int r_=(call);if(r_!=XUI_OK)return r_;}while(0)
struct xide_find_view {
 xide_app* app;
 xui_widget window,body,footer,divider,scroll,rows[5],controls[XIDE_FIND_COUNT];
 xide_document* target;uint64_t targetId,due;
 int replace,valid,count,active,updating,pending,rangeStart,rangeEnd,navigating;
 int layoutWidth,layoutHeight,layoutReplace,readonly,ready;float layoutScale;
 uint32_t targetVersion;char* targetName;
 char cells[4][200];
};
static xide_document* target(xide_app* a) {return a->activeDocument && a->activeDocument->open ? a->activeDocument:NULL;}
static int shown(struct xide_find_view* v) {return v && xuiWindowIsOpen(v->window);}
static int all(struct xide_find_view* v) {return xuiCheckBoxGetChecked(v->controls[XIDE_FIND_ALL_EDITORS]);}
static int hex(unsigned char c) {return c>='0' && c<='9' ? c-'0':c>='a' && c<='f' ? c-'a'+10:c>='A' && c<='F' ? c-'A'+10:-1;}
/* GUI/native find strings cannot carry a decoded NUL. Reject malformed Unicode
 * escapes before handing text to older native engines which accepted them. */
static int escapes(const char* s) {
 for(size_t i=0;s[i];++i)if(s[i]=='\\') {
  if(!s[++i])return 0;
  if(s[i]=='0')return 0;
  if(s[i]=='x' || s[i]=='u') {
   int count=s[i]=='x' ? 2:4,value=0;
   for(int n=0;n<count;++n){if(!s[i+1] || hex((unsigned char)s[i+1])<0)return 0;value=value*16+hex((unsigned char)s[++i]);}
   if(!value || (value>=0xdc00 && value<=0xdfff))return 0;
   if(value>=0xd800 && value<=0xdbff){if(s[i+1]!='\\' || s[i+2]!='u')return 0;i+=2;int low=0;for(int n=0;n<4;++n){if(!s[i+1] || hex((unsigned char)s[i+1])<0)return 0;low=low*16+hex((unsigned char)s[++i]);}if(low<0xdc00 || low>0xdfff)return 0;}
  }
 }
 return 1;
}
static void status(struct xide_find_view* v,const char* s) {xuiLabelSetText(v->controls[XIDE_FIND_STATUS],s);}
static void clearMarks(struct xide_find_view* v) {
 if(all(v)){for(int i=0;i<xuiCodeFindScopeGetEditorCount(v->app->editorFindScope);++i)xuiCodeEditClearFind(xuiCodeFindScopeGetEditor(v->app->editorFindScope,i));}
 else if(v->target && v->target->open)xuiCodeEditClearFind(v->target->editor);
}
static void dirty(struct xide_find_view* v) {
 v->valid=0;v->pending=1;v->due=GetTickCount64()+80;v->active=-1;
 if(shown(v)){xuiTableViewRefreshAdapter(v->controls[XIDE_FIND_RESULTS]);xgeRenderRequest();}
}
void xideFindViewChanged(xide_app* a) {
 struct xide_find_view* v=a ? a->findView:NULL;if(!v || !v->ready || v->navigating)return;
 if(v->target && v->target->open && v->target->id==v->targetId && v->targetVersion!=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(v->target->editor))) {
  xuiCheckBoxSetChecked(v->controls[XIDE_FIND_SELECTION],0);v->rangeStart=v->rangeEnd=0;
 }
 dirty(v);
}
static void captureRange(struct xide_find_view* v) {
 v->rangeStart=v->rangeEnd=0;
 if(v->target)xuiCodeSelectionGetRange(xuiCodeEditGetSelection(v->target->editor),&v->rangeStart,&v->rangeEnd);
}
static void options(struct xide_find_view* v,xui_find_options_t* o) {
 *o=(xui_find_options_t){0};o->iSize=sizeof(*o);o->sPattern=xuiInputGetText(v->controls[XIDE_FIND_QUERY]);o->sReplacement=xuiInputGetText(v->controls[XIDE_FIND_REPLACEMENT]);
 const uint32_t flags[]={XUI_FIND_CASE_SENSITIVE,XUI_FIND_WHOLE_WORD,XUI_FIND_REGEX,XUI_FIND_ESCAPE,XUI_FIND_SELECTION};
 for(int i=0;i<5;++i)if(xuiCheckBoxGetChecked(v->controls[XIDE_FIND_CASE+i]))o->iFlags|=flags[i];
 o->iStartOffset=-1;o->iRangeEnd=-1;
 if(o->iFlags&XUI_FIND_SELECTION){o->iRangeStart=v->rangeStart;o->iRangeEnd=v->rangeEnd;}
 if(all(v))o->iFlags&=~XUI_FIND_SELECTION;
}
static int result(struct xide_find_view* v,int i,xui_code_find_result_t* r) {
 if(!v->valid || i<0 || i>=v->count)return XUI_ERROR_INVALID_ARGUMENT;
 return all(v) ? xuiCodeFindScopeGetResult(v->app->editorFindScope,i,r):v->target ? xuiCodeEditGetFindResult(v->target->editor,i,r):XUI_ERROR_NOT_FOUND;
}
static int rows(xui_widget w,void* user) {(void)w;struct xide_find_view* v=user;return v->valid ? v->count:0;}
static int cell(xui_widget w,int row,int column,xui_table_view_cell_t* c,void* user) {
 (void)w;struct xide_find_view* v=user;xui_code_find_result_t r={.iSize=sizeof(r)};
 if(column<0 || column>3 || result(v,row,&r)!=XUI_OK)return 0;
 if(column==0){c->sText=all(v) ? xuiCodeFindScopeGetResultEditorName(v->app->editorFindScope,row):v->target ? (v->target->path ? v->target->path:v->target->title):"";return 1;}
 else if(column<3)snprintf(v->cells[column],sizeof(v->cells[column]),"%d",(column==1 ? r.iLine:r.iColumn)+1);
 else snprintf(v->cells[3],sizeof(v->cells[3]),"%s",r.sPreview);
 c->sText=v->cells[column];return 1;
}
static void activate(struct xide_find_view* v,int i) {
 xui_code_find_result_t r={.iSize=sizeof(r)};if(result(v,i,&r)!=XUI_OK)return;
 v->active=i;v->navigating=1;
 if(all(v))xuiCodeFindScopeActivateResult(v->app->editorFindScope,i);
 else if(v->target && xideEditorShow(v->app,v->target)==XUI_OK){xuiCodeSelectionSetRange(xuiCodeEditGetSelection(r.pEditor),xuiCodeEditGetDocument(r.pEditor),r.iStart,r.iEnd);xuiCodeEditEnsureCaretVisible(r.pEditor);}
 v->navigating=0;xgeRenderRequest();
}
static void selected(xui_widget w,int row,int col,int mode,void* user) {(void)w;(void)col;(void)mode;struct xide_find_view* v=user;if(!v->updating)activate(v,row);}
int xideFindViewRefresh(xide_app* a) {
 struct xide_find_view* v=a ? a->findView:NULL;if(!v)return XUI_ERROR_INVALID_ARGUMENT;
 v->pending=0;v->valid=0;v->count=0;v->active=-1;xui_find_options_t o;options(v,&o);int r=XUI_OK;
 if((!v->target && !all(v)) || !*o.sPattern || (all(v) && !xuiCodeFindScopeGetEditorCount(a->editorFindScope))){clearMarks(v);xuiTableViewRefreshAdapter(v->controls[XIDE_FIND_RESULTS]);status(v,*o.sPattern ? "没有可查找的编辑器；打开文档后自动继续。":"请输入查找内容。");return XUI_OK;}
 if((o.iFlags&XUI_FIND_ESCAPE) && !escapes(o.sPattern)){clearMarks(v);status(v,"无效转义：不支持 NUL、截断的转义或孤立的代理项。");xuiTableViewRefreshAdapter(v->controls[XIDE_FIND_RESULTS]);return XUI_ERROR_INVALID_ARGUMENT;}
 xerror* previous=xrtTakeError();
 if(all(v)){o.iFlags&=~XUI_FIND_SELECTION;o.iRangeStart=0;o.iRangeEnd=-1;r=xuiCodeFindScopeFindAll(a->editorFindScope,&o,&v->count);}
 else if((o.iFlags&XUI_FIND_SELECTION) && v->rangeEnd<=v->rangeStart){xrtSetErrorTake(previous);status(v,"当前编辑器没有已捕获的非空选区。");xuiTableViewRefreshAdapter(v->controls[XIDE_FIND_RESULTS]);return XUI_OK;}
 else r=xuiCodeEditFindAll(v->target->editor,&o,&v->count);
 xrtErrorFree(xrtTakeError());xrtSetErrorTake(previous);
 v->valid=r==XUI_OK;
 v->targetVersion=v->target ? xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(v->target->editor)):0;
 if(r!=XUI_OK){clearMarks(v);status(v,r==XUI_ERROR_OUT_OF_MEMORY ? "内存不足；请缩小查找范围后重试。":"查找表达式无效；请检查正则表达式或转义。");}
 else {char text[160];snprintf(text,sizeof(text),"找到 %d 处 · %s；替换作用于当前文档。",v->count,all(v) ? "所有已打开文档":"当前文档");status(v,text);}
 xuiTableViewRefreshAdapter(v->controls[XIDE_FIND_RESULTS]);xgeRenderRequest();return r;
}
static void changed(xui_widget w,const char* s,void* user) {(void)w;(void)s;struct xide_find_view* v=user;if(!v->updating)dirty(v);}
static void flagChanged(xui_widget w,int checked,void* user) {
 struct xide_find_view* v=user;if(v->updating)return;
 if(w==v->controls[XIDE_FIND_SELECTION] && checked)captureRange(v);
 if(w==v->controls[XIDE_FIND_ALL_EDITORS] && checked)xuiCheckBoxSetChecked(v->controls[XIDE_FIND_SELECTION],0);
 dirty(v);
}
static void closeButton(xui_widget w,void* user) {(void)w;struct xide_find_view* v=user;xideFindViewClose(v->app);}
static void closed(xui_widget w,void* user) {closeButton(w,user);}
static void command(xui_widget w,void* user) {
 struct xide_find_view* v=user;xide_app* a=v->app;int id=-1;for(int i=XIDE_FIND_PREVIOUS;i<=XIDE_FIND_REPLACE_ALL;++i)if(w==v->controls[i])id=i;
 xideFindViewPoll(a);if(!v->target && !all(v))return;
 if(id==XIDE_FIND_ALL){xideFindViewRefresh(a);return;}
 if(v->pending && xideFindViewRefresh(a)!=XUI_OK)return;
 if(id==XIDE_FIND_NEXT || id==XIDE_FIND_PREVIOUS){if(v->valid && v->count){int i=v->active;if(i<0)i=id==XIDE_FIND_PREVIOUS ? 0:-1;i=(i+(id==XIDE_FIND_PREVIOUS ? v->count-1:1))%v->count;activate(v,i);}return;}
 if(!v->target || xuiCodeEditIsReadonly(v->target->editor)){status(v,"当前没有可写文档，不能替换。");return;}
 xui_find_options_t o;options(v,&o);int n=0,r;
 if(!*o.sPattern){status(v,"请输入查找内容。");return;}
 if((o.iFlags&XUI_FIND_ESCAPE) && (!escapes(o.sPattern) || !escapes(o.sReplacement))){status(v,"无效转义：不支持 NUL、截断的转义或孤立的代理项。");return;}
 if((o.iFlags&XUI_FIND_SELECTION) && v->rangeEnd<=v->rangeStart){status(v,"当前编辑器没有已捕获的非空选区。");return;}
 xerror* previous=xrtTakeError();
 if(id==XIDE_FIND_REPLACE)r=xuiCodeEditReplaceCurrent(v->target->editor,&o);
 else r=xuiCodeEditReplaceAll(v->target->editor,&o,&n);
 xrtErrorFree(xrtTakeError());xrtSetErrorTake(previous);
 if(r!=XUI_OK){status(v,"替换未完成：请检查表达式、转义与当前匹配。");return;}
 if(id==XIDE_FIND_REPLACE_ALL){captureRange(v);if(xuiCheckBoxGetChecked(v->controls[XIDE_FIND_SELECTION]))xuiCheckBoxSetChecked(v->controls[XIDE_FIND_SELECTION],0);}
 xideFindViewRefresh(a);if(id==XIDE_FIND_REPLACE_ALL){char text[120];snprintf(text,sizeof(text),"已在当前文档替换 %d 处；可在该文档中一次撤销。",n);status(v,text);}
}
static int key(xui_widget w,const xui_event_t* e,void* user) {
 (void)w;struct xide_find_view* v=user;if(e->iType!=XUI_EVENT_KEY_DOWN || e->iPhase==XUI_EVENT_PHASE_CAPTURE)return XUI_OK;
 if(e->iKey==XUI_KEY_ESCAPE){xideFindViewClose(v->app);return XUI_EVENT_DISPATCH_STOP;}
 if((e->iModifiers&XUI_MOD_CTRL) && (e->iKey=='F' || e->iKey=='H' || e->iKey=='f' || e->iKey=='h')){xideFindViewOpen(v->app,e->iKey=='H' || e->iKey=='h');return XUI_EVENT_DISPATCH_STOP;}
 if(e->iKey==XUI_KEY_ENTER || e->iKey==XUI_KEY_F3){command(v->controls[e->iModifiers&XUI_MOD_SHIFT ? XIDE_FIND_PREVIOUS:XIDE_FIND_NEXT],v);return XUI_EVENT_DISPATCH_STOP;}
 return XUI_OK;
}
static int add(xui_widget parent,xui_widget child) {return xuiWidgetAddChild(parent,child);}
static float stackHeight(xui_widget parent) {
 xui_thickness_t p=xuiWidgetGetPadding(parent);float height=p.fTop+p.fBottom;int count=0;
 for(xui_widget w=xuiWidgetGetFirstChild(parent);w;w=xuiWidgetGetNextSibling(w))if(xuiWidgetGetVisible(w)){height+=xuiWidgetGetPreferredSize(w).fY;if(count++)height+=xuiWidgetGetGap(parent);}
 return height;
}
static int create(xide_app* a) {
 struct xide_find_view* v=calloc(1,sizeof(*v));if(!v)return XUI_ERROR_OUT_OF_MEMORY;v->app=a;v->active=-1;a->findView=v;
 xui_window_desc_t d={0};d.iSize=sizeof(d);d.sTitle="查找和替换";d.pFont=a->uiFont;d.bClosed=1;d.bTopMost=1;d.bHideCollapse=1;d.bHideMaximize=1;d.fMinWidth=500;d.fMinHeight=330;
 TRY(xuiWindowCreate(a->context,&v->window,&d));TRY(add(a->root,v->window));TRY(xuiWindowSetClose(v->window,closed,v));
 xui_widget client=xuiWindowGetClientWidget(v->window);TRY(xuiWidgetSetLayoutType(client,XUI_LAYOUT_COLUMN));TRY(xuiWidgetSetEventHandler(client,XUI_EVENT_KEY_DOWN,key,v));
 xui_scroll_view_desc_t sd={0};sd.iSize=sizeof(sd);TRY(xuiScrollViewCreate(a->context,&v->scroll,&sd));TRY(add(client,v->scroll));TRY(xuiWidgetSetSizeMode(v->scroll,XUI_SIZE_FILL,XUI_SIZE_FILL));TRY(xuiWidgetSetFlex(v->scroll,1,1));TRY(xuiScrollViewSetScrollbarPolicy(v->scroll,XUI_SCROLLBAR_POLICY_HIDDEN,XUI_SCROLLBAR_POLICY_AUTO));v->body=xuiScrollViewGetContentWidget(v->scroll);TRY(xuiWidgetSetLayoutType(v->body,XUI_LAYOUT_COLUMN));TRY(xuiWidgetSetPadding(v->body,(xui_thickness_t){12,12,12,12}));TRY(xuiWidgetSetGap(v->body,8));
 for(int i=0;i<2;++i){xui_input_desc_t in={0};in.iSize=sizeof(in);in.pFont=a->uiFont;in.sPlaceholder=i ? "替换为":"查找内容";TRY(xuiInputCreate(a->context,&v->controls[i],&in));TRY(xuiWidgetSetAccessibleName(v->controls[i],in.sPlaceholder));TRY(add(v->body,v->controls[i]));TRY(xuiInputSetChange(v->controls[i],changed,v));}
 for(int i=0;i<5;++i){TRY(xuiWidgetCreate(a->context,&v->rows[i]));TRY(xuiWidgetSetLayoutType(v->rows[i],XUI_LAYOUT_ROW));TRY(xuiWidgetSetGap(v->rows[i],10));TRY(add(i<2 ? v->body:client,v->rows[i]));}
 const char* titles[]={"区分大小写","全词匹配","正则表达式","转义字符","仅选区","所有已打开编辑器"};
 for(int i=0;i<6;++i){xui_checkbox_desc_t c={0};c.iSize=sizeof(c);c.sText=titles[i];c.pFont=a->uiFont;TRY(xuiCheckBoxCreate(a->context,&v->controls[XIDE_FIND_CASE+i],&c));TRY(add(v->rows[i/3],v->controls[XIDE_FIND_CASE+i]));TRY(xuiCheckBoxSetChange(v->controls[XIDE_FIND_CASE+i],flagChanged,v));}
 TRY(xuiWidgetRemoveFromParent(v->controls[XIDE_FIND_ALL_EDITORS]));TRY(add(v->body,v->controls[XIDE_FIND_ALL_EDITORS]));
 for(int i=XIDE_FIND_STATUS;i<=XIDE_FIND_TARGET;++i){xui_label_desc_t l={0};l.iSize=sizeof(l);l.sText="";l.pFont=a->uiFont;l.iWrapMode=XUI_TEXT_WRAP_WORD;TRY(xuiLabelCreate(a->context,&v->controls[i],&l));TRY(add(v->body,v->controls[i]));}
 xui_table_view_desc_t td={0};td.iSize=sizeof(td);td.pFont=a->uiFont;td.onCount=rows;td.onCell=cell;td.pAdapterUser=v;td.iSelectionMode=XUI_TABLE_VIEW_SELECTION_ROW;TRY(xuiTableViewCreate(a->context,&v->controls[XIDE_FIND_RESULTS],&td));TRY(add(v->body,v->controls[XIDE_FIND_RESULTS]));TRY(xuiTableViewSetSelect(v->controls[XIDE_FIND_RESULTS],selected,v));
 TRY(xuiWidgetCreate(a->context,&v->footer));TRY(xuiWidgetSetLayoutType(v->footer,XUI_LAYOUT_COLUMN));TRY(xuiWidgetSetPadding(v->footer,(xui_thickness_t){12,8,12,8}));TRY(xuiWidgetSetGap(v->footer,6));
 xui_separator_desc_t line={0};line.iSize=sizeof(line);line.iOrientation=XUI_ORIENTATION_HORIZONTAL;TRY(xuiSeparatorCreate(a->context,&v->divider,&line));TRY(add(client,v->divider));TRY(add(client,v->footer));
 for(int i=2;i<5;++i){TRY(xuiWidgetRemoveFromParent(v->rows[i]));TRY(add(v->footer,v->rows[i]));}
 const char* buttons[]={"上一个","下一个","查找全部","替换当前匹配","全部替换（当前文档）","关闭"};
 for(int i=0;i<6;++i){xui_button_desc_t b={0};b.iSize=sizeof(b);b.pFont=a->uiFont;b.sText=buttons[i];TRY(xuiButtonCreate(a->context,&v->controls[XIDE_FIND_PREVIOUS+i],&b));TRY(add(v->rows[2+i/3],v->controls[XIDE_FIND_PREVIOUS+i]));TRY(xuiButtonSetClick(v->controls[XIDE_FIND_PREVIOUS+i],i==5 ? closeButton:command,v));}
 xui_rect_t owner=xuiWidgetGetWorldRect(target(a)->editor);
 TRY(xuiWidgetSetRect(v->window,(xui_rect_t){owner.fX+16,owner.fY+16,780,500}));
 TRY(xideUiFontBindWidget(a,v->window));v->ready=1;return XUI_OK;
}
void xideFindViewLayout(xide_app* a) {
 struct xide_find_view* v=a->findView;if(!shown(v))return;float scale=xideUiFontScale(a);
 xui_rect_t r=xuiWidgetGetRect(v->window);int width=a->viewportWidth-24,height=a->viewportHeight-24;if(width<1)width=1;if(height<1)height=1;
 if(r.fW<1 || r.fH<1)r=(xui_rect_t){24,24,780,500};
 if(r.fW>width)r.fW=width;
 if(r.fH>height)r.fH=height;
 if(r.fX+r.fW>a->viewportWidth)r.fX=a->viewportWidth-r.fW;
 if(r.fY+r.fH>a->viewportHeight)r.fY=a->viewportHeight-r.fH;
 if(r.fX<0)r.fX=0;
 if(r.fY<0)r.fY=0;
 xui_rect_t original=xuiWidgetGetRect(v->window);
 if(original.fX!=r.fX || original.fY!=r.fY)xuiWidgetSetRect(v->window,r);
 if(v->layoutWidth==r.fW && v->layoutHeight==r.fH && v->layoutReplace==v->replace && v->layoutScale==scale)return;
 v->layoutWidth=r.fW;v->layoutHeight=r.fH;v->layoutReplace=v->replace;v->layoutScale=scale;
 xuiWindowSetMinSize(v->window,fminf(width,500*scale),fminf(height,330*scale));xuiWidgetSetRect(v->window,r);
 for(int i=0;i<XIDE_FIND_COUNT;++i)if(i!=XIDE_FIND_RESULTS){int h=i==XIDE_FIND_STATUS || i==XIDE_FIND_TARGET ? 42:30;xuiWidgetSetSizeMode(v->controls[i],XUI_SIZE_FILL,XUI_SIZE_FIXED);xuiWidgetSetFlex(v->controls[i],1,1);xuiWidgetSetPreferredSize(v->controls[i],(xui_vec2_t){0,h*scale});}
 for(int i=0;i<5;++i){xuiWidgetSetSizeMode(v->rows[i],XUI_SIZE_FILL,XUI_SIZE_FIXED);xuiWidgetSetPreferredSize(v->rows[i],(xui_vec2_t){0,30*scale});}
 xuiWidgetSetVisible(v->controls[XIDE_FIND_REPLACE],v->replace);xuiWidgetSetVisible(v->controls[XIDE_FIND_REPLACE_ALL],v->replace);xuiWidgetSetVisible(v->controls[XIDE_FIND_REPLACEMENT],v->replace);
 float buttonWidth=120*scale;for(int i=XIDE_FIND_PREVIOUS;i<=XIDE_FIND_CLOSE;++i)if(xuiWidgetGetVisible(v->controls[i])){xui_vec2_t m={0};if(xuiWidgetMeasureContent(v->controls[i],(xui_vec2_t){XUI_LAYOUT_UNBOUNDED,XUI_LAYOUT_UNBOUNDED},&m)==XUI_OK)buttonWidth=fmaxf(buttonWidth,m.fX+12*scale);}
 int buttonColumns=r.fW-44>=3*buttonWidth ? 3:2,visible=0;
 for(int i=XIDE_FIND_PREVIOUS;i<=XIDE_FIND_CLOSE;++i){int row=xuiWidgetGetVisible(v->controls[i]) ? 2+visible++/buttonColumns:4;xui_widget parent=v->rows[row];if(xuiWidgetGetParent(v->controls[i])!=parent){xuiWidgetRemoveFromParent(v->controls[i]);xuiWidgetAddChild(parent,v->controls[i]);}}
 for(int i=2;i<5;++i)xuiWidgetSetVisible(v->rows[i],i<2+(visible+buttonColumns-1)/buttonColumns);
 xuiWidgetSetSizeMode(v->divider,XUI_SIZE_FILL,XUI_SIZE_FIXED);xuiWidgetSetSizeMode(v->footer,XUI_SIZE_FILL,XUI_SIZE_FIXED);
 xuiWidgetSetPreferredSize(v->divider,(xui_vec2_t){0,1});xuiWidgetSetPreferredSize(v->footer,(xui_vec2_t){0,stackHeight(v->footer)});
 xuiWidgetSetSizeMode(v->controls[XIDE_FIND_RESULTS],XUI_SIZE_FILL,XUI_SIZE_FIXED);xuiWidgetSetPreferredSize(v->controls[XIDE_FIND_RESULTS],(xui_vec2_t){0,180*scale});xuiScrollViewSetContentSize(v->scroll,r.fW-4,stackHeight(v->body));
 const char* names[]={"文档","行","列","内容"};xui_table_view_column_t columns[4]={0};float fractions[]={.38f,.08f,.08f,.46f};for(int i=0;i<4;++i){columns[i].iId=i;columns[i].sTitle=names[i];columns[i].fWidth=fmaxf(1,r.fW-40)*fractions[i];columns[i].fMinWidth=20;columns[i].bResizable=1;}
 xuiTableViewSetColumns(v->controls[XIDE_FIND_RESULTS],columns,4);
}
void xideFindViewPoll(xide_app* a) {
 struct xide_find_view* v=a->findView;if(!shown(v))return;
 xui_widget focus=xuiGetFocusWidget(a->context);
 if(focus && (!a->activeDocument || a->activeDocument->editor!=focus) && xuiWidgetGetType(focus)==xuiCodeEditGetType(a->context))for(size_t i=0;i<a->documents.count;++i)if(a->documents.items[i]->open && a->documents.items[i]->editor==focus){a->activeDocument=a->documents.items[i];xideActionsRefresh(a);break;}
 xide_document* d=target(a);
 if(d!=v->target || (d && d->id!=v->targetId)){v->target=d;v->targetId=d ? d->id:0;v->targetVersion=d ? xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor)):0;captureRange(v);if(!all(v) || !v->valid)dirty(v);}
 const char* name=d ? (d->path ? d->path:d->title):"无";int readonly=d && xuiCodeEditIsReadonly(d->editor);
 if(!v->targetName || strcmp(v->targetName,name) || v->readonly!=readonly){size_t size=strlen(name)+80;char* text=malloc(size);char* copy=malloc(strlen(name)+1);if(text && copy){strcpy(copy,name);snprintf(text,size,"当前编辑器：%s%s",name,readonly ? "（只读）":"");xuiLabelSetText(v->controls[XIDE_FIND_TARGET],text);free(v->targetName);v->targetName=copy;copy=NULL;v->readonly=readonly;}free(text);free(copy);}
 int writable=d && !xuiCodeEditIsReadonly(d->editor);xuiWidgetSetEnabled(v->controls[XIDE_FIND_REPLACE],writable);xuiWidgetSetEnabled(v->controls[XIDE_FIND_REPLACE_ALL],writable);xuiWidgetSetEnabled(v->controls[XIDE_FIND_SELECTION],!all(v));
 if(v->pending && GetTickCount64()>=v->due && !xuiGetPointerCapture(a->context) && !xideActionsHasModal(a))xideFindViewRefresh(a);
}
int xideFindViewOpen(xide_app* a,int replace) {
 if(!target(a))return 0;
 if(!a->findView && create(a)!=XUI_OK)return 0;
 if(!a->findView->ready)return 0;
 struct xide_find_view* v=a->findView;v->replace=replace;v->target=target(a);v->targetId=v->target->id;v->targetVersion=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(v->target->editor));captureRange(v);
 if(!shown(v) && v->rangeEnd>v->rangeStart && v->rangeEnd-v->rangeStart<256){char s[256];if(xuiCodeDocumentCopyRange(xuiCodeEditGetDocument(v->target->editor),v->rangeStart,v->rangeEnd,s,sizeof(s),NULL)==XUI_OK){v->updating=1;xuiInputSetText(v->controls[XIDE_FIND_QUERY],s);v->updating=0;}}
 xuiWindowSetOpen(v->window,1);xideFindViewLayout(a);xuiWindowBringToFront(v->window);xideFindViewPoll(a);xideFindViewRefresh(a);xuiSetFocusWidget(a->context,v->controls[XIDE_FIND_QUERY]);xgeRenderRequest();return 1;
}
void xideFindViewClose(xide_app* a) {struct xide_find_view* v=a ? a->findView:NULL;if(v){xuiWindowSetOpen(v->window,0);xide_document* d=target(a);if(d)xuiSetFocusWidget(a->context,d->editor);}}
void xideFindViewDestroy(xide_app* a) {if(a->findView){free(a->findView->targetName);free(a->findView);a->findView=NULL;}}
xui_widget xideFindViewWindow(xide_app* a) {return a && a->findView ? a->findView->window:NULL;}
xui_widget xideFindViewControl(xide_app* a,int i) {return a && a->findView && i>=0 && i<XIDE_FIND_COUNT ? a->findView->controls[i]:NULL;}
int xideFindViewNavigate(xide_app* a,int previous) {if(!a || !shown(a->findView))return 0;command(a->findView->controls[previous ? XIDE_FIND_PREVIOUS:XIDE_FIND_NEXT],a->findView);return 1;}
