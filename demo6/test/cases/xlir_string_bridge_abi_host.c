/* 使用 -include 载入生成 C，直接校验其公开 XRT 声明与故障清理。 */
#include <assert.h>

static void checkAllocationFailures(void)
{
    unsigned int point;
    unsigned int failed = 0;
    unsigned int succeeded = 0;
    xmemdebugsnapshot snapshot;

    for ( point = 0; point < 128; ++point ) {
        xvalue* value;
        bool triggered;
        xrtClearError();
        assert(xrtMemDebugFailAfter(point));
        value = xl6_xrt_string_split("a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t", ",");
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if ( value != NULL ) {
            assert(xrtValueCount(value) == 20);
            ++succeeded;
        } else {
            assert(triggered);
            ++failed;
        }
        xrtValueRelease(value);
        xrtClearError();

        assert(xrtMemDebugFailAfter(point));
        value = xl6_xrt_string_split_lines("a\r\nb\nc\rd\ne\nf\ng\nh\ni\nj\nk\nl\nm\nn\no\np\nq\nr\ns\nt");
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if ( value != NULL ) {
            assert(xrtValueCount(value) == 20);
            ++succeeded;
        } else {
            assert(triggered);
            ++failed;
        }
        xrtValueRelease(value);
        xrtClearError();
    }
    assert(failed != 0 && succeeded != 0);
    xrtMemDebugSnapshot(&snapshot);
    assert(snapshot.DoubleFreeCount == 0 && snapshot.InvalidFreeCount == 0);
    printf("string bridge allocation failures ok: rejected=%u completed=%u\n", failed, succeeded);
}

int main(void)
{
    xvalue* arguments;
    int result;
    assert(xrtMemDebugEnable(true));
    checkAllocationFailures();
    arguments = xrtValueArray();
    assert(arguments != NULL);
    result = xlang_main(arguments);
    xrtValueRelease(arguments);
    if ( xl_script_has_error() ) {
        xl_script_print_uncaught();
        result = 1;
    }
    xl_script_module_unit();
    xl_script_error_unit();
    return result;
}
