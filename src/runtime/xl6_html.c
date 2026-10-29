#include "src/runtime/xl6_text_internal.h"

static xstrview xl6HtmlEscape(xstrview text, bool attribute, int64_t size)
{
    if (!xl6TextSelectRaw(&text, size)) return xl6TextFailure;
    size_t written = 0;
    char* result = xrtHtmlEscape(text,
        attribute ? XHTML_ESCAPE_ATTRIBUTE : XHTML_ESCAPE_TEXT, &written);
    return xl6TextOwn(result, written);
}
xstrview xl6HtmlEscapeTextV1(xstrview text, int64_t size)
{ return xl6HtmlEscape(text, false, size); }
xstrview xl6HtmlEscapeAttributeV1(xstrview text, int64_t size)
{ return xl6HtmlEscape(text, true, size); }
