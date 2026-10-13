#include "src/runtime/xl6_path_build.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static xl6_error_result call(const xvalue* source,int64_t style,xstrview* output)
{
    xl6_error_scope scope={0};assert(xl6ForeignCallBegin(&scope));
    *output=xl6_xrt_fs_path_build(source,style);return xl6ForeignCallEnd(&scope);
}
static void rejected(const xvalue* source,int64_t style,xerrkind kind)
{
    xmemdebugsnapshot before;xstrview result={0};xrtMemDebugSnapshot(&before);
    xl6_error_result outcome=call(source,style,&result);
    assert(!outcome.ok && outcome.error && xrtErrorKind(outcome.error)==kind && !result.Data && !result.Size);
    xrtErrorFree(outcome.error);balance(&before);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xl6_error_scope warm={0};assert(xl6ForeignCallBegin(&warm));
    assert(xl6ForeignCallEnd(&warm).ok);xrtSetErrorKind(XERR_MEMORY);xrtClearError();
    xmemdebugsnapshot initial;xrtMemDebugSnapshot(&initial);
    xvalue* array=xrtValueArray();assert(array);
    assert(xrtValueArrayAppendNew(array,xrtValueString((xstrview){"alpha",5})) &&
        xrtValueArrayAppendNew(array,xrtValueString((xstrview){"beta",4})));
    xstrview result={0};xl6_error_result outcome=call(array,1,&result);
    assert(outcome.ok && !outcome.error && result.Size==10 && !memcmp(result.Data,"alpha/beta",10));xrtFree((void*)result.Data);
    rejected(array,-1,XERR_RANGE);rejected(array,3,XERR_RANGE);rejected(NULL,1,XERR_TYPE);
    xvalue* wrong=xrtValueInt(42);assert(wrong);rejected(wrong,1,XERR_TYPE);xrtValueRelease(wrong);
    xvalue* invalid=xrtValueArray();assert(invalid && xrtValueArrayAppendNew(invalid,xrtValueInt(42)));
    rejected(invalid,1,XERR_TYPE);xrtValueRelease(invalid);
    invalid=xrtValueArray();assert(invalid && xrtValueArrayAppendNew(invalid,xrtValueString((xstrview){"a\0b",3})));
    rejected(invalid,1,XERR_VALUE);xrtValueRelease(invalid);
    unsigned failures=0,budgets=0;bool complete=false;
    for(unsigned fault=0;fault<128;++fault){
        xmemdebugsnapshot before;xl6_error_scope scope={0};assert(xl6ForeignCallBegin(&scope));
        xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(fault));
        result=xl6_xrt_fs_path_build(array,1);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();outcome=xl6ForeignCallEnd(&scope);++budgets;
        if(hit){++failures;assert(!outcome.ok && !result.Data && !result.Size);if(outcome.error)assert(xrtErrorKind(outcome.error)==XERR_MEMORY);}
        else{assert(outcome.ok && !outcome.error && result.Size==10 && !memcmp(result.Data,"alpha/beta",10));complete=true;}
        xrtFree((void*)result.Data);xrtErrorFree(outcome.error);
        assert(xrtValueCount(array)==2);balance(&before);if(complete)break;
    }
    assert(complete && failures && budgets==failures+1);xrtValueRelease(array);balance(&initial);
    printf("Path parts product: %u complete OOM budgets, %u actual failures, exact input/output/error and memory balance\n",budgets,failures);
    return 0;
}
