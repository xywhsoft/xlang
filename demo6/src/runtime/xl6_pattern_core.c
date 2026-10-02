#include "src/runtime/xl6_pattern_internal.h"

void xl6PatternDestroyV1(void* handle)
{xrtPatternRelease(handle);}
static void* compile(xstrview text,const xpatternconfig* config,int64_t size)
{
    if (!xl6TextSelect(&text,size)) return NULL;
    xpatternspec spec={text,NULL,0,0};
    return xrtPatternCompileManyConfig(&spec,1,config);
}
void* xl6PatternCreateV1(xstrview text,int64_t size)
{xpatternconfig config;xrtPatternConfigInit(&config);return compile(text,&config,size);}
void* xl6PatternCreateOptionsV1(void* handle,xstrview text,int64_t size)
{
    if (!handle) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    return compile(text,&((XL6_PatternOptions*)handle)->config,size);
}
int64_t xl6PatternCountV1(void* handle)
{if (!handle) {xrtSetErrorKind(XERR_ARGUMENT);return 0;}return (int64_t)xrtPatternCount(handle);}
int64_t xl6PatternCompiledBytesV1(void* handle)
{
    if (!handle) {xrtSetErrorKind(XERR_ARGUMENT);return 0;}
    size_t bytes=xrtPatternCompiledBytes((xpattern*)handle);
    if (bytes>INT64_MAX) {xrtSetErrorKind(XERR_RANGE);return 0;}
    return (int64_t)bytes;
}
xstrview xl6PatternSeparatorsV1(void* handle)
{
    if (!handle) {xrtSetErrorKind(XERR_ARGUMENT);return xl6TextFailure;}
    return xl6TextCopy(xrtPatternSeparators((xpattern*)handle));
}
xstrview xl6PatternSourceV1(void* handle,int64_t index)
{
    if (!xl6PatternIndex(handle,index)) return xl6TextFailure;
    return xl6TextCopy(xrtPatternSource((xpattern*)handle,(size_t)index));
}
uint64_t xl6PatternIdV1(void* handle,int64_t index)
{return xl6PatternIndex(handle,index)?xrtPatternId((xpattern*)handle,(size_t)index):0;}
int64_t xl6PatternCaptureCountV1(void* handle,int64_t index)
{return xl6PatternIndex(handle,index)?(int64_t)xrtPatternCaptureCount((xpattern*)handle,(size_t)index):0;}
xstrview xl6PatternCaptureNameV1(void* handle,int64_t capture,int64_t index)
{
    if (!xl6PatternIndex(handle,index)) return xl6TextFailure;
    const xpattern* program=(xpattern*)handle;
    if (capture<0 || (uint64_t)capture>=xrtPatternCaptureCount(program,(size_t)index)) {
        xrtSetErrorKind(XERR_RANGE);return xl6TextFailure;
    }
    xstrview name;
    return xrtPatternCaptureName(program,(size_t)index,(size_t)capture,&name)?xl6TextCopy(name):xl6TextFailure;
}
int64_t xl6PatternCaptureIndexV1(void* handle,xstrview name,int64_t index,int64_t size)
{
    if (!xl6PatternIndex(handle,index) || !xl6TextSelect(&name,size)) return -1;
    size_t capture=xrtPatternCaptureIndex((xpattern*)handle,(size_t)index,name);
    return capture==XRT_NPOS?-1:(int64_t)capture;
}
int64_t xl6PatternFirstV1(void* handle,xstrview text,int64_t size)
{
    if (!xl6PatternInput(handle,&text,size)) return -1;
    xpatternmatch match;
    return xrtPatternLookup((xpattern*)handle,text,&match)==XPATTERN_MATCH?
        (int64_t)match.PatternIndex:-1;
}
bool xl6PatternTestV1(void* handle,xstrview text,int64_t size)
{
    if (!xl6PatternInput(handle,&text,size)) return false;
    return xrtPatternTest((xpattern*)handle,text)==XPATTERN_MATCH;
}
int64_t xl6PatternErrorOffsetV1(const xerror* error)
{
    for (;error;error=xrtErrorCause(error)) {size_t result;
        if (xrtPatternErrorOffset(error,&result) && result<=INT64_MAX) return (int64_t)result;}
    return -1;
}
int64_t xl6PatternErrorIndexV1(const xerror* error)
{
    for (;error;error=xrtErrorCause(error)) {size_t result;
        if (xrtPatternErrorPattern(error,&result) && result<=INT64_MAX) return (int64_t)result;}
    return -1;
}
