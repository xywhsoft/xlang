#ifndef XL6_PROCESS_INTERNAL_H
#define XL6_PROCESS_INTERNAL_H
#include "xl6_text_thread.h"
#include "xl6_process_text.h"
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

/* Optional owned native capability, with no generated callback or language
 * graph edge. Command-only users do not depend on File/IO code. */
typedef struct XL6_ProcessIoLease {
    intptr_t handle;
    bool owned;
} XL6_ProcessIoLease;
static inline void xl6ProcessIoLeaseDrop(XL6_ProcessIoLease* lease)
{
    XL6_ProcessIoLease old=*lease;memset(lease,0,sizeof(*lease));
    if(old.owned){
#if defined(_WIN32)
        (void)CloseHandle((HANDLE)old.handle);
#else
        /* Do not retry close after EINTR: the descriptor can already be gone. */
        (void)close((int)old.handle);
#endif
    }
}
/* Owned configuration. XRT borrows its views only until Spawn/Run returns.
 * Optional File capabilities are owned duplicate OS handles, never script
 * graph edges or unowned handles. Spawn makes the child inheritance copies. */
typedef struct XL6_ProcessCommand {
    XL6_TextThreadOwner owner;
    xprocessconfig config;
    size_t envCapacity;
    XL6_ProcessIoLease stdio[3];
} XL6_ProcessCommand;
static inline XL6_ProcessCommand* xl6ProcessCommandCheck(void* handle)
{
    if (!handle) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    XL6_ProcessCommand* command=handle;
    return xl6TextThreadCheck(&command->owner) ? command : NULL;
}
static inline bool xl6ProcessDuration(int64_t milliseconds,uint64* duration)
{
    if (milliseconds<0 || (uint64_t)milliseconds>UINT64_MAX/1000u) {
        xrtSetErrorKind(XERR_RANGE);return false;
    }
    *duration=(uint64_t)milliseconds*1000u;return true;
}
#endif
