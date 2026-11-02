/* 完整保留结果，释放输入后再输出；独立 Python 枚举验证顺序，不借用本包算法。 */
#include "graph_test.h"

typedef struct { const char* data; size_t size; } node_name;
typedef struct { unsigned kind; int start, target; bool dag; xvalue* value; } query;
static const char* kinds[] = {"dag", "topo", "components", "scc", "nodes", "neighbors", "bfs", "distance", "path"};
static const node_name names[4][4] = {
    {{"z",1},{"a",1},{"m",1},{"c",1}},
    {{"",0},{"a\0b",3},{"a",1},{"b",1}},
    {{"\xe4\xb8\xad",3},{"a",1},{"\xe6\x96\x87",3},{"\xf0\x9f\x98\x80",4}},
    {{"A",1},{"a",1},{"A\0",2},{"A\0B",3}}
};

static void add(xvalue* graph, const node_name* name, int from, int to)
{
    xvalue* row = xrtValueArray();
    assert(row && xrtValueArrayAppendNew(row, text(name[from].data, name[from].size)));
    if (to >= 0) {
        assert(xrtValueArrayAppendNew(row, text(name[to].data, name[to].size)));
        assert(xrtValueArrayAppendNew(row, xrtValueInt((from * 3 + to) % 3)));
    }
    assert(xrtValueArrayAppendNew(graph, row));
}

static xvalue* make_graph(unsigned sample, unsigned reverse, const node_name* name, unsigned count)
{
    unsigned from[9], to[9], edges = 0, bit = 0;
    unsigned mask = sample < 512 ? sample : sample - 512;
    xvalue* graph = xrtValueArray();
    assert(graph);
    for (unsigned i = 0; i < count; ++i) add(graph, name, (int)(reverse ? count - 1 - i : i), -1);
    for (unsigned i = 0; i < count; ++i) for (unsigned j = 0; j < count; ++j) {
        if (sample >= 512 && i >= j) continue;
        if (mask & (1u << bit)) { from[edges] = i; to[edges++] = j; }
        ++bit;
    }
    for (unsigned i = 0; i < edges; ++i) {
        unsigned edge = reverse ? edges - 1 - i : i;
        add(graph, name, (int)from[edge], (int)to[edge]);
        if (reverse && i % 2 == 0) add(graph, name, (int)from[edge], (int)to[edge]);
    }
    return graph;
}

