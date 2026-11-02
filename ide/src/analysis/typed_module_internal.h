#ifndef XLS_TYPED_MODULE_INTERNAL_H
#define XLS_TYPED_MODULE_INTERNAL_H
#include "analysis/typed_module.h"
#include "src/ast/xl6_ast.h"
/* Takes root ownership only on success. Caller may keep a borrowed alias;
 * it must not mutate/free the root and must hold this module while reading. */
xls_typed_module* xlsTypedModuleTake(XL6_AstNode* root);
/* Construction-private composition. owners has exactly root->childCount slots;
 * NULL slots transfer exclusively owned fresh children,non-NULL slots borrow
 * precisely that immutable function root. Success retains each borrowed owner
 * independently;failure changes/takes nothing. No freshness/semantic proof is
 * supplied here. The caller must establish that before composing a result. */
xls_typed_module* xlsTypedModuleTakeBorrowed(XL6_AstNode* root,xls_typed_function* const* owners,size_t count);
const XL6_AstNode* xlsTypedModuleRoot(const xls_typed_module*);
/* Optional ownership partition of immediate module function declarations.
 * rootSlot is the ORIGINAL module child slot,not a new function ordinal.
 * One owned reference,or NULL for non-functions/resource fallback. O(1).
 * All AST values/addresses stay unchanged until final module destruction.
 * Input must be an exclusively owned forest,as for Take itself. */
xls_typed_function* xlsTypedModuleFunction(const xls_typed_module*,size_t rootSlot);
const XL6_AstNode* xlsTypedFunctionRoot(const xls_typed_function*);
/* Known metadata shared by siblings;conservatively charge it in FULL to a
 * retained function. Native XRT pool backing is outside this local count. */
size_t xlsTypedFunctionMetadataBytes(const xls_typed_function*);
/* Stable while a function reference lives. Deduplicate known metadata only;
 * never use this opaque owner identity as an input or semantic equality key. */
const void* xlsTypedFunctionMetadataOwner(const xls_typed_function*);
/* Construction-private,once before publication. Snapshot fresh native forests
 * with full known shared pool backing upper bounds,children/strings/sources.
 * No reads/writes of source lifetime counters. Borrowed forests keep their
 * original snapshot. A pure budget stop certifies only the completed prefix;
 * malformed trees,allocation failures and cancellation certify nothing.
 * Failure declines retention,never the semantic tree. */
int xlsTypedModuleMeasureFunctions(xls_typed_module*,size_t limit,int (*cancelled)(void*),void*);
/* Conservative union cost for all certified functions and their FULL shared
 * pool backing. The entire forest metadata is included. Zero for functions
 * outside the certified prefix. Immutable after publication;not a freshness proof. */
size_t xlsTypedFunctionKnownBytes(const xls_typed_function*);
/* Original module child slot;SIZE_MAX for NULL. Ownership identity only. */
size_t xlsTypedFunctionRootSlot(const xls_typed_function*);
/* Optional independent owner identity. Borrowed while the module lives;
 * retain it to compare owners after a module/root is destroyed. It retains
 * neither the AST nor a module/check and cannot prove input freshness.
 * NULL declines provenance optimizations without invalidating the module. */
typedef struct xls_typed_input_identity xls_typed_input_identity;
/* Equality token only; the issuer supplies any semantic proof separately. */
xls_typed_input_identity* xlsTypedInputIdentityCreate(void);
size_t xlsTypedInputIdentityOwnedBytes(const xls_typed_input_identity*);
xls_typed_input_identity* xlsTypedModuleIdentity(const xls_typed_module*);
xls_typed_input_identity* xlsTypedInputIdentityRetain(xls_typed_input_identity*);
void xlsTypedInputIdentityRelease(xls_typed_input_identity*);
#endif
