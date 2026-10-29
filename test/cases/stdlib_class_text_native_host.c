#include "src/runtime/xl6_class_text.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const xstrview fields[] = {{"text", 4}, {"zero", 4}, {"unicode", 7}, {"large", 5}};
#define CLASS_ID UINT64_C(0xC13A550045F19E42)
static const XL6_ClassTextSchemaV1 schema = {{"ExactTextRecord", 15}, 71, CLASS_ID, fields, 4};
static const XL6_ClassTextSchemaV1 emptySchema = {{"Empty", 5}, 71, CLASS_ID, NULL, 0};
static const char expected[] = "ExactTextRecord{text=a\0\xE4\xBD\xA0, zero=\0, unicode=\xE4\xBD\xA0, large=18446744073709551615}";
static unsigned foreignDrops;
static void foreignDrop(void* payload, void* user)
{ assert(payload == (void*)(uintptr_t)1); (void)user; ++foreignDrops; }
static const xvaluehandleops foreignOps = {NULL, foreignDrop, NULL, NULL};
static xvalue* seed(void)
{
    xvalue* value = xrtValueObject(); assert(value);
    assert(xrtValueTypeIdBind(value, CLASS_ID));
    assert(xrtValueObjectSetNew(value, xrtStrView("$xlir.class.runtime-type"), xrtValueInt(71)));
    assert(xrtValueObjectSetNew(value, fields[0], xrtValueString((xstrview){"a\0\xE4\xBD\xA0", 5})));
    assert(xrtValueObjectSetNew(value, fields[1], xrtValueChar(0)));
    assert(xrtValueObjectSetNew(value, fields[2], xrtValueChar(0x4F60)));
    assert(xrtValueObjectSetNew(value, fields[3], xrtValueUInt(UINT64_MAX)));
    return value;
}
static void expect(const xvalue* value, const XL6_ClassTextSchemaV1* selected, const char* data, size_t size)
{
    xstrview text = xl6ClassValueTextV1(value, selected);
    assert(text.Data && text.Size == size && text.Data[size] == 0 && !memcmp(text.Data, data, size));
    xrtFree((void*)text.Data);
}
static void bad(const xvalue* value, const XL6_ClassTextSchemaV1* selected, xerrkind kind)
{
    xstrview text = xl6ClassValueTextV1(value, selected);
    assert(!text.Data && !text.Size && xrtErrorKind(xrtGetError()) == kind);
    xrtClearError();
}
static void vectors(void)
{
    for (unsigned cycle = 0; cycle < 100; ++cycle) {
        xl6ProductMemoryBegin(SIZE_MAX);
        xvalue* value = seed();
        expect(value, &schema, expected, sizeof(expected)-1);
        expect(value, &emptySchema, "Empty{}", 7);
        expect(NULL, &schema, "null", 4);
        expect(xrtValueNull(), &schema, "null", 4);
        XL6_ClassTextSchemaV1 changed = schema;
        changed.RuntimeTypeTag = 72; bad(value, &changed, XERR_TYPE);
        changed.RuntimeTypeTag = 0; bad(value, &changed, XERR_ARGUMENT);
        changed = schema; changed.TypeId = CLASS_ID + 1; bad(value, &changed, XERR_TYPE);
        changed.TypeId = 0; bad(value, &changed, XERR_ARGUMENT);
        bad(value, NULL, XERR_ARGUMENT);
        changed = schema; changed.Fields = NULL; bad(value, &changed, XERR_ARGUMENT);
        changed = schema; changed.Name.Data = NULL; bad(value, &changed, XERR_ARGUMENT);
        xstrview missing = {"missing", 7};
        changed = schema; changed.Fields = &missing; changed.FieldCount = 1;
        bad(value, &changed, XERR_STATE);
        missing.Data = NULL; bad(value, &changed, XERR_ARGUMENT);
        bad(xrtValueBool(true), &schema, XERR_TYPE);
        xvalue* noTag = xrtValueObject(); assert(noTag);
        bad(noTag, &schema, XERR_TYPE); xrtValueRelease(noTag);
        assert(xrtValueObjectSetNew(value, xrtStrView("$xlir.class.runtime-type"), xrtValueString(xrtStrView("71"))));
        bad(value, &schema, XERR_TYPE);
        assert(xrtValueObjectSetNew(value, xrtStrView("$xlir.class.runtime-type"), xrtValueInt(71)));
        /* Field names themselves are views, not accidentally C strings. */
        xstrview nulName = {"n\0key", 5};
        assert(xrtValueObjectSetNew(value, nulName, xrtValueBool(true)));
        changed = (XL6_ClassTextSchemaV1){{"\xE4\xBD\xA0", 3}, 71, CLASS_ID, &nulName, 1};
        const char named[] = "\xE4\xBD\xA0{n\0key=true}";
        expect(value, &changed, named, sizeof(named)-1);
        void* raw = (void*)(uintptr_t)1;
        xvalue* unsupported = xrtValueHandleTake(&raw, &foreignOps, NULL); assert(unsupported && !raw);
        assert(xrtValueObjectSetNew(value, fields[0], unsupported));
        bad(value, &schema, XERR_UNSUPPORTED);
        char* large = xrtMalloc(1024*1024); assert(large);
        memset(large, 'x', 1024*1024); large[1024] = 0;
        assert(xrtValueObjectSetNew(value, fields[0], xrtValueString((xstrview){large, 1024*1024})));
        changed = (XL6_ClassTextSchemaV1){{"Long", 4}, 71, CLASS_ID, fields, 1};
        xstrview rendered = xl6ClassValueTextV1(value, &changed);
        assert(rendered.Data && rendered.Size == 1024*1024 + 11 &&
            !memcmp(rendered.Data, "Long{text=", 10) && !memcmp(rendered.Data+10, large, 1024*1024) &&
            rendered.Data[rendered.Size-1] == '}' && rendered.Data[rendered.Size] == 0);
        xrtFree((void*)rendered.Data); xrtFree(large);
        xrtValueRelease(value);
        assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    }
    assert(foreignDrops == 100);
    puts("class text: 100 exact schema/tag/NUL/char/large/boundary cycles balanced");
}
static void fault(const char* action, size_t point)
{
    xvalue* value = seed();
    const XL6_ClassTextSchemaV1* selected = &schema;
    if (!strcmp(action, "empty")) selected = &emptySchema;
    else if (!strcmp(action, "null")) value = (xrtValueRelease(value), NULL);
    else if (!strcmp(action, "large")) {
        char large[8192]; memset(large, 'x', sizeof(large)); large[31] = 0;
        assert(xrtValueObjectSetNew(value, fields[0], xrtValueString((xstrview){large, sizeof(large)})));
    } else assert(!strcmp(action, "class"));
    xmemdebugsnapshot before, after;
    xrtMemDebugSnapshot(&before); assert(xrtMemDebugFailAfter(point));
    xstrview text = xl6ClassValueTextV1(value, selected);
    bool hit = xl6ProductMemoryEnd();
    xerror* error = xrtTakeError();
    assert(hit ? !text.Data && !text.Size && xrtErrorKind(error) == XERR_MEMORY : text.Data && !error);
    xrtFree((void*)text.Data); xrtErrorFree(error); xrtClearError();
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
        after.AllocCount-before.AllocCount == after.FreeCount-before.FreeCount &&
        before.DoubleFreeCount == after.DoubleFreeCount && before.InvalidFreeCount == after.InvalidFreeCount &&
        before.UseAfterFreeCount == after.UseAfterFreeCount);
    if (value) expect(value, &emptySchema, "Empty{}", 7);
    xrtValueRelease(value);
    printf("class text %s point=%zu hit=%d balanced\n", action, point, hit);
}
int main(int argc, char** argv)
{
    assert(xrtMemDebugEnable(true));
    if (argc == 1) vectors();
    else { assert(argc == 3); fault(argv[1], (size_t)strtoull(argv[2], NULL, 10)); }
    return 0;
}
