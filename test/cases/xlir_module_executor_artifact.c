/* Generated module owns its unmodified executor global. These hooks only
 * bridge foreign calls and the verified private machine entry; they never
 * replace ReserveOwned, select an executor or add hidden owning fields. */
#include "xlir_module_executor.generated.h"
#include "xlir_module_executor.generated.c"
#include "xlir_module_executor.binding.h"
typedef struct ModuleExecutorHooks {
    xfuture* (*peer)(int64_t);
    void (*caught)(int64_t);
    void (*finally)(int64_t);
    void (*after)(int64_t);
    void (*dropped)(void);
} ModuleExecutorHooks;
static ModuleExecutorHooks module_executor_hooks;
__declspec(dllexport) void module_executor_bind(const ModuleExecutorHooks* hooks) { module_executor_hooks = *hooks; }
__declspec(dllexport) xfuture* module_executor_start(int64_t id, xfuture* boot, xfuture* cleanup, bool waitCleanup)
{
    xlir_result_future result = MODULE_EXECUTOR_START(id, boot, cleanup, waitCleanup);
    return result.ok ? result.value : NULL;
}
__declspec(dllexport) xrtownershipref module_executor_view(void)
{ return xl6ContinuationExecutorOwnership(xlir_continuation_executor); }
xfuture* dependency_peer(int64_t id) { return module_executor_hooks.peer(id); }
void dependency_caught(int64_t id) { module_executor_hooks.caught(id); }
void dependency_finally(int64_t id) { module_executor_hooks.finally(id); }
void dependency_after(int64_t id) { module_executor_hooks.after(id); }
void module_executor_dropped(void) { module_executor_hooks.dropped(); }
xfuture* dependency_boot(void) { abort(); return NULL; }
