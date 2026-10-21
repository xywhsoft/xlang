#ifndef XLANG_IDE_TEMPLATE_VIEW_H
#define XLANG_IDE_TEMPLATE_VIEW_H
#include "app/xide_app.h"
enum xide_template_control { XIDE_TEMPLATE_KIND,XIDE_TEMPLATE_NAME,XIDE_TEMPLATE_PARENT,XIDE_TEMPLATE_BROWSE,XIDE_TEMPLATE_FILES,XIDE_TEMPLATE_PREVIEW,XIDE_TEMPLATE_STARTUP,XIDE_TEMPLATE_CREATE,XIDE_TEMPLATE_CANCEL,XIDE_TEMPLATE_CONTROL_COUNT };
int xideTemplateViewCreate(xide_app* app);
void xideTemplateViewDestroy(xide_app* app);
int xideTemplateViewOpen(xide_app* app);
int xideTemplateViewIsOpen(xide_app* app);
void xideTemplateViewLayout(xide_app* app);
int xideTemplateViewApply(xide_app* app,xide_model_error* error);
int xideTemplateViewRefresh(xide_app* app);
int xideTemplateViewCodeFont(xide_app* app);
xui_widget xideTemplateViewWindow(xide_app* app);
xui_widget xideTemplateViewControl(xide_app* app,int control);
const char* xideTemplateViewMessage(xide_app* app);
#endif
