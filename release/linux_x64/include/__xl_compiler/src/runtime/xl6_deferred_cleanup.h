#ifndef XL6_DEFERRED_CLEANUP_H
#define XL6_DEFERRED_CLEANUP_H
#include "src/common/xl6_xrt.h"
#include "src/api/xl_foreign_error.h"
/* Intrusive, allocation-free scheduling for resident runtime subsystems.
 * Records and Run code must outlive the runtime; generated modules must not
 * register callbacks here. A safe point processes one detached batch, never
 * recursively spins on work deferred by an active native operation/thread. */
typedef struct XL6_DeferredCleanupV1 {
    struct XL6_DeferredCleanupV1* next;
    xatomic32 scheduled;
    bool (*Run)(void);
} XL6_DeferredCleanupV1;
XL6_FOREIGN_API void xl6DeferredCleanupScheduleV1(XL6_DeferredCleanupV1*);
#endif
