#ifndef XLANG_IDE_RECOVERY_STORE_H
#define XLANG_IDE_RECOVERY_STORE_H
#include "model/workspace.h"
#include <stdint.h>
/* All handles belong to one ordinary session directory; release in all paths. */
typedef struct xide_recovery_guard { void *directoryHandle,*recoveryHandle,*lockHandle; char* directory; } xide_recovery_guard;
typedef struct xide_recovery_stats {
    size_t scanned,referenced,retained,removed,skipped;
    uint64_t removedBytes;
} xide_recovery_stats;
int xideRecoveryLock(xide_recovery_guard* guard,const char* directory,xide_model_error* error);
void xideRecoveryUnlock(xide_recovery_guard* guard);
/* Literal child path. Refuses traversal, directories and reparse-point files. */
char* xideRecoveryPath(const char* directory,const char* relative,xide_model_error* error);
/* Refresh last-reference time before retiring a published checkpoint. Content stays immutable. */
int xideRecoveryRetain(xide_recovery_guard* guard,const xvalue* session,uint64_t nowFileTime,xide_model_error* error);
int xideRecoveryCollect(xide_recovery_guard* guard,uint64_t nowFileTime,
    int (*validateSession)(const xvalue*),xide_recovery_stats* stats,xide_model_error* error);
#endif
