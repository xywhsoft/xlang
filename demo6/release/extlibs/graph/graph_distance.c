#include "graph_internal.h"

typedef struct {
    double Distance;
    int Position;
    bool Done;
    bool Overflowed;
} distance;

/* 索引堆每个节点最多占一个槽；降低距离时原位上浮，不创建重复队列项。 */
static bool less(const distance* state, int a, int b)
{
    return state[a].Distance < state[b].Distance ||
        (state[a].Distance == state[b].Distance && a < b);
}

static void swap(int* heap, distance* state, int a, int b)
{
    int node = heap[a];
    heap[a] = heap[b];
    heap[b] = node;
    state[heap[a]].Position = a;
    state[heap[b]].Position = b;
}

static void offer(int* heap, distance* state, int* size, int node)
{
    int i = state[node].Position;
    if (i < 0) {
        i = (*size)++;
        heap[i] = node;
        state[node].Position = i;
    }
    while (i && less(state, heap[i], heap[(i - 1) / 2])) {
        int parent = (i - 1) / 2;
        swap(heap, state, i, parent);
        i = parent;
    }
}

static int pop(int* heap, distance* state, int* size)
{
    int node = heap[0], i = 0;
    state[node].Position = -1;
    if (!--*size) return node;
    heap[0] = heap[*size];
    state[heap[0]].Position = 0;
    while (2 * i + 1 < *size) {
        int child = 2 * i + 1;
        if (child + 1 < *size && less(state, heap[child + 1], heap[child])) ++child;
        if (!less(state, heap[child], heap[i])) break;
        swap(heap, state, i, child);
        i = child;
    }
    return node;
}

xvalue* xg_dijkstra(xvalue* edges, const char* start, size_t startSize)
{
    xgmodel g;
    distance* state = NULL;
    int *heap = NULL, size = 0, first;
    xvalue* result = NULL;
    if (!xgLoad(&g, edges)) return NULL;
    if ((first = xgFind(&g, start, startSize)) < 0) goto done;
    for (size_t i = 0; i < g.Edges.Count; ++i) {
        if (xgEdges(&g)[i].Weight < 0) {
            xgError(XERR_VALUE, "Dijkstra requires nonnegative weights");
            goto done;
        }
    }
    state = xrtCalloc(g.Nodes.Count, sizeof(*state));
    if (!state) goto done;
    heap = xrtCalloc(g.Nodes.Count, sizeof(*heap));
    if (!heap) goto done;
    for (size_t i = 0; i < g.Nodes.Count; ++i) {
        state[i].Distance = INFINITY;
        state[i].Position = -1;
    }
    state[first].Distance = 0;
    offer(heap, state, &size, first);
    while (size) {
        int from = pop(heap, state, &size);
        state[from].Done = true;
        for (int i = xgNodes(&g)[from].FirstOut; i >= 0; i = xgEdges(&g)[i].NextOut) {
            xgedge* edge = &xgEdges(&g)[i];
            int to = edge->To;
            double candidate;
            if (state[to].Done) continue;
            candidate = state[from].Distance + edge->Weight;
            if (!isfinite(candidate)) {
                state[to].Overflowed = true;
                continue;
            }
            if (candidate < state[to].Distance) {
                state[to].Distance = candidate;
                offer(heap, state, &size, to);
            }
        }
    }
    /* 溢出边只有在目标最终没有可表示的路径时才使整次查询失败。 */
    for (size_t i = 0; i < g.Nodes.Count; ++i) {
        if (state[i].Overflowed && !state[i].Done) {
            xgError(XERR_RANGE, "graph path distance overflows float64");
            goto done;
        }
    }
    result = xrtValueObject();
    if (!result) goto done;
    for (size_t i = 0; i < g.Nodes.Count; ++i) {
        xvalue* value;
        if (!state[i].Done) continue;
        value = xrtValueFloat(state[i].Distance);
        if (!value || !xrtValueObjectSetNew(result, xgNodes(&g)[i].Name, value)) {
            xrtValueRelease(result);
            result = NULL;
            break;
        }
    }
done:
    xrtFree(heap);
    xrtFree(state);
    xgUnit(&g);
    return result;
}
