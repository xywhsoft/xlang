#include "ui/template_view.h"
#include "ui/rename_view.h"
#include "services/project_migration.h"
#include "ui/workspace_view.h"
#include "services/execution_target.h"
#include "ui/reading_state.h"
#include "services/ui_font.h"
#include "services/session.h"
#include "ui/editor_view.h"
#include "ui/configuration_view.h"
#include "ui/language_view.h"
#include "ui/completion_view.h"
#include "platform/path.h"
#include "lang/languages.h"
#include "theme/theme.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TRY(call) do { int result_=(call); if(result_!=XUI_OK) return result_; } while(0)
enum { NODE_WORKSPACE=1,NODE_PROJECT,NODE_GROUP,NODE_FILE,NODE_LIBRARY };
typedef struct workspace_node {
    int id,parent,depth,type,expanded;
    char *project,*file,*caption,*path;
} workspace_node;
struct xide_workspace_view {
    xide_ui_layout layout;
    workspace_node* nodes;
    size_t count,capacity;
    int nextId,selectedNode;
    xui_widget content,overview,form,name,description,kind,entry,apply,configuration,configure,references,resources,newResource;
    char** configurationNames; size_t configurationNameCount; int syncing;
};
static char* duplicate(const char* text)
{
    if(!text) text="";
    size_t size=strlen(text)+1; char* result=malloc(size); if(result) memcpy(result,text,size); return result;
}
static void report(xide_app* app,const char* text)
{
    xideMessageShow(app,"xlang IDE",text,XUI_MSGBOX_ICON_ERROR);
}
static char* chooseFile(xide_app* app,int save,int type)
{
    const char* filter=type==0 ? "xlang 工作空间|*.xlw|所有文件|*.*" : type==1 ? "xlang 项目|*.xlp|所有文件|*.*" : "源文件和资源|*.xl;*.c;*.h;*.json;*.xlon;*.txt|所有文件|*.*";
    const char* title=save ? (type==0 ? "保存工作空间" : "保存项目") : (type==0 ? "打开工作空间" : type==1 ? "选择项目" : "选择文件");
    return xideMessageChoosePath(app,save ? XUI_FILE_DIALOG_MODE_SAVE_FILE : XUI_FILE_DIALOG_MODE_OPEN_FILE,title,filter,
        save && type==0 ? app->workspaceModel.manifest : NULL,save ? (type==0 ? "xlw" : type==1 ? "xlp" : NULL) : NULL,
        UINT64_C(710)+(uint64_t)(save*3+type),0);
}
static void clearNodes(workspace_node* nodes,size_t count)
{
    for(size_t i=0;i<count;++i) { free(nodes[i].project); free(nodes[i].file); free(nodes[i].caption); free(nodes[i].path); }
    free(nodes);
}
void xideWorkspaceViewDestroy(xide_app* app)
{
    if(!app->workspaceView) return;
    for(size_t i=0;i<app->workspaceView->configurationNameCount;++i) free(app->workspaceView->configurationNames[i]);
    free(app->workspaceView->configurationNames);
    clearNodes(app->workspaceView->nodes,app->workspaceView->count); free(app->workspaceView); app->workspaceView=NULL;
}
static workspace_node* nodeAt(xide_workspace_view* view,int id)
{
    for(size_t i=0;i<view->count;++i) if(view->nodes[i].id==id) return &view->nodes[i];
    return NULL;
}
static int sameNode(const workspace_node* node,int type,const char* project,const char* file)
{
    return node->type==type && !strcmp(node->project,project ? project : "") && !strcmp(node->file,file ? file : "");
}
static int addNode(xide_app* app,xide_workspace_view* fresh,int type,int parent,int depth,const char* project,const char* file,const char* caption,const char* path)
{
    int id=type==NODE_WORKSPACE ? 1 : 0,expanded=type==NODE_PROJECT || type==NODE_GROUP || type==NODE_WORKSPACE;
    xide_workspace_view* old=app->workspaceView;
    for(size_t i=0;i<old->count;++i) if(sameNode(&old->nodes[i],type,project,file)) { id=old->nodes[i].id; expanded=xuiTreeViewGetNodeExpanded(app->workspace,id); break; }
    if(!id) { if(fresh->nextId==INT_MAX) return -1; id=fresh->nextId++; }
    workspace_node node={0}; node.id=id; node.parent=parent; node.depth=depth; node.type=type; node.expanded=expanded;
    node.project=duplicate(project); node.file=duplicate(file); node.caption=duplicate(caption); node.path=duplicate(path);
    if(!node.project || !node.file || !node.caption || !node.path) { free(node.project); free(node.file); free(node.caption); free(node.path); return -1; }
    if(fresh->count==fresh->capacity) {
        size_t size=fresh->capacity ? fresh->capacity*2 : 32; workspace_node* nodes=realloc(fresh->nodes,size*sizeof(*nodes));
        if(!nodes) { free(node.project); free(node.file); free(node.caption); free(node.path); return -1; }
        fresh->nodes=nodes; fresh->capacity=size;
    }
    fresh->nodes[fresh->count++]=node; return id;
}
static int groupKind(const char* kind)
{
    if(!strcmp(kind,"xlang")) return 0;
    if(!strcmp(kind,"c")) return 1;
    if(!strcmp(kind,"header")) return 2;
    if(!strcmp(kind,"resource") || !strcmp(kind,"resource-config")) return 3;
    return 4;
}
static char* caption(const char* text,int missing,int dirty,int startup,int entry)
{
    size_t size=strlen(text)+96; char* result=malloc(size);
    if(result) snprintf(result,size,"%s%s%s%s%s",missing ? "[缺失/不可用] " : "",text,entry ? " [入口]" : "",startup ? " [启动]" : "",dirty ? " *" : "");
    return result;
}
static xide_project* selectedProject(xide_app* app) { return xideWorkspaceFindProject(&app->workspaceModel,app->selectedProject); }
const xide_file* xideWorkspaceViewSelectedFile(xide_app* app)
{
 if(!app || !app->workspaceView) return NULL;
 workspace_node* node=nodeAt(app->workspaceView,app->workspaceView->selectedNode);
 if(!node || node->type!=NODE_FILE) return NULL;
 xide_project* p=*node->project ? xideWorkspaceFindProject(&app->workspaceModel,node->project) : NULL;
 if(*node->project && (!p || !p->available)) return NULL;
 xide_files* files=p ? &p->files : &app->workspaceModel.files;
 for(size_t i=0;i<files->count;++i) if(!strcmp(files->items[i].id,node->file)) return &files->items[i];
 return NULL;
}
int xideWorkspaceViewCanRenameFile(xide_app* app)
{ const xide_file* f=xideWorkspaceViewSelectedFile(app); return f && xideRenameViewCanOpenFile(app,f->path); }
int xideWorkspaceViewCanRemoveFile(xide_app* app)
{
    if(!app || !app->workspaceView) return 0;
    workspace_node* node=nodeAt(app->workspaceView,app->workspaceView->selectedNode);
    return node && node->type==NODE_FILE;
}
static void syncProperties(xide_app* app)
{
    xide_workspace_view* view=app->workspaceView; xide_project* project=selectedProject(app);
    int enabled=project && project->available;
    xuiWidgetSetVisible(view->form,enabled);
    xui_widget edits[]={view->name,view->description,view->kind,view->entry,view->apply};
    for(size_t i=0;i<sizeof(edits)/sizeof(*edits);++i) xuiWidgetSetEnabled(edits[i],enabled);
    if(!project) {
        xuiLabelSetText(view->overview,"选择项目以查看属性。当前选择不会改变启动项目。");
        xuiEditSetText(view->name,""); xuiTextEditSetText(view->description,""); xuiEditSetText(view->entry,"");
    } else {
        size_t size=strlen(project->name)+strlen(project->manifest)+(project->issue ? strlen(project->issue) : 0)+192;
        char* text=malloc(size);
        if(text) {
            snprintf(text,size,"%s%s\n文件：%zu · 配置：%zu\n%s%s%s",project->name,
                app->workspaceModel.startupProject && !strcmp(project->id,app->workspaceModel.startupProject) ? " [启动项目]" : "",
                project->files.count,project->configurationCount,project->manifest,project->issue ? "\n" : "",project->issue ? project->issue : "");
            xuiLabelSetText(view->overview,text); free(text);
        }
        xuiEditSetText(view->name,project->name); xuiTextEditSetText(view->description,project->description);
        xuiComboBoxSetSelected(view->kind,!strcmp(project->kind,"exe") ? 0 : !strcmp(project->kind,"dll") ? 1 : 2);
        char *base=xidePathDirectory(project->manifest),*entry=enabled ? xidePathRelative(base,project->entry) : duplicate(""); free(base);
        xuiEditSetText(view->entry,entry ? entry : ""); free(entry);
    }
    if(view->configuration) {
        size_t count=enabled ? project->configurationCount : 0; char** names=calloc(count+1,sizeof(*names)); int ok=names && count<=INT_MAX,active=0;
        const char* effective=xideExecutionConfiguration(app,project);
        for(size_t i=0;ok && i<count;++i) { names[i]=duplicate(project->configurations[i].name); ok=names[i]!=NULL; if(effective && !strcmp(project->configurations[i].id,effective)) active=(int)i; }
        view->syncing=1;
        if(ok && xuiComboBoxSetItems(view->configuration,(const char**)names,(int)count)==XUI_OK) {
            for(size_t i=0;i<view->configurationNameCount;++i) free(view->configurationNames[i]);
            free(view->configurationNames); view->configurationNames=names; view->configurationNameCount=count; xuiComboBoxSetSelected(view->configuration,count ? active : -1);
        } else { if(names) { for(size_t i=0;i<count;++i) free(names[i]); free(names); } }
        view->syncing=0; xuiWidgetSetEnabled(view->configuration,enabled && count>0); xuiWidgetSetEnabled(view->configure,enabled);
    }
    xideActionsRefresh(app); xideWorkspaceViewLayout(app);
}
int xideWorkspaceViewHasDraft(xide_app* app)
{
    xide_workspace_view* v=app->workspaceView; xide_project* p=selectedProject(app);
    if(!v || !p || !p->available) return 0;
    int kind=!strcmp(p->kind,"exe") ? 0 : !strcmp(p->kind,"dll") ? 1 : 2;
    if(strcmp(xuiEditGetText(v->name),p->name) || strcmp(xuiTextEditGetText(v->description),p->description) || xuiComboBoxGetSelected(v->kind)!=kind) return 1;
    char* base=xidePathDirectory(p->manifest); char* entry=base ? xidePathRelative(base,p->entry) : NULL; free(base);
    int changed=!entry || strcmp(xuiEditGetText(v->entry),entry); free(entry); return changed;
}
xvalue* xideWorkspaceViewDraftSnapshot(xide_app* app)
{
    if(!xideWorkspaceViewHasDraft(app)) return NULL;
    xide_workspace_view* v=app->workspaceView; xide_project* p=selectedProject(app); xvalue* root=xrtValueObject();
    const char* keys[]={"projectId","manifest","name","description","kind","entry"}; const char* kinds[]={"exe","dll","shared"}; int kind=xuiComboBoxGetSelected(v->kind);
    const char* values[]={p->id,p->manifest,xuiEditGetText(v->name),xuiTextEditGetText(v->description),kind>=0 && kind<3 ? kinds[kind] : "",xuiEditGetText(v->entry)};
    for(size_t i=0;root && i<sizeof(keys)/sizeof(*keys);++i) if(!xrtValueObjectSetNew(root,(xstrview){keys[i],strlen(keys[i])},xrtValueString((xstrview){values[i],strlen(values[i])}))) { xrtValueRelease(root); return NULL; }
    return root;
}
static void selected(xui_widget widget,int id,void* user)
{
    (void)widget; xide_app* app=user; workspace_node* node=nodeAt(app->workspaceView,id);
    if(!node) return;
    const char* owner=node->type==NODE_FILE && !*node->project && selectedProject(app) ? app->selectedProject : node->project;
    char* project=duplicate(owner); if(!project) return;
    int analysisChanged=!app->selectedProject || strcmp(app->selectedProject,project);
    free(app->selectedProject); app->selectedProject=project; app->workspaceView->selectedNode=id;
    syncProperties(app);
    if(analysisChanged) {xideLanguageViewConfigurationChanged(app);xideCompletionViewConfigurationChanged(app);}
}
int xideWorkspaceViewRefresh(xide_app* app)
{
    if(!app->workspaceView) return XUI_ERROR_INVALID_ARGUMENT;
    xide_workspace_view fresh={0}; fresh.nextId=app->workspaceView->nextId;
    xide_workspace* model=&app->workspaceModel;
    char* title=caption(model->name ? model->name : "尚未打开工作空间",0,model->dirty,0,0);
    int root=title ? addNode(app,&fresh,NODE_WORKSPACE,-1,0,NULL,NULL,title,model->manifest) : -1; free(title);
    if(root<0) goto failed;
    for(size_t i=0;i<model->projectCount;++i) {
        xide_project* project=&model->projects[i];
        title=caption(project->name,!project->available,project->dirty,!strcmp(project->id,model->startupProject),0);
        int parent=title ? addNode(app,&fresh,NODE_PROJECT,root,1,project->id,NULL,title,project->manifest) : -1; free(title);
        if(parent<0) goto failed;
        const char* groups[]={"xlang 源码","C 源码","头文件","资源","其他文件"};
        for(int group=0;group<5;++group) {
            int category=-1; char groupId[16]; snprintf(groupId,sizeof(groupId),"group-%d",group);
            for(size_t j=0;j<project->files.count;++j) {
                xide_file* file=&project->files.items[j]; if(groupKind(file->kind)!=group) continue;
                if(category<0) category=addNode(app,&fresh,NODE_GROUP,parent,2,project->id,groupId,groups[group],NULL);
                if(category<0) goto failed;
                title=caption(xidePathFilename(file->path),!xidePathExists(file->path),0,0,xidePathEqual(file->path,project->entry));
                int added=title ? addNode(app,&fresh,NODE_FILE,category,3,project->id,file->id,title,file->path) : -1; free(title);
                if(added<0) goto failed;
            }
        }
        if(project->libraries && xrtValueCount(project->libraries)) {
            int libs=addNode(app,&fresh,NODE_GROUP,parent,2,project->id,"libraries","库引用",NULL); if(libs<0) goto failed;
            for(size_t j=0;j<xrtValueCount(project->libraries);++j) {
                xvalue* library=xrtValueArrayGet(project->libraries,j); xstrview name={0};
                xvalue* value=xrtValueObjectGet(library,(xstrview){"name",4});
                char* text=NULL;
                if(value && xrtValueGetString(value,&name)) { text=malloc(name.Size+1); if(text) { memcpy(text,name.Data,name.Size); text[name.Size]=0; } }
                if(!text) text=duplicate("库引用");
                char id[40]; snprintf(id,sizeof(id),"library-%zu",j);
                int added=text ? addNode(app,&fresh,NODE_LIBRARY,libs,3,project->id,id,text,NULL) : -1; free(text);
                if(added<0) goto failed;
            }
        }
    }
    if(model->files.count) {
        int group=addNode(app,&fresh,NODE_GROUP,root,1,NULL,"workspace-files","工作空间文件",NULL); if(group<0) goto failed;
        for(size_t i=0;i<model->files.count;++i) {
            xide_file* file=&model->files.items[i]; title=caption(xidePathFilename(file->path),!xidePathExists(file->path),0,0,0);
            int added=title ? addNode(app,&fresh,NODE_FILE,group,2,NULL,file->id,title,file->path) : -1; free(title); if(added<0) goto failed;
        }
    }
    if(fresh.count>(size_t)INT_MAX) goto failed;
    xui_tree_view_node_t* nodes=calloc(fresh.count,sizeof(*nodes)); if(!nodes) goto failed;
    int selectedId=app->workspaceView->selectedNode;
    for(size_t i=0;i<fresh.count;++i) {
        workspace_node* node=&fresh.nodes[i]; nodes[i]=(xui_tree_view_node_t){.iId=node->id,.iParent=node->parent,.iDepth=node->depth,.sText=node->caption,.bExpanded=node->expanded,.bEnabled=1,.bIconReserved=1};
    }
    int result=xuiTreeViewSetNodes(app->workspace,nodes,(int)fresh.count); free(nodes);
    if(result!=XUI_OK) { clearNodes(fresh.nodes,fresh.count); return result; }
    clearNodes(app->workspaceView->nodes,app->workspaceView->count);
    app->workspaceView->nodes=fresh.nodes; app->workspaceView->count=fresh.count; app->workspaceView->capacity=fresh.capacity; app->workspaceView->nextId=fresh.nextId;
    if(nodeAt(app->workspaceView,selectedId)) xuiTreeViewSetSelected(app->workspace,selectedId);
    syncProperties(app);xideLanguageViewConfigurationChanged(app);xideCompletionViewConfigurationChanged(app); xgeRenderRequest(); return XUI_OK;
failed:
    clearNodes(fresh.nodes,fresh.count); return XUI_ERROR_OUT_OF_MEMORY;
}
static void revealAncestors(xide_app* app,workspace_node* node)
{
    while(node && node->parent>=0) { xuiTreeViewSetNodeExpanded(app->workspace,node->parent,1); node=nodeAt(app->workspaceView,node->parent); }
}
int xideWorkspaceViewSelectProject(xide_app* app,const char* id)
{
    if(!xideWorkspaceFindProject(&app->workspaceModel,id)) return 0;
    for(size_t i=0;i<app->workspaceView->count;++i) if(sameNode(&app->workspaceView->nodes[i],NODE_PROJECT,id,NULL)) {
        revealAncestors(app,&app->workspaceView->nodes[i]);
        selected(app->workspace,app->workspaceView->nodes[i].id,app); xuiTreeViewSetSelected(app->workspace,app->workspaceView->nodes[i].id); return 1;
    }
    return 0;
}
int xideWorkspaceViewSelectFile(xide_app* app,const char* project,const char* file)
{
    for(size_t i=0;i<app->workspaceView->count;++i) if(sameNode(&app->workspaceView->nodes[i],NODE_FILE,project,file)) {
        revealAncestors(app,&app->workspaceView->nodes[i]);
        int id=app->workspaceView->nodes[i].id; selected(app->workspace,id,app); xuiTreeViewSetSelected(app->workspace,id); return 1;
    }
    return 0;
}
int xideWorkspaceOpenFile(xide_app* app,const char* path)
{
    xide_model_error error={0}; int ok=xideEditorOpen(app,path,&error);
    if(!ok) report(app,error.message);
    return ok;
}
static int treeEvent(xui_widget widget,const xui_event_t* event,void* user)
{
    xide_app* app=user;
    if(event->iType==XUI_EVENT_KEY_DOWN && event->iPhase==XUI_EVENT_PHASE_TARGET && event->iKey==XUI_KEY_F2 && !event->iModifiers) { xideActionExecute(app,XIDE_ACTION_RENAME_SELECTED_FILE); return XUI_EVENT_DISPATCH_STOP; }
    if(event->iType==XUI_EVENT_POINTER_DOUBLE_CLICK) {
        xui_rect_t rect=xuiWidgetGetWorldRect(widget);
        int visible=xuiTreeViewGetItemAt(widget,(float)(event->fX-rect.fX),(float)(event->fY-rect.fY));
        int id=xuiTreeViewGetVisibleNodeId(widget,visible); workspace_node* node=nodeAt(app->workspaceView,id);
        if(node && node->type==NODE_FILE) { selected(widget,id,app); xideWorkspaceOpenFile(app,node->path); }
    } else if(event->iType==XUI_EVENT_KEY_DOWN && event->iKey==XUI_KEY_ENTER) {
        workspace_node* node=nodeAt(app->workspaceView,app->workspaceView->selectedNode);
        if(node && node->type==NODE_FILE) xideWorkspaceOpenFile(app,node->path);
    }
    return XUI_OK;
}
static int manifestDocumentClean(xide_app* app,const char* path,xide_model_error* error)
{
    xide_document* document=path ? xideDocumentsFind(&app->documents,path) : NULL;
    if(!document || !document->dirty) return 1;
    if(error) { error->code=XIDE_MODEL_CONFLICT; snprintf(error->message,sizeof(error->message),"Manifest text has unsaved changes. Save or close that document before editing/saving project properties."); }
    return 0;
}
int xideWorkspaceViewApplyProperties(xide_app* app,const char* name,const char* description,const char* kind,const char* entry,xide_model_error* error)
{
    xide_project* project=selectedProject(app);
    if(project && !manifestDocumentClean(app,project->manifest,error)) return 0;
    if(!xideProjectApplyProperties(project,name,description,kind,entry,error)) return 0;
    return xideWorkspaceViewRefresh(app)==XUI_OK;
}
static void applyProperties(xui_widget widget,void* user)
{
    (void)widget; xide_app* app=user; xide_workspace_view* view=app->workspaceView;
    static const char* kinds[]={"exe","dll","shared"}; int kind=xuiComboBoxGetSelected(view->kind); xide_model_error error={0};
    if(kind<0 || kind>2) return;
    if(!xideWorkspaceViewApplyProperties(app,xuiEditGetText(view->name),xuiTextEditGetText(view->description),kinds[kind],xuiEditGetText(view->entry),&error)) report(app,error.message);
}
void xideWorkspaceViewLayout(xide_app* app)
{
    if(!app->workspaceView || !app->properties) return;
    xide_workspace_view* view=app->workspaceView; xui_rect_t rect=xuiWidgetGetContentRect(app->properties);
    float width=rect.fW-14; if(width<100) width=100;
    xui_vec2_t measured={0}; xuiWidgetMeasureContent(view->overview,(xui_vec2_t){width-20,XUI_LAYOUT_UNBOUNDED},&measured);
    float height=measured.fY+12; if(height<xideUiHeight(app,64)) height=(float)xideUiHeight(app,64);
    xideUiLayoutApply(app,&view->layout,0);
    xuiWidgetSetGap(view->form,(float)xideUiHeight(app,6));
    xuiWidgetSetPreferredSize(view->form,(xui_vec2_t){0,(float)xideUiHeight(app,566)});
    xuiWidgetSetPreferredSize(view->overview,(xui_vec2_t){0,height});
    xuiScrollViewSetContentSize(app->properties,width,height+(xuiWidgetGetVisible(view->form) ? xideUiHeight(app,564) : xideUiHeight(app,24)));
}
static int addPropertyLabel(xide_app* app,const char* text)
{
    xui_widget widget=NULL; xui_label_desc_t desc={0}; desc.iSize=sizeof(desc); desc.pFont=app->uiFont; desc.sText=text;
    TRY(xuiLabelCreate(app->context,&widget,&desc)); TRY(ideThemeSetTextRole(widget,IDE_MUTED));
    TRY(xuiWidgetSetSizeMode(widget,XUI_SIZE_FILL,XUI_SIZE_FIXED)); TRY(xuiWidgetSetPreferredSize(widget,(xui_vec2_t){0,22}));
    return xuiWidgetAddChild(app->workspaceView->form,widget);
}
static int addInput(xide_app* app,xui_widget* widget)
{
    xui_input_desc_t desc={0}; desc.iSize=sizeof(desc); desc.pFont=app->uiFont;
    TRY(xuiInputCreate(app->context,widget,&desc)); TRY(xuiWidgetSetSizeMode(*widget,XUI_SIZE_FILL,XUI_SIZE_FIXED));
    TRY(xuiWidgetSetPreferredSize(*widget,(xui_vec2_t){0,32})); return xuiWidgetAddChild(app->workspaceView->form,*widget);
}
static void configurationSelected(xui_widget widget,int index,int value,void* user)
{
    (void)widget; (void)value; xide_app* app=user; xide_project* project=selectedProject(app); xide_model_error error={0};
    if(app->workspaceView->syncing || !project || index<0 || (size_t)index>=project->configurationCount) return;
    if(!xideWorkspaceViewSelectConfiguration(app,project->configurations[index].id,&error)) { report(app,error.message); syncProperties(app); }
}
int xideWorkspaceViewCreate(xide_app* app)
{
    app->workspaceView=calloc(1,sizeof(*app->workspaceView)); if(!app->workspaceView) return XUI_ERROR_OUT_OF_MEMORY;
    xide_workspace_view* view=app->workspaceView; view->nextId=2;
    xui_tree_view_desc_t tree={0}; tree.iSize=sizeof(tree); tree.pFont=app->uiFont; tree.fItemHeight=28; tree.fIndent=14; tree.fPadding=8;
    TRY(xuiTreeViewCreate(app->context,&app->workspace,&tree)); TRY(xuiTreeViewSetSelect(app->workspace,selected,app));
    TRY(xuiWidgetSetEventHandler(app->workspace,XUI_EVENT_POINTER_DOUBLE_CLICK,treeEvent,app));
    TRY(xuiWidgetSetEventHandler(app->workspace,XUI_EVENT_KEY_DOWN,treeEvent,app));
    xui_scroll_view_desc_t scroll={0}; scroll.iSize=sizeof(scroll); scroll.iPolicyX=XUI_SCROLLBAR_POLICY_HIDDEN; scroll.iPolicyY=XUI_SCROLLBAR_POLICY_AUTO;
    scroll.iScrollbarMode=XUI_SCROLLBAR_MODE_COMPACT; scroll.fScrollbarSize=10; scroll.fContentWidth=230; scroll.fContentHeight=440;
    TRY(xuiScrollViewCreate(app->context,&app->properties,&scroll)); view->content=xuiScrollViewGetContentWidget(app->properties);
    TRY(xuiWidgetSetLayoutType(view->content,XUI_LAYOUT_COLUMN)); TRY(xuiWidgetSetPadding(view->content,(xui_thickness_t){10,10,10,10})); TRY(xuiWidgetSetGap(view->content,6));
    xui_label_desc_t overview={0}; overview.iSize=sizeof(overview); overview.pFont=app->uiFont; overview.iWrapMode=XUI_TEXT_WRAP_WORD;
    TRY(xuiLabelCreate(app->context,&view->overview,&overview)); TRY(ideThemeSetTextRole(view->overview,IDE_MUTED));
    TRY(xuiWidgetSetSizeMode(view->overview,XUI_SIZE_FILL,XUI_SIZE_FIXED)); TRY(xuiWidgetAddChild(view->content,view->overview));
    TRY(xuiWidgetCreate(app->context,&view->form)); TRY(xuiWidgetSetLayoutType(view->form,XUI_LAYOUT_COLUMN));
    TRY(xuiWidgetSetSizeMode(view->form,XUI_SIZE_FILL,XUI_SIZE_FIXED)); TRY(xuiWidgetSetPreferredSize(view->form,(xui_vec2_t){0,566}));
    TRY(xuiWidgetSetGap(view->form,6)); TRY(xuiWidgetAddChild(view->content,view->form));
    TRY(addPropertyLabel(app,"项目名称")); TRY(addInput(app,&view->name));
    TRY(addPropertyLabel(app,"项目描述"));
    xui_text_edit_desc_t description={0}; description.iSize=sizeof(description); description.pFont=app->uiFont; description.bWordWrap=1; description.sPlaceholder="项目描述（可为空）";
    TRY(xuiTextEditCreate(app->context,&view->description,&description));
    /* This pinned SDK's self-cache fails for an empty multiline field. Draw
       this small editable property directly, retaining the actual empty text. */
    xui_cache_policy_t cache=xuiWidgetGetCachePolicy(view->description); cache.iSize=sizeof(cache); cache.iPolicy=XUI_CACHE_POLICY_NONE;
    TRY(xuiWidgetSetCachePolicy(view->description,&cache)); TRY(xuiWidgetSetSizeMode(view->description,XUI_SIZE_FILL,XUI_SIZE_FIXED));
    TRY(xuiWidgetSetPreferredSize(view->description,(xui_vec2_t){0,64})); TRY(xuiWidgetAddChild(view->form,view->description));
    TRY(addPropertyLabel(app,"输出类型"));
    static const char* types[]={"exe","dll","shared"}; xui_combobox_desc_t kind={0}; kind.iSize=sizeof(kind); kind.arrItems=types; kind.iItemCount=3; kind.pFont=app->uiFont;
    TRY(xuiComboBoxCreate(app->context,&view->kind,&kind)); TRY(xuiWidgetSetSizeMode(view->kind,XUI_SIZE_FILL,XUI_SIZE_FIXED));
    TRY(xuiWidgetSetPreferredSize(view->kind,(xui_vec2_t){0,32})); TRY(xuiWidgetAddChild(view->form,view->kind));
    TRY(addPropertyLabel(app,"入口文件 (.xl)")); TRY(addInput(app,&view->entry));
    xui_button_desc_t apply={0}; apply.iSize=sizeof(apply); apply.pFont=app->uiFont; apply.sText="应用项目属性";
    TRY(xuiButtonCreate(app->context,&view->apply,&apply)); TRY(xideActionsButton(app,view->apply,XIDE_ACTION_APPLY_PROPERTIES));
    TRY(xuiWidgetSetSizeMode(view->apply,XUI_SIZE_FILL,XUI_SIZE_FIXED)); TRY(xuiWidgetSetPreferredSize(view->apply,(xui_vec2_t){0,32}));
    TRY(xuiWidgetAddChild(view->form,view->apply));
    TRY(addPropertyLabel(app,"当前项目配置"));
    xui_combobox_desc_t configuration={0}; configuration.iSize=sizeof(configuration); configuration.pFont=app->uiFont;
    TRY(xuiComboBoxCreate(app->context,&view->configuration,&configuration)); TRY(xuiComboBoxSetSelect(view->configuration,configurationSelected,app));
    TRY(xuiWidgetSetSizeMode(view->configuration,XUI_SIZE_FILL,XUI_SIZE_FIXED)); TRY(xuiWidgetSetPreferredSize(view->configuration,(xui_vec2_t){0,32})); TRY(xuiWidgetAddChild(view->form,view->configuration));
    apply.sText="编辑构建、运行与测试配置…";
    TRY(xuiButtonCreate(app->context,&view->configure,&apply)); TRY(xideActionsButton(app,view->configure,XIDE_ACTION_CONFIGURATIONS));
    TRY(xuiWidgetSetSizeMode(view->configure,XUI_SIZE_FILL,XUI_SIZE_FIXED)); TRY(xuiWidgetSetPreferredSize(view->configure,(xui_vec2_t){0,32})); TRY(xuiWidgetAddChild(view->form,view->configure));
    apply.sText="项目文件、资源与库…";
    TRY(xuiButtonCreate(app->context,&view->references,&apply)); TRY(xideActionsButton(app,view->references,XIDE_ACTION_REFERENCES));
    TRY(xuiWidgetSetSizeMode(view->references,XUI_SIZE_FILL,XUI_SIZE_FIXED)); TRY(xuiWidgetSetPreferredSize(view->references,(xui_vec2_t){0,32})); TRY(xuiWidgetAddChild(view->form,view->references));
    apply.sText="Windows 资源属性…";
    TRY(xuiButtonCreate(app->context,&view->resources,&apply)); TRY(xideActionsButton(app,view->resources,XIDE_ACTION_RESOURCES));
    TRY(xuiWidgetSetSizeMode(view->resources,XUI_SIZE_FILL,XUI_SIZE_FIXED)); TRY(xuiWidgetSetPreferredSize(view->resources,(xui_vec2_t){0,32})); TRY(xuiWidgetAddChild(view->form,view->resources));
    apply.sText="新建 Windows 资源配置…";
    TRY(xuiButtonCreate(app->context,&view->newResource,&apply)); TRY(xideActionsButton(app,view->newResource,XIDE_ACTION_NEW_RESOURCE));
    TRY(xuiWidgetSetSizeMode(view->newResource,XUI_SIZE_FILL,XUI_SIZE_FIXED)); TRY(xuiWidgetSetPreferredSize(view->newResource,(xui_vec2_t){0,32})); TRY(xuiWidgetAddChild(view->form,view->newResource));
    TRY(xideUiLayoutCapture(&view->layout,view->form));
    return xideWorkspaceViewRefresh(app);
}
int xideWorkspaceOpen(xide_app* app,const char* path,int projectOnly,xide_model_error* error)
{
    xide_workspace model={0};
    char* projectPath=NULL; int recovered=0;
    int ok=xideSessionLoadMigration(app,path,projectOnly,&model,&projectPath,&recovered,error);
    if(ok && !recovered) ok=projectOnly ? xideWorkspaceCreate(&model,xidePathFilename(path),error) && xideWorkspaceAddProject(&model,path,NULL,error) : xideWorkspaceLoad(&model,path,error);
    if(ok && !recovered && projectOnly && model.projectCount==1) { ok=xideWorkspaceSetStartup(&model,model.projects[0].id,error); model.dirty=0; }
    if(!ok) { free(projectPath); xideSessionDiscardMigration(app); xideWorkspaceClear(&model); return 0; }
    if(!manifestDocumentClean(app,model.manifest,error)) { free(projectPath); xideSessionDiscardMigration(app); xideWorkspaceClear(&model); return 0; }
    for(size_t i=0;i<model.projectCount;++i) if(!manifestDocumentClean(app,model.projects[i].manifest,error)) { free(projectPath); xideSessionDiscardMigration(app); xideWorkspaceClear(&model); return 0; }
    char* selection=duplicate(model.projectCount ? (*model.startupProject ? model.startupProject : model.projects[0].id) : "");
    if(!selection) { free(projectPath); xideSessionDiscardMigration(app); xideWorkspaceClear(&model); return 0; }
    if(!recovered) projectPath=projectOnly ? xidePathResolve(NULL,path) : NULL;
    if((projectOnly && !projectPath) || !xideSessionBeforeWorkspace(app,error)) { free(projectPath); free(selection); xideSessionDiscardMigration(app); xideWorkspaceClear(&model); return 0; }
    free(app->projectSessionPath); app->projectSessionPath=projectPath;
    xideWorkspaceClear(&app->workspaceModel); app->workspaceModel=model;
    free(app->selectedProject); app->selectedProject=selection;
    app->workspaceView->selectedNode=0;
    if(xideWorkspaceViewRefresh(app)!=XUI_OK) return 0;
    if(*selection) xideWorkspaceViewSelectProject(app,selection);
    xideSessionAfterWorkspace(app);
    return 1;
}
int xideWorkspaceSaveAll(xide_app* app,const char* path,xide_model_error* error)
{
    if(!manifestDocumentClean(app,app->workspaceModel.manifest,error)) return 0;
    for(size_t i=0;i<app->workspaceModel.projectCount;++i) if(!manifestDocumentClean(app,app->workspaceModel.projects[i].manifest,error)) return 0;
    for(size_t i=0;i<app->workspaceModel.projectCount;++i) {
        xide_project* project=&app->workspaceModel.projects[i];
        if(project->dirty && !xideProjectSave(project,NULL,error)) return 0;
    }
    char* oldPath=app->workspaceModel.manifest ? duplicate(app->workspaceModel.manifest) : NULL;
    if(!xideWorkspaceSave(&app->workspaceModel,path,error)) { free(oldPath); return 0; }
    int relocated=!oldPath || !xidePathEqual(oldPath,app->workspaceModel.manifest); free(oldPath);
    if(relocated) { free(app->projectSessionPath); app->projectSessionPath=NULL; xideSessionRelocate(app); }
    for(size_t i=0;i<app->documents.count;++i) {
        xide_document* document=app->documents.items[i]; int manifest=0;
        if(!document->open || !document->path) continue;
        if(app->workspaceModel.manifest && xidePathEqual(document->path,app->workspaceModel.manifest)) manifest=1;
        for(size_t j=0;j<app->workspaceModel.projectCount && !manifest;++j) if(xidePathEqual(document->path,app->workspaceModel.projects[j].manifest)) manifest=1;
        if(manifest && !document->dirty && xideDocumentCheckExternal(document) && !xideDocumentReload(document,0,error)) return 0;
    }
    return xideWorkspaceViewRefresh(app)==XUI_OK;
}
static int dirty(xide_app* app)
{
    if(app->workspaceModel.dirty) return 1;
    for(size_t i=0;i<app->workspaceModel.projectCount;++i) if(app->workspaceModel.projects[i].dirty) return 1;
    return 0;
}
static int saveWithDialog(xide_app* app,int saveAs)
{
    char* path=saveAs || !app->workspaceModel.manifest ? chooseFile(app,1,0) : duplicate(app->workspaceModel.manifest);
    if(!path) return 0;
    xide_model_error error={0}; int ok=xideWorkspaceSaveAll(app,path,&error); free(path); if(!ok) report(app,error.message); return ok;
}
int xideWorkspaceQuitRequest(void* user)
{
    xide_app* app=user; if(!xideEditorConfirmAll(app)) return 0; if(!dirty(app)) return 1;
    if(app->hidden || !xgePlatformNativeHandle()) return 0;
    int answer=xideMessageConfirm(app,"保存工作空间","工作空间或项目有未保存的修改。是否保存？",XUI_MSGBOX_BUTTON_YES_NO_CANCEL,xideMessageWorkspaceKey(app,4));
    if(answer==XUI_MSGBOX_RESULT_CANCEL || answer==XUI_MSGBOX_RESULT_CLOSE) return 0;
    return answer==XUI_MSGBOX_RESULT_NO || saveWithDialog(app,0);
}
int xideWorkspaceAction(xide_app* app,int action)
{
    if(action<XIDE_ACTION_NEW_WORKSPACE || action>XIDE_ACTION_MIGRATE_PROJECT_DIRECTORY) return 0;
    xide_model_error error={0}; char* path=NULL; int ok=1;
    xide_project* project=selectedProject(app); workspace_node* node=nodeAt(app->workspaceView,app->workspaceView->selectedNode);
    if(action==XIDE_ACTION_RENAME_SELECTED_FILE) { const xide_file* file=xideWorkspaceViewSelectedFile(app); if(file && !xideRenameViewOpenFile(app,file->path,&error)) report(app,error.message); return 1; }
    if(action==XIDE_ACTION_MIGRATE_PROJECT_MANIFEST || action==XIDE_ACTION_MIGRATE_PROJECT_DIRECTORY) { if(!xideRenameViewOpenProject(app,action==XIDE_ACTION_MIGRATE_PROJECT_DIRECTORY ? XIDE_MIGRATE_DIRECTORY : XIDE_MIGRATE_MANIFEST,&error)) report(app,error.message); return 1; }
    if(action==XIDE_ACTION_APPLY_PROPERTIES) { applyProperties(NULL,app); return 1; }
    if(action==XIDE_ACTION_CONFIGURATIONS) { xideConfigurationViewOpen(app); return 1; }
    if(action==XIDE_ACTION_NEW_WORKSPACE) {
        if(!xideWorkspaceQuitRequest(app)) return 1;
        path=chooseFile(app,1,0); if(!path) return 1;
        xide_workspace model={0}; ok=xideWorkspaceCreate(&model,xidePathFilename(path),&error) && xideWorkspaceSave(&model,path,&error);
        app->sessionSwitchConfirmed=1;
        if(ok) ok=xideSessionBeforeWorkspace(app,&error);
        if(ok) { xideWorkspaceClear(&app->workspaceModel); app->workspaceModel=model; free(app->projectSessionPath); app->projectSessionPath=NULL; free(app->selectedProject); app->selectedProject=duplicate(""); app->workspaceView->selectedNode=0; xideSessionAfterWorkspace(app); }
        else xideWorkspaceClear(&model);
        app->sessionSwitchConfirmed=0;
    } else if(action==XIDE_ACTION_OPEN_WORKSPACE || action==XIDE_ACTION_OPEN_PROJECT) {
        if(!xideWorkspaceQuitRequest(app)) return 1;
        path=chooseFile(app,0,action==XIDE_ACTION_OPEN_PROJECT ? 1 : 0);
        app->sessionSwitchConfirmed=1;
        if(path) ok=xideWorkspaceOpen(app,path,action==XIDE_ACTION_OPEN_PROJECT,&error);
        if(path && ok && app->preferencesInitialized) { xide_model_error recent={0};if(!xideSettingsRememberOpen(&app->preferences,path,&recent))xuiStatusBarSetItemText(app->statusBar,0,recent.message); }
        app->sessionSwitchConfirmed=0;
    } else if(action==XIDE_ACTION_SAVE_WORKSPACE || action==XIDE_ACTION_SAVE_WORKSPACE_AS) {
        saveWithDialog(app,action==XIDE_ACTION_SAVE_WORKSPACE_AS); return 1;
    } else if(action==XIDE_ACTION_ADD_PROJECT) {
        path=chooseFile(app,0,1); if(path) ok=xideWorkspaceAddProject(&app->workspaceModel,path,NULL,&error);
    } else if(action==XIDE_ACTION_NEW_PROJECT) {
        xideTemplateViewOpen(app); return 1;
    } else if(action==XIDE_ACTION_REMOVE_PROJECT && project) {
        if(project->dirty && app->hidden) return 1;
        if(project->dirty && xideMessageConfirm(app,"移除项目引用","此项目有未保存修改。移除引用将放弃这些修改，是否继续？",XUI_MSGBOX_BUTTON_OK_CANCEL,xideMessageWorkspaceKey(app,5))!=XUI_MSGBOX_RESULT_OK) return 1;
        ok=xideWorkspaceRemoveProject(&app->workspaceModel,project->id,&error); if(ok) { free(app->selectedProject); app->selectedProject=duplicate(""); app->workspaceView->selectedNode=0; }
    } else if(action==XIDE_ACTION_SET_STARTUP && project) ok=xideWorkspaceSetStartup(&app->workspaceModel,project->id,&error);
    else if(action==XIDE_ACTION_ADD_FILE) {
        path=chooseFile(app,0,2); if(path) ok=project ? xideProjectAddFile(project,path,NULL,&error) : xideWorkspaceAddFile(&app->workspaceModel,path,&error);
    } else if(action==XIDE_ACTION_REMOVE_FILE && node && node->type==NODE_FILE) {
        ok=*node->project ? xideProjectRemoveFile(project,node->file,&error) : xideWorkspaceRemoveFile(&app->workspaceModel,node->file,&error);
        if(ok) app->workspaceView->selectedNode=0;
    }
    free(path); if(!ok) report(app,error.message); xideWorkspaceViewRefresh(app); return 1;
}
int xideWorkspaceViewApplyConfigurations(xide_app* app,const char* projectId,const xvalue* configs,const char* active,xide_model_error* error)
{
    xide_project* project=xideWorkspaceFindProject(&app->workspaceModel,projectId);
    if(project && !manifestDocumentClean(app,project->manifest,error)) return 0;
    if(!xideProjectApplyConfigurations(project,configs,active,error)) return 0;
    return xideWorkspaceViewRefresh(app)==XUI_OK;
}
int xideWorkspaceViewApplyReferences(xide_app* app,const char* projectId,const xvalue* references,xide_model_error* error)
{
    xide_project* project=xideWorkspaceFindProject(&app->workspaceModel,projectId);
    if(project && !manifestDocumentClean(app,project->manifest,error)) return 0;
    xvalue* reading=xideReadingCapture(app,NULL);
    if(!reading) { if(error) { error->code=XIDE_MODEL_MEMORY; snprintf(error->message,sizeof(error->message),"Cannot preserve tree reading state."); } return 0; }
    if(!xideProjectApplyReferences(project,references,error)) { xrtValueRelease(reading); return 0; }
    int okay=xideWorkspaceViewRefresh(app)==XUI_OK;
    if(okay) { xideReadingRestore(app,reading,0); xideWorkbenchLayout(app); okay=xuiLayout(app->context)==XUI_OK; if(okay) xideReadingRestore(app,reading,1); }
    xrtValueRelease(reading); return okay;
}
int xideWorkspaceViewSelectConfiguration(xide_app* app,const char* id,xide_model_error* error)
{
    xide_project* project=selectedProject(app);
    if(project && !manifestDocumentClean(app,project->manifest,error)) return 0;
    if(!xideProjectSetActiveConfiguration(project,id,error)) return 0;
    if(!xideExecutionSelectConfiguration(app,project->id,id))return 0;
    return xideWorkspaceViewRefresh(app)==XUI_OK;
}
char* xideWorkspaceTreeKey(xide_app* app,int id)
{
    workspace_node* node=app->workspaceView ? nodeAt(app->workspaceView,id) : NULL; if(!node) return NULL;
    static const char* kinds[]={"none","workspace","project","group","file","library"};
    if(node->type==NODE_LIBRARY) {
        xide_project* project=xideWorkspaceFindProject(&app->workspaceModel,node->project); int index=-1;
        if(project && sscanf(node->file,"library-%d",&index)==1 && index>=0 && (size_t)index<xrtValueCount(project->libraries)) {
            xvalue* library=xrtValueArrayGet(project->libraries,(size_t)index); xstrview stable={0};
            if(xrtValueGetString(xrtValueObjectGet(library,(xstrview){"id",2}),&stable) && stable.Size) {
                char* identity=malloc(stable.Size+1); if(!identity) return NULL;
                memcpy(identity,stable.Data,stable.Size); identity[stable.Size]=0;
                char* key=xideReadingKey("library",node->project,identity); free(identity); return key;
            }
            size_t length=0; char* definition=xrtJsonStringify(library,false,&length);
            char* key=definition ? xideReadingKey("library",node->project,definition) : NULL; xrtFree(definition); return key;
        }
        return NULL;
    }
    return xideReadingKey(kinds[node->type],node->project,node->file);
}
int xideWorkspaceTreeSelect(xide_app* app,int id)
{
    if(!app->workspaceView || !nodeAt(app->workspaceView,id)) return 0;
    selected(app->workspace,id,app); return xuiTreeViewSetSelected(app->workspace,id)==XUI_OK;
}
