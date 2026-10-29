/* The gate preincludes the generated module in this translation unit. */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct IteratorFault {size_t calls, fail; bool hit;} IteratorFault;
static void* iteratorAllocate(void* context, size_t size)
{
    IteratorFault* fault = context;
    if (++fault->calls >= fault->fail) { fault->hit = true; return NULL; }
    return malloc(size);
}
static void* iteratorResize(void* context, void* pointer, size_t size)
{
    IteratorFault* fault = context;
    if (++fault->calls >= fault->fail) { fault->hit = true; return NULL; }
    return realloc(pointer, size);
}
static void iteratorRelease(void* context, void* pointer) {(void)context; free(pointer);}

int main(int argc, char** argv)
{
    static IteratorFault fault = {0, SIZE_MAX, false};
    assert(argc >= 1 && argc <= 3);
    bool physical = argc == 3 && !strcmp(argv[2], "allocator");
    assert(argc != 3 || physical);
    if (physical) {
        xallocator allocator = {&fault, iteratorAllocate, iteratorResize, iteratorRelease};
        assert(xrtSetAllocator(&allocator));
    }
    size_t point = argc >= 2 ? (size_t)strtoull(argv[1], NULL, 10) : SIZE_MAX;
    assert(point > 0);
    if (!physical) assert(xrtMemDebugEnable(true));
    fault.calls = 0;
    if (physical) fault.fail = point;
    else if (point != SIZE_MAX) assert(xrtMemDebugFailAfter(point - 1));
    int initialized = xlir_module_init();
    bool hit = physical ? fault.hit : xrtMemDebugFailTriggered();
    fault.fail = SIZE_MAX; xrtMemDebugFailClear();
    if (!initialized) assert(hit && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    else assert(!xrtGetError());
    /* Optional bookkeeping and caught failures may recover. The complete
     * program must still satisfy its assertions and finish without an error. */
    xrtClearError(); xlir_module_fini(); assert(!xrtGetError());
    assert(xrtMemDebugReset());
    printf("iterator %s allocation point=%zu recovered=%d hit=%d balanced\n",
        physical ? "allocator" : "logical",point,initialized && hit,hit);
    return 0;
}
