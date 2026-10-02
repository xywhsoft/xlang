#include "src/runtime/xl6_text_value.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct Fault { size_t calls, fail; bool hit; } Fault;
static void* allocate(void* context, size_t size)
{
    Fault* fault = context;
    if (++fault->calls >= fault->fail) { fault->hit = true; return NULL; }
    return malloc(size);
}
static void* resize(void* context, void* data, size_t size)
{
    Fault* fault = context;
    if (++fault->calls >= fault->fail) { fault->hit = true; return NULL; }
    return realloc(data, size);
}
static void release(void* context, void* data) { (void)context; free(data); }
static void equal(xstrview actual, xstrview expected)
{
    assert(actual.Data && actual.Size == expected.Size && !actual.Data[actual.Size]);
    assert(!actual.Size || !memcmp(actual.Data, expected.Data, actual.Size));
    xrtFree((void*)actual.Data);
}
static void bytesEqual(const xvalue* actual, const unsigned char* expected, size_t size)
{
    xbytesview view;
    assert(actual && xrtValueGetBytes(actual, &view) && view.Size == size);
    assert(!size || !memcmp(view.Data, expected, size));
}
static void fault(unsigned mode, size_t point)
{
    static Fault state;
    xallocator allocator = {&state, allocate, resize, release};
    state.fail = SIZE_MAX; assert(xrtSetAllocator(&allocator));
    unsigned char source[8192]; memset(source, 'A', sizeof(source));
    xvalue* input = xrtValueBytes((xbytesview){source, sizeof(source)}); assert(input);
    state.calls = 0; state.fail = point;
    if (mode == 1 || mode == 4 || mode == 5) {
        xstrview result = mode == 1 ? xl6EncodingDecodeV1(input,1,0,1,-1) :
            mode == 4 ? xl6EncodingNameV1(5) : xl6EncodingRepairTextV1((xstrview){(const char*)source,sizeof(source)},-1);
        if (result.Data) { assert(!state.hit); xrtFree((void*)result.Data); }
        else assert(state.hit && !result.Size && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    } else {
        xvalue* result = mode == 0 ? xl6EncodingEncodeV1((xstrview){(const char*)source,sizeof(source)},4,true,0,-1) :
            mode == 2 ? xl6EncodingTranscodeV1(input,1,3,0,5,-1) : xl6EncodingBomBytesV1(5);
        if (result) { assert(!state.hit); xrtValueRelease(result); }
        else assert(state.hit && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    }
    state.fail = SIZE_MAX; xrtValueRelease(input); xrtClearError(); assert(xrtMemDebugReset());
    printf("encoding-%u allocation point=%zu calls=%zu hit=%d balanced\n", mode,point,state.calls,state.hit);
}
int main(int argc, char** argv)
{
    if (argc == 3) { fault((unsigned)strtoul(argv[1],NULL,10),strtoull(argv[2],NULL,10)); return 0; }
    static const char source[] = "A\0\xE4\xBD\xA0\xF0\x9F\x98\x80";
    xstrview text = {source,sizeof(source)-1};
    assert(xrtMemDebugReset());
    for (int64_t encoding=1;encoding<=5;++encoding) {
        for (int emit=0;emit<=1;++emit) {
            xvalue* data=xl6EncodingEncodeV1(text,encoding,emit,0,-1); assert(data);
            equal(xl6EncodingDecodeV1(data,encoding,0,1,-1),text);
            if (emit) { assert(xl6EncodingBomV1(data,-1) == encoding); assert(xl6EncodingBomSizeV1(data,-1) == (encoding==1 ? 3 : encoding<=3 ? 2 : 4)); }
            for (int64_t target=1;target<=5;++target) {
                xvalue* other=xl6EncodingTranscodeV1(data,encoding,target,0,5,-1); assert(other);
                equal(xl6EncodingDecodeV1(other,target,0,2,-1),text);
                xrtValueRelease(other);
            }
            xvalue* empty=xl6EncodingEncodeV1((xstrview){NULL,0},encoding,emit,0,-1); assert(empty);
            equal(xl6EncodingDecodeV1(empty,encoding,0,1,-1),(xstrview){"",0});
            xrtValueRelease(empty); xrtValueRelease(data);
        }
    }
    /* Explicit UTF-16 BOM followed by NUL must not be mistaken for UTF-32. */
    const unsigned char ambiguous[] = {0xFF,0xFE,0,0};
    xvalue* input=xrtValueBytes((xbytesview){ambiguous,sizeof(ambiguous)}); assert(input);
    equal(xl6EncodingDecodeV1(input,2,0,2,-1),(xstrview){"\0",1});
    equal(xl6EncodingDecodeV1(input,4,0,2,-1),(xstrview){"",0});
    equal(xl6EncodingDecodeV1(input,2,0,0,-1),(xstrview){"\xEF\xBB\xBF\0",4});
    assert(!xl6EncodingDecodeV1(input,2,0,3,-1).Data && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT); xrtClearError();
    assert(!xl6EncodingDecodeV1(input,1,1,1,-1).Data && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT); xrtClearError();
    xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
    for (int i=0;i<10000;++i) {
        assert(xl6EncodingUnitSizeV1(4) == 4);
        assert(xl6EncodingBomV1(input,-1) == 4 && xl6EncodingBomSizeV1(input,-1) == 4);
    }
    xrtMemDebugSnapshot(&after); assert(before.AllocCount == after.AllocCount);
    xrtValueRelease(input);
    const unsigned char invalid[][5] = {{0xE4,0xBD,0,0,0},{0xC0,0xAF,0,0,0},{0,0xD8,'A',0,0},{0,0,0x11,0,0},{'A',0,0,0,0xFF}};
    const size_t lengths[] = {2,2,4,4,5};
    const int64_t encodings[] = {1,1,2,4,4};
    const xstrview replacements[] = {{"\xEF\xBF\xBD",3},{"\xEF\xBF\xBD\xEF\xBF\xBD",6},
        {"\xEF\xBF\xBD" "A",4},{"\xEF\xBF\xBD",3},{"A\xEF\xBF\xBD",4}};
    for (size_t i=0;i<5;++i) {
        input=xrtValueBytes((xbytesview){invalid[i],lengths[i]}); assert(input);
        assert(!xl6EncodingDecodeV1(input,encodings[i],0,0,-1).Data && xrtErrorKind(xrtGetError()) == XERR_VALUE); xrtClearError();
        equal(xl6EncodingDecodeV1(input,encodings[i],1,0,-1),replacements[i]);
        xrtValueRelease(input);
    }
    input=xrtValueBytes((xbytesview){(const unsigned char*)source,text.Size}); assert(input);
    equal(xl6EncodingDecodeV1(input,1,0,0,1000),text);
    equal(xl6EncodingDecodeV1(input,1,1,0,3),(xstrview){"A\0\xEF\xBF\xBD",5});
    assert(!xl6EncodingDecodeV1(input,1,0,0,-2).Data && xrtErrorKind(xrtGetError()) == XERR_RANGE); xrtClearError();
    assert(!xl6EncodingDecodeV1(input,1,2,0,-1).Data && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT); xrtClearError();
    assert(!xl6EncodingDecodeV1(input,4294967297LL,0,0,-1).Data && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT); xrtClearError();
    assert(!xl6EncodingTranscodeV1(input,1,1,0,8,-1) && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT); xrtClearError();
    assert(!xl6EncodingDecodeV1(NULL,1,0,0,-1).Data && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT); xrtClearError();
    xrtValueRelease(input);
    xvalue* bom=xl6EncodingBomBytesV1(1); bytesEqual(bom,(const unsigned char*)"\xEF\xBB\xBF",3); xrtValueRelease(bom);
    equal(xl6EncodingRepairTextV1((xstrview){"A\xFF\0",3},-1),(xstrview){"A\xEF\xBF\xBD\0",5});
    equal(xl6EncodingNameV1(0),(xstrview){"unknown",7});
    assert(!xl6EncodingNameV1(-1).Data && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT); xrtClearError();
    assert(xrtMemDebugReset());
    puts("encoding native: 5x5/BOM exact NUL, surrogate/endian replacement, ambiguous BOM, cap/options, allocation-free queries passed");
    return 0;
}
