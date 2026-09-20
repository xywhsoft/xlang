#include "xlir_continuation_closure.generated.h"
#include "xlir_continuation_closure.generated.c"
typedef struct ContinuationClosureHooks {
    xfuture* source;
    void (*capture)(xvalue*);
} ContinuationClosureHooks;
static ContinuationClosureHooks hooks;
void continuation_closure_bind(const ContinuationClosureHooks* input) { hooks = *input; }
xfuture* continuation_closure_source(void) { return xrtFutureRef(hooks.source); }
void continuation_closure_capture(xvalue* value) { hooks.capture(value); }
