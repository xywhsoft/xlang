#ifndef XLANG_ANALYSIS_FRONTEND_INTERNAL_H
#define XLANG_ANALYSIS_FRONTEND_INTERNAL_H
#include "analysis/session.h"
#include "src/parser/xl6_parser_api.h"
/* Compiler facts remain inside the analysis layer. Never mutate a published
 * parse tree; semantic passes require their own owned representation. */
const XL6_AstNode* xlsSyntaxAst(const xls_syntax* syntax);
#endif
