#define XRT_MODULE_ALL
#include "xrt.h"
#include "../msgpack_native.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "MessagePack Python interop failed at %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

static xvalue *field(xvalue *object, const char *name) {
    return xrtValueObjectGet(object, (xstrview){name, strlen(name)});
}

int main(int argc, char **argv) {
    xvalue *doc, *nested, *items, *item;
    xbytesview bytes;
    xstrview text;
    uint8 *wire;
    size_t size;
    int64 count;
    uint64 maximum;
    xtime moment;
    bool enabled;
    CHECK(argc == 2);
    wire = xrtFileReadAll(argv[1], &size);
    CHECK(wire != NULL);
    doc = xmsgpack_decode(wire, size);
    xrtFree(wire);
    CHECK(doc != NULL && xrtValueType(doc) == XVALUE_OBJECT);
    item = field(doc, "name");
    CHECK(item && xrtValueGetString(item, &text) &&
        text.Size == strlen("Python 你好") &&
        memcmp(text.Data, "Python 你好", text.Size) == 0);
    item = field(doc, "count");
    CHECK(item && xrtValueGetInt(item, &count) && count == 1099511627776LL);
    item = field(doc, "maximum");
    CHECK(item && xrtValueGetUInt(item, &maximum) && maximum == UINT64_MAX);
    nested = field(doc, "nested");
    item = nested ? field(nested, "ok") : NULL;
    CHECK(item && xrtValueGetBool(item, &enabled) && enabled);
    items = field(doc, "items");
    CHECK(items && xrtValueCount(items) == 3);
    item = xrtValueArrayAt(items, 2);
    CHECK(item && xrtValueGetInt(item, &count) && count == 3);
    item = field(doc, "raw");
    CHECK(item && xrtValueGetBytes(item, &bytes) && bytes.Size == 2 &&
        bytes.Data[0] == 0 && bytes.Data[1] == 255);
    item = field(doc, "when");
    CHECK(item && xrtValueGetTime(item, &moment) && moment == XRT_TIME_UNIX_EPOCH - 500);
    xrtValueRelease(doc);
    puts("Python to C MessagePack: PASS");
    return 0;
}
