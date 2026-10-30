/* 与 atom_links.xl 相同的输入、断言和输出；结果随 feed 一起释放。 */
#define XRT_MODULE_ALL
#include "../include/rss_native.h"
#include <stdio.h>
#include <string.h>

static bool print_field(xvalue *object, const char *key, const char *expected) {
    xstrview text;
    xvalue *value = xrtValueObjectGet(object, (xstrview){key, strlen(key)});
    if (!value || !xrtValueGetString(value, &text) || text.Size != strlen(expected) ||
        memcmp(text.Data, expected, text.Size)) return false;
    return printf("%.*s\n", (int)text.Size, text.Data) >= 0;
}

int main(void) {
    const char xml[] =
        "<a:feed xmlns:a='http://www.w3.org/2005/Atom'>"
        "<a:title>News &amp; Notes</a:title>"
        "<a:link rel='self' href='https://example.test/feed'/>"
        "<a:link rel='http://www.iana.org/assignments/relation/alternate' href='https://example.test/'/>"
        "<a:entry><a:link rel='self' href='https://example.test/feed/1'/>"
        "<a:link rel='http://www.iana.org/assignments/relation/alternate' href='https://example.test/posts/1'/>"
        "</a:entry></a:feed>";
    xvalue *feed = xrss_parse_atom(xml, (int64)(sizeof(xml) - 1));
    xvalue *items, *entry;
    bool ok = false;
    if (!feed || xrtValueType(feed) != XVALUE_OBJECT) goto done;
    items = xrtValueObjectGet(feed, (xstrview){"items", 5});
    if (!items || xrtValueCount(items) != 1) goto done;
    entry = xrtValueArrayAt(items, 0);
    ok = print_field(feed, "title", "News & Notes") &&
        print_field(feed, "link", "https://example.test/") &&
        print_field(entry, "link", "https://example.test/posts/1");
done:
    xrtValueRelease(feed);
    return ok ? 0 : 1;
}
