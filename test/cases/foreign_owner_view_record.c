#include "foreign_owner_view_record.h"
#include <assert.h>

static int queries;
static xmemdebugsnapshot before;

int watched_record_seed(ManagedRoot* root)
{
    ManagedChild* second = NULL;
    ManagedChild* first = NULL;
    int status = managed_child_create(root, 0, &second);
    if (!status) status = managed_child_create(root, 0, &first);
    if (status) return status;
    first->value = 24;
    second->value = 58;
    return 0;
}

int watched_record_query(ManagedRoot* root, int mode, WatchedRecord* result)
{
    ++queries;
    if (mode == 3) {
        result->first = (ManagedChild*)(uintptr_t)1;
        result->second = result->first;
        return -19;
    }
    ManagedChild* first = managed_child_first(root);
    assert(first);
    *result = (WatchedRecord){
        mode == 2 ? NULL : first,
        mode == 1 ? NULL : mode == 4 ? first : first->next,
        "owned text", 47
    };
    return 0;
}

int watched_record_watches(ManagedRoot* root)
{
    int count = 0;
    for (ManagedChild* child = managed_child_first(root); child; child = child->next)
        for (ManagedWatch* watch = child->watches; watch; watch = watch->next)
            ++count;
    return count;
}

int watched_record_queries(void) { return queries; }

void watched_record_set_budget(int count)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    assert(xrtMemDebugFailAfter(count));
}

void watched_record_reset(void) { xrtMemDebugFailClear(); xrtClearError(); }

void watched_record_mark(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
}

void watched_record_balanced(void)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);
    assert(after.InvalidFreeCount == before.InvalidFreeCount && after.DoubleFreeCount == before.DoubleFreeCount);
}
