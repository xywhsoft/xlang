#ifndef XLANG_ANALYSIS_AST_CLONE_H
#define XLANG_ANALYSIS_AST_CLONE_H
#include "src/ast/xl6_ast.h"
#include "analysis/ast_storage.h"
#include <string.h>
/* Private representation copy, never a second parser/semantic rule set.
 * Sources are copied into the new tree, without touching non-atomic source
 * references in an immutable snapshot owned by another thread. */
typedef int (*xls_ast_transform)(const XL6_AstNode* input,XL6_AstNode* output,void* user);
typedef XL6_AstSource* (*xls_ast_source_target)(const XL6_AstSource* input,void* user);
typedef struct xls_ast_clone_options {
    xls_ast_transform transform;
    void* user;
    size_t bytes; /* zero: no extra byte budget; allocation failure still fails */
    int dropSources;
    /* Optional destination owned by a PRIVATE construction tree on this
     * thread. Never return an immutable/published snapshot's source. NULL
     * selects the normal independent source copy. */
    xls_ast_source_target targetSource;
} xls_ast_clone_options;
XL6_AstNode* xlsAstClone(const XL6_AstNode* input,const xls_ast_clone_options* options,size_t* bytes);
/* Explicit opt-in for cursor fact restoration only. The clone still requires
 * different private destination ownership, identical source path and flags.
 * ONLY a start/end difference with cursor flag=1 on both sides may be admitted;
 * default cloning and the existing options layout stay unchanged. */
typedef int (*xls_ast_cursor_source_policy)(const XL6_AstSource* input,const XL6_AstSource* target,void* user);
/* Shared exact admission for eager clones and immutable demand views. The
 * caller separately guarantees that target belongs to its private result. */
static inline int xlsAstCloneSourceTargetValid(const XL6_AstSource* input,const XL6_AstSource* target,
    xls_ast_cursor_source_policy policy,void* user)
{
    if(!input || !target || target==input || !target->references || !!target->path!=!!input->path ||
       (input->path && strcmp(target->path,input->path)) || target->completionQuery!=input->completionQuery)return 0;
    return (target->completionStart==input->completionStart && target->completionEnd==input->completionEnd) ||
        (input->completionQuery==1 && policy && policy(input,target,user));
}
XL6_AstNode* xlsAstCloneWithCursorSource(const XL6_AstNode* input,const xls_ast_clone_options* options,
    xls_ast_cursor_source_policy policy,void* user,size_t* bytes);
/* Optional native representation optimization. Same transform/source contract;
 * failure leaves input untouched. Zero pool allowance selects regular cloning.
 * Work counts constructed nodes/pages and conservative pool capacity charge. */
XL6_AstNode* xlsAstClonePooled(const XL6_AstNode* input,const xls_ast_clone_options* options,
    xls_ast_cursor_source_policy policy,void* user,size_t poolBudget,xls_ast_pool_work* work,size_t* bytes);
typedef struct xls_ast_leaf_work {uint64_t hits,entries,keyBytes;} xls_ast_leaf_work;
/* Replayed semantic fact forests ONLY. Direct forest-root unlocated leaf
 * types with no marker may share within this private result. Exact type/ABI
 * identities are part of each key; all other annotations must be absent.
 * Nested types stay independent: class-handle finalization can write a
 * parent's identity into an unresolved child. Callbacks run in their
 * original order and may inspect/transform values,not retain output addresses.
 * The audited final identity pass writes the same identities for equal leaves;
 * execution plans do not traverse inferred/declared-type forests. Published
 * trees are read-only; ordinary clone APIs always make independent nodes.
 * The temporary exact-key XRT map admits <=64 labels of <=256 bytes;failure
 * disables sharing only. Zero pool budget uses heap nodes with the same refs. */
XL6_AstNode* xlsAstCloneFactTypes(const XL6_AstNode* input,const xls_ast_clone_options* options,
    xls_ast_cursor_source_policy policy,void* user,size_t poolBudget,xls_ast_pool_work* work,
    xls_ast_leaf_work* leaves,size_t* bytes);
typedef struct xls_ast_tree_work {uint64_t entries,hits,validatedNodes,allocatedNodes,certifiedNodes;} xls_ast_tree_work;
/* A certificate provider is an audited state-projection adapter,not a generic
 * transform hint. Mark returns the current coordinate-sequence cursor,or
 * SIZE_MAX to decline. Replay must prove the same complete projected values
 * and reproduce every observable transform effect for that repeated immutable
 * input subtree;zero rejects this clone attempt. NULL replays every callback. */
typedef struct xls_ast_tree_projection {size_t (*mark)(void* user);int (*replay)(void* user,size_t begin,size_t count);} xls_ast_tree_projection;
/* Private STATIC semantic restoration only. Direct type-root aliases select
 * identical immutable input pointers within this one clone call. Each alias
 * either replays all transforms in original preorder or uses the explicit
 * audited projection certificate. Current values/source projection stay exact.
 * Each edge owns a reference on this private result's root.
 * Callers must restore callback state and retry the ordinary clone on failure.
 * No cross-result sharing,cursor admission or transfer of callback-owned fields.
 * max256pointer keys;index failure disables this optional sharing only. */
XL6_AstNode* xlsAstCloneFactTrees(const XL6_AstNode* input,const xls_ast_clone_options* options,
    const xls_ast_tree_projection* projection,size_t poolBudget,xls_ast_pool_work* work,xls_ast_tree_work* trees,size_t* bytes);
/* Borrow a construction-pinned forest owner. Does not unpin it. Per-copy bytes
 * use the original logical node/field accounting; extra pool capacity is bounded
 * separately. No implicit retry or transform/source-contract changes. */
XL6_AstNode* xlsAstCloneInStorage(const XL6_AstNode* input,const xls_ast_clone_options* options,
    xls_ast_storage* storage,size_t extraPoolBytes,size_t* bytes);
#endif
