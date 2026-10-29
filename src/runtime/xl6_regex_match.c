#include "src/runtime/xl6_regex_internal.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"

static XL6_RegexSnapshot* xl6RegexSnapshotCheck(void* handle)
{
    if (!handle) xrtSetErrorKind(XERR_ARGUMENT);
    return handle;
}
void xl6RegexMatchDestroyV1(void* handle)
{
    XL6_RegexSnapshot* snapshot = handle;
    if (!snapshot) return;
    xrtRegexRelease(snapshot->regex);
    xrtValueRelease(snapshot->input); xrtFree(snapshot);
}
void* xl6RegexMatchCreateV1(void* regex, xstrview text, int64_t mode, int64_t start, int64_t size)
{
    if (mode < 0 || mode > 2 || (mode == 2 && start != 0)) {
        xrtSetErrorKind(XERR_RANGE); return NULL;
    }
    if (!xl6RegexInput(regex, &text, start, size)) return NULL;
    xvalue* input=xrtValueString(text);
    if (!input) return NULL;
    xregexmatcher* matcher=xrtRegexMatcherCreate(regex);
    void* snapshot=NULL;
    if (matcher) {
        xregexresult result = mode == 0 ? xrtRegexMatcherFind(matcher, text, (size_t)start) :
            mode == 1 ? xrtRegexMatcherAt(matcher, text, (size_t)start) : xrtRegexMatcherFull(matcher, text);
        if (result != XREGEX_ERROR) snapshot=xl6RegexSnapshotCopy(regex,input,matcher);
    }
    xrtRegexMatcherFree(matcher); xrtValueRelease(input); return snapshot;
}
bool xl6RegexTestV1(void* regex, xstrview text, bool full, int64_t size)
{
    if (!xl6RegexInput(regex, &text, 0, size)) return false;
    xregexresult result = full ? xrtRegexFullTest(regex, text) : xrtRegexTest(regex, text);
    return result == XREGEX_MATCH;
}
bool xl6RegexMatchMatchedV1(void* handle)
{
    const XL6_RegexSnapshot* s = xl6RegexSnapshotCheck(handle);
    return s ? s->matched : false;
}
int64_t xl6RegexMatchCountV1(void* handle)
{
    const XL6_RegexSnapshot* s = xl6RegexSnapshotCheck(handle);
    return s ? (int64_t)s->count : 0;
}
static const XL6_RegexSnapshotCapture* xl6RegexCaptureCheck(XL6_RegexSnapshot* s, int64_t index)
{
    if (!s) return NULL;
    if (index < 0 || (uint64_t)index >= s->count) { xrtSetErrorKind(XERR_RANGE); return NULL; }
    return &s->captures[index];
}
xstrview xl6RegexMatchTextV1(void* handle, int64_t index)
{
    XL6_RegexSnapshot* s = xl6RegexSnapshotCheck(handle);
    const XL6_RegexSnapshotCapture* c = xl6RegexCaptureCheck(s, index);
    if (!c || !c->matched) return xl6TextFailure;
    xstrview text={s->text.Data+c->span.Begin,c->span.End-c->span.Begin};
    return xl6TextSelect(&text,-1)?xl6TextCopy(text):xl6TextFailure;
}
bool xl6RegexMatchGroupMatchedV1(void* handle, int64_t index)
{
    const XL6_RegexSnapshotCapture* c = xl6RegexCaptureCheck(xl6RegexSnapshotCheck(handle), index);
    return c ? c->matched : false;
}
int64_t xl6RegexMatchStartV1(void* handle, int64_t index)
{
    const XL6_RegexSnapshotCapture* c = xl6RegexCaptureCheck(xl6RegexSnapshotCheck(handle), index);
    return c && c->matched ? (int64_t)c->span.Begin : -1;
}
int64_t xl6RegexMatchEndV1(void* handle, int64_t index)
{
    const XL6_RegexSnapshotCapture* c = xl6RegexCaptureCheck(xl6RegexSnapshotCheck(handle), index);
    return c && c->matched ? (int64_t)c->span.End : -1;
}
xstrview xl6RegexMatchNameV1(void* handle, int64_t index)
{
    XL6_RegexSnapshot* s = xl6RegexSnapshotCheck(handle);
    if (!xl6RegexCaptureCheck(s, index)) return xl6TextFailure;
    xstrview name;
    return xrtRegexCaptureName(s->regex, (size_t)index, &name) ? xl6TextCopy(name) : xl6TextFailure;
}
int64_t xl6RegexMatchIndexV1(void* handle, xstrview name, int64_t size)
{
    XL6_RegexSnapshot* s = xl6RegexSnapshotCheck(handle);
    if (!s || !xl6TextSelect(&name, size) || !name.Size) return -1;
    size_t index = xrtRegexCaptureIndex(s->regex, name);
    return index == XRT_NPOS ? -1 : (int64_t)index;
}
xstrview xl6RegexMatchInputV1(void* handle)
{
    XL6_RegexSnapshot* s = xl6RegexSnapshotCheck(handle);
    return s ? xl6TextCopy(s->text) : xl6TextFailure;
}

/* Resident policy: terminal Drop runs after the phased handle is detached. */
static bool xl6RegexMatchHandlePolicyV1GetTrace(const xvalue* value,xrtownershipvisitor visitor,void* context)
{
    void* raw=NULL;
    if(!visitor || !xrtValueGetHandle(value,&raw,NULL,NULL) || !raw)return false;
    const XL6_RegexSnapshot* payload=raw;
    return (payload->input && visitor(xrtValueOwnership(payload->input),context));
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6RegexMatchHandlePolicyV1Get,xl6RegexMatchDestroyV1,xl6RegexMatchHandlePolicyV1GetTrace)
