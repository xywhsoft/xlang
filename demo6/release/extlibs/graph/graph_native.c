/*
 * graph_native.c - Graph algorithms over edge lists with string nodes.
 *
 * Edges cross the ABI as an array of [u, v] or [u, v, weight] arrays;
 * missing weights default to 1. Traversal algorithms are iterative with
 * explicit stacks so deep graphs cannot overflow the call stack. Nodes
 * are interned in insertion order and results preserve deterministic
 * ordering (sorted for node sets, insertion order for traversal output).
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"

#define XG_DOMAIN "xlang.extlib.graph"
#define XG_MAX_NODES 4096
#define XG_MAX_EDGES 32768

static void xg_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XG_DOMAIN, 0, message);
}

static xvalue *xg_text(const char *s, size_t n) {
    str owned = (str)xrtMalloc(n + 1);
    xvalue *value;
    if (owned == NULL) return NULL;
    if (n > 0) memcpy(owned, s, n);
    owned[n] = '\0';
    value = xrtValueStringTake(&owned, n);
    if (value == NULL && owned != NULL) xrtFree(owned);
    return value;
}

static xvalue *xg_null(void) {
    return xrtValueNull();
}

/* ---------------- graph model ---------------- */

typedef struct {
    char *nodes[XG_MAX_NODES];
    int nodeCount;
    int from[XG_MAX_EDGES];
    int to[XG_MAX_EDGES];
    double weight[XG_MAX_EDGES];
    int edgeCount;
} XGraph;

static void xg_free(XGraph *g) {
    int i;
    for (i = 0; i < g->nodeCount; i++) {
        xrtFree(g->nodes[i]);
    }
}

static int xg_intern(XGraph *g, const char *name) {
    int i;
    for (i = 0; i < g->nodeCount; i++) {
        if (strcmp(g->nodes[i], name) == 0) {
            return i;
        }
    }
    if (g->nodeCount >= XG_MAX_NODES) return -1;
    g->nodes[g->nodeCount] = (char *)xrtMalloc(strlen(name) + 1);
    if (g->nodes[g->nodeCount] == NULL) return -1;
    strcpy(g->nodes[g->nodeCount], name);
    return g->nodeCount++;
}

/* Copy a string element out of an array value. */
static char *xg_read_str(xvalue *item) {
    xstrview text;
    char *copy;
    if (item == NULL || !xrtValueGetString(item, &text)) {
        return NULL;
    }
    copy = (char *)xrtMalloc(text.Size + 1);
    if (copy == NULL) return NULL;
    memcpy(copy, text.Data, text.Size);
    copy[text.Size] = '\0';
    return copy;
}

static int xg_read_num(xvalue *item, double *out) {
    int64 i;
    double d;
    if (item == NULL || !xrtValueIsNumber(item)) return 0;
    if (xrtValueGetFloat(item, &d)) {
        *out = d;
        return 1;
    }
    if (xrtValueGetInt(item, &i)) {
        *out = (double)i;
        return 1;
    }
    return 0;
}

/* Parse the edge list into the graph model. Returns 0 on failure. */
static int xg_load(xvalue *edges, XGraph *g) {
    int64 i;
    int64 n;
    memset(g, 0, sizeof(*g));
    if (edges == NULL || xrtValueType(edges) != XVALUE_ARRAY) {
        xg_error("edges must be an array of [u, v] or [u, v, weight]");
        return 0;
    }
    n = xrtValueCount(edges);
    for (i = 0; i < n; i++) {
        xvalue *edge = xrtValueArrayAt(edges, i);
        xvalue *uItem;
        xvalue *vItem;
        char *u;
        char *v;
        double w = 1.0;
        int ui;
        int vi;
        int64 arity;
        if (edge == NULL || xrtValueType(edge) != XVALUE_ARRAY) {
            xg_error("each edge must be [u], [u, v] or [u, v, weight]");
            return 0;
        }
        arity = xrtValueCount(edge);
        if (arity < 1) {
            xg_error("each edge must be [u], [u, v] or [u, v, weight]");
            return 0;
        }
        uItem = xrtValueArrayAt(edge, 0);
        u = xg_read_str(uItem);
        if (u == NULL) {
            xg_error("edge endpoints must be strings");
            return 0;
        }
        ui = xg_intern(g, u);
        xrtFree(u);
        if (ui < 0) {
            xg_error("graph is too large");
            return 0;
        }
        if (arity == 1) {
            /* single-element edge declares an isolated node */
            continue;
        }
        vItem = xrtValueArrayAt(edge, 1);
        v = xg_read_str(vItem);
        if (v == NULL) {
            xg_error("edge endpoints must be strings");
            return 0;
        }
        if (arity >= 3) {
            if (!xg_read_num(xrtValueArrayAt(edge, 2), &w)) {
                xrtFree(v);
                xg_error("edge weight must be a number");
                return 0;
            }
        }
        vi = xg_intern(g, v);
        xrtFree(v);
        if (vi < 0 || g->edgeCount >= XG_MAX_EDGES) {
            xg_error("graph is too large");
            return 0;
        }
        g->from[g->edgeCount] = ui;
        g->to[g->edgeCount] = vi;
        g->weight[g->edgeCount] = w;
        g->edgeCount++;
    }
    return 1;
}

