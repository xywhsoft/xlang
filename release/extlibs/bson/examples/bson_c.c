#include "../bson_native.c"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    xvalue* document = xrtValueObject();
    xvalue* count = xrtValueInt(7);
    xvalue* name = xrtValueString((xstrview){ "xlang", 5 });
    xvalue* encoded = NULL;
    xvalue* restored = NULL;
    xvalue* stream = NULL;
    xvalue* documents = NULL;
    xvalue* loaded = NULL;
    xvalue* field;
    xstrview text;
    int64 value;
    int64 length;
    char* hex = NULL;
    char* oid = NULL;
    int status = 1;

    if (argc != 1 && argc != 2) goto done;
    if (!document || !count || !name ||
        !xrtValueObjectSetTake(document, (xstrview){ "name", 4 }, &name) ||
        !xrtValueObjectSetTake(document, (xstrview){ "count", 5 }, &count)) goto done;
    encoded = xbson_encode(document);
    restored = xbson_decode(encoded);
    if (!restored || xrtValueType(restored) != XVALUE_OBJECT) goto done;
    field = xrtValueObjectGet(restored, (xstrview){ "name", 4 });
    if (!field || !xrtValueGetString(field, &text)) goto done;
    field = xrtValueObjectGet(restored, (xstrview){ "count", 5 });
    if (!field || !xrtValueGetInt(field, &value) || value != 7) goto done;
    length = xbson_doc_length(encoded);
    hex = xbson_to_hex(encoded);
    if (length != 32 || !hex || strlen(hex) != (size_t)length * 2 ||
        strncmp(hex, "20000000", 8) != 0) goto done;
    stream = xbson_concat(encoded, encoded);
    if (!stream || xrtValueType(stream) != XVALUE_BYTES) goto done;
    documents = xbson_decode_all(stream);
    if (!documents || xrtValueType(documents) != XVALUE_ARRAY ||
        xrtValueCount(documents) != 2) goto done;
    field = xrtValueObjectGet(xrtValueArrayAt(documents, 0),
        (xstrview){ "name", 4 });
    if (!field || !xrtValueGetString(field, &text) ||
        text.Size != 5 || memcmp(text.Data, "xlang", 5) != 0) goto done;
    field = xrtValueObjectGet(xrtValueArrayAt(documents, 1),
        (xstrview){ "count", 5 });
    if (!field || !xrtValueGetInt(field, &value) || value != 7 ||
        xbson_oid_time("000000000000000000000000") != 0) goto done;
    oid = xbson_gen_oid();
    if (!oid || strlen(oid) != 24 || xbson_oid_time(oid) <= 0) goto done;
    if (argc == 2) {
        if (xbson_save(argv[1], document) <= 0) goto done;
        loaded = xbson_load(argv[1]);
        if (!loaded || xrtValueType(loaded) != XVALUE_OBJECT) goto done;
        field = xrtValueObjectGet(loaded, (xstrview){ "name", 4 });
        if (!field || !xrtValueGetString(field, &text) ||
            text.Size != 5 || memcmp(text.Data, "xlang", 5) != 0) goto done;
        field = xrtValueObjectGet(loaded, (xstrview){ "count", 5 });
        if (!field || !xrtValueGetInt(field, &value) || value != 7) goto done;
    }
    field = xrtValueObjectGet(restored, (xstrview){ "name", 4 });
    if (!field || !xrtValueGetString(field, &text)) goto done;
    printf("name=%.*s\n", (int)text.Size, text.Data);
    printf("count=7\nbytes=%" PRId64 "\n", length);
    printf("hexPrefix=%.8s\nstream=2\noidEpoch=0\nobjectId=ok\n", hex);
    if (argc == 2) printf("file=ok\n");
    status = 0;
done:
    xrtFree(oid);
    xrtFree(hex);
    xrtValueRelease(loaded);
    xrtValueRelease(documents);
    xrtValueRelease(stream);
    xrtValueRelease(restored);
    xrtValueRelease(encoded);
    xrtValueRelease(name);
    xrtValueRelease(count);
    xrtValueRelease(document);
    return status;
}
