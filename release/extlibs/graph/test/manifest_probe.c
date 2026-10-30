/* 先按命令行清单生成 XRT，避免后包含的公开头掩盖缺失依赖。 */
#include "xrt.h"
#include "../include/graph.h"
#include <stdio.h>

int main(void)
{
    xvalue *graph=xrtValueArray(), *row=xrtValueArray();
    bool ok=graph && row &&
        xrtValueArrayAppendNew(row,xrtValueString(XRT_STR_LITERAL("a"))) &&
        xrtValueArrayAppendNew(row,xrtValueString(XRT_STR_LITERAL("b"))) &&
        xrtValueArrayAppendTake(graph,&row);
    xvalue* nodes=ok ? xg_nodes(graph) : NULL;
    ok=ok && xg_is_dag(graph) && nodes && xrtValueCount(nodes)==2 && !xrtGetError();
    xrtValueRelease(nodes); xrtValueRelease(row); xrtValueRelease(graph);
    if (ok) puts("graph manifest-only: PASS");
    return ok ? 0 : 1;
}

#include "../graph_native.c"
#include "../graph_traversal.c"
#include "../graph_components.c"
#include "../graph_distance.c"
