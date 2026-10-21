/* 与 rss.xl 不同名，避免构建范例时自动混编两个 main。 */
#define XRT_MODULE_ALL
#include "rss_native.h"
#include <stdio.h>
#include <string.h>

static int put_text(xvalue *object, const char *name, const char *text) {
    return xrtValueObjectSetNew(object,
        (xstrview){name, strlen(name)},
        xrtValueString((xstrview){text, strlen(text)}));
}

int main(void) {
    xvalue *channel = xrtValueObject();
    xvalue *items = xrtValueArray();
    xvalue *item = xrtValueObject();
    xvalue *feed;
    char *xml = NULL;
    int ok = 0;
    if (channel == NULL || items == NULL || item == NULL) goto done;
    if (!put_text(channel, "title", "xlang release notes") ||
        !put_text(channel, "link", "https://xlang.dev") ||
        !put_text(channel, "description", "compiler and ecosystem updates") ||
        !put_text(item, "title", "new package") ||
        !put_text(item, "description", "C & XLang")) goto done;
    {
        int appended = xrtValueArrayAppendNew(items, item);
        item = NULL; /* AppendNew 无论成功失败都消费 item。 */
        if (!appended) goto done;
    }
    xml = xrss_build_rss(channel, items);
    if (xml == NULL || *xml == 0) goto done;
    feed = xrss_parse(xml, (int64)strlen(xml));
    if (feed == NULL || xrtValueType(feed) != XVALUE_OBJECT) {
        xrtValueRelease(feed);
        goto done;
    }
    {
        xvalue *parsed_items = xrtValueObjectGet(feed, (xstrview){"items", 5});
        xvalue *first;
        xvalue *title;
        xstrview title_text;
        if (parsed_items == NULL || xrtValueType(parsed_items) != XVALUE_ARRAY ||
            xrtValueCount(parsed_items) != 1) {
            xrtValueRelease(feed);
            goto done;
        }
        first = xrtValueArrayAt(parsed_items, 0);
        if (first == NULL || xrtValueType(first) != XVALUE_OBJECT) {
            xrtValueRelease(feed);
            goto done;
        }
        title = xrtValueObjectGet(first, (xstrview){"title", 5});
        if (title == NULL ||
            !xrtValueGetString(title, &title_text) ||
            title_text.Size != sizeof("new package") - 1 ||
            memcmp(title_text.Data, "new package", title_text.Size) != 0 ||
            strcmp(xrss_detect(xml, (int64)strlen(xml)), "rss") != 0) {
            xrtValueRelease(feed);
            goto done;
        }
    }
    printf("type: %s\n", xrss_detect(xml, (int64)strlen(xml)));
    printf("items: %lld\n", (long long)xrtValueCount(
        xrtValueObjectGet(feed, (xstrview){"items", 5})));
    xrtValueRelease(feed);
    ok = 1;
done:
    xrtFree(xml);
    xrtValueRelease(item);
    xrtValueRelease(items);
    xrtValueRelease(channel);
    return ok ? 0 : 1;
}
