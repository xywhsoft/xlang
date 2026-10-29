#define XRT_MODULE_FUTURE_COMBINE
#define XRT_MODULE_TASK_GROUP
#define XRT_MODULE_MEMORY
#include "src/common/xl6_xrt.h"
#include "xl6_future_wait.h"
#include <string.h>

static bool same(xrtownershipref a, xrtownershipref b)
{ return a.Data == b.Data && a.Ops == b.Ops; }
typedef struct WaitDescription {
    xrtownershipref output;
    xfuture* single;
    xfuture* const* inputs;
    size_t count;
    bool all;
    xrtownershipref group;
} WaitDescription;
static bool describe(xrtownershipref producer, const void* token, WaitDescription* wait)
{
    XL6_FutureWaitV1 single = {0}; xfuturecombinewaitv1 multiple = {0}; xtaskgroupwaitv1 group = {0};
    if (!producer.Data || !producer.Ops || !token || !wait) return false;
    /* Closed providers check identity and stable same-claim state before
     * reading Data. Physical adapter admission remains independent. */
    if ((xl6ContinuationFutureWaitV1(producer, token, &single) ||
         xl6CallbackFutureWaitV1(producer, token, &single)) && single.output && single.source &&
        xrtFutureState(single.source) == XFUTURE_PENDING) {
        *wait = (WaitDescription){.output=xrtPromiseOwnership(single.output), .single=single.source, .count=1, .all=true};
        return true;
    }
    if (xrtFutureCombineWaitV1(producer, token, &multiple) && multiple.Output && multiple.Sources && multiple.Count &&
        (multiple.Rule == XFUTURE_WAIT_ALL_TERMINAL || multiple.Rule == XFUTURE_WAIT_ANY_TERMINAL)) {
        *wait = (WaitDescription){.output=xrtPromiseOwnership(multiple.Output), .inputs=multiple.Sources,
            .count=multiple.Count, .all=multiple.Rule == XFUTURE_WAIT_ALL_TERMINAL};
        return true;
    }
    if (xrtTaskGroupWaitV1(producer,token,&group,NULL,NULL) && group.Output && group.Count) {
        *wait = (WaitDescription){.output=xrtPromiseOwnership(group.Output), .count=group.Count, .all=true, .group=producer};
        return true;
    }
    return false;
}
typedef struct ProducerEdge {
    xrtownershipref input, producer;
    const void* token;
} ProducerEdge;
static bool output_edge(xrtownershipref ref, void* data)
{
    ProducerEdge* edge = data; WaitDescription wait = {0};
    if (describe(ref, edge->token, &wait) && same(wait.output, edge->input)) {
        if (edge->producer.Data && !same(edge->producer, ref)) return false;
        edge->producer = ref;
    }
    return true;
}
static xrtownershipref source_producer(xfuture* source, const void* token)
{
    const xrtownershipref none = {0};
    if (!source || xrtFutureState(source) != XFUTURE_PENDING) return none;
    ProducerEdge edge = {xrtFutureOwnership(source), {0}, token};
    if (!edge.input.Data || !edge.input.Ops || !edge.input.Ops->Trace(edge.input.Data, output_edge, &edge)) return none;
    return edge.producer;
}
#define WAIT_NONE SIZE_MAX
typedef struct WaitNode {
    xrtownershipref producer;
    size_t first, count, reverse, queueNext, remaining;
    bool all, blocked, reachesRoot;
} WaitNode;
typedef struct WaitEdge { size_t source, target, nextReverse; } WaitEdge;
typedef struct WaitGraph {
    WaitNode* nodes; WaitEdge* edges;
    size_t count, nodeCapacity, edgeCount, edgeCapacity;
    WaitNode localNodes[16]; WaitEdge localEdges[32];
} WaitGraph;
static void graph_init(WaitGraph* graph)
{
    *graph = (WaitGraph){0}; graph->nodes = graph->localNodes; graph->edges = graph->localEdges;
    graph->nodeCapacity = sizeof(graph->localNodes) / sizeof(*graph->localNodes);
    graph->edgeCapacity = sizeof(graph->localEdges) / sizeof(*graph->localEdges);
}
static void graph_free(WaitGraph* graph)
{
    if (graph->nodes != graph->localNodes) xrtFree(graph->nodes);
    if (graph->edges != graph->localEdges) xrtFree(graph->edges);
}
static bool add_node(WaitGraph* graph, xrtownershipref producer, size_t* index)
{
    for (size_t i = 0; i < graph->count; ++i) if (same(graph->nodes[i].producer, producer)) { *index = i; return true; }
    if (graph->count == graph->nodeCapacity) {
        if (graph->nodeCapacity > SIZE_MAX / 2 / sizeof(*graph->nodes)) return false;
        size_t capacity = graph->nodeCapacity * 2; WaitNode* nodes = xrtMalloc(capacity * sizeof(*nodes));
        if (!nodes) return false;
        memcpy(nodes, graph->nodes, graph->count * sizeof(*nodes));
        if (graph->nodes != graph->localNodes) xrtFree(graph->nodes);
        graph->nodes = nodes; graph->nodeCapacity = capacity;
    }
    *index = graph->count++; graph->nodes[*index] = (WaitNode){0}; graph->nodes[*index].producer = producer;
    return true;
}
static bool add_edge(WaitGraph* graph, size_t source, size_t target)
{
    if (graph->edgeCount == graph->edgeCapacity) {
        if (graph->edgeCapacity > SIZE_MAX / 2 / sizeof(*graph->edges)) return false;
        size_t capacity = graph->edgeCapacity * 2; WaitEdge* edges = xrtMalloc(capacity * sizeof(*edges));
        if (!edges) return false;
        memcpy(edges, graph->edges, graph->edgeCount * sizeof(*edges));
        if (graph->edges != graph->localEdges) xrtFree(graph->edges);
        graph->edges = edges; graph->edgeCapacity = capacity;
    }
    graph->edges[graph->edgeCount++] = (WaitEdge){source, target, WAIT_NONE}; return true;
}
typedef enum WaitBuild { WAIT_BUILD_RESOURCE = -1, WAIT_BUILD_UNAVAILABLE = 0, WAIT_BUILD_READY = 1 } WaitBuild;
typedef struct WaitInputs {
    WaitGraph* graph;
    const void* token;
    size_t source, count;
    bool resource;
} WaitInputs;
static bool append_input(xfuture* source, void* data)
{
    WaitInputs* inputs=data; xrtownershipref next=source_producer(source,inputs->token); size_t target=WAIT_NONE;
    if ((next.Data && !add_node(inputs->graph,next,&target)) || !add_edge(inputs->graph,inputs->source,target)) {
        inputs->resource=true; return false;
    }
    ++inputs->count; return true;
}
static WaitBuild build_graph(WaitGraph* graph, xrtownershipref producer, const void* token)
{
    WaitDescription initial = {0}; size_t root;
    if (!describe(producer, token, &initial)) return WAIT_BUILD_UNAVAILABLE;
    if (!add_node(graph, producer, &root)) return WAIT_BUILD_RESOURCE;
    for (size_t i = 0; i < graph->count; ++i) {
        WaitDescription wait = {0};
        if (!describe(graph->nodes[i].producer, token, &wait)) return WAIT_BUILD_UNAVAILABLE;
        graph->nodes[i].first = graph->edgeCount; graph->nodes[i].count = wait.count; graph->nodes[i].all = wait.all;
        WaitInputs inputs={.graph=graph,.token=token,.source=i};
        if (wait.group.Data) {
            xtaskgroupwaitv1 observed={0};
            if (!xrtTaskGroupWaitV1(wait.group,token,&observed,append_input,&inputs))
                return inputs.resource ? WAIT_BUILD_RESOURCE : WAIT_BUILD_UNAVAILABLE;
            if (observed.Count!=wait.count || !same(xrtPromiseOwnership(observed.Output),wait.output)) return WAIT_BUILD_UNAVAILABLE;
        } else for (size_t j = 0; j < wait.count; ++j) {
            if (!append_input(wait.inputs ? wait.inputs[j] : wait.single,&inputs)) return WAIT_BUILD_RESOURCE;
        }
        if (inputs.count!=wait.count) return WAIT_BUILD_UNAVAILABLE;
    }
    return WAIT_BUILD_READY;
}
static void enqueue(WaitGraph* graph, size_t node, size_t* head, size_t* tail)
{
    graph->nodes[node].queueNext = WAIT_NONE;
    if (*tail == WAIT_NONE) *head = node; else graph->nodes[*tail].queueNext = node;
    *tail = node;
}
static size_t dequeue(WaitGraph* graph, size_t* head, size_t* tail)
{
    size_t node = *head; *head = graph->nodes[node].queueNext;
    if (*head == WAIT_NONE) *tail = WAIT_NONE;
    return node;
}
/* Reverse edges compute the greatest blocked fixed point in O(V+E), including
 * duplicate slots, then paths back to root in the remaining blocked graph.
 * Scratch is private; no capture edge or SCC approximation grants authority. */
