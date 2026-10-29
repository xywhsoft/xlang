#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ATOMIC
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void owned_capture_prepare(void);
void owned_capture_finish(void);
void owned_capture_cycle(int);
unsigned owned_capture_faults(void);
unsigned owned_capture_suspensions(void);
static xatomic32 dropCount = {0}, bodyCount = {0};
void owned_capture_record_drop(void) { (void)xrtAtomic32FetchAdd(&dropCount, 1, XMEMORY_RELAXED); }
void owned_capture_record_body(void) { (void)xrtAtomic32FetchAdd(&bodyCount, 1, XMEMORY_RELAXED); }
unsigned owned_capture_drop_count(void) { return xrtAtomic32Load(&dropCount, XMEMORY_RELAXED); }
int main(int argc, char** argv)
{
    xmemdebugsnapshot initial, before, after;
    unsigned faultStart, suspensionStart; int i;
    int smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    owned_capture_prepare(); owned_capture_cycle(smoke);
    if (!smoke) xrtMemDebugSnapshot(&before);
    faultStart = owned_capture_faults(); suspensionStart = owned_capture_suspensions();
    for (i = 0; i < 50; ++i) owned_capture_cycle(smoke);
    assert(owned_capture_suspensions() - suspensionStart == 300);
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        if (before.LiveCount != after.LiveCount || before.LiveBytes != after.LiveBytes)
            fprintf(stderr, "owned capture live: %llu/%llu -> %llu/%llu\n",
                (unsigned long long)before.LiveCount, (unsigned long long)before.LiveBytes,
                (unsigned long long)after.LiveCount, (unsigned long long)after.LiveBytes);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(owned_capture_faults() - faultStart == 1100);
        printf("owned activation: %llu balanced allocations, 1100 real allocation failures with/without raising destructors\n",
            (unsigned long long)(after.AllocCount - before.AllocCount));
    }
    owned_capture_finish();
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(initial.LiveCount == after.LiveCount && initial.LiveBytes == after.LiveBytes);
        assert(initial.InvalidFreeCount == after.InvalidFreeCount && initial.DoubleFreeCount == after.DoubleFreeCount);
        assert(initial.UseAfterFreeCount == after.UseAfterFreeCount);
    }
    puts("owned activation: 300 real suspensions, ready/pool/generator, primary errors, input cancel/close, generated module pool teardown");
    return 0;
}
