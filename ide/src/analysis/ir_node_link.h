#ifndef XLS_IR_NODE_LINK_H
#define XLS_IR_NODE_LINK_H
#include "analysis/ir_node_graph.h"
#include <stdint.h>

typedef enum xls_ir_reference_domain {
    XLS_IR_REF_NODE,XLS_IR_REF_TYPE,XLS_IR_REF_SYMBOL,
    XLS_IR_REF_REGION,XLS_IR_REF_AGGREGATE_FIELD
} xls_ir_reference_domain;
typedef struct xls_ir_node_binding {
    xls_ir_node_unit* unit;const void* producer;
    /* Pure deterministic source-to-current namespace map. No implicit
     * identity fallback. Called only for actual references,not sentinels. */
    int (*map)(void*,xls_ir_reference_domain,uint64_t,uint64_t*);
    void* user;
} xls_ir_node_binding;
typedef struct xls_ir_node_layout {
    const XL6_IrNode* nodes;size_t nodeCount;
    const XL6_IrNodeId* children;size_t childCount;XL6_IrNodeId root;
    size_t typeCount,aggregateFieldCount;
} xls_ir_node_layout;
typedef enum xls_ir_link_reason {
    XLS_IR_LINK_NONE,XLS_IR_LINK_INPUT,XLS_IR_LINK_NAMESPACE,
    XLS_IR_LINK_UNMAPPED,XLS_IR_LINK_RANGE,XLS_IR_LINK_DUPLICATE,
    XLS_IR_LINK_UNCOVERED,XLS_IR_LINK_TOPOLOGY,XLS_IR_LINK_SOURCE
} xls_ir_link_reason;
typedef struct xls_ir_link_error {
    xls_ir_link_reason reason;size_t binding;XL6_IrNodeId source,destination;
    xls_ir_reference_domain domain;uint64_t value;const char* field;
} xls_ir_link_error;
typedef struct xls_ir_node_link xls_ir_node_link;
/* Link selected immutable units into the exact current numbering/topology
 * and spans. Validate rooted layout,bijective coverage,source syntax payloads
 * and mapped topology. Relocate every audited native node reference.
 * Caller proves semantic/dependency eligibility and symbol/region validity;
 * this is not a reuse certificate. Unit producer must match each binding.
 * Inputs/maps are borrowed for the call only. Result pins selected units,
 * never source graphs,ASTs,old results,layout or maps. Payloads stay read-only.
 * Budget bounds requested image plus temporary scratch,not retained input,
 * allocator overhead or RSS. Never native-ModuleFree/mutate the arrays. */
xls_ir_node_link* xlsIrNodeLink(const xls_ir_node_layout*,
    const xls_ir_node_binding*,size_t bindings,size_t bytes,
    int (*cancelled)(void*),void*,xls_ir_graph_status*,xls_ir_link_error*);
void xlsIrNodeLinkDestroy(xls_ir_node_link*);
const XL6_IrNode* xlsIrNodeLinkNodes(const xls_ir_node_link*,size_t* count);
const XL6_IrNodeId* xlsIrNodeLinkChildren(const xls_ir_node_link*,size_t* count);
XL6_IrNodeId xlsIrNodeLinkRoot(const xls_ir_node_link*);
size_t xlsIrNodeLinkBytes(const xls_ir_node_link*);
size_t xlsIrNodeLinkPeakBytes(const xls_ir_node_link*);
#endif
