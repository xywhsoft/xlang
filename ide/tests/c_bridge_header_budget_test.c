/* Exercise the actual private header cache at the protocol capacity. */
#define _WIN32_WINNT 0x0601
#define XRT_MODULE_ALL
#include <windows.h>
#include <stdio.h>
#include "../src/services/c_bridge.c"
#define REQUIRE(x) do { if (!(x)) { fprintf(stderr, "header budget:%d: %s\n", __LINE__, #x); return 1; } } while (0)

static xvalue* environment(size_t count)
{
    xvalue* result = xrtValueObject();
    xvalue* headers = xrtValueArray();
    unsigned char digest[32]; char hex[65];
    if (!result || !headers || !xrtSha256("x", 1, digest)) abort();
    for (size_t i = 0; i < 32; ++i) snprintf(hex + 2 * i, 3, "%02x", digest[i]);
    for (size_t i = 0; i < count; ++i) {
        char name[64]; snprintf(name, sizeof(name), "include/header_%zu.h", i);
        xvalue* row = xrtValueObject();
        if (!row || !cbput(row, "name", xrtValueString(cbsv(name))) ||
            !cbput(row, "base64", xrtValueString(cbsv("eA=="))) ||
            !cbput(row, "sha256", xrtValueString(cbsv(hex))) ||
            !cbput(row, "size", xrtValueInt(1)) || !xrtValueArrayAppendNew(headers, row)) abort();
    }
    if (!cbput(result, "embeddedHeadersAvailable", xrtValueBool(true)) ||
        !cbput(result, "builtinBytes", xrtValueInt((int64)count)) ||
        !cbput(result, "builtinHeaders", headers)) abort();
    return result;
}
static int initialize(xide_c_bridge_batch* batch, const char* root, const char* name)
{
    batch->cache = xidePathResolveLexical(root, name);
    batch->headers = xrtValueArray(); batch->paths = xrtValueArray();
    batch->directories = xrtValueArray();
    return batch->cache && batch->headers && batch->paths && batch->directories && cbmkdir(batch, batch->cache);
}
static void cleanup(xide_c_bridge_batch* batch)
{
    for (size_t i = 0; i < batch->leaseCount; ++i) CloseHandle(batch->leases[i]);
    for (size_t i = xrtValueCount(batch->directories); i > 0; --i) {
        wchar_t* path = cbwide(cbstr(xrtValueArrayGet(batch->directories, i - 1), 32767));
        if (path) RemoveDirectoryW(path);
        free(path);
    }
    xrtValueRelease(batch->headers); xrtValueRelease(batch->paths);
    xrtValueRelease(batch->directories); free(batch->cache);
}
int main(int argc, char** argv)
{
    xide_c_bridge_batch accepted = {0}, rejected = {0}; xide_model_error error = {0};
    REQUIRE(argc == 2 && xrtDirCreateAll(argv[1]));
    REQUIRE(initialize(&accepted, argv[1], "accepted"));
    REQUIRE(initialize(&rejected, argv[1], "rejected"));
    xvalue* good = environment(1024); xvalue* bad = environment(1025);
    REQUIRE(cbheaders(&accepted, good, &error));
    REQUIRE(xrtValueCount(accepted.headers) == 1024 && accepted.leaseCount == 1024);
    REQUIRE(!cbheaders(&rejected, bad, &error) && rejected.leaseCount == 0);
    /* Each of the 128 projects also owns one forced header and one VFS file. */
    for (size_t i = 0; i < 256; ++i) {
        char name[64]; char* path = NULL;
        snprintf(name, sizeof(name), "project_%zu.cache", i);
        REQUIRE(cbwrite(&accepted, name, "x", 1, &path)); free(path);
    }
    char* overflow = NULL;
    REQUIRE(accepted.leaseCount == 1280 && !cbwrite(&accepted, "overflow.cache", "x", 1, &overflow) && !overflow);
    xrtValueRelease(good); xrtValueRelease(bad); cleanup(&accepted); cleanup(&rejected);
    puts("C bridge: 1024 headers, 128 projects, overflow rejection passed");
    return 0;
}
