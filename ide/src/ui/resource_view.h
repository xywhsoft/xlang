#ifndef XLANG_IDE_RESOURCE_VIEW_H
#define XLANG_IDE_RESOURCE_VIEW_H
#include "app/xide_app.h"
enum xide_resource_control { XIDE_RES_CONFIG,XIDE_RES_VERSION_ENABLED,XIDE_RES_LANGUAGE,XIDE_RES_CODEPAGE,
 XIDE_RES_COMMENTS,XIDE_RES_COMPANY,XIDE_RES_DESCRIPTION,XIDE_RES_FILE_VERSION,XIDE_RES_INTERNAL_NAME,XIDE_RES_COPYRIGHT,XIDE_RES_FILENAME,XIDE_RES_PRODUCT,XIDE_RES_PRODUCT_VERSION,
 XIDE_RES_ICON_MODE,XIDE_RES_ICON,XIDE_RES_ICON_BROWSE,XIDE_RES_MANIFEST_MODE,XIDE_RES_MANIFEST,XIDE_RES_MANIFEST_BROWSE,XIDE_RES_VALIDATE,XIDE_RES_PREVIEW,XIDE_RES_APPLY,XIDE_RES_CANCEL,XIDE_RES_NEW_PATH,XIDE_RES_NEW_BROWSE,XIDE_RES_CONTROL_COUNT };
int xideResourceViewCreate(xide_app* app);
void xideResourceViewDestroy(xide_app* app);
int xideResourceViewOpen(xide_app* app);
int xideResourceViewOpenNew(xide_app* app);
int xideResourceViewIsOpen(xide_app* app);
void xideResourceViewLayout(xide_app* app);
int xideResourceViewApply(xide_app* app,xide_model_error* error);
xui_widget xideResourceViewWindow(xide_app* app);
xui_widget xideResourceViewControl(xide_app* app,int control);
const char* xideResourceViewMessage(xide_app* app);
#endif
