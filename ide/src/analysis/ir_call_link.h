#ifndef XLS_IR_CALL_LINK_H
#define XLS_IR_CALL_LINK_H
#include "analysis/ir_record_link.h"
typedef xls_ir_record_domain xls_ir_call_domain;
typedef xls_ir_record_binding xls_ir_call_binding;
typedef xls_ir_record_reason xls_ir_call_reason;
typedef xls_ir_record_error xls_ir_call_error;
typedef xls_ir_record_link xls_ir_call_link;
#define XLS_CALL_SITE XLS_RECORD_SITE
#define XLS_CALL_ARGUMENT XLS_RECORD_ARGUMENT
#define XLS_CALL_DYNAMIC_SITE XLS_RECORD_DYNAMIC_SITE
#define XLS_CALL_DYNAMIC_ARGUMENT XLS_RECORD_DYNAMIC_ARGUMENT
#define XLS_CALL_METHOD XLS_RECORD_METHOD
#define XLS_CALL_PARAMETER XLS_RECORD_PARAMETER
#define XLS_CALL_INTRINSIC XLS_RECORD_INTRINSIC
#define XLS_CALL_OPERAND XLS_RECORD_OPERAND
#define XLS_CALL_NODE XLS_RECORD_NODE
#define XLS_CALL_TYPE XLS_RECORD_TYPE
#define XLS_CALL_SYMBOL XLS_RECORD_SYMBOL
#define XLS_CALL_CLASS_SLOT XLS_RECORD_CLASS_SLOT
#define XLS_CALL_PROTOCOL_SLOT XLS_RECORD_PROTOCOL_SLOT
#define XLS_CALL_RUNTIME_TAG XLS_RECORD_RUNTIME_TAG
#define XLS_CALL_FUNCTION XLS_RECORD_FUNCTION
#define XLS_CALL_CODEC XLS_RECORD_CODEC
#define XLS_CALL_NONE XLS_RECORD_NONE
#define XLS_CALL_INPUT XLS_RECORD_INPUT
#define XLS_CALL_NAMESPACE XLS_RECORD_NAMESPACE
#define XLS_CALL_UNMAPPED XLS_RECORD_UNMAPPED
#define XLS_CALL_RANGE XLS_RECORD_RANGE
#define XLS_CALL_DUPLICATE XLS_RECORD_DUPLICATE
#define XLS_CALL_DIFFERENT XLS_RECORD_DIFFERENT
#define XLS_CALL_DOMAIN_COUNT XLS_RECORD_CLEANUP
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
