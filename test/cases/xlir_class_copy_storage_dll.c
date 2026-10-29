#include "xlir_class_storage_dll.c"
#include <stdint.h>
#include <stdbool.h>
static unsigned* hash_calls;
__declspec(dllexport) void class_copy_bind_counter(unsigned* calls) { hash_calls = calls; }
__declspec(dllexport) uint64_t class_copy_hash(const struct xvalue* value, void* data)
{
    (void)value; (void)data;
    ++*hash_calls;
    return UINT64_C(0x4c494645); /* Actual generated tail, called after Close. */
}
__declspec(dllexport) bool class_copy_equal(const struct xvalue* left, const struct xvalue* right, void* data)
{ (void)left; (void)right; (void)data; return true; }
