/* Real source program receives genuine native-produced keys, then uses their
 * direct owning ABI in copies, parameter rebinding, returns, products and Value. */
#include "src/runtime/xl6_slot_map_key_value.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array_extract_native_value_abi.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef XL6_NATIVE_VALUE_TEST_FAULTS
static size_t faultAt,seen;
static unsigned lane;
static bool armed;
static bool fail(unsigned operation)
{ if (armed && lane==operation && seen++==faultAt) { xrtClearError(); return true; } return false; }
extern bool __real_xl6SlotMapKeyCopyValueV1(XL6_SlotMapKeyV1,XL6_SlotMapKeyV1*);
bool __wrap_xl6SlotMapKeyCopyValueV1(XL6_SlotMapKeyV1 key,XL6_SlotMapKeyV1* out)
{ return fail(0) ? false : __real_xl6SlotMapKeyCopyValueV1(key,out); }
extern bool __real_xl6SlotMapKeyUnboxV1(const xvalue*,XL6_SlotMapKeyV1*);
bool __wrap_xl6SlotMapKeyUnboxV1(const xvalue* value,XL6_SlotMapKeyV1* out)
{ return fail(1) ? false : __real_xl6SlotMapKeyUnboxV1(value,out); }
extern xvalue* __real_xl6SlotMapKeyBoxV1(XL6_SlotMapKeyV1);
xvalue* __wrap_xl6SlotMapKeyBoxV1(XL6_SlotMapKeyV1 key)
{ return fail(2) ? NULL : __real_xl6SlotMapKeyBoxV1(key); }
extern bool __real_xl6ArrayExtractNativeValueV1(const xvalue*,uint64_t,uint64_t,int64_t,bool,
    const XL6_NativeLeafValueProfileV1*,void*);
bool __wrap_xl6ArrayExtractNativeValueV1(const xvalue* value,uint64_t identity,uint64_t element,
    int64_t index,bool pop,const XL6_NativeLeafValueProfileV1* profile,void* output)
{ return fail(3) ? false : __real_xl6ArrayExtractNativeValueV1(value,identity,element,index,pop,profile,output); }
#endif
static xvalue* create(XL6_SlotMapKeyV1* key)
{
    const xrttype* type=xrtTypeInt64();
    XL6_SlotMapLayoutV1 layout={0xDA01,{0xDA02,0xDA03,1,sizeof(int64_t),type}};
    xvalue* witness=xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,layout.value.identity,
        layout.value.elementIdentity,1,sizeof(int64_t),type,XL6_VALUE_ADMIT_I64,false,layout.value.elementIdentity);
    assert(witness); xvalue* map=xl6SlotMapCreateTakeV1(&layout,witness); assert(map);
    assert(xl6SlotMapInsertV1(map,&layout,&(int64_t){42},key)); return map;
}
static void independentReturn(void)
{
    /* The function ID comes from verified IR, not a hard-coded generated name.
     * Observe allocation lifetime before dereferencing a potentially bad key:
     * an unretained copy must fail deterministically, not by accidental UAF. */
    xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
    XL6_SlotMapKeyV1 input={0}; xvalue* map=create(&input);
    xlir_result_slot_map_key_v1 result=XL6_TEST_NATIVE_KEPT(input);
    assert(result.ok && !xrtGetError());
    xrtValueRelease(map); xl6SlotMapKeyDropValueV1(input);
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before.LiveCount+1);
    xl6SlotMapKeyDropValueV1(result.value); xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before.LiveCount && after.LiveBytes==before.LiveBytes);
}
int main(int argc,char** argv)
{
    xl6ProductMemoryBegin(SIZE_MAX); assert(xlir_module_init());
    if (argc==1) independentReturn();
    XL6_SlotMapKeyV1 key={0},otherKey={0}; xvalue* map=create(&key),*other=create(&otherKey);
    xvalue* arguments=xrtValueArray(); assert(arguments);
    assert(xrtValueArrayAppendNew(arguments,xl6SlotMapKeyBoxV1(key)) &&
        xrtValueArrayAppendNew(arguments,xl6SlotMapKeyBoxV1(key)) &&
        xrtValueArrayAppendNew(arguments,xl6SlotMapKeyBoxV1(otherKey)));
    xrtValueRelease(map); xrtValueRelease(other);
    xl6SlotMapKeyDropValueV1(key); xl6SlotMapKeyDropValueV1(otherKey);
    if (argc==1) {
        for (unsigned round=0;round<1000;++round)
            assert(xlang_main(arguments)==0 && !xrtGetError());
    } else {
#ifdef XL6_NATIVE_VALUE_TEST_FAULTS
        assert(argc==3); faultAt=(size_t)strtoull(argv[2],NULL,10);
        bool oom=strcmp(argv[1],"oom")==0;
        lane=strcmp(argv[1],"copy")==0 ? 0 : strcmp(argv[1],"project")==0 ? 1 : strcmp(argv[1],"box")==0 ? 2 : 3;
        assert(oom || strcmp(argv[1],"copy")==0 || strcmp(argv[1],"project")==0 || strcmp(argv[1],"box")==0 || strcmp(argv[1],"extract")==0);
        if (oom) assert(xrtMemDebugFailAfter(faultAt)); else armed=true;
        int code=-1; bool ok=xl_script_main_checked(arguments,&code)!=0;
        armed=false; bool hit=oom ? xrtMemDebugFailTriggered() : seen>faultAt;
        xrtMemDebugFailClear();
        assert(ok==!hit && code==(hit ? 1 : 0));
        if (!oom) assert(!xrtGetError());
        xrtClearError();
        /* Neither a failed projection nor a partially completed callee may
         * corrupt caller-owned input; retry the full source after each fault. */
        assert(xlang_main(arguments)==0 && !xrtGetError());
        printf("prefix=%s at=%zu hit=%u\n",argv[1],faultAt,(unsigned)hit);
#else
        (void)argv; assert(false);
#endif
    }
    xrtValueRelease(arguments);
    xlir_module_fini(); assert(!xrtGetError() && !xlir_module_initialized);
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    puts("[PASS] native owning record source copies/returns/products/equality/Value, 1000 lifetimes");
    return 0;
}