static xvalue *xg_node_list(const XGraph *g, const int *order, int count) {
    xvalue *result = xrtValueArray();
    int i;
    if (result == NULL) return xg_null();
    for (i = 0; i < count; i++) {
        const char *name = g->nodes[order[i]];
        if (!xrtValueArrayAppendNew(result, xg_text(name, strlen(name)))) {
            return xg_null();
        }
    }
    return result;
}

/* ---------------- public API ---------------- */

long long xg_is_dag(xvalue *edges) {
    XGraph g;
    int indegree[XG_MAX_NODES];
    int queue[XG_MAX_NODES];
    int head = 0;
    int tail = 0;
    int processed = 0;
    int i;
    if (!xg_load(edges, &g)) return -1;
    memset(indegree, 0, sizeof(indegree));
    for (i = 0; i < g.edgeCount; i++) {
        indegree[g.to[i]]++;
    }
    for (i = 0; i < g.nodeCount; i++) {
        if (indegree[i] == 0) queue[tail++] = i;
    }
    while (head < tail) {
        int u = queue[head++];
        processed++;
        for (i = 0; i < g.edgeCount; i++) {
            if (g.from[i] == u) {
                if (--indegree[g.to[i]] == 0) queue[tail++] = g.to[i];
            }
        }
    }
    xg_free(&g);
    return processed == g.nodeCount;
}

xvalue *xg_topo_sort(xvalue *edges) {
    XGraph g;
    int indegree[XG_MAX_NODES];
    int queue[XG_MAX_NODES];
    int order[XG_MAX_NODES];
    int head = 0;
    int tail = 0;
    int processed = 0;
    int i;
    xvalue *result;
    if (!xg_load(edges, &g)) return xg_null();
    memset(indegree, 0, sizeof(indegree));
    for (i = 0; i < g.edgeCount; i++) {
        indegree[g.to[i]]++;
    }
    for (i = 0; i < g.nodeCount; i++) {
        if (indegree[i] == 0) queue[tail++] = i;
    }
    while (head < tail) {
        int u = queue[head++];
        order[processed++] = u;
        for (i = 0; i < g.edgeCount; i++) {
            if (g.from[i] == u) {
                if (--indegree[g.to[i]] == 0) queue[tail++] = g.to[i];
            }
        }
    }
    if (processed != g.nodeCount) {
        xg_free(&g);
        xg_error("graph has a cycle; topological sort is impossible");
        return xg_null();
    }
    result = xg_node_list(&g, order, processed);
    xg_free(&g);
    return result;
}

/* Shared BFS returning parent array; start is a node name. */
static int xg_find_node(const XGraph *g, const char *name) {
    int i;
    for (i = 0; i < g->nodeCount; i++) {
        if (strcmp(g->nodes[i], name) == 0) return i;
    }
    return -1;
}

static void xg_bfs_parents(const XGraph *g, int start, int *parent) {
    int queue[XG_MAX_NODES];
    int head = 0;
    int tail = 0;
    int i;
    for (i = 0; i < g->nodeCount; i++) parent[i] = -2;
    parent[start] = -1;
    queue[tail++] = start;
    while (head < tail) {
        int u = queue[head++];
        for (i = 0; i < g->edgeCount; i++) {
            if (g->from[i] == u && parent[g->to[i]] == -2) {
                parent[g->to[i]] = u;
                queue[tail++] = g->to[i];
            }
        }
    }
}

xvalue *xg_shortest_path(xvalue *edges, const char *start, const char *end) {
    XGraph g;
    int parent[XG_MAX_NODES];
    int path[XG_MAX_NODES];
    int len = 0;
    int s;
    int e;
    int cur;
    xvalue *result;
    if (!xg_load(edges, &g)) return xg_null();
    s = xg_find_node(&g, start);
    e = xg_find_node(&g, end);
    if (s < 0 || e < 0) {
        xg_free(&g);
        xg_error("start or end node is not in the graph");
        return xg_null();
    }
    xg_bfs_parents(&g, s, parent);
    if (parent[e] == -2) {
        xg_free(&g);
        xg_error("no path connects the two nodes");
        return xg_null();
    }
    cur = e;
    while (cur != -1) {
        path[len++] = cur;
        cur = parent[cur];
    }
    /* reverse */
    {
        int i;
        for (i = 0; i < len / 2; i++) {
            int tmp = path[i];
            path[i] = path[len - 1 - i];
            path[len - 1 - i] = tmp;
        }
    }
    result = xg_node_list(&g, path, len);
    xg_free(&g);
    return result;
}

