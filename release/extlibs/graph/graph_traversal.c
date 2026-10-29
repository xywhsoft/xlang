#include "graph_internal.h"

/* Kahn 队列同时就是拓扑序；只沿邻接边推进。 */
static int topological(xgmodel* g, int* order, int* degree)
{
    int head = 0, tail = 0;
    for (size_t i = 0; i < g->Edges.Count; ++i) ++degree[xgEdges(g)[i].To];
    for (size_t i = 0; i < g->Nodes.Count; ++i) if (!degree[i]) order[tail++] = (int)i;
    while (head < tail) {
        int node = order[head++];
        for (int i = xgNodes(g)[node].FirstOut; i >= 0; i = xgEdges(g)[i].NextOut) {
            int to = xgEdges(g)[i].To;
            if (!--degree[to]) order[tail++] = to;
        }
    }
    return tail;
}

bool xg_is_dag(xvalue* edges)
{
    xgmodel g;
    int* work;
    bool result = false;
    if (!xgLoad(&g, edges)) return false;
    work = xrtCalloc(g.Nodes.Count + 1, 2 * sizeof(*work));
    if (work) result = topological(&g, work, work + g.Nodes.Count) == (int)g.Nodes.Count;
    xrtFree(work);
    xgUnit(&g);
    return result;
}

xvalue* xg_topo_sort(xvalue* edges)
{
    xgmodel g;
    int* work;
    xvalue* result = NULL;
    if (!xgLoad(&g, edges)) return NULL;
    work = xrtCalloc(g.Nodes.Count + 1, 2 * sizeof(*work));
    if (work) {
        int count = topological(&g, work, work + g.Nodes.Count);
        if (count == (int)g.Nodes.Count) result = xgNodeList(&g, work, count);
        else xgError(XERR_VALUE, "graph has a cycle; topological sort is impossible");
    }
    xrtFree(work);
    xgUnit(&g);
    return result;
}

/* 一次 BFS 同时生成访问序与父节点，最短路无需重跑遍历。 */
static int breadthFirst(xgmodel* g, int start, int* order, int* parent)
{
    int head = 0, tail = 0;
    for (size_t i = 0; i < g->Nodes.Count; ++i) parent[i] = -2;
    parent[start] = -1;
    order[tail++] = start;
    while (head < tail) {
        int node = order[head++];
        for (int i = xgNodes(g)[node].FirstOut; i >= 0; i = xgEdges(g)[i].NextOut) {
            int to = xgEdges(g)[i].To;
            if (parent[to] != -2) continue;
            parent[to] = node;
            order[tail++] = to;
        }
    }
    return tail;
}

static xvalue* walk(xvalue* edges, const char* start, size_t startSize,
    const char* target, size_t targetSize, bool path)
{
    xgmodel g;
    int *work = NULL, *parent, s, t = -1, count;
    xvalue* result = NULL;
    if (!xgLoad(&g, edges)) return NULL;
    if ((s = xgFind(&g, start, startSize)) < 0) goto done;
    if (path && (t = xgFind(&g, target, targetSize)) < 0) goto done;
    work = xrtCalloc(g.Nodes.Count, 2 * sizeof(*work));
    if (!work) goto done;
    parent = work + g.Nodes.Count;
    count = breadthFirst(&g, s, work, parent);
    if (path) {
        if (parent[t] == -2) {
            xgError(XERR_VALUE, "no path connects the two nodes");
            goto done;
        }
        count = 0;
        for (int i = t; i >= 0; i = parent[i]) work[count++] = i;
        for (int i = 0; i < count / 2; ++i) {
            int saved = work[i];
            work[i] = work[count - 1 - i];
            work[count - 1 - i] = saved;
        }
    }
    result = xgNodeList(&g, work, count);
done:
    xrtFree(work);
    xgUnit(&g);
    return result;
}

xvalue* xg_bfs(xvalue* edges, const char* start, size_t startSize)
{
    return walk(edges, start, startSize, NULL, 0, false);
}

xvalue* xg_shortest_path(xvalue* edges, const char* start, size_t startSize,
    const char* target, size_t targetSize)
{
    return walk(edges, start, startSize, target, targetSize, true);
}
