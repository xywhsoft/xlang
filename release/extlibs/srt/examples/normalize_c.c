#define XRT_MODULE_BUFFER
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_UNICODE
#include "../srt_native.c"

int main(void)
{
    const char* source="7\r\n00:00:00,500 --> 00:00:01,750\r\nHello\r\n\r\n";
    const char* plain="1\n00:00:00,500 --> 00:00:01,750\nHello\n\n";
    const char* web="WEBVTT\n\n00:00:00.500 --> 00:00:01.750\nHello\n\n";
    xvalue *cues=xsrt_parse(source), *parsed=NULL, *roundtrip=NULL;
    char *normalized=NULL, *converted=NULL, *again=NULL, *start=NULL, *end=NULL;
    int64 index=0;
    int result=1;
    if (!cues || xrtGetError()) goto done;
    normalized=xsrt_build(cues); converted=xsrt_build_vtt(cues);
    if (!normalized || !converted || strcmp(normalized,plain) || strcmp(converted,web)) goto done;
    parsed=xsrt_parse(normalized); roundtrip=xsrt_parse_vtt(converted);
    if (!parsed || !roundtrip || xrtGetError()) goto done;
    again=xsrt_build(roundtrip);
    if (!again || strcmp(again,plain) || !xrtValueGetInt(xrtValueObjectGet(xrtValueArrayAt(parsed,0),XRT_STR_LITERAL("index")),&index)) goto done;
    start=xsrt_format_time(xsrt_parse_time("00:00.500"));
    end=xsrt_format_time(xsrt_parse_time("00:01.750"));
    if (!start || !end || xrtGetError()) goto done;
    printf("cues=%llu\nfirstIndex=%lld\nstart=%s\nend=%s\nvtt=true\nroundtrip=true\n",
        (unsigned long long)xrtValueCount(cues),(long long)index,start,end);
    result=ferror(stdout) ? 1 : 0;
done:
    // 输出独立拥有，输入可以按调用方自己的生命周期释放。
    xrtFree(end); xrtFree(start); xrtFree(again); xrtFree(converted); xrtFree(normalized);
    xrtValueRelease(roundtrip); xrtValueRelease(parsed); xrtValueRelease(cues);
    return result;
}
