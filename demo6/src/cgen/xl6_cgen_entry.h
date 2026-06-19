#ifndef XL6_CGEN_ENTRY_H
#define XL6_CGEN_ENTRY_H

#include "src/ast/xl6_ast.h"

int xl6CgenIsTopLevelRuntimeStmt(const XL6_AstNode* node);
int xl6CgenFuncIsMainEntry(const XL6_AstNode* node);
const XL6_AstNode* xl6CgenFindMainEntry(const XL6_AstNode* root);
int xl6CgenHasTopLevelRuntimeStmt(const XL6_AstNode* root);

#endif
