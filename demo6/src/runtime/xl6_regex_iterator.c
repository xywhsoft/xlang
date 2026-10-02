#include "src/runtime/xl6_regex_internal.h"
typedef struct XL6_RegexIterator {
    XL6_RegexThreadOwner thread;
    xregex* regex;
    xregexmatcher* matcher;
    xvalue* input;
    xstrview text;
    size_t start, remaining;
    bool first, pending, closed;
} XL6_RegexIterator;
static XL6_RegexIterator* check(void* handle)
{
    XL6_RegexIterator* it=handle;
    if (!it) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    if (!xl6RegexThreadCheck(&it->thread)) return NULL;
    return it;
}
static void closeInput(XL6_RegexIterator* it)
{
    it->closed=true; it->pending=false;
    xrtRegexMatcherFree(it->matcher); it->matcher=NULL;
    xrtRegexRelease(it->regex); it->regex=NULL;
    xrtValueRelease(it->input); it->input=NULL; it->text=(xstrview){NULL,0};
}
void xl6RegexIteratorDestroyV1(void* handle)
{ XL6_RegexIterator* it=handle; if (it) {closeInput(it);xl6RegexThreadDrop(&it->thread);xrtFree(it);} }
void* xl6RegexIteratorCreateV1(void* regex, xstrview text, int64_t start, int64_t limit, int64_t size)
{
    size_t count;
    if (!xl6RegexInput(regex,&text,start,size) || !xl6RegexLimit(limit,&count)) return NULL;
    XL6_RegexIterator* it=xrtCalloc(1,sizeof(*it)); if (!it) return NULL;
    if (!xl6RegexThreadInit(&it->thread)) goto fail;
    it->start=(size_t)start; it->remaining=count; it->first=true;
    it->regex=xrtRegexRef(regex); if (!it->regex) goto fail;
    it->input=xrtValueString(text);
    if (!it->input || !xrtValueGetString(it->input,&it->text)) goto fail;
    it->matcher=xrtRegexMatcherCreate(regex); if (!it->matcher) goto fail;
    return it;
fail:
    xl6RegexIteratorDestroyV1(it); return NULL;
}
bool xl6RegexIteratorHasNextV1(void* handle)
{
    XL6_RegexIterator* it=check(handle); if (!it || it->closed) return false;
    if (it->pending) return true;
    if (!it->remaining) {closeInput(it); return false;}
    xregexresult r=it->first ? xrtRegexMatcherFind(it->matcher,it->text,it->start):xrtRegexMatcherNext(it->matcher);
    it->first=false;
    if (r!=XREGEX_MATCH) {closeInput(it); return false;}
    it->pending=true; return true;
}
void* xl6RegexIteratorSnapshotV1(void* handle)
{
    XL6_RegexIterator* it=check(handle);
    if (!it) return NULL;
    if (!it->pending || it->closed) {xrtSetErrorKind(XERR_STATE);return NULL;}
    return xl6RegexSnapshotCopy(it->regex,it->input,it->matcher);
}
void xl6RegexIteratorConsumeV1(void* handle)
{
    XL6_RegexIterator* it=check(handle); if (!it) return;
    if (!it->pending || it->closed) {xrtSetErrorKind(XERR_STATE);return;}
    it->pending=false; if (it->remaining!=SIZE_MAX) --it->remaining;
}
void xl6RegexIteratorCloseV1(void* handle)
{ XL6_RegexIterator* it=check(handle); if (it) closeInput(it); }
bool xl6RegexIteratorClosedV1(void* handle)
{ XL6_RegexIterator* it=check(handle); return it ? it->closed:false; }
