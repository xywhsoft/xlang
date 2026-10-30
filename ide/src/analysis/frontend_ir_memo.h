#ifndef XLANG_ANALYSIS_FRONTEND_IR_MEMO_H
#define XLANG_ANALYSIS_FRONTEND_IR_MEMO_H
#include "src/ir/xl6_ir.h"
typedef struct xls_ir_memo xls_ir_memo;
typedef struct xls_ir_result xls_ir_result;
struct xls_entity_view;
/* Final typed-module storage. Exact complete native non-coordinate input is
 * required;all native IDs remain exact. Current spans are rebuilt and the full
 * original verifier always runs. Explicit opt-in;ordinary APIs stay unchanged.
 * persistentBytes bounds key/ownership metadata only,max32MiB;it excludes the
 * existing model,raw fragment payloads,transients andRSS,not globalmemory. */
typedef struct xls_ir_persistent_work {
    uint64_t built,reused,captured,verified,nodesCopied,tablesShared,keyBytes,metadataBytes,declined;
} xls_ir_persistent_work;
xls_ir_result* xlsFrontendIrResultBuildPersistent(const XL6_AstNode*,XL6_DiagList*,
    const xls_ir_result*,size_t rawBytes,size_t ownerBytes,size_t persistentBytes);
xls_ir_persistent_work xlsFrontendIrResultPersistentWork(const xls_ir_result*);
struct xls_typed_module;
typedef struct xls_ir_input_work {
    uint64_t encoded,reused,captured,declined,avoidedBytes,copiedBytes,bytes;
} xls_ir_input_work;
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
struct xls_sem_memo;
/* Current validated check only. Bind its unpublished semantic memo to the
 * independently owned immutable typed input before reading final receipts.
 * Ordinary mutable-AST/typed APIs keep their complete key path. */
xls_ir_result* xlsFrontendIrResultBuildTypedChecked(struct xls_typed_module*,XL6_DiagList*,
    const xls_ir_result*,size_t,const struct xls_entity_view*,size_t,struct xls_sem_memo*);
xls_ir_input_work xlsFrontendIrResultInputWork(const xls_ir_result*);
#endif
/* Caller holds result lifetime. Concurrent preparation never waits:one owner
 * builds metadata;other callers use the original route. 2=new,1=ready,0=decline.
 * Only an independently owned typed input may prepare a published result. */
int xlsFrontendIrResultPreparePersistent(xls_ir_result*,size_t persistentBytes);
xls_ir_result* xlsFrontendIrResultBuildTyped(struct xls_typed_module*,XL6_DiagList*,
    const xls_ir_result*,size_t rawBytes,const struct xls_entity_view*,size_t persistentBytes);
xls_ir_result* xlsFrontendIrResultBuildEntitiesPersistent(const XL6_AstNode*,XL6_DiagList*,
    const xls_ir_result*,size_t rawBytes,const struct xls_entity_view*,size_t persistentBytes);

typedef struct xls_ir_memo_work {
    uint64_t checked,reused,captured,nodesReused,bytes,declined;
    uint64_t textNodes,textBytes,ownerBytes;
} xls_ir_memo_work;
/* Optional immutable pre-pass function fragments. No final XLIR is shared.
 * The full compiler builder,all module passes and independent verifier remain
 * authoritative. Changed/unsupported/resource-failed fragments use the exact
 * original builder. Caller owns previous for this call;new memos never hold it.
 * Logical retained data budget<=32MiB;not transient allocations or RSS. Zero
 * disables capture/reuse. Full context/input validity remains caller-owned. */
int xlsFrontendIrBuildMemo(XL6_IrModule* module,const XL6_AstNode* root,XL6_DiagList* diags,
    const xls_ir_memo* previous,xls_ir_memo** result,xls_ir_memo_work* work,size_t bytes);
void xlsFrontendIrMemoDestroy(xls_ir_memo* memo);
/* Opaque IR owner may borrow immutable text/literals from pinned fragments.
 * Never free/mutate its borrowed module;destroy this result as one unit.
 * previous is borrowed for this call only;no result retains a previous one.
 * Independent original compiler modules remain unchanged. NULL means build
 * failed or optional owner allocation failed;caller retains its oracle path. */
xls_ir_result* xlsFrontendIrResultBuild(const XL6_AstNode* root,XL6_DiagList* diags,
    const xls_ir_result* previous,size_t bytes);
/* Current checked owner supplies an optional retained namespace/binding view.
 * Complete normalized AST/type-prefix/current-symbol dependency proofs still
 * gate every raw fragment. This does not cache final passes or verification. */
xls_ir_result* xlsFrontendIrResultBuildEntities(const XL6_AstNode* root,XL6_DiagList* diags,
    const xls_ir_result* previous,size_t bytes,const struct xls_entity_view* entities);
/* Optional ownership-index retained capacities only,max/default128KiB.
 * Zero disables borrowing. Decline/allocation failure uses ordinary owned
 * text. This is not the fixed owner,IR arena,model,transient or RSS budget. */
xls_ir_result* xlsFrontendIrResultBuildWithStorage(const XL6_AstNode* root,XL6_DiagList* diags,
    const xls_ir_result* previous,size_t bytes,size_t storageBytes);
const XL6_IrModule* xlsFrontendIrResultModule(const xls_ir_result* result);
xls_ir_memo_work xlsFrontendIrResultWork(const xls_ir_result* result);
/* Conservative declaration dependencies of one actually captured raw function.
 * Includes its global operands/writes as well as reads. Interface versions are
 * optional(0); IDs need equal retained store/domain and never prove freshness.
 * Returned values are copied;no AST/result borrow,no allocation or I/O. */
typedef struct xls_ir_declaration_dependency {uint64_t declaration,version;} xls_ir_declaration_dependency;
size_t xlsFrontendIrResultDependencies(const xls_ir_result* result,uint64_t function);
int xlsFrontendIrResultDependency(const xls_ir_result* result,uint64_t function,size_t at,xls_ir_declaration_dependency* out);
void xlsFrontendIrResultDestroy(xls_ir_result* result);
#endif
