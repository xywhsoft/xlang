#define XRT_MODULE_ALL
#include <stdio.h>
#include <string.h>
#include "../include/jwt_native.h"

int main(void)
{
    const char* token = xjwt_encode("{\"sub\":\"xlang\"}", "secret");
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

    if ( token[0] == '\0' || !xjwt_verify(token, "secret") || xjwt_verify(token, "wrong") ) return 1;
    if ( strcmp(xjwt_decode(token), "{\"sub\":\"xlang\"}") != 0 ) return 2;
    if ( strstr(xjwt_header(token), "HS256") == NULL ) return 3;
    if ( !xjwt_verify(rfcToken, "your-256-bit-secret") ) return 4;
    if ( xjwt_verify(noneToken, "secret") || xjwt_error()[0] == '\0' ) return 5;
    if (xjwt_encode("[1,2]", "secret")[0] != '\0' || xjwt_error()[0] == '\0') return 6;
    if (xjwt_encode("{invalid}", "secret")[0] != '\0' || xjwt_error()[0] == '\0') return 7;
    if (xjwt_encode("{}", "secret")[0] == '\0' || xjwt_error()[0] != '\0') return 8;
    if (xjwt_verify(arrayToken, "secret") || xjwt_error()[0] == '\0') return 9;

    puts("jwt c smoke ok");
    return 0;
}

#include "../jwt_native.c"
