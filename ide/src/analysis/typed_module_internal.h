#ifndef XLS_TYPED_MODULE_INTERNAL_H
#define XLS_TYPED_MODULE_INTERNAL_H
#include "analysis/typed_module.h"
#include "src/ast/xl6_ast.h"
/* Takes root ownership only on success. Caller may keep a borrowed alias;
 * it must not mutate/free the root and must hold this module while reading. */
xls_typed_module* xlsTypedModuleTake(XL6_AstNode* root);
const XL6_AstNode* xlsTypedModuleRoot(const xls_typed_module*);
#endif
