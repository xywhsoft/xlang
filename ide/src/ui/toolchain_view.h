#ifndef XLANG_IDE_TOOLCHAIN_VIEW_H
#define XLANG_IDE_TOOLCHAIN_VIEW_H
#include "app/xide_app.h"
enum xide_toolchain_control { XIDE_TOOL_INFO, XIDE_TOOL_HELP, XIDE_TOOL_REFRESH, XIDE_TOOL_CANCEL, XIDE_TOOL_CLOSE, XIDE_TOOL_CONTROL_COUNT };
int xideToolchainViewCreate(xide_app* app);
void xideToolchainViewStop(xide_app* app);
void xideToolchainViewDestroy(xide_app* app);
int xideToolchainViewOpen(xide_app* app);
int xideToolchainViewCanOpen(xide_app* app);
int xideToolchainViewIsOpen(xide_app* app);
int xideToolchainViewBusy(xide_app* app);
void xideToolchainViewPoll(xide_app* app);
void xideToolchainViewLayout(xide_app* app);
xui_widget xideToolchainViewWindow(xide_app* app);
xui_widget xideToolchainViewControl(xide_app* app,int control);
const char* xideToolchainViewMessage(xide_app* app);
#endif
