#ifndef XIDE_STARTUP_CACHE_H
#define XIDE_STARTUP_CACHE_H
#include <windows.h>
#include <stddef.h>
typedef struct xide_startup_cache_lease xide_startup_cache_lease;
typedef struct xide_startup_cache_stats {
    size_t scanned,removed,retained,rejected;
    ULONGLONG hashedBytes;
} xide_startup_cache_stats;
#define XIDE_STARTUP_CACHE_GRACE (7ULL*24*60*60*10000000)
/* New v2 folders only. Old unrecorded folders are never inferred to be owned. */
xide_startup_cache_lease* xideStartupCachePublish(const wchar_t* directory,
    const wchar_t* program,BY_HANDLE_FILE_INFORMATION* markerIdentity,DWORD* error);
void xideStartupCacheRelease(xide_startup_cache_lease* lease);
int xideStartupCacheChildHold(const wchar_t* program,DWORD* error);
/* Bounded, nonrecursive and identity checked; active, recent, unknown, modified
   and linked contents remain. nowTicks uses Windows UTC FILETIME units. */
int xideStartupCachePrune(const wchar_t* root,ULONGLONG nowTicks,xide_startup_cache_stats* stats);
void xideStartupCacheMaintain(void);
#endif
