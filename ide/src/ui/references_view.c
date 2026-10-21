#include "ui/toolchain_view.h"
#include "ui/catalog_view.h"
#include "ui/task_view.h"
#include "services/library_info.h"
#include "ui/resource_view.h"
#include "ui/import_view.h"
#include "ui/template_view.h"
#include "ui/rename_view.h"
#include "ui/references_view.h"
#include "ui/workspace_view.h"
#include "ui/configuration_view.h"
#include "ui/font_view.h"
#include "ui/recovery_view.h"
#include "ui/file_changes_view.h"
#include "services/ui_font.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define TRY(call) do { int r_=(call); if(r_!=XUI_OK) return r_; } while(0)
struct xide_references_view {
    xui_widget window,content,controls[XIDE_REF_CONTROL_COUNT],identity,message,libraryDetails;
    xide_ui_layout layout; float scale;
    xvalue *draft,*baseline;
    char *project,*manifest,**rows; size_t rowCount;
    int category,sync,suspended; char messageText[512]; char* libraryText;
};
static const char* fileKinds[]={"xlang","c","header","resource-config","resource","text"};
static const char* fileTitles[]={"xlang 源码","C 源码","头文件","编译资源配置","普通资源","文本","保留原文件种类"};
static const char* libraryTitles[]={"包引用","保留原库种类"};
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* v,const char* key) { return v ? xrtValueObjectGet(v,sv(key)) : NULL; }
static const char* text(const xvalue* v) { xstrview s={0}; return v && xrtValueGetString(v,&s) ? s.Data : ""; }
static char* copy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static int put(xvalue* v,const char* key,const char* s) { return xrtValueObjectSetNew(v,sv(key),xrtValueString(sv(s))); }
static xvalue* list(xide_references_view* v) { return get(v->draft,v->category ? "libraries" : "files"); }
static void message(xide_references_view* v,const char* s) { snprintf(v->messageText,sizeof(v->messageText),"%s",s); xuiLabelSetText(v->message,v->messageText); xgeRenderRequest(); }
static void clearRows(xide_references_view* v) { for(size_t i=0;i<v->rowCount;++i) free(v->rows[i]); free(v->rows); v->rows=NULL; v->rowCount=0; }
static int entryItem(xide_app* a,const xvalue* item)
{
    xide_references_view* v=a->referencesView; xide_project* p=xideWorkspaceFindProject(&a->workspaceModel,v->project);
    char* base=p ? xidePathDirectory(p->manifest) : NULL; char* path=base ? xidePathResolve(base,text(get(item,"path"))) : NULL;
    int entry=path && xidePathEqual(path,p->entry); free(path); free(base); return entry;
}
static void resetLibrary(xide_references_view* v) { free(v->libraryText); v->libraryText=NULL; xuiLabelSetText(v->libraryDetails,"选择包引用，点击检查显示候选清单、版本和声明文件状态。"); }
static void rowSelected(xui_widget w,int index,void* user)
{
    (void)w; xide_app* a=user; xide_references_view* v=a->referencesView;
    xvalue* item=index>=0 ? xrtValueArrayGet(list(v),(size_t)index) : NULL;
    int selected=v->category ? (!strcmp(text(get(item,"kind")),"package") ? 0 : 1) : 6;
    if(!v->category) for(int i=0;i<6;++i) if(!strcmp(text(get(item,"kind")),fileKinds[i])) selected=i;
    v->sync=1; xuiComboBoxSetSelected(v->controls[XIDE_REF_KIND],item ? selected : 0); v->sync=0;
    xuiEditSetText(v->controls[XIDE_REF_VALUE],text(get(item,v->category ? "name" : "path")));
    char label[512];
    if(item) snprintf(label,sizeof(label),"稳定 ID：%s%s",*text(get(item,"id")) ? text(get(item,"id")) : "旧记录（应用时保留）",!v->category && entryItem(a,item) ? " · 项目入口" : "");
    else snprintf(label,sizeof(label),"选择引用以查看稳定 ID");
    xuiLabelSetText(v->identity,label); resetLibrary(v);
}
static int refresh(xide_app* a,int selected)
{
    xide_references_view* v=a->referencesView; xvalue* values=list(v); size_t count=xrtValueCount(values); if(count>INT_MAX) return 0;
    char** rows=calloc(count+1,sizeof(*rows)); if(!rows) return 0;
    for(size_t i=0;i<count;++i) {
        xvalue* item=xrtValueArrayGet(values,i); const char *kind=text(get(item,"kind")),*value=text(get(item,v->category ? "name" : "path"));
        size_t n=strlen(kind)+strlen(value)+64; rows[i]=malloc(n);
        if(!rows[i]) { for(size_t j=0;j<i;++j) free(rows[j]); free(rows); return 0; }
        snprintf(rows[i],n,"[%s] %s%s",kind,value,!v->category && entryItem(a,item) ? " [入口]" : "");
    }
    if(xuiListViewSetItems(v->controls[XIDE_REF_LIST],(const char**)rows,(int)count)!=XUI_OK) { for(size_t i=0;i<count;++i) free(rows[i]); free(rows); return 0; }
    clearRows(v); v->rows=rows; v->rowCount=count;
    if(selected>=(int)count) selected=(int)count-1;
    xuiListViewSetSelected(v->controls[XIDE_REF_LIST],selected); rowSelected(NULL,selected,a);
    xuiWidgetSetVisible(v->controls[XIDE_REF_BROWSE],!v->category);
    xuiWidgetSetVisible(v->controls[XIDE_REF_INSPECT],v->category); xuiWidgetSetVisible(v->controls[XIDE_REF_DOCUMENTATION],v->category); xuiWidgetSetVisible(v->libraryDetails,v->category);
    return 1;
}
static void categorySelected(xui_widget w,int index,int value,void* user)
{
    (void)w; (void)value; xide_app* a=user; xide_references_view* v=a->referencesView; if(v->sync || index<0 || index>1) return;
    v->category=index; v->sync=1; xuiComboBoxSetItems(v->controls[XIDE_REF_KIND],index ? libraryTitles : fileTitles,index ? 2 : 7); v->sync=0;
    refresh(a,-1); message(v,index ? "编辑后按添加或更新，再应用。包引用与源码 import、配置中的链接库保持独立。" : "编辑后按添加或更新，再应用。路径相对于项目清单；仅资源配置进入构建。移除只删除引用。");
}
static int pendingFields(xide_references_view* v)
{
    int index=xuiListViewGetSelected(v->controls[XIDE_REF_LIST]); xvalue* item=index>=0 ? xrtValueArrayGet(list(v),(size_t)index) : NULL;
    const char* value=xuiEditGetText(v->controls[XIDE_REF_VALUE]); if(!item) return *value!=0;
    int k=xuiComboBoxGetSelected(v->controls[XIDE_REF_KIND]);
    const char* kind=v->category ? (k==0 ? "package" : k==1 ? text(get(item,"kind")) : "") : (k>=0 && k<6 ? fileKinds[k] : k==6 ? text(get(item,"kind")) : "");
    return strcmp(value,text(get(item,v->category ? "name" : "path"))) || strcmp(kind,text(get(item,"kind")));
}
static int fail(xide_model_error* e,int code,const char* s) { if(e) { e->code=code; snprintf(e->message,sizeof(e->message),"%s",s); } return 0; }
int xideReferencesViewApply(xide_app* a,xide_model_error* error)
{
    xide_references_view* v=a->referencesView; if(!v || !v->draft || !xideReferencesViewIsOpen(a)) return fail(error,XIDE_MODEL_INVALID,"No reference draft is open.");
    xide_project* p=xideWorkspaceFindProject(&a->workspaceModel,v->project); xvalue* now=p ? xideProjectExport(p,error) : NULL;
    int same=p && !strcmp(p->manifest,v->manifest) && now && xrtValueEqual(now,v->baseline); xrtValueRelease(now);
    if(!same) return fail(error,XIDE_MODEL_CONFLICT,"Project changed while editing references; cancel and reopen this editor.");
    if(pendingFields(v)) return fail(error,XIDE_MODEL_CONFLICT,"引用字段尚未加入列表。请先点击添加或更新，再应用；当前输入已保留。");
    if(!xideWorkspaceViewApplyReferences(a,v->project,v->draft,error)) return 0;
    v->suspended=0; xideDialogSetOpen(v->window,0); xideActionsRefresh(a); xgeRenderRequest(); return 1;
}
static void referenceSelected(xide_app* a,const char* path,void* user)
{ if(path && a->referencesView) xuiEditSetText(a->referencesView->controls[XIDE_REF_VALUE],path); }
static void browse(xide_app* a)
{ xideFileDialogChoose(a,a->referencesView->window,XUI_FILE_DIALOG_MODE_OPEN_FILE,"选择工程文件","所有文件|*.*",NULL,NULL,referenceSelected,a); }
static char* packageEnvironment(void)
{
    for(int attempt=0;attempt<3;++attempt) {
        DWORD n=GetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",NULL,0); if(!n || n>32768) return NULL;
        wchar_t* wide=calloc(n,sizeof(*wide)); if(!wide) return NULL;
        DWORD used=GetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",wide,n);
        if(!used || used>=n) { free(wide); continue; }
        int bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide,-1,NULL,0,NULL,NULL); char* value=bytes ? malloc((size_t)bytes) : NULL;
        if(value && !WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide,-1,value,bytes,NULL,NULL)) { free(value); value=NULL; }
        free(wide); return value;
    }
    return NULL;
}
static void inspectLibrary(xide_app* a)
{
    xide_references_view* v=a->referencesView; int index=xuiListViewGetSelected(v->controls[XIDE_REF_LIST]);
    xvalue* item=v->category && index>=0 ? xrtValueArrayGet(list(v),(size_t)index) : NULL;
    if(!item) { message(v,"选择一个包或库引用后再检查。"); return; }
    if(pendingFields(v)) { message(v,"先添加或更新当前字段，再检查库；输入已经保留。"); return; }
    xide_project* project=xideWorkspaceFindProject(&a->workspaceModel,v->project); xide_library_info info={0}; xide_model_error error={0};
    char* env=packageEnvironment(); const char* name=text(get(item,"name"));
    if(!project || strcmp(project->manifest,v->manifest) || !xideLibraryInspect(&info,project,text(get(item,"kind")),name,xideTaskViewCompiler(a),env,&error)) { message(v,*error.message ? error.message : "项目位置已改变，请取消并重新打开引用编辑。"); free(env); return; }
    free(env); size_t ci=xideLibraryCatalogEntry(&a->catalog,name); const xide_command* command=ci!=SIZE_MAX ? &a->catalog.entries[ci] : NULL;
    size_t size=strlen(info.message)+strlen(info.manifest ? info.manifest : "")+strlen(info.version ? info.version : "")+strlen(info.abiVersion ? info.abiVersion : "")+strlen(command ? command->version : "")+384;
    char* detail=malloc(size);
    if(detail) {
        snprintf(detail,size,"%s\n候选清单：%s\n磁盘版本：%s · ABI：%s\n命令目录版本：%s%s\n文档和命令目录为静态快照；检查不会修改 import、配置或引用。",info.message,info.manifest ? info.manifest : "未发现",info.version ? info.version : "未知",info.abiVersion ? info.abiVersion : "未知",command ? command->version : "未收录",command && info.version && strcmp(command->version,info.version) ? "（与磁盘版本不同）" : "");
        free(v->libraryText); v->libraryText=detail; xuiLabelSetText(v->libraryDetails,detail); message(v,"库检查完成；结果仅描述当前磁盘候选。真实构建会确认递归依赖与链接能力。");
    } else message(v,"无法分配库检查结果。");
    xideLibraryInfoClear(&info); xideReferencesViewLayout(a); xgeRenderRequest();
}
static void documentation(xide_app* a)
{
    xide_references_view* v=a->referencesView; int index=xuiListViewGetSelected(v->controls[XIDE_REF_LIST]);
    xvalue* item=v->category && index>=0 ? xrtValueArrayGet(list(v),(size_t)index) : NULL;
    if(!item || strcmp(text(get(item,"kind")),"package")) { message(v,"选择包引用以查阅命令帮助；其他库种类尚无文档适配器。"); return; }
    if(pendingFields(v)) { message(v,"先添加或更新当前字段，再查看包文档；输入已经保留。"); return; }
    size_t ci=xideLibraryCatalogEntry(&a->catalog,text(get(item,"name")));
    if(ci==SIZE_MAX) { message(v,"命令目录尚未收录此包。引用草稿保留，可用性请按实际构建确认。"); return; }
    v->suspended=1; xideDialogSetOpen(v->window,0);
    if(xideCatalogViewSearch(a,text(get(item,"name")))!=XUI_OK || !xideCatalogViewSelect(a,ci)) {
        v->suspended=0; xideDialogSetOpen(v->window,1); message(v,"无法打开包帮助，引用草稿已保留。"); return;
    }
    xideWorkbenchShowHelp(a,(int)ci); xideWorkbenchLayout(a); xideActionsRefresh(a); xgeRenderRequest();
}
static void clicked(xui_widget w,void* user)
{
    xide_app* a=user; xide_references_view* v=a->referencesView; int action=-1;
    for(int i=0;i<XIDE_REF_CONTROL_COUNT;++i) if(v->controls[i]==w) action=i;
    if(action==XIDE_REF_CANCEL) { v->suspended=0; xideDialogSetOpen(v->window,0); xideActionsRefresh(a); return; }
    if(action==XIDE_REF_APPLY) { xide_model_error e={0}; if(!xideReferencesViewApply(a,&e)) message(v,*e.message ? e.message : "无法应用引用。"); return; }
    if(action==XIDE_REF_BROWSE) { browse(a); return; }
    if(action==XIDE_REF_INSPECT) { inspectLibrary(a); return; }
    if(action==XIDE_REF_DOCUMENTATION) { documentation(a); return; }
    xvalue* values=list(v); int index=xuiListViewGetSelected(v->controls[XIDE_REF_LIST]);
    xvalue* previous=index>=0 ? xrtValueArrayGet(values,(size_t)index) : NULL;
    if(action!=XIDE_REF_ADD && !previous) { message(v,"请先选择要修改的引用。"); return; }
    int okay=0;
    if(action==XIDE_REF_ADD || action==XIDE_REF_UPDATE) {
        const char* value=xuiEditGetText(v->controls[XIDE_REF_VALUE]); int k=xuiComboBoxGetSelected(v->controls[XIDE_REF_KIND]);
        if(action==XIDE_REF_ADD && (v->category ? k!=0 : k<0 || k>=6)) { message(v,"新建引用请选择明确的文件种类或包类型；保留原种类用于更新现有记录。"); return; }
        const char* kind=v->category ? (k==0 ? "package" : text(get(previous,"kind"))) : (k>=0 && k<6 ? fileKinds[k] : text(get(previous,"kind")));
        if(!*value || !*kind) { message(v,"填写路径或库名称，并选择有效种类。新建库引用使用包类型。"); return; }
        xvalue* item=action==XIDE_REF_UPDATE ? xrtValueDeepClone(previous) : xrtValueObject(); char* id=action==XIDE_REF_ADD ? xideCreateId(v->category ? "library" : "file") : NULL;
        okay=item && (action!=XIDE_REF_ADD || (id && put(item,"id",id))) && put(item,"kind",kind) && put(item,v->category ? "name" : "path",value); free(id);
        if(okay) okay=action==XIDE_REF_ADD ? xrtValueArrayAppendNew(values,item) : xrtValueArraySetNew(values,(size_t)index,item); else xrtValueRelease(item);
        if(action==XIDE_REF_ADD) index=(int)xrtValueCount(values)-1;
    } else if(action==XIDE_REF_REMOVE) {
        if(!v->category && entryItem(a,previous)) { message(v,"入口引用必须保留；请先在项目属性中更换入口。"); return; }
        okay=xrtValueArrayRemove(values,(size_t)index,1);
    } else if(action==XIDE_REF_UP || action==XIDE_REF_DOWN) {
        int next=index+(action==XIDE_REF_UP ? -1 : 1); if(next<0 || (size_t)next>=xrtValueCount(values)) return;
        okay=xrtValueArraySwap(values,(size_t)index,(size_t)next); if(okay) index=next;
    }
    if(!okay || !refresh(a,index)) message(v,"无法更新引用草稿。"); else message(v,"草稿已更新。点击应用后，再保存项目；尚未改动源文件。");
}
static int label(xide_app* a,xui_widget* out,const char* s,xui_rect_t r)
{
    xui_label_desc_t d={0}; d.iSize=sizeof(d); d.pFont=a->uiFont; d.sText=s; d.iWrapMode=XUI_TEXT_WRAP_WORD; xui_widget w;
    TRY(xuiLabelCreate(a->context,&w,&d)); TRY(xuiWidgetSetRect(w,r)); TRY(xuiWidgetAddChild(a->referencesView->content,w)); if(out) *out=w; return XUI_OK;
}
static int button(xide_app* a,int id,const char* s,xui_rect_t r)
{
    xui_button_desc_t d={0}; d.iSize=sizeof(d); d.pFont=a->uiFont; d.sText=s; xui_widget* w=&a->referencesView->controls[id];
    TRY(xuiButtonCreate(a->context,w,&d)); TRY(xuiWidgetSetRect(*w,r)); TRY(xuiButtonSetClick(*w,clicked,a)); return xuiWidgetAddChild(a->referencesView->content,*w);
}
static void windowClosed(xui_widget widget,void* user) { xide_app* a=user; clicked(a->referencesView->controls[XIDE_REF_CANCEL],a); }
int xideReferencesViewCreate(xide_app* a)
{
    xide_references_view* v=calloc(1,sizeof(*v)); if(!v) return XUI_ERROR_OUT_OF_MEMORY; a->referencesView=v;
    TRY(xideDialogCreate(a,&v->window,"项目文件、资源与库引用",736,814,14)); TRY(xideDialogSetCancel(v->window,windowClosed,a)); v->content=xideDialogGetContentWidget(v->window); TRY(xuiWidgetAddChild(a->root,v->window)); TRY(xuiWidgetSetLayoutType(v->content,XUI_LAYOUT_MANUAL));
    TRY(label(a,NULL,"项目文件、资源与库引用",(xui_rect_t){0,0,736,26}));
    static const char* categories[]={"工程文件与资源","包与库引用"}; xui_combobox_desc_t combo={0}; combo.iSize=sizeof(combo); combo.pFont=a->uiFont; combo.arrItems=categories; combo.iItemCount=2;
    TRY(xuiComboBoxCreate(a->context,&v->controls[XIDE_REF_CATEGORY],&combo)); TRY(xuiWidgetSetRect(v->controls[XIDE_REF_CATEGORY],(xui_rect_t){0,34,360,32})); TRY(xuiWidgetAddChild(v->content,v->controls[XIDE_REF_CATEGORY])); TRY(xuiComboBoxSetSelect(v->controls[XIDE_REF_CATEGORY],categorySelected,a));
    TRY(label(a,NULL,"取消保留原模型；应用后需保存项目。",(xui_rect_t){376,34,360,32}));
    xui_list_view_desc_t listDesc={0}; listDesc.iSize=sizeof(listDesc); listDesc.pFont=a->uiFont; listDesc.fItemHeight=28; listDesc.fPadding=6;
    TRY(xuiListViewCreate(a->context,&v->controls[XIDE_REF_LIST],&listDesc)); TRY(xuiWidgetSetRect(v->controls[XIDE_REF_LIST],(xui_rect_t){0,76,736,212})); TRY(xuiWidgetAddChild(v->content,v->controls[XIDE_REF_LIST])); TRY(xuiListViewSetSelect(v->controls[XIDE_REF_LIST],rowSelected,a));
    combo.arrItems=fileTitles; combo.iItemCount=7; TRY(xuiComboBoxCreate(a->context,&v->controls[XIDE_REF_KIND],&combo)); TRY(xuiWidgetSetRect(v->controls[XIDE_REF_KIND],(xui_rect_t){0,300,260,32})); TRY(xuiWidgetAddChild(v->content,v->controls[XIDE_REF_KIND]));
    TRY(label(a,&v->identity,"稳定 ID",(xui_rect_t){272,300,464,32}));
    xui_input_desc_t input={0}; input.iSize=sizeof(input); input.pFont=a->uiFont; input.sPlaceholder="工程路径或包名称";
    TRY(xuiInputCreate(a->context,&v->controls[XIDE_REF_VALUE],&input)); TRY(xuiWidgetSetRect(v->controls[XIDE_REF_VALUE],(xui_rect_t){0,342,628,32})); TRY(xuiWidgetAddChild(v->content,v->controls[XIDE_REF_VALUE])); TRY(xuiWidgetSetAccessibleName(v->controls[XIDE_REF_VALUE],"引用路径或名称"));
    TRY(button(a,XIDE_REF_BROWSE,"选择文件…",(xui_rect_t){638,342,98,32}));
    const char* titles[]={"添加","更新","移除引用","上移","下移"}; for(int i=0;i<5;++i) TRY(button(a,XIDE_REF_ADD+i,titles[i],(xui_rect_t){i*150,388,136,32}));
    TRY(button(a,XIDE_REF_INSPECT,"检查库信息",(xui_rect_t){0,434,180,32})); TRY(button(a,XIDE_REF_DOCUMENTATION,"查看包命令与文档…",(xui_rect_t){194,434,250,32}));
    TRY(label(a,&v->libraryDetails,"",(xui_rect_t){0,478,736,244}));
    TRY(label(a,&v->message,"",(xui_rect_t){0,734,736,28})); TRY(button(a,XIDE_REF_APPLY,"应用",(xui_rect_t){520,774,100,32})); TRY(button(a,XIDE_REF_CANCEL,"取消",(xui_rect_t){636,774,100,32}));
    return xideUiLayoutCapture(&v->layout,v->content);
}
int xideReferencesViewOpen(xide_app* a)
{
    if(xideReferencesViewIsOpen(a)) return 1;
    xide_references_view* v=a->referencesView; xide_project* p=xideWorkspaceFindProject(&a->workspaceModel,a->selectedProject);
    if(!v || xideToolchainViewIsOpen(a) || xideResourceViewIsOpen(a) || xideImportViewIsOpen(a) || xideRenameViewIsOpen(a) || xideTemplateViewIsOpen(a) || xideConfigurationViewIsOpen(a) || xideFontViewIsOpen(a) || xideRecoveryViewIsOpen(a) || xideFileChangesViewIsOpen(a)) return 0;
    if(v->suspended) {
        v->suspended=0; xideReferencesViewLayout(a); xideDialogSetOpen(v->window,1); xideActionsRefresh(a);
        xuiSetFocusWidget(a->context,v->controls[XIDE_REF_VALUE]); message(v,"已返回保留的引用草稿。查看帮助期间的项目变化仍由应用时的基线检查处理。"); xgeRenderRequest(); return 1;
    }
    if(!p || !p->available || xideWorkspaceViewHasDraft(a)) return 0;
    xvalue *draft=xideProjectReferencesJson(p),*baseline=xideProjectExport(p,NULL); char *id=copy(p->id),*manifest=copy(p->manifest);
    if(!draft || !baseline || !id || !manifest) { xrtValueRelease(draft); xrtValueRelease(baseline); free(id); free(manifest); return 0; }
    xrtValueRelease(v->draft); xrtValueRelease(v->baseline); free(v->project); free(v->manifest); v->draft=draft; v->baseline=baseline; v->project=id; v->manifest=manifest;
    v->sync=1; xuiComboBoxSetSelected(v->controls[XIDE_REF_CATEGORY],0); v->sync=0; categorySelected(NULL,0,0,a);
    xideReferencesViewLayout(a); xuiScrollViewSetOffset(xideDialogGetScrollViewWidget(v->window),0,0); xideDialogSetOpen(v->window,1); xideActionsRefresh(a); xuiSetFocusWidget(a->context,v->controls[XIDE_REF_CATEGORY]); xgeRenderRequest(); return 1;
}
int xideReferencesViewIsOpen(xide_app* a) { return a && a->referencesView && xideDialogIsOpen(a->referencesView->window); }
void xideReferencesViewLayout(xide_app* a)
{
    xide_references_view* v=a->referencesView; if(!v) return;
    int width=a->viewportWidth-48,height=a->viewportHeight-48; if(width<200) width=200; if(height<200) height=200;
    xideDialogSetMaxSize(v->window,(float)width,(float)height); float scale=xideUiFontScale(a);
    if(v->scale!=scale) { xideUiLayoutApply(a,&v->layout,1); xideDialogSetContentSize(v->window,xideUiHeight(a,736),xideUiHeight(a,814)); v->scale=scale; }
    float contentHeight=(float)xideUiHeight(a,554),messageY=(float)xideUiHeight(a,434);
    if(v->category) {
        xui_vec2_t measured={0}; xuiWidgetMeasureContent(v->libraryDetails,(xui_vec2_t){xideUiHeight(a,736),XUI_LAYOUT_UNBOUNDED},&measured);
        float detailsHeight=measured.fY+12; if(detailsHeight<xideUiHeight(a,140)) detailsHeight=(float)xideUiHeight(a,140);
        xuiWidgetSetRect(v->libraryDetails,(xui_rect_t){0,xideUiHeight(a,478),xideUiHeight(a,736),detailsHeight}); messageY=xideUiHeight(a,478)+detailsHeight+12;
    }
    xui_vec2_t notice={0}; xuiWidgetMeasureContent(v->message,(xui_vec2_t){xideUiHeight(a,736),XUI_LAYOUT_UNBOUNDED},&notice);
    float noticeHeight=notice.fY+8; if(noticeHeight<xideUiHeight(a,60)) noticeHeight=(float)xideUiHeight(a,60);
    float footer=messageY+noticeHeight+12; contentHeight=footer+xideUiHeight(a,40);
    xuiWidgetSetRect(v->message,(xui_rect_t){0,messageY,xideUiHeight(a,736),noticeHeight});
    xuiWidgetSetRect(v->controls[XIDE_REF_APPLY],(xui_rect_t){xideUiHeight(a,520),footer,xideUiHeight(a,100),xideUiHeight(a,32)});
    xuiWidgetSetRect(v->controls[XIDE_REF_CANCEL],(xui_rect_t){xideUiHeight(a,636),footer,xideUiHeight(a,100),xideUiHeight(a,32)});
    xideDialogSetContentSize(v->window,xideUiHeight(a,736),contentHeight);

}
void xideReferencesViewDestroy(xide_app* a) { xide_references_view* v=a->referencesView; if(!v) return; clearRows(v); xrtValueRelease(v->draft); xrtValueRelease(v->baseline); free(v->project); free(v->manifest); free(v->libraryText); free(v); a->referencesView=NULL; }
xui_widget xideReferencesViewWindow(xide_app* a) { return a->referencesView ? a->referencesView->window : NULL; }
xui_widget xideReferencesViewControl(xide_app* a,int i) { return a->referencesView && i>=0 && i<XIDE_REF_CONTROL_COUNT ? a->referencesView->controls[i] : NULL; }
const char* xideReferencesViewMessage(xide_app* a) { return a->referencesView ? a->referencesView->messageText : ""; }

int xideReferencesViewSuspended(xide_app* a) { return a && a->referencesView && a->referencesView->suspended; }
const char* xideReferencesViewLibraryDetails(xide_app* a) { return a && a->referencesView && a->referencesView->libraryText ? a->referencesView->libraryText : ""; }
