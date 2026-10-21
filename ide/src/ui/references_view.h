#ifndef XLANG_IDE_REFERENCES_VIEW_H
#define XLANG_IDE_REFERENCES_VIEW_H
#include "app/xide_app.h"
enum xide_reference_control { XIDE_REF_CATEGORY,XIDE_REF_LIST,XIDE_REF_KIND,XIDE_REF_VALUE,XIDE_REF_BROWSE,XIDE_REF_ADD,XIDE_REF_UPDATE,XIDE_REF_REMOVE,XIDE_REF_UP,XIDE_REF_DOWN,XIDE_REF_APPLY,XIDE_REF_CANCEL,XIDE_REF_INSPECT,XIDE_REF_DOCUMENTATION,XIDE_REF_CONTROL_COUNT };
int xideReferencesViewCreate(xide_app* app);
void xideReferencesViewDestroy(xide_app* app);
int xideReferencesViewOpen(xide_app* app);
int xideReferencesViewIsOpen(xide_app* app);
void xideReferencesViewLayout(xide_app* app);
int xideReferencesViewApply(xide_app* app,xide_model_error* error);
xui_widget xideReferencesViewWindow(xide_app* app);
xui_widget xideReferencesViewControl(xide_app* app,int control);
const char* xideReferencesViewMessage(xide_app* app);
int xideReferencesViewSuspended(xide_app* app);
const char* xideReferencesViewLibraryDetails(xide_app* app);
#endif
