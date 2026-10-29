#include "../../release/include/xrt_decl.h"
#include "../../release/extlibs/argon2/include/argon2_native.h"
#include "../../release/extlibs/bloom/include/bloom.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define LIT(text) (text), (sizeof(text) - 1u)

static const char *encoded;
static const char *bloom_text;
static const char *counting_text;
static void *filter;
static void *counting;
static xallocator system_allocator;
static int backing_fail_after = -1;
static bool backing_triggered;

/* 临时区直接使用 backing allocator；普通堆故障注入不能覆盖这条路径。 */
static bool fail_backing(void)
{
    if (backing_fail_after < 0) return false;
    if (backing_fail_after-- != 0) return false;
    backing_triggered = true;
    return true;
}

static ptr test_alloc(ptr context, size_t size)
{
    (void)context;
    if (fail_backing()) return NULL;
    return system_allocator.Alloc(system_allocator.Context, size);
}

static ptr test_realloc(ptr context, ptr memory, size_t size)
{
    (void)context;
    if (fail_backing()) return NULL;
    return system_allocator.Realloc(system_allocator.Context, memory, size);
}

static void test_free(ptr context, ptr memory)
{
    (void)context;
    system_allocator.Free(system_allocator.Context, memory);
}

static int owned_text(char *text)
{
    int ok = text != NULL && text[0] != '\0';
    xrtFree(text);
    return ok;
}

static int operation(unsigned int kind)
{
    void *copy;
    switch (kind) {
    case 0:
        return owned_text(xargon2_hash(LIT("password"), LIT("somesalt"), LIT(""), LIT(""), 1, 64, 1, 32, 16, 2));
    case 1: return xargon2_verify(LIT("password"), encoded, strlen(encoded), LIT(""), LIT("")) != 0;
    case 2: return owned_text(xargon2_extract(encoded, strlen(encoded)));
    case 3: copy = xbloom_create(100, 0.01); break;
    case 4: copy = xbloom_deserialize(bloom_text); break;
    case 5: copy = xbloom_clone(filter); break;
    case 6: return xbloom_serialize(filter)[0] != '\0';
    case 7: copy = xcounting_bloom_create(100, 0.01); break;
    case 8: copy = xcounting_bloom_deserialize(counting_text); break;
    case 9: copy = xcounting_bloom_clone(counting); break;
    case 11: {
        char *owned = xbloom_serialize_owned(filter);
        int ok = owned != NULL && owned[0] != '\0';
        xrtFree(owned);
        return ok;
    }
    case 12: {
        char *owned = xcounting_bloom_serialize_owned(counting);
        int ok = owned != NULL && owned[0] != '\0';
        xrtFree(owned);
        return ok;
    }
    case 13:
        return owned_text(xargon2_hash_raw(LIT("password"), LIT("somesalt"), LIT(""), LIT(""), 1, 64, 1, 32, 16, 2));
    case 14: return owned_text(xargon2_generate_salt(16));
    case 15: {
        char *empty = xargon2_extract(LIT("invalid"));
        int ok = empty != NULL && empty[0] == '\0';
        xrtFree(empty);
        return ok;
    }
    default: return xcounting_bloom_serialize(counting)[0] != '\0';
    }
    if (kind < 7) xbloom_destroy(copy);
    else xcounting_bloom_destroy(copy);
    return copy != NULL;
}

int main(void)
{
    unsigned int mode, kind, index, failed = 0, passed = 0;
    xallocator allocator = {NULL, test_alloc, test_realloc, test_free};
    xmemdebugsnapshot before, after;
    xrtGetAllocator(&system_allocator);
    assert(xrtSetAllocator(&allocator));
    assert(xrtMemDebugEnable(true));
    encoded = xargon2_hash(LIT("password"), LIT("somesalt"), LIT(""), LIT(""), 1, 64, 1, 32, 16, 2);
    filter = xbloom_create(100, 0.01);
    counting = xcounting_bloom_create(100, 0.01);
    assert(encoded && filter && counting);
    xbloom_add(filter, "keep", -1);
    xcounting_bloom_add(counting, "keep", -1);
    bloom_text = xrtStrDup(xbloom_serialize(filter));
    counting_text = xrtStrDup(xcounting_bloom_serialize(counting));
    assert(bloom_text && counting_text);
    assert(xrtTempClear() && xrtTempTrim(xrtTempCurrent(), 0));
    xrtClearError();
    xrtMemDebugSnapshot(&before);

    /* 普通堆与临时区分别注入；失败必须报 MemoryError，且不改变输入对象。 */
    for (mode = 0; mode < 2; ++mode) {
      for (kind = 0; kind < 16; ++kind) {
        unsigned int good = 0, bad = 0;
        if (mode == 0 && (kind == 6 || kind == 10)) continue;
        if (mode == 1 && kind != 6 && kind != 10) continue;
        for (index = 0; index < 16; ++index) {
            int ok;
            bool injected;
            if (mode == 0) assert(xrtMemDebugFailAfter(index));
            else { backing_triggered = false; backing_fail_after = (int)index; }
            ok = operation(kind);
            injected = mode == 0 ? xrtMemDebugFailTriggered() : backing_triggered;
            xrtMemDebugFailClear();
            backing_fail_after = -1;
            if (ok) { assert(!injected && xrtGetError() == NULL); ++good; ++passed; }
            else {
                assert(injected && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                ++bad; ++failed;
            }
            xrtClearError();
            assert(xrtTempClear() && xrtTempTrim(xrtTempCurrent(), 0));
            assert(xbloom_contains(filter, "keep", -1) && xbloom_count(filter) == 1);
            assert(xcounting_bloom_contains(counting, "keep", -1) && xcounting_bloom_count(counting) == 1);
            xrtMemDebugSnapshot(&after);
            assert(after.LiveBytes == before.LiveBytes && after.LiveCount == before.LiveCount);
            assert(after.InvalidFreeCount == before.InvalidFreeCount);
            assert(after.DoubleFreeCount == before.DoubleFreeCount);
            assert(after.OverflowCount == before.OverflowCount && after.UnderflowCount == before.UnderflowCount);
        }
        if (!good || !bad) fprintf(stderr, "allocation mode=%u kind=%u good=%u bad=%u\n", mode, kind, good, bad);
        assert(good && bad);
      }
    }
    xbloom_destroy(filter);
    xcounting_bloom_destroy(counting);
    xrtFree((void *)encoded);
    xrtFree((void *)bloom_text);
    xrtFree((void *)counting_text);
    xrtTempUnit(xrtTempCurrent());
    printf("argon2/bloom allocation: rejected=%u completed=%u\n", failed, passed);
    return 0;
}
