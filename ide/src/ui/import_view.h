#ifndef XLANG_IDE_IMPORT_VIEW_H
#define XLANG_IDE_IMPORT_VIEW_H
#include "app/xide_app.h"
enum xide_import_control {
 XIDE_IMPORT_SOURCE,XIDE_IMPORT_BROWSE_SOURCE,XIDE_IMPORT_DESTINATION,XIDE_IMPORT_BROWSE_DESTINATION,XIDE_IMPORT_ANALYZE,
 XIDE_IMPORT_SELECTED_PROJECT,XIDE_IMPORT_CANDIDATES,XIDE_IMPORT_ENTRY,XIDE_IMPORT_BROWSE_ENTRY,XIDE_IMPORT_SET_ENTRY,
 XIDE_IMPORT_KIND,XIDE_IMPORT_STARTUP,XIDE_IMPORT_PREVIEW_KIND,XIDE_IMPORT_PREVIEW,
 XIDE_IMPORT_REVIEWED,XIDE_IMPORT_OPEN_AFTER,XIDE_IMPORT_RESTORE_STATE,XIDE_IMPORT_APPLY,XIDE_IMPORT_CANCEL,XIDE_IMPORT_CONTROL_COUNT
};
int xideImportViewCreate(xide_app* app);
void xideImportViewDestroy(xide_app* app);
int xideImportViewOpen(xide_app* app);
int xideImportViewIsOpen(xide_app* app);
void xideImportViewLayout(xide_app* app);
int xideImportViewAnalyze(xide_app* app,xide_model_error* error);
int xideImportViewSetEntry(xide_app* app,xide_model_error* error);
int xideImportViewApply(xide_app* app,xide_model_error* error);
xui_widget xideImportViewWindow(xide_app* app);
xui_widget xideImportViewControl(xide_app* app,int control);
const char* xideImportViewMessage(xide_app* app);
int xideImportViewPublished(xide_app* app);
#endif