static query collect(xvalue* graph, unsigned kind, int start, int target, const node_name* name)
{
    query q = {kind, start, target, false, NULL};
    switch (kind) {
    case 0: q.dag = xg_is_dag(graph); break;
    case 1: q.value = xg_topo_sort(graph); break;
    case 2: q.value = xg_components(graph); break;
    case 3: q.value = xg_scc(graph); break;
    case 4: q.value = xg_nodes(graph); break;
    case 5: q.value = xg_neighbors(graph, name[start].data, name[start].size); break;
    case 6: q.value = xg_bfs(graph, name[start].data, name[start].size); break;
    case 7: q.value = xg_dijkstra(graph, name[start].data, name[start].size); break;
    case 8: q.value = xg_shortest_path(graph, name[start].data, name[start].size,
        name[target].data, name[target].size); break;
    default: assert(0);
    }
    if (kind == 0 || q.value) assert(!xrtGetError());
    else assert((kind == 1 || kind == 8) && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    return q;
}

static int identity(xvalue* value, const node_name* name, unsigned count)
{
    xstrview got;
    assert(xrtValueGetString(value, &got));
    for (unsigned i = 0; i < count; ++i)
        if (got.Size == name[i].size && (!got.Size || !memcmp(got.Data, name[i].data, got.Size))) return (int)i;
    assert(0 && "truncated, stale or unexpected node name");
    return -1;
}

static void node_list(xvalue* value, const node_name* name, unsigned count, bool output)
{
    assert(xrtValueType(value) == XVALUE_ARRAY);
    if (output) putchar('[');
    for (size_t i = 0; i < xrtValueCount(value); ++i) {
        int id = identity(xrtValueArrayAt(value, i), name, count);
        if (output) printf("%s%d", i ? "," : "", id);
    }
    if (output) putchar(']');
}

static void emit(query q, unsigned sample, unsigned reverse, const node_name* name, unsigned count, bool output)
{
    if (output) printf("[%u,%u,\"%s\",%d,%d,", sample, reverse, kinds[q.kind], q.start, q.target);
    if (q.kind == 0) { if (output) fputs(q.dag ? "true" : "false", stdout); }
    else if (!q.value) { if (output) fputs("\"value-error\"", stdout); }
    else if (q.kind == 2 || q.kind == 3) {
        assert(xrtValueType(q.value) == XVALUE_ARRAY);
        if (output) putchar('[');
        for (size_t i = 0; i < xrtValueCount(q.value); ++i) {
            if (output && i) putchar(',');
            node_list(xrtValueArrayAt(q.value, i), name, count, output);
        }
        if (output) putchar(']');
    } else if (q.kind == 7) {
        unsigned found = 0;
        assert(xrtValueType(q.value) == XVALUE_OBJECT);
        if (output) putchar('[');
        for (unsigned i = 0; i < count; ++i) {
            xvalue* value = xrtValueObjectGet(q.value, (xstrview){name[i].data, name[i].size});
            if (output && i) putchar(',');
            if (value) {
                double distance;
                assert(xrtValueGetFloat(value, &distance) && isfinite(distance) && distance >= 0 && distance <= 6);
                assert(distance == (int)distance);
                if (output) printf("%d", (int)distance);
                ++found;
            } else if (output) fputs("null", stdout);
        }
        assert(xrtValueCount(q.value) == found);
        if (output) putchar(']');
    } else node_list(q.value, name, count, output);
    if (output) puts("]");
}

int main(int argc, char** argv)
{
    bool output = argc == 2 && !strcmp(argv[1], "--records");
    unsigned graphs = 0, records = 0;
    if (argc != 1 && !output) return 2;
    assert(xrtMemDebugEnabled());
    xrtSetErrorInfo(XERR_IO, "test.graph", 0, "warm diagnostic slot"); xrtClearError();
    for (unsigned sample = 0; sample < 576; ++sample) for (unsigned reverse = 0; reverse < 2; ++reverse) {
        xmemdebugsnapshot before, after;
        const node_name* name = names[sample % 4];
        unsigned count = sample < 512 ? 3 : 4, used = 0;
        query results[33];
        xrtMemDebugSnapshot(&before);
        xvalue* graph = make_graph(sample, reverse, name, count);
        for (unsigned kind = 0; kind < 5; ++kind) results[used++] = collect(graph, kind, -1, -1, name);
        for (unsigned start = 0; start < count; ++start) {
            for (unsigned kind = 5; kind < 8; ++kind) results[used++] = collect(graph, kind, (int)start, -1, name);
            for (unsigned target = 0; target < count; ++target)
                results[used++] = collect(graph, 8, (int)start, (int)target, name);
        }
        assert(used <= sizeof(results) / sizeof(results[0]));
        /* 后面的查询及输入销毁不能改变前面结果的节点名字、顺序和距离。 */
        xrtValueRelease(graph);
        for (unsigned i = 0; i < used; ++i) {
            emit(results[i], sample, reverse, name, count, output);
            xrtValueRelease(results[i].value);
            ++records;
        }
        xrtClearError();
        xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
        assert(before.OverflowCount == after.OverflowCount && before.UnderflowCount == after.UnderflowCount);
        assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
        ++graphs;
    }
    assert(graphs == 1152 && records == 27776);
    if (!output) printf("graph ordered records: %u graphs, %u records, PASS\n", graphs, records);
    return ferror(stdout) ? 2 : 0;
}

#include "../graph_native.c"
#include "../graph_traversal.c"
#include "../graph_components.c"
#include "../graph_distance.c"
