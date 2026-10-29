#include "graph_internal.h"

xvalue* xg_components(xvalue* edges)
{
    xgmodel g;
    int *work, *component, *queue, next = 0;
    xvalue* result = NULL;
    if (!xgLoad(&g, edges)) return NULL;
    work = xrtCalloc(g.Nodes.Count + 1, 2 * sizeof(*work));
    if (!work) goto done;
    component = work;
    queue = work + g.Nodes.Count;
    for (size_t i = 0; i < g.Nodes.Count; ++i) component[i] = -1;
    for (size_t root = 0; root < g.Nodes.Count; ++root) {
        int head = 0, tail = 0;
        if (component[root] >= 0) continue;
        component[root] = next;
        queue[tail++] = (int)root;
        while (head < tail) {
            int node = queue[head++];
            /* 弱连通分量同时沿出边和入边走，每条边最多检查两次。 */
            for (int direction = 0; direction < 2; ++direction) {
                int edge = direction ? xgNodes(&g)[node].FirstIn : xgNodes(&g)[node].FirstOut;
                while (edge >= 0) {
                    xgedge* e = &xgEdges(&g)[edge];
                    int other = direction ? e->From : e->To;
                    if (component[other] < 0) {
                        component[other] = next;
                        queue[tail++] = other;
                    }
                    edge = direction ? e->NextIn : e->NextOut;
                }
            }
        }
        ++next;
    }
    result = xgGroups(&g, component);
done:
    xrtFree(work);
    xgUnit(&g);
    return result;
}

/* 迭代 Kosaraju：显式栈取代递归，每个节点只存一个分量编号。 */
xvalue* xg_scc(xvalue* edges)
{
    xgmodel g;
    int *work, *visited, *cursor, *stack, *finish, *component;
    int finished = 0, next = 0;
    xvalue* result = NULL;
    if (!xgLoad(&g, edges)) return NULL;
    work = xrtCalloc(g.Nodes.Count + 1, 5 * sizeof(*work));
    if (!work) goto done;
    visited = work;
    cursor = visited + g.Nodes.Count;
    stack = cursor + g.Nodes.Count;
    finish = stack + g.Nodes.Count;
    component = finish + g.Nodes.Count;
    for (size_t i = 0; i < g.Nodes.Count; ++i) {
        cursor[i] = xgNodes(&g)[i].FirstOut;
        component[i] = -1;
    }
    for (size_t root = 0; root < g.Nodes.Count; ++root) {
        int top = 0;
        if (visited[root]) continue;
        visited[root] = 1;
        stack[top++] = (int)root;
        while (top) {
            int node = stack[top - 1], edge = cursor[node];
            if (edge < 0) {
                finish[finished++] = node;
                --top;
                continue;
            }
            cursor[node] = xgEdges(&g)[edge].NextOut;
            node = xgEdges(&g)[edge].To;
            if (!visited[node]) {
                visited[node] = 1;
                stack[top++] = node;
            }
        }
    }
    while (finished) {
        int root = finish[--finished], top = 0;
        if (component[root] >= 0) continue;
        component[root] = next;
        stack[top++] = root;
        while (top) {
            int node = stack[--top];
            for (int i = xgNodes(&g)[node].FirstIn; i >= 0; i = xgEdges(&g)[i].NextIn) {
                int from = xgEdges(&g)[i].From;
                if (component[from] >= 0) continue;
                component[from] = next;
                stack[top++] = from;
            }
        }
        ++next;
    }
    result = xgGroups(&g, component);
done:
    xrtFree(work);
    xgUnit(&g);
    return result;
}
