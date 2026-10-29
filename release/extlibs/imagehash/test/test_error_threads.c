#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/imagehash_native.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    xevent *ready;
    xevent *resume;
} error_case;

static int fail(const char *message) {
    fprintf(stderr, "imagehash thread test: %s\n", message);
    return 1;
}

static int worker(void *context) {
    error_case *state = (error_case *)context;
    if (xihash_hamming("bad", 3, "0000000000000000", 16) != -1 ||
        strcmp(xihash_last_error(), "invalid hash string") != 0) return 2;
    if (!xrtEventSet(state->ready) ||
        xrtEventWaitFor(state->resume, 5000000) != XWAIT_OK) return 3;
    return strcmp(xihash_last_error(), "invalid hash string") == 0 ? 0 : 4;
}

static int phash_worker(void *context) {
    char *hash;
    int valid;
    if (xrtEventWaitFor((xevent *)context, 5000000) != XWAIT_OK) return 2;
    hash = xihash_phash("release/examples/extlibs/assets/tiny.png");
    valid = hash != NULL && strcmp(hash, "0000000000000000") == 0;
    xrtFree(hash);
    return valid ? 0 : 3;
}

static int test_phash_first_use(void) {
    xthread *threads[8] = {0};
    xevent *start = xrtEventCreate(true, false);
    int result = 0, i;
    if (start == NULL) return fail("pHash start event");
    for (i = 0; i < 8; ++i) {
        threads[i] = xrtThreadCreate(phash_worker, start, 0);
        if (threads[i] == NULL) {
            result = fail("pHash thread creation");
            break;
        }
    }
    xrtEventSet(start);
    for (i = 0; i < 8; ++i) {
        if (threads[i] == NULL) continue;
        if (xrtThreadWait(threads[i]) != XWAIT_OK ||
            xrtThreadExitCode(threads[i]) != 0) result = fail("concurrent first pHash");
        xrtThreadDestroy(threads[i]);
    }
    xrtEventDestroy(start);
    return result;
}

int main(void) {
    error_case state = {xrtEventCreate(false, false), xrtEventCreate(false, false)};
    xthread *thread;
    int result = 0;
    if (test_phash_first_use() != 0) return 1;
    if (state.ready == NULL || state.resume == NULL) return fail("event creation");
    thread = xrtThreadCreate(worker, &state, 0);
    if (thread == NULL) return fail("thread creation");
    if (xrtEventWaitFor(state.ready, 5000000) != XWAIT_OK) result = fail("worker ready");
    if (xihash_hamming("0000000000000000", 16,
            "0000000000000000", 16) != 0 ||
        strcmp(xihash_last_error(), "") != 0) result = fail("main-thread success");
    xrtEventSet(state.resume);
    if (xrtThreadWait(thread) != XWAIT_OK || xrtThreadExitCode(thread) != 0)
        result = fail("worker error was overwritten by another thread");
    xrtThreadDestroy(thread);
    xrtEventDestroy(state.resume);
    xrtEventDestroy(state.ready);
    if (result == 0) puts("imagehash thread tests: PASS");
    return result;
}
