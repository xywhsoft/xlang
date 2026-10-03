/* Real unloadable provider. Only the generated program lives here; all XRT
 * values, Process operations and continuation machinery are host-resident. */
#include PROCESS_GENERATED_H
#include PROCESS_GENERATED_C
#include PROCESS_BINDING_H
typedef struct ProcessLifetimeHooks { void (*finally)(void); } ProcessLifetimeHooks;
static ProcessLifetimeHooks hooks;
XLIR_C_API void process_lifetime_bind(const ProcessLifetimeHooks* input) { hooks=*input; }
void process_lifetime_finally(void) { hooks.finally(); }
bool process_lifetime_launch(void) { return false; }
/* Test-only canonical entry, not an extension to the public C export ABI. */
XLIR_C_API xvalue* process_lifetime_create(void)
{
    xlir_result_xvalue result=PROCESS_CREATE();
    return result.ok ? result.value : NULL;
}
XLIR_C_API xfuture* process_lifetime_start(xvalue* process)
{
    xlir_result_future result=PROCESS_RUN(process);
    return result.ok ? result.value : NULL;
}
XLIR_C_API XL6_FuturePayloadDrop process_lifetime_drop_address(void)
{ return xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_XVALUE); }
XLIR_C_API const xrtownershipops* process_lifetime_callable_identity(void)
{ return xrtCallableOwnership(NULL).Ops; }
