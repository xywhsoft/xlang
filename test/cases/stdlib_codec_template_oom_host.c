#include "src/runtime/xl6_text_value.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void fault(int operation,size_t point)
{
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    xstrview text={"a\0b",3},resultText={0};xvalue* resultValue=NULL;void* resultObject=NULL;
    void* compiled=xl6TemplateCreateV1(XRT_STR_LITERAL("hello"),NULL,-1);assert(compiled);
    xvalue* zipped=xl6DeflateTextV1(text,-1,2,6,0,-1,15);assert(zipped);
    assert(xrtMemDebugFailAfter(point));
    switch (operation) {
        case 0:resultText=xl6CodecHexTextV1(text,-1,0);break;
        case 1:resultValue=xl6CodecBase64DecodeV1(XRT_STR_LITERAL("YQBi"),-1,0,XRT_STR_LITERAL(""),-1);break;
        case 2:resultValue=xl6CodecPercentDecodeV1(XRT_STR_LITERAL("a%00b"),-1,false);break;
        case 3:resultValue=xl6DeflateTextV1(text,-1,2,6,0,-1,15);break;
        case 4:resultValue=xl6InflateBytesV1(zipped,-1,2,-1,65536,15);break;
        case 5:resultObject=xl6TemplateCreateV1(XRT_STR_LITERAL("{#define:'row'}X{#end}{#include:'row'}"),NULL,-1);break;
        case 6:resultText=xl6TemplateRenderV1(compiled,NULL,NULL,NULL);break;
        case 7:resultObject=xl6DeflaterCreateV1(2,6,0,-1,15);break;
        case 8:resultObject=xl6InflaterCreateV1(2,-1,65536,15);break;
        default:abort();
    }
    bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    if (hit && (resultText.Data || resultValue || resultObject)) {
        fprintf(stderr,"operation=%d point=%zu returned success after allocation failure\n",operation,point);abort();
    }
    if (!hit) assert(resultText.Data || resultValue || resultObject);
    xerror* failure=xrtTakeError();
    xrtFree((void*)resultText.Data);xrtValueRelease(resultValue);
    if (operation==7) xl6DeflaterDestroyV1(resultObject);
    else if (operation==8) xl6InflaterDestroyV1(resultObject);
    else xl6TemplateDestroyV1(resultObject);
    xl6TemplateDestroyV1(compiled);xrtValueRelease(zipped);xrtErrorFree(failure);xrtClearError();
    xrtMemDebugSnapshot(&after);
    if (before.LiveCount!=after.LiveCount || before.LiveBytes!=after.LiveBytes) {
        fprintf(stderr,"operation=%d point=%zu leak count=%lld bytes=%lld\n",operation,point,
            (long long)(after.LiveCount-before.LiveCount),(long long)(after.LiveBytes-before.LiveBytes));abort();
    }
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(after.InvalidFreeCount==before.InvalidFreeCount && after.DoubleFreeCount==before.DoubleFreeCount && after.UseAfterFreeCount==before.UseAfterFreeCount);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    void* warm=xl6TemplateCreateV1(XRT_STR_LITERAL("warm"),NULL,-1);assert(warm);
    xstrview output=xl6TemplateRenderV1(warm,NULL,NULL,NULL);assert(output.Data);xrtFree((void*)output.Data);xl6TemplateDestroyV1(warm);
    for (int operation=0;operation<9;operation++) for (size_t point=0;point<48;point++) fault(operation,point);
    puts("432 allocation fault points balanced; no partial values or invalid frees");return 0;
}
