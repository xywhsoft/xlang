/* Compare with the genuine backend initializer; never copy its body here. */
#include "src/runtime/xl6_tls_stream_options_internal.h"
#include "src/runtime/xl6_tls_stream_options_abi.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    xtlsstreamconfig native;xrtTlsStreamConfigInit(&native);
    const xtlsstreamconfig* language=xl6TlsStreamOptionsCreateV1();assert(language);
    assert(language->HandshakeTimeout==native.HandshakeTimeout && language->CloseTimeout==native.CloseTimeout &&
        language->AsyncBytesLimit==native.AsyncBytesLimit && language->AsyncCountLimit==native.AsyncCountLimit && language->AsyncBatch==native.AsyncBatch);
    xl6TlsStreamOptionsDestroyV1((void*)language);puts("TLS stream options defaults: genuine native initializer agrees");return 0;
}
