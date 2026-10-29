#include "src/runtime/xl6_error_text.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char exact[] = "a\0\xE4\xBD\xA0";
static const xstrview span = {exact, sizeof(exact)-1};
static void same(xstrview text, xstrview expected)
{
    assert(text.Data && text.Size == expected.Size && text.Data[text.Size] == 0);
    assert(!memcmp(text.Data, expected.Data, text.Size));
}
static void owned(xstrview text, xstrview expected)
{ same(text, expected); xrtFree((void*)text.Data); }
static xerror* seed(void)
{
    xerrordescview desc = {XERR_VALUE, 73, 91, span, span, span, span, NULL};
    xerrorlocationview location = {span, 17, 23};
    xerror* error = xrtErrorBuildViewAt(&desc, &location);
    assert(error); return error;
}
static void vectors(void)
{
    /* Warm the process-lifetime callback TLS key outside the per-operation
     * allocator snapshots; the tested errors themselves remain tracked. */
    xrtSetErrorKind(XERR_STATE); xl6ForeignCallbackFail(); xrtClearError();
    for (unsigned cycle = 0; cycle < 100; ++cycle) {
        xl6ProductMemoryBegin(SIZE_MAX);
        xerror* error = seed();
        owned(xl6ErrorMessageCopyV1(error), span);
        owned(xl6ErrorDomainCopyV1(error), span);
        owned(xl6ErrorOperationCopyV1(error), span);
        owned(xl6ErrorDataCopyV1(error), span);
        owned(xl6ErrorFileCopyV1(error), span);
        owned(xl6ErrorKindCopyV1(error), xrtStrView("ValueError"));
        assert(xrtErrorCode(error) == 73 && xrtErrorSystemCode(error) == 91);
        assert(xrtErrorLine(error) == 17 && xrtErrorColumn(error) == 23);
        xrtSetErrorTake(xrtErrorRef(error));
        owned(xl6ErrorMessageCopyV1(error), span);
        assert(xrtGetError() == error); xrtClearError();
        xrtErrorFree(error);
        owned(xl6ErrorMessageCopyV1(NULL), xrtStrView(""));
        error = xl6ErrorCreateV1("IndexError", span, 8, 9);
        assert(error && xrtErrorKind(error) == XERR_RANGE);
        assert(xl6ErrorKindIsV1(error, "LookupError") && xl6ErrorKindIsV1(error, "Error"));
        assert(!xl6ErrorKindIsV1(error, "TypeError"));
        owned(xl6ErrorKindCopyV1(error), xrtStrView("IndexError"));
        same(xrtErrorMessageView(error), span);
        xrtSetErrorTake(error); xl6ErrorWrapAsyncV1("fallback");
        error = xrtTakeError();
        assert(xl6ErrorKindIsV1(error, "AsyncError") && xrtErrorCause(error));
        same(xrtErrorMessageView(error), span);
        same(xrtErrorMessageView(xrtErrorCause(error)), span);
        xrtErrorFree(error);
        xrtSetErrorTake(seed()); xl6ForeignCallbackFail(); error = xrtTakeError();
        same(xrtErrorMessageView(error), span); same(xrtErrorDomainView(error), span);
        same(xrtErrorDataView(error), span); same(xrtErrorFileView(error), span);
        same(xrtErrorOperationView(error), xrtStrView("callback")); xrtErrorFree(error);
        /* A prefix must not forge the language domain or truncate its kind. */
        xerrordescview forged = {XERR_TYPE, 0, 0, {"xlang\0suffix", 12}, {"", 0}, span,
            {"FileError", 9}, NULL};
        error = xrtErrorBuildView(&forged); assert(error);
        assert(!strcmp(xl6ErrorKindNameV1(error), "TypeError")); xrtErrorFree(error);
        forged.Domain = xrtStrView("xlang"); forged.Data = (xstrview){"FileError\0suffix", 16};
        error = xrtErrorBuildView(&forged); assert(error);
        assert(!strcmp(xl6ErrorKindNameV1(error), "TypeError")); xrtErrorFree(error);
        error = xl6ErrorCreateV1("ValueError", (xstrview){NULL, 1}, 1, 1);
        assert(error && xrtErrorKind(error) == XERR_ARGUMENT && !xrtGetError()); xrtErrorFree(error);
        char large[8192]; memset(large, 'x', sizeof(large)); large[1] = 0;
        error = xl6ErrorCreateV1("ValueError", (xstrview){large, sizeof(large)}, 1, 1);
        assert(error); owned(xl6ErrorMessageCopyV1(error), (xstrview){large, sizeof(large)});
        xrtErrorFree(error);
        assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    }
    puts("error spans: 100 exact text/cause/callback/metadata/boundary cycles balanced");
}
static void fault(const char* action, size_t point)
{
    xrtSetErrorKind(XERR_STATE); xl6ForeignCallbackFail(); xrtClearError();
    xl6ProductMemoryBegin(SIZE_MAX);
    xerror* error = seed();
    assert(xrtMemDebugFailAfter(point));
    xstrview result = {NULL, 0};
    bool create = !strcmp(action, "create"), async = !strcmp(action, "async"), callback = !strcmp(action, "callback");
    if (create) { xrtErrorFree(error); error = xl6ErrorCreateV1("ValueError", span, 1, 1); }
    else if (async || callback) {
        xrtSetErrorTake(error);
        if (async) xl6ErrorWrapAsyncV1("fallback"); else xl6ForeignCallbackFail();
        error = xrtTakeError();
    } else if (!strcmp(action, "message")) result = xl6ErrorMessageCopyV1(error);
    else if (!strcmp(action, "domain")) result = xl6ErrorDomainCopyV1(error);
    else if (!strcmp(action, "operation")) result = xl6ErrorOperationCopyV1(error);
    else if (!strcmp(action, "data")) result = xl6ErrorDataCopyV1(error);
    else if (!strcmp(action, "file")) result = xl6ErrorFileCopyV1(error);
    else { assert(!strcmp(action, "kind")); result = xl6ErrorKindCopyV1(error); }
    bool hit = xl6ProductMemoryEnd();
    if (create || async || callback) {
        assert(error && !xrtGetError());
        if (hit && !callback) assert(xrtErrorKind(error) == XERR_MEMORY);
        else same(xrtErrorMessageView(error), span);
    } else if (hit) assert(!result.Data && !result.Size && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    else { assert(!xrtGetError()); same(result, !strcmp(action, "kind") ? xrtStrView("ValueError") : span); }
    xrtFree((void*)result.Data); xrtErrorFree(error); xrtClearError(); xl6ProductMemoryVerify();
    printf("error spans %s point=%zu hit=%d balanced\n", action, point, hit);
}
int main(int argc, char** argv)
{
    if (argc == 1) vectors();
    else { assert(argc == 3); fault(argv[1], (size_t)strtoull(argv[2], NULL, 10)); }
    return 0;
}
