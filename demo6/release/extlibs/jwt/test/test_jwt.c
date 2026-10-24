#define XRT_MODULE_ALL
#include "xrt.h"
#include <stdio.h>
#include <string.h>
#include "../include/jwt_native.h"

int main(void)
{
    char* token = xjwt_encode("{\"sub\":\"xlang\"}", "secret");
    char* decoded = NULL;
    char* header = NULL;
    char* empty = NULL;
    char* retained[16] = {0};
    size_t i;
    int result = 0;
    const char* rfcToken =
        "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
        "eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5MDIyfQ."
        "SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c";
    const char* noneToken =
        "eyJhbGciOiJub25lIiwidHlwIjoiSldUIn0."
        "eyJzdWIiOiJ4bGFuZyJ9.";
    const char* arrayToken =
        "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
        "WzEsMl0.Ha9RdPPxY2XSrcm8aE5F7mpFzEt5PMzBqV9GFvtbXLo";

    if (token == NULL || !xjwt_verify(token, "secret") || xjwt_verify(token, "wrong")) {
        result = 1; goto cleanup;
    }
    decoded = xjwt_decode(token);
    if (decoded == NULL || strcmp(decoded, "{\"sub\":\"xlang\"}") != 0) {
        result = 2; goto cleanup;
    }
    header = xjwt_header(token);
    if (header == NULL || strstr(header, "HS256") == NULL) { result = 3; goto cleanup; }
    if (!xjwt_verify(rfcToken, "your-256-bit-secret")) { result = 4; goto cleanup; }
    if (xjwt_verify(noneToken, "secret") || xjwt_error()[0] == '\0') {
        result = 5; goto cleanup;
    }
    if (xjwt_encode("[1,2]", "secret") != NULL || xjwt_error()[0] == '\0') {
        result = 6; goto cleanup;
    }
    if (xjwt_encode("{invalid}", "secret") != NULL || xjwt_error()[0] == '\0') {
        result = 7; goto cleanup;
    }
    empty = xjwt_encode("{}", "secret");
    if (empty == NULL || xjwt_error()[0] != '\0') { result = 8; goto cleanup; }
    if (xjwt_verify(arrayToken, "secret") || xjwt_error()[0] == '\0') {
        result = 9; goto cleanup;
    }
    for (i = 0u; i < 16u; ++i) {
        char claims[64];
        snprintf(claims, sizeof(claims), "{\"index\":%u}", (unsigned)i);
        retained[i] = xjwt_encode(claims, "secret");
        if (retained[i] == NULL) { result = 10; goto cleanup; }
    }
    if (strcmp(retained[0], retained[15]) == 0 || !xjwt_verify(retained[0], "secret")) {
        result = 11; goto cleanup;
    }
    xrtFree(token);
    token = NULL;
    if (strcmp(decoded, "{\"sub\":\"xlang\"}") != 0 ||
        strstr(header, "HS256") == NULL) { result = 12; goto cleanup; }
    puts("jwt c smoke ok");

cleanup:
    xrtFree(token);
    xrtFree(decoded);
    xrtFree(header);
    xrtFree(empty);
    for (i = 0u; i < 16u; ++i) xrtFree(retained[i]);
    return result;
}

#include "../jwt_native.c"
