#define XRT_MODULE_BUFFER
#define XRT_MODULE_UNICODE
#include "../../extlibs/srt/include/srt_native.h"
#include "../../extlibs/srt/srt_native.c"
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *source = "1\n00:00:01,000 --> 00:00:03,500\n你好，世界\n\n";
    const char *web_source = "WEBVTT\n\nintro\n00:00.500 --> 00:01.000 align:start\nHello\n\n";
    xvalue *cues = xsrt_parse(source);
    xvalue *web_cues = xsrt_parse_vtt(web_source);
    char *srt = NULL, *vtt = NULL, *web_vtt = NULL, *stamp = NULL;
    int ok = 0;
    if (cues == NULL || web_cues == NULL ||
        xrtValueType(cues) != XVALUE_ARRAY ||
        xrtValueType(web_cues) != XVALUE_ARRAY) goto done;
    srt = (char *)xsrt_build(cues);
    vtt = (char *)xsrt_build_vtt(cues);
    web_vtt = (char *)xsrt_build_vtt(web_cues);
    if (xsrt_parse_time("00:00:03.500") != 3500) goto done;
    stamp = (char *)xsrt_format_time(3500);
    if (srt == NULL || vtt == NULL || web_vtt == NULL || stamp == NULL ||
        strcmp(srt, source) != 0 ||
        strcmp(vtt, "WEBVTT\n\n00:00:01.000 --> 00:00:03.500\n你好，世界\n\n") != 0 ||
        strcmp(web_vtt, "WEBVTT\n\nintro\n00:00:00.500 --> 00:00:01.000 align:start\nHello\n\n") != 0 ||
        strcmp(stamp, "00:00:03,500") != 0) goto done;
    printf("%s%s%s%s\n", srt, vtt, web_vtt, stamp);
    ok = 1;
done:
    xrtFree(stamp);
    xrtFree(web_vtt);
    xrtFree(vtt);
    xrtFree(srt);
    xrtValueRelease(web_cues);
    xrtValueRelease(cues);
    return ok ? 0 : 1;
}
