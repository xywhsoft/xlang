#ifndef XLS_TYPED_MODULE_INTERNAL_H
#define XLS_TYPED_MODULE_INTERNAL_H
#include "analysis/typed_module.h"
#include "src/ast/xl6_ast.h"
/* Takes root ownership only on success. Caller may keep a borrowed alias;
 * it must not mutate/free the root and must hold this module while reading. */
xls_typed_module* xlsTypedModuleTake(XL6_AstNode* root);
const XL6_AstNode* xlsTypedModuleRoot(const xls_typed_module*);
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
