#include "src/runtime/xl6_array_shuffle.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_erased.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ID UINT64_C(1001)
#define ITEM UINT64_C(1002)
#define INT_LAYOUT ID,ITEM,1,sizeof(int64_t),xrtTypeInt64()
#define STR_LAYOUT ID,ITEM,2,sizeof(xstrview),xl6StringTypeV1()
#define REF_LAYOUT ID,ITEM,3,sizeof(xvalue*),xrtTypeValue()
#define FUT_LAYOUT ID,ITEM,XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1,sizeof(xfuture*),xrtTypeFuture()

static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount &&
        after.UseAfterFreeCount==before->UseAfterFreeCount && !xrtGetError());
}
static void error(xerrkind kind)
{ assert(xrtGetError() && xrtErrorKind(xrtGetError())==kind); xrtClearError(); }
static bool shuffle(xvalue* source, xrng* rng, bool useDefault)
{ return useDefault ? xl6ArrayRandShuffleV1(source) : xl6ArrayRngShuffleV1(rng,source); }
static void seed(xrng* rng)
{ xrtRngSeed(rng,2026,7); xrtRandSeed(2026,7); }
static xfuture* ready(void)
{
    xfuture* future=NULL; xpromise* promise=xrtPromiseCreate(&future,NULL); assert(promise && future);
    assert(xrtPromiseResolve(promise,NULL)); xrtPromiseDestroy(promise); return future;
}
static xvalue* integers(bool bind)
{
    xvalue* value=xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,INT_LAYOUT,XL6_VALUE_ADMIT_I64,false,ITEM); assert(value);
    for (int64_t i=0;i<16;++i) assert(xl6ArrayPushV1(value,INT_LAYOUT,&i));
    if (bind) { xvalue* alias=xl6ArrayEraseV1(value,INT_LAYOUT,NULL); assert(alias==value); xrtValueRelease(alias); }
    return value;
}
static xarray storage(xvalue* source,uint64_t identity,uint64_t item,unsigned transfer,size_t size,const xrttype* type)
{
    XL6_Sequence* pin=xl6SequenceBeginV1(source,identity,item,transfer,size,type); assert(pin);
    xarray result=xl6SequenceStorageV1(pin)->Storage; xl6SequenceEndV1(pin); return result;
}
static void zeroAllocation(xvalue* value,xrng* rng,bool useDefault)
{
    assert(xrtMemDebugFailAfter(0));
    assert(shuffle(value,rng,useDefault) && !xrtMemDebugFailTriggered());
    xrtMemDebugFailClear(); assert(!xrtGetError());
}
static void vectors(bool useDefault)
{
    xrng rng,oracle; seed(&rng); xrtRngSeed(&oracle,2026,7);
    int64_t expected[16]; for (int64_t i=0;i<16;++i) expected[i]=i;
    assert(xrtRngShuffle(&oracle,expected,16,sizeof(*expected)));
    xvalue* value=integers(true); xvalue* alias=xrtValueRetain(value); assert(alias==value);
    xarray before=storage(value,INT_LAYOUT);
    zeroAllocation(alias,&rng,useDefault);
    xarray after=storage(value,INT_LAYOUT);
    assert(before.Data==after.Data && before.Capacity==after.Capacity && before.Count==after.Count);
    for (size_t i=0;i<16;++i) {
        int64_t actual=-1; assert(xl6ArrayReadV1(value,INT_LAYOUT,(int64_t)i,&actual)); assert(actual==expected[i]);
    }
    assert((useDefault ? xrtRand64() : xrtRng64(&rng))==xrtRng64(&oracle));
    xrtValueRelease(alias); xrtValueRelease(value);

    value=xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,STR_LAYOUT,XL6_VALUE_ADMIT_STRING,false,ITEM); assert(value);
    const xstrview strings[]={{"a\0b",3},{"你",3},{"",0}};
    for (unsigned i=0;i<3;++i) assert(xl6ArrayPushV1(value,STR_LAYOUT,&strings[i]));
    alias=xl6ArrayEraseV1(value,STR_LAYOUT,xl6StringConverterV1()); assert(alias==value); xrtValueRelease(alias);
    before=storage(value,STR_LAYOUT); xstrview owned[3]; memcpy(owned,before.Data,sizeof(owned));
    seed(&rng); zeroAllocation(value,&rng,useDefault); after=storage(value,STR_LAYOUT); assert(after.Data==before.Data);
    for (unsigned i=0;i<3;++i) {
        const xstrview* current=(const xstrview*)after.Data+i; unsigned matches=0;
        for (unsigned j=0;j<3;++j) matches+=current->Data==owned[j].Data && current->Size==owned[j].Size;
        assert(matches==1);
    }
    xrtValueRelease(value);

    value=xl6SequenceReferenceCreateV1(XL6_SEQUENCE_ARRAY_V1,ID,ITEM,XL6_VALUE_ADMIT_ANY,false,ITEM); assert(value);
    xvalue* objects[3]={xrtValueObject(),xrtValueObject(),xrtValueObject()};
    for (unsigned i=0;i<3;++i) { assert(objects[i]); assert(xl6ArrayPushV1(value,REF_LAYOUT,&objects[i])); }
    assert(xl6ArrayPushV1(value,REF_LAYOUT,&objects[0]));
    alias=xl6ArrayEraseV1(value,REF_LAYOUT,NULL); assert(alias); xrtValueRelease(alias);
    before=storage(value,REF_LAYOUT); seed(&rng); zeroAllocation(value,&rng,useDefault);
    after=storage(value,REF_LAYOUT); assert(after.Data==before.Data && after.Count==4);
    unsigned found[3]={0};
    for (unsigned i=0;i<4;++i) {
        xvalue* current=*((xvalue**)after.Data+i);
        for (unsigned j=0;j<3;++j) found[j]+=current==objects[j];
    }
    assert(found[0]==2 && found[1]==1 && found[2]==1);
    for (unsigned i=0;i<3;++i) xrtValueRelease(objects[i]);
    xrtValueRelease(value);

    value=xl6SequenceFutureCreateV1(XL6_SEQUENCE_ARRAY_V1,ID,ITEM,ITEM+1,true); assert(value);
    xfuture* futures[3]={ready(),ready(),NULL};
    for (unsigned i=0;i<3;++i) assert(xl6ArrayPushV1(value,FUT_LAYOUT,&futures[i]));
    alias=xl6ArrayEraseV1(value,FUT_LAYOUT,NULL); assert(alias); xrtValueRelease(alias);
    before=storage(value,FUT_LAYOUT); seed(&rng); zeroAllocation(value,&rng,useDefault);
    after=storage(value,FUT_LAYOUT); assert(after.Data==before.Data);
    unsigned seen[3]={0};
    for (unsigned i=0;i<3;++i) {
        xfuture* actual=NULL; assert(xl6ArrayReadV1(value,FUT_LAYOUT,(int64_t)i,&actual));
        for (unsigned j=0;j<3;++j) seen[j]+=actual==futures[j];
        xrtFutureDestroy(actual);
    }
    assert(seen[0]==1 && seen[1]==1 && seen[2]==1);
    xrtValueRelease(value); xrtFutureDestroy(futures[0]); xrtFutureDestroy(futures[1]);

    value=xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,INT_LAYOUT,XL6_VALUE_ADMIT_I64,false,ITEM); assert(value);
    alias=xl6ArrayEraseV1(value,INT_LAYOUT,NULL); assert(alias); xrtValueRelease(alias);
    seed(&rng); oracle=rng; zeroAllocation(value,&rng,useDefault);
    int64_t one=9; assert(xl6ArrayPushV1(value,INT_LAYOUT,&one)); zeroAllocation(value,&rng,useDefault);
    assert((useDefault ? xrtRand64() : xrtRng64(&rng))==xrtRng64(&oracle)); xrtValueRelease(value);
}
static void foreignDrop(void* value,void* context) { (void)value; (void)context; }
static const xvaluehandleops foreignOps={NULL,foreignDrop,NULL,NULL};
static void boundaries(bool useDefault)
{
    xrng rng,original; seed(&rng); original=rng;
    xvalue* value=integers(false); xarray before=storage(value,INT_LAYOUT);
    int64_t originalItems[16]; memcpy(originalItems,before.Data,sizeof(originalItems));
    assert(!shuffle(value,&rng,useDefault)); error(XERR_STATE);
    xarray after=storage(value,INT_LAYOUT); assert(after.Data==before.Data && !memcmp(originalItems,after.Data,sizeof(originalItems)));
    xrtValueRelease(value);
    value=integers(true); XL6_Sequence* pin=xl6SequenceBeginV1(value,INT_LAYOUT); assert(pin);
    assert(!shuffle(value,&rng,useDefault)); error(XERR_STATE); xl6SequenceEndV1(pin);
    xrtValueRelease(value);
    void* raw=(void*)(uintptr_t)1; value=xrtValueHandleTake(&raw,&foreignOps,NULL); assert(value && xrtValueTypeIdBind(value,ID));
    assert(!shuffle(value,&rng,useDefault)); error(XERR_TYPE); xrtValueRelease(value);
    value=xl6SequenceWrapAsV1(xrtTypedArrayCreate(xrtTypeInt64()),XL6_SEQUENCE_STACK_V1,INT_LAYOUT); assert(value);
    assert(!shuffle(value,&rng,useDefault)); error(XERR_TYPE); xrtValueRelease(value);
    assert(!shuffle(NULL,&rng,useDefault)); error(XERR_TYPE);
    assert(!shuffle(xrtValueBool(true),&rng,useDefault)); error(XERR_TYPE);
    assert(!memcmp(&rng,&original,sizeof(rng)));
    assert(xrtRand64()==xrtRng64(&original));
    value=integers(true); xrng invalid={0};
    assert(!xl6ArrayRngShuffleV1(NULL,value)); error(XERR_STATE);
    assert(!xl6ArrayRngShuffleV1(&invalid,value)); error(XERR_STATE); xrtValueRelease(value);
}
static xvalue* wire(void)
{
    xvalue* value=xrtValueArray(); assert(value);
    for (int64_t i=0;i<16;++i) assert(xrtValueArrayAppendNew(value,xrtValueInt(i)));
    return value;
}
static void fault(bool useDefault,bool copy,size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xrng rng; seed(&rng); xvalue* source=wire(); xvalue* other=copy ? xrtValueClone(source) : NULL;
    if (copy) assert(other);
    assert(xrtMemDebugFailAfter(point)); bool ok=shuffle(source,&rng,useDefault);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if (hit) { assert(!ok); error(XERR_MEMORY); }
    else assert(ok && !xrtGetError());
    unsigned mask=0;
    for (size_t i=0;i<16;++i) {
        int64_t current=-1; assert(xrtValueGetInt(xrtValueArrayGet(source,i),&current));
        assert(current>=0 && current<16 && !(mask&(1u<<(unsigned)current))); mask|=1u<<(unsigned)current;
        if (!ok) assert(current==(int64_t)i);
        if (other) { assert(xrtValueGetInt(xrtValueArrayGet(other,i),&current)); assert(current==(int64_t)i); }
    }
    assert(mask==65535); xrtValueRelease(other); xrtValueRelease(source); balanced(&before);
    printf("shuffle wire fault default=%d copy=%d point=%zu hit=%d balanced\n",(int)useDefault,(int)copy,point,(int)hit);
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true)); xrtRandSeed(2026,7); assert(!xrtGetError());
    if (argc==4) { fault(atoi(argv[1])!=0,atoi(argv[2])!=0,(size_t)strtoull(argv[3],NULL,10)); return 0; }
    assert(argc==1);
    for (unsigned mode=0;mode<2;++mode) for (unsigned cycle=0;cycle<50;++cycle) {
        xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); vectors(mode!=0); boundaries(mode!=0); balanced(&before);
    }
    puts("4 native slot families, 2 RNG sources: 100 physical cycles balanced; native shuffle zero allocations");
    return 0;
}
