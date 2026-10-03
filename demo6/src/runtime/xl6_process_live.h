#ifndef XL6_PROCESS_LIVE_H
#define XL6_PROCESS_LIVE_H
#include "src/runtime/xl6_process_internal.h"
#include <stdlib.h>

/* One native process reference, shared by the language Process and its IO
 * adapters. No borrowed OS handles, output caches or script references.
 * Callers own a reference throughout each entry. A stream has one adapter;
 * operations on that adapter are serialized by the caller (XRT IO contract). */
typedef struct XL6_ProcessLive {
    volatile int32 refs;
    xmutex* guard;
    xprocess* native;
    unsigned pipes, leased, closed;
} XL6_ProcessLive;
static inline XL6_ProcessLive* xl6ProcessLiveCheck(void* handle)
{
    if (!handle) xrtSetErrorKind(XERR_ARGUMENT);
    return handle;
}
static inline void xl6ProcessLiveLock(XL6_ProcessLive* process)
{ if (!xrtMutexLock(process->guard)) abort(); }
static inline void xl6ProcessLiveUnlock(XL6_ProcessLive* process)
{ if (!xrtMutexUnlock(process->guard)) abort(); }
static inline bool xl6ProcessStreamBit(int64_t stream, unsigned* bit)
{
    if (stream<XPROCESS_STDIN || stream>XPROCESS_STDERR) {
        xrtSetErrorKind(XERR_RANGE); return false;
    }
    *bit=1u<<(unsigned)stream; return true;
}
#endif
