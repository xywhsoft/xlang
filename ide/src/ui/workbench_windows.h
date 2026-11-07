#ifndef XLANG_IDE_WORKBENCH_WINDOWS_H
#define XLANG_IDE_WORKBENCH_WINDOWS_H
#include "app/actions.h"
/* 1 = present (including inactive tabs, floating and auto-hide), 0 = hidden,
 * -1 = not yet registered. Querying menus during startup must be harmless. */
int xideWorkbenchWindowVisible(xide_app* app,int window);
int xideWorkbenchToggleWindow(xide_app* app,int window);
int xideWorkbenchRevealHelp(xide_app*);
#endif
