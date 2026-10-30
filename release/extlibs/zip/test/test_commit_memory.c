/* 提交内存归档时的真实分配故障：分类、回滚、重试与完整数据。 */
#define XRT_MODULE_ALL
#include "../include/xrt_zip.h"
#include <stdio.h>
#include <string.h>

/* 语言薄绑定使用同一个 close，不另造测试实现。 */
extern bool xzip_archive_close(void *handle);

static unsigned failures, cases, oom;

static void check(bool ok, const char *why) {
    if (!ok) { ++failures; fprintf(stderr, "zip commit: %s\n", why); }
}

static bool balanced(const xmemdebugsnapshot *a, const xmemdebugsnapshot *b) {
    return a->LiveCount == b->LiveCount && a->LiveBytes == b->LiveBytes &&
        a->InvalidFreeCount == b->InvalidFreeCount && a->DoubleFreeCount == b->DoubleFreeCount &&
        a->OverflowCount == b->OverflowCount && a->UnderflowCount == b->UnderflowCount &&
        a->UseAfterFreeCount == b->UseAfterFreeCount;
}

static bool roundtrip(xrtziparchive *archive, const unsigned char *payload, size_t size) {
    xbuffer *wire = xrtZipArchiveTakeBuffer(archive);
    xrtziparchive *reader = wire ? xrtZipArchiveOpenMemory(wire->Data, wire->Size, false, NULL) : NULL;
    xbuffer *value = reader && xrtZipArchiveIsOpen(reader) ? xrtZipArchiveRead(reader, "payload.bin") : NULL;
    bool ok = value && value->Size == size && (!size || !memcmp(value->Data, payload, size));
    xrtBufferDestroy(value);
    xrtZipArchiveDestroy(reader);
    xrtBufferDestroy(wire);
    return ok;
}

int main(void) {
    static const size_t sizes[] = {0, 65472, 65536, 131072, 262161};
    size_t maximum = sizes[sizeof(sizes) / sizeof(sizes[0]) - 1];
    unsigned char *payload;
    unsigned method, mode, length;
    uint32 state = 0x6a09e667u;
    xmemdebugsnapshot before, after;
    check(xrtMemDebugEnabled() || xrtMemDebugEnable(true), "enable tracking");
    xrtSetErrorInfo(XERR_VALUE, "zip.test", 0, "warmup");
    xrtClearError();
    payload = (unsigned char *)xrtMalloc(maximum);
    if (!payload) return 1;
    /* 固定的不可压缩数据，让 STORE/DEFLATE 都跨过内存源的初始容量。 */
    for (size_t i = 0; i < maximum; ++i) {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        payload[i] = (unsigned char)state;
    }
    for (method = 0; method < 2; ++method) for (mode = 0; mode < 2; ++mode)
        for (length = 0; length < sizeof(sizes) / sizeof(sizes[0]); ++length) {
            unsigned budget;
            bool complete = false;
            for (budget = 0; budget < 64; ++budget) {
                xrtziparchive *archive;
                bool closed, triggered;
                int compression = method ? XRT_ZIP_METHOD_DEFLATE : XRT_ZIP_METHOD_STORE;
                xrtMemDebugSnapshot(&before);
                archive = xrtZipArchiveCreate(NULL);
                if (!archive || !xrtZipArchiveIsOpen(archive) ||
                    !xrtZipArchiveAdd(archive, "payload.bin", payload, sizes[length], false, compression, 6)) {
                    xrtZipArchiveDestroy(archive); xrtFree(payload); return 1;
                }
                xrtClearError();
                check(xrtMemDebugFailAfter(budget), "set OOM");
                closed = mode ? xzip_archive_close(archive) : xrtZipArchiveClose(archive);
                triggered = xrtMemDebugFailTriggered();
                xrtMemDebugFailClear();
                if (triggered) {
                    const xrtziperror *error = xrtZipArchiveError(archive);
                    check(!closed && error && error->iCode == XRT_ZIP_ERROR_MEMORY,
                        "commit OOM became an I/O/archive error");
                    check(xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY,
                        "XRT error lost MEMORY");
                    check(xrtZipArchiveIsOpen(archive), "failed commit closed the archive");
                    ++oom;
                    xrtClearError();
                    check(xrtZipArchiveClose(archive), "commit did not recover after OOM");
                } else {
                    check(closed, "ordinary close failed");
                    complete = true;
                }
                check(roundtrip(archive, payload, sizes[length]), "recovered archive lost payload bytes/CRC");
                xrtZipArchiveDestroy(archive);
                xrtClearError();
                xrtMemDebugSnapshot(&after);
                check(balanced(&before, &after), "commit/rollback leaked or invalidly freed XRT memory");
                ++cases;
                if (complete) break;
            }
            check(complete, "OOM scan did not reach successful commit");
        }
    xrtFree(payload);
    printf("zip commit: %u cases, %u real OOM, %u failures\n", cases, oom, failures);
    return failures ? 1 : 0;
}
