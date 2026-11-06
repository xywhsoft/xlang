#define XRT_MODULE_THREAD
#include "xrt.h"
#include "../include/ansi_native.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    int id;
} ansi_worker;

static int run_worker(void *input) {
    ansi_worker *worker = (ansi_worker *)input;
    char expected[48];
    char *held;
    int result = 1;
    int i;

    snprintf(expected, sizeof(expected), "\x1b[%dm线程\x1b[0m", 31 + worker->id);
    held = (char *)xansi_fg("线程", 31 + worker->id);
    if (held == NULL || strcmp(held, expected) != 0) goto done;

    /* 各线程交错调用，已返回的文本和错误槽都不能被其他线程覆盖。 */
    for (i = 0; i < 1000; ++i) {
        char *text = (char *)xansi_bold("ok");
        int valid = text != NULL && strcmp(text, "\x1b[1mok\x1b[0m") == 0;
        xrtFree(text);
        if (!valid) goto done;

        xrtSetErrorInfo(XERR_STATE, "ansi.thread", worker->id + 1, "marker");
        xrtThreadYield();
        if (xrtErrorCode(xrtGetError()) != worker->id + 1) goto done;

        text = (char *)xansi_strip(held);
        valid = text != NULL && strcmp(text, "线程") == 0 &&
            xrtGetError() == NULL;
        xrtFree(text);
        if (!valid || xansi_display_width(held) != 4 ||
            xrtGetError() != NULL || strcmp(held, expected) != 0) goto done;
    }
    result = 0;
done:
    xrtFree(held);
    xrtClearError();
    return result;
}

int main(void) {
    ansi_worker workers[4];
    xthread *threads[4];
    int created = 0;
    int failed = 0;
    int i;

    for (i = 0; i < 4; ++i) {
        workers[i].id = i;
        threads[i] = xrtThreadCreate(run_worker, &workers[i], 0);
        if (threads[i] == NULL) {
            failed = 1;
            break;
        }
        created++;
    }
    for (i = 0; i < created; ++i) {
        if (xrtThreadWait(threads[i]) != XWAIT_OK ||
            xrtThreadExitCode(threads[i]) != 0) failed = 1;
        xrtThreadDestroy(threads[i]);
    }
    if (failed) {
        fputs("ansi thread isolation failed\n", stderr);
        return 1;
    }
    puts("ansi thread isolation: PASS");
    return 0;
}
