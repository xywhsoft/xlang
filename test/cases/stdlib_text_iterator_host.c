#include "src/runtime/xl6_text_value.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif
typedef struct Fault {size_t calls, fail; bool hit;} Fault;
static void* closingHandle;
static bool reentered;
static void* allocate(void* context, size_t size)
{
    Fault* f = context;
    if (++f->calls >= f->fail) {f->hit = true; return NULL;}
    return malloc(size);
}
static void* resize(void* context, void* pointer, size_t size)
{
    Fault* f = context;
    if (++f->calls >= f->fail) {f->hit = true; return NULL;}
    return realloc(pointer, size);
}
static void release(void* context, void* pointer)
{
    (void)context;
    if (closingHandle && !reentered) {
        reentered = true;
        xl6TextIteratorCloseV1(closingHandle);
    }
    free(pointer);
}
static void equal(xstrview actual, xstrview expected)
{
    assert(actual.Data && actual.Size == expected.Size && actual.Data[actual.Size] == 0);
    assert(!actual.Size || !memcmp(actual.Data, expected.Data, actual.Size));
    xrtFree((void*)actual.Data);
}
static void memoryFault(unsigned mode, size_t point)
{
    static Fault fault;
    fault.fail = SIZE_MAX;
    xallocator allocator = {&fault, allocate, resize, release};
    assert(xrtSetAllocator(&allocator));
    char data[8192], separator[4096];
    memset(data, 'a', sizeof(data)); memset(separator, '|', sizeof(separator));
    fault.calls = 0; fault.fail = point;
    void* it = xl6TextIteratorCreateV1((xstrview){data, sizeof(data)},
        (xstrview){separator, sizeof(separator)}, mode, -1, -1);
    if (!it) assert(fault.hit && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    else {
        xstrview item = xl6TextIteratorNextV1(it);
        if (item.Data) equal(item, (xstrview){data, sizeof(data)});
        else {
            assert(fault.hit && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            fault.fail = SIZE_MAX; xrtClearError();
            /* A failed next must not advance the cursor. */
            equal(xl6TextIteratorNextV1(it), (xstrview){data, sizeof(data)});
        }
        fault.fail = SIZE_MAX;
        assert(!xl6TextIteratorNextV1(it).Data && xl6TextIteratorClosedV1(it));
        xl6TextIteratorDestroyV1(it);
    }
    xrtClearError(); assert(xrtMemDebugReset());
    printf("iterator-%u allocation point=%zu calls=%zu hit=%d balanced\n", mode, point, fault.calls, fault.hit);
}
typedef struct Worker {void* handle; bool destroy;} Worker;
#if defined(_WIN32)
static DWORD WINAPI crossThread(void* context)
#else
static void* crossThread(void* context)
#endif
{
    Worker* worker = context;
    if (worker->destroy) xl6TextIteratorDestroyV1(worker->handle);
    else {
        assert(!xl6TextIteratorNextV1(worker->handle).Data && xrtErrorKind(xrtGetError()) == XERR_STATE);
        xrtClearError();
        xl6TextIteratorCloseV1(worker->handle); assert(xrtErrorKind(xrtGetError()) == XERR_STATE);
        xrtClearError();
        assert(!xl6TextIteratorClosedV1(worker->handle) && xrtErrorKind(xrtGetError()) == XERR_STATE);
        xrtClearError();
    }
    return 0;
}
static void runWorker(Worker* worker)
{
#if defined(_WIN32)
    HANDLE thread = CreateThread(NULL, 0, crossThread, worker, 0, NULL);
    assert(thread && WaitForSingleObject(thread, INFINITE) == WAIT_OBJECT_0);
    assert(CloseHandle(thread));
#else
    pthread_t thread;
    assert(!pthread_create(&thread, NULL, crossThread, worker));
    assert(!pthread_join(thread, NULL));
#endif
}
int main(int argc, char** argv)
{
    if (argc == 2 && !strcmp(argv[1], "reentry")) {
        static Fault fault;
        fault.fail = SIZE_MAX;
        xallocator allocator = {&fault, allocate, resize, release};
        assert(xrtSetAllocator(&allocator));
        /* Debug quarantine intentionally defers backing frees. Disable it
         * only for this isolated reentry probe; fault sweeps stay tracked. */
        assert(xrtMemDebugEnable(false));
        char text[8192]; memset(text, 'a', sizeof(text));
        void* it = xl6TextIteratorCreateV1((xstrview){text,sizeof(text)}, XRT_STR_LITERAL("|"), 0, -1, -1);
        assert(it); closingHandle = it;
        xl6TextIteratorCloseV1(it);
        closingHandle = NULL;
        assert(reentered && xl6TextIteratorClosedV1(it));
        xl6TextIteratorDestroyV1(it); assert(xrtMemDebugReset());
        puts("text iterator: allocator reentry observes detached closed state, no double release");
        return 0;
    }
    if (argc == 3) {memoryFault((unsigned)strtoul(argv[1], NULL, 10), (size_t)strtoull(argv[2], NULL, 10)); return 0;}
    assert(argc == 1);
    char data[] = "你\0|😀||a\0|", separator[] = "|";
    void* it = xl6TextIteratorCreateV1((xstrview){data,sizeof(data)-1}, (xstrview){separator,1}, 0, -1, -1);
    assert(it); memset(data, 'x', sizeof(data)-1); separator[0] = '!';
    Worker worker = {it, false}; runWorker(&worker);
    assert(!xl6TextIteratorClosedV1(it));
    equal(xl6TextIteratorNextV1(it), XRT_STR_LITERAL("你\0"));
    equal(xl6TextIteratorNextV1(it), XRT_STR_LITERAL("😀"));
    equal(xl6TextIteratorNextV1(it), XRT_STR_LITERAL(""));
    xstrview saved = xl6TextIteratorNextV1(it);
    xl6TextIteratorCloseV1(it); xl6TextIteratorCloseV1(it);
    assert(xl6TextIteratorClosedV1(it) && !xl6TextIteratorNextV1(it).Data);
    worker.destroy = true; runWorker(&worker);
    equal(saved, XRT_STR_LITERAL("a\0"));
    for (unsigned mode=0; mode<3; ++mode) {
        it = xl6TextIteratorCreateV1(XRT_STR_LITERAL(""), XRT_STR_LITERAL("|"), mode, -1, -1); assert(it);
        if (!mode) equal(xl6TextIteratorNextV1(it), XRT_STR_LITERAL(""));
        assert(!xl6TextIteratorNextV1(it).Data && xl6TextIteratorClosedV1(it));
        xl6TextIteratorDestroyV1(it);
    }
    it = xl6TextIteratorCreateV1(XRT_STR_LITERAL("a\0b\0"), XRT_STR_LITERAL("\0"), 0, 100, 100); assert(it);
    equal(xl6TextIteratorNextV1(it), XRT_STR_LITERAL("a")); equal(xl6TextIteratorNextV1(it), XRT_STR_LITERAL("b"));
    equal(xl6TextIteratorNextV1(it), XRT_STR_LITERAL("")); assert(!xl6TextIteratorNextV1(it).Data);
    xrtSetErrorKind(XERR_IO); const xerror* pending = xrtGetError();
    xl6TextIteratorDestroyV1(it); xl6TextIteratorDestroyV1(NULL); assert(xrtGetError() == pending); xrtClearError();
    assert(!xl6TextIteratorCreateV1(XRT_STR_LITERAL("你"), XRT_STR_LITERAL("|"), 0, 1, -1) && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtClearError();
    assert(!xl6TextIteratorCreateV1((xstrview){NULL,5}, XRT_STR_LITERAL("|"), 0, 0, -1) && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    xrtClearError();
    assert(!xl6TextIteratorCreateV1(XRT_STR_LITERAL("a"), XRT_STR_LITERAL("|"), 3, -1, -1) && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtClearError(); assert(xrtMemDebugReset());
    puts("text iterator: snapshot/owned-items/NUL/EOF/early-close/thread-affinity/destruction balanced");
    return 0;
}
