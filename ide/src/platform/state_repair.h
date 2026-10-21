#ifndef XLANG_IDE_STATE_REPAIR_H
#define XLANG_IDE_STATE_REPAIR_H
#include "platform/state_file.h"
#include <windows.h>
typedef struct xide_state_repair {
    char *path,*directory;
    HANDLE lock,original,parents[256]; size_t parentCount;
    uint64_t bytes; int existed,complete,retired,published;
} xide_state_repair;
/* Worker prepares an independent raw backup and keeps the file's writer lock.
   Preparation never retires or changes the original. Outputs start at zero. */
int xideStateRepairPrepare(const char* path,volatile LONG* cancel,xide_state_repair* result,xide_model_error* error);
/* Owner publishes small captured JSON atomically without replacing a competing
   file. Failure rolls back retirement; raw backup remains independently usable. */
int xideStateRepairPublish(xide_state_repair* repair,xvalue* root,xide_model_error* error);
void xideStateRepairClear(xide_state_repair* repair);
#endif
