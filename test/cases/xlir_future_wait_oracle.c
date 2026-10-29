/* Exercise the production solver, not a copy. The independent oracle below
 * starts with potentially completing nodes (least fixed point) and uses a
 * dense transitive closure, instead of blocked-node reverse work queues.
 * This is a graph-algorithm test only; real provider/claim/module tests live
 * in xlir_module_aggregate_dependency_host.c. No mock provider is admitted. */
#define XRT_MODULE_MEMORY_DEBUG
#include "src/runtime/xl6_future_wait.c"
#include <assert.h>
#include <stdio.h>

static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before->LiveCount && after.LiveBytes == before->LiveBytes);
    assert(after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount);
    assert(after.InvalidFreeCount == before->InvalidFreeCount && after.DoubleFreeCount == before->DoubleFreeCount &&
        after.UseAfterFreeCount == before->UseAfterFreeCount);
}
static bool oracle(const WaitGraph* graph, size_t root)
{
    bool complete[4] = {0}, path[4][4] = {{0}}, changed;
    assert(graph->count <= 4 && root < graph->count);
    do {
        changed = false;
        for (size_t i = 0; i < graph->count; ++i) {
            const WaitNode* node = &graph->nodes[i];
            bool value = node->all || !node->count;
            for (size_t j = 0; j < node->count; ++j) {
                size_t target = graph->edges[node->first + j].target;
                bool available = target == WAIT_NONE || complete[target];
                value = node->all ? value && available : value || available;
            }
            if (value && !complete[i]) { complete[i] = true; changed = true; }
        }
    } while (changed);
    for (size_t i = 0; i < graph->edgeCount; ++i) {
        const WaitEdge* edge = &graph->edges[i];
        if (edge->target != WAIT_NONE && !complete[edge->source] && !complete[edge->target])
            path[edge->source][edge->target] = true;
    }
    for (size_t k = 0; k < graph->count; ++k) for (size_t i = 0; i < graph->count; ++i)
        for (size_t j = 0; j < graph->count; ++j) path[i][j] |= path[i][k] && path[k][j];
    return path[root][root];
}
static void make_graph(WaitGraph* graph, unsigned n, unsigned rules, unsigned links, unsigned open, unsigned duplicate)
{
    graph_init(graph); graph->count = n;
    for (unsigned i = 0; i < n; ++i) {
        WaitNode* node = &graph->nodes[i]; node->first = graph->edgeCount; node->all = (rules & (1u << i)) != 0;
        for (unsigned j = 0; j < n; ++j) if (links & (1u << (i * n + j)))
            for (unsigned copy = 0; copy <= duplicate; ++copy) assert(add_edge(graph, i, j));
        if (open & (1u << i)) assert(add_edge(graph, i, WAIT_NONE));
        node->count = graph->edgeCount - node->first;
    }
}
static void boundary_cases(void)
{
    WaitGraph graph;
    /* ALL(self, unknown) is blocked, but ANY(self, unknown) can progress. */
    make_graph(&graph, 1, 1, 1, 1, 1); assert(solve_graph(&graph, 0)); graph_free(&graph);
    make_graph(&graph, 1, 0, 1, 1, 1); assert(!solve_graph(&graph, 0)); graph_free(&graph);
    /* 0 feeds 1 -> 2 -> 1: only actual members authorize interruption. */
    make_graph(&graph, 3, 7, (1u << 1) | (1u << 5) | (1u << 7), 0, 1);
    assert(!solve_graph(&graph, 0) && solve_graph(&graph, 1) && solve_graph(&graph, 2)); graph_free(&graph);
    /* An open alternative at 1 dissolves the apparent 0 <-> 1 cycle. */
    make_graph(&graph, 2, 1, (1u << 1) | (1u << 2), 2, 1);
    assert(!solve_graph(&graph, 0) && !solve_graph(&graph, 1)); graph_free(&graph);
}
static unsigned growth_failures(void)
{
    unsigned failures = 0, successes = 0; unsigned identities[96] = {0};
    for (unsigned budget = 0; budget < 16; ++budget) {
        WaitGraph graph; xmemdebugsnapshot before; bool ok = true;
        xrtMemDebugSnapshot(&before); graph_init(&graph); assert(xrtMemDebugFailAfter(budget));
        for (size_t i = 0; i < 96 && ok; ++i) {
            size_t index; ok = add_node(&graph, (xrtownershipref){&identities[i], NULL}, &index);
            if (ok) assert(index == i);
        }
        for (size_t i = 0; i < 96 && ok; ++i) {
            graph.nodes[i].first = graph.edgeCount; graph.nodes[i].count = 8; graph.nodes[i].all = (i & 1) != 0;
            for (unsigned j = 0; j < 8 && ok; ++j) ok = add_edge(&graph, i, (i + 1) % 96);
        }
        bool failed = xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); xrtClearError();
        if (ok) { assert(!failed && graph.count == 96 && graph.edgeCount == 768); ++successes;
            for (size_t i = 0; i < 96; ++i) assert(solve_graph(&graph, i));
        } else { assert(failed); ++failures; }
        graph_free(&graph); balanced(&before);
    }
    assert(failures == 8 && successes == 8); return failures;
}
int main(void)
{
    uint64_t graphs = 0, checks = 0; xmemdebugsnapshot before;
    assert(xrtMemDebugEnable(true)); xrtMemDebugSnapshot(&before); boundary_cases();
    for (unsigned duplicate = 0; duplicate < 2; ++duplicate) for (unsigned n = 1; n <= 3; ++n)
        for (unsigned rules = 0; rules < (1u << n); ++rules) for (unsigned links = 0; links < (1u << (n * n)); ++links)
            for (unsigned open = 0; open < (1u << n); ++open) {
                WaitGraph graph; make_graph(&graph, n, rules, links, open, duplicate); ++graphs;
                for (size_t root = 0; root < n; ++root) {
                    bool expected = oracle(&graph, root); assert(solve_graph(&graph, root) == expected); ++checks;
                }
                graph_free(&graph);
            }
    unsigned failures = growth_failures(); balanced(&before);
    assert(graphs == 66064 && checks == 197648);
    printf("Future wait oracle: %llu exhaustive graphs, %llu root decisions, ordered duplicates, ALL/ANY open alternatives and feeders; 96-node/768-edge growth, 16 allocation budgets, %u exact failures; memory balanced\n",
        (unsigned long long)graphs, (unsigned long long)checks, failures);
    return 0;
}
