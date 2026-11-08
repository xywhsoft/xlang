#ifndef XIDE_SEMANTIC_SYMBOLS_H
#define XIDE_SEMANTIC_SYMBOLS_H
#include "services/semantic_definition.h"
#include "analysis/declaration.h"
typedef struct xide_semantic_symbols xide_semantic_symbols;
typedef struct xide_symbol_work {
 size_t facts,declarations,ownedBytes,constructionAllowanceBytes,borrowedValidationBytes;
 size_t coordinateEndpoints,coordinateScratchAllowanceBytes;int denseCoordinates;
 int reusedValidation,fullValidationFallback;
} xide_symbol_work;
typedef struct xide_semantic_symbol_view {
 const char *path,*name,*detail;
 int kind;size_t parent;
 xide_lsp_position start,end,selectionStart,selectionEnd;
} xide_semantic_symbol_view;
/* Independent owned declaration projection. No AST/check, source-text, GUI,
 * JSON-index or navigation-table pointer survives construction. */
void xideSemanticSymbolsDestroy(xide_semantic_symbols* symbols);
size_t xideSemanticSymbolsBytes(const xide_semantic_symbols* symbols);
size_t xideSemanticSymbolsCount(const xide_semantic_symbols* symbols);
int xideSemanticSymbolAt(const xide_semantic_symbols* symbols,size_t at,xide_semantic_symbol_view* out);
xvalue* xideSemanticSymbolsDocument(xide_semantic_symbols* symbols,const char* path,const char* uri);
xide_semantic_symbols* xideSemanticSymbolsCreate(const xvalue* index,const xide_semantic_source* sources,size_t count,
 int (*stopped)(void*),void* user,xide_symbol_work* work,xide_model_error* error);
#ifdef XLS_COMPILER_XRT
/* Prefer the exact existing validated immutable point view. If its optional
 * preparation was declined, validate through the original typed constructor.
 * Freshness/publication guards remain the caller's responsibility. */
xide_semantic_symbols* xideSemanticSymbolsFromNavigation(const xide_navigation_snapshot* snapshot,
 int (*stopped)(void*),void* user,xide_symbol_work* work,xide_model_error* error);
xide_semantic_symbols* xideSemanticSymbolsCreateTyped(const xls_semantic_index* index,const xide_semantic_source* sources,size_t count,
 int (*stopped)(void*),void* user,xide_symbol_work* work,xide_model_error* error);
#endif
#endif
