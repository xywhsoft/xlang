#ifndef XLANG_IDE_RENAME_VIEW_H
#define XLANG_IDE_RENAME_VIEW_H
#include "app/xide_app.h"
enum xide_rename_control { XIDE_RENAME_ORIGINAL,XIDE_RENAME_NAME,XIDE_RENAME_TARGET,XIDE_RENAME_APPLY,XIDE_RENAME_CANCEL,XIDE_RENAME_CONTROL_COUNT };
int xideRenameViewCreate(xide_app* app);
void xideRenameViewDestroy(xide_app* app);
int xideRenameViewCanOpen(xide_app* app);
int xideRenameViewCanOpenFile(xide_app* app,const char* path);
int xideRenameViewOpenFile(xide_app* app,const char* path,xide_model_error* error);
int xideRenameViewOpen(xide_app* app);
/* mode uses XIDE_MIGRATE_MANIFEST/DIRECTORY from project_migration.h. */
int xideRenameViewCanMigrateProject(xide_app* app);
int xideRenameViewOpenProject(xide_app* app,int mode,xide_model_error* error);
int xideRenameViewIsOpen(xide_app* app);
int xideRenameViewHasPending(xide_app* app);
int xideRenameViewRefresh(xide_app* app);
int xideRenameViewApply(xide_app* app,xide_model_error* error);
void xideRenameViewLayout(xide_app* app);
xui_widget xideRenameViewWindow(xide_app* app);
xui_widget xideRenameViewControl(xide_app* app,int control);
const char* xideRenameViewMessage(xide_app* app);
#endif
