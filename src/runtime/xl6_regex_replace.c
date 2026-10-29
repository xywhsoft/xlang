#include "src/runtime/xl6_regex_internal.h"
xstrview xl6RegexReplaceV1(void* regex, xstrview text, xstrview replacement,
    int64_t limit, int64_t size, int64_t replacementSize)
{
    size_t count;
    if (!xl6RegexInput(regex,&text,0,size) || !xl6TextSelect(&replacement,replacementSize) ||
        !xl6RegexLimit(limit,&count)) return xl6TextFailure;
    xstrbuf output;
    xrtStrBufInit(&output);
    if (!xrtRegexReplaceTo(regex,text,replacement,count,&output,NULL)) {
        xrtStrBufFree(&output); return xl6TextFailure;
    }
    xstrview resultText={output.Data,output.Size};
    if (!xl6TextSelect(&resultText,-1)) {xrtStrBufFree(&output);return xl6TextFailure;}
    size_t length=output.Size;
    char* result=xrtStrBufTake(&output);
    xrtStrBufFree(&output); return xl6TextOwn(result,length);
}
