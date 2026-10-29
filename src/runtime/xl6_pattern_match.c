#include "src/runtime/xl6_pattern_internal.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"

typedef struct XL6_PatternSnapshot {
    xpattern* program;
    char* input;
    xstrview text;
    xpatternmatch match;
    bool matched;
    xstrview captures[];
} XL6_PatternSnapshot;
void xl6PatternMatchDestroyV1(void* handle)
{
    XL6_PatternSnapshot* result=handle;
    if (result) {
        xrtPatternRelease(result->program);xrtFree(result->input);xrtFree(result);
    }
}
void* xl6PatternMatchCreateV1(void* handle,xstrview text,int64_t size)
{
    xpattern* source=handle;
    if (!xl6PatternInput(handle,&text,size)) return NULL;
    size_t capacity=xrtPatternMaxCaptureCount(source);
    if (capacity>(SIZE_MAX-sizeof(XL6_PatternSnapshot))/sizeof(xstrview)) {
        xrtSetErrorKind(XERR_RANGE);return NULL;
    }
    XL6_PatternSnapshot* result=xrtCalloc(1,sizeof(*result)+capacity*sizeof(xstrview));
    if (!result) return NULL;
    result->program=xrtPatternRef(source);if (!result->program) goto fail;
    result->input=xrtStrDupView(text);
    if (!result->input) goto fail;
    result->text=(xstrview){result->input,text.Size};
    xpatternresult status=xrtPatternMatch(result->program,result->text,result->captures,capacity,&result->match);
    if (status==XPATTERN_ERROR) goto fail;
    result->matched=status==XPATTERN_MATCH;
    return result;
fail: xl6PatternMatchDestroyV1(result);return NULL;
}
static bool check(const XL6_PatternSnapshot* result)
{
    if (!result) {xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if (!result->matched) {xrtSetErrorKind(XERR_STATE);return false;}
    return true;
}
static bool capture(const XL6_PatternSnapshot* result,int64_t index)
{
    if (!check(result)) return false;
    if (index<0 || (uint64_t)index>=result->match.CaptureCount) {xrtSetErrorKind(XERR_RANGE);return false;}
    return true;
}
bool xl6PatternMatchMatchedV1(void* handle)
{if (!handle) {xrtSetErrorKind(XERR_ARGUMENT);return false;}return ((XL6_PatternSnapshot*)handle)->matched;}
int64_t xl6PatternMatchCountV1(void* handle)
{return check(handle)?(int64_t)((XL6_PatternSnapshot*)handle)->match.CaptureCount:0;}
int64_t xl6PatternMatchPatternIndexV1(void* handle)
{return check(handle)?(int64_t)((XL6_PatternSnapshot*)handle)->match.PatternIndex:-1;}
uint64_t xl6PatternMatchIdV1(void* handle)
{return check(handle)?((XL6_PatternSnapshot*)handle)->match.Id:0;}
xstrview xl6PatternMatchInputV1(void* handle)
{
    if (!handle) {xrtSetErrorKind(XERR_ARGUMENT);return xl6TextFailure;}
    return xl6TextCopy(((XL6_PatternSnapshot*)handle)->text);
}
xstrview xl6PatternMatchSourceV1(void* handle)
{
    XL6_PatternSnapshot* result=handle;
    return check(result)?xl6TextCopy(xrtPatternSource(result->program,result->match.PatternIndex)):xl6TextFailure;
}
xstrview xl6PatternMatchTextV1(void* handle,int64_t index)
{
    XL6_PatternSnapshot* result=handle;
    if (!capture(result,index)) return xl6TextFailure;
    xstrview text=result->captures[index];
    return xl6TextSelect(&text,-1)?xl6TextCopy(text):xl6TextFailure;
}
int64_t xl6PatternMatchStartV1(void* handle,int64_t index)
{
    XL6_PatternSnapshot* result=handle;
    if (!capture(result,index)) return -1;
    return (int64_t)(result->captures[index].Data-result->text.Data);
}
int64_t xl6PatternMatchEndV1(void* handle,int64_t index)
{
    XL6_PatternSnapshot* result=handle;
    return capture(result,index)?(int64_t)(result->captures[index].Data-result->text.Data+result->captures[index].Size):-1;
}
xstrview xl6PatternMatchNameV1(void* handle,int64_t index)
{
    XL6_PatternSnapshot* result=handle;xstrview name;
    if (!capture(result,index)) return xl6TextFailure;
    return xrtPatternCaptureName(result->program,result->match.PatternIndex,(size_t)index,&name)?xl6TextCopy(name):xl6TextFailure;
}
int64_t xl6PatternMatchIndexV1(void* handle,xstrview name,int64_t size)
{
    XL6_PatternSnapshot* result=handle;
    if (!check(result) || !xl6TextSelect(&name,size)) return -1;
    size_t index=xrtPatternCaptureIndex(result->program,result->match.PatternIndex,name);
    return index==XRT_NPOS?-1:(int64_t)index;
}

/* Resident policy: terminal Drop runs after the phased handle is detached. */
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6PatternMatchHandlePolicyV1Get,xl6PatternMatchDestroyV1)
