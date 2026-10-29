#include "xl6_text_value.h"
#include "xl6_foundation_abi.h"
#include "src/api/xl_foreign_error.h"

bool xl6MemoryDebugEnabledV1(void) { return xrtMemDebugEnabled(); }
void* xl6MemoryDebugCaptureV1(void)
{
    xmemdebugsnapshot value; xrtMemDebugSnapshot(&value);
    xmemdebugsnapshot* result = xrtMalloc(sizeof(*result));
    if (result) *result = value;
    return result;
}
void xl6MemoryDebugSnapshotDestroyV1(void* raw) { xrtFree(raw); }
bool xl6MemoryDebugSnapshotEnabledV1(void* raw)
{ return ((const xmemdebugsnapshot*)raw)->Enabled; }
#define XL6_MEMORY_DEBUG(Field) \
    uint64_t xl6MemoryDebugSnapshot##Field##V1(void* raw) { return ((const xmemdebugsnapshot*)raw)->Field; }
#include "xl6_memory_debug_fields.inc"
#undef XL6_MEMORY_DEBUG
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6MemoryDebugSnapshotHandlePolicyV1Get,xl6MemoryDebugSnapshotDestroyV1)
