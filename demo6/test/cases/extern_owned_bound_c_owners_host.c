/* Track the foreign allocator independently from the XRT owner-record heap. */
#include <stdlib.h>
#include <stdint.h>
static uint64_t allocated,freed;
static void* tracked_malloc(size_t size) {void* p=malloc(size);if(p)++allocated;return p;}
static void tracked_free(void* p) {if(p)++freed;free(p);}
#define malloc tracked_malloc
#define free tracked_free
#include "xlir_c_owned_native.c"
#undef malloc
#undef free
bool bound_c_owners_balanced(void) {return allocated==freed;}
bool bound_c_owners_one_record(void)
{
    xmemstats s={0};xrtMemStatsGet(&s);
    return s.MallocCalls==1 && s.CallocCalls==0 && s.ReallocCalls==0 &&
        s.MemDupCalls==0 && s.FreeCalls==0;
}
