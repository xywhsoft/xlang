#ifndef XL6_SERIALIZATION_CALLBACK_H
#define XL6_SERIALIZATION_CALLBACK_H
#include "xl6_text_value.h"

/* The verified callable bridge supplies XL6_Function before this header.
 * Each invocation owns its callback failure independently of native parser
 * diagnostics, including nested visits and parser cleanup after a throw. */
typedef struct xl6_serialization_visitor_context {
    XL6_Function Visitor;
    int CallbackFailed;
    xerror* CallbackError;
    xerror* PriorError;
} xl6_serialization_visitor_context;
#if defined(XL6_SERIALIZATION_STATUS_RESULT)
typedef xlir_result_c_int XL6_Serialization_Visit_Result;
#endif

static inline void xl6_serialization_callback_fail(xl6_serialization_visitor_context* context)
{
    if (context->CallbackFailed) return;
    context->CallbackFailed = 1;
    context->CallbackError = xrtTakeError();
    if (context->CallbackError == NULL) {
        context->CallbackError = xrtErrorCreate(XERR_STATE, "xlang", 0,
            "serialization callback failed without an error");
        if (context->CallbackError == NULL)
            context->CallbackError = xrtTakeError();
    }
}

#if defined(XL6_SERIALIZATION_STATUS_RESULT)
static inline int xl6_serialization_callback_call(
    xl6_serialization_visitor_context* context, xvalue* event, bool* keepGoing)
{
    xerror* prior = xrtTakeError();
    xvalue* arguments[1] = {event};
    xvalue* result = context->Visitor.invoke_var != NULL
        ? context->Visitor.invoke_var(context->Visitor.env, 1, arguments) : NULL;
    int ok = result != NULL && xrtValueGetBool(result, keepGoing);
    if (!ok) xl6_serialization_callback_fail(context);
    xrtValueRelease(result);
    xrtValueRelease(event);
    xrtClearError(); xrtSetErrorTake(prior);
    return ok;
}

static inline XL6_Serialization_Visit_Result xl6_serialization_callback_finish(
    xl6_serialization_visitor_context* context, int status)
{
    if (context->CallbackFailed) {
        xrtSetErrorTake(context->CallbackError);
        context->CallbackError = NULL;
        return (XL6_Serialization_Visit_Result){false, 0};
    }
    return (XL6_Serialization_Visit_Result){true, status};
}

/* visitComplete/visitStopped/visitInvalid are values, not exception signals.
 * Native syntax/limit diagnostics are consumed on that successful boundary.
 * Allocation failures and original callback exceptions remain checked failures.
 * PriorError is taken before option validation/parse and consumed exactly once. */
static inline XL6_Serialization_Visit_Result xl6_serialization_visit_finish(
    xl6_serialization_visitor_context* context, int status)
{
    xerror* prior = context->PriorError;
    context->PriorError = NULL;
    if (context->CallbackFailed) {
        xrtErrorFree(prior);
        return xl6_serialization_callback_finish(context, status);
    }
    if (xrtErrorKind(xrtGetError()) == XERR_MEMORY) {
        xerror* failure = xrtTakeError();
        xrtErrorFree(prior);
        xrtClearError(); xrtSetErrorTake(failure);
        return (XL6_Serialization_Visit_Result){false, 0};
    }
    xrtClearError(); xrtSetErrorTake(prior);
    return (XL6_Serialization_Visit_Result){true, status};
}
#endif
#endif
