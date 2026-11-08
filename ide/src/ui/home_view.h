#ifndef XLANG_IDE_HOME_VIEW_H
#define XLANG_IDE_HOME_VIEW_H
#include "app/xide_app.h"
enum xide_home_control {
    XIDE_HOME_NEW_PROJECT, XIDE_HOME_NEW_FILE, XIDE_HOME_NEW_WORKSPACE,
    XIDE_HOME_OPEN_FILE, XIDE_HOME_OPEN_PROJECT, XIDE_HOME_OPEN_WORKSPACE,
    XIDE_HOME_TUTORIAL, XIDE_HOME_CLEAR, XIDE_HOME_HISTORY, XIDE_HOME_CONTROL_COUNT
};
int xideHomeRefresh(xide_app*);
void xideHomeDestroy(xide_app*);
void xideHomeRemember(xide_app*,const char* path);
int xideHomeOpenPath(xide_app*,const char* path);
int xideHomeAction(xide_app*,int);
int xideWorkbenchShowHome(xide_app*);
xui_widget xideHomeControl(xide_app*,int);
const char* xideHomeTutorialUrl(void);
#endif
