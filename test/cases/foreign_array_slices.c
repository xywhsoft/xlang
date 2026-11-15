#include "foreign_array_slices.h"
#include <__xl_runtime/xl6_sequence.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct { const uint8_t* data; size_t size; uint8_t copy[64]; } sliceExpected[3];
static int sliceCalls,sliceFailure;
static xmemdebugsnapshot sliceBefore;
void fixtureSliceRemember(int i,const uint8_t* p,size_t size)
{
    assert(i>=0 && i<3 && size<=sizeof(sliceExpected[i].copy) && (!size || p));
    sliceExpected[i].data=p;sliceExpected[i].size=size;
    if(size)memcpy(sliceExpected[i].copy,p,size);
}
void fixtureSliceRememberStorage(const xvalue* value,int binary)
{
    XL6_Sequence* owner=NULL;XL6_SequenceLayoutV1 layout={0};const xvalueconverter* converter=NULL;
    unsigned i;
    /* Independent resident reader. An element assignment may replace storage;
     * compare with the actual retained slot used by this call. */
    assert(xl6SequenceBeginErasedArrayV1(value,&owner,&layout,&converter)==1);
    const xtypedarray* storage=xl6SequenceStorageV1(owner);assert(xrtTypedArrayCount(storage)==3);
    for(i=0;i<3;++i){const void* slot=xrtTypedArrayConstGet(storage,i);assert(slot);
        if(binary){xbytesview bytes={0};assert(xrtValueGetBytes(*(xvalue* const*)slot,&bytes));fixtureSliceRemember((int)i,bytes.Data,bytes.Size);}
        else {const xstrview* text=slot;fixtureSliceRemember((int)i,(const uint8_t*)text->Data,text->Size);}
    }
    xl6SequenceEndV1(owner);assert(!xrtGetError());
}
static int fixtureSliceValidate(const FixtureArraySlice* rows,uint64_t count)
{
    uint64_t i;
    assert(count<=3 && (!count || rows));
    for(i=0;i<count;++i){
        assert(rows[i].iSize==sizeof(*rows));
        if(rows[i].data!=sliceExpected[i].data || rows[i].length!=sliceExpected[i].size)
            fprintf(stderr,"Array slice mismatch: call=%d row=%llu actual=%p/%llu expected=%p/%zu\n",sliceCalls,
                (unsigned long long)i,(const void*)rows[i].data,(unsigned long long)rows[i].length,(const void*)sliceExpected[i].data,sliceExpected[i].size);
        assert(rows[i].data==sliceExpected[i].data && rows[i].length==sliceExpected[i].size);
        assert(rows[i].tag==(i ? i+1:UINT64_MAX));
        assert(!rows[i].length || !memcmp(rows[i].data,sliceExpected[i].copy,(size_t)rows[i].length));
    }
    return 0;
}
int fixtureArraySlices(const FixtureArraySlice* rows,uint64_t count)
{
    ++sliceCalls;fixtureSliceValidate(rows,count);
    if(sliceFailure){xrtSetErrorKind(XERR_STATE);return -1;}
    return 0;
}
int fixtureArraySlicesReenter(const FixtureArraySlice* rows,uint64_t count,int(*callback)(void*),void* user)
{
    ++sliceCalls;fixtureSliceValidate(rows,count);
    if(callback(user)<0)return -1;
    return fixtureSliceValidate(rows,count);
}
int fixtureArraySlicesPacked(const FixtureSliceOptions* options,const FixtureArraySlice* rows,uint64_t count)
{assert(options && options->iSize==sizeof(*options) && options->value==7);return fixtureArraySlices(rows,count);}
int fixtureSliceCalls(void){return sliceCalls;}
void fixtureSliceFailure(int value){sliceFailure=value;}
void fixtureSliceHeapBegin(void)
{assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));xrtMemDebugSnapshot(&sliceBefore);}
void fixtureSliceBudget(int count)
{assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));if(count<0)xrtMemDebugFailClear();else assert(xrtMemDebugFailAfter(count));}
void fixtureSliceEndBudget(void)
{xrtMemDebugFailClear();xrtClearError();sliceFailure=0;}
void fixtureSliceHeapBalanced(void)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==sliceBefore.LiveCount && after.LiveBytes==sliceBefore.LiveBytes);
    assert(after.InvalidFreeCount==sliceBefore.InvalidFreeCount && after.DoubleFreeCount==sliceBefore.DoubleFreeCount);
}
void fixtureSliceReport(int native)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    printf("Array slice heap %s: alloc=%llu free=%llu realloc=%llu row=%zu\n",native?"native":"language",
        (unsigned long long)(after.AllocCount-sliceBefore.AllocCount),(unsigned long long)(after.FreeCount-sliceBefore.FreeCount),
        (unsigned long long)(after.ReallocCount-sliceBefore.ReallocCount),sizeof(FixtureArraySlice));
}
void fixtureSliceNativeHot(void)
{
    unsigned round,i;FixtureArraySlice rows[3];
    fixtureSliceHeapBegin();
    for(round=0;round<1000;++round){
        for(i=0;i<3;++i){memset(&rows[i],0,sizeof(rows[i]));rows[i].iSize=sizeof(rows[i]);
            rows[i].data=sliceExpected[i].data;rows[i].length=sliceExpected[i].size;rows[i].tag=i ? i+1:UINT64_MAX;}
        assert(fixtureArraySlices(rows,3)==0);
    }
    fixtureSliceReport(1);fixtureSliceHeapBalanced();
}
