/* DNS 扩展库的 C 侧 smoke：协议工具与 XRT localhost 解析。 */
#define XRT_MODULE_BUFFER
#define XRT_MODULE_CODEC_HEX
#define XRT_MODULE_NET_DNS
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../include/dns_native.h"
#include <stdio.h>
#include <string.h>

static int test_allocation_errors(void)
{
    unsigned failure, failed = 0u, succeeded = 0u;
    for (failure = 0u; failure < 24u; ++failure) {
        char* text;
        int triggered;
        if (!xrtMemDebugFailAfter(failure)) return 0;
        text = xdns_encode_name("example.com");
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            if (text != NULL || xrtErrorKind(xrtGetError()) != XERR_MEMORY) {
                xrtFree(text);
                return 0;
            }
            ++failed;
        } else {
            if (text == NULL) return 0;
            ++succeeded;
        }
        xrtFree(text);
        text = xdns_encode_name("b.net");
        if (text == NULL || strcmp(text, "0162036e657400") != 0 ||
            xrtErrorKind(xrtGetError()) != XERR_NONE) {
            xrtFree(text);
            return 0;
        }
        xrtFree(text);
    }
    return failed > 0u && succeeded > 0u;
}

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
    bOk = bOk && xrtErrorKind(xrtGetError()) == XERR_VALUE;
    pAfterError = xdns_encode_name("b.net");
    bOk = bOk && pAfterError != NULL &&
        strcmp(pAfterError, "0162036e657400") == 0 &&
        xrtErrorKind(xrtGetError()) == XERR_NONE;
    bOk = bOk && xdns_is_ipv4("256.1.1.1") == 0 &&
        xrtErrorKind(xrtGetError()) == XERR_NONE &&
        xdns_is_ipv4("not.an.ip") == 0 &&
        xrtErrorKind(xrtGetError()) == XERR_NONE &&
        xdns_is_ipv4("::1") == 0 &&
        xrtErrorKind(xrtGetError()) == XERR_NONE &&
        strcmp(xdns_type_name(28), "AAAA") == 0 &&
        strcmp(xdns_class_name(1), "IN") == 0 &&
        strcmp(xdns_rcode_name(3), "NXDOMAIN") == 0 &&
        strcmp(xdns_opcode_name(5), "UPDATE") == 0;

    for (i = 0u; bOk && i < xrtValueCount(pAddresses); ++i) {
        xstrview address;
        bOk = xrtValueGetString(xrtValueArrayGet(pAddresses, i), &address) &&
            address.Size > 0u && address.Data[0] != '\0';
    }
    bOk = bOk && test_allocation_errors();

    xrtFree(pEncoded);
    xrtFree(pEncodedAgain);
    xrtFree(pAfterError);
    xrtValueRelease(pAddresses);
    if (bOk) { puts("test_dns: PASS"); }
    return bOk ? 0 : 1;
}
