#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_PATH
#define XRT_MODULE_TIME
#include "../bson_native.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifndef _WIN32
#include <signal.h>
#include <sys/resource.h>
#endif

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "BSON boundary failed at %d: %s\n", __LINE__, #condition); \
    return 1; \
} } while (0)

static int live_unchanged(xmemdebugsnapshot before, const char* operation,
    unsigned int point) {
    xmemdebugsnapshot after;
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    if (after.LiveCount == before.LiveCount &&
        after.LiveBytes == before.LiveBytes) return 1;
    fprintf(stderr, "BSON %s point %u leaked: %zu -> %zu allocations\n",
        operation, point, before.LiveCount, after.LiveCount);
    return 0;
}

#if defined(__linux__)
static int fail_clock_once;

int __real_clock_gettime(clockid_t clock_id, struct timespec* value);

int __wrap_clock_gettime(clockid_t clock_id, struct timespec* value)
{
    if (fail_clock_once) {
        fail_clock_once = 0;
        errno = EIO;
        return -1;
    }
    return __real_clock_gettime(clock_id, value);
}
#endif

static xvalue* input(const unsigned char* wire, size_t size) {
    return xrtValueBytes((xbytesview){ wire, size });
}

static int invalid(const unsigned char* wire, size_t size) {
    xvalue* source = input(wire, size);
    xvalue* result = xbson_decode(source);
    int rejected = result && xrtValueType(result) == XVALUE_NULL && xrtGetError();
    xrtValueRelease(result);
    xrtValueRelease(source);
    xrtClearError();
    return rejected;
}

/* 从合法嵌套文档变异，覆盖随机噪声很难走到的子文档与数组路径。 */
static int nested_mutations(void) {
    static const unsigned char nested[] = {
        41, 0, 0, 0,
        3, 'a', 0, 12, 0, 0, 0, 16, 'b', 0, 42, 0, 0, 0, 0,
        4, 'c', 0, 18, 0, 0, 0, 8, '0', 0, 1,
        2, '1', 0, 2, 0, 0, 0, 'x', 0, 0,
        0
    };
    static const unsigned char replacements[] = {
        0, 1, 2, 4, 8, 16, 127, 128, 255
    };
    unsigned char wire[sizeof(nested)];
    size_t i, j;
    xvalue *source, *result;

    source = input(nested, sizeof(nested));
    CHECK(source != NULL);
    result = xbson_decode(source);
    CHECK(result != NULL && xrtValueType(result) == XVALUE_OBJECT && !xrtGetError());
    xrtValueRelease(result);
    xrtValueRelease(source);

    for (i = 0; i < sizeof(nested); ++i) {
        CHECK(invalid(nested, i));
        for (j = 0; j < sizeof(replacements); ++j) {
            memcpy(wire, nested, sizeof(wire));
            wire[i] = replacements[j];
            source = input(wire, sizeof(wire));
            CHECK(source != NULL);
            result = xbson_decode(source);
            CHECK(result != NULL);
            CHECK((xrtValueType(result) == XVALUE_OBJECT && !xrtGetError()) ||
                (xrtValueType(result) == XVALUE_NULL && xrtGetError()));
            xrtValueRelease(result);
            xrtValueRelease(source);
        }
    }
    return 0;
}

