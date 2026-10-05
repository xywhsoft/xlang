#ifndef XLIR_MULTI_AWAIT_HOST_PRELUDE_H
#define XLIR_MULTI_AWAIT_HOST_PRELUDE_H
#include "xruntime_decl.h"
#include "src/runtime/xl6_continuation_future.h"

/* Test-only interposition: the real resident registration runs first while
 * the host still owns the unresolved Promise. Completion is delivered only
 * AFTER its real Watch is attached, BEFORE generated activation Commit.
 * No sleeps, readiness races or synthetic Future notifications are used. */
#ifdef XLIR_MULTI_AWAIT_PENDING
static bool xlir_multi_await_pending_register(XL6_ContinuationTicket*,xfuturewatch*,xfuture*);
#define xl6ContinuationWatchRegister xlir_multi_await_pending_register
#ifdef XLIR_MULTI_AWAIT_AFTER_COMMIT
static void xlir_multi_await_pending_commit(XL6_ContinuationTicket*);
#define xl6ContinuationCommit xlir_multi_await_pending_commit
#endif
#endif
#endif
