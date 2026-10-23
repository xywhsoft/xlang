#define XRT_MODULE_ALL
#include "epub_native.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "epub C test failed at %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

static int put(xvalue *object, const char *key, const char *value) {
    return xrtValueObjectSetNew(object, (xstrview){key, strlen(key)},
        xrtValueString((xstrview){value, strlen(value)}));
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

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : NULL;
    char *workdir = NULL;
    char *owned_path = NULL;
    char broken[1024];
    xvalue *meta = xrtValueObject();
    xvalue *chapters = xrtValueArray();
    xvalue *chapter = xrtValueObject();
    xvalue *many;
    int i;
    char long_line[4097];
    char *entry, *second;
    long long written;
    unsigned char *bytes;
    size_t size, directory;
    if (path == NULL) {
        workdir = xrtDirTemp(NULL, "xlang_epub_test_", NULL);
        CHECK(workdir != NULL);
        owned_path = xrtPathJoin(workdir, "book.epub");
        CHECK(owned_path != NULL);
        path = owned_path;
    }
    CHECK(snprintf(broken, sizeof(broken), "%s.bad", path) > 0 &&
        strlen(path) + 4 < sizeof(broken));
    CHECK(meta != NULL && chapters != NULL && chapter != NULL);
    memset(long_line, 'X', 4096);
    long_line[4096] = 0;
    CHECK(put(meta, "title", "中文 & <测试>") &&
        put(meta, "language", "zh-CN") &&
        put(meta, "identifier", "urn:xlang:test:epub-c") &&
        put(meta, "modified", "2026-08-01T00:00:00Z") &&
        put(chapter, "title", "第一章") &&
        put(chapter, "text", long_line) &&
        xrtValueArrayAppendNew(chapters, chapter));
#if defined(__linux__)
    {
        xvalue *clock_meta = xrtValueObject();
        CHECK(clock_meta != NULL &&
            put(clock_meta, "title", "Clock test") &&
            put(clock_meta, "language", "en") &&
            put(clock_meta, "identifier", "urn:xlang:test:clock"));
        fail_clock_once = 1;
        CHECK(xep_build(broken, clock_meta, chapters) == 0);
        CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_IO);
        CHECK(!xrtFileExists(broken));
        CHECK(xrtFileWriteAll(broken, (xbytesview){(cbytes)"old", 3}));
        fail_clock_once = 1;
        CHECK(xep_build(broken, clock_meta, chapters) == 0);
        CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_IO);
        bytes = xrtFileReadAll(broken, &size);
        CHECK(bytes != NULL && size == 3 && memcmp(bytes, "old", 3) == 0);
        xrtFree(bytes);
        CHECK(xrtFileDelete(broken));
        CHECK(xep_build(broken, clock_meta, chapters) > 0);
        CHECK(xrtFileDelete(broken));
        xrtValueRelease(clock_meta);
    }
