#include "ui/import_view.h"
#include "ui/template_view.h"
#include "ui/rename_view.h"
#include "ui/file_changes_view.h"
#include "services/ui_font.h"
#include "services/editor_font.h"
#include "ui/catalog_view.h"
#include "ui/font_view.h"
#include "ui/goto_view.h"
#include "ui/recovery_view.h"
#include "ui/workspace_view.h"
#include "ui/configuration_view.h"
#include "ui/references_view.h"
#include "ui/resource_view.h"
#include "ui/toolchain_view.h"
#include "ui/reading_state.h"
#include "platform/path.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
static int fail(xide_model_error* error,int code,const char* message)
{ if(error) { error->code=code; snprintf(error->message,sizeof(error->message),"%s",message); } return 0; }
char* xideUiFontDefaultPath(void)
{
    return xideEditorFontDefaultPath();
}
float xideUiFontScale(xide_app* app)
{
    float scale=(app->uiFontSize ? app->uiFontSize : 16)/16.0f; xui_font_metrics_t metrics={0};
    if(app->uiFont && app->proxy.fontGetMetrics(&app->proxy,app->uiFont,&metrics)==XUI_OK && isfinite(metrics.fLineHeight) && metrics.fLineHeight>scale*20) scale=metrics.fLineHeight/20;
    return scale<0.75f ? 0.75f : scale;
}
int xideUiHeight(xide_app* app,int base) { return (int)lroundf(base*xideUiFontScale(app)); }
xui_rect_t xideUiRect(xide_app* app,xui_rect_t base)
{ float scale=xideUiFontScale(app); return (xui_rect_t){(int)lroundf(base.fX*scale),(int)lroundf(base.fY*scale),(int)lroundf(base.fW*scale),(int)lroundf(base.fH*scale)}; }
int xideUiLayoutCapture(xide_ui_layout* layout,xui_widget content)
{
    if(!layout || !content) { return XUI_ERROR_INVALID_ARGUMENT; } layout->count=0;
    for(xui_widget w=xuiWidgetGetFirstChild(content);w;w=xuiWidgetGetNextSibling(w)) {
        if(layout->count==64) return XUI_ERROR_OUT_OF_MEMORY;
        size_t i=layout->count++; layout->widgets[i]=w; layout->rectangles[i]=xuiWidgetGetRect(w); layout->preferred[i]=xuiWidgetGetPreferredSize(w);
    }
    return XUI_OK;
}
void xideUiLayoutApply(xide_app* app,const xide_ui_layout* layout,int manual)
{
    float scale=xideUiFontScale(app);
    for(size_t i=0;i<layout->count;++i) {
        if(manual) xuiWidgetSetRect(layout->widgets[i],xideUiRect(app,layout->rectangles[i]));
        else xuiWidgetSetPreferredSize(layout->widgets[i],(xui_vec2_t){layout->preferred[i].fX*scale,layout->preferred[i].fY*scale});
    }
}
int xideUiFontStart(xide_app* app)
{
    xide_model_error error={0}; int size=app->preferences.uiFontSize ? app->preferences.uiFontSize : 16;
    char* path=app->preferences.uiFont ? xidePathResolve(NULL,app->preferences.uiFont) : xideUiFontDefaultPath();
    if(!xideFontLoad(app,path,size,&app->uiFont,&error)) {
        fprintf(stderr,"UI font: %s; trying default and bundled fonts.\n",error.message); free(path); path=xideUiFontDefaultPath(); size=16;
        if(!xideFontLoad(app,path,size,&app->uiFont,&error)) {
            free(path); path=xideBundledFontPath();
            if(!xideFontLoad(app,path,size,&app->uiFont,&error)) { free(path); return XUI_ERROR; }
        }
    }
    app->uiFontPath=path; app->uiFontSize=size; return XUI_OK;
}
static int metrics(xide_app* app,xui_widget widget,xui_widget_type type)
{
    float row=(float)xideUiHeight(app,28);
    if(type==xuiWidgetFindType(app->context,"treeview")) { float h=0,indent=0,pad=0,border=0; int result=xuiTreeViewGetMetrics(widget,&h,&indent,&pad,&border); return result==XUI_OK ? xuiTreeViewSetMetrics(widget,(float)xideUiHeight(app,widget==app->commands ? 26 : 28),indent,pad,border) : result; }
    if(type==xuiWidgetFindType(app->context,"listview")) { float h=0,pad=0,border=0; int result=xuiListViewGetMetrics(widget,&h,&pad,&border); return result==XUI_OK ? xuiListViewSetMetrics(widget,row,pad,border) : result; }
    if(type==xuiWidgetFindType(app->context,"tableview")) { float col=0,h=0,head=0; int result=xuiTableViewGetDefaultMetrics(widget,&col,&h,&head); return result==XUI_OK ? xuiTableViewSetDefaultMetrics(widget,col,row,row) : result; }
    if(type==xuiCheckBoxGetType(app->context)) { int result=xuiCheckBoxSetIndicatorSize(widget,(float)xideUiHeight(app,18)); return result==XUI_OK ? xuiCheckBoxSetGap(widget,(float)xideUiHeight(app,8)) : result; }
    if(type==xuiWidgetFindType(app->context,"combobox")) { float h=0,border=0; int result=xuiComboBoxGetMetrics(widget,&h,&border); return result==XUI_OK ? xuiComboBoxSetMetrics(widget,row,border) : result; }
    if(type==xuiWidgetFindType(app->context,"menu")) { xui_menu_metrics_t m={0}; m.iSize=sizeof(m); int result=xuiMenuGetMetrics(widget,&m); m.fItemHeight=row; return result==XUI_OK ? xuiMenuSetMetrics(widget,&m) : result; }
    if(type==xuiWidgetFindType(app->context,"menubar")) { xui_menubar_metrics_t m={0}; m.iSize=sizeof(m); int result=xuiMenuBarGetMetrics(widget,&m); m.fHeight=row; return result==XUI_OK ? xuiMenuBarSetMetrics(widget,&m) : result; }
    if(type==xuiWidgetFindType(app->context,"toolbar")) { xui_toolbar_metrics_t m={0}; m.iSize=sizeof(m); int result=xuiToolbarGetMetrics(widget,&m); m.fItemWidth=(float)xideUiHeight(app,64); m.fItemHeight=(float)xideUiHeight(app,28); return result==XUI_OK ? xuiToolbarSetMetrics(widget,&m) : result; }
    if(type==xuiWidgetFindType(app->context,"statusbar")) { xui_statusbar_metrics_t m={0}; m.iSize=sizeof(m); int result=xuiStatusBarGetMetrics(widget,&m); m.fHeight=row; return result==XUI_OK ? xuiStatusBarSetMetrics(widget,&m) : result; }
    if(type==xuiWidgetFindType(app->context,"dockpanel")) { xui_dock_panel_metrics_t m={0}; m.iSize=sizeof(m); int result=xuiDockPanelGetMetrics(widget,&m); m.fCaptionHeight=(float)xideUiHeight(app,24); m.fTabStripHeight=(float)xideUiHeight(app,24); m.fFloatTitleHeight=(float)xideUiHeight(app,28); m.fAutoHideStripSize=(float)xideUiHeight(app,24); return result==XUI_OK ? xuiDockPanelSetMetrics(widget,&m) : result; }
    return XUI_OK;
}
int xideUiFontBindMenu(xide_app* app,xui_widget menu)
{
    if(!app || !menu) return XUI_ERROR_INVALID_ARGUMENT;
    int ret=metrics(app,menu,xuiWidgetGetType(menu));
    return ret==XUI_OK ? xuiMenuSetFont(menu,app->uiFont) : ret;
}
typedef struct widget_scan { xui_widget* items; size_t count,capacity; } widget_scan;
static int firstVisit(widget_scan* scan,xui_widget widget)
{
    for(size_t i=0;i<scan->count;++i) if(scan->items[i]==widget) return 0;
    if(scan->count==scan->capacity) { size_t size=scan->capacity ? scan->capacity*2 : 64; xui_widget* items=realloc(scan->items,size*sizeof(*items)); if(!items) return -1; scan->items=items; scan->capacity=size; }
    scan->items[scan->count++]=widget; return 1;
}
static int bind(xide_app* app,xui_widget widget,xui_font previous,xui_font replacement,widget_scan* scan)
{
    if(!widget) return XUI_OK;
    int visited=firstVisit(scan,widget); if(visited<=0) return visited==0 ? XUI_OK : XUI_ERROR_OUT_OF_MEMORY;
    xui_widget_type type=xuiWidgetGetType(widget); xui_font current=NULL; int (*setter)(xui_widget,xui_font)=NULL;
    if(type==xuiWidgetFindType(app->context,"label")) { current=xuiLabelGetFont(widget); setter=xuiLabelSetFont; }
    else if(type==xuiWidgetFindType(app->context,"input")) { current=xuiInputGetFont(widget); setter=xuiInputSetFont; }
    else if(type==xuiCheckBoxGetType(app->context)) { current=xuiCheckBoxGetFont(widget); setter=xuiCheckBoxSetFont; }
    else if(type==xuiWidgetFindType(app->context,"button")) { current=xuiButtonGetFont(widget); setter=xuiButtonSetFont; }
    else if(type==xuiWidgetFindType(app->context,"textedit")) { current=xuiTextEditGetFont(widget); setter=xuiTextEditSetFont; }
    else if(type==xuiWidgetFindType(app->context,"listview")) { current=xuiListViewGetFont(widget); setter=xuiListViewSetFont; }
    else if(type==xuiWidgetFindType(app->context,"treeview")) { current=xuiTreeViewGetFont(widget); setter=xuiTreeViewSetFont; }
    else if(type==xuiWidgetFindType(app->context,"tableview")) { current=xuiTableViewGetFont(widget); setter=xuiTableViewSetFont; }
    else if(type==xuiWidgetFindType(app->context,"combobox")) { current=xuiComboBoxGetFont(widget); setter=xuiComboBoxSetFont; }
    else if(type==xuiWidgetFindType(app->context,"menu")) { current=xuiMenuGetFont(widget); setter=xuiMenuSetFont; }
    else if(type==xuiWidgetFindType(app->context,"menubar")) { current=xuiMenuBarGetFont(widget); setter=xuiMenuBarSetFont; }
    else if(type==xuiWidgetFindType(app->context,"toolbar")) { current=xuiToolbarGetFont(widget); setter=xuiToolbarSetFont; }
    else if(type==xuiWidgetFindType(app->context,"statusbar")) { current=xuiStatusBarGetFont(widget); setter=xuiStatusBarSetFont; }
    else if(type==xuiWidgetFindType(app->context,"dockpanel")) { current=xuiDockPanelGetFont(widget); setter=xuiDockPanelSetFont; }
    else if(type==xuiWidgetFindType(app->context,"window")) { current=xuiWindowGetFont(widget); setter=xuiWindowSetFont; }
    else if(type==xuiWidgetFindType(app->context,"panel")) { current=xuiPanelGetFont(widget); setter=xuiPanelSetFont; }
    else if(type==xuiWidgetFindType(app->context,"tabs")) { current=xuiTabsGetFont(widget); setter=xuiTabsSetFont; }
    if(widget==app->panels[XIDE_OUTPUT]) setter=NULL;
    if(setter && (!current || current==previous || current==replacement)) {
        int result=metrics(app,widget,type); if(result==XUI_OK) result=setter(widget,replacement); if(result!=XUI_OK) return result;
    }
    if(type==xuiWidgetFindType(app->context,"menubar")) for(int i=0;i<xuiMenuBarGetItemCount(widget);++i) { const xui_menubar_item_t* item=xuiMenuBarGetItem(widget,i); int result=item ? bind(app,item->pMenu,previous,replacement,scan) : XUI_OK; if(result!=XUI_OK) return result; }
    if(type==xuiWidgetFindType(app->context,"combobox")) { int result=bind(app,xuiComboBoxGetMenuWidget(widget),previous,replacement,scan); if(result!=XUI_OK) return result; }
    xui_widget owned[3]={0};
    if(type==xuiWidgetFindType(app->context,"popup")) { owned[0]=xuiPopupGetContentWidget(widget); owned[1]=xuiPopupGetScrollViewWidget(widget); }
    else if(type==xuiWidgetFindType(app->context,"scrollview")) owned[0]=xuiScrollViewGetContentWidget(widget);
    else if(type==xuiWidgetFindType(app->context,"menu")) owned[0]=xuiMenuGetPopupWidget(widget);
    else if(type==xuiWidgetFindType(app->context,"codeedit")) owned[0]=xuiCodeEditGetMenuWidget(widget);
    for(int i=0;i<3;++i) { int result=bind(app,owned[i],previous,replacement,scan); if(result!=XUI_OK) return result; }
    for(xui_widget child=xuiWidgetGetFirstChild(widget);child;child=xuiWidgetGetNextSibling(child)) { int result=bind(app,child,previous,replacement,scan); if(result!=XUI_OK) return result; }
    return XUI_OK;
}
int xideUiFontBindWidget(xide_app* app,xui_widget widget)
{
    if(!app || !widget) return XUI_ERROR_INVALID_ARGUMENT;
    widget_scan scan={0}; int result=bind(app,widget,app->uiFont,app->uiFont,&scan);
    free(scan.items); return result;
}
static int install(xide_app* app,xui_font previous,xui_font replacement,int size)
{
    app->uiFont=replacement; app->uiFontSize=size; int result=xuiSetDefaultFont(app->context,replacement);
    widget_scan scan={0};
    if(result==XUI_OK) result=bind(app,app->root,previous,replacement,&scan);
    for(int i=0;i<XIDE_WINDOW_COUNT && result==XUI_OK;++i) result=bind(app,app->panels[i],previous,replacement,&scan);
    if(result==XUI_OK) result=bind(app,xideGotoViewWindow(app),previous,replacement,&scan);
    if(result==XUI_OK) result=bind(app,xideFontViewWindow(app),previous,replacement,&scan);
    if(result==XUI_OK) result=bind(app,xideReferencesViewWindow(app),previous,replacement,&scan);
    if(result==XUI_OK) result=bind(app,xideResourceViewWindow(app),previous,replacement,&scan);
    if(result==XUI_OK) result=bind(app,xideToolchainViewWindow(app),previous,replacement,&scan);
    if(result==XUI_OK) result=bind(app,xideTemplateViewWindow(app),previous,replacement,&scan);
    if(result==XUI_OK) result=bind(app,xideImportViewWindow(app),previous,replacement,&scan);
    if(result==XUI_OK) result=bind(app,xideRenameViewWindow(app),previous,replacement,&scan);
    if(result==XUI_OK) result=bind(app,xideConfigurationViewWindow(app),previous,replacement,&scan);
    if(result==XUI_OK) result=bind(app,xideRecoveryViewWindow(app),previous,replacement,&scan);
    if(result==XUI_OK) result=bind(app,xideFileChangesViewWindow(app),previous,replacement,&scan);
    if(result==XUI_OK) result=bind(app,xideCatalogSourceWindow(app),previous,replacement,&scan);
    free(scan.items);
    if(result==XUI_OK) result=xideFontViewUiFont(app);
    if(result==XUI_OK) result=xideCatalogViewUiFont(app);
    if(result==XUI_OK) { xideWorkbenchLayout(app); result=xuiLayout(app->context); }
    if(result==XUI_OK) { xideCatalogViewLayout(app); xideWorkspaceViewLayout(app); result=xuiLayout(app->context); }
    return result;
}
int xideUiFontInitialize(xide_app* app) { return install(app,app->uiFont,app->uiFont,app->uiFontSize); }
typedef struct ui_scroll { xui_widget widget; int kind; float x,y; } ui_scroll;
typedef struct ui_scrolls { ui_scroll* items; size_t count,capacity; widget_scan scan; } ui_scrolls;
static int retainScroll(xide_app* app,xui_widget widget,ui_scrolls* list)
{
    if(!widget) return 1;
    int visited=firstVisit(&list->scan,widget); if(visited<=0) return visited==0;
    xui_widget_type type=xuiWidgetGetType(widget); int kind=type==xuiWidgetFindType(app->context,"scrollview") ? 1 : type==xuiWidgetFindType(app->context,"textedit") ? 2 : type==xuiWidgetFindType(app->context,"codeedit") ? 3 : 0;
    if(kind) {
        int present=0; for(size_t i=0;i<list->count;++i) if(list->items[i].widget==widget) { present=1; break; }
        if(!present) {
            if(list->count==list->capacity) { size_t cap=list->capacity ? list->capacity*2 : 16; ui_scroll* items=realloc(list->items,cap*sizeof(*items)); if(!items) return 0; list->items=items; list->capacity=cap; }
            ui_scroll value={.widget=widget,.kind=kind};
            int result=kind==1 ? xuiScrollViewGetOffset(widget,&value.x,&value.y) : kind==2 ? xuiTextEditGetScroll(widget,&value.x,&value.y) : xuiCodeEditGetScroll(widget,&value.x,&value.y);
            if(result!=XUI_OK) return 0;
            list->items[list->count++]=value;
        }
    }
    xui_widget owned[2]={0};
    if(type==xuiWidgetFindType(app->context,"popup")) { owned[0]=xuiPopupGetContentWidget(widget); owned[1]=xuiPopupGetScrollViewWidget(widget); }
    else if(type==xuiWidgetFindType(app->context,"scrollview")) owned[0]=xuiScrollViewGetContentWidget(widget);
    for(int i=0;i<2;++i) if(!retainScroll(app,owned[i],list)) return 0;
    for(xui_widget child=xuiWidgetGetFirstChild(widget);child;child=xuiWidgetGetNextSibling(child)) if(!retainScroll(app,child,list)) return 0;
    return 1;
}
static void restoreScroll(const ui_scrolls* list)
{
    for(size_t i=0;i<list->count;++i) { const ui_scroll* value=&list->items[i];
        if(value->kind==1) xuiScrollViewSetOffset(value->widget,value->x,value->y);
        else if(value->kind==2) xuiTextEditSetScroll(value->widget,value->x,value->y);
        else xuiCodeEditSetScroll(value->widget,value->x,value->y);
    }
}
int xideUiFontApply(xide_app* app,const char* path,int size,xide_model_error* error)
{
    if(error) memset(error,0,sizeof(*error));
    if(!app || !app->context) return fail(error,XIDE_MODEL_INVALID,"A native workbench is required to change fonts.");
    char* resolved=xidePathResolve(NULL,path); xui_font replacement=NULL;
    if(!resolved) return fail(error,XIDE_MODEL_INVALID,"Invalid UTF-8 font path.");
    if(!xideFontLoad(app,resolved,size,&replacement,error)) { free(resolved); return 0; }
    ui_scrolls scroll={0}; int retained=retainScroll(app,app->root,&scroll);
    for(int i=0;i<XIDE_WINDOW_COUNT && retained;++i) retained=retainScroll(app,app->panels[i],&scroll);
    for(size_t i=0;i<app->documents.count && retained;++i) retained=retainScroll(app,app->documents.items[i]->editor,&scroll);
    if(retained) retained=retainScroll(app,xideGotoViewWindow(app),&scroll);
    if(retained) retained=retainScroll(app,xideFontViewWindow(app),&scroll);
    if(retained) retained=retainScroll(app,xideReferencesViewWindow(app),&scroll);
    if(retained) retained=retainScroll(app,xideResourceViewWindow(app),&scroll);
    if(retained) retained=retainScroll(app,xideToolchainViewWindow(app),&scroll);
    if(retained) retained=retainScroll(app,xideTemplateViewWindow(app),&scroll);
    if(retained) retained=retainScroll(app,xideImportViewWindow(app),&scroll);
    if(retained) retained=retainScroll(app,xideRenameViewWindow(app),&scroll);
    if(retained) retained=retainScroll(app,xideConfigurationViewWindow(app),&scroll);
    if(retained) retained=retainScroll(app,xideRecoveryViewWindow(app),&scroll);
    if(retained) retained=retainScroll(app,xideFileChangesViewWindow(app),&scroll);
    xvalue* reading=retained ? xideReadingCapture(app,NULL) : NULL;
    if(!retained || !reading) { xrtValueRelease(reading); free(scroll.items); free(scroll.scan.items); app->proxy.fontDestroy(&app->proxy,replacement); free(resolved); return fail(error,XIDE_MODEL_IO,"Cannot retain reading positions while changing fonts."); }
    xui_font previous=app->uiFont; int previousSize=app->uiFontSize;
    int result=install(app,previous,replacement,size); int saved=result==XUI_OK && xideSettingsSetUiFont(&app->preferences,resolved,size,error);
    if(!saved) {
        int restored=install(app,replacement,previous,previousSize); if(restored!=XUI_OK) app->frameError=restored;
        restoreScroll(&scroll); xideReadingRestore(app,reading,1); xrtValueRelease(reading); free(scroll.items); free(scroll.scan.items);
        app->proxy.fontDestroy(&app->proxy,replacement); free(resolved);
        if(result!=XUI_OK) return fail(error,XIDE_MODEL_INVALID,"Cannot bind the new interface font to the workbench.");
        return 0;
    }
    restoreScroll(&scroll); xideReadingRestore(app,reading,1); xrtValueRelease(reading); free(scroll.items); free(scroll.scan.items);
    if(previous && previous!=app->codeFont) app->proxy.fontDestroy(&app->proxy,previous);
    free(app->uiFontPath); app->uiFontPath=resolved; xgeRenderRequest(); return 1;
}
