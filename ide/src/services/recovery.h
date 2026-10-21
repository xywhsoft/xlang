#ifndef XLANG_IDE_RECOVERY_SERVICE_H
#define XLANG_IDE_RECOVERY_SERVICE_H
#include "platform/recovery_backup.h"
#include "platform/state_repair.h"
typedef struct xide_recovery_service xide_recovery_service;
enum { XIDE_RECOVERY_SCAN=1,XIDE_RECOVERY_PREVIEW,XIDE_RECOVERY_MAINTAIN,XIDE_RECOVERY_REBUILD,XIDE_RECOVERY_GLOBAL,XIDE_RECOVERY_MIGRATIONS };
typedef struct xide_recovery_result {
    int job,okay; char* reference; xide_model_error error;
    xide_recovery_list list; xide_text_file preview; xide_recovery_stats stats,migrationStats; xide_recovery_backup backup;
    xide_state_repair global;
} xide_recovery_result;
xide_recovery_service* xideRecoveryServiceCreate(const char* directory);
int xideRecoveryServiceStart(xide_recovery_service* service,int job,const char* reference,xide_model_error* error);
int xideRecoveryServiceBusy(xide_recovery_service* service);
void xideRecoveryServiceCancel(xide_recovery_service* service);
int xideRecoveryServiceTake(xide_recovery_service* service,xide_recovery_result* result);
void xideRecoveryResultClear(xide_recovery_result* result,int undoPending);
void xideRecoveryServiceDestroy(xide_recovery_service* service);
#endif
