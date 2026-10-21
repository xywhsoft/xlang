#include "graph_internal.h"

void xgError(xerrkind kind, const char* message)
{
    xrtSetErrorInfo(kind, "xlang.extlib.graph", 0, message);
}

void xgUnit(xgmodel* g)
{
    xrtArrayUnit(&g->Edges);
    xrtArrayUnit(&g->Nodes);
    xrtMapUnit(&g->Names);
}

/* XRT 字节键映射统一节点名，不用 strlen 截断输入字符串。 */
static int intern(xgmodel* g, xvalue* value)
{
    xstrview name;
    xbytesview key, stored;
    int* id;
    xgnode node;
    if (xrtValueType(value) != XVALUE_STRING) {
        xgError(XERR_VALUE, "graph node must be a string");
        return -1;
    }
    if (!xrtValueGetString(value, &name)) return -1;
    key.Data = (const unsigned char*)name.Data;
    key.Size = name.Size;
    id = xrtMapGet(&g->Names, key);
    if (id) return *id;
    if (g->Nodes.Count == XG_MAX_NODES) {
        xgError(XERR_RANGE, "graph node limit exceeded");
        return -1;
    }
    id = xrtMapGetOrAdd(&g->Names, key, NULL);
    if (!id) return -1;
    *id = (int)g->Nodes.Count;
    if (!xrtMapStoredKey(&g->Names, key, &stored)) return -1;
    node.Name.Data = (const char*)stored.Data;
    node.Name.Size = stored.Size;
    node.FirstOut = node.LastOut = node.FirstIn = node.LastIn = -1;
    return xrtArrayPush(&g->Nodes, &node) ? *id : -1;
}

bool xgLoad(xgmodel* g, xvalue* input)
{
    xrtClearError();
    memset(g, 0, sizeof(*g));
    xrtMapInit(&g->Names, sizeof(int));
    xrtArrayInit(&g->Nodes, sizeof(xgnode));
    xrtArrayInit(&g->Edges, sizeof(xgedge));
    if (xrtValueType(input) != XVALUE_ARRAY) {
        xgError(XERR_VALUE, "graph edges must be an array");
        goto fail;
    }
    for (size_t i = 0, n = xrtValueCount(input); i < n; ++i) {
        xvalue* row = xrtValueArrayAt(input, i);
        size_t count;
        xgedge edge = {0, 0, -1, -1, 1.0};
        int index;
        xgnode *from, *to;
        if (xrtValueType(row) != XVALUE_ARRAY ||
            (count = xrtValueCount(row)) < 1 || count > 3) {
            xgError(XERR_VALUE, "graph edge must be [node], [from,to] or [from,to,weight]");
            goto fail;
        }
        edge.From = intern(g, xrtValueArrayAt(row, 0));
        if (edge.From < 0) goto fail;
        if (count == 1) continue;
        if (g->Edges.Count == XG_MAX_EDGES) {
            xgError(XERR_RANGE, "graph edge limit exceeded");
            goto fail;
        }
        edge.To = intern(g, xrtValueArrayAt(row, 1));
        if (edge.To < 0) goto fail;
        if (count == 3) {
            xvalue* weight = xrtValueArrayAt(row, 2);
            if (xrtValueType(weight) == XVALUE_INT) {
                int64 value;
                if (!xrtValueGetInt(weight, &value)) goto fail;
                edge.Weight = (double)value;
            } else if (xrtValueType(weight) == XVALUE_FLOAT) {
                if (!xrtValueGetFloat(weight, &edge.Weight)) goto fail;
            } else {
                xgError(XERR_VALUE, "graph weight must be an int or float");
                goto fail;
            }
            if (!isfinite(edge.Weight)) {
                xgError(XERR_VALUE, "graph weight must be finite");
                goto fail;
            }
        }
        index = (int)g->Edges.Count;
        if (!xrtArrayPush(&g->Edges, &edge)) goto fail;
        from = &xgNodes(g)[edge.From];
        to = &xgNodes(g)[edge.To];
        if (from->LastOut < 0) from->FirstOut = index;
        else xgEdges(g)[from->LastOut].NextOut = index;
        if (to->LastIn < 0) to->FirstIn = index;
        else xgEdges(g)[to->LastIn].NextIn = index;
        from->LastOut = to->LastIn = index;
    }
    return true;
fail:
    xgUnit(g);
    return false;
}

