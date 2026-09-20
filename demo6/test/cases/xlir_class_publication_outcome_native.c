#include <__xl_runtime/xruntime_decl.h>
#include <assert.h>
static xvalueiter snapshot;
void publication_snapshot(xvalue* value, bool keep)
{
    assert(!snapshot.Backing);
    if (keep) assert(xrtValueIterBegin(value, &snapshot));
}
void publication_snapshot_end(void)
{
    assert(snapshot.Backing); xrtValueIterEnd(&snapshot);
}
