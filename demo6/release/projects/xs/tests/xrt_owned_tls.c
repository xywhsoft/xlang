#include "xrt.h"

#include <stdio.h>

int main(void)
{
    xwebserver* server;
    xneterror error;
    xnet_result result;

    server = xrtWebServerCreateHostTlsEx(
        NULL,
        "examples/fixtures/tls_cert.pem",
        "examples/fixtures/tls_key.pem",
        "127.0.0.1",
        0,
        128,
        1024 * 1024,
        1024 * 1024,
        2);
    if ( server == NULL ) {
        fprintf(stderr, "owned TLS server create failed\n");
        return 1;
    }
    result = xrtWebServerStart(server);
    if ( result != XRT_NET_OK ) {
        if ( xrtNetErrorGet(&error) ) {
            fprintf(stderr, "owned TLS start failed: result=%d phase=%u system=%d message=%s\n",
                (int)result, error.iPhase, error.iSystemError, error.sMessage);
        }
        else {
            fprintf(stderr, "owned TLS start failed without structured error\n");
        }
        xrtWebServerDestroy(server);
        return 2;
    }
    if ( xrtWebServerPort(server) == 0u ) {
        xrtWebServerDestroy(server);
        return 3;
    }
    xrtWebServerDestroy(server);
    puts("xs2 XRT owned TLS test ok");
    return 0;
}
