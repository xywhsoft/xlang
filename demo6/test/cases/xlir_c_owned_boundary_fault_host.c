#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <__xl_runtime/xruntime_decl.h>

static size_t allocated,freed;
static void* trackedMalloc(size_t size) { void* p=malloc(size);if(p)++allocated;return p; }
static void trackedFree(void* p) { if(p)++freed;free(p); }
#define malloc trackedMalloc
#define free trackedFree
#include "xlir_c_owned_native.c"
#undef malloc
#undef free
static xvalue* expectedPack;
static unsigned observedPacks;
void owned_observe_array_pack(xvalue* pack) { assert(pack && (!expectedPack || pack==expectedPack));++observedPacks; }
void owned_observe_dict_pack(xvalue* pack) { assert(pack && (!expectedPack || pack==expectedPack));++observedPacks; }
#define main unusedGeneratedMain
#include XL6_C_OWNED_BOUNDARY_PROGRAM
#undef main
#include XL6_C_OWNED_BOUNDARY_BINDINGS

static void checkPackIdentity(void)
{
    xlir_closure *array=OWNED_ARRAY_ADAPTER,*dict=OWNED_DICT_ADAPTER;
    assert(array && dict);
    for(unsigned named=0;named<2;++named) {
        expectedPack=named ? xrtValueObject() : xrtValueArray();assert(expectedPack);
        for(unsigned populated=0;populated<2;++populated) {
            if(populated) {
                xvalue* item=xrtValueInt(42);assert(item);
                assert(named ? xrtValueObjectSet(expectedPack,(xstrview){"answer",6},item) :
                    xrtValueArrayAppend(expectedPack,item));
                xrtValueRelease(item);
            }
            unsigned before=observedPacks;
            if(named) {
                xvalue* result=NULL;
                assert(((bool(*)(void*,xvalue**,int64_t,xvalue*))dict->invoke)(
                    dict->environment,&result,7,expectedPack));
                assert(result);
                xbytesview view={0};assert(xrtValueGetBytes(result,&view));
                assert(c_owned_binary_span_valid(view) && !c_owned_binary_span_original(view));
                xrtValueRelease(result);
            } else {
                xlir_string result={0};
                assert(((bool(*)(void*,xlir_string*,xvalue*))array->invoke)(
                    array->environment,&result,expectedPack));
                assert(result.Size==5 && !memcmp(result.Data,"hello",5) && result.Data!=originalText);
                xlir_string_drop(result);
            }
            assert(observedPacks==before+1 && allocated==freed);
        }
        xrtValueRelease(expectedPack);expectedPack=NULL;
    }
    puts("C owner variadic adapters: empty/populated array/dict pack pointer identity preserved");
}

int main(void)
{
    assert(xrtMemDebugEnable(true) && xrtMemDebugEnabled());
    assert(xlir_module_init());
    checkPackIdentity();
    size_t failures=0,successes=0;
    for(int mode=0;mode<16;++mode) {
        xl6_foreign_error_scope entry={0};
        assert(xl6ForeignCallBegin(&entry));
        assert(owned_boundary_exercise(mode));
        xl6_foreign_error_result outcome=xl6ForeignCallEnd(&entry);
        assert(outcome.ok && !outcome.error && allocated==freed);
        xmemdebugsnapshot before={0};xrtMemDebugSnapshot(&before);
        size_t modeFailures=0,modeSuccesses=0;
        for(unsigned limit=0;limit<96;++limit) {
            assert(xl6ForeignCallBegin(&entry));
            assert(xrtMemDebugFailAfter(limit));
            bool value=owned_boundary_exercise(mode);
            bool injected=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
            outcome=xl6ForeignCallEnd(&entry);
            if(outcome.ok){assert(value && !injected);++modeSuccesses;}
            else{assert(!value && injected && outcome.error);++modeFailures;}
            xrtErrorFree(outcome.error);xrtClearError();
            assert(allocated==freed);
            xmemdebugsnapshot after={0};xrtMemDebugSnapshot(&after);
            assert(after.LiveCount==before.LiveCount);
        }
        assert(modeFailures && modeSuccesses);
        failures+=modeFailures;successes+=modeSuccesses;
    }
    printf("C owner callable boundaries: %zu injected failures, %zu completions; no foreign or XRT leaks\n",failures,successes);
    return 0;
}
