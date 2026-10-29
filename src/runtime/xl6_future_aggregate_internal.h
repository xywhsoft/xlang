#ifndef XL6_FUTURE_AGGREGATE_INTERNAL_H
#define XL6_FUTURE_AGGREGATE_INTERNAL_H
/* Shared resident diagnostics. No callable table or Array dependency. */
enum { XL6_AGGREGATE_ALL, XL6_AGGREGATE_RACE, XL6_AGGREGATE_ANY };
static inline const char* __xl6FutureAggregateNullSourceError(unsigned mode)
{
    return mode == XL6_AGGREGATE_ALL
        ? "task.all source future is null"
        : mode == XL6_AGGREGATE_RACE
            ? "task.race source future is null"
            : "task.whenAny source future is null";
}

static inline void __xl6FutureSetAggregateError(const char* message)
{
    xerrordesc desc = {XERR_INTERNAL, 1, 0, "xlang.async.aggregate",
        "aggregate", message != NULL ? message : "future aggregate failed", "TaskError", NULL};
    xerror* error;
    if (xrtGetError() != NULL) return;
    error = xrtErrorBuild(&desc);
    if (error != NULL) xrtSetErrorTake(error);
    else xrtSetErrorKind(XERR_MEMORY);
}
#endif
