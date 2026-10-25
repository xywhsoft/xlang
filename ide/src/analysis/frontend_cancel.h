#ifndef XLANG_ANALYSIS_FRONTEND_CANCEL_H
#define XLANG_ANALYSIS_FRONTEND_CANCEL_H
#include <stddef.h>
#include <stdint.h>

/* Private compiler-adapter scope. One owner thread, strictly nested lifetime;
 * callbacks only inspect their cancellation token and must not reenter this
 * scope. No signal, longjmp, transport or platform dependency. */
enum xls_frontend_cancel_kind {
    XLS_CANCEL_SEM_MODULE, XLS_CANCEL_SEM_FUNCTION, XLS_CANCEL_SEM_STATEMENT,
    XLS_CANCEL_SEM_EXPRESSION, XLS_CANCEL_IR_NODE, XLS_CANCEL_IR_PHASE,
    XLS_CANCEL_IR_VERIFY, XLS_CANCEL_KIND_COUNT
};
typedef struct xls_frontend_cancel_work {
    uint64_t points[XLS_CANCEL_KIND_COUNT],predicateCalls;
    enum xls_frontend_cancel_kind kind;
    int cancelled;
} xls_frontend_cancel_work;
typedef struct xls_frontend_cancel_scope {
    struct xls_frontend_cancel_scope* previous;
    int (*cancelled)(void*);
    void* user;
    xls_frontend_cancel_work work;
    unsigned pending;
    int polling,active;
} xls_frontend_cancel_scope;
extern _Thread_local xls_frontend_cancel_scope* xlsActiveFrontendCancel;
void xlsFrontendCancelBegin(xls_frontend_cancel_scope* scope,int (*cancelled)(void*),void* user);
void xlsFrontendCancelEnd(xls_frontend_cancel_scope* scope,xls_frontend_cancel_work* work);
int xlsFrontendCancelPoll(void);
xls_frontend_cancel_work xlsFrontendCancelWork(void);
static inline int xlsFrontendCancelObserved(void)
{return xlsActiveFrontendCancel && xlsActiveFrontendCancel->work.cancelled;}
/* At most 64 recursive work points between predicate calls, with forced
 * checks at function/module-pass boundaries. This bounds work, not wall time:
 * parser, imports and an individual unchecked compiler helper can still run
 * until the next audited point. Once observed, cancellation stays latched. */
static inline int xlsFrontendCancelPoint(enum xls_frontend_cancel_kind kind,int force)
{
    xls_frontend_cancel_scope* scope=xlsActiveFrontendCancel;
    if(!scope)return 0;
    if(scope->work.cancelled)return 1;
    ++scope->work.points[kind];scope->work.kind=kind;
    if(!force && ++scope->pending<64)return 0;
    scope->pending=0;return xlsFrontendCancelPoll();
}
#endif
