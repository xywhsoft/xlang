#define XRT_MODULE_BUFFER
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_RANDOM_SECURE
#include "xrt.h"
#include "../include/sharedmem_native.h"

/* 故障注入和资源清理检查不能随发布编译选项一起消失。 */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>



int main(void)
{
    char name[96];
    uint64_t nonce;
    xmemdebugsnapshot before;
    xmemdebugsnapshot after;
    void* owner;
    unsigned int failures = 0;
    unsigned int successes = 0;
    unsigned int i;

    assert(xrtSecureRandom(&nonce, sizeof(nonce)));
    assert(xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);

    /* 创建失败后不得留下可按名称打开的半成品。 */
    for (i = 0; i < 8; ++i) {
        void* handle;
        bool failed;
        snprintf(name, sizeof(name), "xlang_shm_alloc_%016llx_%u",
            (unsigned long long)nonce, i);
        assert(xrtMemDebugFailAfter(i));
        handle = xshm_create(name, 64);
        failed = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (handle == NULL) {
            assert(failed && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            ++failures;
        } else {
            assert(!failed);
            ++successes;
            xshm_close(handle);
        }
        assert(xshm_open(name) == NULL);
        assert(xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND);
        xrtClearError();
    }

    snprintf(name, sizeof(name), "xlang_shm_alloc_%016llx_live",
        (unsigned long long)nonce);
    owner = xshm_create(name, 64);
    assert(owner != NULL && xshm_write(owner, 0, "A\0B", 3));
    for (i = 0; i < 8; ++i) {
        void* peer;
        xbuffer* bytes;
        bool failed;

        assert(xrtMemDebugFailAfter(i));
        peer = xshm_open(name);
        failed = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (peer == NULL) {
            assert(failed && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            ++failures;
        } else {
            assert(!failed);
            ++successes;
            xshm_close(peer);
        }
        xrtClearError();

        assert(xrtMemDebugFailAfter(i));
        bytes = (xbuffer*)xshm_read(owner, 0, 3);
        failed = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (bytes == NULL) {
            assert(failed && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            ++failures;
        } else {
            assert(!failed && bytes->Size == 3);
            assert(memcmp(bytes->Data, "A\0B", 3) == 0);
            ++successes;
            xrtBufferDestroy(bytes);
        }
        xrtClearError();

        assert(xrtMemDebugFailAfter(i));
        {
            char* text = xshm_read_text(owner, 0, 3);
            failed = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (text == NULL) {
                assert(failed && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                ++failures;
            } else {
                assert(!failed && memcmp(text, "A\0B", 3) == 0 && text[3] == '\0');
                ++successes;
                xrtFree(text);
            }
        }
        xrtClearError();
    }

    {
        xbuffer* bytes = (xbuffer*)xshm_read(owner, 0, 3);
        assert(bytes != NULL && bytes->Size == 3);
        assert(memcmp(bytes->Data, "A\0B", 3) == 0);
        xrtBufferDestroy(bytes);
    }
    xshm_close(owner);
    assert(xshm_open(name) == NULL);
    assert(xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND);
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    assert(failures != 0 && successes != 0);
    assert(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);
    assert(after.InvalidFreeCount == before.InvalidFreeCount);
    assert(after.DoubleFreeCount == before.DoubleFreeCount);
    printf("sharedmem allocation checks: rejected=%u completed=%u\n",
        failures, successes);
    return 0;
}
