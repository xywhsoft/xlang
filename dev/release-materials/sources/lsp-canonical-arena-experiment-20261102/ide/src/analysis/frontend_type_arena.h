#ifndef XLS_FRONTEND_TYPE_ARENA_H
#define XLS_FRONTEND_TYPE_ARENA_H
#include <xrt.h>
#include "src/ast/xl6_ast.h"
/* Private formatter sink, reusing the original canonical visitor and limits.
 * arena must be a valid initialized XRT buffer, disjoint from the immutable
 * AST and its text. Appends one explicit NUL plus XRT's trailing NUL; returned
 * offset/length exclude that terminator. Pointers must be resolved after the
 * last append, because Reserve may relocate storage.
 * On failure Size and its prefix terminator are restored, output scalars are
 * zero; Capacity may have grown. This is a success admission limit, not a
 * bound on transient allocator pools or process memory. */
int xlsFrontendTypeCanonicalAppend(const XL6_AstNode*,const XL6_AstNode*,size_t,size_t,size_t,xstrbuf*,size_t,size_t*,size_t*);
#endif
