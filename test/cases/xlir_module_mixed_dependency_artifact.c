#include "xlir_module_executor_artifact.c"
static void (*mixed_callback_hook)(int64_t, int64_t);
__declspec(dllexport) void mixed_dependency_bind(void (*callback)(int64_t, int64_t))
{ mixed_callback_hook = callback; }
void dependency_callback(int64_t id, int64_t stage)
{ mixed_callback_hook(id, stage); }
