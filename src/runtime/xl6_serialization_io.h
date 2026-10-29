#ifndef XL6_SERIALIZATION_IO_H
#define XL6_SERIALIZATION_IO_H
#include "xl6_text_value.h"
#include "xl6_serialization_options.h"
#include "xl6_closure.h"

/* The status is independent of both the bool result and diagnostic allocation.
 * Ordinary native write failures are successful false; OOM, invalid options
 * and language callback failures take the checked failure edge. */
static inline bool xl6SerializeWriteFailV1(xerror* prior, xerrkind fallback)
{
    xerror* failure = xrtTakeError();
    if (failure == NULL) { xrtSetErrorKind(fallback); failure = xrtTakeError(); }
    xrtErrorFree(prior);
    xrtClearError(); xrtSetErrorTake(failure);
    return false;
}

static inline bool xl6SerializeWriteFinishV1(bool written, xerror* prior, bool* result)
{
    if (xrtErrorKind(xrtGetError()) == XERR_MEMORY)
        return xl6SerializeWriteFailV1(prior, XERR_MEMORY);
    xrtClearError(); xrtSetErrorTake(prior);
    *result = written;
    return true;
}

/* Paths have exact language lengths. Marshal only at the explicit C ABI
 * boundary; embedded NUL must never name a different prefix file. */
#define XL6_SERIALIZATION_FILE_V1(Name, Config, Options, Write) \
static inline bool Name(const xvalue* wire, const xvalue* options, xstrview path, bool* result) \
{ \
    Config config; xerror* prior; char* name; bool written; \
    if (result == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return false; } \
    *result = false; prior = xrtTakeError(); \
    if (wire == NULL || !Options(options, &config)) \
        return xl6SerializeWriteFailV1(prior, XERR_ARGUMENT); \
    if (path.Data == NULL || memchr(path.Data, 0, path.Size) != NULL) { \
        xrtSetErrorKind(XERR_ARGUMENT); \
        return xl6SerializeWriteFinishV1(false, prior, result); \
    } \
    name = xrtStrDupView(path); \
    if (name == NULL) return xl6SerializeWriteFailV1(prior, XERR_MEMORY); \
    written = Write(name, wire, &config); \
    { xerror* failure = xrtTakeError(); xrtFree(name); \
      xrtClearError(); xrtSetErrorTake(failure); } \
    return xl6SerializeWriteFinishV1(written, prior, result); \
}
XL6_SERIALIZATION_FILE_V1(xl6JsonWriteFileV1, xjsonwriteconfig, xl6JsonWriteOptions, xrtJsonWriteFile)
XL6_SERIALIZATION_FILE_V1(xl6XlonWriteFileV1, xxlonwriteconfig, xl6XlonWriteOptions, xrtXlonWriteFile)
XL6_SERIALIZATION_FILE_V1(xl6JsonlWriteFileV1, xjsonlwriteconfig, xl6JsonlWriteOptions, xrtJsonlWriteFile)
XL6_SERIALIZATION_FILE_V1(xl6XlonlWriteFileV1, xxlonlwriteconfig, xl6XlonlWriteOptions, xrtXlonlWriteFile)
#undef XL6_SERIALIZATION_FILE_V1

/* bool/default facades share the same status/output boundary, without an
 * options dictionary or boolean boxing. The native writer and path owner
 * obey exactly the configured writer's failure and retirement contract. */
#define XL6_SERIALIZATION_FILE_PRETTY_V1(Name, Write) \
static inline bool Name(const xvalue* wire, bool pretty, xstrview path, bool* result) \
{ \
    xerror* prior; char* name; bool written; \
    if (result == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return false; } \
    *result = false; prior = xrtTakeError(); \
    if (wire == NULL) return xl6SerializeWriteFailV1(prior, XERR_ARGUMENT); \
    if (path.Data == NULL || memchr(path.Data, 0, path.Size) != NULL) { \
        xrtSetErrorKind(XERR_ARGUMENT); return xl6SerializeWriteFinishV1(false, prior, result); \
    } \
    name = xrtStrDupView(path); \
    if (name == NULL) return xl6SerializeWriteFailV1(prior, XERR_MEMORY); \
    written = Write(name, wire, pretty); \
    { xerror* failure = xrtTakeError(); xrtFree(name); \
      xrtClearError(); xrtSetErrorTake(failure); } \
    return xl6SerializeWriteFinishV1(written, prior, result); \
}
XL6_SERIALIZATION_FILE_PRETTY_V1(xl6JsonWriteFilePrettyV1, xrtJsonStringifyFile)
XL6_SERIALIZATION_FILE_PRETTY_V1(xl6XlonWriteFilePrettyV1, xrtXlonStringifyFile)
#undef XL6_SERIALIZATION_FILE_PRETTY_V1

