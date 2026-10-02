#include "src/runtime/xl6_text_internal.h"

typedef struct XL6_RegexSnapshotCapture {
    xregexspan span;
    bool matched;
} XL6_RegexSnapshotCapture;
typedef struct XL6_RegexSnapshot {
    xregex* regex;
    char* text;
    size_t size, count;
    bool matched;
    XL6_RegexSnapshotCapture captures[];
} XL6_RegexSnapshot;

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
    xrtFree(snapshot->text); xrtFree(snapshot);
}
static bool xl6RegexInput(void* regex, xstrview* text, int64_t start, int64_t size)
{
    if (!regex) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (!xl6TextSelect(text, size)) return false;
    if (start < 0 || (uint64_t)start > text->Size ||
        ((uint64_t)start < text->Size && ((unsigned char)text->Data[start] & 0xC0) == 0x80)) {
        xrtSetErrorKind(XERR_RANGE); return false;
    }
    return true;
}
void* xl6RegexMatchCreateV1(void* regex, xstrview text, int64_t mode, int64_t start, int64_t size)
{
    if (mode < 0 || mode > 2 || (mode == 2 && start != 0)) {
        xrtSetErrorKind(XERR_RANGE); return NULL;
    }
    if (!xl6RegexInput(regex, &text, start, size)) return NULL;
    size_t count = xrtRegexCaptureCount(regex);
    if (count > (SIZE_MAX - sizeof(XL6_RegexSnapshot)) / sizeof(XL6_RegexSnapshotCapture)) {
        xrtSetErrorKind(XERR_RANGE); return NULL;
    }
    XL6_RegexSnapshot* snapshot = xrtCalloc(1, sizeof(*snapshot) + count * sizeof(snapshot->captures[0]));
    if (!snapshot) return NULL;
    xregexmatcher* matcher = NULL;
    snapshot->count = count; snapshot->size = text.Size;
    snapshot->regex = xrtRegexRef(regex);
    if (!snapshot->regex) goto fail;
    snapshot->text = xrtStrDupView(text);
    if (!snapshot->text) goto fail;
    matcher = xrtRegexMatcherCreate(regex);
    if (!matcher) goto fail;
    xstrview input = {snapshot->text, snapshot->size};
    xregexresult result = mode == 0 ? xrtRegexMatcherFind(matcher, input, (size_t)start) :
        mode == 1 ? xrtRegexMatcherAt(matcher, input, (size_t)start) : xrtRegexMatcherFull(matcher, input);
    if (result == XREGEX_ERROR) goto fail;
    snapshot->matched = result == XREGEX_MATCH;
    if (snapshot->matched) for (size_t i = 0; i < count; ++i) {
        xregexcapture capture;
        if (!xrtRegexMatcherCapture(matcher, i, &capture)) goto fail;
        snapshot->captures[i] = (XL6_RegexSnapshotCapture){capture.Span, capture.Matched};
    }
    xrtRegexMatcherFree(matcher); return snapshot;
fail:
    xrtRegexMatcherFree(matcher); xl6RegexMatchDestroyV1(snapshot); return NULL;
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
    return xl6TextCopy((xstrview){s->text + c->span.Begin, c->span.End - c->span.Begin});
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
    return s ? xl6TextCopy((xstrview){s->text, s->size}) : xl6TextFailure;
}
