/* DNS 扩展库的 C 侧 smoke：协议工具与 XRT localhost 解析。 */
#define XRT_MODULE_BUFFER
#define XRT_MODULE_CODEC_HEX
#define XRT_MODULE_NET_DNS
#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"
#include <stdio.h>
#include <string.h>

char* xdns_encode_name(const char* sDomain);
long long xdns_valid_name(const char* sName);
long long xdns_is_ipv4(const char* sAddress);
xvalue* xdns_resolve(const char* sHost, long long iFamily);

int main(void)
{
    xvalue* pAddresses = xdns_resolve("localhost", 4);
    char* pEncoded = xdns_encode_name("example.com");
    char* pEncodedAgain = xdns_encode_name("a.io");
    char* pAfterError;
    size_t i;
    int bOk =
        pEncoded != NULL && pEncodedAgain != NULL &&
        strcmp(pEncoded, "076578616d706c6503636f6d00") == 0 &&
        strcmp(pEncodedAgain, "016102696f00") == 0 &&
        xdns_valid_name("example.com.") == 1 &&
        xdns_valid_name("bad..name") == 0 &&
        xdns_is_ipv4("127.0.0.1") == 1 &&
        xrtValueCount(pAddresses) > 0u;

    bOk = bOk && xdns_encode_name("bad..name") == NULL;
    pAfterError = xdns_encode_name("b.net");
    bOk = bOk && pAfterError != NULL &&
        strcmp(pAfterError, "0162036e657400") == 0 &&
        xrtErrorKind(xrtGetError()) == XERR_NONE;

    for (i = 0u; bOk && i < xrtValueCount(pAddresses); ++i) {
        xstrview address;
        bOk = xrtValueGetString(xrtValueArrayGet(pAddresses, i), &address) &&
            address.Size > 0u && address.Data[0] != '\0';
    }

    xrtFree(pEncoded);
    xrtFree(pEncodedAgain);
    xrtFree(pAfterError);
    xrtValueRelease(pAddresses);
    if (bOk) { puts("test_dns: PASS"); }
    return bOk ? 0 : 1;
}

#include "../dns_native.c"
