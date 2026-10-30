/* 标准空块不是空输入：仍须校验 CRC、raw Snappy 语法及完整 framing。 */
#define XRT_MODULE_MEMORY_DEBUG
#ifdef __TINYC__
#include "../third_party/xrt_snappy.c"
#include "../xrt_snappy.c"
#else
#include "xrt_snappy.h"
#endif
#include <stdio.h>
#include <string.h>

static const unsigned char identifier[] = {255,6,0,0,'s','N','a','P','p','Y'};
static const unsigned char raw_empty[] = {1,4,0,0,216,234,130,162};
static const unsigned char compressed_empty[] = {0,5,0,0,216,234,130,162,0};
static const unsigned char abc[] = {1,7,0,0,110,87,241,33,'a','b','c'};
static unsigned failures, cases, injected;
static void check(bool ok, const char *why) {
    if (!ok) { ++failures; fprintf(stderr, "snappy empty: %s\n", why); }
}
static bool equal(const xbuffer *value, const unsigned char *expected, size_t size) {
    return value && value->Size == size && (!size || !memcmp(value->Data, expected, size));
}
static void append(unsigned char *out, size_t *used, const void *value, size_t size) {
    memcpy(out + *used, value, size); *used += size;
}
static bool balanced(const xmemdebugsnapshot *a, const xmemdebugsnapshot *b) {
    return a->LiveCount == b->LiveCount && a->LiveBytes == b->LiveBytes &&
        a->InvalidFreeCount == b->InvalidFreeCount && a->DoubleFreeCount == b->DoubleFreeCount &&
        a->OverflowCount == b->OverflowCount && a->UnderflowCount == b->UnderflowCount &&
        a->UseAfterFreeCount == b->UseAfterFreeCount;
}

/* 每个合法样本走一次性、所有二分切点及前半段 drain，核对累计计数。 */
static void valid(const unsigned char *wire, size_t size, const unsigned char *plain,
    size_t length, unsigned members) {
    xbuffer *value = NULL;
    xrtsnappyerror error;
    bool ok = xrtSnappyDecompress(wire, size, NULL, &value, &error);
    check(ok && equal(value, plain, length), "valid one-shot frame rejected/corrupted");
    xrtBufferDestroy(value); ++cases;
    xreader *reader = xrtReaderFromMemory((xbytesview){wire, size});
    value = xrtBufferCreate();
    xwriter *writer = value ? xrtWriterFromBuffer(value) : NULL;
    check(reader && writer && xrtSnappyDecompressStream(reader, writer, NULL, NULL, NULL, &error) &&
        equal(value, plain, length), "stream wrapper rejected/corrupted empty chunk");
    xrtReaderDestroy(reader); xrtWriterDestroy(writer); xrtBufferDestroy(value); ++cases;
    for (size_t split = 0; split <= size; ++split) {
        xrtsnappydecompressor *decoder = xrtSnappyDecompressorCreate(NULL);
        xbuffer *head = NULL, *tail = NULL;
        ok = decoder && xrtSnappyDecompressorUpdate(decoder, wire, split);
        if (ok) { head = xrtSnappyDecompressorDrainBuffer(decoder); ok = head != NULL; }
        if (ok) { ok = xrtSnappyDecompressorUpdate(decoder, wire + split, size - split) &&
            xrtSnappyDecompressorFinish(decoder); }
        if (ok) {
            tail = xrtSnappyDecompressorTakeBuffer(decoder);
            ok = tail && xrtBufferAppend(head, xrtBufferView(tail)) && equal(head, plain, length) &&
                xrtSnappyDecompressorFinished(decoder) &&
                xrtSnappyDecompressorInputBytes(decoder) == size &&
                xrtSnappyDecompressorOutputBytes(decoder) == length &&
                xrtSnappyDecompressorMembers(decoder) == members;
        }
        check(ok, "split/drain empty chunk rejected/corrupted");
        xrtBufferDestroy(tail); xrtBufferDestroy(head); xrtSnappyDecompressorDestroy(decoder);
        ++cases;
    }
}

/* 只允许真正的空 raw 流；CRC 错、缺 varint、假零长度后带指令仍须拒绝。 */
static void invalid(const unsigned char *chunk, size_t size) {
    unsigned char wire[64]; size_t used = 0;
    xbuffer *value = NULL; xrtsnappyerror error;
    append(wire, &used, identifier, sizeof(identifier)); append(wire, &used, chunk, size);
    bool ok = xrtSnappyDecompress(wire, used, NULL, &value, &error);
    check(!ok && !value && (error.iCode == XRT_SNAPPY_ERROR_DATA ||
        error.iCode == XRT_SNAPPY_ERROR_TRUNCATED), "invalid empty chunk accepted/misclassified");
    xrtBufferDestroy(value); ++cases;
}

