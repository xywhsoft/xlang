#include "src/runtime/xl6_future_metadata.h"
#include <limits.h>

/* Every failure sets a diagnostic; successful language observation starts with
 * a clean current diagnostic. Caller entry/error scopes preserve outer errors.
 * Native calls are protected by the caller's actual Future borrow. */
static bool readSnapshot(xfuture* future, xfuturedebugsnapshot* output)
{
    xrtClearError();
    return xrtFutureDebugSnapshot(future,output);
}
bool xl6FutureDebugNameSetV2(xfuture* future, xstrview name)
{
    xrtClearError();
    if (!xrtUtf8Valid(name,NULL)) { xrtSetErrorKind(XERR_TYPE); return false; }
    return xrtFutureDebugSetNameN(future,name.Data,name.Size);
}
xstrview xl6FutureDebugNameCopyV2(xfuture* future)
{
    xstrview name;
    xrtClearError();
    if (!xrtFutureDebugNameCopy(future,&name)) return (xstrview){NULL,0};
    if (!xrtUtf8Valid(name,NULL)) {
        xrtFree((void*)name.Data);
        xrtSetErrorKind(XERR_TYPE); return (xstrview){NULL,0};
    }
    return name;
}
int64_t xl6FutureFirstObservedMsV2(xfuture* future)
{
    xfuturedebugsnapshot snapshot;
    return readSnapshot(future,&snapshot) ? (int64_t)(snapshot.FirstObserved) : 0;
}
int64_t xl6FutureFirstTerminalObservedMsV2(xfuture* future)
{
    xfuturedebugsnapshot snapshot;
    return readSnapshot(future,&snapshot) ? (int64_t)(snapshot.FirstTerminalObserved) : 0;
}
int64_t xl6FuturePendingWatchesV2(xfuture* future)
{
    xfuturedebugsnapshot snapshot;
    if (!readSnapshot(future,&snapshot)) return 0;
    if (snapshot.PendingWatches > (size_t)INT64_MAX) { xrtSetErrorKind(XERR_RANGE); return 0; }
    return (int64_t)snapshot.PendingWatches;
}
