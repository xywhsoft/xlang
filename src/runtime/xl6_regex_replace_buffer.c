#include "src/runtime/xl6_regex_internal.h"
int64_t xl6RegexReplaceBufferV1(void* regex, void* target, xstrview text, xstrview replacement,
    int64_t limit, int64_t size, int64_t replacementSize)
{
    size_t count, replaced=0;
    if (!target) {xrtSetErrorKind(XERR_ARGUMENT);return 0;}
    if (!xl6RegexInput(regex,&text,0,size) || !xl6TextSelect(&replacement,replacementSize) ||
        !xl6RegexLimit(limit,&count)) return 0;
    /* Independent staging: input/replacement may alias target. Append is the
     * only commit, and failure preserves every original logical byte. */
    xstrbuf output; xrtStrBufInit(&output);
    bool ok=xrtRegexReplaceTo(regex,text,replacement,count,&output,&replaced);
    if (ok && replaced>INT64_MAX) {xrtSetErrorKind(XERR_RANGE);ok=false;}
    if (ok) ok=xrtBufferAppend(target,(xbytesview){(const uint8*)output.Data,output.Size});
    xrtStrBufFree(&output);
    return ok ? (int64_t)replaced:0;
}
