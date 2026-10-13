#include "src/runtime/xl6_char_checked.h"
#include "src/runtime/xl6_text_value.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>

#define CHAR_ID UINT64_C(91001)
static unsigned mode;
static unsigned reads;
extern bool __real_xrtValueGetChar(const xvalue*,uint32_t*);
bool __wrap_xrtValueGetChar(const xvalue* source,uint32_t* output)
{
    ++reads;
    if (mode==1) { xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL}); return false; }
    bool ok=__real_xrtValueGetChar(source,output);
    if (mode==2 || mode==5) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    if (mode==3) xrtSetErrorKind(XERR_IO);
    if (mode==4) *output=0xd800;
    if (mode==5) xrtSetErrorKind(XERR_TYPE);
    return ok;
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount &&
        after.UseAfterFreeCount==before->UseAfterFreeCount && !xrtGetError());
}
static void check(xvalue* input,bool strict,bool expected,uint32_t value,xerrkind kind)
{
    xl6_error_scope call={0}; assert(xl6ErrorScopeBegin(&call)); uint32_t output=0xabcdef;
    bool ok=xl6CharValueCheckedV1(input,CHAR_ID,strict,&output);
    xl6_error_result result=xl6ErrorScopeEnd(&call);
    assert(ok==expected && result.ok==expected);
    assert(output==(expected ? value : 0xabcdef));
    assert(expected ? !result.error : result.error && xrtErrorKind(result.error)==kind);
    xrtErrorFree(result.error); xrtClearError(); xrtValueRelease(input);
}
static void vectors(void)
{
    const uint32_t codes[]={0,65,0x4f60,0xd7ff,0xe000,0x10ffff};
    for (size_t i=0;i<sizeof(codes)/sizeof(codes[0]);++i) {
        check(xrtValueChar(codes[i]),true,true,codes[i],XERR_NONE);
        check(xrtValueChar(codes[i]),false,true,codes[i],XERR_NONE);
        check(xrtValueInt(codes[i]),false,true,codes[i],XERR_NONE);
        check(xrtValueUInt(codes[i]),false,true,codes[i],XERR_NONE);
        check(xrtValueInt(codes[i]),true,false,0,XERR_TYPE);
    }
    check(xrtValueInt(-1),false,false,0,XERR_RANGE);
    check(xrtValueInt(INT64_MIN),false,false,0,XERR_RANGE);
    check(xrtValueUInt(UINT64_MAX),false,false,0,XERR_RANGE);
    check(xrtValueUInt(UINT64_C(0x100000041)),false,false,0,XERR_RANGE);
    check(xrtValueUInt(0x110000),false,false,0,XERR_RANGE);
    check(xrtValueInt(0xd800),false,false,0,XERR_RANGE);
    check(xrtValueInt(0xdfff),false,false,0,XERR_RANGE);
    check(xrtValueBool(true),false,false,0,XERR_TYPE);
    check(xrtValueFloat(65.0),false,false,0,XERR_TYPE);
    check(xrtValueString((xstrview){"A\0B",3}),false,false,0,XERR_TYPE);
    check(xrtValueRetain(xrtValueNull()),false,false,0,XERR_TYPE);
    check(xrtValueArray(),false,false,0,XERR_TYPE);
    xvalue* scalar=xrtValueChar(65); assert(scalar && xrtValueTypeIdBind(scalar,CHAR_ID));
    check(scalar,true,true,65,XERR_NONE);
    scalar=xrtValueChar(65); assert(scalar && xrtValueTypeIdBind(scalar,CHAR_ID+1));
    check(scalar,true,false,0,XERR_TYPE);
    uint32_t output=17; assert(!xl6CharValueCheckedV1(NULL,CHAR_ID,false,&output) && output==17);
    assert(xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError();
    scalar=xrtValueChar(0); assert(scalar);
    assert(!xl6CharValueCheckedV1(scalar,0,false,&output) && output==17);
    assert(xrtErrorKind(xrtGetError())==XERR_ARGUMENT); xrtClearError();
    assert(!xl6CharValueCheckedV1(scalar,CHAR_ID,false,NULL));
    assert(xrtErrorKind(xrtGetError())==XERR_ARGUMENT); xrtClearError();
    xrtSetErrorKind(XERR_IO); const xerror* incoming=xrtGetError();
    assert(xl6CharValueCheckedV1(scalar,CHAR_ID,true,&output) && !output && xrtGetError()==incoming);
    xrtClearError();
    assert(xrtMemDebugFailAfter(0));
    assert(xl6CharValueCheckedV1(scalar,CHAR_ID,true,&output) && !output && !xrtMemDebugFailTriggered());
    xrtMemDebugFailClear(); xrtValueRelease(scalar);
}
static void callbacks(void)
{
    xvalue* source=xrtValueChar(0); assert(source);
    for (mode=1;mode<=5;++mode) {
        xl6_error_scope call={0}; assert(xl6ErrorScopeBegin(&call)); uint32_t output=17;
        assert(!xl6CharValueCheckedV1(source,CHAR_ID,true,&output) && output==17);
        xl6_error_result result=xl6ErrorScopeEnd(&call); assert(!result.ok);
        if (mode==1 || mode==2 || mode==5) assert(!result.error);
        else assert(result.error && xrtErrorKind(result.error)==(mode==3 ? XERR_IO : XERR_RANGE));
        xrtErrorFree(result.error); xrtClearError();
    }
    mode=0;
    for (mode=1;mode<=2;++mode) {
        xl6_error_scope call={0}; assert(xl6ErrorScopeBegin(&call)); uint32_t output=17;
        assert(xrtMemDebugFailAfter(0));
        assert(!xl6CharValueCheckedV1(source,CHAR_ID,true,&output) && output==17);
        assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
        xl6_error_result result=xl6ErrorScopeEnd(&call); assert(!result.ok && !result.error);
        xrtClearError();
    }
    mode=0; xrtValueRelease(source);
}
int main(void)
{
    assert(xrtMemDebugEnable(true)); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    vectors(); callbacks(); balanced(&before); assert(reads>10);
    puts("char checked domain/identity/status/zero-budget/first-failure balanced"); return 0;
}
