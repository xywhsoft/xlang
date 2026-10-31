#include "ui/main_toolbar.h"
#include "services/execution_target.h"
#include "services/ui_font.h"
#include "ui/configuration_view.h"
#include "ui/workspace_view.h"
#include "ui/task_view.h"
#include "ui/editor_view.h"
#include "platform/path.h"
#include "theme/theme.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TRY(call) do { int r_=(call);if(r_!=XUI_OK)return r_; } while(0)
enum menu_kind { MENU_NEW,MENU_OPEN,MENU_SAVE,MENU_TARGET,MENU_CONFIG,MENU_CONTEXT,MENU_STOP,MENU_TASKS,MENU_MORE,MENU_LANGUAGE };
typedef struct choice { int action,mode;char *project,*configuration,*path;uint64_t task; } choice;
struct xide_main_toolbar {
    xui_widget controls[XIDE_TB_COUNT],menu;
    xui_surface icons[9];float iconScale;
    choice* choices;size_t count,capacity;
    int layoutMode;
    int iconEnabled[XIDE_TB_COUNT];
    char tips[XIDE_TB_COUNT][2048];char* recentPath;
    char captions[XIDE_TB_COUNT][1600];float captionWidth[XIDE_TB_COUNT];xui_font captionFont[XIDE_TB_COUNT];
};
static const int actions[XIDE_TB_COUNT]={XIDE_ACTION_NEW_FILE,0,XIDE_ACTION_OPEN_FILE,0,XIDE_ACTION_SAVE_FILE,0,0,0,0,XIDE_ACTION_BUILD,XIDE_ACTION_RUN,XIDE_ACTION_TEST,XIDE_ACTION_STOP,0,0,0};
static const char* names[XIDE_TB_COUNT]={"新建文件","新建菜单","打开文件","打开菜单","保存文件","保存菜单","执行目标","执行配置","目标与配置","构建","运行","测试","停止当前任务","停止菜单","任务与输出","更多操作"};
/* Shared monochrome paths are rendered once per DPI generation by XUI, and
   tinted by the native Button. No emoji/font glyph substitutes or per-frame SVG. */