xvalue *xg_bfs(xvalue *edges, const char *start) {
    XGraph g;
    int parent[XG_MAX_NODES];
    int order[XG_MAX_NODES];
    int count = 0;
    int s;
    int i;
    xvalue *result;
    if (!xg_load(edges, &g)) return xg_null();
    s = xg_find_node(&g, start);
    if (s < 0) {
        xg_free(&g);
        xg_error("start node is not in the graph");
        return xg_null();
    }
    xg_bfs_parents(&g, s, parent);
    /* report visit order by replaying the BFS queue */
    {
        int queue[XG_MAX_NODES];
        int head = 0;
        int tail = 0;
        int seen[XG_MAX_NODES];
        memset(seen, 0, sizeof(seen));
        seen[s] = 1;
        queue[tail++] = s;
        while (head < tail) {
            int u = queue[head++];
            order[count++] = u;
            for (i = 0; i < g.edgeCount; i++) {
                if (g.from[i] == u && !seen[g.to[i]]) {
                    seen[g.to[i]] = 1;
                    queue[tail++] = g.to[i];
                }
            }
        }
    }
    result = xg_node_list(&g, order, count);
    xg_free(&g);
    return result;
}

xvalue *xg_dijkstra(xvalue *edges, const char *start) {
    XGraph g;
    double dist[XG_MAX_NODES];
    int done[XG_MAX_NODES];
    int s;
    int i;
    xvalue *result;
    if (!xg_load(edges, &g)) return xg_null();
    s = xg_find_node(&g, start);
    if (s < 0) {
        xg_free(&g);
        xg_error("start node is not in the graph");
        return xg_null();
    }
    for (i = 0; i < g.nodeCount; i++) {
        dist[i] = 1e300;
        done[i] = 0;
    }
    dist[s] = 0;
    while (1) {
        int u = -1;
        double best = 1e300;
        int k;
        for (k = 0; k < g.nodeCount; k++) {
            if (!done[k] && dist[k] < best) {
                best = dist[k];
                u = k;
            }
        }
        if (u < 0) break;
        done[u] = 1;
        for (k = 0; k < g.edgeCount; k++) {
            if (g.from[k] == u) {
                double cand = dist[u] + g.weight[k];
                if (cand < dist[g.to[k]]) {
                    dist[g.to[k]] = cand;
                }
            }
        }
    }
    result = xrtValueObject();
    if (result == NULL) {
        xg_free(&g);
        return xg_null();
    }
    for (i = 0; i < g.nodeCount; i++) {
        if (dist[i] < 1e300) {
            xstrview view;
            view.Data = g.nodes[i];
            view.Size = strlen(g.nodes[i]);
            xrtValueObjectSetNew(result, view,
                xrtValueFloat(dist[i]));
        }
    }
    xg_free(&g);
    return result;
}

xvalue *xg_components(xvalue *edges) {
    XGraph g;
    int comp[XG_MAX_NODES];
    int order[XG_MAX_NODES];
    int compCount = 0;
    int i;
    xvalue *result;
    if (!xg_load(edges, &g)) return xg_null();
    memset(comp, -1, sizeof(comp));
    /* undirected reachability by BFS per unvisited node */
    for (i = 0; i < g.nodeCount; i++) {
        if (comp[i] >= 0) continue;
        {
            int queue[XG_MAX_NODES];
            int head = 0;
            int tail = 0;
            int k;
            comp[i] = compCount;
            queue[tail++] = i;
            while (head < tail) {
                int u = queue[head++];
                for (k = 0; k < g.edgeCount; k++) {
                    int other = -1;
                    if (g.from[k] == u) other = g.to[k];
                    else if (g.to[k] == u) other = g.from[k];
                    if (other >= 0 && comp[other] < 0) {
                        comp[other] = compCount;
                        queue[tail++] = other;
                    }
                }
            }
        }
        compCount++;
    }
    result = xrtValueArray();
    if (result == NULL) {
        xg_free(&g);
        return xg_null();
    }
    {
        int c;
        for (c = 0; c < compCount; c++) {
            xvalue *group;
            int count = 0;
            for (i = 0; i < g.nodeCount; i++) {
                if (comp[i] == c) order[count++] = i;
            }
            group = xg_node_list(&g, order, count);
            if (group == NULL || !xrtValueArrayAppendNew(result, group)) {
                xg_free(&g);
                return xg_null();
            }
        }
    }
    xg_free(&g);
    return result;
}

