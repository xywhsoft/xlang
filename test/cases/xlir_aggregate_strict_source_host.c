#include <stdio.h>

#if !defined(XLIR_GENERATED_MODULE_PREINCLUDED)
#error "The aggregate module must be preincluded into this translation unit"
#endif

/* The strict-source gate initializes module-owned storage only. These foreign
 * callbacks belong to the executable async fixture and must remain unreachable
 * until a machine is explicitly started. Keeping the stubs linkable proves the
 * generated declarations have a complete, ordinary C ABI. */
int64_t dependency_aggregate_mode(void) { return 0; }
xfuture* dependency_aggregate_ready(void) { return NULL; }
xfuture* dependency_boot(void) { return NULL; }
xfuture* dependency_peer(int64_t id) { (void)id; return NULL; }
void dependency_after(int64_t id) { (void)id; }
void dependency_callback(int64_t id, int64_t stage) { (void)id; (void)stage; }
void dependency_caught(int64_t id) { (void)id; }
void dependency_finally(int64_t id) { (void)id; }
void module_executor_dropped(void) {}

int main(void)
{
    if (!xlir_module_init()) {
        const xerror* error = xrtGetError();
        fprintf(stderr, "XLIR aggregate module initialization failed: %s\n",
            error != NULL ? xrtErrorMessage(error) : "no runtime error");
        return 1;
    }
    xlir_module_fini();
    return 0;
}
