#include "stdlib_stack_ownership.generated.h"
#include "stdlib_stack_ownership.generated.c"
typedef struct StackGraphHooks {
    void (*capture)(xvalue*);
    void (*dropped)(int64_t,int64_t);
} StackGraphHooks;
static StackGraphHooks stackGraphHooks;
__declspec(dllexport) void stack_graph_bind(const StackGraphHooks* hooks) { stackGraphHooks = *hooks; }
void stack_graph_capture(xvalue* value) { stackGraphHooks.capture(value); }
void stack_graph_dropped(int64_t id, int64_t count) { stackGraphHooks.dropped(id,count); }