/* Tarjan strongly connected components, iterative. */
xvalue *xg_scc(xvalue *edges) {
    XGraph g;
    int index[XG_MAX_NODES];
    int low[XG_MAX_NODES];
    int onStack[XG_MAX_NODES];
    int stackArr[XG_MAX_NODES];
    int stackTop = 0;
    int nextIndex = 0;
    int order[XG_MAX_NODES];
    /* component buckets are large; keep them off the call stack */
    static int comps[XG_MAX_NODES][XG_MAX_NODES / 8 + 8];
    int compSize[XG_MAX_NODES];
    int compCount = 0;
    int i;
    xvalue *result;
    if (!xg_load(edges, &g)) return xg_null();
    memset(index, -1, sizeof(index));
    memset(low, 0, sizeof(low));
    memset(onStack, 0, sizeof(onStack));
    memset(compSize, 0, sizeof(compSize));
    /* iterative Tarjan with an explicit call frame stack */
    {
        int frameNode[XG_MAX_NODES * 2];
        int frameEdge[XG_MAX_NODES * 2];
        int frameTop = 0;
        for (i = 0; i < g.nodeCount; i++) {
            if (index[i] >= 0) continue;
            frameNode[frameTop] = i;
            frameEdge[frameTop] = 0;
            frameTop++;
            while (frameTop > 0) {
                int v = frameNode[frameTop - 1];
                int ei = frameEdge[frameTop - 1];
                if (index[v] < 0) {
                    index[v] = nextIndex;
                    low[v] = nextIndex;
                    nextIndex++;
                    stackArr[stackTop++] = v;
                    onStack[v] = 1;
                }
                /* find next outgoing edge from v at position >= ei */
                {
                    int found = -1;
                    while (ei < g.edgeCount) {
                        if (g.from[ei] == v) {
                            found = ei;
                            break;
                        }
                        ei++;
                    }
                }
                if (ei < g.edgeCount) {
                    int w = g.to[ei];
                    frameEdge[frameTop - 1] = ei + 1;
                    if (index[w] < 0) {
                        frameNode[frameTop] = w;
                        frameEdge[frameTop] = 0;
                        frameTop++;
                    } else if (onStack[w]) {
                        if (index[w] < low[v]) low[v] = index[w];
                    }
                } else {
                    /* done with v */
                    frameTop--;
                    if (frameTop > 0) {
                        int parent = frameNode[frameTop - 1];
                        if (low[v] < low[parent]) low[parent] = low[v];
                    }
                    if (low[v] == index[v]) {
                        int count = 0;
                        while (stackTop > 0) {
                            int w = stackArr[--stackTop];
                            onStack[w] = 0;
                            comps[compCount][count++] = w;
                            if (w == v) break;
                        }
                        compSize[compCount] = count;
                        compCount++;
                    }
                }
            }
        }
    }
    result = xrtValueArray();
    if (result == NULL) {
        xg_free(&g);
        return xg_null();
    }
    {
        int c;
        for (c = 0; c < compCount; c++) {
            xvalue *group;
            memcpy(order, comps[c], sizeof(int) * (size_t)compSize[c]);
            group = xg_node_list(&g, order, compSize[c]);
            if (group == NULL || !xrtValueArrayAppendNew(result, group)) {
                xg_free(&g);
                return xg_null();
            }
        }
    }
    xg_free(&g);
    return result;
}

xvalue *xg_nodes(xvalue *edges) {
    XGraph g;
    int order[XG_MAX_NODES];
    int i;
    xvalue *result;
    if (!xg_load(edges, &g)) return xg_null();
    for (i = 0; i < g.nodeCount; i++) order[i] = i;
    for (i = 1; i < g.nodeCount; i++) {
        int key = order[i];
        int j = i - 1;
        while (j >= 0 && strcmp(g.nodes[order[j]], g.nodes[key]) > 0) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = key;
    }
    result = xg_node_list(&g, order, g.nodeCount);
    xg_free(&g);
    return result;
}

xvalue *xg_neighbors(xvalue *edges, const char *node) {
    XGraph g;
    int order[XG_MAX_NODES];
    int count = 0;
    int u;
    int i;
    xvalue *result;
    if (!xg_load(edges, &g)) return xg_null();
    u = xg_find_node(&g, node);
    if (u < 0) {
        xg_free(&g);
        xg_error("node is not in the graph");
        return xg_null();
    }
    for (i = 0; i < g.edgeCount && count < XG_MAX_NODES; i++) {
        if (g.from[i] == u) {
            int dup = 0;
            int k;
            for (k = 0; k < count; k++) {
                if (order[k] == g.to[i]) dup = 1;
            }
            if (!dup) order[count++] = g.to[i];
        }
    }
    result = xg_node_list(&g, order, count);
    xg_free(&g);
    return result;
}