int xgFind(xgmodel* g, const char* name, size_t size)
{
    xbytesview key;
    const int* id;
    if (!name && size) {
        xgError(XERR_ARGUMENT, "graph node name must not be NULL");
        return -1;
    }
    key.Data = (const unsigned char*)name;
    key.Size = size;
    id = xrtMapConstGet(&g->Names, key);
    if (id) return *id;
    xgError(XERR_VALUE, "graph node was not declared");
    return -1;
}

static bool appendName(xvalue* result, xstrview name)
{
    xvalue* item = xrtValueString(name);
    return item && xrtValueArrayAppendNew(result, item);
}

xvalue* xgNodeList(xgmodel* g, const int* ids, int count)
{
    xvalue* result = xrtValueArray();
    if (!result) return NULL;
    for (int i = 0; i < count; ++i) {
        if (!appendName(result, xgNodes(g)[ids[i]].Name)) {
            xrtValueRelease(result);
            return NULL;
        }
    }
    return result;
}

/* 分量及其内部节点都按输入首次出现顺序输出，不依赖算法发现顺序。 */
xvalue* xgGroups(xgmodel* g, const int* component)
{
    xvalue* result = xrtValueArray();
    xvalue** groups = NULL;
    if (!result) return NULL;
    if (g->Nodes.Count) {
        groups = xrtCalloc(g->Nodes.Count, sizeof(*groups));
        if (!groups) goto fail;
    }
    for (size_t i = 0; i < g->Nodes.Count; ++i) {
        int id = component[i];
        if (!groups[id]) {
            xvalue* group = xrtValueArray();
            if (!group || !xrtValueArrayAppendNew(result, group)) goto fail;
            groups[id] = group;
        }
        if (!appendName(groups[id], xgNodes(g)[i].Name)) goto fail;
    }
    xrtFree(groups);
    return result;
fail:
    xrtFree(groups);
    xrtValueRelease(result);
    return NULL;
}

static int compareName(const void* left, const void* right)
{
    const xstrview *a = left, *b = right;
    size_t size = a->Size < b->Size ? a->Size : b->Size;
    int order = size ? memcmp(a->Data, b->Data, size) : 0;
    return order ? order : (a->Size > b->Size) - (a->Size < b->Size);
}

xvalue* xg_nodes(xvalue* edges)
{
    xgmodel g;
    xarray names;
    xvalue* result = NULL;
    if (!xgLoad(&g, edges)) return NULL;
    xrtArrayInit(&names, sizeof(xstrview));
    for (size_t i = 0; i < g.Nodes.Count; ++i)
        if (!xrtArrayPush(&names, &xgNodes(&g)[i].Name)) goto done;
    if (!xrtArraySort(&names, compareName)) goto done;
    result = xrtValueArray();
    if (!result) goto done;
    for (size_t i = 0; i < names.Count; ++i) {
        if (!appendName(result, ((xstrview*)names.Data)[i])) {
            xrtValueRelease(result);
            result = NULL;
            break;
        }
    }
done:
    xrtArrayUnit(&names);
    xgUnit(&g);
    return result;
}

xvalue* xg_neighbors(xvalue* edges, const char* node, size_t nodeSize)
{
    xgmodel g;
    bool* seen = NULL;
    xvalue* result = NULL;
    int id;
    if (!xgLoad(&g, edges)) return NULL;
    if ((id = xgFind(&g, node, nodeSize)) < 0) goto done;
    seen = xrtCalloc(g.Nodes.Count, sizeof(*seen));
    if (!seen || !(result = xrtValueArray())) goto done;
    for (int i = xgNodes(&g)[id].FirstOut; i >= 0; i = xgEdges(&g)[i].NextOut) {
        int to = xgEdges(&g)[i].To;
        if (seen[to]) continue;
        seen[to] = true;
        if (!appendName(result, xgNodes(&g)[to].Name)) {
            xrtValueRelease(result);
            result = NULL;
            break;
        }
    }
done:
    xrtFree(seen);
    xgUnit(&g);
    return result;
}
