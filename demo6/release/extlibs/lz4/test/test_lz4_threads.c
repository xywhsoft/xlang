#define XRT_MODULE_THREAD
#define XRT_MODULE_ATOMIC
#include "../include/xrt_lz4.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct { xatomic32* Start; int Id; } worker;

static int run(ptr input)
{
    worker* w = input;
    uint8 data[1024];
    unsigned int i, round;
    for (i = 0; i < sizeof(data); ++i) data[i] = (uint8)(i * 17 + w->Id);
    while (!xrtAtomic32Load(w->Start, XMEMORY_ACQUIRE)) xrtThreadYield();
    for (round = 0; round < 250; ++round) {
        xbuffer *packed = NULL, *plain = NULL;
        assert(xrtLz4Compress(data, sizeof(data), NULL, &packed, NULL));
        assert(xrtLz4Decompress(packed->Data, packed->Size, NULL, &plain, NULL));
        assert(plain->Size == sizeof(data) && memcmp(data, plain->Data, sizeof(data)) == 0);
        xrtBufferDestroy(packed);
        xrtBufferDestroy(plain);
        xrtSetErrorInfo(XERR_STATE, "test.lz4.thread", w->Id, "thread-local error");
        xrtThreadYield();
        assert(xrtErrorCode(xrtGetError()) == w->Id);
    }
    xrtClearError();
    return 0;
}

int main(void)
{
    xatomic32 start;
    worker workers[4];
    xthread* threads[4];
    int i;
    xrtAtomic32Init(&start, 0);
    for (i = 0; i < 4; ++i) {
        workers[i] = (worker){&start, i + 1};
        threads[i] = xrtThreadCreate(run, &workers[i], 0);
        assert(threads[i]);
    }
    xrtSetErrorInfo(XERR_RANGE, "test.lz4.thread", 99, "parent");
    xrtAtomic32Store(&start, 1, XMEMORY_RELEASE);
    for (i = 0; i < 4; ++i) {
        assert(xrtThreadWait(threads[i]) == XWAIT_OK && xrtThreadExitCode(threads[i]) == 0);
        xrtThreadDestroy(threads[i]);
    }
    assert(xrtErrorCode(xrtGetError()) == 99);
    xrtClearError();
    puts("lz4 C threads: 4 workers, 1000 independent round trips and TLS passed");
    return 0;
}
