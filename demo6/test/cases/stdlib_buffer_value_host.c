#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct Fault { size_t calls, fail; bool hit; } Fault;
static void* closing;
static bool reentered;
static void* allocate(void* context, size_t size)
{
    Fault* fault = context;
    if (++fault->calls >= fault->fail) { fault->hit = true; return NULL; }
    return malloc(size);
}
static void* resize(void* context, void* pointer, size_t size)
{
    Fault* fault = context;
    if (++fault->calls >= fault->fail) { fault->hit = true; return NULL; }
    return realloc(pointer, size);
}
static void release(void* context, void* pointer)
{
    (void)context;
    if (closing && !reentered) {
        reentered = true;
        xl6BufferIteratorCloseV1(closing);
        assert(xl6BufferIteratorClosedV1(closing));
    }
    free(pointer);
}
static void equal(const xbuffer* buffer, const unsigned char* data, size_t size)
{
    assert(buffer && buffer->Size == size);
    assert(!size || !memcmp(buffer->Data, data, size));
}
static void textEqual(xstrview text, xstrview expected)
{
    assert(text.Data && text.Size == expected.Size && text.Data[text.Size] == 0);
    assert(!text.Size || !memcmp(text.Data, expected.Data, text.Size));
    xrtFree((void*)text.Data);
}
static void memoryFault(const char* operation, size_t point)
{
    static Fault fault;
    fault.fail = SIZE_MAX;
    xallocator allocator = {&fault, allocate, resize, release};
    assert(xrtSetAllocator(&allocator));
    char data[8192]; memset(data, 'a', sizeof(data));
    xbuffer* target = xl6BufferFromTextV1((xstrview){data, 4096}, -1);
    assert(target);
    if (!strcmp(operation, "trim")) assert(xrtBufferReserve(target, 16384));
    xvalue* input = xrtValueBytes((xbytesview){(unsigned char*)data, sizeof(data)});
    assert(input);
    unsigned char* previousData = target->Data;
    size_t previousSize = target->Size, previousCapacity = target->Capacity;
    void* result = NULL;
    xvalue* value = NULL;
    xstrview text = {NULL, 0};
    bool ok = false;
    fault.calls = 0; fault.fail = point;
    if (!strcmp(operation, "from-text")) result = xl6BufferFromTextV1((xstrview){data,sizeof(data)}, -1);
    else if (!strcmp(operation, "from-bytes")) result = xl6BufferFromBytesV1(input, -1);
    else if (!strcmp(operation, "copy")) result = xl6BufferCopyV1(target, -1);
    else if (!strcmp(operation, "assign")) ok = xl6BufferAssignBytesV1(target, input, -1);
    else if (!strcmp(operation, "append")) ok = xl6BufferAppendBytesV1(target, input, -1);
    else if (!strcmp(operation, "insert")) ok = xl6BufferInsertBytesV1(target, 2, input, -1);
    else if (!strcmp(operation, "self-assign")) ok = xl6BufferAssignBufferV1(target, target, -1);
    else if (!strcmp(operation, "self-append")) ok = xl6BufferAppendBufferV1(target, target, -1);
    else if (!strcmp(operation, "self-insert")) ok = xl6BufferInsertBufferV1(target, 2, target, -1);
    else if (!strcmp(operation, "resize")) ok = xl6BufferResizeV1(target, 16384, 65);
    else if (!strcmp(operation, "trim")) ok = xl6BufferTrimV1(target);
    else if (!strcmp(operation, "bytes")) value = xl6BufferBytesV1(target, -1);
    else if (!strcmp(operation, "text")) text = xl6BufferTextV1(target, -1);
    else if (!strcmp(operation, "iterator")) result = xl6BufferIteratorCreateV1(target, -1);
    else if (!strcmp(operation, "take")) {
        /* Create the payload before the measured managed-handle transaction. */
        fault.fail = SIZE_MAX;
        result = xl6BufferIteratorCreateV1(target, -1);
        assert(result);
        fault.calls = 0; fault.fail = point;
        value = xl6NativeHandleTakeV1(&result, xl6BufferIteratorHandlePolicyV1Get());
        assert(!result); /* Valid Take consumes on success and failure. */
    } else abort();
    if (fault.hit) {
        assert(!result && !value && !text.Data && !ok);
        assert(xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        assert(target->Data == previousData && target->Size == previousSize && target->Capacity == previousCapacity);
        equal(target, (unsigned char*)data, 4096);
    } else {
        assert(result || value || text.Data || ok);
        if (!strcmp(operation, "resize")) {
            assert(target->Size == 16384);
            for (size_t i=4096; i<target->Size; ++i) assert(target->Data[i] == 65);
        }
    }
    bool hit = fault.hit;
    size_t calls = fault.calls;
    fault.fail = SIZE_MAX;
    if (result) {
        if (!strcmp(operation, "iterator")) xl6BufferIteratorDestroyV1(result);
        else xrtBufferDestroy(result);
    }
    xrtFree((void*)text.Data);
    xrtValueRelease(value); xrtValueRelease(input); xrtBufferDestroy(target);
    xrtClearError();
    assert(xrtMemDebugReset());
    printf("buffer %s point=%zu calls=%zu hit=%d balanced\n", operation, point, calls, hit);
}

typedef struct Worker { void* iterator; bool destroy; } Worker;
#if defined(_WIN32)
static DWORD WINAPI crossThread(void* context)
#else
static void* crossThread(void* context)
#endif
{
    Worker* worker = context;
    if (worker->destroy) xl6BufferIteratorDestroyV1(worker->iterator);
    else {
        assert(!xl6BufferIteratorHasNextV1(worker->iterator) && xrtErrorKind(xrtGetError()) == XERR_STATE);
        xrtClearError();
        assert(xl6BufferIteratorNextV1(worker->iterator) == 0 && xrtErrorKind(xrtGetError()) == XERR_STATE);
        xrtClearError();
        xl6BufferIteratorCloseV1(worker->iterator);
        assert(xrtErrorKind(xrtGetError()) == XERR_STATE); xrtClearError();
        assert(!xl6BufferIteratorClosedV1(worker->iterator) && xrtErrorKind(xrtGetError()) == XERR_STATE);
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
    if (argc == 3) { memoryFault(argv[1], (size_t)strtoull(argv[2], NULL, 10)); return 0; }
    if (argc == 2 && !strcmp(argv[1], "reentry")) {
        static Fault fault; fault.fail = SIZE_MAX;
        xallocator allocator = {&fault, allocate, resize, release};
        assert(xrtSetAllocator(&allocator));
        assert(xrtMemDebugEnable(false)); /* Force the isolated real free hook. */
        char data[8192]; memset(data, 'a', sizeof(data));
        xbuffer* source = xl6BufferFromTextV1((xstrview){data, sizeof(data)}, -1);
        assert(source);
        closing = xl6BufferIteratorCreateV1(source, -1); assert(closing);
        xl6BufferIteratorCloseV1(closing);
        assert(reentered && xl6BufferIteratorClosedV1(closing));
        void* iterator = closing; closing = NULL;
        xl6BufferIteratorDestroyV1(iterator); xrtBufferDestroy(source);
        assert(xrtMemDebugReset());
        puts("buffer close reentry detached state, balanced"); return 0;
    }
    assert(argc == 1);
    /* A reserved descriptor must have storage even when its size is zero.
     * Otherwise native reserve can accept the capacity and write through NULL. */
    xbuffer malformed = {0};
    malformed.Capacity = 1024;
    assert(!xl6BufferAppendTextV1(&malformed, XRT_STR_LITERAL("x"), -1));
    assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(!malformed.Data && !malformed.Size && malformed.Capacity == 1024);
    xrtClearError();
    assert(!xl6BufferCopyV1(&malformed, -1));
    assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    xrtClearError();
    xbuffer* source = xl6BufferFromTextV1(XRT_STR_LITERAL("a\0你b"), -1);
    assert(source && source->Size == 6);
    textEqual(xl6BufferTextV1(source, 99), XRT_STR_LITERAL("a\0你b"));
    xbuffer* copy = xl6BufferCopyV1(source, -1); assert(copy);
    source->Data[0] = 'Z'; equal(copy, (unsigned char*)"a\0你b", 6);
    void* iterator = xl6BufferIteratorCreateV1(source, -1); assert(iterator);
    xrtBufferClear(source); xrtBufferDestroy(source); source = NULL;
    Worker worker = {iterator, false}; runWorker(&worker);
    const unsigned char expected[] = {'Z', 0, 0xE4, 0xBD, 0xA0, 'b'};
    for (size_t i=0; i<sizeof(expected); ++i) {
        assert(xl6BufferIteratorHasNextV1(iterator));
        assert(xl6BufferIteratorNextV1(iterator) == expected[i]);
    }
    assert(!xl6BufferIteratorHasNextV1(iterator));
    assert(xl6BufferIteratorNextV1(iterator) == 0 && xrtErrorKind(xrtGetError()) == XERR_STATE);
    xrtClearError();
    xl6BufferIteratorCloseV1(iterator); xl6BufferIteratorCloseV1(iterator);
    assert(xl6BufferIteratorClosedV1(iterator));
    xerror* first = xrtErrorCreate(XERR_VALUE, "test", 42, "first failure"); assert(first);
    xrtSetErrorTake(first);
    xl6BufferIteratorDestroyV1(iterator);
    assert(xrtGetError() == first); xrtClearError();
    iterator = xl6BufferIteratorCreateV1(copy, 2); assert(iterator);
    worker = (Worker){iterator, true}; runWorker(&worker);
    iterator = xl6BufferIteratorCreateV1(copy, -1); assert(iterator);
    XL6_NativeHandlePolicyV1 invalid = *xl6BufferIteratorHandlePolicyV1Get(); invalid.abiTag = 0;
    assert(!xl6NativeHandleTakeV1(&iterator, &invalid) && iterator);
    assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT); xrtClearError();
    xvalue* owned = xl6NativeHandleTakeV1(&iterator, xl6BufferIteratorHandlePolicyV1Get());
    assert(owned && !iterator && xrtValueType(owned) == XVALUE_HANDLE); xrtValueRelease(owned);
    assert(!xl6BufferFromTextV1((xstrview){NULL, 2}, 0) && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    xrtClearError();
    assert(!xl6BufferAssignTextV1(copy, XRT_STR_LITERAL("x"), -2) && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtClearError();
    assert(!xl6BufferTextV1(copy, 3).Data && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtClearError();
    assert(!xl6BufferRemoveV1(copy, 1, 99) && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtClearError(); equal(copy, (unsigned char*)"a\0你b", 6);
    xrtBufferDestroy(copy); assert(xrtMemDebugReset());
    puts("buffer native values/snapshots/ranges/thread/first-error/Take balanced");
    return 0;
}
