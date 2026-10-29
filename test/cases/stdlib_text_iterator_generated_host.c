/* The gate preincludes the generated module in this translation unit. */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
typedef struct IteratorFault {size_t calls, fail; bool hit;} IteratorFault;
static void* iteratorAllocate(void* context, size_t size)
{
    IteratorFault* f = context;
    if (++f->calls >= f->fail) {f->hit = true; return NULL;}
    return malloc(size);
}
static void* iteratorResize(void* context, void* pointer, size_t size)
{
    IteratorFault* f = context;
    if (++f->calls >= f->fail) {f->hit = true; return NULL;}
    return realloc(pointer, size);
}
static void iteratorRelease(void* context, void* pointer) {(void)context; free(pointer);}
int main(int argc, char** argv)
{
    static IteratorFault fault;
    fault.fail = SIZE_MAX;
    xallocator allocator = {&fault, iteratorAllocate, iteratorResize, iteratorRelease};
    assert(xrtSetAllocator(&allocator));
    assert(argc == 1 || argc == 2);
    size_t point = argc == 2 ? (size_t)strtoull(argv[1], NULL, 10) : SIZE_MAX;
    fault.fail = point;
    int initialized = xlir_module_init();
    if (!initialized) assert(fault.hit && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    else assert(!fault.hit);
    fault.fail = SIZE_MAX;
    xlir_module_fini(); xrtClearError();
    assert(xrtMemDebugReset());
    printf("iterator managed allocation point=%zu calls=%zu hit=%d balanced\n",point,fault.calls,fault.hit);
    return 0;
}
