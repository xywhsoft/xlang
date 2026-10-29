/* Preinclude the real generated module and its IR-derived export bindings. */
#include "test/support/module_product_memory.h"
#include "release/include/xrt_decl.h"
#include <assert.h>
#include <stdio.h>

static unsigned fieldFault, fieldCalls, scopeFault, scopeCalls;
static unsigned bodyFault, bodyCalls;
static bool noDiagnostic;

extern bool __real_xrtValueGetHandle(const xvalue*, void**, const xvaluehandleops**, void**);
bool __wrap_xrtValueGetHandle(const xvalue* value, void** output,
    const xvaluehandleops** ops, void** user)
{
    if (++fieldCalls == fieldFault) {
        if (noDiagnostic) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
        else xrtSetErrorTake(xlir_error_create("TypeError", "injected native field failure"));
        return false;
    }
    return __real_xrtValueGetHandle(value, output, ops, user);
}
extern bool __real_xl6ForeignCallBegin(xl6_foreign_error_scope*);
bool __wrap_xl6ForeignCallBegin(xl6_foreign_error_scope* scope)
{
    if (++scopeCalls == scopeFault) { xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL}); return false; }
    return __real_xl6ForeignCallBegin(scope);
}
extern int64_t __real_xl6BufferLengthV1(void*);
int64_t __wrap_xl6BufferLengthV1(void* buffer)
{
    ++bodyCalls;
    int64_t result=__real_xl6BufferLengthV1(buffer);
    if (bodyFault) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    return result;
}
extern xstrview __real_xl6BufferTextV1(void*, int64_t);
xstrview __wrap_xl6BufferTextV1(void* buffer, int64_t size)
{
    ++bodyCalls;
    xstrview result=__real_xl6BufferTextV1(buffer, size);
    if (bodyFault) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    return result;
}
extern bool __real_xl6BufferAppendBufferV1(void*, void*, int64_t);
bool __wrap_xl6BufferAppendBufferV1(void* target, void* source, int64_t size)
{
    ++bodyCalls;
    return __real_xl6BufferAppendBufferV1(target, source, size);
}
static void clearFaults(void)
{
    fieldFault=fieldCalls=scopeFault=scopeCalls=bodyFault=bodyCalls=0;
    noDiagnostic=true;
}
static void attempt(unsigned mode)
{
    clearFaults();
    if (mode==1 || mode==2) { fieldFault=1; noDiagnostic=mode==1; }
    else if (mode==3 || mode==4) bodyFault=1;
    else if (mode==5) fieldFault=2;
    else if (mode>=6) scopeFault=mode-5;
    xl6ProductMemoryBegin(mode==1 ? 0 : SIZE_MAX);
    xl6_error_scope transaction={0};
    assert(xl6ErrorScopeBegin(&transaction));
    if (mode==4) {
        xstrview result=NATIVE_TEXT_CALL();
        assert(!result.Data && !result.Size);
    } else if (mode==5) assert(!NATIVE_APPEND_CALL());
    else {
        int64_t result=NATIVE_SIZE_CALL();
        assert(result==(mode ? 0 : 5));
    }
    xl6_error_result outcome=xl6ErrorScopeEnd(&transaction);
    bool budgetHit=xl6ProductMemoryEnd();
    assert(!budgetHit);
    assert(outcome.ok==(mode==0));
    if (mode==2) assert(outcome.error && xrtErrorKind(outcome.error)==XERR_TYPE);
    else assert(!outcome.error);
    assert(bodyCalls==(mode==0 || mode==3 || mode==4 ? 1u : 0u));
    xrtErrorFree(outcome.error); xrtClearError();
    xl6ProductMemoryVerify();
    printf("native dynamic fault mode=%u scopes=%u fields=%u balanced\n",mode,scopeCalls,fieldCalls);
}
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    xmemdebugsnapshot before,after;
    xrtMemDebugSnapshot(&before);
    assert(xlir_module_init());
    assert(!xl6ProductMemoryEnd());
    attempt(0);
    unsigned scopes=scopeCalls;
    assert(scopes==3); /* Export entry, dynamic invocation, native witness. */
    for (unsigned mode=1;mode<=5+scopes;++mode) attempt(mode);
    clearFaults();
    xlir_module_fini();
    assert(!xrtGetError() && !xlir_module_initialized);
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.DoubleFreeCount==after.DoubleFreeCount && before.InvalidFreeCount==after.InvalidFreeCount);
    assert(before.UseAfterFreeCount==after.UseAfterFreeCount);
    puts("native dynamic module teardown balanced");
    return 0;
}
