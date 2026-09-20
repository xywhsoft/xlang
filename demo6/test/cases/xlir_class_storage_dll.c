#include "xlir_class_storage_dll.h"
static ClassStorageHooks* hooks;
__declspec(dllexport) void class_storage_bind(ClassStorageHooks* value) { hooks = value; }
__declspec(dllexport) void class_storage_finalize(struct xvalue* value, void* unused)
{
    (void)unused;
    hooks->finalize(value, hooks->data);
    ++hooks->returned; /* This generated tail must return before unmapping. */
}
