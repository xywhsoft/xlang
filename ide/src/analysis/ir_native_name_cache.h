#ifndef XLS_IR_NATIVE_NAME_CACHE_H
#define XLS_IR_NATIVE_NAME_CACHE_H
#include "analysis/ir_native_pipeline.h"
typedef struct xls_ir_name_cache xls_ir_name_cache;
typedef struct xls_ir_name_cache_work {
 uint64_t checked,reused,captured,declined,nativeNodes,restoredNodes;
 size_t bytes;
} xls_ir_name_cache_work;
/* Explicit qualified component,phase18 only. Previous is an immutable borrow;
 * no AST,module,result or old cache container is retained. Exclusive pipeline
 * use required;start no later than phase18 and not within its partial scan.
 * *out must be NULL;publication requires the complete native verifier.
 * Retained metadata and separate phase scratch each have limit<=16MiB.
 * Zero disables caching;optional allocation failures use native production.
 * No class layouts uses native production without cache metadata. Scratch is
 * shape-sized within the cap;oversized input keys may decline capture.
 * Exact IDs/common structure may reject reuse. Not full final-function IR. */
int xlsIrNativePipelineFinishCachedNames(xls_ir_native_pipeline*,XL6_DiagList*,
 const xls_ir_name_cache*,size_t limit,xls_ir_name_cache** out,
 xls_ir_name_cache_work*,xls_ir_pipeline_status*);
void xlsIrNameCacheDestroy(xls_ir_name_cache*);
size_t xlsIrNameCacheBytes(const xls_ir_name_cache*);
#endif
