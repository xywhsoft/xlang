#ifndef XIDE_NEW_FILE_TEST_HELPERS_H
#define XIDE_NEW_FILE_TEST_HELPERS_H
#include "ui/home_view.h"
#include "ui/editor_view.h"
static int xideTestNewPickerSettle(xide_app* app){for(int i=0;i<3;++i){if(xuiDispatchPendingEvents(app->context)!=XUI_OK)return 0;xideEditorPoll(app);xideWorkbenchLayout(app);if(xuiLayout(app->context)!=XUI_OK)return 0;}return !app->frameError;}
static int xideTestNewPickerClick(xide_app* app,int x,int y){return xuiInputPointerMove(app->context,x,y,0)==XUI_OK&&xuiInputPointerDown(app->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK&&xuiInputPointerUp(app->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK&&xideTestNewPickerSettle(app);}
/* Ctrl+N now chooses an artifact before creating an editor document. */
static int xideTestPickNewFile(xide_app* app,int cSource){
    xui_widget tree=xideHomeControl(app,XIDE_HOME_CATEGORY);int row=xuiTreeViewGetVisibleIndexOfId(tree,3);xui_rect_t item=xuiTreeViewGetItemRect(tree,row),world=xuiWidgetGetWorldRect(tree);
    if(row<0||!xideTestNewPickerClick(app,world.fX+item.fX+item.fW/2,world.fY+item.fY+item.fH/2))return 0;
    if(xuiListViewSetSelected(xideHomeControl(app,XIDE_HOME_TEMPLATES),cSource ? 1:0)!=XUI_OK)return 0;
    xui_rect_t button=xuiWidgetGetWorldRect(xideHomeControl(app,XIDE_HOME_CREATE));return xideTestNewPickerClick(app,button.fX+button.fW/2,button.fY+button.fH/2)&&app->activeDocument&&!app->activeDocument->path;
}
#endif
