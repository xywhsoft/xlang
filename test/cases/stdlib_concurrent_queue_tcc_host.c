/* Actual TinyCC -> resident GCC ABI, including owned struct returns. This is
 * private C bridge acceptance, not the pending xlang Queue language cutover. */
#include "lib/tcc/libtcc.h"
#include "release/include/xl.h"
#include "src/runtime/xl6_concurrent_queue.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array_erased.h"
#ifdef _WIN32
#include <windows.h>
#include "src/api/xl_api_internal.h"
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef int (*Entry)(const xvalue*,const xvalue*,xvalue**);
typedef xvalue* (*Create)(uint64_t,xvalue*,int64_t);
static const Create factories[]={xl6SpscQueueCreateTakeV1,xl6MpscQueueCreateTakeV1,xl6MpmcQueueCreateTakeV1};
static void diagnostic(void* data,const char* message)
{ (void)data; fprintf(stderr,"queue TCC ABI: %s\n",message); }
static xvalue* schema(void)
{ return xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,123,456,1,sizeof(int64_t),xrtTypeInt64(),XL6_VALUE_ADMIT_I64,false,456); }
int main(void)
{
    /* Install debug interception before the provider/environment can own any
     * XRT allocation. The measured product scope starts after configuration. */
    assert(xrtMemDebugEnable(true));
    TCCState* state=tcc_new(); assert(state);
    tcc_set_error_func(state,NULL,diagnostic);
    assert(tcc_set_output_type(state,TCC_OUTPUT_MEMORY)==0);
    /* The real source registrar supplies every referenced resident address,
     * not a hand-maintained list or an old DLL's mixed XRT allocation domain. */
#ifdef _WIN32
    /* tcc_new is imported from xl.dll. Configuring that state through a
     * separately discovered system libtcc mixes incompatible private layouts.
     * Use the SAME provider's environment, with complete source resident
     * symbols registered first (TinyCC keeps the first injected definition). */
    HMODULE provider=GetModuleHandleA("xl.dll"); assert(provider);
    int (*environment)(TCCState*,int);
    FARPROC address=GetProcAddress(provider,"xlTccRegisterXrtEnvironment");
    assert(address); memcpy(&environment,&address,sizeof(address));
    xl__tcc.ready=1; xl__tcc.tcc_add_symbol=tcc_add_symbol;
    assert(xlApiAddDefaultRuntimeSymbols(state));
    assert(environment(state,TCC_OUTPUT_MEMORY));
#else
    assert(xlTccRegisterXrtEnvironment(state,TCC_OUTPUT_MEMORY));
#endif
    assert(tcc_add_include_path(state,".")==0 && tcc_add_include_path(state,"release")==0);
    const char* families[]={"SpscQueue","MpscQueue","MpmcQueue"};
    char source[4096]; size_t used=(size_t)snprintf(source,sizeof(source),
        "#include \"src/runtime/xl6_concurrent_queue.h\"\n");
    for (unsigned i=0;i<3;++i) {
        int added=snprintf(source+used,sizeof(source)-used,
            "int queue_abi_%u(const xvalue* q,const xvalue* a,xvalue** output) {\n"
            " xqueuebatchresult pushed=xl6%sPushArrayV2(q,999,a,123);\n"
            " XL6_QueueArrayResultV2 popped=xl6%sPopArrayV2(q,999,789,4,0);\n"
            " *output=popped.Items;\n"
            " return pushed.Result==XQUEUE_OK && pushed.Count==3 && popped.Result==XQUEUE_OK && popped.Count==3 && popped.Items ? 0 : 1; }\n",
            i,families[i],families[i]);
        assert(added>0 && (size_t)added<sizeof(source)-used); used+=(size_t)added;
    }
    assert(tcc_compile_string(state,source)==0 && tcc_relocate(state)==0);
    Entry entries[3];
    for (unsigned i=0;i<3;++i) { char name[32]; snprintf(name,sizeof(name),"queue_abi_%u",i);
        void* address=tcc_get_symbol(state,name); assert(address); memcpy(&entries[i],&address,sizeof(address)); }
    /* Debug was enabled before configuration. Re-enabling it with live
     * provider allocations is intentionally forbidden by the XRT contract. */
    xmemdebugsnapshot before={0},after={0}; xrtMemDebugSnapshot(&before);
    xvalue* outputs[3]={0};
    for (unsigned i=0;i<3;++i) {
        xvalue* array=schema(),*queue=factories[i](999,schema(),4); assert(array && queue);
        XL6_Sequence* owner=xl6SequenceBeginV1(array,123,456,1,sizeof(int64_t),xrtTypeInt64()); assert(owner);
        int64_t values[]={INT64_MIN,7,INT64_MAX};
        for (unsigned j=0;j<3;++j) assert(xrtTypedArrayPush(xl6SequenceStorageV1(owner),&values[j]));
        xl6SequenceEndV1(owner);
        assert(entries[i](queue,array,&outputs[i])==0 && outputs[i] && xrtValueTypeId(outputs[i])==789);
        xrtValueRelease(queue); xrtValueRelease(array);
    }
    tcc_delete(state); /* Owned results must not retain the generated code. */
    for (unsigned i=0;i<3;++i) {
        int64_t expected[]={INT64_MIN,7,INT64_MAX};
        for (unsigned j=0;j<3;++j) { xvalue* item=xl6ArrayErasedReadV1(outputs[i],j); int64_t actual=0;
            assert(item && xrtValueTypeId(item)==456 && xrtValueGetInt(item,&actual) && actual==expected[j]); xrtValueRelease(item); }
        xrtValueRelease(outputs[i]);
    }
    assert(!xrtGetError()); xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.DoubleFreeCount==after.DoubleFreeCount && before.InvalidFreeCount==after.InvalidFreeCount &&
        before.UseAfterFreeCount==after.UseAfterFreeCount);
    puts("actual TinyCC owned queue struct ABI: three families + post-delete results balanced"); return 0;
}
