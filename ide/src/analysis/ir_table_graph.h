#ifndef XLS_IR_TABLE_GRAPH_H
#define XLS_IR_TABLE_GRAPH_H
#include "analysis/ir_node_graph.h"

typedef struct xls_ir_table_graph xls_ir_table_graph;
typedef struct xls_ir_table_unit xls_ir_table_unit;
typedef struct xls_ir_table_image xls_ir_table_image;
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
