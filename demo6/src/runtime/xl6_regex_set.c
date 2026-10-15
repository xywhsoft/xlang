#include "src/runtime/xl6_regex_internal.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
#include "src/runtime/xl6_array_input.h"
#include "src/runtime/xl6_array_producer.h"
void xl6RegexSetDestroyV1(void* handle)
{ xrtRegexSetRelease(handle); }
int64_t xl6RegexErrorIndexV1(const xerror* error)
{
    for (;error;error=xrtErrorCause(error)) {
        size_t index;
        if (xrtRegexSetErrorIndex(error,&index) && index<=INT64_MAX) return (int64_t)index;
    }
    return -1;
}
void* xl6RegexSetCreateV1(const xvalue* patterns,int64_t flags,int64_t maxPatternBytes,
    int64_t maxCaptures,int64_t maxPatterns,int64_t itemSize)
{
    xregexconfig config;
    if(!xl6RegexConfig(flags,maxPatternBytes,maxCaptures,&config))return NULL;
    if(maxPatterns<=0||itemSize < -1){xrtSetErrorKind(XERR_RANGE);return NULL;}
    XL6_ArrayInputV1 input={0};
    if (!xl6ArrayInputAcquireV1(patterns,&input)) return NULL;
    size_t count=input.count;
    if(count>(uint64_t)maxPatterns||count>SIZE_MAX/sizeof(xstrview)){
        xrtSetErrorKind(XERR_RANGE);goto fail;
    }
    xstrview* views=count?xrtMalloc(count*sizeof(*views)):NULL;
    if(count&&!views)goto fail;
    for(size_t i=0;i<count;++i){
        const xvalue* v=input.items[i];
        if(!v||!xrtValueGetString(v,&views[i])||!xl6TextSelect(&views[i],itemSize)){
            xrtFree(views);goto fail;
        }
    }
    xregexset* set=xrtRegexSetCompileConfig(views,count,&config);
    xrtFree(views);xl6ArrayInputReleaseV1(&input);return set;
fail:
    xl6ArrayInputReleaseV1(&input);return NULL;
}
int64_t xl6RegexSetCountV1(void* handle)
{ if(!handle){xrtSetErrorKind(XERR_ARGUMENT);return 0;}return (int64_t)xrtRegexSetCount(handle); }
xstrview xl6RegexSetPatternV1(void* handle,int64_t index)
{
    if(!handle){xrtSetErrorKind(XERR_ARGUMENT);return xl6TextFailure;}
    if(index<0||(uint64_t)index>=xrtRegexSetCount(handle)){
        xrtSetErrorKind(XERR_RANGE);return xl6TextFailure;
    }
    const xregex* regex=xrtRegexSetRegex(handle,(size_t)index);
    return regex?xl6TextCopy(xrtRegexPattern(regex)):xl6TextFailure;
}
static xregexsetmatcher* run(void* handle,xstrview text,int64_t start,int64_t size)
{
    if(!xl6RegexInput(handle,&text,start,size))return NULL;
    xregexsetmatcher* matcher=xrtRegexSetMatcherCreate(handle);if(!matcher)return NULL;
    if(xrtRegexSetMatcherMatch(matcher,text,(size_t)start)==XREGEX_ERROR){
        xrtRegexSetMatcherFree(matcher);return NULL;
    }
    return matcher;
}
xvalue* xl6RegexSetHitsV2(uint64_t identity,uint64_t element,void* handle,xstrview text,int64_t start,int64_t size)
{
    xregexsetmatcher* matcher=run(handle,text,start,size);if(!matcher)return NULL;
    XL6_ArrayProducerV1 result;
    if(!xl6ArrayProducerBeginV1(&result,identity,element,XL6_VALUE_ADMIT_I64,xrtTypeInt64(),NULL))goto fail;
    size_t count=xrtRegexSetMatcherCount(matcher);
    if(!xl6ArrayProducerReserveV1(&result,count))goto fail;
    for(size_t i=0;i<count;++i){
        size_t index=xrtRegexSetMatcherIndex(matcher,i);
        if(index==XRT_NPOS||index>INT64_MAX){xrtSetErrorKind(XERR_RANGE);goto fail;}
        int64_t item=(int64_t)index;
        if(!xl6ArrayProducerPushV1(&result,&item))goto fail;
    }
    xrtRegexSetMatcherFree(matcher);return xl6ArrayProducerFinishV1(&result,true);
fail: xrtRegexSetMatcherFree(matcher);return xl6ArrayProducerFinishV1(&result,false);
}
int64_t xl6RegexSetFirstV1(void* handle,xstrview text,int64_t start,int64_t size)
{
    xregexsetmatcher* matcher=run(handle,text,start,size);if(!matcher)return -1;
    size_t index=xrtRegexSetMatcherFirst(matcher);xrtRegexSetMatcherFree(matcher);
    return index==XRT_NPOS?-1:(int64_t)index;
}
bool xl6RegexSetTestV1(void* handle,xstrview text,int64_t start,int64_t size)
{ return xl6RegexSetFirstV1(handle,text,start,size)>=0; }

/* Resident policy: terminal Drop runs after the phased handle is detached. */
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6RegexSetHandlePolicyV1Get,xl6RegexSetDestroyV1)
