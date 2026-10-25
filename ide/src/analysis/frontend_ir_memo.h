#ifndef XLANG_ANALYSIS_FRONTEND_IR_MEMO_H
#define XLANG_ANALYSIS_FRONTEND_IR_MEMO_H
#include "src/ir/xl6_ir.h"
typedef struct xls_ir_memo xls_ir_memo;
typedef struct xls_ir_result xls_ir_result;
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
/* Optional ownership-index retained capacities only,max/default128KiB.
 * Zero disables borrowing. Decline/allocation failure uses ordinary owned
 * text. This is not the fixed owner,IR arena,model,transient or RSS budget. */
xls_ir_result* xlsFrontendIrResultBuildWithStorage(const XL6_AstNode* root,XL6_DiagList* diags,
    const xls_ir_result* previous,size_t bytes,size_t storageBytes);
const XL6_IrModule* xlsFrontendIrResultModule(const xls_ir_result* result);
xls_ir_memo_work xlsFrontendIrResultWork(const xls_ir_result* result);
void xlsFrontendIrResultDestroy(xls_ir_result* result);
#endif