/* 根文档深度为 0；第 64 层子文档超过公开的 64 层预算。 */
static int nesting_limit(void) {
    unsigned char wire[5 + 64 * 8] = {5, 0, 0, 0, 0};
    size_t size = 5;
    int depth;
    xvalue *source, *result, *document = xrtValueObject();

    CHECK(document != NULL);

    for (depth = 0; depth < 64; ++depth) {
        xvalue *outer = xrtValueObject();
        CHECK(outer != NULL &&
            xrtValueObjectSetTake(outer, XRT_STR_LITERAL("a"), &document));
        document = outer;
        memmove(wire + 7, wire, size);
        size += 8;
        wire[0] = (unsigned char)size;
        wire[1] = (unsigned char)(size >> 8);
        wire[2] = (unsigned char)(size >> 16);
        wire[3] = (unsigned char)(size >> 24);
        wire[4] = 3;
        wire[5] = 'a';
        wire[6] = 0;
        wire[size - 1] = 0;
        if (depth != 62 && depth != 63) continue;
        source = input(wire, size);
        CHECK(source != NULL);
        result = xbson_decode(source);
        CHECK(result != NULL);
        if (depth == 62) {
            CHECK(xrtValueType(result) == XVALUE_OBJECT && !xrtGetError());
        } else {
            CHECK(xrtValueType(result) == XVALUE_NULL && xrtGetError() &&
                xrtErrorKind(xrtGetError()) == XERR_RANGE);
        }
        xrtValueRelease(result);
        xrtValueRelease(source);
        result = xbson_encode(document);
        CHECK(result != NULL);
        if (depth == 62) {
            CHECK(xrtValueType(result) == XVALUE_BYTES && !xrtGetError());
        } else {
            CHECK(xrtValueType(result) == XVALUE_NULL && xrtGetError() &&
                xrtErrorKind(xrtGetError()) == XERR_RANGE);
        }
        xrtValueRelease(result);
    }
    xrtValueRelease(document);
    return 0;
}

