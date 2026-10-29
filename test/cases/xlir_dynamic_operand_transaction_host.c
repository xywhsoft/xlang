#define XRT_MODULE_MEMORY_DEBUG
#define xrtValueString xlir_boxing_value_string
#include XLIR_BOXING_GENERATED_C
#undef xrtValueString
#include <assert.h>
extern xvalue* xrtValueString(xstrview);

static int site,armed,failed;
static xerror* marker;

void xlir_boxing_arm(void) { armed=1; }
bool xlir_boxing_pretty(void)
{
    if (armed && site==2 && !failed) {
        xl6_error_result error={false,xrtErrorRef(marker)};
        failed=1;
        xl6ErrorScopePropagateFailure(error);
    }
    return false;
}

xvalue* xlir_boxing_value_string(xstrview text)
{
    xvalue* value;
    if (!armed || site!=1 || failed) return xrtValueString(text);
    assert(xrtMemDebugFailAfter(0));
    value=xrtValueString(text);
    assert(!value && xrtMemDebugFailTriggered());
    xrtMemDebugFailClear(); failed=1;
    return value;
}

static void run_case(int format,int failure)
{
    site=failure;armed=0;failed=0;
    assert(xlir_dynamic_boxing_case(format)==(failure?-1:0));
    assert(failed==(failure!=0));
    assert(!xrtGetError());
}

int main(void)
{
    xmemdebugsnapshot before,after;
    unsigned cases=0;
    assert(xrtMemDebugEnable(true));
    marker=xrtErrorCreate(XERR_STATE,"boxing-test",73,"pretty argument failed");
    assert(marker && xlir_module_init());
    for(int format=0;format<2;++format)for(int failure=0;failure<3;++failure)run_case(format,failure);
    xrtMemDebugSnapshot(&before);
    for(int repeat=0;repeat<32;++repeat)for(int format=0;format<2;++format)for(int failure=0;failure<3;++failure) {
        run_case(format,failure);++cases;
        xrtMemDebugSnapshot(&after);
        if(after.LiveCount!=before.LiveCount || after.LiveBytes!=before.LiveBytes)
            fprintf(stderr,"boxing leak: count %+lld bytes %+lld\n",(long long)after.LiveCount-(long long)before.LiveCount,
                (long long)after.LiveBytes-(long long)before.LiveBytes);
        assert(after.LiveCount==before.LiveCount && after.LiveBytes==before.LiveBytes);
        assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
        assert(after.InvalidFreeCount==before.InvalidFreeCount && after.DoubleFreeCount==before.DoubleFreeCount &&
            after.UseAfterFreeCount==before.UseAfterFreeCount);
    }
    xlir_module_fini();xrtErrorFree(marker);
    printf("{\"Cases\":%u,\"RealAllocationFailures\":64,\"LaterArgumentFailures\":64,\"Successes\":64,\"Balanced\":true}\n",cases);
    return 0;
}
