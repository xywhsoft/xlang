/* Home is part of the workbench translation unit; no editor/demo document. */
#include <windows.h>
#include <shellapi.h>
#include <stdlib.h>
#include "platform/path.h"
#include "services/template_registry.h"
struct xide_home_view {
    xui_widget root,caption,frame,newWindow,picker,controls[XIDE_HOME_CONTROL_COUNT];
    xide_settings snapshot;
    xui_table_view_column_t columns[3];
    char* recentPath;
    float scale;
    char times[XIDE_RECENT_LIMIT][40];
    xui_widget_event_proc keyHandler; void* keyUser;
    xide_template_registry templates;
    int templatesLoaded,category,visible[256],visibleCount,categoryDefinition[256],categoryCount;
    xui_surface icons[256];xui_rect_t iconSources[256];
    xui_surface categoryIcons[4];
    struct {float title,metadata,link;} headers[256];
    int linkPressed;uint64_t linkPointer;int linkPointerType;
    char (*descriptions)[2][1024];float descriptionWidth,templateScale;xui_font templateFont;
};
#include "ui/home_templates.inc"
const char* xideHomeTutorialUrl(void) { return "https://xcompiler.com/book/index.html"; }
xui_widget xideHomeControl(xide_app* a,int id) {
    return a && a->homeView && id>=0 && id<XIDE_HOME_CONTROL_COUNT ? a->homeView->controls[id]:NULL;
}
static int homeCount(xui_widget w,void* user) {
    (void)w;struct xide_home_view* v=((xide_app*)user)->homeView;
    return (int)xideSettingsRecentCount(&v->snapshot);
}
static int homeCell(xui_widget w,int row,int column,xui_table_view_cell_t* cell,void* user) {
    (void)w;struct xide_home_view* v=((xide_app*)user)->homeView;
    const char* path=row>=0 ? xideSettingsRecentPath(&v->snapshot,(size_t)row):NULL;
    if(!path || column<0 || column>=3)return XUI_ERROR_INVALID_ARGUMENT;
    cell->iType=XUI_TABLE_CELL_TYPE_TEXT;cell->bReadOnly=1;cell->sTooltip=path;
    cell->sText=column==0 ? xidePathFilename(path):column==1 ? v->times[row]:path;
    return XUI_OK;
}
int xideHomeRefresh(xide_app* a) {
    struct xide_home_view* v=a ? a->homeView:NULL;if(!v)return XUI_OK;
    if(v->snapshot.source==a->preferences.source)return XUI_OK;
    xvalue* source=a->preferences.source ? xrtValueRetain(a->preferences.source):NULL;
    xrtValueRelease(v->snapshot.source);v->snapshot.source=source;
    size_t count=xideSettingsRecentCount(&v->snapshot);
    for(size_t i=0;i<count;++i) {
        int64_t time=xideSettingsRecentTime(&v->snapshot,i);strcpy(v->times[i],"—");
        if(time>0 && time<INT64_C(253402300800)) {
            ULARGE_INTEGER n;n.QuadPart=((uint64_t)time+UINT64_C(11644473600))*UINT64_C(10000000);
            FILETIME ft={n.LowPart,n.HighPart};SYSTEMTIME utc,local;
            if(FileTimeToSystemTime(&ft,&utc) && SystemTimeToTzSpecificLocalTime(NULL,&utc,&local))
                snprintf(v->times[i],sizeof(v->times[i]),"%04u-%02u-%02u %02u:%02u:%02u",local.wYear,local.wMonth,local.wDay,local.wHour,local.wMinute,local.wSecond);
        }
    }
    char caption[128];snprintf(caption,sizeof(caption),"历史记录（双击或按 Enter 打开） · %zu / %d",count,XIDE_RECENT_LIMIT);
    TRY(xuiLabelSetText(v->caption,caption));TRY(xuiWidgetSetEnabled(v->controls[XIDE_HOME_CLEAR],count>0));
    TRY(xuiTableViewSetSelectedRow(v->controls[XIDE_HOME_HISTORY],-1));
    return xuiTableViewRefreshAdapter(v->controls[XIDE_HOME_HISTORY]);
}
void xideHomeRemember(xide_app* a,const char* path) {
    if(!a || !path || !a->preferencesInitialized)return;
    xide_model_error error={0};
    if(!xideSettingsRememberOpen(&a->preferences,path,&error))xuiStatusBarSetItemText(a->statusBar,0,error.message);
    int result=xideHomeRefresh(a);if(result!=XUI_OK)a->frameError=result;
}
int xideHomeOpenPath(xide_app* a,const char* path) {
    if(!a || !path || xideActionsHasModal(a))return 0;
    /* The source snapshot may be replaced by successful open/remember. */
    size_t n=strlen(path)+1;char* owned=malloc(n);if(!owned)return 0;memcpy(owned,path,n);
    int project=xidePathHasExtension(owned,".xlp"),workspace=xidePathHasExtension(owned,".xlw"),okay=0;
    xide_model_error error={0};
    if(project || workspace) {
        if(!xideWorkspaceQuitRequest(a)) {free(owned);return 0;}
        a->sessionSwitchConfirmed=1;okay=xideWorkspaceOpen(a,owned,project,&error);a->sessionSwitchConfirmed=0;
    } else okay=xideEditorOpen(a,owned,&error);
    if(!okay)xideMessageShow(a,"打开失败",error.message,XUI_MSGBOX_ICON_ERROR);
    free(owned);return okay;
}
static int homeEvent(xui_widget w,const xui_event_t* e,void* user) {
    xide_app* a=user;struct xide_home_view* v=a->homeView;int row=-1,column=-1;
    if(e->iType==XUI_EVENT_POINTER_DOUBLE_CLICK && e->iButton==XUI_POINTER_BUTTON_LEFT) {
        xui_rect_t world=xuiWidgetGetWorldRect(w);
        xuiTableViewGetItemAt(w,e->fX-world.fX,e->fY-world.fY,&row,&column);
    } else if(e->iType==XUI_EVENT_KEY_DOWN && (e->iKey==XUI_KEY_ENTER))row=xuiTableViewGetSelectedRow(w);
    if(row>=0) {
        const char* path=xideSettingsRecentPath(&v->snapshot,(size_t)row);
        if(path) {
            size_t size=strlen(path)+1;char* owned=malloc(size);
            if(owned) {memcpy(owned,path,size);free(v->recentPath);v->recentPath=owned;xideActionExecute(a,XIDE_ACTION_HOME_OPEN_RECENT);}
            return XUI_EVENT_DISPATCH_STOP;
        }
    }
    return e->iType==XUI_EVENT_KEY_DOWN && v->keyHandler ? v->keyHandler(w,e,v->keyUser):XUI_OK;
}
static void homeClear(xui_widget w,void* user) {
    (void)w;xideActionExecute(user,XIDE_ACTION_HOME_CLEAR);
}
int xideNewOpenAction(xide_app* a,int action) {
    if(action==XIDE_ACTION_NEW)return homeNewOpen(a);
    if(action==XIDE_ACTION_OPEN) {
        char* path=xideMessageChoosePath(a,XUI_FILE_DIALOG_MODE_OPEN_FILE,"打开文件、工程或工作空间",
            "xlang 文件、工程和工作空间|*.xlw;*.xlp;*.xl;*.c|所有文件|*.*",NULL,NULL,UINT64_C(716),0);
        if(path) {xideHomeOpenPath(a,path);free(path);}
        return 1;
    }
    return 0;
}
int xideHomeAction(xide_app* a,int action) {
    if(action==XIDE_ACTION_HOME_CLEAR) {
        if(!xideSettingsRecentCount(&a->preferences))return 1;
        int answer=xideMessageConfirm(a,"清空历史记录？","将清除最近访问记录，不会删除文件、工程或工作空间。",XUI_MSGBOX_BUTTON_OK_CANCEL,UINT64_C(719));
        xui_msgbox box=xideMessageCurrent(a);
        if(box){static const xui_msgbox_button_t buttons[]={{"关闭",XUI_MSGBOX_RESULT_CANCEL,XUI_BUTTON_SEMANTIC_DEFAULT},{"清空",XUI_MSGBOX_RESULT_OK,XUI_BUTTON_SEMANTIC_DEFAULT}};xuiMsgBoxSetCustomButtons(box,buttons,2);xuiSetFocusWidget(a->context,xuiMsgBoxGetButtonWidget(box,0));}
        if(answer!=XUI_MSGBOX_RESULT_OK)return 1;
        xide_model_error error={0};if(!xideSettingsClearRecent(&a->preferences,&error))xideMessageShow(a,"清空历史记录",error.message,XUI_MSGBOX_ICON_ERROR);
        int result=xideHomeRefresh(a);if(result!=XUI_OK)a->frameError=result;return 1;
    }
    if(action==XIDE_ACTION_HOME_OPEN_RECENT)return a->homeView && a->homeView->recentPath ? xideHomeOpenPath(a,a->homeView->recentPath):0;
    if(action==XIDE_ACTION_VIEW_HOME) {int r=xideWorkbenchShowHome(a);if(r!=XUI_OK)a->frameError=r;return r==XUI_OK;}
    if(action!=XIDE_ACTION_ONLINE_TUTORIAL)return 0;
    if((INT_PTR)ShellExecuteW(NULL,L"open",L"https://xcompiler.com/book/index.html",NULL,NULL,SW_SHOWNORMAL)<=32)
        xideMessageShow(a,"在线教程","无法打开浏览器，请检查默认浏览器设置。",XUI_MSGBOX_ICON_ERROR);
    return 1;
}
static int homeLayout(xui_widget root,xui_rect_t area,void* user) {
    xide_app* a=user;struct xide_home_view* v=a->homeView;float s=xideUiFontScale(a),pad=14*s,gap=8*s;
    float width=fmaxf(0,area.fW-2*pad),button=32*s,y=area.fY+pad;
    float bw=fminf(104*s,fmaxf(0,(width-gap)/2)),tutorial=fminf(width,104*s);
    TRY(xuiLayoutArrangeChild(root,v->controls[XIDE_HOME_NEW],(xui_rect_t){area.fX+pad,y,bw,button}));
    TRY(xuiLayoutArrangeChild(root,v->controls[XIDE_HOME_OPEN],(xui_rect_t){area.fX+pad+bw+gap,y,bw,button}));
    int wrap=width<2*bw+2*gap+tutorial;
    TRY(xuiLayoutArrangeChild(root,v->controls[XIDE_HOME_TUTORIAL],(xui_rect_t){area.fX+pad+width-tutorial,y+(wrap ? button+gap:0),tutorial,button}));
    y+=(wrap ? 2:1)*(button+gap)+8*s;
    float clear=fminf(width,100*s),label=fmaxf(0,width-clear-gap);button=26*s;
    TRY(xuiLayoutArrangeChild(root,v->caption,(xui_rect_t){area.fX+pad,y,label,button}));
    TRY(xuiLayoutArrangeChild(root,v->controls[XIDE_HOME_CLEAR],(xui_rect_t){area.fX+pad+width-clear,y,clear,button}));y+=button+gap;
    TRY(xuiLayoutArrangeChild(root,v->frame,(xui_rect_t){area.fX+pad,y,width,fmaxf(0,area.fY+area.fH-pad-y)}));
    xui_widget table=v->controls[XIDE_HOME_HISTORY];
    if(v->scale!=s) {
        TRY(xuiTableViewSetColumnWidth(table,0,160*s));TRY(xuiTableViewSetColumnWidth(table,1,190*s));v->scale=s;
    }
    /* Keep the path column flexible; narrow panes use the table's own scrolling. */
    float path=fmaxf(220*s,width-350*s-18*s-2);
    if(fabsf(xuiTableViewGetColumnWidth(table,2)-path)>0.5f)TRY(xuiTableViewSetColumnWidth(table,2,path));
    return XUI_OK;
}
static int homeTheme(xide_app* a) {
    struct xide_home_view* v=a->homeView;if(!v)return XUI_OK;
    uint32_t line=(ideThemeColor(a->theme,IDE_BORDER)&0xffffff00u)|110u;
    xui_style_property_t styles[3]={0};const char* names[]={"tableview.grid.color","tableview.focus.color","tableview.border.width"};
    for(int i=0;i<3;++i) {styles[i].iSize=sizeof(styles[i]);styles[i].sName=names[i];styles[i].tValue.iSize=sizeof(styles[i].tValue);styles[i].tValue.iType=i==2 ? XUI_STYLE_VALUE_FLOAT:XUI_STYLE_VALUE_COLOR;}
    styles[0].tValue.iColor=line;styles[1].tValue.iColor=0;styles[2].tValue.fFloat=0;
    TRY(xuiWidgetSetInlineStyle(v->controls[XIDE_HOME_HISTORY],styles,3));
    xui_style_property_t border={0};border.iSize=sizeof(border);border.sName="panel.border.color";
    border.tValue.iSize=sizeof(border.tValue);border.tValue.iType=XUI_STYLE_VALUE_COLOR;border.tValue.iColor=ideThemeColor(a->theme,IDE_BORDER);
    return xuiWidgetSetInlineStyle(v->frame,&border,1);
}
static int homeCreate(xide_app* a) {
    struct xide_home_view* v=calloc(1,sizeof(*v));if(!v)return XUI_ERROR_OUT_OF_MEMORY;a->homeView=v;
    TRY(xuiWidgetCreate(a->context,&v->root));TRY(xuiWidgetSetLayoutType(v->root,XUI_LAYOUT_MANUAL));a->panels[XIDE_HOME]=v->root;
    const int actions[]={XIDE_ACTION_NEW,XIDE_ACTION_OPEN,XIDE_ACTION_ONLINE_TUTORIAL};const char* titles[]={"新建","打开","在线教程"};
    for(int i=0;i<3;++i) {
        xui_button_desc_t d={0};d.iSize=sizeof(d);d.pFont=a->uiFont;d.sText=titles[i];
        TRY(xuiButtonCreate(a->context,&v->controls[i],&d));TRY(xuiWidgetAddChild(v->root,v->controls[i]));
        TRY(xideActionsButton(a,v->controls[i],actions[i]));
    }
    {
        int i=XIDE_HOME_CLEAR;xui_hyperlink_desc_t d={0};d.iSize=sizeof(d);d.pFont=a->uiFont;d.sText="清空历史记录";
        d.iTextFlags=XUI_TEXT_ALIGN_RIGHT|XUI_TEXT_ALIGN_MIDDLE;d.bUnderline=d.bHoverUnderline=d.bActiveUnderline=1;
        TRY(xuiHyperlinkCreate(a->context,&v->controls[i],&d));TRY(xuiWidgetAddChild(v->root,v->controls[i]));
        TRY(xuiHyperlinkSetClick(v->controls[i],homeClear,a));
    }
    xui_label_desc_t label={0};label.iSize=sizeof(label);label.pFont=a->uiFont;label.sText="历史记录（双击或按 Enter 打开） · 0 / 100";
    TRY(xuiLabelCreate(a->context,&v->caption,&label));TRY(xuiWidgetAddChild(v->root,v->caption));
    TRY(xuiLabelSetTextFlags(v->caption,XUI_TEXT_ALIGN_LEFT|XUI_TEXT_ALIGN_MIDDLE));
    const char* headers[]={"名称","访问时间","文件路径"};float widths[]={160,190,300};
    for(int i=0;i<3;++i) {v->columns[i].iId=i;v->columns[i].sTitle=headers[i];v->columns[i].fWidth=widths[i]*xideUiFontScale(a);v->columns[i].fMinWidth=60;v->columns[i].bResizableSet=1;v->columns[i].bResizable=1;}
    xui_table_view_desc_t table={0};table.iSize=sizeof(table);table.pFont=a->uiFont;table.arrColumns=v->columns;table.iColumnCount=3;
    table.onCount=homeCount;table.onCell=homeCell;table.pAdapterUser=a;table.iSelectionMode=XUI_TABLE_VIEW_SELECTION_ROW;table.iSelectedRow=-1;
    table.fDefaultRowHeight=30*xideUiFontScale(a);table.fHeaderHeight=30*xideUiFontScale(a);
    xui_panel_desc_t frame={0};frame.iSize=sizeof(frame);frame.pFont=a->uiFont;frame.fBorderWidth=1;
    TRY(xuiPanelCreate(a->context,&v->frame,&frame));TRY(xuiWidgetAddChild(v->root,v->frame));
    TRY(xuiPanelSetHeaderHeight(v->frame,0));TRY(xuiWidgetSetPadding(v->frame,(xui_thickness_t){1,1,1,1}));
    xui_widget client=xuiPanelGetClientWidget(v->frame);TRY(xuiWidgetSetPadding(client,(xui_thickness_t){0,0,0,0}));TRY(xuiWidgetSetGap(client,0));
    TRY(xuiTableViewCreate(a->context,&v->controls[XIDE_HOME_HISTORY],&table));TRY(xuiWidgetAddChild(client,v->controls[XIDE_HOME_HISTORY]));
    TRY(xuiWidgetSetSizeMode(v->controls[XIDE_HOME_HISTORY],XUI_SIZE_FILL,XUI_SIZE_FILL));TRY(xuiWidgetSetFlex(v->controls[XIDE_HOME_HISTORY],1,1));
    TRY(xuiWidgetGetEventHandler(v->controls[XIDE_HOME_HISTORY],XUI_EVENT_KEY_DOWN,&v->keyHandler,&v->keyUser));
    TRY(xuiWidgetSetEventHandler(v->controls[XIDE_HOME_HISTORY],XUI_EVENT_KEY_DOWN,homeEvent,a));
    TRY(xuiWidgetSetEventHandler(v->controls[XIDE_HOME_HISTORY],XUI_EVENT_POINTER_DOUBLE_CLICK,homeEvent,a));
    TRY(xuiWidgetSetEnabled(v->controls[XIDE_HOME_CLEAR],0));
    TRY(xuiWidgetSetLayoutChildrenCallback(v->root,homeLayout,a));TRY(homeTheme(a));return xideHomeRefresh(a);
}
void xideHomeDestroy(xide_app* a) {
    if(!a || !a->homeView)return;
    for(size_t i=0;i<a->homeView->templates.count;++i)if(a->homeView->icons[i])a->proxy.surfaceDestroy(&a->proxy,a->homeView->icons[i]);
    for(int i=0;i<4;++i)if(a->homeView->categoryIcons[i])a->proxy.surfaceDestroy(&a->proxy,a->homeView->categoryIcons[i]);
    free(a->homeView->descriptions);xideTemplateRegistryClear(&a->homeView->templates);
    xrtValueRelease(a->homeView->snapshot.source);free(a->homeView->recentPath);free(a->homeView);a->homeView=NULL;
}
