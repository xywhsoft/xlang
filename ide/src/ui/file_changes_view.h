#ifndef XLANG_IDE_FILE_CHANGES_VIEW_H
#define XLANG_IDE_FILE_CHANGES_VIEW_H
#include "app/xide_app.h"
enum { XIDE_CHANGES_LIST,XIDE_CHANGES_LOCAL,XIDE_CHANGES_DISK,XIDE_CHANGES_REFRESH,XIDE_CHANGES_KEEP,XIDE_CHANGES_RELOAD,XIDE_CHANGES_CLOSE,XIDE_CHANGES_CONFIRM,XIDE_CHANGES_CANCEL,XIDE_CHANGES_CONTROL_COUNT };
int xideFileChangesViewCreate(xide_app* app);
void xideFileChangesViewDestroy(xide_app* app);
int xideFileChangesViewOpen(xide_app* app);
int xideFileChangesViewIsOpen(xide_app* app);
void xideFileChangesViewPoll(xide_app* app);
void xideFileChangesViewLayout(xide_app* app);
int xideFileChangesViewCodeFont(xide_app* app);
int xideFileChangesViewTheme(xide_app* app);
xui_widget xideFileChangesViewWindow(xide_app* app);
xui_widget xideFileChangesViewControl(xide_app* app,int control);
const char* xideFileChangesViewMessage(xide_app* app);
#endif
