#ifndef XLANG_EXTLIB_GRAPH_INTERNAL_H
#define XLANG_EXTLIB_GRAPH_INTERNAL_H
#include "include/graph.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    xstrview Name;
    int FirstOut, LastOut, FirstIn, LastIn;
} xgnode;
typedef struct {
    int From, To, NextOut, NextIn;
    double Weight;
} xgedge;
/* map 持有节点名字，稳定视图贯穿本次调用；邻接边保持输入顺序。 */
typedef struct {
    xmap Names;
    xarray Nodes, Edges;
} xgmodel;
static inline xgnode* xgNodes(xgmodel* g) { return (xgnode*)g->Nodes.Data; }
static inline xgedge* xgEdges(xgmodel* g) { return (xgedge*)g->Edges.Data; }
void xgError(xerrkind kind, const char* message);
bool xgLoad(xgmodel* g, xvalue* input);
void xgUnit(xgmodel* g);
int xgFind(xgmodel* g, const char* name, size_t size);
xvalue* xgNodeList(xgmodel* g, const int* ids, int count);
xvalue* xgGroups(xgmodel* g, const int* component);
#endif
