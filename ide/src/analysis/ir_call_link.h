#ifndef XLS_IR_CALL_LINK_H
#define XLS_IR_CALL_LINK_H
#include "analysis/ir_table_graph.h"
#include <stdint.h>

typedef enum xls_ir_call_domain {
 XLS_CALL_SITE,XLS_CALL_ARGUMENT,XLS_CALL_DYNAMIC_SITE,XLS_CALL_DYNAMIC_ARGUMENT,
 XLS_CALL_METHOD,XLS_CALL_PARAMETER,XLS_CALL_INTRINSIC,XLS_CALL_OPERAND,
 XLS_CALL_NODE,XLS_CALL_TYPE,XLS_CALL_SYMBOL,XLS_CALL_CLASS_SLOT,
 XLS_CALL_PROTOCOL_SLOT,XLS_CALL_RUNTIME_TAG,XLS_CALL_FUNCTION,XLS_CALL_CODEC,
 XLS_CALL_DOMAIN_COUNT
} xls_ir_call_domain;
typedef struct xls_ir_call_binding {
 xls_ir_table_unit* unit;const void* producer;
 /* Pure deterministic explicit map. Table rows/slices use zero-based IDs;
  * other domains use one-based IDs,with zero bypassing the callback. */
 int(*map)(void*,xls_ir_call_domain,uint64_t,uint64_t*);void* user;
} xls_ir_call_binding;
typedef enum xls_ir_call_reason {
 XLS_CALL_NONE,XLS_CALL_INPUT,XLS_CALL_NAMESPACE,XLS_CALL_UNMAPPED,
 XLS_CALL_RANGE,XLS_CALL_DUPLICATE,XLS_CALL_DIFFERENT
} xls_ir_call_reason;
typedef struct xls_ir_call_error {
 xls_ir_call_reason reason;size_t binding;const char* field;
 xls_ir_call_domain domain;uint64_t value;
} xls_ir_call_error;
typedef struct xls_ir_call_link xls_ir_call_link;
/* Link all rows of the eight call-related tables from independent units.
 * Exact current validated metadata is a call borrow and comparison oracle,
 * not an input/dependency certificate. Map every reference explicitly;
 * nonempty slices must remain contiguous. Empty slice offsets adopt the
 * current value without fabricating a reference. Parameter/evaluation
 * ordinals,ABI identities and source names remain exact values.
 * Own flat arrays and retain only selected units for immutable strings.
 * Do not retain current,module,graph,AST,map or previous-result chains.
 * bytes covers requested result+scratch,not inputs/allocator/RSS. Failure
 * publishes nothing. Single string comparisons are not byte-preemptible. */
xls_ir_call_link* xlsIrCallLink(const XL6_IrModule*,const xls_ir_call_binding*,
 size_t count,size_t bytes,int(*cancelled)(void*),void*,
 xls_ir_graph_status*,xls_ir_call_error*);
/* Replace only these eight tables of a caller-owned readonly module view.
 * All other arrays need their own owners;never native-ModuleFree this view.
 * The link must outlive every read of its arrays. */
int xlsIrCallLinkApply(const xls_ir_call_link*,XL6_IrModule*);
size_t xlsIrCallLinkBytes(const xls_ir_call_link*);
size_t xlsIrCallLinkPeakBytes(const xls_ir_call_link*);
void xlsIrCallLinkDestroy(xls_ir_call_link*);
#endif