#endif
    written = xep_build(path, meta, chapters);
    CHECK(written > 4096);
    bytes = xrtFileReadAll(path, &size);
    CHECK(bytes != NULL && size == (size_t)written &&
        memcmp(bytes, "PK\x03\x04", 4) == 0 &&
        bytes[8] == 0 && bytes[9] == 0 &&
        bytes[28] == 0 && bytes[29] == 0 &&
        memcmp(bytes + 30, "mimetype", 8) == 0);
    directory = (size_t)bytes[size - 22 + 16] |
        ((size_t)bytes[size - 22 + 17] << 8) |
        ((size_t)bytes[size - 22 + 18] << 16) |
        ((size_t)bytes[size - 22 + 19] << 24);
    CHECK(directory + 46 <= size - 22);
    {
        const size_t damaged[] = {
            6, 8, 14, 18, 22, 26, 30, 38,
            directory + 8, directory + 10, directory + 16,
            directory + 20, directory + 24
        };
        size_t k;
        for (k = 0; k < sizeof(damaged) / sizeof(damaged[0]); ++k) {
            unsigned char original;
            CHECK(damaged[k] < size);
            original = bytes[damaged[k]];
            bytes[damaged[k]] ^= 1;
            CHECK(xrtFileWriteAll(broken, (xbytesview){bytes, size}));
            CHECK(xep_read_entry(broken, "mimetype") == NULL &&
                xrtGetError() != NULL);
            bytes[damaged[k]] = original;
        }
    }
    {
        unsigned random = 0x62e2a911u;
        size_t k;
        for (k = 0; k < 512; ++k) {
            size_t offset;
            unsigned char original;
            random = random * 1664525u + 1013904223u;
            offset = (size_t)random % size;
            original = bytes[offset];
            bytes[offset] ^= (unsigned char)(1u << (random >> 24 & 7u));
            CHECK(xrtFileWriteAll(broken, (xbytesview){bytes, size}));
            entry = xep_read_entry(broken, "mimetype");
            if (entry == NULL) {
                CHECK(xrtGetError() != NULL);
            } else {
                CHECK(strcmp(entry, "application/epub+zip") == 0 || entry[0] == 0);
                xrtFree(entry);
            }
            bytes[offset] = original;
        }
    }
    bytes[directory + 34] = 1; /* 条目不能声明位于其他磁盘。 */
    CHECK(xrtFileWriteAll(broken, (xbytesview){bytes, size}));
    CHECK(xep_read_entry(broken, "mimetype") == NULL);
    bytes[directory + 34] = 0;
    bytes[size - 22 + 4] = 1; /* 多磁盘 ZIP 不是本包支持的格式。 */
    CHECK(xrtFileWriteAll(broken, (xbytesview){bytes, size}));
    CHECK(xep_read_entry(broken, "mimetype") == NULL);
    bytes[size - 22 + 4] = 0;
    bytes[size - 22 + 8] = 0; /* 本磁盘条目数与总条目数不一致。 */
    CHECK(xrtFileWriteAll(broken, (xbytesview){bytes, size}));
    CHECK(xep_read_entry(broken, "mimetype") == NULL);
    bytes[size - 22 + 8] = bytes[size - 22 + 10];
    bytes[size - 22 + 16] = 0xff; /* 损坏中央目录偏移。 */
    CHECK(xrtFileWriteAll(broken, (xbytesview){bytes, size}));
    xrtFree(bytes);
    CHECK(xep_read_entry(broken, "OEBPS/content.opf") == NULL);
    xrtClearError();
    entry = xep_read_entry(path, "OEBPS/content.opf");
    CHECK(entry != NULL && strstr(entry, "中文 &amp; &lt;测试&gt;") != NULL &&
        strstr(entry, "dcterms:modified") != NULL);
    second = xep_read_entry(path, "OEBPS/ch1.xhtml");
    CHECK(second != NULL && strlen(second) > 4096 &&
        strstr(second, long_line) != NULL &&
        strstr(entry, "urn:xlang:test:epub-c") != NULL);
    xrtFree(entry);
    xrtFree(second);
    CHECK(put(meta, "title", "Line\rTitle") &&
        put(meta, "author", "A\tB\nC") &&
        put(meta, "language", "zh\tCN") &&
        put(chapter, "title", "Chapter\rName"));
    CHECK(xep_build(broken, meta, chapters) > 0);
    entry = xep_read_entry(broken, "OEBPS/content.opf");
    CHECK(entry != NULL && strstr(entry, "Line&#xD;Title") != NULL &&
        strstr(entry, "A&#x9;B&#xA;C") != NULL);
    xrtFree(entry);
    entry = xep_read_entry(broken, "OEBPS/ch1.xhtml");
    CHECK(entry != NULL && strstr(entry, "xml:lang=\"zh&#x9;CN\"") != NULL &&
        strstr(entry, "Chapter&#xD;Name") != NULL);
    xrtFree(entry);
    CHECK(xrtFileDelete(broken));
    CHECK(put(meta, "title", "中文 & <测试>") &&
        put(meta, "language", "zh-CN") &&
        put(chapter, "title", "第一章"));
    entry = xep_read_entry(path, "missing.xhtml");
    CHECK(entry != NULL && entry[0] == 0);
    xrtFree(entry);

    CHECK(xrtMemDebugFailAfter(0u));
    CHECK(xep_read_entry(path, "mimetype") == NULL &&
        xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    xrtMemDebugFailClear();
    xrtClearError();
    entry = xep_read_entry(path, "mimetype");
    CHECK(entry != NULL && strcmp(entry, "application/epub+zip") == 0);
    xrtFree(entry);

    CHECK(xrtMemDebugFailAfter(0u));
    CHECK(xep_build(path, meta, chapters) == 0);
    xrtMemDebugFailClear();
    xrtClearError();
    entry = xep_read_entry(path, "mimetype");
    CHECK(entry != NULL && strcmp(entry, "application/epub+zip") == 0);
    xrtFree(entry);

    CHECK(put(meta, "modified", "2026-99-99T00:00:00Z"));
    CHECK(xep_build(path, meta, chapters) == 0);
    xrtClearError();
    CHECK(put(meta, "modified", "2024-02-30T00:00:00Z"));
    CHECK(xep_build(path, meta, chapters) == 0);
    xrtClearError();
    entry = xep_read_entry(path, "OEBPS/content.opf");
    CHECK(entry != NULL && strstr(entry, "2026-08-01T00:00:00Z") != NULL);
    xrtFree(entry);
    CHECK(put(meta, "modified", "2024-02-29T00:00:00Z"));
    CHECK(xep_build(path, meta, chapters) > 0);
    entry = xep_read_entry(path, "OEBPS/content.opf");
    CHECK(entry != NULL && strstr(entry, "2024-02-29T00:00:00Z") != NULL);
    xrtFree(entry);
    CHECK(put(meta, "modified", "2026-08-01T00:00:00Z"));
    CHECK(put(meta, "title", "bad \xef\xbf\xbe title"));
    CHECK(xep_build(path, meta, chapters) == 0);
    xrtClearError();
    CHECK(put(meta, "title", "中文 & <测试>"));
    CHECK(put(chapter, "text", "bad \xef\xbf\xbf chapter"));
    CHECK(xep_build(path, meta, chapters) == 0);
    xrtClearError();
    CHECK(put(chapter, "text", long_line));
    {
        const size_t large_sizes[] = {
            8u * 1024u * 1024u - 256u,
            8u * 1024u * 1024u + 1u
        };
        char *large_text = (char *)xrtMalloc(large_sizes[1] + 1u);
        CHECK(large_text != NULL);
        memset(large_text, 'A', large_sizes[1] + 1u);
        for (i = 0; i < 2; ++i) {
            large_text[large_sizes[i]] = 0;
            CHECK(put(chapter, "text", large_text));
            CHECK(xep_build(path, meta, chapters) == 0);
            CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
            entry = xep_read_entry(path, "mimetype");
            CHECK(entry != NULL && strcmp(entry, "application/epub+zip") == 0);
            xrtFree(entry);
            large_text[large_sizes[i]] = 'A';
        }
        xrtFree(large_text);
        CHECK(put(chapter, "text", long_line));
    }
    CHECK(xrtFileWriteAll(broken, (xbytesview){(cbytes)"PK\x03\x04", 4}));
    CHECK(xep_read_entry(broken, "mimetype") == NULL);
    xrtClearError();
    CHECK(xrtFileSetSize(broken, 8u * 1024u * 1024u + 1u));
    CHECK(xep_read_entry(broken, "mimetype") == NULL &&
        xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtClearError();
    CHECK(xrtValueObjectSetNew(meta, (xstrview){"identifier", 10},
        xrtValueInt(4)));
    CHECK(xep_build(path, meta, chapters) == 0);
    xrtClearError();
    entry = xep_read_entry(path, "mimetype");
    CHECK(entry != NULL && strcmp(entry, "application/epub+zip") == 0);
    xrtFree(entry);

    many = xrtValueArray();
    CHECK(many != NULL);
    for (i = 0; i < 100; ++i) {
        xvalue *part = xrtValueObject();
        CHECK(part != NULL && put(part, "title", "章") &&
            put(part, "text", "内容") &&
            xrtValueArrayAppendNew(many, part));
    }
    CHECK(xep_build(path, meta, many) == 0); /* meta.identifier 此时仍是整数。 */
    CHECK(put(meta, "identifier", "urn:xlang:test:epub-c"));
    CHECK(xep_build(path, meta, many) > 0);
    entry = xep_read_entry(path, "OEBPS/ch100.xhtml");
    CHECK(entry != NULL && strstr(entry, "内容") != NULL);
    xrtFree(entry);
    chapter = xrtValueObject();
    CHECK(chapter != NULL && put(chapter, "title", "超限") &&
        xrtValueArrayAppendNew(many, chapter));
    CHECK(xep_build(path, meta, many) == 0);
    xrtValueRelease(many);

    xrtValueRelease(meta);
    xrtValueRelease(chapters);
    CHECK(xrtFileDelete(broken));
    if (workdir != NULL) {
        CHECK(xrtFileDelete(path));
        CHECK(xrtDirRemove(workdir));
        xrtFree(owned_path);
        xrtFree(workdir);
    }
    puts("epub C test: PASS");
    return 0;
}
