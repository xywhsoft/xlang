#include "../include/graph.h"
#include <stdio.h>
#include <string.h>

static bool add(xvalue* graph, const char* from, const char* to, double weight)
{
    xvalue* row = xrtValueArray();
    if (row == NULL) return false;
    if (!xrtValueArrayAppendNew(row, xrtValueString((xstrview){from, strlen(from)}))) goto fail;
    if (to) {
        if (!xrtValueArrayAppendNew(row, xrtValueString((xstrview){to, strlen(to)})) ||
            !xrtValueArrayAppendNew(row, xrtValueFloat(weight))) goto fail;
    }
    /* AppendNew 消费 row，包括插入失败的情况。 */
    return xrtValueArrayAppendNew(graph, row);
fail:
    xrtValueRelease(row);
    return false;
}

static bool addUnsigned(xvalue* graph, const char* from, const char* to, uint64 weight)
{
    xvalue* row = xrtValueArray();
    if (row == NULL) return false;
    if (!xrtValueArrayAppendNew(row, xrtValueString((xstrview){from, strlen(from)})) ||
        !xrtValueArrayAppendNew(row, xrtValueString((xstrview){to, strlen(to)})) ||
        !xrtValueArrayAppendNew(row, xrtValueUInt(weight))) {
        xrtValueRelease(row);
        return false;
    }
    return xrtValueArrayAppendNew(graph, row);
}

static bool names(xvalue* values, const char* separator)
{
    if (values == NULL) return false;
    for (size_t i = 0; i < xrtValueCount(values); ++i) {
        xstrview value;
        if (!xrtValueGetString(xrtValueArrayAt(values, i), &value) ||
            (i && fputs(separator, stdout) == EOF) ||
            fwrite(value.Data, 1, value.Size, stdout) != value.Size) return false;
    }
    return true;
}

/* 每个算法结果独立拥有；打印后及时释放，不借用输入内部字符串。 */
static bool show(const char* label, xvalue* values, const char* separator)
{
    bool ok = values != NULL && fputs(label, stdout) != EOF &&
        names(values, separator) && putchar('\n') != EOF;
    xrtValueRelease(values);
    return ok;
}

int main(void)
{
    xvalue *flow = xrtValueArray(), *calls = xrtValueArray();
    xvalue *costs = NULL, *weak = NULL, *strong = NULL, *unexpected = NULL;
    double cost;
    int status = 1;
    if (flow == NULL || calls == NULL ||
        !addUnsigned(flow, "parse", "check", 2) ||
        !add(flow, "check", "emit", 3) ||
        !add(flow, "parse", "emit", 9) ||
        !add(flow, "docs", NULL, 0)) goto done;
    bool dag = xg_is_dag(flow);
    if (xrtGetError() != NULL) goto done;
    printf("dag=%s\n", dag ? "true" : "false");
    if (!show("topo=", xg_topo_sort(flow), ">") ||
        !show("bfs=", xg_bfs(flow, "parse", 5), ">") ||
        !show("nodes=", xg_nodes(flow), ",") ||
        !show("neighbors=", xg_neighbors(flow, "parse", 5), ",") ||
        !show("fewest-edges=", xg_shortest_path(flow, "parse", 5, "emit", 4), ">")) goto done;
    costs = xg_dijkstra(flow, "parse", 5);
    if (costs == NULL ||
        !xrtValueGetFloat(xrtValueObjectGet(costs, (xstrview){"emit", 4}), &cost)) goto done;
    printf("cost=%.0f\n", cost);
    xrtValueRelease(costs);
    costs = NULL;
    weak = xg_components(flow);
    if (weak == NULL || xrtValueCount(weak) == 0) goto done;
    printf("weak=%zu; first-size=%zu\n", xrtValueCount(weak), xrtValueCount(xrtValueArrayAt(weak, 0)));
    xrtValueRelease(weak);
    weak = NULL;

    if (!add(calls, "a", "b", 1) || !add(calls, "b", "a", 1) ||
        !add(calls, "b", "c", 1)) goto done;
    strong = xg_scc(calls);
    if (strong == NULL || xrtValueCount(strong) != 2) goto done;
    printf("scc=%zu; first-size=%zu\n", xrtValueCount(strong), xrtValueCount(xrtValueArrayAt(strong, 0)));
    xrtValueRelease(strong);
    strong = NULL;
    unexpected = xg_topo_sort(calls);
    if (unexpected != NULL || xrtGetError() == NULL ||
        xrtErrorKind(xrtGetError()) != XERR_VALUE) goto done;
    puts("cycle=rejected");
    xrtClearError();
    status = 0;
done:
    xrtValueRelease(unexpected);
    xrtValueRelease(strong);
    xrtValueRelease(weak);
    xrtValueRelease(costs);
    xrtValueRelease(flow);
    xrtValueRelease(calls);
    return status;
}

/* 单文件范例纳入四个算法单元；正式扩展包仍按清单分别编译。 */
#include "../graph_native.c"
#include "../graph_traversal.c"
#include "../graph_components.c"
#include "../graph_distance.c"
