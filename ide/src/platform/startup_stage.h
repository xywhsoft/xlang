#ifndef XIDE_STARTUP_STAGE_H
#define XIDE_STARTUP_STAGE_H
#include <windows.h>
#include "platform/startup_cache.h"
typedef struct xide_startup_stage {
    wchar_t *directory,*program,*library,*environment;
    HANDLE originals[2],copies[2];
    BY_HANDLE_FILE_INFORMATION identities[2],directoryIdentity;
    int created[2],directoryCreated;
    xide_startup_cache_lease* cacheLease;
    BY_HANDLE_FILE_INFORMATION cacheMarkerIdentity;
} xide_startup_stage;
int xideStartupDirectoryBytes(const wchar_t* path);
/* Origin is accepted only when both running files match the pinned originals. */
int xideStartupStageOrigin(const wchar_t* actual,wchar_t* origin,DWORD* error);
/* The verified resource origin stays in memory; descendants inherit no context. */
int xideStartupStageForgetOrigin(DWORD* error);
int xideStartupStagePrepare(const wchar_t* original,xide_startup_stage* stage,DWORD* error);
/* Removes only this invocation's two copied binaries and newly created folder. */
void xideStartupStageClear(xide_startup_stage* stage);
#endif
