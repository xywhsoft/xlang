/* 同一个真实目录的等价路径必须采用同一套条目检查和覆盖规则。 */
#define XRT_MODULE_ALL
#include "../include/xrt_zip.h"
#include <stdio.h>
#include <string.h>

extern bool xzip_archive_extract_all(void* archive, const char* destination, bool overwrite);

static unsigned cases, failures;

static bool extract(xrtziparchive* archive, const char* destination, bool overwrite, unsigned mode)
{
    return mode ? xzip_archive_extract_all(archive, destination, overwrite) :
        xrtZipArchiveExtractAll(archive, destination, overwrite);
}

static bool contains(const char* filename, const char* expected)
{
    size_t size = 0;
    unsigned char* data = xrtFileReadAll(filename, &size);
    bool same = data && size == strlen(expected) && memcmp(data, expected, size) == 0;
    xrtFree(data);
    return same;
}

int main(void)
{
    static const char* formats[] = {"%s/output", "%s/./output", "%s/output/.",
        "%s/output/./", "%s/output//", "%s/./output//./"};
    char* previous = xrtPathCwd();
    char* root = previous ? xrtDirTemp(NULL, "xlang-zip-paths-", "") : NULL;
    if (!root || !xrtPathSetCwd(root)) { xrtFree(root); xrtFree(previous); return 1; }
    for (unsigned directory = 0; directory < 2; ++directory) {
        xrtziparchive* writer = xrtZipArchiveCreate(NULL);
        xbuffer* wire = NULL;
        if (writer && (!directory || xrtZipArchiveAddDirectory(writer, "a/b/")) &&
            xrtZipArchiveAdd(writer, "a/b/payload.txt", "payload", 7, false, XRT_ZIP_METHOD_STORE, 0) &&
            xrtZipArchiveClose(writer)) wire = xrtZipArchiveTakeBuffer(writer);
        xrtZipArchiveDestroy(writer);
        if (!wire) { ++failures; break; }
        for (unsigned absolute = 0; absolute < 2; ++absolute)
            for (unsigned mode = 0; mode < 2; ++mode)
                for (size_t form = 0; form < sizeof(formats) / sizeof(formats[0]); ++form) {
                    char* destination = xrtFormat(formats[form], absolute ? root : ".");
                    xrtziparchive* archive = xrtZipArchiveOpenMemory(wire->Data, wire->Size, false, NULL);
                    bool ok = destination && archive && xrtZipArchiveIsOpen(archive) && xrtDirCreate("output");
                    xrtClearError();
                    ok = ok && extract(archive, destination, false, mode) && contains("output/a/b/payload.txt", "payload");
                    if (ok) {
                        ok = xrtFileWriteAll("output/a/b/payload.txt", (xbytesview){(const unsigned char*)"prior", 5}) &&
                            !extract(archive, destination, false, mode) &&
                            xrtZipArchiveError(archive)->iCode == XRT_ZIP_ERROR_EXISTS &&
                            contains("output/a/b/payload.txt", "prior") &&
                            extract(archive, destination, true, mode) && contains("output/a/b/payload.txt", "payload");
                    }
                    if (!ok) {
                        const xrtziperror* error = xrtZipArchiveError(archive);
                        ++failures;
                        fprintf(stderr, "zip path dir=%u absolute=%u mode=%u form=%zu: %s\n",
                            directory, absolute, mode, form, error ? error->sMessage : "setup failed");
                    }
                    xrtZipArchiveDestroy(archive);
                    xrtFree(destination);
                    if (!xrtDirRemoveAll("output")) ++failures;
                    xrtClearError();
                    ++cases;
                }
        xrtBufferDestroy(wire);
    }
    // 先恢复工作目录，才清理本例排他创建的树；恢复失败时保留现场。
    if (!xrtPathSetCwd(previous)) { xrtFree(root); xrtFree(previous); return 1; }
    if (!xrtDirRemoveAll(root)) ++failures;
    xrtFree(root);
    xrtFree(previous);
    printf("zip extraction paths: %u cases, %u failures\n", cases, failures);
    return failures ? 1 : 0;
}
