#define XRT_MODULE_ALL
#include "../../release/extlibs/xhttp.protocol/include/xhttp_protocol_package.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void* xhttp_protocol_headers_create(void);
bool xhttp_protocol_headers_write(void*, const char*, size_t, const char*, size_t, bool);
void* xhttp_protocol_headers_build(void*);
void* xhttp_protocol_headers_build_owned(void*);
void xhttp_protocol_headers_destroy_abi(void*);

int main(void)
{
    xmemdebugsnapshot baseline, before, direct, legacy, after;
    assert(xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&baseline);
    void* headers = xhttp_protocol_headers_create();
    assert(headers != NULL);
    assert(xhttp_protocol_headers_write(headers, "X-Test", 6, "hello", 5, false));
    xrtMemDebugSnapshot(&before);
    char* text = xhttp_protocol_headers_build_owned(headers);
    assert(text != NULL && strcmp(text, "X-Test: hello\r\n\r\n") == 0);
    xrtMemDebugSnapshot(&direct);
    xrtFree(text);
    xmemdebugsnapshot oldBefore;
    xrtMemDebugSnapshot(&oldBefore);
    xbuffer* buffer = xhttp_protocol_headers_build(headers);
    assert(buffer != NULL && buffer->Size == 17);
    assert(memcmp(buffer->Data, "X-Test: hello\r\n\r\n", 17) == 0);
    xrtMemDebugSnapshot(&legacy);
    assert(direct.AllocCount - before.AllocCount < legacy.AllocCount - oldBefore.AllocCount);
    xrtBufferDestroy(buffer);
    xhttp_protocol_headers_destroy_abi(headers);
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == baseline.LiveCount && after.LiveBytes == baseline.LiveBytes);
    assert(after.DoubleFreeCount == 0 && after.InvalidFreeCount == 0);
    puts("HTTP owned text: fewer allocations, compatible legacy buffer and balanced lifetime");
    return 0;
}
