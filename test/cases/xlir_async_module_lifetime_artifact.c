/* This entire TU is a genuinely unloadable DLL. Continuation runtime functions
 * are imported from the resident test executable, never compiled into here. */
#ifndef JOINT_GENERATED_H
#define JOINT_GENERATED_H "xlir_async_module_lifetime.generated.h"
#define JOINT_GENERATED_C "xlir_async_module_lifetime.generated.c"
#define JOINT_BINDING_H "xlir_async_module_lifetime.binding.h"
#endif
#include JOINT_GENERATED_H
#include JOINT_GENERATED_C
#include JOINT_BINDING_H
/* Test-only canonical machine entry, not a claim that public language C
 * exports accept Future parameters/results. Host entry lease is real. */
__declspec(dllexport) xfuture* joint_start_test(xfuture* first, xfuture* second)
{
    xlir_result_future result = JOINT_RUN(first, second);
    return result.ok ? result.value : NULL;
}
__declspec(dllexport) XL6_FuturePayloadDrop joint_payload_drop_address(void)
{ return xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_VALUE); }
typedef struct JointHooks {
    int fail_init;
    xfuture* inputs[2];
    int64_t (*probe)(int64_t);
    void (*finally)(void);
    int fail_main;
    void (*entry_probe)(void);
    void (*global_drop)(void);
} JointHooks;
static JointHooks hooks;
__declspec(dllexport) void joint_bind_hooks(const JointHooks* input) { hooks = *input; }
bool joint_fail_init(void) { return hooks.fail_init != 0; }
xfuture* joint_input(int64_t index) { if (hooks.entry_probe) hooks.entry_probe(); return xrtFutureRef(hooks.inputs[index]); }
int64_t joint_probe(int64_t value) { return hooks.probe(value); }
void joint_finally(void) { hooks.finally(); }
bool joint_fail_main(void) { return hooks.fail_main == 1; }
bool joint_skip_async(void) { return hooks.fail_main == 2; }
void joint_global_drop(void) { if (hooks.global_drop) hooks.global_drop(); }
