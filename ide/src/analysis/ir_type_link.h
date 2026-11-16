#ifndef XLS_IR_TYPE_LINK_H
#define XLS_IR_TYPE_LINK_H
#include "analysis/ir_table_graph.h"
#include <stdint.h>

typedef enum xls_ir_type_domain {
    XLS_IR_TYPE_REFERENCE,XLS_IR_TYPE_SYMBOL,XLS_IR_TYPE_SOURCE
} xls_ir_type_domain;
typedef struct xls_ir_type_binding {
    xls_ir_table_unit* unit;const void* producer;
    /* Pure deterministic explicit source-to-current maps. Zero sentinels
     * bypass the callback;successful maps write a nonzero 32-bit result. */
    int (*map)(void*,xls_ir_type_domain,uint64_t,uint64_t*);void* user;
} xls_ir_type_binding;
typedef enum xls_ir_type_reason {
    XLS_IR_TYPE_NONE,XLS_IR_TYPE_INPUT,XLS_IR_TYPE_NAMESPACE,
    XLS_IR_TYPE_UNMAPPED,XLS_IR_TYPE_RANGE,XLS_IR_TYPE_DUPLICATE,
    XLS_IR_TYPE_DIFFERENT
} xls_ir_type_reason;
typedef struct xls_ir_type_error {
    xls_ir_type_reason reason;size_t binding;XL6_IrTypeId source,destination;
    xls_ir_type_domain domain;uint64_t value;const char* field;
} xls_ir_type_error;
typedef struct xls_ir_type_link xls_ir_type_link;
/* Link one independent types.items unit per current row. Current immutable,
 * validated pool supplies layout plus derived hash/lookup/backend names and
 * concrete-type diagnostic source IDs. TYPE_PARAM source IDs require an
 * explicit map. Compare every relocated semantic field and payload with
 * current records;this still proves no input/dependency freshness.
 * Result owns rows,lookup slots and relocated reference vectors;other
 * immutable payloads borrow independently retained units. No old graph,
 * module,AST,map,layout or result-chain retention. Never native-ModuleFree
 * or mutate this readonly pool. Caller holds its lifetime during reads.
 * Budget bounds requested image+scratch bytes,not input/allocator/RSS.
 * Inputs and maps remain immutable call borrows;callbacks must not mutate. */
xls_ir_type_link* xlsIrTypeLink(const XL6_IrTypePool*,const xls_ir_type_binding*,
    size_t count,size_t bytes,int (*cancelled)(void*),void*,
    xls_ir_graph_status*,xls_ir_type_error*);
const XL6_IrTypePool* xlsIrTypeLinkPool(const xls_ir_type_link*);
size_t xlsIrTypeLinkBytes(const xls_ir_type_link*);
size_t xlsIrTypeLinkPeakBytes(const xls_ir_type_link*);
void xlsIrTypeLinkDestroy(xls_ir_type_link*);
#endif
