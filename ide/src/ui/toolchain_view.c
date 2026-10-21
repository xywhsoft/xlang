#include "ui/toolchain_view.h"
#include "ui/task_view.h"
#include "services/ui_font.h"
#include "platform/path.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define TRY(call) do { int r_=(call); if(r_!=XUI_OK) return r_; } while(0)
struct xide_toolchain_view {
 xui_widget window,content,controls[XIDE_TOOL_CONTROL_COUNT],message;
 xide_ui_layout layout; float scale; xide_toolchain_request* request;
 xide_toolchain_profile profile; char *compiler,*directory; char messageText[768];
};
static void message(xide_toolchain_view* v,const char* text)
{ snprintf(v->messageText,sizeof(v->messageText),"%s",text); xuiLabelSetText(v->message,v->messageText); xgeRenderRequest(); }
static void buttons(xide_toolchain_view* v)
{
 xuiWidgetSetEnabled(v->controls[XIDE_TOOL_REFRESH],v->request==NULL); xuiWidgetSetEnabled(v->controls[XIDE_TOOL_CANCEL],v->request!=NULL);
 const uint32_t states[]={XUI_WIDGET_STATE_DISABLED,XUI_WIDGET_STATE_ACTIVE,XUI_WIDGET_STATE_HOVER,XUI_WIDGET_STATE_FOCUS};
 for(int i=XIDE_TOOL_REFRESH;i<=XIDE_TOOL_CLOSE;++i) {
  xui_widget w=v->controls[i]; uint32_t input=xuiWidgetGetInputState(w),visual=0;
  for(unsigned j=0;j<sizeof(states)/sizeof(*states);++j) if(input&states[j]) { visual=states[j]; break; }
  if(xuiWidgetGetStateId(w)!=visual) xuiWidgetSetStateId(w,visual);
 }
}
static void digest(const xide_file_snapshot* s,char out[65])
{ for(unsigned i=0;i<32;++i) snprintf(out+i*2,3,"%02x",s->sha256[i]); out[64]=0; }
static void describe(xide_toolchain_view* v)
{
 xide_toolchain_profile* p=&v->profile; char text[4096],hash[3][65];
 for(int i=0;i<3;++i) digest(&p->stamp.files[i],hash[i]);
 FILETIME utc={(DWORD)p->observedAt,(DWORD)(p->observedAt>>32)},local; SYSTEMTIME at={0}; if(FileTimeToLocalFileTime(&utc,&local)) FileTimeToSystemTime(&local,&at);
 size_t n=(size_t)snprintf(text,sizeof(text),"编译器：%s\n工作目录：%s\nPE 文件版本：%s\n观察时间：%04u-%02u-%02u %02u:%02u:%02u（本地）；本次查询 %llu ms；%s\n\nCLI 声明的命令：",
  p->compiler,p->directory,*p->fileVersion ? p->fileVersion : "未提供",at.wYear,at.wMonth,at.wDay,at.wHour,at.wMinute,at.wSecond,(unsigned long long)p->durationMs,p->cached ? "使用已复核的缓存" : "实际执行 --help");
 const char* names[]={"build","run","check","lex","parse","ir","cgen","abi","impdef","c-bridge"};
 for(unsigned i=0;i<10 && n<sizeof(text);++i) if(p->commands&(1u<<i)) n+=(size_t)snprintf(text+n,sizeof(text)-n,"%s ",names[i]);
 if(!p->commands && n<sizeof(text)) n+=(size_t)snprintf(text+n,sizeof(text)-n,"未识别");
 if(n<sizeof(text)) n+=(size_t)snprintf(text+n,sizeof(text)-n,"\nbuild 行声明的选项：");
 const char* options[]={"-o","--inc-path","--lib-path","--link","--res","-shared","--save-temps","--emit-header","--backend"};
 for(unsigned i=0;i<9 && n<sizeof(text);++i) if(p->options&(1u<<i)) n+=(size_t)snprintf(text+n,sizeof(text)-n,"%s ",options[i]);
 if(n<sizeof(text)) n+=(size_t)snprintf(text+n,sizeof(text)-n,"\nbuild 实际输入记录：%s",p->options&XIDE_TOOL_BUILD_INPUT_REPORT ? "声明 --input-report" : "未声明；构建仅使用直接文件和资源守护");
 if(n<sizeof(text)) n+=(size_t)snprintf(text+n,sizeof(text)-n,"\ncheck --json：%s；abi --json：%s\n\n",p->options&XIDE_TOOL_CHECK_JSON ? "声明" : "未声明",p->options&XIDE_TOOL_ABI_JSON ? "声明" : "未声明");
 const char* files[]={"编译器","xlc.dll","xl.dll"};
 for(int i=0;i<3 && n<sizeof(text);++i) n+=(size_t)snprintf(text+n,sizeof(text)-n,"%s：%s；%llu 字节；SHA-256 %s\n",files[i],p->stamp.present[i] ? "存在" : "未发现",(unsigned long long)p->stamp.files[i].length,p->stamp.present[i] ? hash[i] : "—");
 if(n<sizeof(text)) snprintf(text+n,sizeof(text)-n,"\n这些能力来自帮助文本，尚未证明运行语义。缓存仅覆盖以上三个文件及工作目录；环境、包或其他依赖变化时请刷新。每次构建另行执行能力检查。");
 xuiTextEditSetText(v->controls[XIDE_TOOL_INFO],text); xuiTextEditSetText(v->controls[XIDE_TOOL_HELP],p->help);
}
static int query(xide_app* a,int force)
{
 xide_toolchain_view* v=a->toolchainView; if(v->request) return 0; xide_model_error e={0};
 v->request=xideToolchainRequest(xideTasksToolchains(a->tasks),v->compiler,v->directory,force,&e);
 if(!v->request) { message(v,e.message); buttons(v); return 0; }
 message(v,v->profile.compiler ? "正在后台查询；下方保留上次有效观察，完成后更新。" : "正在后台读取文件指纹并查询编译器帮助…"); buttons(v); return 1;
}
static void clicked(xui_widget widget,void* user)
{
 xide_app* a=user; xide_toolchain_view* v=a->toolchainView;
 if(widget==v->controls[XIDE_TOOL_REFRESH]) query(a,1);
 else if(widget==v->controls[XIDE_TOOL_CANCEL]) { xideToolchainRequestCancel(v->request); message(v,"正在取消查询；上次有效观察保持。"); }
 else if(widget==v->controls[XIDE_TOOL_CLOSE]) { xideToolchainRequestCancel(v->request); xideDialogSetOpen(v->window,0); }
 xgeRenderRequest();
}
static int label(xide_app* a,const char* text,xui_rect_t rect,xui_widget* out)
{
 xui_label_desc_t d={0}; d.iSize=sizeof(d); d.pFont=a->uiFont; d.sText=text; d.iWrapMode=XUI_TEXT_WRAP_WORD; xui_widget w=NULL;
 TRY(xuiLabelCreate(a->context,&w,&d)); TRY(xuiWidgetSetRect(w,rect)); TRY(xuiWidgetAddChild(a->toolchainView->content,w)); if(out) *out=w; return XUI_OK;
}
static int text(xide_app* a,int id,const char* name,xui_rect_t rect)
{
 xui_text_edit_desc_t d={0}; d.iSize=sizeof(d); d.pFont=a->uiFont; d.sText="尚未完成工具链查询。"; d.bReadonly=1; d.bWordWrap=1; d.iMaxLength=65536; xui_widget* w=&a->toolchainView->controls[id];
 TRY(xuiTextEditCreate(a->context,w,&d)); TRY(xuiWidgetSetRect(*w,rect)); TRY(xuiWidgetSetAccessibleName(*w,name)); return xuiWidgetAddChild(a->toolchainView->content,*w);
}
static int button(xide_app* a,int id,const char* name,xui_rect_t rect)
{
 xui_button_desc_t d={0}; d.iSize=sizeof(d); d.pFont=a->uiFont; d.sText=name; xui_widget* w=&a->toolchainView->controls[id];
 TRY(xuiButtonCreate(a->context,w,&d)); TRY(xuiWidgetSetRect(*w,rect)); TRY(xuiButtonSetClick(*w,clicked,a)); return xuiWidgetAddChild(a->toolchainView->content,*w);
}
static void windowClosed(xui_widget widget,void* user) { xide_app* a=user; clicked(a->toolchainView->controls[XIDE_TOOL_CLOSE],a); }
int xideToolchainViewCreate(xide_app* a)
{
 xide_toolchain_view* v=calloc(1,sizeof(*v)); if(!v) return XUI_ERROR_OUT_OF_MEMORY; a->toolchainView=v;
 TRY(xideDialogCreate(a,&v->window,"工具链信息",850,820,14)); TRY(xideDialogSetCancel(v->window,windowClosed,a)); v->content=xideDialogGetContentWidget(v->window); TRY(xuiWidgetAddChild(a->root,v->window)); TRY(xuiWidgetSetLayoutType(v->content,XUI_LAYOUT_MANUAL));
 TRY(label(a,"工具链信息",(xui_rect_t){0,0,850,28},NULL));
 TRY(text(a,XIDE_TOOL_INFO,"工具链版本、能力和文件指纹，只读",(xui_rect_t){0,40,850,300}));
 TRY(label(a,"原始 --help 输出",(xui_rect_t){0,352,850,28},NULL)); TRY(text(a,XIDE_TOOL_HELP,"原始编译器帮助，只读",(xui_rect_t){0,390,850,280}));
 TRY(label(a,"尚未查询。",(xui_rect_t){0,682,850,80},&v->message));
 TRY(button(a,XIDE_TOOL_REFRESH,"重新查询",(xui_rect_t){414,780,140,32})); TRY(button(a,XIDE_TOOL_CANCEL,"取消查询",(xui_rect_t){566,780,140,32})); TRY(button(a,XIDE_TOOL_CLOSE,"关闭",(xui_rect_t){718,780,132,32}));
 buttons(v); return xideUiLayoutCapture(&v->layout,v->content);
}
int xideToolchainViewCanOpen(xide_app* a)
{ const char* compiler=a ? xideTaskViewCompiler(a) : NULL; return a && a->toolchainView && a->tasks && !a->toolchainView->request && compiler && *compiler; }
int xideToolchainViewOpen(xide_app* a)
{
 if(!a || xideActionsHasModal(a)) return 0;
 xideToolchainViewPoll(a); if(!xideToolchainViewCanOpen(a)) return 0; xide_toolchain_view* v=a->toolchainView;
 const char* compiler=xideTaskViewCompiler(a); xide_project* project=xideWorkspaceFindProject(&a->workspaceModel,a->selectedProject);
 char *resolved=xidePathResolve(NULL,compiler),*directory=xidePathDirectory(project && project->available ? project->manifest : compiler);
 if(!resolved || !directory) { free(resolved); free(directory); message(v,"无法准备工具链路径。"); return 0; }
 if(!v->profile.compiler || !xidePathEqual(resolved,v->profile.compiler) || !xidePathEqual(directory,v->profile.directory)) { xideToolchainProfileClear(&v->profile); xuiTextEditSetText(v->controls[XIDE_TOOL_INFO],"尚未完成工具链查询。"); xuiTextEditSetText(v->controls[XIDE_TOOL_HELP],"尚未完成工具链查询。"); }
 free(v->compiler); free(v->directory); v->compiler=resolved; v->directory=directory;
 xideToolchainViewLayout(a); xideDialogSetOpen(v->window,1); query(a,0); xgeRenderRequest(); return 1;
}
void xideToolchainViewPoll(xide_app* a)
{
 xide_toolchain_view* v=a->toolchainView; if(!v) return; buttons(v); if(!v->request) return; char error[512]={0}; enum xide_toolchain_request_state state=xideToolchainRequestState(v->request,error,sizeof(error));
 if(state==XIDE_TOOL_QUERY_RUNNING) return;
 if(xideToolchainViewIsOpen(a)) {
  if(state==XIDE_TOOL_QUERY_READY && xideToolchainRequestResult(v->request,&v->profile)) { describe(v); message(v,v->profile.cached ? "已复核三个文件指纹并读取缓存；可重新查询以检查外部依赖变化。" : "工具链观察已更新。文件版本来自 PE 资源，能力来自本次 --help。"); }
  else { char detail[768]; snprintf(detail,sizeof(detail),"%s%s",*error ? error : "无法读取查询结果。",v->profile.compiler ? " 下方为上次有效观察；未更新。" : " 尚无有效工具链观察。"); message(v,detail); }
 }
 xideToolchainRequestDestroy(v->request); v->request=NULL; buttons(v); xgeRenderRequest();
}
int xideToolchainViewIsOpen(xide_app* a) { return a && a->toolchainView && xideDialogIsOpen(a->toolchainView->window); }
int xideToolchainViewBusy(xide_app* a) { return a && a->toolchainView && a->toolchainView->request; }
void xideToolchainViewLayout(xide_app* a)
{
 xide_toolchain_view* v=a->toolchainView; if(!v) return; int width=a->viewportWidth-48,height=a->viewportHeight-48; if(width<200) width=200; if(height<200) height=200;
 xideDialogSetMaxSize(v->window,width,height); float scale=xideUiFontScale(a);
 if(v->scale!=scale) { xideUiLayoutApply(a,&v->layout,1); xideDialogSetContentSize(v->window,xideUiHeight(a,850),xideUiHeight(a,820)); v->scale=scale; }
 
}
void xideToolchainViewStop(xide_app* a) { if(a->toolchainView) { xideToolchainRequestDestroy(a->toolchainView->request); a->toolchainView->request=NULL; } }
void xideToolchainViewDestroy(xide_app* a) { xide_toolchain_view* v=a->toolchainView; if(v) { xideToolchainViewStop(a); xideToolchainProfileClear(&v->profile); free(v->compiler); free(v->directory); free(v); a->toolchainView=NULL; } }
xui_widget xideToolchainViewWindow(xide_app* a) { return a && a->toolchainView ? a->toolchainView->window : NULL; }
xui_widget xideToolchainViewControl(xide_app* a,int id) { return a && a->toolchainView && id>=0 && id<XIDE_TOOL_CONTROL_COUNT ? a->toolchainView->controls[id] : NULL; }
const char* xideToolchainViewMessage(xide_app* a) { return a && a->toolchainView ? a->toolchainView->messageText : ""; }
