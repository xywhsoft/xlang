#define XRT_MODULE_FUTURE
#include "lib/xrt/xrt.h"
#include "xl6_continuation_future.h"
#include "xl6_ownership_scope.h"

static void continuation_watch_ready(void* data)
{ xl6ContinuationReady((XL6_ContinuationTicket*)data); }
static void continuation_watch_released(void* data)
{ xl6ContinuationTransportReleased((XL6_ContinuationTicket*)data); }
static bool continuation_watch_trace(const void* data, xrtownershipvisitor visit, void* context)
{
    const XL6_ContinuationTicket* ticket = (const XL6_ContinuationTicket*)data;
    return ticket != NULL && ticket->transportOwned && ticket->ownershipRefs == 2 &&
        visit != NULL && visit(xl6ContinuationOwnership(ticket), context);
}

static bool xl6OwnershipBody_xl6ContinuationWatchRegister(XL6_ContinuationTicket* ticket, xfuturewatch* watch, xfuture* source)
{
    xfuturewatchresult status;
    /* Ticket is still private to this registration owner: no event producer
     * exists before successful WatchAdd. Public flags stay opaque elsewhere. */
    if (ticket == NULL || ticket->queue == NULL || ticket->transportOwned || ticket->flags != 0 || watch == NULL || source == NULL) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    /* A successfully registered pending Watch may not have signalled yet.
     * Mark it independently of event bits so Abort or duplicate registration
     * cannot reclaim/reinitialize its storage while XRT owns it. */
    if (ticket->ownershipRefs != 1) { xrtSetErrorKind(XERR_STATE); return false; }
    if (ticket->storage != NULL && !ticket->storage->Retain(ticket->data)) {
        xrtSetErrorKind(XERR_STATE); return false;
    }
    ticket->transportOwned = true; ++ticket->ownershipRefs;
    bool initialized = ticket->storage != NULL && ticket->storage->WatchInit != NULL
        ? ticket->storage->WatchInit(ticket->data, watch)
        : xrtFutureWatchInitTraced(watch, continuation_watch_ready, continuation_watch_released,
            ticket, continuation_watch_trace);
    if (!initialized) {
        ticket->transportOwned = false; --ticket->ownershipRefs;
        if (ticket->storage != NULL) ticket->storage->Release(ticket->data);
        return false;
    }
    status = xrtFutureWatchAdd(source, watch);
    if (status == XFUTURE_WATCH_ERROR) {
        ticket->transportOwned = false; --ticket->ownershipRefs;
        if (ticket->storage != NULL) ticket->storage->Release(ticket->data);
        return false;
    }
    if (status == XFUTURE_WATCH_READY) {
        if(ticket->storage != NULL && ticket->storage->Ready != NULL) ticket->storage->Ready(ticket->data);
        else xl6ContinuationReady(ticket);
        xl6ContinuationTransportReleased(ticket);
    }
    return true;
}
bool xl6ContinuationWatchRegister(XL6_ContinuationTicket* ticket, xfuturewatch* watch, xfuture* source)
{
    XL6_OWNERSHIP_RETURN(bool, xl6OwnershipBody_xl6ContinuationWatchRegister(ticket, watch, source));
}