static const char* paths[]={
    "M4 2H11L14 5V14H4Z M10 2V6H14 M9 8V12 M7 10H11",
    "M2 5V3H6L8 5H14V13H2Z M2 7H14",
    "M3 2H12L14 4V14H2V2Z M5 2V6H11V2 M5 14V9H11V14",
    "M3 13L9 7 M7 3L9 1L15 7L13 9Z M2 12L4 14",
    "M5 2L14 8L5 14Z",
    "M2 8L6 12L14 4",
    "M4 4H12V12H4Z",
    "M4 6L8 10L12 6",
    "M3 8H3.1 M8 8H8.1 M13 8H13.1"
};
static char* copy(const char* text) { if(!text)text="";size_t n=strlen(text)+1;char* p=malloc(n);if(p)memcpy(p,text,n);return p; }
static void clearChoices(xide_main_toolbar* v)
{
    for(size_t i=0;i<v->count;++i) { free(v->choices[i].project);free(v->choices[i].configuration);free(v->choices[i].path); }
    v->count=0;
}
static void label(xui_widget widget,const char* text) { if(strcmp(xuiButtonGetText(widget),text))xuiButtonSetText(widget,text); }
static void fitted(xide_app* app,int i,const char* text)
{
    xide_main_toolbar* v=app->mainToolbar;xui_widget button=v->controls[i];float width=xuiWidgetGetRect(button).fW;xui_font font=xuiButtonGetFont(button);
    if(!strcmp(v->captions[i],text) && v->captionWidth[i]==width && v->captionFont[i]==font)return;
    if(text!=v->captions[i])snprintf(v->captions[i],sizeof(v->captions[i]),"%s",text);
    v->captionWidth[i]=width;v->captionFont[i]=font;text=v->captions[i];xui_vec2_t size={0};
    if(width<=0 || !font || !app->proxy.textMeasure || xideDpiTextMeasure(&app->proxy,font,text,&size)!=XUI_OK || size.fX<=width-16*xideUiFontScale(app)) { label(button,text);return; }
    char shortened[1604];size_t low=0,high=strlen(text);float available=fmaxf(0,width-16*xideUiFontScale(app));
    while(low<high) {
        size_t middle=(low+high+1)/2,cut=middle;while(cut && ((unsigned char)text[cut]&0xc0)==0x80)--cut;
        memcpy(shortened,text,cut);memcpy(shortened+cut,"…",4);
        if(xideDpiTextMeasure(&app->proxy,font,shortened,&size)==XUI_OK && size.fX<=available)low=middle;else high=middle-1;
    }
    while(low && ((unsigned char)text[low]&0xc0)==0x80)--low;
    memcpy(shortened,text,low);memcpy(shortened+low,"…",4);label(button,shortened);
}
static void tooltip(xide_main_toolbar* v,int i,const char* text)
{
    if(strcmp(v->tips[i],text)) {
        snprintf(v->tips[i],sizeof(v->tips[i]),"%s",text);
        xuiWidgetSetTooltipText(v->controls[i],v->tips[i]);xuiWidgetSetAccessibleDescription(v->controls[i],v->tips[i]);
    }
}
static int renderIcons(xide_app* app)
{
    xide_main_toolbar* v=app->mainToolbar;float scale=app->dpiScale>0 ? app->dpiScale : 1;
    if(!app->proxy.surfaceCreate || !app->proxy.surfaceDestroy || !app->proxy.drawSvgPath || (v->icons[0] && scale==v->iconScale))return XUI_OK;
    xui_surface fresh[9]={0};int result=XUI_OK;
    for(int i=0;i<9 && result==XUI_OK;++i) {
        xui_surface_desc_t desc={XUI_SURFACE_KIND_TEXTURE,XUI_SURFACE_FORMAT_RGBA8,20,20,XUI_SURFACE_USAGE_TARGET};xui_painter painter=NULL;
        result=app->proxy.surfaceCreate(&app->proxy,&fresh[i],&desc);
        if(result==XUI_OK)result=xuiPainterBegin(app->context,fresh[i],&painter);
        if(result==XUI_OK)result=xuiPainterClearRect(painter,(xui_rect_t){0,0,20,20},0);
        xui_path_style_t style={0};style.iSize=sizeof(style);style.iStrokeColor=0xffffffffu;style.fStrokeWidth=i==8?2.4f:1.4f;
        style.iLineCap=XUI_PATH_CAP_ROUND;style.iLineJoin=XUI_PATH_JOIN_ROUND;
        style.iFillColor=i==4 || i==6 ? 0xffffffffu : 0;
        if(result==XUI_OK)result=xuiPainterDrawSvgPath(painter,paths[i],(xui_rect_t){0,0,16,16},(xui_rect_t){1,1,18,18},&style,0.15f);
        if(painter) { int end=xuiPainterEnd(painter);if(result==XUI_OK)result=end; }
    }
    if(result!=XUI_OK) { for(int i=0;i<9;++i)if(fresh[i])app->proxy.surfaceDestroy(&app->proxy,fresh[i]);return result; }
    const int iconIndex[XIDE_TB_COUNT]={0,7,1,7,2,7,-1,-1,-1,3,4,5,6,7,-1,8};
    for(int i=0;i<XIDE_TB_COUNT;++i)if(iconIndex[i]>=0) {
        result=xuiButtonSetIcon(v->controls[i],fresh[iconIndex[i]],(xui_rect_t){0,0,20,20});
        if(result!=XUI_OK) {
            for(int j=0;j<i;++j)if(iconIndex[j]>=0)(void)xuiButtonSetIcon(v->controls[j],v->icons[iconIndex[j]],(xui_rect_t){0,0,20,20});
            for(int j=0;j<9;++j)app->proxy.surfaceDestroy(&app->proxy,fresh[j]);
            return result;
        }
    }
    for(int i=0;i<9;++i) { if(v->icons[i])app->proxy.surfaceDestroy(&app->proxy,v->icons[i]);v->icons[i]=fresh[i]; }
    v->iconScale=scale;return XUI_OK;
}
static int addChoice(xide_app* app,const char* title,int type,int enabled,int checked,choice item)
{
    xide_main_toolbar* v=app->mainToolbar;
    if(v->count==v->capacity) {
        size_t capacity=v->capacity ? v->capacity*2 : 32;if(capacity>INT_MAX/sizeof(choice))return XUI_ERROR_OUT_OF_MEMORY;
        choice* values=realloc(v->choices,capacity*sizeof(*values));if(!values)return XUI_ERROR_OUT_OF_MEMORY;v->choices=values;v->capacity=capacity;
    }
    choice owned=item;owned.project=item.project ? copy(item.project) : NULL;owned.configuration=item.configuration ? copy(item.configuration) : NULL;owned.path=item.path ? copy(item.path) : NULL;
    if((item.project && !owned.project) || (item.configuration && !owned.configuration) || (item.path && !owned.path)) { free(owned.project);free(owned.configuration);free(owned.path);return XUI_ERROR_OUT_OF_MEMORY; }
    xui_menu_item_t row={0};row.sText=title;row.iType=type;row.iValue=(int)v->count+1;row.iState=(enabled?XUI_MENU_ITEM_ENABLED:0)|(checked?XUI_MENU_ITEM_CHECKED:0);
    const xide_action_info* action=xideActionFind(item.action);if(action)row.sShortcut=action->shortcut;
    int result=xuiMenuAddItem(v->menu,&row);
    if(result<0) { free(owned.project);free(owned.configuration);free(owned.path);return result; }
    v->choices[v->count++]=owned;return XUI_OK;
}
static int addAction(xide_app* app,int action)
{
    if(!action)return addChoice(app,"",XUI_MENU_ITEM_SEPARATOR,0,0,(choice){0});
    const xide_action_info* info=xideActionFind(action);xide_action_state state=xideActionQuery(app,action);
    return info ? addChoice(app,info->title,XUI_MENU_ITEM_NORMAL,state.enabled,0,(choice){.action=action}) : XUI_ERROR_INVALID_ARGUMENT;
}
static int targetChoices(xide_app* app)
{
    xide_execution_context c;xideExecutionResolve(app,0,&c);int mode=xideExecutionMode(app);char text[768];
    snprintf(text,sizeof(text),"启动项目：%s",app->workspaceModel.startupProject && *app->workspaceModel.startupProject ? app->workspaceModel.startupProject : "未设置");
    xide_project* startup=xideWorkspaceFindProject(&app->workspaceModel,app->workspaceModel.startupProject);if(startup)snprintf(text,sizeof(text),"启动项目：%s",startup->name);
    TRY(addChoice(app,text,XUI_MENU_ITEM_RADIO,1,mode==XIDE_TARGET_STARTUP || (mode==XIDE_TARGET_AUTO && !c.file),(choice){.mode=XIDE_TARGET_STARTUP,.project=""}));
    TRY(addChoice(app,"当前文件（跟随编辑标签）",XUI_MENU_ITEM_RADIO,1,mode==XIDE_TARGET_FILE || (mode==XIDE_TARGET_AUTO && c.file),(choice){.mode=XIDE_TARGET_FILE,.project=""}));
    for(size_t i=0;i<app->workspaceModel.projectCount;++i) {
        xide_project* p=&app->workspaceModel.projects[i];snprintf(text,sizeof(text),"项目：%s%s",p->name,p->available ? "" : "（不可用）");
        TRY(addChoice(app,text,XUI_MENU_ITEM_RADIO,p->available,mode==XIDE_TARGET_PROJECT && !strcmp(p->id,xideExecutionProject(app)),(choice){.mode=XIDE_TARGET_PROJECT,.project=p->id}));
    }
    return XUI_OK;
}
static int configChoices(xide_app* app)
{
    xide_execution_context c;xideExecutionResolve(app,0,&c);
    if(c.project && c.project->available)for(size_t i=0;i<c.project->configurationCount;++i) {
        xide_configuration* config=&c.project->configurations[i];
        TRY(addChoice(app,config->name,XUI_MENU_ITEM_RADIO,1,c.configuration==config,(choice){.project=c.project->id,.configuration=config->id}));
    }
    else TRY(addChoice(app,c.file ? "单文件默认配置" : "配置不可用",XUI_MENU_ITEM_NORMAL,0,0,(choice){0}));
    TRY(addAction(app,0));return addAction(app,XIDE_ACTION_TARGET_CONFIGURATIONS);
}
static const char* taskState(xide_task_info info)
{
    if(info.state==XIDE_TASK_QUEUED)return "排队中";
    if(info.state==XIDE_TASK_CANCELLING)return "停止中";
    if(info.state==XIDE_TASK_SUCCEEDED)return "已完成";
    if(info.state==XIDE_TASK_FAILED)return "失败";
    if(info.state==XIDE_TASK_CANCELLED)return "已停止";
    return info.phase==XIDE_TASK_RUN ? "运行中" : info.phase==XIDE_TASK_TEST ? "测试中" : "构建中";
}
static int taskChoices(xide_app* app,int activeOnly)
{
    size_t count=xideTasksCount(app->tasks),added=0;
    for(size_t i=count;i>0;--i) {
        xide_task* task=xideTasksAt(app->tasks,i-1);xide_task_info info={0};xideTaskInfo(task,&info);if(activeOnly && info.state>=XIDE_TASK_SUCCEEDED)continue;
        char text[768];snprintf(text,sizeof(text),"%s · %s · #%llu",xideTaskProjectName(task),taskState(info),(unsigned long long)info.id);
        TRY(addChoice(app,text,XUI_MENU_ITEM_RADIO,1,task==xideTaskViewSelected(app),(choice){.task=info.id}));++added;
    }
    if(!added)TRY(addChoice(app,"没有活动任务",XUI_MENU_ITEM_NORMAL,0,0,(choice){0}));
    TRY(addAction(app,0));TRY(addAction(app,XIDE_ACTION_STOP));TRY(addAction(app,XIDE_ACTION_STOP_ALL));return addAction(app,XIDE_ACTION_SHOW_TASKS);
}
static int openMenu(xide_app* app,int kind,xui_widget owner)
{
    xide_main_toolbar* v=app->mainToolbar;if(!v)return XUI_ERROR_INVALID_STATE;
    TRY(xuiMenuClose(v->menu));TRY(xuiMenuClear(v->menu));clearChoices(v);TRY(xideUiFontBindMenu(app,v->menu));
    const int newActions[]={XIDE_ACTION_NEW_FILE,XIDE_ACTION_NEW_PROJECT,XIDE_ACTION_NEW_WORKSPACE};
    const int openActions[]={XIDE_ACTION_OPEN_FILE,XIDE_ACTION_OPEN_PROJECT,XIDE_ACTION_OPEN_WORKSPACE};
    const int saveActions[]={XIDE_ACTION_SAVE_FILE,XIDE_ACTION_SAVE_ALL_FILES,XIDE_ACTION_SAVE_FILE_AS};
    if(kind==MENU_NEW || kind==MENU_OPEN || kind==MENU_SAVE) {
        const int* items=kind==MENU_NEW ? newActions : kind==MENU_OPEN ? openActions : saveActions;
        for(int i=0;i<3;++i)TRY(addAction(app,items[i]));
        if(kind==MENU_OPEN) {
            const xvalue* recent=app->preferences.source ? xrtValueObjectGet(app->preferences.source,(xstrview){"recent",6}) : NULL;
            if(xrtValueIs(recent,XVALUE_ARRAY) && xrtValueCount(recent)) {
                TRY(addAction(app,0));TRY(addChoice(app,"最近打开",XUI_MENU_ITEM_NORMAL,0,0,(choice){0}));
                for(size_t i=0;i<xrtValueCount(recent) && i<12;++i) {
                    xstrview path={0};if(!xrtValueGetString(xrtValueArrayGet(recent,i),&path) || path.Size>8192 || memchr(path.Data,0,path.Size))continue;
                    TRY(addChoice(app,path.Data,XUI_MENU_ITEM_NORMAL,xidePathExists(path.Data),0,(choice){.path=(char*)path.Data}));
                }
            }
        }
    } else if(kind==MENU_TARGET)TRY(targetChoices(app));
    else if(kind==MENU_CONFIG)TRY(configChoices(app));
    else if(kind==MENU_CONTEXT) { TRY(targetChoices(app));TRY(addAction(app,0));TRY(configChoices(app)); }
    else if(kind==MENU_STOP || kind==MENU_TASKS)TRY(taskChoices(app,kind==MENU_STOP));
    else if(kind==MENU_LANGUAGE) {
        const int items[]={XIDE_ACTION_LANGUAGE_C,XIDE_ACTION_LANGUAGE_XLANG,XIDE_ACTION_LANGUAGE_TEXT};for(int i=0;i<3;++i)TRY(addAction(app,items[i]));
    } else {
        const int hidden[]={XIDE_TB_NEW,XIDE_TB_OPEN,XIDE_TB_SAVE,XIDE_TB_BUILD,XIDE_TB_TEST};int added=0;
        for(int i=0;i<5;++i)if(!xuiWidgetGetVisible(v->controls[hidden[i]])) { TRY(addAction(app,actions[hidden[i]]));if(hidden[i]==XIDE_TB_SAVE)TRY(addAction(app,XIDE_ACTION_SAVE_ALL_FILES));++added; }
        if(added)TRY(addAction(app,0));
        const int items[]={XIDE_ACTION_TARGET_CONFIGURATIONS,XIDE_ACTION_COMPILER,XIDE_ACTION_TOOLCHAIN_INFO,XIDE_ACTION_SHOW_TASKS};
        for(size_t i=0;i<sizeof(items)/sizeof(*items);++i)TRY(addAction(app,items[i]));
    }
    xui_rect_t rect=xuiWidgetGetWorldRect(owner);return xuiMenuOpenAt(v->menu,owner,rect.fX,rect.fY+rect.fH);
}
static void selected(xui_widget widget,int index,int value,void* user)
{
    (void)widget;(void)index;xide_app* app=user;xide_main_toolbar* v=app->mainToolbar;if(value<1 || (size_t)value>v->count)return;
    choice c=v->choices[value-1];int okay=1;
    if(c.action)xideActionExecute(app,c.action);
    else if(c.configuration)okay=xideExecutionSelectConfiguration(app,c.project,c.configuration);
    else if(c.project)okay=xideExecutionSelect(app,c.mode,c.project);
    else if(c.task) {
        for(size_t i=0;i<xideTasksCount(app->tasks);++i) { xide_task* t=xideTasksAt(app->tasks,i);xide_task_info info={0};xideTaskInfo(t,&info);if(info.id==c.task) { xideTaskViewSelect(app,t);xideMainToolbarAction(app,XIDE_ACTION_SHOW_TASKS);break; } }
    } else if(c.path) {
        char* path=copy(c.path);if(path) { free(v->recentPath);v->recentPath=path;xideActionExecute(app,XIDE_ACTION_OPEN_RECENT); }
        else okay=0;
    }
    if(!okay && !c.path)xideMessageShow(app,"执行上下文已变化","目标或配置已不存在，请重新选择。",XUI_MSGBOX_ICON_ERROR);
    xideMainToolbarRefresh(app);xideActionsRefresh(app);xgeRenderRequest();
}
static void clicked(xui_widget widget,void* user)
{
    xide_app* app=user;xide_main_toolbar* v=app->mainToolbar;int i=0;while(i<XIDE_TB_COUNT && widget!=v->controls[i])++i;if(i==XIDE_TB_COUNT)return;
    if(i==XIDE_TB_TASKS && !xideTaskViewCompiler(app)) { xideActionExecute(app,XIDE_ACTION_COMPILER);return; }
    const int menus[XIDE_TB_COUNT]={-1,MENU_NEW,-1,MENU_OPEN,-1,MENU_SAVE,MENU_TARGET,MENU_CONFIG,MENU_CONTEXT,-1,-1,-1,-1,MENU_STOP,MENU_TASKS,MENU_MORE};
    if(menus[i]>=0) { int result=openMenu(app,menus[i],widget);if(result!=XUI_OK)app->frameError=result; }
}
static int tokenStyle(xui_widget widget,const char* icon,const char* fill)
{
    const char* keys[]={"button.icon_color","button.normal_color"};const char* values[]={icon,fill};xui_style_property_t props[2]={0};
    for(int i=0;i<2;++i) { props[i].iSize=sizeof(props[i]);props[i].sName=keys[i];props[i].iDirtyFlags=XUI_WIDGET_DIRTY_CACHE|XUI_WIDGET_DIRTY_RENDER;props[i].tValue.iSize=sizeof(props[i].tValue);props[i].tValue.iType=XUI_STYLE_VALUE_TOKEN;props[i].tValue.sText=values[i]; }
    return xuiWidgetSetInlineStyle(widget,props,2);
}
int xideMainToolbarCreate(xide_app* app)
{
    xide_main_toolbar* v=calloc(1,sizeof(*v));if(!v)return XUI_ERROR_OUT_OF_MEMORY;app->mainToolbar=v;v->layoutMode=-1;
    TRY(xuiWidgetSetLayoutType(app->toolbar,XUI_LAYOUT_MANUAL));
    for(int i=0;i<XIDE_TB_COUNT;++i) {
        xui_button_desc_t desc={0};desc.iSize=sizeof(desc);desc.pFont=app->uiFont;desc.sText=names[i];
        TRY(xuiButtonCreate(app->context,&v->controls[i],&desc));TRY(xuiWidgetAddChild(app->toolbar,v->controls[i]));
        TRY(xuiWidgetSetAccessibleName(v->controls[i],names[i]));TRY(xuiButtonSetTextFlags(v->controls[i],XUI_TEXT_ALIGN_CENTER|XUI_TEXT_ALIGN_MIDDLE|XUI_TEXT_CLIP));
        if(actions[i])TRY(xideActionsButton(app,v->controls[i],actions[i]));else TRY(xuiButtonSetClick(v->controls[i],clicked,app));
        TRY(tokenStyle(v->controls[i],i==XIDE_TB_RUN ? "ide.accent" : i==XIDE_TB_STOP ? "ide.error" : "ide.text",i==XIDE_TB_RUN ? "ide.selection" : i>=XIDE_TB_TARGET && i<=XIDE_TB_CONTEXT ? "ide.field" : "ide.bg"));
        v->iconEnabled[i]=-1;
    }
    xui_menu_desc_t menu={0};menu.iSize=sizeof(menu);menu.pFont=app->uiFont;menu.pOwner=app->toolbar;
    TRY(xuiMenuCreate(app->context,&v->menu,&menu));TRY(xuiMenuSetSelect(v->menu,selected,app));
    return XUI_OK;
}
int xideMainToolbarRefresh(xide_app* app)
{
    xide_main_toolbar* v=app->mainToolbar;if(!v)return XUI_OK;
    TRY(renderIcons(app));xide_execution_context context;xideExecutionResolve(app,0,&context);char text[1600];
    snprintf(text,sizeof(text),"%s ▾",context.target);fitted(app,XIDE_TB_TARGET,text);
    snprintf(text,sizeof(text),"%s ▾",context.config);fitted(app,XIDE_TB_CONFIGURATION,text);
    snprintf(text,sizeof(text),"%s · %s ▾",context.target,context.config);fitted(app,XIDE_TB_CONTEXT,text);
    const char* location=context.file ? context.document && context.document->path ? context.document->path : "" : context.project ? context.project->manifest : "";
    snprintf(text,sizeof(text),"%s · %s\n%s",context.target,context.config,location);
    tooltip(v,XIDE_TB_TARGET,text);tooltip(v,XIDE_TB_CONFIGURATION,context.reason ? context.reason : context.config);tooltip(v,XIDE_TB_CONTEXT,text);
    for(int i=0;i<XIDE_TB_COUNT;++i) {
        if(actions[i]) {
            xide_action_state state=xideActionQuery(app,actions[i]);const xide_action_info* info=xideActionFind(actions[i]);
            if(v->iconEnabled[i]!=state.enabled) {
                TRY(tokenStyle(v->controls[i],!state.enabled ? "ide.disabled" : i==XIDE_TB_RUN ? "ide.accent" : i==XIDE_TB_STOP ? "ide.error" : "ide.text",i==XIDE_TB_RUN && state.enabled ? "ide.selection" : "ide.bg"));
                v->iconEnabled[i]=state.enabled;
            }
            snprintf(text,sizeof(text),"%s%s%s\n%s · %s%s%s",info->title,info->shortcut ? "  " : "",info->shortcut ? info->shortcut : "",context.target,context.config,state.reason ? "\n" : "",state.reason ? state.reason : "");
            if(i<XIDE_TB_TARGET) {
                const char* file=app->activeDocument && app->activeDocument->open ? app->activeDocument->path ? app->activeDocument->path : "未命名文件" : "未打开文件";
                snprintf(text,sizeof(text),"%s%s%s\n%s%s%s",info->title,info->shortcut ? "  " : "",info->shortcut ? info->shortcut : "",file,state.reason ? "\n" : "",state.reason ? state.reason : "");
            }
            if(i==XIDE_TB_STOP) {
                xide_task* task=xideTaskViewSelected(app);xide_task_info ti={0};xideTaskInfo(task,&ti);
                snprintf(text,sizeof(text),"停止当前任务  Shift+F5\n%s · #%llu%s%s",task ? xideTaskProjectName(task) : "未选择任务",(unsigned long long)ti.id,state.reason ? "\n" : "",state.reason ? state.reason : "");
            }
            tooltip(v,i,text);
        } else if(i<XIDE_TB_TARGET || i==XIDE_TB_STOP_MENU || i==XIDE_TB_MORE)tooltip(v,i,names[i]);
        if(!actions[i]) {
            int enabled=!xideActionsHasModal(app);if(xuiWidgetGetEnabled(v->controls[i])!=enabled)xuiWidgetSetEnabled(v->controls[i],enabled);
        }
    }
    size_t active=0;for(size_t i=0;i<xideTasksCount(app->tasks);++i) { xide_task_info ti={0};xideTaskInfo(xideTasksAt(app->tasks,i),&ti);if(ti.state<XIDE_TASK_SUCCEEDED)++active; }
    xide_task* task=xideTaskViewSelected(app);xide_task_info ti={0};xideTaskInfo(task,&ti);
    if(task)snprintf(text,sizeof(text),"%s · %s%s",xideTaskProjectName(task),taskState(ti),ti.state==XIDE_TASK_FAILED ? "（点击查看）" : "");
    else snprintf(text,sizeof(text),"%s",xideTaskViewCompiler(app) ? "就绪" : "工具链未配置");
    char status[1800];snprintf(status,sizeof(status),"%s\n%zu 个活动任务；点击选择任务与查看输出",text,active);tooltip(v,XIDE_TB_TASKS,status);
    char shortStatus[1024];
    if(v->layoutMode==2 && active>1)snprintf(shortStatus,sizeof(shortStatus),"任务 %zu ▾",active);
    else if(v->layoutMode==2 && task)snprintf(shortStatus,sizeof(shortStatus),"%s ▾",taskState(ti));
    else if(task)snprintf(shortStatus,sizeof(shortStatus),"%s · %s ▾",xideTaskProjectName(task),taskState(ti));
    else snprintf(shortStatus,sizeof(shortStatus),"%s ▾",xideTaskViewCompiler(app) ? "就绪" : "配置工具链");
    fitted(app,XIDE_TB_TASKS,shortStatus);
    return XUI_OK;
}
static void place(xui_widget widget,float* x,float y,float width,float height,float gap)
{
    xuiWidgetSetRect(widget,(xui_rect_t){*x,y,width,height});*x+=width+gap;
}
void xideMainToolbarLayout(xide_app* app)
{
    xide_main_toolbar* v=app->mainToolbar;if(!v)return;
    float scale=xideUiFontScale(app),width=xuiWidgetGetRect(app->toolbar).fW,height=xuiWidgetGetRect(app->toolbar).fH;
    int mode=width>=1120*scale ? 0 : width>=880*scale ? 1 : 2;v->layoutMode=mode;
    for(int i=0;i<XIDE_TB_COUNT;++i) {
        int visible=1;
        if(i<=XIDE_TB_SAVE_MENU)visible=mode==0 || (mode==1 && i>=XIDE_TB_SAVE);
        else if(i==XIDE_TB_TARGET || i==XIDE_TB_CONFIGURATION)visible=mode!=2;
        else if(i==XIDE_TB_CONTEXT)visible=mode==2;
        else if(i==XIDE_TB_BUILD || i==XIDE_TB_TEST)visible=mode!=2;
        if(xuiWidgetGetVisible(v->controls[i])!=visible)xuiWidgetSetVisible(v->controls[i],visible);
    }
    float x=8*scale,gap=4*scale,h=32*scale,y=(height-h)/2,icon=16*scale;
    const int fileButtons[]={XIDE_TB_NEW,XIDE_TB_NEW_MENU,XIDE_TB_OPEN,XIDE_TB_OPEN_MENU,XIDE_TB_SAVE,XIDE_TB_SAVE_MENU};
    for(int n=0;n<6;++n)if(xuiWidgetGetVisible(v->controls[fileButtons[n]])) { int i=fileButtons[n];label(v->controls[i],v->icons[0] ? "" : n%2 ? "▾" : i==XIDE_TB_NEW ? "新" : i==XIDE_TB_OPEN ? "开" : "存");place(v->controls[i],&x,y,(n%2?18:32)*scale,h,n%2?gap:0); }
    if(mode!=2)x+=12*scale;
    if(mode==0) { place(v->controls[XIDE_TB_TARGET],&x,y,220*scale,h,gap);place(v->controls[XIDE_TB_CONFIGURATION],&x,y,156*scale,h,12*scale); }
    else if(mode==1) { place(v->controls[XIDE_TB_TARGET],&x,y,190*scale,h,gap);place(v->controls[XIDE_TB_CONFIGURATION],&x,y,126*scale,h,12*scale); }
    else {
        float fixed=(width<520*scale?204:270)*scale;
        place(v->controls[XIDE_TB_CONTEXT],&x,y,fmaxf(40*scale,width-fixed),h,8*scale);
    }
    const int controls[]={XIDE_TB_BUILD,XIDE_TB_RUN,XIDE_TB_TEST,XIDE_TB_STOP,XIDE_TB_STOP_MENU};
    for(int n=0;n<5;++n) {
        int i=controls[n];if(!xuiWidgetGetVisible(v->controls[i]))continue;
        int textVisible=(mode==0 && n<3) || (i==XIDE_TB_RUN && width>=520*scale);
        label(v->controls[i],textVisible ? i==XIDE_TB_BUILD ? "构建" : i==XIDE_TB_RUN ? "运行" : "测试" : v->icons[0] ? "" : i==XIDE_TB_STOP_MENU ? "▾" : i==XIDE_TB_STOP ? "停" : i==XIDE_TB_RUN ? "运行" : i==XIDE_TB_BUILD ? "构建" : "测试");
        place(v->controls[i],&x,y,(i==XIDE_TB_STOP_MENU?18:textVisible?76:32)*scale,h,i==XIDE_TB_STOP?0:gap);
    }
    float moreWidth=32*scale,taskWidth=fmaxf(44*scale,fminf(180*scale,width-x-moreWidth-20*scale));
    float right=width-8*scale-moreWidth-taskWidth-gap;
    place(v->controls[XIDE_TB_TASKS],&right,y,taskWidth,h,gap);place(v->controls[XIDE_TB_MORE],&right,y,moreWidth,h,0);label(v->controls[XIDE_TB_MORE],v->icons[0] ? "" : "⋯");
    for(int i=0;i<XIDE_TB_COUNT;++i)xuiButtonSetIconLayout(v->controls[i],XUI_BUTTON_ICON_LEFT,i==XIDE_TB_NEW_MENU || i==XIDE_TB_OPEN_MENU || i==XIDE_TB_SAVE_MENU || i==XIDE_TB_STOP_MENU ? icon*0.65f:icon,5*scale);
    const int captions[]={XIDE_TB_TARGET,XIDE_TB_CONFIGURATION,XIDE_TB_CONTEXT,XIDE_TB_TASKS};
    for(int n=0;n<4;++n)fitted(app,captions[n],v->captions[captions[n]]);
}
int xideMainToolbarAction(xide_app* app,int action)
{
    if(action==XIDE_ACTION_OPEN_RECENT) {
        xide_main_toolbar* v=app->mainToolbar;if(!v || !v->recentPath)return 0;
        xide_model_error error={0};int project=xidePathHasExtension(v->recentPath,".xlp"),workspace=xidePathHasExtension(v->recentPath,".xlw"),okay=0;
        if(project || workspace) {
            if(!xideWorkspaceQuitRequest(app))return 1;
            app->sessionSwitchConfirmed=1;okay=xideWorkspaceOpen(app,v->recentPath,project,&error);app->sessionSwitchConfirmed=0;
        } else okay=xideEditorOpen(app,v->recentPath,&error);
        if(!okay)xideMessageShow(app,"打开失败",error.message,XUI_MSGBOX_ICON_ERROR);
        else if(app->preferencesInitialized && !xideSettingsRememberOpen(&app->preferences,v->recentPath,&error))xuiStatusBarSetItemText(app->statusBar,0,error.message);
        return 1;
    }
    if(action==XIDE_ACTION_TARGET_CONFIGURATIONS) {
        xide_execution_context context;xideExecutionResolve(app,0,&context);
        return context.project && xideConfigurationViewOpenProject(app,context.project->id);
    }
    if(action==XIDE_ACTION_SHOW_TASKS) {
        return xideTaskViewShow(app)==XUI_OK;
    }
    return 0;
}
void xideMainToolbarLanguageMenu(xide_app* app) { if(app->mainToolbar)openMenu(app,MENU_LANGUAGE,app->statusBar); }
xui_widget xideMainToolbarControl(xide_app* app,int control) { return app->mainToolbar && control>=0 && control<XIDE_TB_COUNT ? app->mainToolbar->controls[control] : NULL; }
xui_widget xideMainToolbarMenu(xide_app* app) { return app->mainToolbar ? app->mainToolbar->menu : NULL; }
void xideMainToolbarDestroy(xide_app* app)
{
    xide_main_toolbar* v=app->mainToolbar;if(!v)return;
    clearChoices(v);free(v->choices);free(v->recentPath);for(int i=0;i<9;++i)if(v->icons[i])app->proxy.surfaceDestroy(&app->proxy,v->icons[i]);free(v);app->mainToolbar=NULL;
}
