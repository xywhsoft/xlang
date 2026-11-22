#ifndef XLS_IR_RECORD_LINK_H
#define XLS_IR_RECORD_LINK_H
#include "analysis/ir_table_graph.h"
#include <stdint.h>

typedef enum xls_ir_record_domain {
 XLS_RECORD_SITE,XLS_RECORD_ARGUMENT,XLS_RECORD_DYNAMIC_SITE,XLS_RECORD_DYNAMIC_ARGUMENT,
 XLS_RECORD_METHOD,XLS_RECORD_PARAMETER,XLS_RECORD_INTRINSIC,XLS_RECORD_OPERAND,
 XLS_RECORD_NODE,XLS_RECORD_TYPE,XLS_RECORD_SYMBOL,XLS_RECORD_CLASS_SLOT,
 XLS_RECORD_PROTOCOL_SLOT,XLS_RECORD_RUNTIME_TAG,XLS_RECORD_FUNCTION,XLS_RECORD_CODEC,
 XLS_RECORD_CLEANUP,XLS_RECORD_EXIT_STEP,XLS_RECORD_EXIT_PLAN,XLS_RECORD_INITIALIZER,
 XLS_RECORD_REGION,XLS_RECORD_ENUM_LAYOUT,XLS_RECORD_ENUM_VARIANT,
 XLS_RECORD_ENUM_PAYLOAD,XLS_RECORD_DOMAIN_COUNT
} xls_ir_record_domain;
enum xls_ir_record_family {XLS_RECORD_CALLS=1,XLS_RECORD_CONTROL=2,
 XLS_RECORD_ENUMS=4,XLS_RECORD_ALL=7};
typedef struct xls_ir_record_binding {
 xls_ir_table_unit* unit;const void* producer;
 /* Pure deterministic source-to-current map,writes out on success.
  * Table rows use zero-based IDs;other references use one-based IDs and
  * zero bypasses the callback. Regions use the native uint32 namespace. */
 int(*map)(void*,xls_ir_record_domain,uint64_t,uint64_t*);void* user;
} xls_ir_record_binding;
typedef enum xls_ir_record_reason {
 XLS_RECORD_NONE,XLS_RECORD_INPUT,XLS_RECORD_NAMESPACE,XLS_RECORD_UNMAPPED,
 XLS_RECORD_RANGE,XLS_RECORD_DUPLICATE,XLS_RECORD_DIFFERENT
} xls_ir_record_reason;
typedef struct xls_ir_record_error {
 xls_ir_record_reason reason;size_t binding;const char* field;
 xls_ir_record_domain domain;uint64_t value;
} xls_ir_record_error;
typedef struct xls_ir_record_link xls_ir_record_link;
/* Link exactly the rows of selected families from independently owned units.
 * Current validated metadata and maps are immutable call borrows,not input
 * eligibility certificates. Require explicit maps and contiguous nonempty
 * slices;empty offsets adopt current values. Compare every semantic value.
 * Own flat arrays and retain only chosen units for immutable payloads;no
 * module,graph,AST,current layout,map or previous-result retention.
 * Budget covers requested result+scratch,not input/allocator/RSS. Failure
 * publishes nothing;single text comparisons are not byte-preemptible. */
xls_ir_record_link* xlsIrRecordLink(const XL6_IrModule*,const xls_ir_record_binding*,
 size_t count,unsigned families,size_t bytes,int(*cancelled)(void*),void*,
 xls_ir_graph_status*,xls_ir_record_error*);
/* Apply only the selected tables to a readonly caller-owned module view.
 * All other arrays must use the construction's same current ID namespace
 * and have their own owners. Link outlives reads;never native-ModuleFree. */
int xlsIrRecordLinkApply(const xls_ir_record_link*,XL6_IrModule*);
unsigned xlsIrRecordLinkFamilies(const xls_ir_record_link*);
size_t xlsIrRecordLinkBytes(const xls_ir_record_link*);
size_t xlsIrRecordLinkPeakBytes(const xls_ir_record_link*);
void xlsIrRecordLinkDestroy(xls_ir_record_link*);
#endif
