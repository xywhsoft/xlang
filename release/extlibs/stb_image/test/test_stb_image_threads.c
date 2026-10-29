#define XRT_MODULE_THREAD
#define XRT_MODULE_FILE_WHOLE
#include "xrt.h"
#include "../include/stb_image_native.h"

#include <stdio.h>

typedef struct stbiworker {
    bytes Encoded;
    size_t Size;
    int Id;
} stbiworker;

static int worker(void* Context)
{
    stbiworker* Work = (stbiworker*)Context;
    int i;
    for (i = 0; i < 200; ++i) {
        void* Image = xstbi_load_memory(Work->Encoded, Work->Size, 4, 0);
        int Ok = Image != NULL && xstbi_image_width(Image) == 16 &&
            xstbi_image_height(Image) == 16 &&
            xstbi_image_byte_size(Image) == 1024u;
        xstbi_image_release(Image);
        if (!Ok) return Work->Id;
    }
    return 0;
}

int main(void)
{
    size_t Size = 0u;
    bytes Encoded = xrtFileReadAllLimit(
        "release/examples/extlibs/assets/tiny.png", 128u * 1024u * 1024u, &Size);
    stbiworker Workers[4];
    xthread* Threads[4] = { NULL, NULL, NULL, NULL };
    int i;
    int Ok = Encoded != NULL;

    if (!Ok) return 2;
    for (i = 0; i < 4; ++i) {
        Workers[i].Encoded = Encoded;
        Workers[i].Size = Size;
        Workers[i].Id = i + 1;
        Threads[i] = xrtThreadCreate(worker, &Workers[i], 0);
        if (Threads[i] == NULL) Ok = 0;
    }
    for (i = 0; i < 4; ++i) {
        if (Threads[i] == NULL) continue;
        if (xrtThreadWait(Threads[i]) != XWAIT_OK ||
            xrtThreadExitCode(Threads[i]) != 0) Ok = 0;
        xrtThreadDestroy(Threads[i]);
    }
    xrtFree(Encoded);
    if (Ok) puts("stb_image threads: PASS");
    return Ok ? 0 : 2;
}
