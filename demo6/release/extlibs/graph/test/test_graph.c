#include "graph_test.h"

static void basic(void)
{
    xvalue *g = xrtValueArray(), *r, *row;
    assert(g && xg_is_dag(g) && !xrtGetError());
    r = xg_scc(g); assert(r && !xrtValueCount(r)); xrtValueRelease(r);
    r = xg_components(g); assert(r && !xrtValueCount(r)); xrtValueRelease(r);
    r = xg_topo_sort(g); assert(r && !xrtValueCount(r)); xrtValueRelease(r);
    r = xg_nodes(g); assert(r && !xrtValueCount(r)); xrtValueRelease(r);
    assert(!xg_bfs(g, "missing", 7) && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    edge(g, "a", "b", 2);
    edge(g, "a", "b", 1);
    edge(g, "b", "c", 0);
    edge(g, "z", NULL, 0);
    assert(xg_is_dag(g) && !xrtGetError());
    r = xg_neighbors(g, "a", 1);
    assert(r && xrtValueCount(r) == 1 && named(xrtValueArrayAt(r, 0), "b", 1));
    xrtValueRelease(r);
    r = xg_dijkstra(g, "a", 1);
    assert(r && !xrtGetError());
    {
        double distance;
        xvalue* value = xrtValueObjectGet(r, XRT_STR_LITERAL("c"));
        assert(value && xrtValueGetFloat(value, &distance) && distance == 1);
    }
    xrtValueRelease(r);
    r = xg_shortest_path(g, "a", 1, "a", 1);
    assert(r && xrtValueCount(r) == 1); xrtValueRelease(r);
    assert(!xg_shortest_path(g, "z", 1, "a", 1));
    assert(xrtErrorKind(xrtGetError()) == XERR_VALUE);
    assert(!xg_neighbors(g, NULL, 1) && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    /* 返回值拥有名字副本；输入释放后结果仍可使用。 */
    r = xg_bfs(g, "a", 1);
    xrtValueRelease(g);
    assert(r && xrtValueCount(r) == 3 && named(xrtValueArrayAt(r, 2), "c", 1));
    xrtValueRelease(r);

    g = xrtValueArray();
    row = xrtValueArray();
    assert(xrtValueArrayAppendNew(row, text("a\0b", 3)));
    assert(xrtValueArrayAppendNew(row, text("", 0)));
    assert(xrtValueArrayAppendNew(g, row));
    edge(g, "", "中文", 1);
    r = xg_bfs(g, "a\0b", 3);
    assert(r && xrtValueCount(r) == 3 && named(xrtValueArrayAt(r, 0), "a\0b", 3));
    assert(named(xrtValueArrayAt(r, 2), "中文", strlen("中文")));
    xrtValueRelease(r);
    assert(!xg_bfs(g, "a", 1));
    r = xg_bfs(g, NULL, 0); assert(r && xrtValueCount(r) == 2); xrtValueRelease(r);
    r = xg_nodes(g); assert(r && named(xrtValueArrayAt(r, 0), "", 0)); xrtValueRelease(r);
    xrtValueRelease(g);
}

static void invalid(void)
{
    xvalue *g, *row, *r;
    assert(!xg_nodes(NULL) && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    for (int mode = 0; mode < 8; ++mode) {
        g = xrtValueArray();
        edge(g, "a", "b", 1);
        row = xrtValueArray();
        if (mode != 0) assert(xrtValueArrayAppendNew(row, text("c", 1)));
        if (mode >= 2) assert(xrtValueArrayAppendNew(row, text("d", 1)));
        if (mode >= 3) {
            xvalue* weight = mode == 3 ? xrtValueBool(true) : mode == 4 ? text("3", 1) :
                xrtValueFloat(mode == 5 ? NAN : mode == 6 ? INFINITY : -INFINITY);
            assert(weight && xrtValueArrayAppendNew(row, weight));
        }
        if (mode == 1) assert(xrtValueArrayAppendNew(row, xrtValueInt(2)));
        if (mode == 2) {
            assert(xrtValueArrayAppendNew(row, xrtValueInt(1)));
            assert(xrtValueArrayAppendNew(row, xrtValueInt(2)));
        }
        assert(xrtValueArrayAppendNew(g, row));
        r = xg_nodes(g); assert(!r && xrtErrorKind(xrtGetError()) == XERR_VALUE);
        xrtValueRelease(g);
    }
    g = xrtValueArray();
    edge(g, "a", "b", -1);
    assert(xg_is_dag(g) && !xrtGetError());
    assert(!xg_dijkstra(g, "a", 1) && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtValueRelease(g);
    /* 负权在不可达分量也无效；不可达分量的加法则不参与松弛。 */
    g = xrtValueArray();
    edge(g, "a", "b", 1);
    edge(g, "x", "y", -1);
    assert(!xg_dijkstra(g, "a", 1) && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtValueRelease(g);
    g = xrtValueArray();
    edge(g, "a", "b", 1);
    edge(g, "x", "y", DBL_MAX);
    edge(g, "y", "z", DBL_MAX);
    r = xg_dijkstra(g, "a", 1);
    assert(r && xrtValueCount(r) == 2u && !xrtGetError());
    xrtValueRelease(r);
    xrtValueRelease(g);
    g = xrtValueArray();
    edge(g, "a", "b", 1e305);
    r = xg_dijkstra(g, "a", 1);
    assert(r && xrtValueCount(r) == 2); xrtValueRelease(r);
    edge(g, "b", "c", DBL_MAX);
    assert(!xg_dijkstra(g, "a", 1) && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtValueRelease(g);
    /* 先遇到的溢出路径不能遮蔽稍后发现的有限最短路。 */
    g = xrtValueArray();
    edge(g, "a", "b", 1e305);
    edge(g, "b", "c", DBL_MAX);
    edge(g, "a", "d", 2e305);
    edge(g, "d", "c", 1e305);
    r = xg_dijkstra(g, "a", 1);
    assert(r && !xrtGetError());
    {
        double distance;
        xvalue* value = xrtValueObjectGet(r, (xstrview){"c", 1});
        assert(value && xrtValueGetFloat(value, &distance));
        assert(distance > 2e305 && distance < 4e305);
    }
    xrtValueRelease(r);
    xrtValueRelease(g);
}

static void unsigned_weight(void)
{
    xvalue *g = xrtValueArray(), *row = xrtValueArray(), *r;
    double distance;
    assert(g && row);
    assert(xrtValueArrayAppendNew(row, text("a", 1)));
    assert(xrtValueArrayAppendNew(row, text("b", 1)));
    assert(xrtValueArrayAppendNew(row, xrtValueUInt(7)));
    assert(xrtValueArrayAppendNew(g, row));
    r = xg_dijkstra(g, "a", 1);
    assert(r && !xrtGetError());
    assert(xrtValueGetFloat(xrtValueObjectGet(r, XRT_STR_LITERAL("b")), &distance));
    assert(distance == 7.0);
    xrtValueRelease(r);
    xrtValueRelease(g);
}

static void limits(void)
{
    const int sizes[] = {520, 521, XG_MAX_NODES};
    for (size_t k = 0; k < sizeof(sizes) / sizeof(sizes[0]); ++k) {
        int n = sizes[k];
        xvalue *g = xrtValueArray(), *r;
        for (int i = 0; i < n; ++i) {
            char a[24], b[24];
            snprintf(a, sizeof(a), "%d", i);
            snprintf(b, sizeof(b), "%d", (i + 1) % n);
            edge(g, a, b, 1);
        }
        assert(!xg_is_dag(g) && !xrtGetError());
        r = xg_scc(g); assert(r && xrtValueCount(r) == 1);
        assert(xrtValueCount(xrtValueArrayAt(r, 0)) == (size_t)n); xrtValueRelease(r);
        r = xg_bfs(g, "0", 1); assert(r && xrtValueCount(r) == (size_t)n);
        /* map 多次扩容后，结果中借用的节点名仍须保持完整。 */
        for (int probe = 0; probe < 3; ++probe) {
            int index = probe == 0 ? 0 : (probe == 1 ? n / 2 : n - 1);
            char name[24];
            snprintf(name, sizeof(name), "%d", index);
            assert(named(xrtValueArrayAt(r, (size_t)index), name, strlen(name)));
        }
        xrtValueRelease(r);
        if (n == XG_MAX_NODES) {
            edge(g, "extra", NULL, 0);
            assert(!xg_nodes(g) && xrtErrorKind(xrtGetError()) == XERR_RANGE);
        }
        xrtValueRelease(g);
    }
    {
        xvalue *g = xrtValueArray(), *r;
        for (int i = 0; i < XG_MAX_EDGES; ++i) edge(g, "a", "b", 1);
        assert(xg_is_dag(g));
        r = xg_neighbors(g, "a", 1); assert(r && xrtValueCount(r) == 1); xrtValueRelease(r);
        edge(g, "a", "b", 1);
        assert(!xg_is_dag(g) && xrtErrorKind(xrtGetError()) == XERR_RANGE);
        xrtValueRelease(g);
    }
}

/* Floyd 闭包/距离作独立答案，避免用被测 BFS/SCC 相互验证。 */
static void oracle(void)
{
    enum { N = 6, INF = 100000 };
    unsigned random = 9137;
    for (int sample = 0; sample < 128; ++sample) {
        bool adj[N][N] = {{0}}, reach[N][N] = {{0}}, weak[N][N] = {{0}};
        int dist[N][N], hops[N][N], partition[2][N];
        xvalue *g = xrtValueArray(), *r;
        for (int i = 0; i < N; ++i) {
            char name[] = {(char)('a' + i), 0};
            edge(g, name, NULL, 0);
            for (int j = 0; j < N; ++j) dist[i][j] = hops[i][j] = i == j ? 0 : INF;
            reach[i][i] = weak[i][i] = true;
        }
        for (int i = 0; i < N; ++i) for (int j = 0; j < N; ++j) {
            char a[] = {(char)('a' + i), 0}, b[] = {(char)('a' + j), 0};
            random = random * 1664525u + 1013904223u;
            if ((random >> 24) % 4 != 0) continue;
            int weight = (int)((random >> 16) % 9);
            edge(g, a, b, weight);
            adj[i][j] = reach[i][j] = weak[i][j] = weak[j][i] = true;
            if (i != j) { dist[i][j] = weight; hops[i][j] = 1; }
        }
        for (int k = 0; k < N; ++k) for (int i = 0; i < N; ++i) for (int j = 0; j < N; ++j) {
            reach[i][j] |= reach[i][k] && reach[k][j];
            weak[i][j] |= weak[i][k] && weak[k][j];
            if (dist[i][k] + dist[k][j] < dist[i][j]) dist[i][j] = dist[i][k] + dist[k][j];
            if (hops[i][k] + hops[k][j] < hops[i][j]) hops[i][j] = hops[i][k] + hops[k][j];
        }
        for (int kind = 0; kind < 2; ++kind) {
            r = kind ? xg_scc(g) : xg_components(g);
            assert(r);
            for (int i = 0; i < N; ++i) partition[kind][i] = -1;
            for (size_t c = 0; c < xrtValueCount(r); ++c) {
                xvalue* group = xrtValueArrayAt(r, c);
                assert(xrtValueCount(group));
                for (size_t i = 0; i < xrtValueCount(group); ++i) {
                    xstrview name;
                    assert(xrtValueGetString(xrtValueArrayAt(group, i), &name) && name.Size == 1);
                    int id = name.Data[0] - 'a';
                    assert(id >= 0 && id < N && partition[kind][id] == -1);
                    partition[kind][id] = (int)c;
                }
            }
            xrtValueRelease(r);
            for (int i = 0; i < N; ++i) for (int j = 0; j < N; ++j) {
                assert(partition[kind][i] >= 0);
                assert((partition[kind][i] == partition[kind][j]) ==
                    (kind ? (reach[i][j] && reach[j][i]) : weak[i][j]));
            }
        }
        bool dag = true;
        for (int i = 0; i < N; ++i) for (int j = 0; j < N; ++j) if (adj[i][j] && reach[j][i]) dag = false;
        assert(xg_is_dag(g) == dag && !xrtGetError());
        r = xg_topo_sort(g);
        if (dag) {
            int position[N];
            assert(r && xrtValueCount(r) == N);
            for (int i = 0; i < N; ++i) {
                xstrview name;
                assert(xrtValueGetString(xrtValueArrayAt(r, i), &name));
                position[name.Data[0] - 'a'] = i;
            }
            for (int i = 0; i < N; ++i) for (int j = 0; j < N; ++j) if (adj[i][j]) assert(position[i] < position[j]);
            xrtValueRelease(r);
        } else assert(!r && xrtErrorKind(xrtGetError()) == XERR_VALUE);
        for (int i = 0; i < N; ++i) {
            char from[] = {(char)('a' + i), 0};
            r = xg_dijkstra(g, from, 1); assert(r && !xrtGetError());
            for (int j = 0; j < N; ++j) {
                char to[] = {(char)('a' + j), 0};
                xvalue* value = xrtValueObjectGet(r, (xstrview){to, 1});
                double got;
                if (dist[i][j] == INF) assert(!value);
                else assert(value && xrtValueGetFloat(value, &got) && got == dist[i][j]);
                xvalue* path = xg_shortest_path(g, from, 1, to, 1);
                if (hops[i][j] == INF) assert(!path && xrtErrorKind(xrtGetError()) == XERR_VALUE);
                else {
                    assert(path && xrtValueCount(path) == (size_t)hops[i][j] + 1);
                    assert(named(xrtValueArrayAt(path, 0), from, 1));
                    assert(named(xrtValueArrayAt(path, xrtValueCount(path) - 1), to, 1));
                    xrtValueRelease(path);
                }
            }
            xrtValueRelease(r);
        }
        xrtValueRelease(g);
    }
}

int main(void)
{
    xmemdebugsnapshot before;
    assert(xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
    basic(); invalid(); unsigned_weight(); limits(); oracle();
    balanced(&before);
    puts("graph: boundaries, 128 independent oracle graphs, capacity and ownership PASS");
    return 0;
}