/* Exact typed closure ABI: status + bool output + borrowed dynamic argument.
 * The generated caller has verified func(var) as bool, not XL6_Function's
 * erased invoke_var bridge. No callable/result boxing is necessary. */
typedef struct XL6_SerializeSinkV1 {
    XL6_Closure* closure;
    bool failed;
    xerror* error;
} XL6_SerializeSinkV1;

static inline void xl6SerializeSinkFailV1(XL6_SerializeSinkV1* context, xerrkind fallback)
{
    context->failed = true;
    context->error = xrtTakeError();
    if (context->error == NULL) {
        xrtSetErrorKind(fallback);
        context->error = xrtTakeError();
    }
}

static inline bool xl6SerializeSinkWriteV1(xbytesview data, void* user)
{
    XL6_SerializeSinkV1* context = (XL6_SerializeSinkV1*)user;
    xerror* prior;
    xvalue* chunk;
    bool accepted = false;
    if (context->failed) return false;
    prior = xrtTakeError();
    chunk = xrtValueBytes(data);
    if (chunk == NULL) xl6SerializeSinkFailV1(context, XERR_MEMORY);
    else {
        XL6_ClosureScope scope = {0};
        bool invoked;
        xl6ClosureRuntimeV1Get()->Enter(&scope, context->closure);
        invoked = ((bool (*)(void*, bool*, xvalue*))context->closure->invoke)(
            context->closure->environment, &accepted, chunk);
        if (!invoked)
            xl6SerializeSinkFailV1(context, XERR_STATE);
        else if (!accepted) {
            xrtSetErrorKind(XERR_IO);
            xl6SerializeSinkFailV1(context, XERR_IO);
        }
        xl6ClosureRuntimeV1Get()->Leave(&scope);
    }
    xrtValueRelease(chunk);
    xrtClearError(); xrtSetErrorTake(prior);
    return !context->failed;
}

/* No full-output Buffer/string intermediary: XRT calls the typed language
 * closure for each exact bytes chunk. Each invocation owns its error state,
 * including nested/reentrant writers and failures during writer unwinding. */
#define XL6_SERIALIZATION_SINK_V1(Name, Config, Options, Write) \
static inline bool Name(const xvalue* wire, const xvalue* options, \
    XL6_Closure* closure, bool* result) \
{ \
    Config config; xerror* prior; bool written; \
    XL6_SerializeSinkV1 context = {closure, false, NULL}; \
    if (result == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return false; } \
    *result = false; prior = xrtTakeError(); \
    if (wire == NULL || closure == NULL || closure->invoke == NULL || !Options(options, &config)) \
        return xl6SerializeWriteFailV1(prior, XERR_ARGUMENT); \
    written = Write(wire, &config, xl6SerializeSinkWriteV1, &context); \
    if (context.failed) { \
        xrtClearError(); xrtSetErrorTake(context.error); \
        return xl6SerializeWriteFailV1(prior, XERR_STATE); \
    } \
    return xl6SerializeWriteFinishV1(written, prior, result); \
}
XL6_SERIALIZATION_SINK_V1(xl6JsonWriteSinkV1, xjsonwriteconfig, xl6JsonWriteOptions, xrtJsonWrite)
XL6_SERIALIZATION_SINK_V1(xl6XlonWriteSinkV1, xxlonwriteconfig, xl6XlonWriteOptions, xrtXlonWrite)
XL6_SERIALIZATION_SINK_V1(xl6JsonlWriteSinkV1, xjsonlwriteconfig, xl6JsonlWriteOptions, xrtJsonlWrite)
XL6_SERIALIZATION_SINK_V1(xl6XlonlWriteSinkV1, xxlonlwriteconfig, xl6XlonlWriteOptions, xrtXlonlWrite)
#undef XL6_SERIALIZATION_SINK_V1
#endif
