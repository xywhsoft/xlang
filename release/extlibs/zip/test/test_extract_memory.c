/* 覆盖解压的路径准备、临时文件、正文和提交，故障后仍复用同一归档。 */
#define XRT_MODULE_ALL
#include "../include/xrt_zip.h"
#include "../../_libzip/third_party/libzip/lib/zip.h"
#include <stdio.h>
#include <string.h>

extern bool xzip_archive_extract_all(void* archive, const char* destination, bool overwrite);

static unsigned failures, cases, injected;

static void check(bool ok, const char* why, unsigned scenario, unsigned mode, unsigned budget)
{
    if (!ok) {
        ++failures;
        fprintf(stderr, "zip extract scenario=%u mode=%u budget=%u: %s\n", scenario, mode, budget, why);
    }
}

static bool balanced(const xmemdebugsnapshot* a, const xmemdebugsnapshot* b)
{
    return a->LiveCount == b->LiveCount && a->LiveBytes == b->LiveBytes &&
        a->InvalidFreeCount == b->InvalidFreeCount && a->DoubleFreeCount == b->DoubleFreeCount &&
        a->OverflowCount == b->OverflowCount && a->UnderflowCount == b->UnderflowCount &&
        a->UseAfterFreeCount == b->UseAfterFreeCount;
}

static xbuffer* fixture(unsigned scenario)
{
    xrtziparchive* writer = xrtZipArchiveCreate(NULL);
    const char* name = scenario < 2 ? "payload.txt" : "目录/nested/payload.txt";
    xbuffer* wire = NULL;
    if (writer && xrtZipArchiveIsOpen(writer) &&
        (scenario != 3 || xrtZipArchiveAddDirectory(writer, "目录/nested/")) &&
        xrtZipArchiveAdd(writer, name, "payload", 7, false, XRT_ZIP_METHOD_STORE, 0) &&
        xrtZipArchiveClose(writer)) wire = xrtZipArchiveTakeBuffer(writer);
    xrtZipArchiveDestroy(writer);
    return wire;
}

int main(void)
{
    if (!(xrtMemDebugEnabled() || xrtMemDebugEnable(true))) return 1;
    // 错误槽在注入前创建；本测试不把错误槽冷启动与解压的故障混为一谈。
    xrtSetErrorInfo(XERR_VALUE, "zip.test", 0, "warmup");
    xrtClearError();
    char* root = xrtDirTemp(NULL, "xlang-zip-extract-", "");
    if (!root) return 1;
    for (unsigned scenario = 0; scenario < 4; ++scenario) {
        xbuffer* wire = fixture(scenario);
        if (!wire) { ++failures; break; }
        for (unsigned mode = 0; mode < 2; ++mode) {
            bool complete = false;
            for (unsigned budget = 0; budget < 256; ++budget) {
                xmemdebugsnapshot before, after;
                xrtMemDebugSnapshot(&before);
                char* destination = xrtPathJoin(root, "output");
                char* file = destination ? xrtPathJoin(destination,
                    scenario < 2 ? "payload.txt" : "目录/nested/payload.txt") : NULL;
                xrtziparchive* archive = xrtZipArchiveOpenMemory(wire->Data, wire->Size, false, NULL);
                if (!destination || !file || !archive || !xrtZipArchiveIsOpen(archive) ||
                    (scenario != 1 && !xrtDirCreate(destination))) return 1;
                xrtClearError();
                if (!xrtMemDebugFailAfter(budget)) return 1;
                bool ok = mode ? xzip_archive_extract_all(archive, destination, true) :
                    xrtZipArchiveExtractAll(archive, destination, true);
                bool failed = xrtMemDebugFailTriggered();
                xrtMemDebugFailClear();
                if (failed) {
                    const xrtziperror* error = xrtZipArchiveError(archive);
                    check(!ok, "allocation failure reported success", scenario, mode, budget);
                    if (!error || error->iCode != XRT_ZIP_ERROR_MEMORY || error->iBackendCode != ZIP_ER_MEMORY) {
                        ++failures;
                        fprintf(stderr, "zip extract scenario=%u mode=%u budget=%u: code=%d backend=%d %s\n",
                            scenario, mode, budget, error ? error->iCode : -1,
                            error ? error->iBackendCode : -1, error ? error->sMessage : "no error");
                    }
                    if (mode) check(xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY,
                        "language boundary did not publish MEMORY", scenario, mode, budget);
                    ++injected;
                } else {
                    check(ok, "ordinary extraction failed", scenario, mode, budget);
                    complete = true;
                }
                xrtClearError();
                check(xrtZipArchiveIsOpen(archive), "failed extraction consumed archive", scenario, mode, budget);
                check(xrtZipArchiveExtractAll(archive, destination, true),
                    "same archive did not recover", scenario, mode, budget);
                size_t size = 0;
                unsigned char* content = xrtFileReadAll(file, &size);
                check(content && size == 7 && memcmp(content, "payload", 7) == 0,
                    "recovered output differs", scenario, mode, budget);
                xrtFree(content);
                xrtZipArchiveDestroy(archive);
                check(xrtDirRemoveAll(destination), "owned directory cleanup failed", scenario, mode, budget);
                xrtFree(file);
                xrtFree(destination);
                xrtClearError();
                xrtMemDebugSnapshot(&after);
                check(balanced(&before, &after), "live memory or invalid release changed", scenario, mode, budget);
                ++cases;
                if (complete) break;
            }
            check(complete, "fault scan did not reach ordinary success", scenario, mode, 256);
        }
        xrtBufferDestroy(wire);
    }
    if (!xrtDirRemove(root)) ++failures;
    xrtFree(root);
    printf("zip extraction: %u cases, %u real OOM, %u failures\n", cases, injected, failures);
    return failures ? 1 : 0;
}