static void oom(const unsigned char *chunk, size_t size) {
    unsigned char wire[32]; size_t used = 0;
    bool complete = false; unsigned budget;
    append(wire, &used, identifier, sizeof(identifier)); append(wire, &used, chunk, size);
    for (budget = 0; budget < 64; ++budget) {
        xmemdebugsnapshot before, after; xbuffer *value = NULL; xrtsnappyerror error;
        xrtClearError(); xrtMemDebugSnapshot(&before);
        check(xrtMemDebugFailAfter(budget), "arm OOM");
        bool ok = xrtSnappyDecompress(wire, used, NULL, &value, &error);
        bool triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        if (triggered) {
            check(!ok && !value && error.iCode == XRT_SNAPPY_ERROR_MEMORY, "OOM lost memory category");
            ++injected;
        } else { check(ok && equal(value, NULL, 0), "ordinary empty decode failed"); complete = true; }
        xrtBufferDestroy(value); value = NULL; xrtClearError();
        xrtMemDebugSnapshot(&after); check(balanced(&before, &after), "OOM path leaked/damaged memory");
        check(xrtSnappyDecompress(wire, used, NULL, &value, &error) && equal(value, NULL, 0),
            "immediate recovery failed");
        xrtBufferDestroy(value); xrtClearError(); ++cases;
        if (complete) break;
    }
    check(complete, "OOM scan never reached success");
}

/* 空输出不消耗输出预算，但不能绕过输入字节、非空正文及成员数上限。 */
static void limits(const unsigned char *chunk, size_t size) {
    unsigned char wire[64]; size_t used = 0;
    xrtsnappyoptions options; xrtsnappyerror error; xbuffer *value = NULL;
    append(wire, &used, identifier, sizeof(identifier)); append(wire, &used, chunk, size);
    xrtSnappyOptionsInit(&options); options.tLimits.iMaxOutputBytes = 1;
    options.tLimits.iMaxItemBytes = 1; options.tLimits.iMaxInputBytes = used;
    check(xrtSnappyDecompress(wire, used, &options, &value, &error) && equal(value, NULL, 0),
        "empty output rejected at exact input/output budgets");
    xrtBufferDestroy(value); value = NULL; ++cases;
    --options.tLimits.iMaxInputBytes;
    check(!xrtSnappyDecompress(wire, used, &options, &value, &error) && !value &&
        error.iCode == XRT_SNAPPY_ERROR_LIMIT, "empty chunk bypassed input limit"); ++cases;
    options.tLimits.iMaxInputBytes = 0;
    append(wire, &used, identifier, sizeof(identifier)); options.tLimits.iMaxEntries = 1;
    check(!xrtSnappyDecompress(wire, used, &options, &value, &error) && !value &&
        error.iCode == XRT_SNAPPY_ERROR_LIMIT, "empty chunk bypassed member limit"); ++cases;
    options.tLimits.iMaxEntries = 0;
    append(wire, &used, abc, sizeof(abc));
    check(!xrtSnappyDecompress(wire, used, &options, &value, &error) && !value &&
        error.iCode == XRT_SNAPPY_ERROR_LIMIT, "empty chunk reset nonempty output/item budget"); ++cases;
    xrtBufferDestroy(value);
}

int main(void) {
    xmemdebugsnapshot before, after;
    check(xrtMemDebugEnabled() || xrtMemDebugEnable(true), "enable tracking");
    xrtSetErrorInfo(XERR_IO, "snappy.empty", 0, "warmup"); xrtClearError();
    xrtMemDebugSnapshot(&before);
    for (unsigned mode = 0; mode < 8; ++mode) {
        unsigned char wire[96], plain[6]; size_t used = 0, length = 0;
        append(wire, &used, identifier, sizeof(identifier));
        if (mode & 2) { append(wire, &used, abc, sizeof(abc)); append(plain, &length, "abc", 3); }
        append(wire, &used, mode & 1 ? compressed_empty : raw_empty,
            mode & 1 ? sizeof(compressed_empty) : sizeof(raw_empty));
        /* 零长 padding 与重复 identifier 不产生数据，也不重置既有输出。 */
        const unsigned char padding[] = {254,0,0,0}; append(wire, &used, padding, sizeof(padding));
        if (mode & 4) {
            append(wire, &used, identifier, sizeof(identifier));
            append(wire, &used, abc, sizeof(abc)); append(plain, &length, "abc", 3);
        }
        valid(wire, used, plain, length, mode & 4 ? 2 : 1);
    }
    unsigned char wrong[16];
    memcpy(wrong, raw_empty, sizeof(raw_empty)); wrong[4] ^= 1; invalid(wrong, sizeof(raw_empty));
    memcpy(wrong, compressed_empty, sizeof(compressed_empty)); wrong[4] ^= 1; invalid(wrong, sizeof(compressed_empty));
    memcpy(wrong, compressed_empty, sizeof(compressed_empty)); wrong[1] = 6; wrong[9] = 0; invalid(wrong, 10);
    const unsigned char missing_varint[] = {0,4,0,0,216,234,130,162}; invalid(missing_varint, sizeof(missing_varint));
    for (unsigned type = 0; type < 2; ++type) for (unsigned length = 0; length < 4; ++length) {
        memset(wrong, 0, sizeof(wrong)); wrong[0] = (unsigned char)type; wrong[1] = (unsigned char)length;
        invalid(wrong, 4 + length);
    }
    for (size_t cut = 1; cut < sizeof(raw_empty); ++cut) { invalid(raw_empty, cut); }
    for (size_t cut = 1; cut < sizeof(compressed_empty); ++cut) { invalid(compressed_empty, cut); }
    limits(raw_empty, sizeof(raw_empty)); limits(compressed_empty, sizeof(compressed_empty));
    oom(raw_empty, sizeof(raw_empty)); oom(compressed_empty, sizeof(compressed_empty));
    xrtClearError(); xrtMemDebugSnapshot(&after);
    check(balanced(&before, &after), "full test leaked/damaged memory");
    printf("snappy empty chunks: %u cases, %u real OOM, %u failures\n", cases, injected, failures);
    return failures ? 1 : 0;
}
