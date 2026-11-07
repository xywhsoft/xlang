#include "ui/settings_view.h"
#include "services/preferences.h"
#include "services/ui_font.h"
#include "services/editor_font.h"
#include "services/toolchain.h"
#include "ui/task_view.h"
#include "ui/workspace_view.h"
#include "platform/path.h"
#include "platform/startup_alias.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#define TRY(call) do {int r=(call);if(r!=XUI_OK)return r;}while(0)
enum { TEXT,NUMBER,CHECK,THEME };
typedef struct setting_field {int page,kind;const char* title;const char* keywords;} setting_field;
static const char* pageNames[]={"编辑器设置","文档与帮助设置","编译器设置","外观设置","语言服务设置"};
static const setting_field fields[]={
 {0,TEXT,"代码字体文件","字体 font 编辑器"},{0,NUMBER,"代码字号（8–40 DIP）","字体 font size"},
 {0,NUMBER,"Tab 宽度（1–16）","制表符 tab"},{0,NUMBER,"缩进宽度（1–16）","缩进 indent"},
 {0,CHECK,"使用空格缩进","tab 空格"},{0,CHECK,"显示行号","line 行号"},{0,CHECK,"显示代码折叠边栏","fold 折叠"},
 {0,CHECK,"Minimap 滚动条（关闭后显示传统滚动条）","minimap scrollbar 缩略图 传统滚动条 开启 关闭"},{0,CHECK,"自动换行","wrap 换行"},
 {1,TEXT,"文档正文字体文件","文档 帮助 font 字体"},{1,NUMBER,"文档字号（8–40 DIP）","文档 字体 size"},
 {1,NUMBER,"文档行间距（0–24 DIP）","文档 行距 gap"},{1,CHECK,"文档正文自动换行","文档 wrap"},
 {2,TEXT,"xlang 编译器文件","compiler 工具链 xl.exe"},
 {3,TEXT,"界面字体文件","界面 font 字体"},{3,NUMBER,"界面字号（8–40 DIP）","界面 size 字体"},{3,THEME,"配色主题","theme 主题 颜色"},
 {4,CHECK,"启用 xlang 自动诊断","automatic 诊断 检查"},{4,NUMBER,"输入后诊断等待时间（0–2000 ms）","诊断 delay 延迟"},
 {4,CHECK,"自动显示代码补全","completion 补全"},{4,CHECK,"自动显示参数提示","signature 参数"}};
