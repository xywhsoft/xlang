#ifndef XLS_IR_NODE_GRAPH_H
#define XLS_IR_NODE_GRAPH_H
#include "src/ir/xl6_ir.h"

typedef struct xls_ir_node_graph xls_ir_node_graph;
typedef struct xls_ir_node_unit xls_ir_node_unit;
typedef struct xls_ir_node_image xls_ir_node_image;
typedef enum xls_ir_graph_status {
    XLS_IR_GRAPH_OK, XLS_IR_GRAPH_INVALID, XLS_IR_GRAPH_BUDGET,
    XLS_IR_GRAPH_MEMORY, XLS_IR_GRAPH_CANCELLED
} xls_ir_graph_status;

/* Immutable final-node ownership,not a semantic or freshness certificate.
 * Caller holds a validated,immutable native module throughout capture. All
 * referenced nodes and spans are checked as a rooted tree. Native node,
 * symbol,type,region and table IDs remain exact in their producer namespace.
 * The graph does not capture the module's non-node tables or retain the
 * module,AST,check or an earlier graph. Unit0 contains the module root;each
 * immediate root child owns one complete subtree,including nested callables.
 * Each unit independently owns its nodes,child lists and text/binary data.
 * Other-unit references are preserved as native integer IDs.
 * bytes bounds peak requested graph/unit/scratch allocation bytes,excluding
 * allocator overhead and the existing input. Zero declines capture. */
xls_ir_node_graph* xlsIrNodeGraphCapture(const XL6_IrModule*,size_t bytes,
    int (*cancelled)(void*),void*,xls_ir_graph_status*);
xls_ir_node_graph* xlsIrNodeGraphRetain(xls_ir_node_graph*);
void xlsIrNodeGraphRelease(xls_ir_node_graph*);
size_t xlsIrNodeGraphBytes(const xls_ir_node_graph*);
size_t xlsIrNodeGraphPeakBytes(const xls_ir_node_graph*);
size_t xlsIrNodeGraphUnitCount(const xls_ir_node_graph*);
size_t xlsIrNodeGraphNodeCount(const xls_ir_node_graph*);
XL6_IrNodeId xlsIrNodeGraphRoot(const xls_ir_node_graph*);
const XL6_IrNode* xlsIrNodeGraphNode(const xls_ir_node_graph*,XL6_IrNodeId);
const XL6_IrNodeId* xlsIrNodeGraphChildren(const xls_ir_node_graph*,XL6_IrNodeId,size_t* count);
/* One owned reference. Unit lifetime is independent from the graph. */
xls_ir_node_unit* xlsIrNodeGraphPinUnit(const xls_ir_node_graph*,size_t unit);
xls_ir_node_unit* xlsIrNodeUnitRetain(xls_ir_node_unit*);
void xlsIrNodeUnitRelease(xls_ir_node_unit*);
size_t xlsIrNodeUnitBytes(const xls_ir_node_unit*);
size_t xlsIrNodeUnitCount(const xls_ir_node_unit*);
XL6_IrNodeId xlsIrNodeUnitRoot(const xls_ir_node_unit*);
XL6_IrNodeId xlsIrNodeUnitNativeId(const xls_ir_node_unit*,size_t local);
const XL6_IrNode* xlsIrNodeUnitNode(const xls_ir_node_unit*,size_t local);
const XL6_IrNodeId* xlsIrNodeUnitChildren(const xls_ir_node_unit*,size_t local,size_t* count);
/* Independent provenance token,borrowed while this unit lives. Pointer
 * equality identifies an originating snapshot only;it proves no freshness,
 * semantic equivalence or permission to reuse other module tables. */
const void* xlsIrNodeUnitProducer(const xls_ir_node_unit*);

/* Explicit adapter for consumers requiring native flat arrays. It owns only
 * arrays and a graph reference;payload pointers are read-only graph borrows.
 * Never pass these arrays to the native module destructor. Non-node module
 * tables must be supplied and validated by the consumer separately. */
xls_ir_node_image* xlsIrNodeGraphMaterialize(xls_ir_node_graph*,size_t bytes,
    int (*cancelled)(void*),void*,xls_ir_graph_status*);
void xlsIrNodeImageDestroy(xls_ir_node_image*);
const XL6_IrNode* xlsIrNodeImageNodes(const xls_ir_node_image*,size_t* count);
const XL6_IrNodeId* xlsIrNodeImageChildren(const xls_ir_node_image*,size_t* count);
size_t xlsIrNodeImageBytes(const xls_ir_node_image*);
#endif
