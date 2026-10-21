#include "../bson_native.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "BSON Python interop failed at %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

static xvalue *field(xvalue *object, const char *name) {
    return xrtValueObjectGet(object, (xstrview){name, strlen(name)});
}

int main(int argc, char **argv) {
    xvalue *doc, *nested, *items, *item;
    xstrview text;
    xbytesview raw;
    int64 count;
    xtime when;
    bool enabled;
    CHECK(argc == 2);
    doc = xbson_load(argv[1]);
    CHECK(doc != NULL && xrtValueType(doc) == XVALUE_OBJECT);
    item = field(doc, "name");
    CHECK(item && xrtValueGetString(item, &text) &&
        text.Size == strlen("Python 你好") &&
        memcmp(text.Data, "Python 你好", text.Size) == 0);
    item = field(doc, "count");
    CHECK(item && xrtValueGetInt(item, &count) && count == 1099511627776LL);
    nested = field(doc, "nested");
    item = nested ? field(nested, "ok") : NULL;
    CHECK(item && xrtValueGetBool(item, &enabled) && enabled);
    items = field(doc, "items");
    CHECK(items && xrtValueCount(items) == 3);
    item = xrtValueArrayAt(items, 2);
    CHECK(item && xrtValueGetInt(item, &count) && count == 3);
    item = field(doc, "raw");
    CHECK(item && xrtValueGetBytes(item, &raw) && raw.Size == 2 &&
        raw.Data[0] == 0 && raw.Data[1] == 255);
    item = field(doc, "when");
    CHECK(item && xrtValueGetTime(item, &when) && when == -1000);
    xrtValueRelease(doc);
    puts("PyMongo to C BSON: PASS");
    return 0;
}
