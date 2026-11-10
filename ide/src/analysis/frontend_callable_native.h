#ifndef XLS_FRONTEND_CALLABLE_NATIVE_H
#define XLS_FRONTEND_CALLABLE_NATIVE_H
#include "src/ast/xl6_ast.h"
/* Private, borrowed native objects. Defined beside original checker helpers. */
const char* xlsNativeCallableName(const XL6_AstNode*,int global,unsigned* kind);
int xlsNativeCallableRelated(const XL6_AstNode*,const XL6_AstNode*,int global);
#endif
