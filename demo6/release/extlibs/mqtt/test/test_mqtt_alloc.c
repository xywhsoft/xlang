#define XRT_MODULE_MEMORY_DEBUG
#include "../include/mqtt_native.h"
#include <assert.h>
#include <stdio.h>



static bool operation(unsigned int kind)
{
    static const char* frames[] = {
        "101800044d51545404c20000000464656d6f000175000300ff61",
        "3209000174000100ff6162", "820c00010003612f2b0000016201", "9006000900010280"
    };
    xbuffer* frame;
    xvalue* object;
    if (kind < 4) {
        frame = xrtBufferFromHex(xrtStrView(frames[kind]), 0);
        if (frame == NULL) return false;
        object = xrtMqttParse(xrtBufferView(frame));
        xrtBufferDestroy(frame);
        if (object == NULL) return false;
        xrtValueRelease(object);
        return true;
    }
    if (kind == 4) frame = xmq_connect_text("client", "user", "secret", 0, true);
    else if (kind == 5) frame = xmq_publish_text(1, "topic", "payload", 7, true, true);
    else frame = xmq_subscribe_frame(7, "home/+/value", 1);
    if (frame != NULL) {
        void* hex = xmq_frame_to_hex(frame->Data, frame->Size);
        xrtBufferDestroy(frame);
        if (hex == NULL) return false;
        xrtFree(hex);
        return true;
    }
    return false;
}



int main(void)
{
    xmemdebugsnapshot before, after;
    unsigned int kind, index, failed = 0, passed = 0;
    assert(xrtMemDebugEnable(true));
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    for (kind = 0; kind < 7; ++kind) {
        bool reached_success = false;
        for (index = 0; index < 128; ++index) {
            bool ok, injected;
            assert(xrtMemDebugFailAfter(index));
            ok = operation(kind);
            injected = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (ok) { assert(!injected); ++passed; reached_success = true; }
            else { assert(injected); ++failed; }
            xrtClearError();
            xrtMemDebugSnapshot(&after);
            assert(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);
            assert(after.InvalidFreeCount == before.InvalidFreeCount);
            assert(after.DoubleFreeCount == before.DoubleFreeCount);
            assert(after.OverflowCount == before.OverflowCount);
            assert(after.UnderflowCount == before.UnderflowCount);
        }
        assert(reached_success);
    }
    assert(failed != 0 && passed != 0);
    printf("mqtt allocation checks: rejected=%u completed=%u\n", failed, passed);
    return 0;
}