static int input_size_limit(void) {
    static const unsigned char empty[] = {5, 0, 0, 0, 0};
    const size_t size = 64u * 1024u * 1024u + 1u;
    bytes owned = xrtCalloc(size, 1u);
    xvalue *large, *small, *result;
    char *hex;

    CHECK(owned != NULL);
    large = xrtValueBytesTake(&owned, size);
    small = input(empty, sizeof(empty));
    CHECK(large != NULL && small != NULL);
    result = xbson_decode(large);
    CHECK(result != NULL && xrtValueType(result) == XVALUE_NULL &&
        xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtValueRelease(result);
    result = xbson_decode_all(large);
    CHECK(result != NULL && xrtValueType(result) == XVALUE_NULL &&
        xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtValueRelease(result);
    result = xbson_concat(large, small);
    CHECK(result != NULL && xrtValueType(result) == XVALUE_NULL &&
        xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtValueRelease(result);
    hex = xbson_to_hex(large);
    CHECK(hex == NULL && xrtGetError() &&
        xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtValueRelease(small);
    xrtValueRelease(large);
    return 0;
}

static int hex_output_size_limit(void) {
    const size_t limit = 32u * 1024u * 1024u;
    bytes owned = xrtCalloc(limit + 1u, 1u);
    xvalue* source;
    char* hex;
    bool rejected;
    CHECK(owned != NULL);
    source = xrtValueBytesTake(&owned, limit + 1u);
    CHECK(source != NULL);
    hex = xbson_to_hex(source);
    rejected = hex == NULL && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_RANGE;
    xrtFree(hex);
    xrtValueRelease(source);
    CHECK(rejected);
    owned = xrtCalloc(limit, 1u);
    CHECK(owned != NULL);
    source = xrtValueBytesTake(&owned, limit);
    CHECK(source != NULL);
    hex = xbson_to_hex(source);
    rejected = hex != NULL && strlen(hex) == limit * 2u &&
        hex[0] == '0' && hex[limit * 2u - 1u] == '0' && xrtGetError() == NULL;
    xrtFree(hex);
    xrtValueRelease(source);
    CHECK(rejected);
    return 0;
}

static int string_nul_boundaries(void) {
    static const char embedded[] = {'a', 0, 'b'};
    static const char key_with_nul[] = {'a', 0, 'b'};
    xvalue* document = xrtValueObject();
    xvalue* text = xrtValueString((xstrview){embedded, sizeof(embedded)});
    xvalue* wire;
    xvalue* decoded;
    xvalue* number;
    xstrview restored;

    CHECK(document != NULL && text != NULL);
    CHECK(xrtValueObjectSetTake(document, XRT_STR_LITERAL("text"), &text));
    wire = xbson_encode(document);
    CHECK(wire != NULL && xrtValueType(wire) == XVALUE_BYTES);
    decoded = xbson_decode(wire);
    CHECK(decoded != NULL && xrtValueType(decoded) == XVALUE_OBJECT);
    CHECK(xrtValueGetString(xrtValueObjectGet(decoded, XRT_STR_LITERAL("text")),
            &restored) && restored.Size == sizeof(embedded) &&
        memcmp(restored.Data, embedded, sizeof(embedded)) == 0);
    xrtValueRelease(decoded);
    xrtValueRelease(wire);

    number = xrtValueInt(1);
    CHECK(number != NULL && xrtValueObjectSetTake(document,
        (xstrview){key_with_nul, sizeof(key_with_nul)}, &number));
    wire = xbson_encode(document);
    CHECK(wire != NULL && xrtValueType(wire) == XVALUE_NULL &&
        xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtValueRelease(wire);
    xrtValueRelease(document);
    return 0;
}

/* 对象键查询、子文档创建和写入任一点失配都不得返回半成品。 */
static int decode_allocation_failures(void) {
    static const unsigned char wire[] = {
        20, 0, 0, 0, 3, 'a', 0,
        12, 0, 0, 0, 16, 'b', 0, 1, 0, 0, 0, 0, 0
    };
    xvalue* source = input(wire, sizeof(wire));
    unsigned int failure;
    int completed = 0, triggered_count = 0;
    CHECK(source != NULL);
    for (failure = 0; failure < 128u; ++failure) {
        xvalue* decoded;
        bool triggered;
        xmemdebugsnapshot before;
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        CHECK(xrtMemDebugFailAfter(failure));
        decoded = xbson_decode(source);
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            ++triggered_count;
            CHECK(decoded != NULL && xrtValueType(decoded) == XVALUE_NULL);
            CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        } else {
            completed = decoded != NULL && xrtValueType(decoded) == XVALUE_OBJECT &&
                xrtGetError() == NULL;
        }
        xrtValueRelease(decoded);
        CHECK(live_unchanged(before, "decode", failure));
        if (!triggered) break;
    }
    xrtValueRelease(source);
    CHECK(triggered_count > 0);
    CHECK(completed);
    return 0;
}

/* 多文档入口还需覆盖结果数组创建和追加失败。 */
static int decode_all_allocation_failures(void) {
    static const unsigned char wire[] = {
        12, 0, 0, 0, 16, 'a', 0, 1, 0, 0, 0, 0,
        12, 0, 0, 0, 16, 'b', 0, 2, 0, 0, 0, 0
    };
    xvalue* source = input(wire, sizeof(wire));
    unsigned int failure;
    int completed = 0, triggered_count = 0;
    CHECK(source != NULL);
    for (failure = 0; failure < 128u; ++failure) {
        xvalue* decoded;
        bool triggered;
        xmemdebugsnapshot before;
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        CHECK(xrtMemDebugFailAfter(failure));
        decoded = xbson_decode_all(source);
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            ++triggered_count;
            CHECK(decoded == NULL || xrtValueType(decoded) == XVALUE_NULL);
            if (xrtGetError() == NULL || xrtErrorKind(xrtGetError()) != XERR_MEMORY) {
                fprintf(stderr, "decodeAll allocation point %u: error kind %d\n", failure,
                    xrtGetError() == NULL ? -1 : (int)xrtErrorKind(xrtGetError()));
            }
            CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        } else {
            completed = decoded != NULL && xrtValueType(decoded) == XVALUE_ARRAY &&
                xrtValueCount(decoded) == 2u && xrtGetError() == NULL;
        }
        xrtValueRelease(decoded);
        CHECK(live_unchanged(before, "decodeAll", failure));
        if (!triggered) break;
    }
    xrtValueRelease(source);
    CHECK(triggered_count > 0);
    CHECK(completed);
    return 0;
}

/* BSON 允许紧凑编码大量字段；编解码必须共用整份文档的项数预算。 */
static int aggregate_items_limit(void) {
    xvalue* document = xrtValueObject();
    xvalue* array = xrtValueArray();
    xvalue* encoded;
    xvalue* decoded;
    size_t i;
    CHECK(document != NULL && array != NULL);
    CHECK(xrtValueObjectSet(document, (xstrview){"items", 5u}, array));
    for (i = 0u; i < 999999u; ++i) {
        CHECK(xrtValueArrayAppend(array, xrtValueNull()));
    }
    encoded = xbson_encode(document);
    CHECK(encoded != NULL && xrtValueType(encoded) == XVALUE_BYTES &&
        xrtGetError() == NULL);
    decoded = xbson_decode(encoded);
    CHECK(decoded != NULL && xrtValueType(decoded) == XVALUE_OBJECT &&
        xrtValueCount(xrtValueObjectGet(decoded, (xstrview){"items", 5u})) ==
            999999u && xrtGetError() == NULL);
    xrtValueRelease(decoded);
    {
        xvalue* extra = xrtValueObject();
        xvalue* tail;
        xvalue* stream;
        CHECK(extra != NULL && xrtValueObjectSet(extra,
            (xstrview){"extra", 5u}, xrtValueNull()));
        tail = xbson_encode(extra);
        CHECK(tail != NULL && xrtValueType(tail) == XVALUE_BYTES);
        stream = xbson_concat(encoded, tail);
        CHECK(stream != NULL && xrtValueType(stream) == XVALUE_BYTES);
        decoded = xbson_decode_all(stream);
        CHECK((decoded == NULL || xrtValueType(decoded) == XVALUE_NULL) &&
            xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
        xrtValueRelease(decoded);
        xrtValueRelease(stream);
        xrtValueRelease(tail);
        xrtValueRelease(extra);
    }
    xrtValueRelease(encoded);

    CHECK(xrtValueArrayAppend(array, xrtValueNull()));
    encoded = xbson_encode(document);
    CHECK((encoded == NULL || xrtValueType(encoded) == XVALUE_NULL) &&
        xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtValueRelease(encoded);
    xrtValueRelease(array);
    xrtValueRelease(document);
    return 0;
}

static int encode_allocation_failures(void) {
    char text[600];
    xvalue* document = xrtValueObject();
    xvalue* item;
    unsigned int failure;
    int completed = 0, triggered_count = 0;
    CHECK(document != NULL);
    memset(text, 'x', sizeof(text));
    item = xrtValueString((xstrview){text, sizeof(text)});
    CHECK(item != NULL && xrtValueObjectSetTake(document,
        (xstrview){"blob", 4u}, &item));
    for (failure = 0; failure < 128u; ++failure) {
        xvalue* encoded;
        bool triggered;
        xmemdebugsnapshot before;
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        CHECK(xrtMemDebugFailAfter(failure));
        encoded = xbson_encode(document);
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            ++triggered_count;
            CHECK(encoded == NULL || xrtValueType(encoded) == XVALUE_NULL);
            CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        } else {
            completed = encoded != NULL && xrtValueType(encoded) == XVALUE_BYTES &&
                xrtGetError() == NULL;
        }
        xrtValueRelease(encoded);
        CHECK(live_unchanged(before, "encode", failure));
        if (!triggered) break;
    }
    xrtValueRelease(document);
    CHECK(triggered_count > 0);
    CHECK(completed);
    return 0;
}

static int concat_allocation_failures(void) {
    static const unsigned char wire[] = {
        12, 0, 0, 0, 16, 'a', 0, 1, 0, 0, 0, 0
    };
    xvalue* source = input(wire, sizeof(wire));
    unsigned int failure;
    int completed = 0, triggered_count = 0;
    CHECK(source != NULL);
    for (failure = 0; failure < 64u; ++failure) {
        xvalue* result;
        bool triggered;
        xmemdebugsnapshot before;
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        CHECK(xrtMemDebugFailAfter(failure));
        result = xbson_concat(source, source);
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            ++triggered_count;
            CHECK(result == NULL || xrtValueType(result) == XVALUE_NULL);
            CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        } else {
            xbytesview data;
            completed = result != NULL && xrtValueGetBytes(result, &data) &&
                data.Size == 2u * sizeof(wire) && xrtGetError() == NULL;
        }
        xrtValueRelease(result);
        CHECK(live_unchanged(before, "concat", failure));
        if (!triggered) break;
    }
    xrtValueRelease(source);
    CHECK(triggered_count > 0);
    CHECK(completed);
    return 0;
}

#ifndef _WIN32
static int save_failure_preserves_file(const char* work) {
    str path = xrtPathJoin(work, "atomic_write.bson");
    static const unsigned char old[] = "old document";
    struct rlimit saved, limited;
    void (*old_handler)(int) = SIG_DFL;
    xvalue* object = NULL;
    unsigned char* data = NULL;
    size_t size = 0;
    int ok = 0, handler_changed = 0, limit_changed = 0;

    if (!path) return 0;
    object = xrtValueObject();
    if (!object || !xrtFileWriteAll(path,
            (xbytesview){ old, sizeof(old) - 1u })) goto done;
    if (getrlimit(RLIMIT_FSIZE, &saved) != 0 || saved.rlim_cur <= 4u) goto done;
    limited = saved;
    limited.rlim_cur = 4u;
    old_handler = signal(SIGXFSZ, SIG_IGN);
    if (old_handler == SIG_ERR) goto done;
    handler_changed = 1;
    if (setrlimit(RLIMIT_FSIZE, &limited) != 0) goto done;
    limit_changed = 1;

    /* 文档最小也有 5 字节，限制为 4 字节即可稳定触发写入失败。 */
    ok = xbson_save(path, object) == 0;
done:
    if (limit_changed && setrlimit(RLIMIT_FSIZE, &saved) != 0) ok = 0;
    if (handler_changed && signal(SIGXFSZ, old_handler) == SIG_ERR) ok = 0;
    if (ok) {
        data = xrtFileReadAllLimit(path, 64u, &size);
        ok = data && size == sizeof(old) - 1u &&
            memcmp(data, old, size) == 0;
    }
    xrtFree(data);
    if (xrtFileExists(path) && !xrtFileDelete(path)) ok = 0;
    xrtFree(path);
    xrtValueRelease(object);
    return ok;
}
#endif

/* 文件缓冲与解码结果各自释放；失败后同一文件仍可正常读取。 */
static int load_allocation_failures(const char* work) {
    static const unsigned char wire[] = {
        12, 0, 0, 0, 16, 'a', 0, 42, 0, 0, 0, 0
    };
    str path = xrtPathJoin(work, "load_allocation.bson");
    unsigned int failure;
    int completed = 0, triggered_count = 0;
    CHECK(path != NULL);
    CHECK(xrtFileWriteAll(path, (xbytesview){ wire, sizeof(wire) }));
    for (failure = 0u; failure < 128u; ++failure) {
        xmemdebugsnapshot before;
        xvalue* result;
        bool triggered;
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        CHECK(xrtMemDebugFailAfter(failure));
        result = xbson_load(path);
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            ++triggered_count;
            CHECK(result == NULL || xrtValueType(result) == XVALUE_NULL);
            CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        } else {
            int64 value = 0;
            completed = result != NULL && xrtValueType(result) == XVALUE_OBJECT &&
                xrtValueGetInt(xrtValueObjectGet(result, (xstrview){ "a", 1u }), &value) &&
                value == 42 && xrtGetError() == NULL;
        }
        xrtValueRelease(result);
        CHECK(live_unchanged(before, "load", failure));
        if (!triggered) break;
    }
    CHECK(xrtFileDelete(path));
    xrtFree(path);
    CHECK(triggered_count > 0 && completed);
    return 0;
}

/* 编码和原子落盘的任一分配失败都保留旧文件及内存错误。 */
static int save_allocation_failures(const char* work) {
    static const unsigned char old[] = "old document";
    str path = xrtPathJoin(work, "save_allocation.bson");
    xvalue* document = xrtValueObject();
    xvalue* name = xrtValueString((xstrview){ "xlang", 5u });
    unsigned int failure;
    int completed = 0, triggered_count = 0;
    if (!path || !document || !name ||
        !xrtValueObjectSetTake(document, (xstrview){ "name", 4u }, &name) ||
        !xrtFileWriteAll(path, (xbytesview){ old, sizeof(old) - 1u })) goto done;
    for (failure = 0u; failure < 128u; ++failure) {
        size_t size = 0u;
        bytes saved;
        int64_t written;
        bool triggered, memory_error;
        xmemdebugsnapshot before;
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        if (!xrtMemDebugFailAfter(failure)) goto done;
        written = xbson_save(path, document);
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        memory_error = xrtGetError() != NULL &&
            xrtErrorKind(xrtGetError()) == XERR_MEMORY;
        if (triggered) {
            ++triggered_count;
            if (written != 0 || !memory_error) {
                fprintf(stderr, "BSON save allocation %u lost memory error\n", failure);
                goto done;
            }
            saved = xrtFileReadAllLimit(path, 64u, &size);
            if (!saved || size != sizeof(old) - 1u ||
                memcmp(saved, old, size) != 0) {
                xrtFree(saved);
                fprintf(stderr, "BSON save allocation %u changed target\n", failure);
                goto done;
            }
            xrtFree(saved);
        } else {
            completed = written > 0 && xrtGetError() == NULL;
        }
        if (!live_unchanged(before, "save", failure)) goto done;
        if (!triggered) break;
    }
done:
    xrtMemDebugFailClear();
    xrtClearError();
    xrtValueRelease(name);
    xrtValueRelease(document);
    if (path != NULL) {
        (void)xrtFileDelete(path);
        xrtFree(path);
    }
    return completed && triggered_count > 0;
}

int main(int argc, char** argv) {
    static const unsigned char empty[] = {5, 0, 0, 0, 0};
    static const unsigned char one[] = {12, 0, 0, 0, 0x10, 'a', 0, 42, 0, 0, 0, 0};
    static const unsigned char truncated[] = {9, 0, 0, 0, 0x10, 'a', 0, 42, 0};
    static const unsigned char duplicate[] = {19, 0, 0, 0,
        0x10, 'a', 0, 1, 0, 0, 0, 0x10, 'a', 0, 2, 0, 0, 0, 0};
    static const unsigned char bad_array[] = {20, 0, 0, 0, 0x04, 'a', 0,
        12, 0, 0, 0, 0x10, '1', 0, 3, 0, 0, 0, 0, 0};
    static const unsigned char bad_bool[] = {9, 0, 0, 0, 0x08, 'a', 0, 2, 0};
    static const unsigned char bad_utf8[] = {14, 0, 0, 0, 0x02, 'a', 0,
        2, 0, 0, 0, 0xff, 0, 0};
    static const unsigned char unsupported[] = {8, 0, 0, 0, 0x13, 'a', 0, 0};
    static const unsigned char undefined[] = {8, 0, 0, 0, 0x06, 'a', 0, 0};
    static const unsigned char bad_key[] = {6, 0, 0, 0, 0x10, 0};
    static const unsigned char trailing[] = {5, 0, 0, 0, 0, 0};
    static const unsigned char bad_subtype[] = {13, 0, 0, 0, 0x05, 'a', 0,
        0, 0, 0, 0, 2, 0};
    static const unsigned char binary[] = {15, 0, 0, 0, 0x05, 'b', 0,
        2, 0, 0, 0, 0, 0xff, 0, 0};
    xvalue *source, *decoded, *encoded, *stream, *documents, *item;
    xvalue *object, *list, *number;
    int64 integer;
    xtime time_value;
    static const xtime date_cases[] = {1, -1, 1000, -1000};
    char *oid1, *oid2, *hex1, *hex2;
    unsigned char random_wire[48];
    uint32_t seed = 0x12345678u;
    size_t iteration, i;

    if (argc != 2 || argv[1][0] == '\0') {
        fprintf(stderr, "BSON boundary test requires an isolated work directory\n");
        return 2;
    }
    source = input(one, sizeof(one));
    decoded = xbson_decode(source);
    CHECK(decoded && xrtValueType(decoded) == XVALUE_OBJECT);
    item = xrtValueObjectGet(decoded, (xstrview){ "a", 1 });
    CHECK(item && xrtValueGetInt(item, &integer) && integer == 42);
    encoded = xbson_encode(decoded);
    {
        xbytesview bytes;
        CHECK(encoded && xrtValueGetBytes(encoded, &bytes) &&
            bytes.Size == sizeof(one) && memcmp(bytes.Data, one, sizeof(one)) == 0);
    }
    CHECK(xbson_doc_length(encoded) == (long long)sizeof(one));
    hex1 = xbson_to_hex(encoded);
    hex2 = xbson_to_hex(encoded);
    CHECK(hex1 && hex2 && hex1 != hex2 &&
        strcmp(hex1, "0c0000001061002a00000000") == 0 && strcmp(hex1, hex2) == 0);
    xrtFree(hex1);
    xrtFree(hex2);
    CHECK(xbson_oid_time("invalid") == -1 && xrtGetError());
    {
        char unterminated_oid[25];
        memset(unterminated_oid, '0', sizeof(unterminated_oid));
        CHECK(xbson_oid_time(unterminated_oid) == -1 && xrtGetError());
    }
    hex1 = xbson_to_hex(encoded);
    CHECK(hex1 && !xrtGetError());
    xrtFree(hex1);
    CHECK(xbson_oid_time("invalid") == -1 && xrtGetError());
    CHECK(xbson_doc_length(encoded) == (long long)sizeof(one) && !xrtGetError());
    xrtValueRelease(decoded);
    xrtValueRelease(source);

    /* 即使内容是合法 BSON，string 也不能冒充原始 bytes。 */
    source = xrtValueString((xstrview){ (const char *)one, sizeof(one) });
    CHECK(source != NULL);
    decoded = xbson_decode(source);
    CHECK(decoded != NULL && xrtValueType(decoded) == XVALUE_NULL && xrtGetError());
    xrtValueRelease(decoded);
    xrtValueRelease(source);

    source = input(empty, sizeof(empty));
    stream = xbson_concat(encoded, source);
    CHECK(stream && xrtValueType(stream) == XVALUE_BYTES);
    documents = xbson_decode_all(stream);
    CHECK(documents && xrtValueCount(documents) == 2);
    xrtValueRelease(documents);
    xrtValueRelease(stream);
    xrtValueRelease(source);
    xrtValueRelease(encoded);

    source = input(binary, sizeof(binary));
    decoded = xbson_decode(source);
    CHECK(decoded && xrtValueType(decoded) == XVALUE_OBJECT);
    item = xrtValueObjectGet(decoded, (xstrview){ "b", 1 });
    {
        xbytesview bytes;
        CHECK(item && xrtValueGetBytes(item, &bytes) && bytes.Size == 2 &&
            bytes.Data[0] == 0xff && bytes.Data[1] == 0);
    }
    encoded = xbson_encode(decoded);
    {
        xbytesview bytes;
        CHECK(encoded && xrtValueGetBytes(encoded, &bytes) &&
            bytes.Size == sizeof(binary) && memcmp(bytes.Data, binary, sizeof(binary)) == 0);
    }
    xrtValueRelease(encoded);
    xrtValueRelease(decoded);
    xrtValueRelease(source);

    CHECK(invalid(truncated, sizeof(truncated)));
    CHECK(invalid(duplicate, sizeof(duplicate)));
    CHECK(invalid(bad_array, sizeof(bad_array)));
    CHECK(invalid(bad_bool, sizeof(bad_bool)));
    CHECK(invalid(bad_utf8, sizeof(bad_utf8)));
    CHECK(invalid(unsupported, sizeof(unsupported)));
    source = input(unsupported, sizeof(unsupported));
    CHECK(source != NULL && xbson_doc_length(source) == (int64)sizeof(unsupported) &&
        xrtGetError() == NULL);
    xrtValueRelease(source);
    CHECK(xbson_doc_length(NULL) == 0 && xrtGetError() == NULL);
    CHECK(invalid(undefined, sizeof(undefined)));
    CHECK(invalid(bad_key, sizeof(bad_key)));
    CHECK(invalid(trailing, sizeof(trailing)));
    CHECK(invalid(bad_subtype, sizeof(bad_subtype)));
    CHECK(nested_mutations() == 0);
    CHECK(nesting_limit() == 0);
    CHECK(input_size_limit() == 0);
    CHECK(hex_output_size_limit() == 0);
    CHECK(string_nul_boundaries() == 0);

    /* 稀疏 list 不能假装是连续 BSON array。 */
    list = xrtValueIntMap();
    number = xrtValueInt(3);
    CHECK(list && number && xrtValueIntMapSetTake(list, 4, &number));
    object = xrtValueObject();
    CHECK(object && xrtValueObjectSetTake(object, (xstrview){ "items", 5 }, &list));
    encoded = xbson_encode(object);
    CHECK(encoded && xrtValueType(encoded) == XVALUE_NULL && xrtGetError());
    xrtValueRelease(encoded);
    xrtValueRelease(object);

    /* BSON Date 只有毫秒精度；不能悄悄截断 XRT 的微秒。 */
    for (i = 0; i < sizeof(date_cases) / sizeof(date_cases[0]); ++i) {
        object = xrtValueObject();
        item = xrtValueTime(date_cases[i]);
        CHECK(object && item &&
            xrtValueObjectSetTake(object, (xstrview){ "when", 4 }, &item));
        encoded = xbson_encode(object);
        CHECK(encoded != NULL);
        if (date_cases[i] % XRT_TIME_MILLISECOND != 0) {
            CHECK(xrtValueType(encoded) == XVALUE_NULL && xrtGetError());
        } else {
            CHECK(xrtValueType(encoded) == XVALUE_BYTES && !xrtGetError());
            decoded = xbson_decode(encoded);
            CHECK(decoded && xrtValueType(decoded) == XVALUE_OBJECT);
            item = xrtValueObjectGet(decoded, (xstrview){ "when", 4 });
            CHECK(item && xrtValueGetTime(item, &time_value) &&
                time_value == date_cases[i]);
            xrtValueRelease(decoded);
        }
        xrtValueRelease(encoded);
        xrtValueRelease(object);
    }

    {
        int64 before, after;
#if defined(__linux__)
        /* 时钟故障不得生成带伪造纪元时间戳的 ObjectId。 */
        fail_clock_once = 1;
        CHECK(xbson_gen_oid() == NULL);
        CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_IO);
#endif
        xrtClearError();
        before = xrtTimeUnix(xrtNow());
        CHECK(!xrtGetError());
        oid1 = xbson_gen_oid();
        CHECK(oid1 && !xrtGetError());
        oid2 = xbson_gen_oid();
        CHECK(oid2 && !xrtGetError());
        after = xrtTimeUnix(xrtNow());
        CHECK(!xrtGetError());
        CHECK(oid1 != oid2 && strlen(oid1) == 24 && strlen(oid2) == 24 &&
            strcmp(oid1, oid2) != 0);
        CHECK(xbson_oid_time(oid1) >= before && xbson_oid_time(oid1) <= after);
        CHECK(xbson_oid_time(oid2) >= before && xbson_oid_time(oid2) <= after);
    }
    CHECK(xbson_oid_time("000000000000000000000000") == 0);
    CHECK(xbson_oid_time("invalid") == -1);
    CHECK(xrtGetError() != NULL);
    CHECK(xbson_oid_time("000000000000000000000000") == 0 && !xrtGetError());
    xrtFree(oid1);
    xrtFree(oid2);

    /* 文件限额必须在读取前生效，不能先分配超限缓冲区。 */
    {
        str path = xrtPathJoin(argv[1], "over_limit.bson");
        int rejected;
        CHECK(path != NULL);
        CHECK(xrtFileWriteAll(path, (xbytesview){NULL, 0u}));
        CHECK(xrtFileSetSize(path, 64u * 1024u * 1024u + 1u));
        decoded = xbson_load(path);
        rejected = decoded && xrtValueType(decoded) == XVALUE_NULL &&
            xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_RANGE;
        xrtValueRelease(decoded);
        CHECK(xrtFileDelete(path));
        xrtFree(path);
        CHECK(rejected);
    }

#ifndef _WIN32
    CHECK(save_failure_preserves_file(argv[1]));
#endif
    CHECK(load_allocation_failures(argv[1]) == 0);
    CHECK(save_allocation_failures(argv[1]));
    CHECK(decode_allocation_failures() == 0);
    CHECK(decode_all_allocation_failures() == 0);
    CHECK(aggregate_items_limit() == 0);
    CHECK(encode_allocation_failures() == 0);
    CHECK(concat_allocation_failures() == 0);

    /* 固定种子畸形输入，兼顾回归可重现性与解析器边界。 */
    for (iteration = 0; iteration < 5000; ++iteration) {
        size_t size = iteration % sizeof(random_wire);
        xmemdebugsnapshot before;
        for (i = 0; i < size; ++i) {
            seed = seed * 1664525u + 1013904223u;
            random_wire[i] = (unsigned char)(seed >> 24);
        }
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        source = input(random_wire, size);
        decoded = xbson_decode(source);
        CHECK(decoded != NULL);
        xrtValueRelease(decoded);
        xrtValueRelease(source);
        CHECK(live_unchanged(before, "malformed input", (unsigned int)iteration));
    }
    puts("bson boundaries: ok");
    return 0;
}
