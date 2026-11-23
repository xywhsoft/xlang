#ifndef XLS_IR_NATIVE_HANDLE_GROUP_H
#define XLS_IR_NATIVE_HANDLE_GROUP_H
#include "analysis/ir_native_pipeline.h"
typedef struct xls_ir_handle_group xls_ir_handle_group;
typedef struct xls_ir_handle_group_work {
 uint64_t checked,reused,captured,declined,proofChecks;
 uint64_t bindNodes,projectNodes,restoredBinds,restoredProjects;
 size_t bytes;
} xls_ir_handle_group_work;
/* Explicit reviewed component. One immutable input proof covers both adjacent
 * phases28/29,while preserving all-binds-then-all-projects order. Exact native
 * IDs required. No fresh final-output oracle or AST/module/result retention.
 * Previous is borrowed until return;exclusive pipeline use. Start no later
 * than phase28 and not within its partial scan. *out must be NULL;only full
 * module verification publishes it. Ready is idempotent without a new cache.
 * Retained metadata<=16MiB;combined provisional records+phase scratch<=16MiB.
 * Optional metadata failure uses native production;required native output
 * failure or cancellation retires the module. Zero disables caching. */
int xlsIrNativePipelineFinishCachedHandles(xls_ir_native_pipeline*,XL6_DiagList*,
 const xls_ir_handle_group*,size_t,xls_ir_handle_group**,
 xls_ir_handle_group_work*,xls_ir_pipeline_status*);
void xlsIrHandleGroupDestroy(xls_ir_handle_group*);
size_t xlsIrHandleGroupBytes(const xls_ir_handle_group*);
#endif
