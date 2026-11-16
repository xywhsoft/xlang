#ifndef XL6_TEST_XGE_HOST_MEMORY_H
#define XL6_TEST_XGE_HOST_MEMORY_H
#include <__xl_runtime/xrt_decl.h>
/* Test instrumentation only, linked to the program's one real XRT. The OS
 * context fixture remains independent of XRT and the compiler. */
static inline void xgeTestHostHeap(size_t* live,size_t* bytes,uint64* allocations,
    uint64* frees,uint64* reallocations)
{
    xmemdebugsnapshot state={0};xrtMemDebugSnapshot(&state);
    *live=state.LiveCount;*bytes=state.LiveBytes;
    *allocations=state.AllocCount;*frees=state.FreeCount;*reallocations=state.ReallocCount;
}
static xmemdebugsnapshot xgeTestHostHeapBaseline;
static inline void xgeTestHostHeapMark(void)
{xrtMemDebugSnapshot(&xgeTestHostHeapBaseline);}
static inline void xgeTestHostHeapDelta(uint64* allocations,uint64* frees,uint64* reallocations)
{
    xmemdebugsnapshot state={0};xrtMemDebugSnapshot(&state);
    *allocations=state.AllocCount-xgeTestHostHeapBaseline.AllocCount;
    *frees=state.FreeCount-xgeTestHostHeapBaseline.FreeCount;
    *reallocations=state.ReallocCount-xgeTestHostHeapBaseline.ReallocCount;
}
#endif