_Static_assert(sizeof(fields)/sizeof(*fields)==XIDE_PREF_COUNT,"Settings field IDs must match their descriptors");
struct xide_settings_view {
 xui_widget window,content,search,page,scroll,body,title,message,preview,previewButton,query,defaults,apply,okay,cancel;
 xui_widget navigation,navigationBody,nav[5],labels[XIDE_PREF_COUNT],controls[XIDE_PREF_COUNT],browse[XIDE_PREF_COUNT];
 xui_font previewFont;int selected,loading,dirtyNotice;char text[1024];
 xide_toolchain_request* request;xide_toolchain_profile profile;
};
static void message(xide_settings_view* v,const char* text)
{snprintf(v->text,sizeof(v->text),"%s",text);xuiLabelSetText(v->message,v->text);}
static int label(xide_app* a,xui_widget owner,xui_widget* out,const char* text)
{xui_label_desc_t d={0};d.iSize=sizeof(d);d.pFont=a->uiFont;d.sText=text;d.iWrapMode=XUI_TEXT_WRAP_WORD;TRY(xuiLabelCreate(a->context,out,&d));return xuiWidgetAddChild(owner,*out);}
static void previewClear(xide_app* a)
{xide_settings_view* v=a->settingsView;if(!v)return;if(v->preview)xuiLabelSetFont(v->preview,a->uiFont);if(v->previewFont)a->proxy.fontDestroy(&a->proxy,v->previewFont);v->previewFont=NULL;}
static void number(xui_widget widget,int n) {char s[32];snprintf(s,sizeof(s),"%d",n);xuiEditSetText(widget,s);}
static int integer(xui_widget widget,int* n)
{const char* text=xuiEditGetText(widget);char* end=NULL;errno=0;long value=strtol(text,&end,10);if(!*text || errno || *end || value<0 || value>2000)return 0;*n=(int)value;return 1;}
static int draft(xide_app* a,xide_settings_draft* d)
{
 xide_settings_view* v=a->settingsView;*d=(xide_settings_draft){0};d->options=*xideSettingsOptions(&a->preferences);
 d->compiler=xuiEditGetText(v->controls[XIDE_PREF_COMPILER]);d->editorFont=xuiEditGetText(v->controls[XIDE_PREF_CODE_PATH]);
 d->uiFont=xuiEditGetText(v->controls[XIDE_PREF_UI_PATH]);d->documentFont=xuiEditGetText(v->controls[XIDE_PREF_DOC_PATH]);
 int* values[]={&d->editorSize,&d->options.tabColumns,&d->options.indentColumns,&d->documentSize,&d->options.documentLineGap,&d->uiSize,&d->options.diagnosticDelay};
 const int ids[]={XIDE_PREF_CODE_SIZE,XIDE_PREF_TAB,XIDE_PREF_INDENT,XIDE_PREF_DOC_SIZE,XIDE_PREF_DOC_GAP,XIDE_PREF_UI_SIZE,XIDE_PREF_DELAY};
 for(int i=0;i<7;++i)if(!integer(v->controls[ids[i]],values[i])){message(v,"请输入有效的整数；已保留设置草稿。");return 0;}
 int* flags[]={&d->options.expandTabs,&d->options.lineNumbers,&d->options.foldMargin,&d->options.minimap,&d->options.wordWrap,&d->options.documentWrap,&d->options.automaticDiagnostics,&d->options.completion,&d->options.signatureHelp};
 const int boolIds[]={XIDE_PREF_EXPAND,XIDE_PREF_LINES,XIDE_PREF_FOLD,XIDE_PREF_MINIMAP,XIDE_PREF_WRAP,XIDE_PREF_DOC_WRAP,XIDE_PREF_AUTO,XIDE_PREF_COMPLETION,XIDE_PREF_SIGNATURE};
 for(int i=0;i<9;++i)*flags[i]=xuiCheckBoxGetChecked(v->controls[boolIds[i]]);
 d->options.theme=xuiComboBoxGetSelected(v->controls[XIDE_PREF_THEME]);return 1;
}
static void populate(xide_app* a)
{
 xide_settings_view* v=a->settingsView;const xide_settings_options* o=xideSettingsOptions(&a->preferences);v->loading=1;
 xuiEditSetText(v->controls[XIDE_PREF_CODE_PATH],a->codeFontPath ? a->codeFontPath : "");number(v->controls[XIDE_PREF_CODE_SIZE],a->codeFontSize);
 xuiEditSetText(v->controls[XIDE_PREF_UI_PATH],a->uiFontPath ? a->uiFontPath : "");number(v->controls[XIDE_PREF_UI_SIZE],a->uiFontSize);
 xuiEditSetText(v->controls[XIDE_PREF_DOC_PATH],a->documentFontPath ? a->documentFontPath : "");number(v->controls[XIDE_PREF_DOC_SIZE],a->documentFontSize);
 xuiEditSetText(v->controls[XIDE_PREF_COMPILER],xideTaskViewCompiler(a) ? xideTaskViewCompiler(a) : "");
 const int ns[]={o->tabColumns,o->indentColumns,o->documentLineGap,o->diagnosticDelay};const int ni[]={XIDE_PREF_TAB,XIDE_PREF_INDENT,XIDE_PREF_DOC_GAP,XIDE_PREF_DELAY};for(int i=0;i<4;++i)number(v->controls[ni[i]],ns[i]);
 const int bs[]={o->expandTabs,o->lineNumbers,o->foldMargin,o->minimap,o->wordWrap,o->documentWrap,!a->disableAutomaticDiagnostics,o->completion,o->signatureHelp};const int bi[]={XIDE_PREF_EXPAND,XIDE_PREF_LINES,XIDE_PREF_FOLD,XIDE_PREF_MINIMAP,XIDE_PREF_WRAP,XIDE_PREF_DOC_WRAP,XIDE_PREF_AUTO,XIDE_PREF_COMPLETION,XIDE_PREF_SIGNATURE};for(int i=0;i<9;++i)xuiCheckBoxSetChecked(v->controls[bi[i]],bs[i]);
 xuiComboBoxSetSelected(v->controls[XIDE_PREF_THEME],a->theme);v->loading=0;
}
int xideSettingsViewApply(xide_app* a)
{
 if(!a || !a->settingsView)return 0;
 a->settingsView->dirtyNotice=0;xide_settings_draft d;xide_model_error e={0};
 if(!draft(a,&d))return 0;
 a->settingsView->dirtyNotice=0;previewClear(a);
 if(!xidePreferencesApply(a,&d,&e)){message(a->settingsView,e.message);return 0;}
 populate(a);message(a->settingsView,"设置已保存并应用；正在运行的构建任务继续使用原工具链。");return 1;
}
static void chosen(xide_app* a,const char* path,void* user)
{if(path && a->settingsView)xuiEditSetText((xui_widget)user,path);}
static char* bundledCompiler(void)
{wchar_t binary[32768];char text[131072];if(!GetModuleFileNameW(NULL,binary,32768) || !WideCharToMultiByte(CP_UTF8,0,xideStartupResourceModule(binary),-1,text,sizeof(text),NULL,NULL))return NULL;char* dir=xidePathDirectory(text);char* p=dir ? xidePathResolve(dir,"xl.exe") : NULL;free(dir);return p;}
static void defaults(xide_app* a)
{
 xide_settings_view* v=a->settingsView;int p=v->selected;
 if(p==0 || p==1 || p==3){int pathId=p==0 ? XIDE_PREF_CODE_PATH : p==1 ? XIDE_PREF_DOC_PATH : XIDE_PREF_UI_PATH;char* path=xideEditorFontDefaultPath();if(path){xuiEditSetText(v->controls[pathId],path);free(path);}number(v->controls[pathId+1],16);}
 if(p==0){number(v->controls[XIDE_PREF_TAB],4);number(v->controls[XIDE_PREF_INDENT],4);for(int i=XIDE_PREF_EXPAND;i<=XIDE_PREF_MINIMAP;++i)xuiCheckBoxSetChecked(v->controls[i],i!=XIDE_PREF_EXPAND);xuiCheckBoxSetChecked(v->controls[XIDE_PREF_WRAP],0);}
 if(p==1){number(v->controls[XIDE_PREF_DOC_GAP],4);xuiCheckBoxSetChecked(v->controls[XIDE_PREF_DOC_WRAP],1);}
 if(p==2){char* path=bundledCompiler();if(path){xuiEditSetText(v->controls[XIDE_PREF_COMPILER],path);free(path);}}
 if(p==3)xuiComboBoxSetSelected(v->controls[XIDE_PREF_THEME],0);
 if(p==4){for(int i=XIDE_PREF_AUTO;i<=XIDE_PREF_SIGNATURE;++i)if(i!=XIDE_PREF_DELAY)xuiCheckBoxSetChecked(v->controls[i],1);number(v->controls[XIDE_PREF_DELAY],75);}
 message(v,"本页已填入默认值；点击应用后保存。其他页面的草稿保持不变。");
}
static void click(xui_widget widget,void* user)
{
 xide_app* a=user;xide_settings_view* v=a->settingsView;
 if(widget==v->cancel){previewClear(a);xideDialogSetOpen(v->window,0);}
 else if(widget==v->apply || widget==v->okay){if(xideSettingsViewApply(a) && widget==v->okay)xideDialogSetOpen(v->window,0);}
 else if(widget==v->defaults)defaults(a);
 else if(widget==v->previewButton){
  int id=v->selected==XIDE_SETTINGS_DOCUMENTS ? XIDE_PREF_DOC_PATH : v->selected==XIDE_SETTINGS_APPEARANCE ? XIDE_PREF_UI_PATH : XIDE_PREF_CODE_PATH;int size;xide_model_error e={0};xui_font font=NULL;
  if(integer(v->controls[id+1],&size) && xideFontLoad(a,xuiEditGetText(v->controls[id]),size,&font,&e)){previewClear(a);v->previewFont=font;xuiLabelSetFont(v->preview,font);message(v,"仅预览区已更新；点击应用才会保存并修改工作台。");}else message(v,*e.message ? e.message : "请输入有效字号。");
 } else if(widget==v->query){
  if(!v->request){const char* path=xuiEditGetText(v->controls[XIDE_PREF_COMPILER]);char* dir=xidePathDirectory(path);xide_model_error e={0};v->request=dir ? xideToolchainRequest(xideTasksToolchains(a->tasks),path,dir,1,&e) : NULL;free(dir);message(v,v->request ? "正在后台查询工具链…" : *e.message ? e.message : "请选择编译器文件。");}
 } else {
  for(int i=0;i<5;++i)if(widget==v->nav[i]){v->selected=i;previewClear(a);xuiEditSetText(v->search,"");xuiScrollViewSetOffset(v->scroll,0,0);break;}
  for(int i=0;i<XIDE_PREF_COUNT;++i)if(widget==v->browse[i])xideFileDialogChoose(a,v->window,XUI_FILE_DIALOG_MODE_OPEN_FILE,i==XIDE_PREF_COMPILER ? "选择 xlang 编译器" : "选择字体文件",i==XIDE_PREF_COMPILER ? "可执行程序|*.exe" : "字体|*.ttf;*.ttc;*.otf",xuiEditGetText(v->controls[i]),NULL,chosen,v->controls[i]);
 }
 v->dirtyNotice=0;xideSettingsViewLayout(a);xgeRenderRequest();
}
static void closed(xui_widget widget,void* user) {(void)widget;xide_app* a=user;click(a->settingsView->cancel,a);}
static int button(xide_app* a,xui_widget owner,xui_widget* out,const char* title)
{xui_button_desc_t d={0};d.iSize=sizeof(d);d.pFont=a->uiFont;d.sText=title;TRY(xuiButtonCreate(a->context,out,&d));TRY(xuiButtonSetClick(*out,click,a));return xuiWidgetAddChild(owner,*out);}
static void changed(xui_widget widget,const char* text,void* user) {(void)widget;(void)text;(void)user;/* Reflow on the next owner frame, after Input releases its edit lock. */xgeRenderRequest();}
static void fieldChanged(xui_widget widget,const char* text,void* user)
{(void)widget;(void)text;xide_app* a=user;if(a->settingsView && !a->settingsView->loading)a->settingsView->dirtyNotice=1;xgeRenderRequest();}
static void checkChanged(xui_widget widget,int state,void* user) {(void)state;fieldChanged(widget,NULL,user);}
static void selectChanged(xui_widget widget,int index,int value,void* user) {(void)index;(void)value;fieldChanged(widget,NULL,user);}
static int layoutSettingsContent(xui_widget,xui_rect_t,void*);
int xideSettingsViewCreate(xide_app* a)
{
 xide_settings_view* v=calloc(1,sizeof(*v));if(!v)return XUI_ERROR_OUT_OF_MEMORY;a->settingsView=v;
 TRY(xideDialogCreate(a,&v->window,"设置",940,590,14));TRY(xideDialogSetCancel(v->window,closed,a));v->content=xideDialogGetContentWidget(v->window);TRY(xuiWidgetAddChild(a->root,v->window));TRY(xuiWidgetSetLayoutType(v->content,XUI_LAYOUT_MANUAL));
 xui_input_desc_t input={0};input.iSize=sizeof(input);input.pFont=a->uiFont;input.sText="";input.sPlaceholder="搜索设置：字体、缩进、编译器、诊断…";
 TRY(xuiInputCreate(a->context,&v->search,&input));TRY(xuiWidgetAddChild(v->content,v->search));TRY(xuiInputSetChange(v->search,changed,a));
 xui_scroll_view_desc_t navigation={0};navigation.iSize=sizeof(navigation);navigation.iPolicyX=XUI_SCROLLBAR_POLICY_HIDDEN;navigation.iPolicyY=XUI_SCROLLBAR_POLICY_AUTO;
 TRY(xuiScrollViewCreate(a->context,&v->navigation,&navigation));TRY(xuiWidgetAddChild(v->content,v->navigation));v->navigationBody=xuiScrollViewGetContentWidget(v->navigation);
 TRY(xuiWidgetSetLayoutType(v->navigationBody,XUI_LAYOUT_MANUAL));
 for(int i=0;i<5;++i)TRY(button(a,v->navigationBody,&v->nav[i],pageNames[i]));
 TRY(label(a,v->content,&v->title,"编辑器设置"));
 xui_scroll_view_desc_t scroll={0};scroll.iSize=sizeof(scroll);scroll.iPolicyX=XUI_SCROLLBAR_POLICY_HIDDEN;scroll.iPolicyY=XUI_SCROLLBAR_POLICY_AUTO;TRY(xuiScrollViewCreate(a->context,&v->scroll,&scroll));TRY(xuiWidgetAddChild(v->content,v->scroll));v->body=xuiScrollViewGetContentWidget(v->scroll);TRY(xuiWidgetSetLayoutType(v->body,XUI_LAYOUT_MANUAL));
 for(int i=0;i<XIDE_PREF_COUNT;++i){
  TRY(label(a,v->body,&v->labels[i],fields[i].title));
  if(fields[i].kind==CHECK){xui_checkbox_desc_t d={0};d.iSize=sizeof(d);d.pFont=a->uiFont;d.sText=fields[i].title;TRY(xuiCheckBoxCreate(a->context,&v->controls[i],&d));}
  else if(fields[i].kind==THEME){xui_combobox_desc_t d={0};d.iSize=sizeof(d);d.pFont=a->uiFont;TRY(xuiComboBoxCreate(a->context,&v->controls[i],&d));const char* names[]={"经典浅色","石墨深色","暖纸白","海洋蓝"};TRY(xuiComboBoxSetItems(v->controls[i],names,4));}
  else {xui_input_desc_t d={0};d.iSize=sizeof(d);d.pFont=a->uiFont;d.sText="";TRY(xuiInputCreate(a->context,&v->controls[i],&d));}
  if(fields[i].kind==CHECK)TRY(xuiCheckBoxSetChange(v->controls[i],checkChanged,a));else if(fields[i].kind==THEME)TRY(xuiComboBoxSetSelect(v->controls[i],selectChanged,a));else TRY(xuiInputSetChange(v->controls[i],fieldChanged,a));
  TRY(xuiWidgetSetAccessibleName(v->controls[i],fields[i].title));TRY(xuiWidgetAddChild(v->body,v->controls[i]));
  if(fields[i].kind==TEXT)TRY(button(a,v->body,&v->browse[i],"浏览…"));
 }
 TRY(label(a,v->body,&v->preview,"中文字体预览 / ABC 0123456789 · func main() as int"));
 TRY(button(a,v->body,&v->previewButton,"预览字体"));TRY(button(a,v->body,&v->query,"查询工具链"));
 TRY(label(a,v->body,&v->message,""));TRY(button(a,v->content,&v->defaults,"恢复本页默认值"));TRY(button(a,v->content,&v->apply,"应用"));TRY(button(a,v->content,&v->okay,"确定"));TRY(button(a,v->content,&v->cancel,"取消"));
 TRY(xideDialogFormBegin(v->window,640,440));TRY(xideDialogSetFixedBody(v->window,1));
 TRY(xuiWidgetSetLayoutType(v->content,XUI_LAYOUT_MANUAL));
 const xui_widget actions[]={v->defaults,v->okay,v->apply,v->cancel};
 TRY(xideDialogFormActions(v->window,actions,sizeof(actions)/sizeof(*actions)));
 TRY(xuiWidgetSetLayoutChildrenCallback(v->content,layoutSettingsContent,a));
 return XUI_OK;
}
static int matches(const char* value,const char* query)
{if(strstr(value,query))return 1;size_t n=strlen(query);for(;*value;++value)if(!_strnicmp(value,query,n))return 1;return !n;}
static int layoutSettingsContent(xui_widget content,xui_rect_t area,void* user)
{
 (void)content;xide_app* a=user;xide_settings_view* v=a->settingsView;if(!v)return XUI_OK;
 float s=xideUiFontScale(a),width=area.fW,height=area.fH,nav=fminf(174*s,width*.28f),x=nav+18*s,w=fmaxf(1,width-x),y=0;
 float top=82*s,scrollHeight=fmaxf(1,height-top);
 xuiLayoutArrangeChild(content,v->search,(xui_rect_t){area.fX,area.fY,width,32*s});
 xuiLayoutArrangeChild(content,v->navigation,(xui_rect_t){area.fX,area.fY+48*s,nav,fmaxf(1,height-48*s)});
 for(int i=0;i<5;++i)xuiWidgetSetRect(v->nav[i],(xui_rect_t){0,i*43*s,fmaxf(1,nav-12*s),36*s});
 xuiScrollViewSetContentSize(v->navigation,fmaxf(1,nav-12*s),5*43*s);
 const char* q=xuiEditGetText(v->search);const char* title=*q ? "搜索结果（所有分类）" : pageNames[v->selected];if(strcmp(xuiLabelGetText(v->title),title))xuiLabelSetText(v->title,title);xuiLayoutArrangeChild(xuiWidgetGetParent(v->title),v->title,(xui_rect_t){area.fX+x,area.fY+44*s,w,30*s});xuiLayoutArrangeChild(xuiWidgetGetParent(v->scroll),v->scroll,(xui_rect_t){area.fX+x,area.fY+top,w,scrollHeight});
 for(int i=0;i<XIDE_PREF_COUNT;++i){int show=*q ? matches(fields[i].title,q) || matches(fields[i].keywords,q) || matches(pageNames[fields[i].page],q) : fields[i].page==v->selected;
  xuiWidgetSetVisible(v->labels[i],show && fields[i].kind!=CHECK);xuiWidgetSetVisible(v->controls[i],show);if(v->browse[i])xuiWidgetSetVisible(v->browse[i],show);if(!show)continue;
  if(fields[i].kind==CHECK){xuiWidgetSetRect(v->controls[i],(xui_rect_t){0,y,w-20*s,32*s});y+=40*s;}
  else {xuiWidgetSetRect(v->labels[i],(xui_rect_t){0,y,w-20*s,24*s});y+=26*s;float iw=fields[i].kind==TEXT ? fmaxf(40*s,w-108*s) : fminf(180*s,w-20*s);xuiWidgetSetRect(v->controls[i],(xui_rect_t){0,y,iw,32*s});if(v->browse[i])xuiWidgetSetRect(v->browse[i],(xui_rect_t){iw+8*s,y,80*s,32*s});y+=46*s;}
 }
 float previewY=y+8*s,bodyWidth=fmaxf(1,w-20*s);
 int stacked=bodyWidth<340*s;
 xuiWidgetSetRect(v->preview,(xui_rect_t){0,previewY,stacked ? bodyWidth:fmaxf(1,bodyWidth-124*s),40*s});
 xuiWidgetSetRect(v->previewButton,(xui_rect_t){stacked ? 0:bodyWidth-116*s,previewY+(stacked ? 44*s:0),fminf(bodyWidth,116*s),34*s});
 xuiWidgetSetVisible(v->preview,!*q && v->selected!=2 && v->selected!=4);xuiWidgetSetVisible(v->previewButton,!*q && v->selected!=2 && v->selected!=4);xuiWidgetSetEnabled(v->defaults,!*q);
 xuiWidgetSetRect(v->query,(xui_rect_t){0,previewY,fminf(bodyWidth,156*s),34*s});xuiWidgetSetVisible(v->query,v->selected==2);xuiWidgetSetEnabled(v->query,v->request==NULL);
 const char* defaultTitle=v->selected==2 ? "使用内置编译器" : "恢复本页默认值";if(strcmp(xuiButtonGetText(v->defaults),defaultTitle))xuiButtonSetText(v->defaults,defaultTitle);
 float messageY=previewY+(!*q && v->selected!=4 ? (stacked && v->selected!=2 ? 86:46)*s:0);
 xuiWidgetSetRect(v->message,(xui_rect_t){0,messageY,bodyWidth,64*s});
 xuiWidgetSetPreferredSize(v->body,(xui_vec2_t){bodyWidth,messageY+64*s});
 xuiScrollViewSetContentSize(v->scroll,bodyWidth,messageY+64*s);return XUI_OK;
}
void xideSettingsViewLayout(xide_app* a)
{
 xide_settings_view* v=a->settingsView;if(!v)return;
 xideDialogSetMaxSize(v->window,a->viewportWidth-32,a->viewportHeight-32);
}
int xideSettingsViewOpen(xide_app* a,int page)
{
 if(!a || !a->settingsView || xideActionsHasModal(a))return 0;
 xide_settings_view* v=a->settingsView;v->selected=page>=0 && page<5 ? page : 0;v->dirtyNotice=0;previewClear(a);populate(a);xuiEditSetText(v->search,"");
 message(v,"修改先进入草稿；点击应用后统一保存。项目构建参数仍在项目菜单中设置。");xideSettingsViewLayout(a);xideDialogSetOpen(v->window,1);xuiSetFocusWidget(a->context,v->search);xgeRenderRequest();return 1;
}
void xideSettingsViewPoll(xide_app* a)
{
 xide_settings_view* v=a->settingsView;if(!v)return;if(v->dirtyNotice){v->dirtyNotice=0;message(v,"有尚未应用的修改；点击应用保存，取消将放弃这些修改。");}if(!v->request)return;char error[512]={0};enum xide_toolchain_request_state state=xideToolchainRequestState(v->request,error,sizeof(error));if(state==XIDE_TOOL_QUERY_RUNNING)return;
 if(state==XIDE_TOOL_QUERY_READY && xideToolchainRequestResult(v->request,&v->profile)){char text[1024];snprintf(text,sizeof(text),"工具链：%s；xlc.dll：%s；xl.dll：%s。能力已在后台查询；此结果仅供查看，不保存路径草稿。",*v->profile.fileVersion ? v->profile.fileVersion : "未提供版本",v->profile.stamp.present[1] ? "存在" : "未发现",v->profile.stamp.present[2] ? "存在" : "未发现");message(v,text);}else message(v,*error ? error : "无法读取工具链信息。");
 xideToolchainRequestDestroy(v->request);v->request=NULL;xideSettingsViewLayout(a);xgeRenderRequest();
}
void xideSettingsViewStop(xide_app* a)
{if(a && a->settingsView){xideToolchainRequestDestroy(a->settingsView->request);a->settingsView->request=NULL;}}
void xideSettingsViewDestroy(xide_app* a)
{if(a && a->settingsView){xide_settings_view* v=a->settingsView;xideToolchainRequestDestroy(v->request);xideToolchainProfileClear(&v->profile);if(v->previewFont)a->proxy.fontDestroy(&a->proxy,v->previewFont);free(v);a->settingsView=NULL;}}
int xideSettingsViewIsOpen(xide_app* a) {return a && a->settingsView && xideDialogIsOpen(a->settingsView->window);}
xui_widget xideSettingsViewWindow(xide_app* a) {return a && a->settingsView ? a->settingsView->window : NULL;}
xui_widget xideSettingsViewControl(xide_app* a,int id)
{xide_settings_view* v=a ? a->settingsView : NULL;if(!v)return NULL;if(id>=0 && id<XIDE_PREF_COUNT)return v->controls[id];switch(id){case XIDE_PREF_SEARCH:return v->search;case XIDE_PREF_APPLY:return v->apply;case XIDE_PREF_OK:return v->okay;case XIDE_PREF_CANCEL:return v->cancel;case XIDE_PREF_DEFAULTS:return v->defaults;case XIDE_PREF_PREVIEW:return v->preview;case XIDE_PREF_QUERY:return v->query;default:return NULL;}}
const char* xideSettingsViewMessage(xide_app* a) {return a && a->settingsView ? a->settingsView->text : "";}
int xideSettingsViewAction(xide_app* a,int id)
{
 if(id==XIDE_ACTION_SETTINGS_FILE){char* path=a->preferences.path ? xidePathResolve(NULL,a->preferences.path) : xideSettingsDefaultPath();int okay=path && xideWorkspaceOpenFile(a,path);free(path);return okay;}
 int page=id==XIDE_ACTION_COMPILER || id==XIDE_ACTION_SETTINGS_COMPILER ? 2 : id==XIDE_ACTION_UI_FONT || id==XIDE_ACTION_SETTINGS_APPEARANCE ? 3 : id==XIDE_ACTION_SETTINGS_DOCUMENTS ? 1 : id==XIDE_ACTION_SETTINGS_LANGUAGE ? 4 : 0;
 return xideSettingsViewOpen(a,page);
}
