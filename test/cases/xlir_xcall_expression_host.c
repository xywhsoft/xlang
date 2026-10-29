#include "xlir_xcall_expression.generated.c"
#include <assert.h>
extern bool xrtMemDebugFailAfter(uint64_t);
extern bool xrtMemDebugFailTriggered(void);
extern void xrtMemDebugFailClear(void);
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
    unsigned budget, failures = 0;
    bool succeeded = false;
    /* The callable box now lives behind the resident closure ABI, so a
     * preprocessor interposition cannot observe it. Sweep the real allocator
     * boundary instead, covering producer, environment, box and Value
     * rollback paths until one unarmed execution succeeds. */
    for (budget = 0; budget < 64; ++budget) {
        xl6_foreign_error_scope scope = {0};
        xl6_foreign_error_result result;
        int64_t boxed;
        bool triggered;
        assert(xrtMemDebugFailAfter(budget));
        assert(xl6ForeignCallBegin(&scope));
        boxed = expression_box();
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        result = xl6ForeignCallEnd(&scope);
        if (!triggered) {
            assert(result.ok && result.error == NULL && boxed == 42);
            succeeded = true;
            break;
        }
        assert(boxed == 0 && !result.ok && result.error != NULL);
        xrtErrorFree(result.error);
        ++failures;
        assert(!xrtGetError());
        assert(expression_balanced());
    }
    assert(succeeded && failures >= 4);
    assert(expression_balanced() && !xrtGetError());
}
