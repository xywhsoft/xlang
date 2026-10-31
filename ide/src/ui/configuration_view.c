#include "ui/toolchain_view.h"
#include "ui/resource_view.h"
#include "ui/import_view.h"
#include "ui/template_view.h"
#include "ui/rename_view.h"
#include "ui/references_view.h"
#include "ui/file_changes_view.h"
#include "ui/configuration_view.h"
#include "services/ui_font.h"
#include "ui/font_view.h"
#include "ui/recovery_view.h"
#include "ui/workspace_view.h"
#include "platform/path.h"
#include "theme/theme.h"
#include <windows.h>
#include <objbase.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#define TRY(call) do { int result_=(call); if(result_!=XUI_OK) return result_; } while(0)
struct xide_configuration_view {
    xui_widget window,content,controls[XIDE_CONFIG_CONTROL_COUNT],identity,message,outputLabel,directoryLabel;
    xide_ui_layout layout; float scale;
    xvalue *drafts,*baseline;
    char *project,*manifest,**titles,**rows;
    size_t titleCount,rowCount;
    int index,category,sync;
};
static xstrview sv(const char* text) { return (xstrview){text,strlen(text)}; }
static xvalue* get(const xvalue* value,const char* key) { return value ? xrtValueObjectGet(value,sv(key)) : NULL; }
static const char* text(const xvalue* value)
{
    xstrview string={0}; return value && xrtValueGetString(value,&string) ? string.Data : "";
}
static char* copy(const char* string) { size_t size=strlen(string)+1; char* value=malloc(size); if(value) memcpy(value,string,size); return value; }
static int put(xvalue* value,const char* key,const char* string) { return xrtValueObjectSetNew(value,sv(key),xrtValueString(sv(string))); }
static void clearTitles(char*** titles,size_t* count)
{
    for(size_t i=0;i<*count;++i) free((*titles)[i]);
    free(*titles); *titles=NULL; *count=0;
}
static xvalue* current(xide_configuration_view* view) { return view->index>=0 ? xrtValueArrayGet(view->drafts,(size_t)view->index) : NULL; }
static int isEnvironment(const xide_configuration_view* view) { return view->category==4 || view->category==10; }
static xvalue* testObject(xvalue* root)
{
    xvalue* test=get(root,"test");
    if(!test && root) { test=xrtValueObject(); if(!test || !xrtValueObjectSetNew(root,sv("test"),test)) return NULL; }
    return test;
}
static xvalue* list(xide_configuration_view* view)
{
    if(view->category>=9) {
        xvalue* test=testObject(current(view)); const char* key=view->category==9 ? "arguments" : "environment";
        if(!get(test,key) && (!test || !xrtValueObjectSetNew(test,sv(key),view->category==9 ? xrtValueArray() : xrtValueObject()))) return NULL;
        return get(test,key);
    }
    static const char* keys[]={"includePaths","libraryPaths","linkLibraries","arguments","environment"};
    xvalue* root=current(view); if(view->category<5) return get(view->category<3 ? root : get(root,"run"),keys[view->category]);
#if XIDE_ENABLE_C_LSP
    if(!root) return NULL;
    xvalue* analysis=get(root,"cAnalysis");
    if(!analysis) { analysis=xrtValueObject(); if(!analysis || !xrtValueObjectSetNew(root,sv("cAnalysis"),analysis)) return NULL; }
    const char* cKeys[]={"systemIncludes","forcedIncludes","defines","undefines"}; const char* key=cKeys[view->category-5];
    if(!get(analysis,key) && !xrtValueObjectSetNew(analysis,sv(key),xrtValueArray())) return NULL;
    return get(analysis,key);
#else
    return NULL;
#endif
}
static void message(xide_configuration_view* view,const char* value) { xuiLabelSetText(view->message,value); xgeRenderRequest(); }
static int storeFields(xide_configuration_view* view)
{
    xvalue* root=current(view); if(!root) return 1;
    int okay=put(root,"name",xuiEditGetText(view->controls[XIDE_CONFIG_NAME])) && put(root,"output",xuiEditGetText(view->controls[XIDE_CONFIG_OUTPUT])) &&
        put(get(root,"run"),"workingDirectory",xuiEditGetText(view->controls[XIDE_CONFIG_DIRECTORY]));
    const char* program=xuiEditGetText(view->controls[XIDE_CONFIG_TEST_PROGRAM]);
    if(okay && (get(root,"test") || *program)) {
        xvalue* test=testObject(root);
        okay=test && put(test,"program",program) && put(test,"workingDirectory",xuiEditGetText(view->controls[XIDE_CONFIG_TEST_DIRECTORY]));
    }
    return okay;
}
static void rowSelected(xui_widget widget,int index,void* user)
{
    (void)widget; xide_app* app=user; xide_configuration_view* view=app->configurationView;
    xvalue* values=list(view); if(index<0 || (size_t)index>=xrtValueCount(values)) return;
    xstrview key={0}; xvalue* item=isEnvironment(view) ? xrtValueObjectAt(values,(size_t)index,&key) : xrtValueArrayGet(values,(size_t)index);
    if(isEnvironment(view)) { char* name=malloc(key.Size+1); if(name) { memcpy(name,key.Data,key.Size); name[key.Size]=0; xuiEditSetText(view->controls[XIDE_CONFIG_KEY],name); free(name); } }
    xuiEditSetText(view->controls[XIDE_CONFIG_VALUE],text(item));
}
static int refreshRows(xide_app* app,int selected)
{
    xide_configuration_view* view=app->configurationView; xvalue* values=list(view); size_t count=xrtValueCount(values);
    if(count>INT_MAX) return 0;
    char** rows=calloc(count+1,sizeof(*rows)); if(!rows) return 0;
    for(size_t i=0;i<count;++i) {
        xstrview key={0}; xvalue* item=isEnvironment(view) ? xrtValueObjectAt(values,i,&key) : xrtValueArrayGet(values,i);
        const char* value=text(item);
        if(isEnvironment(view)) { rows[i]=malloc(key.Size+strlen(value)+5); if(rows[i]) sprintf(rows[i],"%.*s = %s",(int)key.Size,key.Data,value); }
        else rows[i]=copy(*value ? value : "[空参数]");
        if(!rows[i]) { for(size_t j=0;j<i;++j) free(rows[j]); free(rows); return 0; }
    }
    if(xuiListViewSetItems(view->controls[XIDE_CONFIG_LIST],(const char**)rows,(int)count)!=XUI_OK) { for(size_t i=0;i<count;++i) free(rows[i]); free(rows); return 0; }
    clearTitles(&view->rows,&view->rowCount); view->rows=rows; view->rowCount=count;
    if(selected>=(int)count) selected=(int)count-1;
    xuiListViewSetSelected(view->controls[XIDE_CONFIG_LIST],selected);
    xuiEditSetText(view->controls[XIDE_CONFIG_VALUE],""); xuiEditSetText(view->controls[XIDE_CONFIG_KEY],"");
    if(selected>=0) rowSelected(view->controls[XIDE_CONFIG_LIST],selected,app);
    xuiWidgetSetVisible(view->controls[XIDE_CONFIG_KEY],isEnvironment(view));
    xuiWidgetSetRect(view->controls[XIDE_CONFIG_VALUE],xideUiRect(app,(xui_rect_t){isEnvironment(view) ? 188 : 0,444,isEnvironment(view) ? 548 : 736,32}));
    int testing=view->category>=9;
    xuiWidgetSetVisible(view->controls[XIDE_CONFIG_OUTPUT],!testing); xuiWidgetSetVisible(view->controls[XIDE_CONFIG_DIRECTORY],!testing);
    xuiWidgetSetVisible(view->controls[XIDE_CONFIG_TEST_PROGRAM],testing); xuiWidgetSetVisible(view->controls[XIDE_CONFIG_TEST_DIRECTORY],testing);
    xuiLabelSetText(view->outputLabel,testing ? "测试程序 (.exe，留空关闭测试)" : "输出路径（留空使用默认产物路径）");
    xuiLabelSetText(view->directoryLabel,testing ? "测试工作目录" : "运行工作目录");
    xuiWidgetSetEnabled(view->controls[XIDE_CONFIG_UP],!isEnvironment(view)); xuiWidgetSetEnabled(view->controls[XIDE_CONFIG_DOWN],!isEnvironment(view));
    return 1;
}
static void loadFields(xide_app* app)
{
    xide_configuration_view* view=app->configurationView; xvalue* root=current(view);
    xuiEditSetText(view->controls[XIDE_CONFIG_NAME],text(get(root,"name"))); xuiEditSetText(view->controls[XIDE_CONFIG_OUTPUT],text(get(root,"output")));
    xuiEditSetText(view->controls[XIDE_CONFIG_DIRECTORY],text(get(get(root,"run"),"workingDirectory")));
    xvalue* test=get(root,"test"); xuiEditSetText(view->controls[XIDE_CONFIG_TEST_PROGRAM],text(get(test,"program")));
    xuiEditSetText(view->controls[XIDE_CONFIG_TEST_DIRECTORY],get(test,"workingDirectory") ? text(get(test,"workingDirectory")) : ".");
    char identity[180]; snprintf(identity,sizeof(identity),"配置 ID：%s",text(get(root,"id"))); xuiLabelSetText(view->identity,identity);
    refreshRows(app,-1); message(view,"路径相对于项目清单。列表项编辑后按添加或更新；运行参数按列表顺序传递。");
}
static int refreshConfigurations(xide_app* app)
{
    xide_configuration_view* view=app->configurationView; size_t count=xrtValueCount(view->drafts); if(count>INT_MAX) return 0;
    char** titles=calloc(count+1,sizeof(*titles)); if(!titles) return 0;
    for(size_t i=0;i<count;++i) { titles[i]=copy(text(get(xrtValueArrayGet(view->drafts,i),"name"))); if(!titles[i]) { for(size_t j=0;j<i;++j) free(titles[j]); free(titles); return 0; } }
    view->sync=1; int result=xuiComboBoxSetItems(view->controls[XIDE_CONFIG_PICKER],(const char**)titles,(int)count);
    if(result==XUI_OK) { clearTitles(&view->titles,&view->titleCount); view->titles=titles; view->titleCount=count; xuiComboBoxSetSelected(view->controls[XIDE_CONFIG_PICKER],view->index); }
    else { for(size_t i=0;i<count;++i) free(titles[i]); free(titles); }
    view->sync=0; loadFields(app); return result==XUI_OK;
}
static void configSelected(xui_widget widget,int index,int value,void* user)
{
    (void)widget; (void)value; xide_app* app=user; xide_configuration_view* view=app->configurationView;
    if(view->sync || index<0 || (size_t)index>=xrtValueCount(view->drafts)) return;
    if(!storeFields(view)) { message(view,"内存不足，无法保留配置草稿。"); return; }
    view->index=index; loadFields(app);
}
static void categorySelected(xui_widget widget,int index,int value,void* user)
{
    (void)widget; (void)value; xide_app* app=user; xide_configuration_view* view=app->configurationView;
#if XIDE_ENABLE_C_LSP
    if(index<0 || index>10 || view->sync) return;
    view->category=index;
#else
    if(index<0 || index>6 || view->sync) return;
    view->category=index<5 ? index : index+4;
#endif
    refreshRows(app,-1);
    if(view->category>=9) message(view,"测试仅启动明确配置的 .exe 程序。脚本填写解释器程序及参数；Ctrl+F7 主动运行所选项目测试。");
    else if(index>=5) message(view,"C 分析设置由 clangd 使用，不改变 TCC 构建。强制头按列表顺序包含；宏定义先于取消定义。");
    else message(view,"参数保持原文，不添加 shell 引号。测试程序在测试类别中配置。");
    xgeRenderRequest();
}
static char* newId(void)
{
    GUID id; if(FAILED(CoCreateGuid(&id))) return NULL;
    char value[64]; snprintf(value,sizeof(value),"config-%08lx-%04x-%04x-%02x%02x%02x%02x%02x%02x%02x%02x",(unsigned long)id.Data1,id.Data2,id.Data3,id.Data4[0],id.Data4[1],id.Data4[2],id.Data4[3],id.Data4[4],id.Data4[5],id.Data4[6],id.Data4[7]); return copy(value);
}
static int addConfiguration(xide_app* app,int clone)
{
    xide_configuration_view* view=app->configurationView; if(!storeFields(view)) return 0;
    xvalue* root=clone && current(view) ? xrtValueDeepClone(current(view)) : xrtJsonParse(sv("{\"name\":\"新配置\",\"output\":\"\",\"includePaths\":[],\"libraryPaths\":[],\"linkLibraries\":[],\"run\":{\"workingDirectory\":\".\",\"arguments\":[],\"environment\":{}}}"));
    char* id=newId(); int okay=root && id && put(root,"id",id); free(id);
    if(okay && clone) { const char* old=text(get(root,"name")); char* name=malloc(strlen(old)+20); if(name) { sprintf(name,"%s 副本",old); okay=put(root,"name",name); free(name); } else okay=0; }
    if(!okay) { xrtValueRelease(root); return 0; }
    if(!xrtValueArrayAppendNew(view->drafts,root)) return 0;
    view->index=(int)xrtValueCount(view->drafts)-1; return refreshConfigurations(app);
}
static void listAction(xide_app* app,int action)
{
    xide_configuration_view* view=app->configurationView; xvalue* values=list(view); int index=xuiListViewGetSelected(view->controls[XIDE_CONFIG_LIST]);
    const char *value=xuiEditGetText(view->controls[XIDE_CONFIG_VALUE]),*key=xuiEditGetText(view->controls[XIDE_CONFIG_KEY]);
    if(action!=XIDE_CONFIG_ADD && (index<0 || (size_t)index>=xrtValueCount(values))) { message(view,"请先选择一个列表项。"); return; }
    if(!values) { message(view,"无法创建配置列表。"); return; }
    int okay=0;
    if(action==XIDE_CONFIG_ADD || action==XIDE_CONFIG_UPDATE) {
        if(view->category!=3 && view->category!=9 && !isEnvironment(view) && !*value) { message(view,"路径或库名称不能为空。"); return; }
        if((view->category==7 || view->category==8) && !xideCAnalysisMacroValid(value,view->category==7)) { message(view,"C 宏使用名称或名称=值；名称为 ASCII 标识符，值不允许换行。取消定义只填写名称。"); return; }
        if(view->category>=5 && view->category<=8 && action==XIDE_CONFIG_ADD && xrtValueCount(values)>=128) { message(view,"每类 C 分析输入最多 128 项。"); return; }
        if(isEnvironment(view)) {
            if(!*key || strchr(key,'=')) { message(view,"环境变量名称不能为空或含等号；值可以为空。"); return; }
            xstrview old={0}; if(action==XIDE_CONFIG_UPDATE) xrtValueObjectAt(values,(size_t)index,&old);
            for(size_t i=0;i<xrtValueCount(values);++i) {
                xstrview existing; xrtValueObjectAt(values,i,&existing); char* name=malloc(existing.Size+1);
                if(!name) { message(view,"内存不足。"); return; } memcpy(name,existing.Data,existing.Size); name[existing.Size]=0;
                int duplicate=!_stricmp(name,key) && (action==XIDE_CONFIG_ADD || (int)i!=index); free(name);
                if(duplicate) { message(view,"该环境变量已存在；请选择后更新。"); return; }
            }
            xvalue* replacement=xrtValueClone(values); okay=replacement && put(replacement,key,value);
            if(okay && action==XIDE_CONFIG_UPDATE && (old.Size!=strlen(key) || memcmp(old.Data,key,old.Size))) okay=xrtValueObjectRemove(replacement,old);
            if(okay) okay=xrtValueObjectSetNew(get(current(view),view->category==10 ? "test" : "run"),sv("environment"),replacement); else xrtValueRelease(replacement);
        } else okay=action==XIDE_CONFIG_ADD ? xrtValueArrayAppendNew(values,xrtValueString(sv(value))) : xrtValueArraySetNew(values,(size_t)index,xrtValueString(sv(value)));
        if(action==XIDE_CONFIG_ADD) index=(int)xrtValueCount(list(view))-1;
    } else if(action==XIDE_CONFIG_REMOVE) {
        if(isEnvironment(view)) { xstrview keyView; xrtValueObjectAt(values,(size_t)index,&keyView); okay=xrtValueObjectRemove(values,keyView); }
        else okay=xrtValueArrayRemove(values,(size_t)index,1);
    } else if(!isEnvironment(view)) {
        int next=index+(action==XIDE_CONFIG_UP ? -1 : 1); if(next<0 || (size_t)next>=xrtValueCount(values)) return;
        okay=xrtValueArraySwap(values,(size_t)index,(size_t)next); if(okay) index=next;
    }
    if(!okay || !refreshRows(app,index)) message(view,"无法更新列表，配置草稿仍未应用。");
    else message(view,view->category>=9 ? "测试配置草稿已更新；应用后 Ctrl+F7 运行，打开项目不会执行。" : view->category>=5 ? "C 分析草稿已更新；应用后 clangd 重新分析。这些专用设置不更改 TCC 构建参数。" : "列表草稿已更新。应用后保存项目，构建运行使用该配置。");
}
int xideConfigurationViewApply(xide_app* app,xide_model_error* error)
{
    xide_configuration_view* view=app->configurationView; if(!view || !view->drafts || !storeFields(view)) return 0;
    xide_project* project=xideWorkspaceFindProject(&app->workspaceModel,view->project); xvalue* now=xideProjectConfigurationsJson(project);
    int same=project && !strcmp(project->manifest,view->manifest) && now && xrtValueEqual(now,view->baseline); xrtValueRelease(now);
    if(!same) { if(error) { error->code=XIDE_MODEL_CONFLICT; snprintf(error->message,sizeof(error->message),"Project/configurations changed while editing; cancel and reopen the configuration editor."); } return 0; }
    const char* active=text(get(current(view),"id"));
    if(!xideWorkspaceViewApplyConfigurations(app,view->project,view->drafts,active,error)) return 0;
    xideDialogSetOpen(view->window,0); xgeRenderRequest(); return 1;
}
static void clicked(xui_widget widget,void* user)
{
    xide_app* app=user; xide_configuration_view* view=app->configurationView; int action=-1;
    for(int i=0;i<XIDE_CONFIG_CONTROL_COUNT;++i) if(view->controls[i]==widget) { action=i; break; }
    if(action==XIDE_CONFIG_CANCEL) xideDialogSetOpen(view->window,0);
    else if(action==XIDE_CONFIG_APPLY) { xide_model_error error={0}; if(!xideConfigurationViewApply(app,&error)) message(view,*error.message ? error.message : "无法应用配置。"); }
    else if(action==XIDE_CONFIG_NEW || action==XIDE_CONFIG_CLONE) { if(!addConfiguration(app,action==XIDE_CONFIG_CLONE)) message(view,"无法创建配置草稿。"); }
    else if(action==XIDE_CONFIG_DELETE) {
        if(xrtValueCount(view->drafts)<=1) message(view,"请保留至少一个配置。");
        else { xrtValueArrayRemove(view->drafts,(size_t)view->index,1); if((size_t)view->index>=xrtValueCount(view->drafts)) --view->index; refreshConfigurations(app); }
    } else listAction(app,action);
    xgeRenderRequest();
}
static int label(xide_app* app,xui_widget* output,const char* value,xui_rect_t rect)
{
    xui_label_desc_t desc={0}; desc.iSize=sizeof(desc); desc.pFont=app->uiFont; desc.sText=value; desc.iWrapMode=XUI_TEXT_WRAP_WORD;
    xui_widget widget=NULL; TRY(xuiLabelCreate(app->context,&widget,&desc)); TRY(xuiWidgetSetRect(widget,rect)); TRY(xuiWidgetAddChild(app->configurationView->content,widget)); if(output) *output=widget; return XUI_OK;
}
static int input(xide_app* app,int control,const char* name,xui_rect_t rect)
{
    xui_input_desc_t desc={0}; desc.iSize=sizeof(desc); desc.pFont=app->uiFont; xui_widget* widget=&app->configurationView->controls[control];
    TRY(xuiInputCreate(app->context,widget,&desc)); TRY(xuiWidgetSetRect(*widget,rect)); TRY(xuiWidgetSetAccessibleName(*widget,name)); return xuiWidgetAddChild(app->configurationView->content,*widget);
}
static int button(xide_app* app,int control,const char* name,xui_rect_t rect)
{
    xui_button_desc_t desc={0}; desc.iSize=sizeof(desc); desc.pFont=app->uiFont; desc.sText=name; xui_widget* widget=&app->configurationView->controls[control];
    TRY(xuiButtonCreate(app->context,widget,&desc)); TRY(xuiWidgetSetRect(*widget,rect)); TRY(xuiButtonSetClick(*widget,clicked,app)); return xuiWidgetAddChild(app->configurationView->content,*widget);
}
static void windowClosed(xui_widget widget,void* user) { xide_app* a=user; clicked(a->configurationView->controls[XIDE_CONFIG_CANCEL],a); }
int xideConfigurationViewCreate(xide_app* app)
{
    xide_configuration_view* view=calloc(1,sizeof(*view)); if(!view) return XUI_ERROR_OUT_OF_MEMORY; app->configurationView=view; view->index=-1;
    TRY(xideDialogCreate(app,&view->window,"项目构建、运行、测试与 C 分析配置",736,624,14)); TRY(xideDialogSetCancel(view->window,windowClosed,app)); view->content=xideDialogGetContentWidget(view->window); TRY(xuiWidgetAddChild(app->root,view->window)); TRY(xuiWidgetSetLayoutType(view->content,XUI_LAYOUT_MANUAL));
    TRY(label(app,NULL,"项目构建、运行、测试与 C 分析配置",(xui_rect_t){0,0,736,26}));
    xui_combobox_desc_t combo={0}; combo.iSize=sizeof(combo); combo.pFont=app->uiFont;
    TRY(xuiComboBoxCreate(app->context,&view->controls[XIDE_CONFIG_PICKER],&combo)); TRY(xuiWidgetSetRect(view->controls[XIDE_CONFIG_PICKER],(xui_rect_t){0,32,448,32}));
    TRY(xuiWidgetAddChild(view->content,view->controls[XIDE_CONFIG_PICKER])); TRY(xuiComboBoxSetSelect(view->controls[XIDE_CONFIG_PICKER],configSelected,app));
    TRY(button(app,XIDE_CONFIG_NEW,"新建",(xui_rect_t){456,32,84,32})); TRY(button(app,XIDE_CONFIG_CLONE,"复制",(xui_rect_t){554,32,84,32})); TRY(button(app,XIDE_CONFIG_DELETE,"删除",(xui_rect_t){652,32,84,32}));
    TRY(label(app,NULL,"配置名称",(xui_rect_t){0,74,360,20})); TRY(input(app,XIDE_CONFIG_NAME,"配置名称",(xui_rect_t){0,96,360,32})); TRY(label(app,&view->identity,"配置 ID",(xui_rect_t){374,96,362,32}));
    TRY(label(app,&view->outputLabel,"输出路径（留空使用默认产物路径）",(xui_rect_t){0,136,736,20})); TRY(input(app,XIDE_CONFIG_OUTPUT,"输出路径",(xui_rect_t){0,158,736,32}));
    TRY(label(app,&view->directoryLabel,"运行工作目录",(xui_rect_t){0,198,736,20})); TRY(input(app,XIDE_CONFIG_DIRECTORY,"运行工作目录",(xui_rect_t){0,220,736,32}));
    TRY(input(app,XIDE_CONFIG_TEST_PROGRAM,"测试程序 (.exe)",(xui_rect_t){0,158,736,32})); TRY(input(app,XIDE_CONFIG_TEST_DIRECTORY,"测试工作目录",(xui_rect_t){0,220,736,32}));
    TRY(xuiWidgetSetVisible(view->controls[XIDE_CONFIG_TEST_PROGRAM],0)); TRY(xuiWidgetSetVisible(view->controls[XIDE_CONFIG_TEST_DIRECTORY],0));
    static const char* categories[]={"头文件搜索路径","库搜索路径","链接库","运行参数","环境变量",
#if XIDE_ENABLE_C_LSP
        "C 分析：系统头目录","C 分析：强制包含头文件","C 分析：宏定义","C 分析：取消宏定义",
#endif
        "测试参数与程序","测试环境变量"};
    combo.arrItems=categories; combo.iItemCount=(int)(sizeof(categories)/sizeof(*categories)); combo.fPopupHeight=360; combo.fPopupMaxHeight=420;
    TRY(xuiComboBoxCreate(app->context,&view->controls[XIDE_CONFIG_CATEGORY],&combo)); TRY(xuiWidgetSetRect(view->controls[XIDE_CONFIG_CATEGORY],(xui_rect_t){0,268,360,32}));
    TRY(xuiWidgetAddChild(view->content,view->controls[XIDE_CONFIG_CATEGORY])); TRY(xuiComboBoxSetSelect(view->controls[XIDE_CONFIG_CATEGORY],categorySelected,app));
    TRY(label(app,NULL,"参数保持原文，不需要手动添加 shell 引号。",(xui_rect_t){374,268,362,32}));
    xui_list_view_desc_t desc={0}; desc.iSize=sizeof(desc); desc.pFont=app->uiFont; desc.fItemHeight=28; desc.fPadding=6;
    TRY(xuiListViewCreate(app->context,&view->controls[XIDE_CONFIG_LIST],&desc)); TRY(xuiWidgetSetRect(view->controls[XIDE_CONFIG_LIST],(xui_rect_t){0,308,736,128}));
    TRY(xuiWidgetAddChild(view->content,view->controls[XIDE_CONFIG_LIST])); TRY(xuiListViewSetSelect(view->controls[XIDE_CONFIG_LIST],rowSelected,app));
    TRY(input(app,XIDE_CONFIG_KEY,"环境变量名称",(xui_rect_t){0,444,180,32})); TRY(input(app,XIDE_CONFIG_VALUE,"列表项值",(xui_rect_t){0,444,736,32}));
    static const char* actions[]={"添加","更新所选","移除所选","上移","下移"};
    for(int i=0;i<5;++i) TRY(button(app,XIDE_CONFIG_ADD+i,actions[i],(xui_rect_t){i*149,484,140,32}));
    TRY(label(app,&view->message,"",(xui_rect_t){0,526,736,44}));
    TRY(button(app,XIDE_CONFIG_APPLY,"应用",(xui_rect_t){520,580,100,32})); TRY(button(app,XIDE_CONFIG_CANCEL,"取消",(xui_rect_t){636,580,100,32}));
    return xideUiLayoutCapture(&view->layout,view->content);
}
int xideConfigurationViewOpen(xide_app* app) { return xideConfigurationViewOpenProject(app,app->selectedProject); }
int xideConfigurationViewOpenProject(xide_app* app,const char* projectId)
{
    xide_configuration_view* view=app->configurationView; xide_project* project=xideWorkspaceFindProject(&app->workspaceModel,projectId);
    if(!view || !project || !project->available || xideToolchainViewIsOpen(app) || xideResourceViewIsOpen(app) || xideImportViewIsOpen(app) || xideRenameViewIsOpen(app) || xideTemplateViewIsOpen(app) || xideReferencesViewIsOpen(app) || xideFontViewIsOpen(app) || xideRecoveryViewIsOpen(app) || xideFileChangesViewIsOpen(app)) return 0;
    xvalue *drafts=xideProjectConfigurationsJson(project),*baseline=drafts ? xrtValueDeepClone(drafts) : NULL; char *id=copy(project->id),*manifest=copy(project->manifest);
    if(!drafts || !baseline || !id || !manifest) { xrtValueRelease(drafts); xrtValueRelease(baseline); free(id); free(manifest); return 0; }
    xrtValueRelease(view->drafts); xrtValueRelease(view->baseline); free(view->project); free(view->manifest); view->drafts=drafts; view->baseline=baseline; view->project=id; view->manifest=manifest;
    view->index=project->configurationCount ? 0 : -1; view->category=0;
    for(size_t i=0;i<project->configurationCount;++i) if(!strcmp(project->configurations[i].id,project->activeConfiguration)) view->index=(int)i;
    view->sync=1; xuiComboBoxSetSelected(view->controls[XIDE_CONFIG_CATEGORY],0); view->sync=0;
    if(view->index<0) { if(!addConfiguration(app,0)) return 0; } else if(!refreshConfigurations(app)) return 0;
    xideConfigurationViewLayout(app); xideDialogSetOpen(view->window,1); xuiSetFocusWidget(app->context,view->controls[XIDE_CONFIG_NAME]); xgeRenderRequest(); return 1;
}
int xideConfigurationViewIsOpen(xide_app* app) { return app->configurationView && xideDialogIsOpen(app->configurationView->window); }
void xideConfigurationViewLayout(xide_app* app)
{
    if(!app->configurationView) return;
    xide_configuration_view* view=app->configurationView; int width=app->viewportWidth-48,height=app->viewportHeight-48;
    if(width<200) width=200;
    if(height<200) height=200;
    xideDialogSetMaxSize(view->window,(float)width,(float)height);
    float scale=xideUiFontScale(app);
    xuiComboBoxSetPopupHeight(view->controls[XIDE_CONFIG_CATEGORY],(float)xideUiHeight(app,360));
    xuiComboBoxSetPopupMaxHeight(view->controls[XIDE_CONFIG_CATEGORY],(float)height);
    if(view->scale!=scale) { xideUiLayoutApply(app,&view->layout,1); xideDialogSetContentSize(view->window,xideUiHeight(app,736),xideUiHeight(app,624)); view->scale=scale; }
    xuiWidgetSetRect(view->controls[XIDE_CONFIG_VALUE],xideUiRect(app,(xui_rect_t){isEnvironment(view) ? 188 : 0,444,isEnvironment(view) ? 548 : 736,32}));

}
xui_widget xideConfigurationViewControl(xide_app* app,int control) { return app->configurationView && control>=0 && control<XIDE_CONFIG_CONTROL_COUNT ? app->configurationView->controls[control] : NULL; }
xui_widget xideConfigurationViewWindow(xide_app* app) { return app->configurationView ? app->configurationView->window : NULL; }
void xideConfigurationViewDestroy(xide_app* app)
{
    xide_configuration_view* view=app->configurationView; if(!view) return;
    clearTitles(&view->titles,&view->titleCount); clearTitles(&view->rows,&view->rowCount); xrtValueRelease(view->drafts); xrtValueRelease(view->baseline); free(view->project); free(view->manifest); free(view); app->configurationView=NULL;
}
