#ifndef GRAPH_TEST_H
#define GRAPH_TEST_H
#define XRT_MODULE_MEMORY_DEBUG
#include "../include/graph.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static inline xvalue* text(const char* data, size_t size)
{
    xvalue* v = xrtValueString((xstrview){data, size});
    assert(v);
    return v;
}

static inline void edge(xvalue* graph, const char* from, const char* to, double weight)
{
    xvalue* row = xrtValueArray();
    assert(row && xrtValueArrayAppendNew(row, text(from, strlen(from))));
    if (to) {
        assert(xrtValueArrayAppendNew(row, text(to, strlen(to))));
        assert(xrtValueArrayAppendNew(row, xrtValueFloat(weight)));
    }
    assert(xrtValueArrayAppendNew(graph, row));
}

static inline bool named(xvalue* value, const char* name, size_t size)
{
    xstrview got;
    assert(xrtValueGetString(value, &got));
    return got.Size == size && (!size || memcmp(got.Data, name, size) == 0);
}

static inline void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before->LiveCount && after.LiveBytes == before->LiveBytes);
    assert(after.InvalidFreeCount == before->InvalidFreeCount);
    assert(after.DoubleFreeCount == before->DoubleFreeCount);
    assert(after.OverflowCount == before->OverflowCount && after.UnderflowCount == before->UnderflowCount);
}

static inline xvalue* invoke(int operation, xvalue* graph)
{
    switch (operation) {
    case 1: return xg_topo_sort(graph);
    case 2: return xg_bfs(graph, "a", 1);
    case 3: return xg_shortest_path(graph, "a", 1, "c", 1);
    case 4: return xg_dijkstra(graph, "a", 1);
    case 5: return xg_components(graph);
    case 6: return xg_scc(graph);
    case 7: return xg_nodes(graph);
    case 8: return xg_neighbors(graph, "a", 1);
    default: assert(0); return NULL;
    }
}
#endif
