#ifndef XLANG_IDE_RECOVERY_VIEW_H
#define XLANG_IDE_RECOVERY_VIEW_H
#include "app/xide_app.h"
enum { XIDE_RECOVERY_LIST,XIDE_RECOVERY_CODE,XIDE_RECOVERY_LANGUAGE,XIDE_RECOVERY_REFRESH,XIDE_RECOVERY_CLEAN,
    XIDE_RECOVERY_REPAIR,XIDE_RECOVERY_RESTORE,XIDE_RECOVERY_CLOSE,XIDE_RECOVERY_CONFIRM,XIDE_RECOVERY_CANCEL_REPAIR,
    XIDE_RECOVERY_GLOBAL_KIND,XIDE_RECOVERY_GLOBAL_REPAIR,XIDE_RECOVERY_CONTROL_COUNT };
int xideRecoveryViewCreate(xide_app* app);
void xideRecoveryViewStop(xide_app* app);
void xideRecoveryViewDestroy(xide_app* app);
int xideRecoveryViewOpen(xide_app* app);
int xideRecoveryViewIsOpen(xide_app* app);
int xideRecoveryViewBusy(xide_app* app);
void xideRecoveryViewPoll(xide_app* app);
void xideRecoveryViewLayout(xide_app* app);
int xideRecoveryViewCodeFont(xide_app* app);
int xideRecoveryViewTheme(xide_app* app);
xui_widget xideRecoveryViewWindow(xide_app* app);
xui_widget xideRecoveryViewControl(xide_app* app,int control);
const char* xideRecoveryViewMessage(xide_app* app);
const char* xideRecoveryViewBackupPath(xide_app* app);
#endif
