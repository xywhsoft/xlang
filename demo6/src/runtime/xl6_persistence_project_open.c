#include "src/runtime/xl6_persistence_project.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

typedef struct OpenContext { xvaluegraphnextv1* next; xvalue* result; } OpenContext;
static bool projectField(xstrview key,const xvalue* borrowed,ptr user)
{
    const OpenContext* context=user;
    xvalue* item=xrtValueGraphNextV1(context->next,borrowed);
    if (item && xrtValueObjectSetTake(context->result,key,&item)) return true;
    xl6_error_scope cleanup={0};
    xerror* failure=xrtTakeError();
    if (!xl6ErrorScopeBegin(&cleanup)) abort();
    xrtSetErrorTake(failure); xl6ErrorScopeCaptureFailure(&cleanup);
    xrtValueRelease(item);
    xl6_error_result outcome=xl6ErrorScopeEnd(&cleanup);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return false;
}

/* This independently pruned provider is the sole raw DynamicFields access in
 * persistence. Exact ops admission precedes payload access. XRT holds the
 * owner and gates field reentry across Next, protecting keys AND owning edges.
 * Copying a key alone cannot prevent a reentrant erase from losing a busy
 * child's owner reference. No deep copy or second identity memo is involved. */
bool xl6PersistenceProjectOpenV1(const XL6_PersistenceOpenV1* open,
    xvaluegraphnextv1* next, xvalue* result)
{
    if (!open || !open->ops || !next || !result) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    const xvalue* storage=xrtValueGraphObjectGetV1(next,open->storage);
    const xvalue* boxed=storage ? xrtValueObjectGet(storage,open->handle) : NULL;
    ptr raw=NULL; const xvaluehandleops* ops=NULL; ptr user=NULL;
    if (!boxed || !xrtValueGetHandle(boxed,&raw,&ops,&user) ||
        ops!=open->ops || !raw || user) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_TYPE);
        return false;
    }
    OpenContext context={next,result};
    return xrtDynamicFieldsVisitV1((xrtdynamicfields*)raw,projectField,&context);
}
