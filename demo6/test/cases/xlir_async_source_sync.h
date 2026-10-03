#ifndef XLIR_ASYNC_SOURCE_SYNC_H
#define XLIR_ASYNC_SOURCE_SYNC_H
/* Test-only observation of the resident executor. Never runs a continuation
 * inline, and never substitutes Future publication for physical retirement. */
void xlir_async_test_begin(void);
void xlir_async_test_wait_retired(void);
void xlir_async_test_wait_watches(unsigned count);
bool xlir_async_test_take_fault(void);
bool xlir_async_test_execution_idle(void);
void xlir_async_test_assert_owner_released(void);
#ifdef XLIR_ASYNC_SOURCE_HOST
extern const XL6_CodeOwner* xlir_async_test_code_owner(void);
extern void xlir_async_test_pause(unsigned long long* started);
bool xlir_async_test_execution_idle(void)
{
    return xl6ContinuationExecutorIsIdle(xlir_continuation_executor);
}
static void xlir_async_test_bind(void)
{
    static int bound;
    if (!bound) {
        if (xlir_code_owner.size != 0) {
            /* CLI/JIT/AOT binds the real provider before calling main. Keep
             * that immutable capability; only the standalone native lane
             * uses our lease/fault observer. Prove replacement is refused. */
            XL6_CodeOwner original = xlir_code_owner;
            const struct xrtownershipops* ops = xlir_code_owner_graph.ops;
            assert(original.size >= sizeof(original) && original.acquire && original.release);
            assert(!xl_script_bind_code_owner(xlir_async_test_code_owner()));
            assert(xlir_code_owner.size == original.size &&
                xlir_code_owner.context == original.context &&
                xlir_code_owner.acquire == original.acquire &&
                xlir_code_owner.release == original.release && xlir_code_owner_graph.ops == ops);
        } else {
            assert(xl_script_bind_code_owner(xlir_async_test_code_owner()));
        }
        bound = 1;
    }
}
static void xlir_async_test_fini(void)
{
    unsigned long long started = 0;
    xlir_async_test_wait_retired();
    /* The resident queue credit is released just AFTER the final code hook.
     * Honor that small busy window before trying the generated cleanup. */
    while (!xlir_continuation_quiesce(false)) xlir_async_test_pause(&started);
    xlir_module_fini(); assert(!xrtGetError());
    xlir_async_test_assert_owner_released();
}
#endif
#endif
