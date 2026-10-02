#include "src/runtime/xl6_regex_internal.h"
typedef struct XL6_RegexSplitIterator {
    uint64_t thread;
    xregexsplitter* splitter;
    xvalue* input;
    xstrview text;
    xregexsplitpart part;
    bool pending, closed;
} XL6_RegexSplitIterator;
typedef struct XL6_RegexSplitPart {
    xvalue* input;
    xstrview text;
    size_t begin, end, capture;
    bool matched;
} XL6_RegexSplitPart;
static XL6_RegexSplitIterator* check(void* handle)
{
    XL6_RegexSplitIterator* it=handle;
    if (!it) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    if (it->thread!=xrtThreadCurrentId()) {xrtSetErrorKind(XERR_STATE);return NULL;}
    return it;
}
static void closeInput(XL6_RegexSplitIterator* it)
{
    it->closed=true; it->pending=false;
    xrtRegexSplitterFree(it->splitter); it->splitter=NULL;
    xrtValueRelease(it->input); it->input=NULL; it->text=(xstrview){NULL,0};
}
void xl6RegexSplitDestroyV1(void* handle)
{ XL6_RegexSplitIterator* it=handle; if(it){closeInput(it);xrtFree(it);} }
void* xl6RegexSplitCreateV1(void* regex,xstrview text,int64_t limit,int64_t flags,int64_t size)
{
    xregexsplitconfig config; xrtRegexSplitConfigInit(&config);
    if(!xl6RegexInput(regex,&text,0,size)||!xl6RegexLimit(limit,&config.Limit)) return NULL;
    if(flags<0||flags>3){xrtSetErrorKind(XERR_RANGE);return NULL;}
    config.Flags=(uint32)flags;
    XL6_RegexSplitIterator* it=xrtCalloc(1,sizeof(*it)); if(!it)return NULL;
    it->thread=xrtThreadCurrentId(); it->input=xrtValueString(text);
    if(!it->input||!xrtValueGetString(it->input,&it->text))goto fail;
    it->splitter=xrtRegexSplitterCreate(regex,it->text,&config);
    if(!it->splitter)goto fail;
    return it;
fail: xl6RegexSplitDestroyV1(it);return NULL;
}
bool xl6RegexSplitHasNextV1(void* handle)
{
    XL6_RegexSplitIterator* it=check(handle);if(!it||it->closed)return false;
    if(it->pending)return true;
    xregexresult r=xrtRegexSplitterNext(it->splitter,&it->part);
    if(r!=XREGEX_MATCH){closeInput(it);return false;}
    it->pending=true;return true;
}
void xl6RegexSplitPartDestroyV1(void* handle)
{ XL6_RegexSplitPart* part=handle;if(part){xrtValueRelease(part->input);xrtFree(part);} }
void* xl6RegexSplitSnapshotV1(void* handle)
{
    XL6_RegexSplitIterator* it=check(handle);if(!it)return NULL;
    if(!it->pending||it->closed){xrtSetErrorKind(XERR_STATE);return NULL;}
    XL6_RegexSplitPart* part=xrtCalloc(1,sizeof(*part));if(!part)return NULL;
    part->input=xrtValueRetain(it->input);
    if(!part->input){xrtFree(part);return NULL;}
    part->text=it->text;part->capture=it->part.Capture;part->matched=it->part.Matched;
    if(part->matched){
        part->begin=(size_t)(it->part.Text.Data-it->text.Data);
        part->end=part->begin+it->part.Text.Size;
    }
    return part;
}
void xl6RegexSplitConsumeV1(void* handle)
{
    XL6_RegexSplitIterator* it=check(handle);if(!it)return;
    if(!it->pending||it->closed){xrtSetErrorKind(XERR_STATE);return;}
    it->pending=false;
}
void xl6RegexSplitCloseV1(void* handle)
{ XL6_RegexSplitIterator* it=check(handle);if(it)closeInput(it); }
bool xl6RegexSplitClosedV1(void* handle)
{ XL6_RegexSplitIterator* it=check(handle);return it?it->closed:false; }
static XL6_RegexSplitPart* partCheck(void* handle)
{ if(!handle)xrtSetErrorKind(XERR_ARGUMENT);return handle; }
xstrview xl6RegexSplitPartTextV1(void* handle)
{
    XL6_RegexSplitPart* p=partCheck(handle);
    if (!p||!p->matched) return xl6TextFailure;
    xstrview text={p->text.Data+p->begin,p->end-p->begin};
    return xl6TextSelect(&text,-1)?xl6TextCopy(text):xl6TextFailure;
}
bool xl6RegexSplitPartMatchedV1(void* handle)
{ XL6_RegexSplitPart* p=partCheck(handle);return p?p->matched:false; }
int64_t xl6RegexSplitPartCaptureV1(void* handle)
{ XL6_RegexSplitPart* p=partCheck(handle);return p&&p->capture!=XRT_NPOS?(int64_t)p->capture:-1; }
int64_t xl6RegexSplitPartStartV1(void* handle)
{ XL6_RegexSplitPart* p=partCheck(handle);return p&&p->matched?(int64_t)p->begin:-1; }
int64_t xl6RegexSplitPartEndV1(void* handle)
{ XL6_RegexSplitPart* p=partCheck(handle);return p&&p->matched?(int64_t)p->end:-1; }
