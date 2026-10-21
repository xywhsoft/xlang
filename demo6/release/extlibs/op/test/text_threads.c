#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "xrt.h"

#include "../include/op.h"

typedef struct {
    HANDLE ready[2];
    HANDLE release;
    const char *text[2];
} text_probe;

typedef struct {
    text_probe *probe;
    int index;
} text_worker;

static DWORD WINAPI read_text(void *argument)
{
    text_worker *worker = (text_worker *)argument;
    text_probe *probe = worker->probe;
    int index = worker->index;

    probe->text[index] = index == 0 ?
        xop_get_screen_size() : xop_get_cursor_pos();
    SetEvent(probe->ready[index]);
    return WaitForSingleObject(probe->release, 5000) == WAIT_OBJECT_0 ? 0 : 1;
}

int main(void)
{
    text_probe probe = {0};
    text_worker workers[2];
    HANDLE threads[2] = {NULL, NULL};
    int ok = 0;
    int i;

    probe.ready[0] = CreateEventA(NULL, TRUE, FALSE, NULL);
    probe.ready[1] = CreateEventA(NULL, TRUE, FALSE, NULL);
    probe.release = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!probe.ready[0] || !probe.ready[1] || !probe.release) goto cleanup;

    for (i = 0; i < 2; ++i) {
        workers[i].probe = &probe;
        workers[i].index = i;
        threads[i] = CreateThread(NULL, 0, read_text, &workers[i], 0, NULL);
        if (!threads[i]) goto cleanup;
    }
    if (WaitForMultipleObjects(2, probe.ready, TRUE, 5000) != WAIT_OBJECT_0)
        goto cleanup;
    ok = probe.text[0] && probe.text[1] &&
        probe.text[0] != probe.text[1] &&
        strchr(probe.text[0], 'x') && strchr(probe.text[1], ',');

cleanup:
    if (probe.release) SetEvent(probe.release);
    for (i = 0; i < 2; ++i) {
        if (threads[i]) {
            if (WaitForSingleObject(threads[i], 5000) != WAIT_OBJECT_0) ok = 0;
            CloseHandle(threads[i]);
        }
        if (probe.ready[i]) CloseHandle(probe.ready[i]);
    }
    if (probe.release) CloseHandle(probe.release);
    if (!ok) {
        fprintf(stderr, "op text thread isolation failed: %p=%s %p=%s\n",
                (const void *)probe.text[0], probe.text[0] ? probe.text[0] : "null",
                (const void *)probe.text[1], probe.text[1] ? probe.text[1] : "null");
        return 1;
    }
    xrtFree((void *)probe.text[0]);
    xrtFree((void *)probe.text[1]);
    puts("op text thread isolation: ok");
    return 0;
}
