#include "../include/dns_native.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    char* encoded = xdns_encode_name("example.com");
    xvalue* addresses = xdns_resolve("localhost", 4);
    int available = addresses != NULL && xrtValueCount(addresses) > 0u;
    if (encoded == NULL || !available) {
        xrtFree(encoded);
        xrtValueRelease(addresses);
        return 1;
    }
    printf("type A: %s\n", xdns_type_name(1));
    printf("class IN: %s\n", xdns_class_name(1));
    printf("rcode 3: %s\n", xdns_rcode_name(3));
    printf("opcode 5: %s\n", xdns_opcode_name(5));
    printf("example.com: %s\n", encoded);
    printf("valid example.com: %lld\n", (long long)xdns_valid_name("example.com"));
    printf("valid bad..name: %lld\n", (long long)xdns_valid_name("bad..name"));
    printf("IPv4 127.0.0.1: %lld\n", (long long)xdns_is_ipv4("127.0.0.1"));
    printf("IPv4 invalid: %lld\n", (long long)xdns_is_ipv4("256.1.1.1"));
    printf("localhost IPv4: %s\n", available ? "yes" : "no");
    xrtFree(encoded);
    xrtValueRelease(addresses);
    return 0;
}
