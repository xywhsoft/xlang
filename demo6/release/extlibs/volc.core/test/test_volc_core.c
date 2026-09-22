#define XRT_MODULE_ALL
#include "xrt.h"

#include <stdio.h>
#include <string.h>

const char* x_volc_core_sha256_hex(const char* text);
const char* x_volc_core_hmac_sha256_hex(const char* key, const char* text);
const char* x_volc_core_canonical_query(const char* action, const char* version, const char* extra_pairs);
const char* x_volc_core_sign_preview_acep(const char* action, const char* version,
    const char* access_key, const char* secret_key, const char* session_token,
    const char* body, const char* x_date);

static int fail(const char* message)
{
    fprintf(stderr, "volc.core C smoke failed: %s\n", message);
    return 1;
}

int main(void)
{
    const char* digest = x_volc_core_sha256_hex("abc");
    const char* mac = x_volc_core_hmac_sha256_hex("key", "The quick brown fox jumps over the lazy dog");
    const char* query = x_volc_core_canonical_query("ListApps", "2020-01-01", "Limit=10\nName=x lang");
    const char* preview = x_volc_core_sign_preview_acep("ListApps", "2020-01-01", "AKID", "SECRET", "", "{}", "20240102T030405Z");

    if (strcmp(digest, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") != 0) return fail("sha256");
    if (strcmp(mac, "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8") != 0) return fail("hmac");
    if (strstr(query, "Action=ListApps") == NULL || strstr(query, "Name=x%20lang") == NULL) return fail("query");
    if (strstr(preview, "authorization") == NULL || strstr(preview, "20240102T030405Z") == NULL) return fail("preview");
    puts("volc.core C smoke: PASS");
    return 0;
}

#include "../x_volc_core_native.c"
