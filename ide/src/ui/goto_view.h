#ifndef XLANG_IDE_GOTO_VIEW_H
#define XLANG_IDE_GOTO_VIEW_H
#include "app/xide_app.h"
enum xide_goto_control { XIDE_GOTO_POSITION, XIDE_GOTO_APPLY, XIDE_GOTO_CANCEL, XIDE_GOTO_CONTROL_COUNT };
int xideGotoViewCreate(xide_app* app);
void xideGotoViewDestroy(xide_app* app);
int xideGotoViewOpen(xide_app* app);
int xideGotoViewIsOpen(xide_app* app);
int xideGotoViewApply(xide_app* app,xide_model_error* error);
void xideGotoViewLayout(xide_app* app);
xui_widget xideGotoViewWindow(xide_app* app);
xui_widget xideGotoViewControl(xide_app* app,int control);
const char* xideGotoViewMessage(xide_app* app);
#endif
