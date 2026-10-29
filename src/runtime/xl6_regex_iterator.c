#include "src/runtime/xl6_regex_internal.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
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
    xrtownershipscope mutation={0};
    if(!xrtOwnershipMutationBegin(&mutation))abort();
    xregexmatcher* matcher=it->matcher;xregex* regex=it->regex;xvalue* input=it->input;
    it->closed=true; it->pending=false;
    it->matcher=NULL;it->regex=NULL;it->input=NULL;it->text=(xstrview){NULL,0};
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    /* Detach the owning edge while frozen inspection is excluded, then drop
     * children without an enclosing mutation or a borrowed input view. */
    xrtRegexMatcherFree(matcher);xrtRegexRelease(regex);xrtValueRelease(input);
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

/* Resident policy: terminal Drop runs after the phased handle is detached. */
static bool xl6RegexIteratorHandlePolicyV1GetTrace(const xvalue* value,xrtownershipvisitor visitor,void* context)
{
    void* raw=NULL;
    if(!visitor || !xrtValueGetHandle(value,&raw,NULL,NULL) || !raw)return false;
    const XL6_RegexIterator* payload=raw;
    return (payload->input ? visitor(xrtValueOwnership(payload->input),context) : payload->closed);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6RegexIteratorHandlePolicyV1Get,xl6RegexIteratorDestroyV1,xl6RegexIteratorHandlePolicyV1GetTrace)
