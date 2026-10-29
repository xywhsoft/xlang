#include <__xl_runtime/xruntime_decl.h>
#include <assert.h>
static xvalueiter snapshot;
static xvalueiter* heapSnapshot;
void finalizer_snapshot_keep(xvalue* value, int64 mode)
{
    xvalue* clone = NULL;
    assert(!snapshot.Backing && !heapSnapshot);
    if (mode & 2) { clone = xrtValueClone(value); assert(clone); value = clone; }
    if (mode & 1) { heapSnapshot = xrtValueIterRCreate(value); assert(heapSnapshot); }
    else assert(xrtValueIterBegin(value, &snapshot));
    xrtValueRelease(clone);
}
void finalizer_snapshot_end(void)
{
    if (heapSnapshot) { xvalueiter* iterator = heapSnapshot; heapSnapshot = NULL; xrtValueIterDestroy(iterator); }
    else xrtValueIterEnd(&snapshot);
}
