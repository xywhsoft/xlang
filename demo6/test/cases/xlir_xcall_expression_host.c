#define xrtCallableCreate expression_create
#include "xlir_xcall_expression.generated.c"
#undef xrtCallableCreate
#include <assert.h>
extern xrtcallable* xrtCallableCreate(const xrtfunctionsig*, xrtcallproc, void*, xrtcalldrop);
extern bool xrtMemDebugFailAfter(uint64_t);
extern bool xrtMemDebugFailTriggered(void);
extern void xrtMemDebugFailClear(void);
static int inject_box_failure, injected_box_failures;
xrtcallable* expression_create(const xrtfunctionsig* signature, xrtcallproc entry, void* env, xrtcalldrop drop)
{
    xrtcallable* result;
    if (!inject_box_failure) return xrtCallableCreate(signature, entry, env, drop);
    inject_box_failure = 0;
    assert(xrtMemDebugFailAfter(0));
    result = xrtCallableCreate(signature, entry, env, drop);
    assert(!result && xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    ++injected_box_failures; return result;
}
int expression_case(void)
{
    xl6_foreign_error_scope scope = {0}; xl6_foreign_error_result result;
    int64_t value;
    assert(xl6ForeignCallBegin(&scope)); value = expression_run();
    result = xl6ForeignCallEnd(&scope);
    if (result.error) { fprintf(stderr, "expression failure: %s\n", xrtErrorMessage(result.error)); xrtErrorFree(result.error); }
    return result.ok && value == 42;
}
void expression_finish(void)
{ assert(xl_script_module_unit_checked()); xl_script_error_unit(); }
void expression_failure_case(void)
{
    xl6_foreign_error_scope scope = {0}; xl6_foreign_error_result result;
    int before = injected_box_failures;
    inject_box_failure = 1; assert(xl6ForeignCallBegin(&scope));
    assert(expression_box() == 0); result = xl6ForeignCallEnd(&scope);
    assert(!result.ok && result.error && injected_box_failures == before + 1);
    xrtErrorFree(result.error); assert(!xrtGetError());
    assert(expression_balanced()); /* the owned producer was released on error */
}
