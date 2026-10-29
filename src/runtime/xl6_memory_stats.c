#include "xl6_text_value.h"
#include "xl6_foundation_abi.h"
#include "src/api/xl_foreign_error.h"

bool xl6MemoryStatisticsEnabledV1(void) { return xrtMemStatsEnabled(); }
void xl6MemoryStatsEnableV1(bool enabled) { xrtMemStatsEnable(enabled); }
void xl6MemoryStatsResetV1(void) { xrtMemStatsReset(); }
void* xl6MemoryStatsCaptureV1(void)
{
    /* Capture before allocating the immutable carrier, so the snapshot does
     * not count its own carrier allocation. All counters share one boundary. */
    xmemstats value; xrtMemStatsGet(&value);
    xmemstats* result = xrtMalloc(sizeof(*result));
    if (result) *result = value;
    return result;
}
void xl6MemoryStatsDestroyV1(void* raw) { xrtFree(raw); }
bool xl6MemoryStatsEnabledV1(void* raw) { return ((const xmemstats*)raw)->Enabled; }
#define XL6_MEMORY_STAT(Field) \
    uint64_t xl6MemoryStats##Field##V1(void* raw) { return ((const xmemstats*)raw)->Field; }
XL6_MEMORY_STAT(ClassStep)
XL6_MEMORY_STAT(ClassCutoff)
XL6_MEMORY_STAT(ClassCount)
#include "xl6_memory_stats_fields.inc"
#undef XL6_MEMORY_STAT
static bool valid_index(int64_t index)
{ if (index >= 0 && index < XRT_MEM_STATS_CLASS_COUNT) return true; xrtSetErrorKind(XERR_RANGE); return false; }
uint64_t xl6MemoryStatsClassCallsV1(void* raw, int64_t index)
{ return valid_index(index) ? ((const xmemstats*)raw)->ClassCalls[index] : 0; }
uint64_t xl6MemoryStatsClassBytesV1(void* raw, int64_t index)
{ return valid_index(index) ? ((const xmemstats*)raw)->ClassBytes[index] : 0; }
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6MemoryStatsHandlePolicyV1Get,xl6MemoryStatsDestroyV1)
