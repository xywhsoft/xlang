#ifndef XL6_SEM_INDEX_H
#define XL6_SEM_INDEX_H

#include "src/ast/xl6_ast.h"
#include "src/compile/xl6_source_report.h"

/* Returns owned schema-1 JSON from the checked AST and its exact report.
 * NULL rejects the whole index, with a bounded error, never partial facts. */
char* xl6SemIndexJson(const XL6_AstNode* root, const XL6_SourceReport* report,
    char* error, size_t errorSize);

/* Exact lowered public C bindings joined by semantic declaration identity to
 * checked AST selections in this same source report. Unavailable targets are
 * explicit null; no source-name matching or fabricated ranges. */
char* xl6SemCBridgeMapJson(const XL6_AstNode* root, const XL6_SourceReport* report,
    const xvalue* bindings, const char* headerText, char* error, size_t errorSize);

#endif
