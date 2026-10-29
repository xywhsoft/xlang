#include "xlir_object_escape.generated.h"
#include "xlir_object_escape.generated.c"

typedef struct EscapeHooks {
    void (*capture)(xvalue*);
    void (*dropped)(int64_t);
} EscapeHooks;
static EscapeHooks escape_hooks;
__declspec(dllexport) void escape_bind(const EscapeHooks* hooks) { escape_hooks = *hooks; }
void escape_capture(xvalue* value) { escape_hooks.capture(value); }
void escape_dropped(int64_t id) { escape_hooks.dropped(id); }
