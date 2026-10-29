#ifndef XLANG_EXTLIB_GRAPH_H
#define XLANG_EXTLIB_GRAPH_H
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#ifndef XRT_MODULE_ARRAY
#define XRT_MODULE_ARRAY
#endif
#ifndef XRT_MODULE_MAP
#define XRT_MODULE_MAP
#endif
#include "xrt.h"

/* 保留当前资源上限；按实际输入分配，不预留上限大小的栈或全局数组。 */
#define XG_MAX_NODES 4096
#define XG_MAX_EDGES 32768
XRT_EXTERN_C_BEGIN

/* 输入借用且不修改：[name] 声明孤立点，[from,to] 或 [from,to,weight] 声明有向边。
 * 节点按完整字符串字节辨认，支持空串、UTF-8 和 NUL；查询名称显式携带字节长度。
 * 权重为有限 int/float；Dijkstra 另要求非负。
 * 返回的 xvalue 由调用者 xrtValueRelease；失败返回 NULL 并设置 XRT error。
 * is_dag 的 false 也可能是正常有环结果，C 调用者用 xrtGetError 区分失败。
 * 每次调用清除旧错误，无共享可变状态；调用期间输入不得被其他线程修改。
 */
bool xg_is_dag(xvalue* edges);
xvalue* xg_topo_sort(xvalue* edges);
xvalue* xg_bfs(xvalue* edges, const char* start, size_t startSize);
xvalue* xg_shortest_path(xvalue* edges, const char* start, size_t startSize,
    const char* target, size_t targetSize);
xvalue* xg_dijkstra(xvalue* edges, const char* start, size_t startSize);
xvalue* xg_components(xvalue* edges);
xvalue* xg_scc(xvalue* edges);
xvalue* xg_nodes(xvalue* edges);
xvalue* xg_neighbors(xvalue* edges, const char* node, size_t nodeSize);
XRT_EXTERN_C_END
#endif
