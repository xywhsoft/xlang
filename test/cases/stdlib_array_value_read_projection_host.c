/* Actual generated functions, with identities extracted from verified IR. */
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_value_admission.h"
#include <assert.h>
#include <stdio.h>
#define LAYOUT XL6_INDEX_OWNER,XL6_INDEX_ELEMENT,1,sizeof(uint32_t),xl6CharTypeV1()
static void error(xerrkind kind)
{ assert(xrtGetError() && xrtErrorKind(xrtGetError())==kind); xrtClearError(); }
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount && !xrtGetError());
}
static bool invoke(xvalue* owner,xvalue* key,unsigned operation,bool success)
{
    int64_t before=XL6_INDEX_CLEANUPS; bool ok;
    if(operation==0) { XL6_INDEX_CHAR_RESULT result=XL6_INDEX_CHAR(owner,0); ok=result.ok; if(ok) assert(result.value==0x4F60); }
    else if(operation==1 || operation==3) {
        XL6_INDEX_SAFE_RESULT result=XL6_INDEX_SAFE(owner,operation==3 ? INT64_MIN : 0); ok=result.ok;
        if(ok) assert(operation==3 ? !result.value.hasValue : result.value.hasValue && result.value.value==0x4F60);
    } else {
        XL6_INDEX_KEYED_RESULT result=XL6_INDEX_KEYED(owner,key); ok=result.ok;
        if(ok) { uint32_t ch=0; assert(xrtValueGetChar(result.value,&ch) && ch==0x4F60); xrtValueRelease(result.value); }
    }
    assert(ok==success && XL6_INDEX_CLEANUPS==before+1); return ok;
}
int main(void)
{
    assert(xrtMemDebugEnable(true)); assert(xlir_module_init());
    for(unsigned cycle=0;cycle<100;++cycle) {
        xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
        xvalue* owner=xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,LAYOUT,XL6_VALUE_ADMIT_CHAR,false,XL6_INDEX_ELEMENT);
        xvalue* key=xrtValueInt(0); assert(owner && key); uint32_t ch=0x4F60;
        assert(xl6ArrayPushV1(owner,LAYOUT,&ch));
        xvalue* alias=xl6ArrayEraseV1(owner,LAYOUT,xl6CharConverterV1()); assert(alias==owner); xrtValueRelease(alias);
        for(unsigned operation=0;operation<4;++operation) {
            bool complete=false;
            for(size_t point=0;point<128;++point) {
                assert(xrtMemDebugFailAfter(point)); int64_t cleanups=XL6_INDEX_CLEANUPS; bool ok;
                /* Observe physical failure independently of pending diagnostics. */
                if(operation==0) { XL6_INDEX_CHAR_RESULT r=XL6_INDEX_CHAR(owner,0); ok=r.ok; if(ok) assert(r.value==ch); }
                else if(operation==1 || operation==3) {
                    XL6_INDEX_SAFE_RESULT r=XL6_INDEX_SAFE(owner,operation==3 ? INT64_MIN : 0); ok=r.ok;
                    if(ok) assert(operation==3 ? !r.value.hasValue : r.value.hasValue && r.value.value==ch);
                } else {
                    XL6_INDEX_KEYED_RESULT r=XL6_INDEX_KEYED(owner,key); ok=r.ok;
                    if(ok) { uint32_t found=0; assert(xrtValueGetChar(r.value,&found) && found==ch); xrtValueRelease(r.value); }
                }
                bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
                assert(XL6_INDEX_CLEANUPS==cleanups+1 && ok==!hit);
                if(hit) error(XERR_MEMORY);
                else { assert(!xrtGetError()); complete=true; break; }
            }
            assert(complete);
        }
        XL6_Sequence* pin=xl6SequenceBeginV1(owner,LAYOUT); assert(pin);
        for(unsigned operation=0;operation<4;++operation) { invoke(owner,key,operation,false); error(XERR_STATE); }
        xl6SequenceEndV1(pin);
        for(unsigned operation=0;operation<4;++operation) invoke(owner,key,operation,true);
        xrtValueRelease(key); xrtValueRelease(owner); balanced(&before);
    }
    xlir_module_fini(); xlir_module_fini(); assert(!xrtGetError());
    puts("generated/native owned index reads: 100 physical cycles, complete OOM prefixes, finally and ownership balanced"); return 0;
}
