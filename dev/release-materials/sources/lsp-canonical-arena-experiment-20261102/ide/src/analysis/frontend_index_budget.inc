/* The shared visitor stays unchanged except this allocation-policy seam.
 * Legacy JSON keeps its 1536-byte per-record allowance. Native typed capture
 * charges actual arena capacity and reserves 64 bytes per node for the
 * canonical strings, including the explicit terminator; the original 6*length
 * charges conservatively cover the shared XRT buffer's geometric growth. Neither mode changes collected semantic facts. */
#include <limits.h>
static _Thread_local XL6_IndexBuild* xlsTypedIndexBuild;
static int xl6IndexBudget(XL6_IndexBuild*,size_t);
static int xlsIndexRecordBudget(XL6_IndexBuild* build)
{ return xl6IndexBudget(build,build==xlsTypedIndexBuild ? 64 : 1536); }
static size_t xlsIndexNodeLimit(const XL6_IndexBuild* build)
{ return build==xlsTypedIndexBuild ? UINT_MAX : XL6_INDEX_NODES; }
static size_t xlsIndexCapacity(XL6_IndexBuild* build,size_t capacity,size_t item,size_t initial)
{
    size_t available=(XL6_INDEX_BYTES-build->budget)/item;
    if(!available || capacity>=UINT_MAX) { build->error="semantic index exceeds byte limit"; return 0; }
    size_t increment=capacity ? capacity/2 : initial;
    if(!increment) increment=1;
    if(increment>available) increment=available;
    if(increment>UINT_MAX-capacity) increment=UINT_MAX-capacity;
    return capacity+increment;
}
static int xlsIndexReserveFact(XL6_IndexBuild* build)
{
    if(build!=xlsTypedIndexBuild || build->count<build->factsCapacity) return 1;
    size_t capacity=xlsIndexCapacity(build,build->factsCapacity,sizeof(*build->facts),256);
    if(!capacity) return 0;
    XL6_IndexFact* facts=xrtRealloc(build->facts,capacity*sizeof(*facts));
    if(!facts) { build->error="cannot grow semantic index facts"; return 0; }
    size_t added=capacity-build->factsCapacity;
    memset(facts+build->factsCapacity,0,added*sizeof(*facts));
    build->facts=facts; build->factsCapacity=capacity;
    return xl6IndexBudget(build,added*sizeof(*facts));
}
static int xlsIndexReserveSource(XL6_IndexBuild* build)
{
    if(build!=xlsTypedIndexBuild) return build->sourceCount<XL6_INDEX_NODES;
    if(build->sourceCount<build->sourcesCapacity) return 1;
    size_t capacity=xlsIndexCapacity(build,build->sourcesCapacity,sizeof(*build->sources),16);
    if(!capacity) return 0;
    XL6_IndexSource* sources=xrtRealloc(build->sources,capacity*sizeof(*sources));
    if(!sources) { build->error="cannot grow semantic index sources"; return 0; }
    size_t added=capacity-build->sourcesCapacity;
    memset(sources+build->sourcesCapacity,0,added*sizeof(*sources));
    build->sources=sources; build->sourcesCapacity=capacity;
    return xl6IndexBudget(build,added*sizeof(*sources));
}

/* Native collection stores offsets until all appends complete. Legacy JSON
 * continues to own and free each original canonical allocation. */
#include "analysis/frontend_type_arena.h"
static _Thread_local xstrbuf* xlsTypedCanonicalArena;
static int xlsIndexCaptureCanonical(XL6_IndexBuild* b,XL6_IndexFact* f)
{
    size_t length=0;
    if(b==xlsTypedIndexBuild && xlsTypedCanonicalArena){
        if(!xlsFrontendTypeCanonicalAppend(b->root,f->type,8192,128,8192,xlsTypedCanonicalArena,XL6_INDEX_BYTES,&f->canonicalOffset,&length))goto failed;
    }else{
        f->canonical=xl6TypeToCanonicalStringLimited(b->root,f->type,8192,128,8192);
        if(!f->canonical)goto failed;
        length=strlen(f->canonical);
    }
    return xl6IndexBudget(b,length*6);
failed:b->error="canonical semantic type exceeds limits or allocation failed";return 0;
}
