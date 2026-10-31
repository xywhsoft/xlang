#ifndef XLANG_IDE_MAIN_TOOLBAR_H
#define XLANG_IDE_MAIN_TOOLBAR_H
#include "app/xide_app.h"
enum xide_toolbar_control {
    XIDE_TB_NEW,XIDE_TB_NEW_MENU,XIDE_TB_OPEN,XIDE_TB_OPEN_MENU,XIDE_TB_SAVE,XIDE_TB_SAVE_MENU,
    XIDE_TB_TARGET,XIDE_TB_CONFIGURATION,XIDE_TB_CONTEXT,XIDE_TB_BUILD,XIDE_TB_RUN,XIDE_TB_TEST,
    XIDE_TB_STOP,XIDE_TB_STOP_MENU,XIDE_TB_TASKS,XIDE_TB_MORE,XIDE_TB_COUNT
};
int xideMainToolbarCreate(xide_app* app);
void xideMainToolbarDestroy(xide_app* app);
int xideMainToolbarRefresh(xide_app* app);
void xideMainToolbarLayout(xide_app* app);
int xideMainToolbarAction(xide_app* app,int action);
xui_widget xideMainToolbarControl(xide_app* app,int control);
xui_widget xideMainToolbarMenu(xide_app* app);
void xideMainToolbarLanguageMenu(xide_app* app);
#endif
