#ifndef XLS_IR_TABLE_GRAPH_H
#define XLS_IR_TABLE_GRAPH_H
#include "analysis/ir_node_graph.h"

typedef struct xls_ir_table_graph xls_ir_table_graph;
typedef struct xls_ir_table_unit xls_ir_table_unit;
typedef struct xls_ir_table_image xls_ir_table_image;
size_t xlsIrTableSchemaCount(void);
size_t xlsIrTableSchemaFind(const char* name); /* SIZE_MAX when absent */
const char* xlsIrTableSchemaName(size_t table);
size_t xlsIrTableSchemaRowBytes(size_t table);
typedef struct xls_ir_table_capture_input {
    size_t table,first,count;const void* rows;
    /* Needed only for non-NULL persistence keys;reads just the referenced
     * ClassField and its canonical key pointers. Never retained. */
    const XL6_IrModule* aliases;
} xls_ir_table_capture_input;
/* One actual native struct row,or one complete scalar arena (first=0,
 * including an explicit empty arena). No graph or other table capture.
 * Caller supplies the selected native type and immutable validated value;
 * IDs stay exact,not a semantic/freshness certificate. Optional cohort
 * shares only its token under the same immutable-source caller proof.
 * Budget/peak charge the unit and one token,also conservatively when shared;
 * exclude inputs,allocator/RSS. Zero declines;failure leaves peak zero.
 * Payload packing reuses the actual native ownership policies. */
xls_ir_table_unit* xlsIrTableUnitCapture(const xls_ir_table_capture_input*,
    const xls_ir_table_unit* cohort,size_t bytes,int (*cancelled)(void*),void*,
    xls_ir_graph_status*,size_t* peakBytes);
/* Caller holds a validated immutable native module. Owns every non-node
 * table value and pointer payload;native integer IDs remain exact. Not an
 * input/freshness certificate or permission to reuse in another namespace.
 * Each record is independent;dense scalar arenas use one vector unit.
 * bytes bounds requested graph/unit allocation bytes,not input/allocator/RSS. */
xls_ir_table_graph* xlsIrTablesCapture(const XL6_IrModule*,size_t bytes,
    int (*cancelled)(void*),void*,xls_ir_graph_status*);
xls_ir_table_graph* xlsIrTablesRetain(xls_ir_table_graph*);
void xlsIrTablesRelease(xls_ir_table_graph*);
size_t xlsIrTablesBytes(const xls_ir_table_graph*);
size_t xlsIrTablesCount(const xls_ir_table_graph*);
const char* xlsIrTablesName(const xls_ir_table_graph*,size_t table);
size_t xlsIrTablesRows(const xls_ir_table_graph*,size_t table);
size_t xlsIrTablesUnits(const xls_ir_table_graph*,size_t table);
/* Own one reference;does not retain the graph,other rows,AST or old result.
 * Pointer payloads are immutable unit-owned copies. Native borrow aliases
 * between rows are restored only in a complete materialized table image. */
xls_ir_table_unit* xlsIrTablesPinUnit(const xls_ir_table_graph*,size_t table,size_t unit);
xls_ir_table_unit* xlsIrTableUnitRetain(xls_ir_table_unit*);
void xlsIrTableUnitRelease(xls_ir_table_unit*);
size_t xlsIrTableUnitBytes(const xls_ir_table_unit*);
size_t xlsIrTableUnitRows(const xls_ir_table_unit*);
size_t xlsIrTableUnitRowBytes(const xls_ir_table_unit*);
size_t xlsIrTableUnitFirstRow(const xls_ir_table_unit*);
const char* xlsIrTableUnitName(const xls_ir_table_unit*);
const void* xlsIrTableUnitRow(const xls_ir_table_unit*,size_t row);
const void* xlsIrTableUnitProducer(const xls_ir_table_unit*);
/* Exact captured ClassField pointer selector:0none,1name,2JSON,3XLON.
 * Still in the source classField index namespace;not a current alias proof. */
unsigned xlsIrTableUnitKeyAlias(const xls_ir_table_unit*);

/* Explicit non-owning node/child inputs must live for the entire image.
 * Counts must match the captured native namespace;this is structural only.
 * Caller must prove matching semantic input and IDs. Image owns flat table
 * arrays and retains its graph. Never native-ModuleFree/mutate this view. */
xls_ir_table_image* xlsIrTablesMaterialize(xls_ir_table_graph*,
    const XL6_IrNode*,size_t nodeCount,const XL6_IrNodeId*,size_t childCount,
    size_t bytes,int (*cancelled)(void*),void*,xls_ir_graph_status*);
const XL6_IrModule* xlsIrTableImageModule(const xls_ir_table_image*);
size_t xlsIrTableImageBytes(const xls_ir_table_image*);
void xlsIrTableImageDestroy(xls_ir_table_image*);
#endif
