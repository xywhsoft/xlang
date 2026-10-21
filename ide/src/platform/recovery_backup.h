#ifndef XLANG_IDE_RECOVERY_BACKUP_H
#define XLANG_IDE_RECOVERY_BACKUP_H
#include "platform/recovery_store.h"
#include "platform/text_file.h"
#include <windows.h>
typedef struct xide_recovery_item { char *reference,*label; uint64_t bytes; int archived; } xide_recovery_item;
typedef struct xide_recovery_list { xide_recovery_item* items; size_t count,capacity; } xide_recovery_list;
typedef struct xide_recovery_backup {
    xide_recovery_guard guard; char* directory; unsigned retired; size_t files; uint64_t bytes;
} xide_recovery_backup;
void xideRecoveryListClear(xide_recovery_list* list);
int xideRecoveryListRead(xide_recovery_guard* guard,xide_recovery_list* list,volatile LONG* cancel,xide_model_error* error);
int xideRecoveryRead(xide_recovery_guard* guard,const char* reference,xide_text_file* text,xide_model_error* error);
int xideRecoveryBackupsCheck(xide_recovery_guard* guard,xide_model_error* error);
/* On success the caller owns the checkpoint lock until rebuild or undo. */
int xideRecoveryBackupCreate(const char* directory,volatile LONG* cancel,xide_recovery_backup* result,xide_model_error* error);
int xideRecoveryBackupUndo(xide_recovery_backup* backup,xide_model_error* error);
void xideRecoveryBackupClear(xide_recovery_backup* backup);
#endif