static bool solve_graph(WaitGraph* graph, size_t root)
{
    size_t head = WAIT_NONE, tail = WAIT_NONE;
    if (root >= graph->count) return false;
    for (size_t i = 0; i < graph->count; ++i) {
        WaitNode* node = &graph->nodes[i]; node->reverse = WAIT_NONE; node->remaining = 0;
        node->blocked = true; node->reachesRoot = false;
    }
    for (size_t i = 0; i < graph->edgeCount; ++i) {
        WaitEdge* edge = &graph->edges[i];
        if (edge->target != WAIT_NONE) {
            edge->nextReverse = graph->nodes[edge->target].reverse; graph->nodes[edge->target].reverse = i;
            ++graph->nodes[edge->source].remaining;
        }
    }
    for (size_t i = 0; i < graph->count; ++i) {
        WaitNode* node = &graph->nodes[i];
        if (!node->count || (node->all ? !node->remaining : node->remaining != node->count)) {
            node->blocked = false; enqueue(graph, i, &head, &tail);
        }
    }
    while (head != WAIT_NONE) {
        size_t open = dequeue(graph, &head, &tail);
        for (size_t e = graph->nodes[open].reverse; e != WAIT_NONE; e = graph->edges[e].nextReverse) {
            size_t source = graph->edges[e].source; WaitNode* node = &graph->nodes[source];
            if (node->blocked && (!node->all || !--node->remaining)) {
                node->blocked = false; enqueue(graph, source, &head, &tail);
            }
        }
    }
    if (!graph->nodes[root].blocked) return false;
    graph->nodes[root].reachesRoot = true; enqueue(graph, root, &head, &tail);
    while (head != WAIT_NONE) {
        size_t reached = dequeue(graph, &head, &tail);
        for (size_t e = graph->nodes[reached].reverse; e != WAIT_NONE; e = graph->edges[e].nextReverse) {
            size_t source = graph->edges[e].source; WaitNode* node = &graph->nodes[source];
            if (node->blocked && !node->reachesRoot) { node->reachesRoot = true; enqueue(graph, source, &head, &tail); }
        }
    }
    WaitNode* node = &graph->nodes[root];
    for (size_t i = 0; i < node->count; ++i) {
        size_t target = graph->edges[node->first + i].target;
        if (target != WAIT_NONE && graph->nodes[target].reachesRoot) return true;
    }
    return false;
}
XL6_FutureWaitProof xl6FutureWaitCycleMemberV2(xrtownershipref producer, const void* token)
{
    WaitGraph graph; xerror* prior = xrtTakeError(); graph_init(&graph);
    WaitBuild built = build_graph(&graph, producer, token);
    XL6_FutureWaitProof proof = built == WAIT_BUILD_RESOURCE ? XL6_FUTURE_WAIT_RESOURCE_FAILURE :
        built == WAIT_BUILD_READY && solve_graph(&graph, 0) ? XL6_FUTURE_WAIT_CYCLE_MEMBER : XL6_FUTURE_WAIT_UNPROVEN;
    graph_free(&graph);
    /* Analysis is not language execution. Resource pressure postpones the
     * decision, without overwriting a primary diagnostic or invoking user
     * cleanup. The result is explicit even when no error can be allocated. */
    xrtClearError(); xrtSetErrorTake(prior); return proof;
}
