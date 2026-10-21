#define XRT_MODULE_IO_FILE
#define XRT_MODULE_FILE_TEMP
#include "../include/zstd_native.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    static const unsigned char payload[] = {'z', 's', 't', 'd', 0, 0xff, 'f', 'i', 'l', 'e'};
    char packed[512] = {0}, plain[512] = {0};
    str source = NULL;
    xfile file = NULL;
    unsigned char result[sizeof(payload) + 1u];
    xreader *reader = NULL;
    size_t written = 0, size = 0;
    int ok = 0;

    file = xrtFileTemp(".", ".zstd-test-", ".bin", &source);
    if (file == NULL || source == NULL) { goto cleanup; }
    if (!xrtWriteFull(file, payload, sizeof(payload), &written) || written != sizeof(payload)) {
        goto cleanup;
    }
    if (!xrtClose(file)) { file = NULL; goto cleanup; }
    file = NULL;
    if (snprintf(packed, sizeof(packed), "%s.zst", source) >= (int)sizeof(packed) ||
        snprintf(plain, sizeof(plain), "%s.plain", source) >= (int)sizeof(plain)) {
        goto cleanup;
    }
    if (xzstd_compress_file(source, packed, 3, false, 0, 0, NULL) != 1 ||
        xzstd_decompress_file(packed, plain, false, 0, 0, 0, NULL) != 1) {
        goto cleanup;
    }
    reader = xrtReaderOpen(plain);
    if (reader == NULL || !xrtReaderRead(reader, result, sizeof(result), &size) ||
        size != sizeof(payload) || memcmp(result, payload, size) != 0) {
        goto cleanup;
    }
    if (!xrtReaderDestroy(reader)) { reader = NULL; goto cleanup; }
    reader = NULL;
    if (xzstd_compress_file(source, packed, 3, false, 0, 0, NULL) != 0 ||
        xrtErrorKind(xrtGetError()) != XERR_EXISTS) {
        goto cleanup;
    }
    ok = 1;

cleanup:
    if (!ok) { fprintf(stderr, "zstd C file API: FAIL (%s)\n", xzstd_last_error()); }
    if (reader != NULL) xrtReaderDestroy(reader);
    if (file != NULL) xrtClose(file);
    if (source != NULL) {
        xrtFileDelete(source);
        if (packed[0] != 0) xrtFileDelete(packed);
        if (plain[0] != 0) xrtFileDelete(plain);
        xrtFree(source);
    }
    if (ok) puts("zstd C file API: PASS");
    return ok ? 0 : 1;
}
