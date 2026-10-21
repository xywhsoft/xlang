#include "app/xide_app.h"
#include "ui/file_dialog.h"
#include "services/ui_font.h"
#include "platform/path.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
struct xide_file_dialog {
    xide_app* app; xui_file_dialog dialog; xui_widget backdrop,restore,owner;
    xui_widget_event_proc event; void* eventUser;
    xide_path_selected_proc selected; void* user;
    char *path,*extension; int ready,mode; xui_msgbox styledOverwrite;
};
static char* copy(const char* s) { if(!s) return NULL;size_t n=strlen(s)+1;char* p=malloc(n);if(p) memcpy(p,s,n);return p; }
/* Above business windows; SDK menus and message boxes retain higher layers. */
static void layers(xui_widget w) { xuiWidgetSetLayer(w,XUI_LAYER_FLOATING,2003);for(xui_widget c=xuiWidgetGetFirstChild(w);c;c=xuiWidgetGetNextSibling(c)) layers(c); }
static int consume(xui_widget w,const xui_event_t* e,void* user) { return XUI_EVENT_DISPATCH_STOP; }
static int windowEvent(xui_widget w,const xui_event_t* e,void* user) {
    xide_file_dialog* p=user;
    int skip=e && e->iType==XUI_EVENT_POINTER_DOWN && e->iPhase==XUI_EVENT_PHASE_CAPTURE && e->pTarget!=w;
    int r=!skip && p->event ? p->event(w,e,p->eventUser) : XUI_OK;
    if(!p->ready) layers(w);
    return r;
}
static void result(xui_file_dialog d,int answer,const char* path,void* user) {
    xide_file_dialog* p=user;free(p->path);p->path=answer==XUI_FILE_DIALOG_RESULT_OK ? xidePathResolve(NULL,path) : NULL;p->ready=1;xgeRenderRequest();
}
static void commit(xui_widget w,void* user) {
    xide_file_dialog* p=user;xui_widget input=xuiFileDialogGetPathInputWidget(p->dialog);
    if(xuiGetFocusWidget(p->app->context)==input) {
        int r=xuiFileDialogSetDirectory(p->dialog,xuiInputGetText(input));xuiInputSetError(input,r!=XUI_OK);
        if(r==XUI_OK) { xuiWidgetSetVisible(input,0);xuiWidgetSetVisible(xuiFileDialogGetPathBreadcrumbWidget(p->dialog),1); }return;
    }
    input=xuiFileDialogGetNameInputWidget(p->dialog);const char* name=xuiInputGetText(input);
    if(p->mode==XUI_FILE_DIALOG_MODE_SAVE_FILE && p->extension && name && *name) {
        char* path=xidePathResolve(xuiFileDialogGetDirectory(p->dialog),name);
        if(path && !xidePathIsDirectory(path) && !strchr(xidePathFilename(name),'.')) {
            size_t n=strlen(name)+strlen(p->extension)+2;char* full=malloc(n);
            if(!full) { free(path);return; }snprintf(full,n,"%s.%s",name,p->extension);xuiInputSetText(input,full);free(full);
        }free(path);
    }
    if(xuiFileDialogCommit(p->dialog)!=XUI_OK) xuiInputSetError(input,1);
    xgeRenderRequest();
}
static xui_thickness_t scaled(xui_thickness_t t,float s) { return (xui_thickness_t){t.fLeft*s,t.fTop*s,t.fRight*s,t.fBottom*s}; }
static void scaleLayout(xui_widget w,float s) {
    xui_vec2_t size=xuiWidgetGetPreferredSize(w);xuiWidgetSetPreferredSize(w,(xui_vec2_t){size.fX*s,size.fY*s});
    xuiWidgetSetPadding(w,scaled(xuiWidgetGetPadding(w),s));xuiWidgetSetMargin(w,scaled(xuiWidgetGetMargin(w),s));xuiWidgetSetGap(w,xuiWidgetGetGap(w)*s);
    for(xui_widget c=xuiWidgetGetFirstChild(w);c;c=xuiWidgetGetNextSibling(c)) scaleLayout(c,s);
}
int xideFileDialogChoose(xide_app* a,xui_widget owner,int mode,const char* title,const char* filter,const char* initial,const char* extension,xide_path_selected_proc selected,void* user) {
    if(!a || !a->context || a->hidden || a->fileDialog) return 0;
    xide_file_dialog* p=calloc(1,sizeof(*p));if(!p) return 0;
    p->app=a;p->owner=owner;p->mode=mode;p->selected=selected;p->user=user;p->extension=copy(extension);p->restore=xuiGetFocusWidget(a->context);
    char* directory=initial && *initial ? (xidePathIsDirectory(initial) ? xidePathResolve(NULL,initial) : xidePathDirectory(initial)) : NULL;
    if(!directory || !xidePathIsDirectory(directory)) { free(directory);xide_project* project=xideWorkspaceFindProject(&a->workspaceModel,a->selectedProject);directory=project ? xidePathDirectory(project->manifest) : a->workspaceModel.manifest ? xidePathDirectory(a->workspaceModel.manifest) : xidePathResolve(NULL,"."); }
    xui_file_dialog_desc_t d={0};d.iSize=sizeof(d);d.iMode=mode;d.sTitle=title;d.sFilter=filter;d.sInitialDir=directory;d.pFont=a->uiFont;d.onResult=result;d.pResultUser=p;
    d.sFileName=mode==XUI_FILE_DIALOG_MODE_SAVE_FILE && initial && *initial && !xidePathIsDirectory(initial) ? xidePathFilename(initial) : NULL;
    float scale=xideUiFontScale(a);d.fWidth=fminf(800*scale,fmaxf(560,a->viewportWidth-32));d.fHeight=fminf(580*scale,fmaxf(430,a->viewportHeight-32));
    int r=xuiStyleBeginUpdate(a->context);if(r==XUI_OK) {
        r=xuiFileDialogCreate(a->context,&p->dialog,&d);
        if(r==XUI_OK) { xui_panel_desc_t panel={0};panel.iSize=sizeof(panel);r=xuiPanelCreate(a->context,&p->backdrop,&panel); }
        int end=xuiStyleEndUpdate(a->context);if(r==XUI_OK) r=end;
    }free(directory);
    if(r!=XUI_OK) { if(p->dialog) xuiFileDialogDestroy(p->dialog);if(p->backdrop) xuiWidgetDestroy(p->backdrop);free(p->extension);free(p);xideMessageShow(a,"文件选择","无法创建 XUI 文件对话框。",XUI_MSGBOX_ICON_ERROR);return 0; }
    a->fileDialog=p;xui_widget window=xuiFileDialogGetWindowWidget(p->dialog);
    if(!xuiWidgetGetParent(window)) xuiWidgetAddChild(a->root,window);
    scaleLayout(xuiWindowGetClientWidget(window),scale);xuiWindowSetChrome(window,32*scale,1,6*scale,24*scale);xideUiFontBindWidget(a,window);
    xui_widget lists[]={xuiFileDialogGetRootListWidget(p->dialog),xuiFileDialogGetFileListWidget(p->dialog)};
    for(size_t i=0;i<2;++i) { float h,pad,border;if(xuiListViewGetMetrics(lists[i],&h,&pad,&border)==XUI_OK) xuiListViewSetMetrics(lists[i],28*scale,pad,border); }
    xuiWidgetSetDefaultAction(window,commit,p);xuiWidgetSetDefaultAction(xuiFileDialogGetNameInputWidget(p->dialog),commit,p);xuiButtonSetClick(xuiFileDialogGetOkButtonWidget(p->dialog),commit,p);
    xuiWidgetGetEventCallback(window,&p->event,&p->eventUser);xuiWidgetSetEventCallback(window,windowEvent,p);
    xui_style_property_t styles[3]={0};const char* names[]={"panel.background.color","panel.client.color","panel.border.color"};
    for(size_t i=0;i<3;++i) { styles[i].iSize=sizeof(styles[i]);styles[i].sName=names[i];styles[i].tValue.iSize=sizeof(styles[i].tValue);styles[i].tValue.iType=XUI_STYLE_VALUE_TOKEN;styles[i].tValue.sText=i==0 ? "ide.shadow" : "ide.transparent"; }
    xuiWidgetSetInlineStyle(p->backdrop,styles,3);
    const int events[]={XUI_EVENT_POINTER_DOWN,XUI_EVENT_POINTER_UP,XUI_EVENT_POINTER_MOVE,XUI_EVENT_POINTER_WHEEL,XUI_EVENT_KEY_DOWN};
    for(size_t i=0;i<sizeof(events)/sizeof(*events);++i) xuiWidgetSetEventHandler(p->backdrop,events[i],consume,p);
    xuiOverlayAttach(a->context,window,p->backdrop,XUI_LAYER_FLOATING,2002);xuiFileDialogSetOpen(p->dialog,1);layers(window);xideFileDialogLayout(a);xideActionsRefresh(a);xgeRenderRequest();return 1;
}
int xideFileDialogBusy(xide_app* a) { return a && a->fileDialog; }
void xideFileDialogLayout(xide_app* a) {
    xide_file_dialog* p=a->fileDialog;if(!p) return;
    xuiWidgetSetRect(p->backdrop,(xui_rect_t){0,0,a->viewportWidth,a->viewportHeight});layers(xuiFileDialogGetWindowWidget(p->dialog));
    xui_msgbox box=xuiFileDialogGetOverwriteMsgBox(p->dialog);
    if(box && box!=p->styledOverwrite) {
        float s=xideUiFontScale(a);xui_msgbox_metrics_t m={0};m.iSize=sizeof(m);xuiMsgBoxGetMetrics(box,&m);
        m.fPaddingX*=s;m.fPaddingY*=s;m.fIconSize*=s;m.fIconGap*=s;m.fButtonWidth*=s;m.fButtonHeight*=s;m.fButtonGap*=s;m.fFooterHeight*=s;m.fMinMessageHeight*=s;m.fMinWidth*=s;m.fMaxWidth*=s;
        xuiMsgBoxSetMetrics(box,&m);xuiWindowSetChrome(xuiMsgBoxGetWindowWidget(box),32*s,1,6*s,24*s);xideUiFontBindWidget(a,xuiMsgBoxGetWindowWidget(box));p->styledOverwrite=box;
    }
}
void xideFileDialogDestroy(xide_app* a) { xide_file_dialog* p=a->fileDialog;if(!p) return;a->fileDialog=NULL;xuiFileDialogDestroy(p->dialog);xuiWidgetDestroy(p->backdrop);free(p->extension);free(p->path);free(p); }
void xideFileDialogPoll(xide_app* a) {
    xide_file_dialog* p=a->fileDialog;if(!p || !p->ready) return;
    char* path=p->path;p->path=NULL;xide_path_selected_proc selected=p->selected;void* user=p->user;
    int valid=!p->owner || xuiWidgetGetVisible(p->owner);xui_widget restore=p->restore;
    xideFileDialogDestroy(a);if(restore) xuiSetFocusWidget(a->context,restore);
    if(selected) selected(a,valid ? path : NULL,user);
    free(path);xideActionsRefresh(a);
}
xui_file_dialog xideFileDialogCurrent(xide_app* a) { return a && a->fileDialog ? a->fileDialog->dialog : NULL; }
