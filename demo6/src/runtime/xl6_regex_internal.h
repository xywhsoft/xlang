#ifndef XL6_REGEX_INTERNAL_H
#define XL6_REGEX_INTERNAL_H
#include "src/runtime/xl6_text_internal.h"

static inline bool xl6RegexConfig(int64_t flags, int64_t bytes, int64_t captures, xregexconfig* out)
{
    if (flags < 0 || flags > 15 || bytes <= 0 || captures <= 0 ||
        (uint64_t)bytes > SIZE_MAX || (uint64_t)captures > SIZE_MAX) {
        xrtSetErrorKind(XERR_RANGE); return false;
    }
    xrtRegexConfigInit(out); out->Flags=(uint32)flags;
    out->MaxPatternBytes=(size_t)bytes; out->MaxCaptures=(size_t)captures;
    return true;
}
static inline bool xl6RegexInput(void* regex, xstrview* text, int64_t start, int64_t size)
{
    if (!regex) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (!xl6TextSelect(text, size)) return false;
    if (start < 0 || (uint64_t)start > text->Size ||
        ((uint64_t)start < text->Size && ((unsigned char)text->Data[start]&0xC0)==0x80)) {
        xrtSetErrorKind(XERR_RANGE); return false;
    }
    return true;
}
static inline bool xl6RegexLimit(int64_t limit, size_t* out)
{
    if (limit < -1 || (limit >= 0 && (uint64_t)limit > SIZE_MAX)) {
        xrtSetErrorKind(XERR_RANGE); return false;
    }
    *out=limit<0 ? SIZE_MAX:(size_t)limit; return true;
}
/* One immutable carrier for all snapshot inputs. Internal retaining is not
 * language byref: no mutator or borrowed view escapes through the facade. */
#if !defined(XRT_SINGLE_HEADER) || defined(XRT_FEATURE_REGEX_MATCH)
typedef struct XL6_RegexSnapshotCapture {
    xregexspan span; bool matched;
} XL6_RegexSnapshotCapture;
typedef struct XL6_RegexSnapshot {
    xregex* regex;
    xvalue* input;
    xstrview text;
    size_t count;
    bool matched;
    XL6_RegexSnapshotCapture captures[];
} XL6_RegexSnapshot;
static inline void* xl6RegexSnapshotCopy(xregex* regex, xvalue* input, const xregexmatcher* matcher)
{
    size_t count=xrtRegexCaptureCount(regex);
    if (count>(SIZE_MAX-sizeof(XL6_RegexSnapshot))/sizeof(XL6_RegexSnapshotCapture)) {
        xrtSetErrorKind(XERR_RANGE); return NULL;
    }
    XL6_RegexSnapshot* s=xrtCalloc(1,sizeof(*s)+count*sizeof(s->captures[0]));
    if (!s) return NULL;
    s->regex=xrtRegexRef(regex); if (!s->regex) goto fail;
    s->input=xrtValueRetain(input); s->count=count;
    if (!s->input || !xrtValueGetString(s->input,&s->text)) goto fail;
    s->matched=xrtRegexMatcherMatched(matcher);
    if (s->matched) for (size_t i=0;i<count;++i) {
        xregexcapture c;
        if (!xrtRegexMatcherCapture(matcher,i,&c)) goto fail;
        s->captures[i]=(XL6_RegexSnapshotCapture){c.Span,c.Matched};
    }
    return s;
fail:
    xl6RegexMatchDestroyV1(s); return NULL;
}
#endif
#endif
