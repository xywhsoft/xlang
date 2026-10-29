#include "src/runtime/xl6_text_value.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void refusal(xerrkind kind)
{
    assert(xrtErrorKind(xrtGetError()) == kind); xrtClearError();
}
static void equal(xbuffer* buffer, const void* data, size_t size)
{ assert(buffer && buffer->Size == size && (!size || !memcmp(buffer->Data, data, size))); }
static void memoryFault(const char* operation, size_t point)
{
    assert(xrtMemDebugEnable(true));
    xmemdebugsnapshot before, after; xrtMemDebugSnapshot(&before);
    xbuffer* target = xl6BufferCreateReserveV1(128); assert(target);
    unsigned char data[4096]; memset(data, 65, sizeof(data));
    assert(xrtBufferAppend(target, (xbytesview){data,128}));
    /* Exactly full: all append operations must grow, including alias paths. */
    assert(target->Capacity <= sizeof(data));
    assert(xrtBufferResize(target, target->Capacity));
    memset(target->Data, 65, target->Size);
    unsigned char original[4096]; memcpy(original, target->Data, target->Size);
    const unsigned char* previous = target->Data;
    size_t size = target->Size, capacity = target->Capacity;
    assert(xrtMemDebugFailAfter(point));
    bool ok = false; xbuffer* created = NULL;
    if (!strcmp(operation,"create")) ok = (created = xl6BufferCreateV1()) != NULL;
    else if (!strcmp(operation,"create-reserve")) ok = (created = xl6BufferCreateReserveV1(32768)) != NULL;
    else if (!strcmp(operation,"reserve")) ok = xl6BufferReserveV1(target,32768);
    else if (!strcmp(operation,"append-byte")) ok = xl6BufferAppendByteV1(target,255);
    else if (!strcmp(operation,"append-raw")) ok = xl6BufferAppendRawV1(target,data,sizeof(data));
    else if (!strcmp(operation,"append-alias")) ok = xl6BufferAppendRawV1(target,target->Data,(int64_t)size);
    else if (!strcmp(operation,"insert-alias")) ok = xl6BufferInsertRawV1(target,1,target->Data,(int64_t)size);
    else if (!strcmp(operation,"copy-gap")) ok = xl6BufferCopyFromV1(target,8192,target,0,-1);
    else abort();
    bool hit = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    assert(hit ? !ok && xrtErrorKind(xrtGetError()) == XERR_MEMORY : ok && !xrtGetError());
    if (hit) {
        assert(target->Data == previous && target->Size == size && target->Capacity == capacity);
        assert(!memcmp(target->Data,original,size));
    }
    const xerror* failure = xrtGetError();
    xl6BufferDestroyV1(created); xl6BufferDestroyV1(target);
    assert(xrtGetError() == failure); xrtClearError();
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
    assert(after.InvalidFreeCount == before.InvalidFreeCount && after.DoubleFreeCount == before.DoubleFreeCount);
    assert(after.UseAfterFreeCount == before.UseAfterFreeCount);
    printf("buffer core %s point=%zu hit=%d balanced\n",operation,point,hit);
}
int main(int argc, char** argv)
{
    if (argc == 3) { memoryFault(argv[1],(size_t)strtoull(argv[2],NULL,10)); return 0; }
    assert(argc == 1);
    const unsigned char data[] = {0,255,65,90};
    xbuffer* source = xl6BufferCreateV1(); assert(source);
    assert(xl6BufferAppendRawV1(source,(void*)data,sizeof(data)));
    assert(xl6BufferAdoptV1(source) == source && xl6BufferHandleV1(source) == source);
    assert(xl6BufferDataV1(source) == source->Data && xl6BufferLengthV1(source) == 4);
    assert(xl6BufferCapacityV1(source) >= 4 && xl6BufferByteAtV1(source,1) == 255);
    assert(xl6BufferCompareV1(source,source,INT64_MAX,-1) == 0);
    assert(xl6BufferIndexOfV1(source,source,0,99,2) == 0);
    xbuffer* empty = xl6BufferCreateV1(); assert(empty);
    assert(xl6BufferIndexOfV1(source,empty,4,-1,-1) == 4);
    assert(xl6BufferIndexOfV1(source,empty,5,-1,-1) == -1);
    assert(xl6BufferCopyFromV1(empty,3,source,1,2));
    const unsigned char gap[] = {0,0,0,255,65}; equal(empty,gap,sizeof(gap));
    assert(xl6BufferCopyWithinV1(empty,1,3,2));
    const unsigned char overlap[] = {0,255,65,255,65}; equal(empty,overlap,sizeof(overlap));
    assert(xl6BufferFillV1(empty,90,3,-1));
    assert(xl6BufferSetByteV1(empty,0,255)); assert(xl6BufferByteAtV1(empty,0) == 255);
    assert(xl6BufferCopyWithinV1(empty,5,5,0));
    size_t capacity = empty->Capacity;
    assert(xl6BufferClearV1(empty) && empty->Size == 0 && empty->Capacity == capacity);
    assert(xl6BufferAppendRawV1(empty,NULL,0) && xl6BufferFillV1(empty,0,0,0));
    assert(!xl6BufferCreateReserveV1(-1)); refusal(XERR_RANGE);
    assert(!xl6BufferReserveV1(source,-1)); refusal(XERR_RANGE);
    assert(!xl6BufferAppendRawV1(source,NULL,1)); refusal(XERR_ARGUMENT);
    assert(!xl6BufferInsertRawV1(source,-1,(void*)data,1)); refusal(XERR_RANGE);
    assert(!xl6BufferAppendByteV1(source,256)); refusal(XERR_VALUE);
    assert(!xl6BufferSetByteV1(source,4,0)); refusal(XERR_RANGE);
    assert(!xl6BufferByteAtV1(source,-1)); refusal(XERR_RANGE);
    assert(!xl6BufferCopyFromV1(empty,INT64_MAX,source,0,2)); refusal(XERR_RANGE);
    assert(!xl6BufferCopyFromV1(empty,0,source,1,4)); refusal(XERR_RANGE);
    assert(!xl6BufferCopyWithinV1(source,0,0,-1)); refusal(XERR_RANGE);
    assert(!xl6BufferFillV1(source,0,0,5)); refusal(XERR_RANGE);
    assert(!xl6BufferCompareV1(source,source,-2,-1)); refusal(XERR_RANGE);
    assert(xl6BufferIndexOfV1(source,source,-1,-1,-1) == -1); refusal(XERR_RANGE);
    assert(!xl6BufferAdoptV1(NULL)); refusal(XERR_ARGUMENT);
    xbuffer malformed = {0}; malformed.Capacity = 8;
    assert(!xl6BufferHandleV1(&malformed)); refusal(XERR_ARGUMENT);
    equal(source,data,sizeof(data));
    xrtSetErrorKind(XERR_VALUE); const xerror* original = xrtGetError();
    xl6BufferDestroyV1(empty); xl6BufferDestroyV1(source); xl6BufferDestroyV1(NULL);
    assert(xrtGetError() == original); xrtClearError();
    assert(xrtMemDebugReset()); puts("Buffer core native vectors/refusals/error-preserving cleanup ok");
    return 0;
}
