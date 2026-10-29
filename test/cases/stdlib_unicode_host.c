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
static void* allocate(void* context, size_t size)
{
    Fault* fault = context;
    if (++fault->calls >= fault->fail) {fault->hit = true; return NULL;}
    return malloc(size);
}
static void* resize(void* context, void* pointer, size_t size)
{
    Fault* fault = context;
    if (++fault->calls >= fault->fail) {fault->hit = true; return NULL;}
    return realloc(pointer, size);
}
static void release(void* context, void* pointer) {(void)context; free(pointer);}
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
    char source[8192]; memset(source, '<', sizeof(source));
    if (mode == 4) source[sizeof(source) - 1] = (char)0xFF;
    fault.calls = 0; fault.fail = point;
    if (mode == 3) {
        void* handle = xl6UnicodeValidatorCreateV1();
        if (!handle) assert(fault.hit && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        else { assert(!fault.hit); xl6UnicodeValidatorDestroyV1(handle); }
    } else {
        xstrview result = mode == 0 ? xl6UnicodeCharStringV1(0x1F600) :
            mode == 2 ? xl6HtmlEscapeAttributeV1((xstrview){source,sizeof(source)},-1) :
            xl6HtmlEscapeTextV1((xstrview){source,sizeof(source)},-1);
        if (!result.Data) {
            if (fault.hit) assert(xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            else assert(mode == 4 && xrtErrorKind(xrtGetError()) == XERR_VALUE);
        } else { assert(!fault.hit && mode != 4); xrtFree((void*)result.Data); }
    }
    fault.fail = SIZE_MAX; xrtClearError(); assert(xrtMemDebugReset());
    printf("unicode-%u allocation point=%zu calls=%zu hit=%d balanced\n",mode,point,fault.calls,fault.hit);
}
typedef struct Worker {void* handle; const xvalue* data; bool destroy;} Worker;
#if defined(_WIN32)
static DWORD WINAPI worker(void* context)
#else
static void* worker(void* context)
#endif
{
    Worker* work = context;
    assert(xrtGetError() == NULL);
    if (work->destroy) xl6UnicodeValidatorDestroyV1(work->handle);
    else {
        assert(xl6UnicodeValidatorFeedV1(work->handle,work->data,false,-1) == XUTF_INVALID);
        assert(xrtErrorKind(xrtGetError()) == XERR_STATE); xrtClearError();
        xl6UnicodeValidatorResetV1(work->handle);
        assert(xrtErrorKind(xrtGetError()) == XERR_STATE); xrtClearError();
        assert(xl6UnicodeValidatorErrorV1(work->handle) == -1);
        assert(xrtErrorKind(xrtGetError()) == XERR_STATE); xrtClearError();
        assert(xl6UnicodeValidatorTotalV1(work->handle) == 0);
        assert(xrtErrorKind(xrtGetError()) == XERR_STATE); xrtClearError();
        assert(!xl6UnicodeValidatorFinishedV1(work->handle));
        assert(xrtErrorKind(xrtGetError()) == XERR_STATE); xrtClearError();
    }
    assert(xrtGetError() == NULL);
    return 0;
}
static void crossThread(Worker* work)
{
#if defined(_WIN32)
    HANDLE thread = CreateThread(NULL,0,worker,work,0,NULL);
    assert(thread); assert(WaitForSingleObject(thread,10000) == WAIT_OBJECT_0); CloseHandle(thread);
#else
    pthread_t thread; assert(!pthread_create(&thread,NULL,worker,work)); assert(!pthread_join(thread,NULL));
#endif
}
int main(int argc, char** argv)
{
    assert(xrtMemDebugEnable(true));
    if (argc == 3) { memoryFault((unsigned)strtoul(argv[1],NULL,10),(size_t)strtoull(argv[2],NULL,10)); return 0; }
    assert(argc == 1);
    const char source[] = "A\xE4\xBD\xA0\0\xF0\x9F\x98\x80";
    xstrview text = {source,sizeof(source)-1};
    static const int64_t offsets[] = {0,1,4,5,9};
    xmemdebugsnapshot before, after; xrtMemDebugSnapshot(&before);
    for (unsigned round=0;round<10000;++round) {
        assert(xl6UnicodeScalarV1(0) && xl6UnicodeScalarV1(0x10FFFF));
        assert(!xl6UnicodeScalarV1(-1) && !xl6UnicodeScalarV1(0xD800));
        assert(!xl6UnicodeScalarV1(INT64_MAX) && !xl6UnicodeScalarV1(INT64_MIN));
        assert(xl6UnicodeInvalidTextV1(text,-1) == -1);
        for (size_t i=0;i<sizeof(offsets)/sizeof(offsets[0]);++i) {
            assert(xl6UnicodeByteOffsetV1(text,(int64_t)i,-1) == offsets[i]);
            assert(xl6UnicodeScalarIndexV1(text,offsets[i],-1) == (int64_t)i);
        }
    }
    xrtMemDebugSnapshot(&after); assert(after.AllocCount == before.AllocCount);
    const uint32_t scalars[] = {0,0x7F,0x80,0x7FF,0x800,0xD7FF,0xE000,0xFFFF,0x10000,0x10FFFF};
    for (size_t i=0;i<sizeof(scalars)/sizeof(scalars[0]);++i) {
        xstrview encoded = xl6UnicodeCharStringV1(scalars[i]);
        uint32_t scalar=0; size_t read=0;
        assert(encoded.Data && xrtUtf8Decode(encoded,&scalar,&read) == XUTF_OK);
        assert(scalar == scalars[i] && read == encoded.Size); xrtFree((void*)encoded.Data);
    }
    equal(xl6HtmlEscapeTextV1((xstrview){"<\0>&",4},-1),(xstrview){"&lt;\0&gt;&amp;",14});
    equal(xl6HtmlEscapeAttributeV1((xstrview){"\"'",2},-1),(xstrview){"&quot;&#39;",11});
    equal(xl6HtmlEscapeTextV1((xstrview){NULL,0},-1),(xstrview){"",0});
    assert(!xl6UnicodeCharStringV1(0xD800).Data && xrtErrorKind(xrtGetError()) == XERR_VALUE); xrtClearError();
    assert(xl6UnicodeByteOffsetV1(text,INT64_MAX,-1) == -1 && xrtErrorKind(xrtGetError()) == XERR_RANGE); xrtClearError();
    assert(xl6UnicodeScalarIndexV1(text,2,-1) == -1 && xrtErrorKind(xrtGetError()) == XERR_RANGE); xrtClearError();
    assert(xl6UnicodeScalarIndexV1((xstrview){"a\xFF",2},0,-1) == -1 && xrtErrorKind(xrtGetError()) == XERR_VALUE); xrtClearError();
    assert(xl6UnicodeInvalidTextV1((xstrview){NULL,1},-1) == -1 && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT); xrtClearError();
    assert(xl6UnicodeInvalidTextV1(text,-2) == -1 && xrtErrorKind(xrtGetError()) == XERR_RANGE); xrtClearError();
    assert(xl6UnicodeInvalidTextV1(text,2) == 1 && !xrtGetError());
    for (size_t split=0;split<=text.Size;++split) {
        xvalue* first = xrtValueBytes((xbytesview){(const unsigned char*)source,split});
        xvalue* last = xrtValueBytes((xbytesview){(const unsigned char*)source+split,text.Size-split});
        assert(first && last);
        void* validator = xl6UnicodeValidatorCreateV1(); assert(validator);
        xrtMemDebugSnapshot(&before);
        int64_t status = xl6UnicodeValidatorFeedV1(validator,first,false,-1);
        assert(status == XUTF_OK || status == XUTF_MORE);
        assert(xl6UnicodeValidatorFeedV1(validator,last,true,-1) == XUTF_OK);
        assert(xl6UnicodeValidatorTotalV1(validator) == (int64_t)text.Size);
        assert(xl6UnicodeValidatorErrorV1(validator) == -1 && xl6UnicodeValidatorFinishedV1(validator));
        xl6UnicodeValidatorResetV1(validator); xrtMemDebugSnapshot(&after);
        assert(after.AllocCount == before.AllocCount);
        xl6UnicodeValidatorDestroyV1(validator); xrtValueRelease(first); xrtValueRelease(last);
    }
    const unsigned char invalid[][4] = {{0xC0,0xAF,0,0},{0xED,0xA0,0x80,0},{0xF4,0x90,0x80,0x80},{0xE4,0,0,0}};
    static const size_t invalidSize[] = {2,3,4,1};
    for (size_t i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) {
        for (size_t split=0;split<=invalidSize[i];++split) {
            xvalue* first=xrtValueBytes((xbytesview){invalid[i],split});
            xvalue* last=xrtValueBytes((xbytesview){invalid[i]+split,invalidSize[i]-split});
            void* validator=xl6UnicodeValidatorCreateV1(); assert(first && last && validator);
            assert(xl6UnicodeInvalidBytesV1(first,-1) == (split ? 0 : -1));
            (void)xl6UnicodeValidatorFeedV1(validator,first,false,-1);
            assert(xl6UnicodeValidatorFeedV1(validator,last,true,-1) == XUTF_INVALID);
            assert(xl6UnicodeValidatorErrorV1(validator) == 0 && !xrtGetError());
            xl6UnicodeValidatorDestroyV1(validator); xrtValueRelease(first); xrtValueRelease(last);
        }
    }
    xvalue* data=xrtValueBytes((xbytesview){(const unsigned char*)source,text.Size}); assert(data);
    void* validator=xl6UnicodeValidatorCreateV1(); assert(validator);
    assert(xl6UnicodeValidatorFeedV1(validator,data,false,2) == XUTF_MORE);
    Worker work={validator,data,false}; crossThread(&work);
    assert(xl6UnicodeValidatorTotalV1(validator) == 2 && !xrtGetError());
    work.destroy=true; crossThread(&work); xrtValueRelease(data);
    validator=xl6UnicodeValidatorCreateV1(); assert(validator);
    xerror* error=xrtErrorCreate(XERR_VALUE,"test.unicode",1,"retirement sentinel"); assert(error); xrtSetError(error);
    xl6UnicodeValidatorDestroyV1(validator); xl6UnicodeValidatorDestroyV1(NULL);
    assert(xrtGetError() == error); xrtErrorFree(error);
    xrtClearError(); assert(xrtMemDebugReset());
    puts("unicode/html native: exact NUL, all chunk boundaries, invalid sequences, allocation-free queries/feed, thread confinement and retirement passed");
    return 0;
}
