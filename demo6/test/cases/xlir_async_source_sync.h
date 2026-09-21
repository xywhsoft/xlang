#ifndef XLIR_ASYNC_SOURCE_SYNC_H
#define XLIR_ASYNC_SOURCE_SYNC_H
/* Test-only observation of the resident executor. Never runs a continuation
 * inline, and never substitutes Future publication for physical retirement. */
void xlir_async_test_begin(void);
void xlir_async_test_wait_retired(void);
void xlir_async_test_wait_watches(unsigned count);
bool xlir_async_test_take_fault(void);
#ifdef XLIR_ASYNC_SOURCE_HOST
extern const XL6_CodeOwner* xlir_async_test_code_owner(void);
extern void xlir_async_test_pause(unsigned long long* started);
static void xlir_async_test_bind(void)
{
    static int bound;
    if (!bound) { assert(xl_script_bind_code_owner(xlir_async_test_code_owner())); bound = 1; }
}
static void xlir_async_test_fini(void)
{
    unsigned long long started = 0;
    xlir_async_test_wait_retired();
    /* The resident queue credit is released just AFTER the final code hook.
     * Honor that small busy window before trying the generated cleanup. */
    while (!xlir_continuation_quiesce(false)) xlir_async_test_pause(&started);
    xlir_module_fini(); assert(!xrtGetError());
}
#endif
#endif
