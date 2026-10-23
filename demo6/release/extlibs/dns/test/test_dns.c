/* DNS 扩展库的 C 侧 smoke：协议工具与 XRT localhost 解析。 */
#define XRT_MODULE_BUFFER
#define XRT_MODULE_CODEC_HEX
#define XRT_MODULE_NET_DNS
#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"
#include <stdio.h>
#include <string.h>

const char* xdns_encode_name(const char* sDomain);
long long xdns_valid_name(const char* sName);
long long xdns_is_ipv4(const char* sAddress);
xvalue* xdns_resolve(const char* sHost, long long iFamily);

int main(void)
{
    xvalue* pAddresses = xdns_resolve("localhost", 4);
    size_t i;
    int bOk =
        strcmp(xdns_encode_name("example.com"),
            "076578616d706c6503636f6d00") == 0 &&
        xdns_valid_name("example.com.") == 1 &&
        xdns_valid_name("bad..name") == 0 &&
        xdns_is_ipv4("127.0.0.1") == 1 &&
        xrtValueCount(pAddresses) > 0u;

    for (i = 0u; bOk && i < xrtValueCount(pAddresses); ++i) {
        xstrview address;
        bOk = xrtValueGetString(xrtValueArrayGet(pAddresses, i), &address) &&
            address.Size > 0u && address.Data[0] != '\0';
    }

    xrtValueRelease(pAddresses);
    if (bOk) { puts("test_dns: PASS"); }
    return bOk ? 0 : 1;
}

#include "../dns_native.c"
