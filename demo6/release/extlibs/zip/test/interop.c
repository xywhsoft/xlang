#include "../include/xrt_zip.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* const kUtf8Name = "docs/\xe4\xbd\xa0\xe5\xa5\xbd.txt";
static const unsigned char kBinary[] = {0u, 255u};

static int emit_archive(void)
{
    xrtziparchive* archive = xrtZipArchiveCreate(NULL);
    xbuffer* result = NULL;
    size_t i;
    int status = 1;
    if (archive == NULL ||
        !xrtZipArchiveAddDirectory(archive, "docs") ||
        !xrtZipArchiveAdd(archive, "docs/hello.txt", "hello ZIP", 9u,
            false, XRT_ZIP_METHOD_DEFLATE, 6) ||
        !xrtZipArchiveAdd(archive, "data.bin", kBinary, sizeof(kBinary),
            false, XRT_ZIP_METHOD_STORE, 0) ||
        !xrtZipArchiveAdd(archive, kUtf8Name, "world", 5u,
            false, XRT_ZIP_METHOD_DEFLATE, 6) ||
        !xrtZipArchiveClose(archive)) goto done;
    result = xrtZipArchiveTakeBuffer(archive);
    if (result == NULL) goto done;
    for (i = 0; i < result->Size; ++i)
        printf("%02x", ((const unsigned char*)result->Data)[i]);
    putchar('\n');
    status = 0;
done:
    xrtBufferDestroy(result);
    xrtZipArchiveDestroy(archive);
    return status;
}

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static int check_entry(xrtziparchive* archive, const char* name,
    const void* expected, size_t length)
{
    xbuffer* value = xrtZipArchiveRead(archive, name);
    int valid = value != NULL && value->Size == length &&
        memcmp(value->Data, expected, length) == 0;
    xrtBufferDestroy(value);
    return valid;
}

static int read_archive(const char* hex, int expect_corrupt)
{
    unsigned char data[4096];
    size_t length = strlen(hex), i;
    xrtziparchive* archive;
    int valid;
    if (length == 0u || length > sizeof(data) * 2u || (length & 1u)) return 1;
    for (i = 0; i < length / 2u; ++i) {
        int high = hex_digit(hex[i * 2u]);
        int low = hex_digit(hex[i * 2u + 1u]);
        if (high < 0 || low < 0) return 1;
        data[i] = (unsigned char)((high << 4) | low);
    }
    archive = xrtZipArchiveOpenMemory(data, length / 2u, false, NULL);
    if (archive == NULL) return 1;
    if (expect_corrupt) {
        xbuffer* value = xrtZipArchiveRead(archive, "data.bin");
        const xrtziperror* error = xrtZipArchiveError(archive);
        valid = value == NULL && error != NULL && error->iCode != XRT_ZIP_OK;
        xrtBufferDestroy(value);
        xrtZipArchiveDestroy(archive);
        if (valid) puts("zip damaged CRC: PASS");
        return valid ? 0 : 1;
    }
    valid = xrtZipArchiveCount(archive) == 4u &&
        xrtZipArchiveLocate(archive, "docs/") >= 0 &&
        check_entry(archive, "docs/hello.txt", "hello ZIP", 9u) &&
        check_entry(archive, "data.bin", kBinary, sizeof(kBinary)) &&
        check_entry(archive, kUtf8Name, "world", 5u);
    xrtZipArchiveDestroy(archive);
    if (valid) puts("zip independent interop: PASS");
    return valid ? 0 : 1;
}

int main(void)
{
    const char* mode = getenv("XL_ZIP_INTEROP_MODE");
    if (mode != NULL && strcmp(mode, "emit") == 0) return emit_archive();
    if (mode != NULL && strcmp(mode, "read") == 0) {
        const char* hex = getenv("XL_ZIP_INTEROP_HEX");
        return hex == NULL ? 1 : read_archive(hex, 0);
    }
    if (mode != NULL && strcmp(mode, "reject") == 0) {
        const char* hex = getenv("XL_ZIP_INTEROP_HEX");
        return hex == NULL ? 1 : read_archive(hex, 1);
    }
    fputs("expected XL_ZIP_INTEROP_MODE=emit|read|reject\n", stderr);
    return 1;
}
