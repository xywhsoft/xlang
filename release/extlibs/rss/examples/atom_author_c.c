/* Atom 作者按 entry、source、feed 的次序选择。 */
#define XRT_MODULE_ALL
#include "rss_native.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *xml = "<feed xmlns='http://www.w3.org/2005/Atom'>"
        "<author><name>Editor</name></author>"
        "<entry><title>One</title></entry>"
        "<entry><source><author><name>Source</name></author></source>"
        "<title>Two</title></entry></feed>";
    xvalue *feed = xrss_parse_atom(xml, (int64)strlen(xml));
    xvalue *items;
    size_t i;
    if (feed == NULL || xrtValueType(feed) != XVALUE_OBJECT) {
        xrtValueRelease(feed);
        return 1;
    }
    items = xrtValueObjectGet(feed, (xstrview){"items", 5});
    if (items == NULL || xrtValueType(items) != XVALUE_ARRAY || xrtValueCount(items) != 2) {
        xrtValueRelease(feed);
        return 1;
    }
    for (i = 0; i < xrtValueCount(items); ++i) {
        xvalue *entry = xrtValueArrayAt(items, i);
        xvalue *author = xrtValueObjectGet(entry, (xstrview){"author", 6});
        xstrview text;
        if (author == NULL || !xrtValueGetString(author, &text)) {
            xrtValueRelease(feed);
            return 1;
        }
        printf("%.*s\n", (int)text.Size, text.Data);
    }
    xrtValueRelease(feed);
    return 0;
}
