#define XRT_MODULE_THREAD
#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"
#include "../include/redis_native.h"

#include <stdio.h>
#include <string.h>

static int worker(void* Context)
{
    int Id = *(int*)Context;
    xvalue* Input = xrtValueString(XRT_STR_LITERAL("$4\r\nPONG\r\n"));
    xvalue* Held = xr_decode(Input);
    int i;

    if (Input == NULL || Held == NULL) return Id;
    for (i = 0; i < 500; ++i) {
        xvalue* Reply = xr_decode(Input);
        xvalue* Value = xrtValueObjectGet(Reply, XRT_STR_LITERAL("value"));
        xvalue* Text = xrtValueObjectGet(Value, XRT_STR_LITERAL("value"));
        xstrview Bytes;
        bool Ok = Text != NULL && xrtValueGetString(Text, &Bytes) &&
            Bytes.Size == 4u && memcmp(Bytes.Data, "PONG", 4u) == 0 &&
            xr_is_complete(Input);
        xrtValueRelease(Reply);
        if (!Ok) {
            xrtValueRelease(Held);
            xrtValueRelease(Input);
            return Id;
        }
    }
    xrtValueRelease(Held);
    xrtValueRelease(Input);
    return 0;
}

int main(void)
{
    xthread* Threads[4] = { NULL, NULL, NULL, NULL };
    int Ids[4] = { 1, 2, 3, 4 };
    int i;
    int Ok = 1;

    for (i = 0; i < 4; ++i) {
        Threads[i] = xrtThreadCreate(worker, &Ids[i], 0);
        if (Threads[i] == NULL) Ok = 0;
    }
    for (i = 0; i < 4; ++i) {
        if (Threads[i] == NULL) continue;
        if (xrtThreadWait(Threads[i]) != XWAIT_OK ||
            xrtThreadExitCode(Threads[i]) != 0) Ok = 0;
        xrtThreadDestroy(Threads[i]);
    }
    if (Ok) puts("redis threads: PASS");
    return Ok ? 0 : 2;
}
