#include "src/runtime/xl6_array_iterator.h"
#include <assert.h>
#include <stdio.h>

#define SOURCE UINT64_C(801),UINT64_C(901),1,sizeof(int64_t),xrtTypeInt64()
#define CURSOR UINT64_C(802),UINT64_C(901),1,sizeof(int64_t),xrtTypeInt64()

/* Strict, actual snapshot-only closure: build input storage through XRT, not
 * the Array/Stack facades. No source-level container implementation copy. */
int main(void)
{
    assert(xrtMemDebugEnable(true));
    xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
    xtypedarray* storage=xrtTypedArrayCreate(xrtTypeInt64()); assert(storage);
    for (int64_t i=1;i<=3;++i) assert(xrtTypedArrayPush(storage,&i));
    xvalue* source=xl6SequenceWrapV1(storage,SOURCE); assert(source);
    xvalue* cursor=xl6ArrayIteratorCreateV1(source,SOURCE,802); assert(cursor);
    XL6_Sequence* owner=xl6SequenceBeginV1(source,SOURCE); assert(owner);
    xrtTypedArrayClear(xl6SequenceStorageV1(owner)); xl6SequenceEndV1(owner);
    xrtValueRelease(source);
    for (int64_t i=1;i<=3;++i) {
        int64_t observed=-1;
        assert(xl6ArrayIteratorAdvanceV1(cursor,CURSOR));
        assert(xl6ArrayIteratorReadV1(cursor,CURSOR,&observed) && observed==i);
        assert(xl6ArrayIteratorOrdinalV1(cursor,CURSOR)==i-1);
    }
    assert(!xl6ArrayIteratorAdvanceV1(cursor,CURSOR)); xrtValueRelease(cursor);
    assert(!xrtGetError()); xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
           after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
           before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
           before.UseAfterFreeCount==after.UseAfterFreeCount);
    puts("native snapshot-only closure balanced"); return 0;
}
