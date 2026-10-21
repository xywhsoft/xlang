#ifndef XIDE_SEMANTIC_DEFINITION_H
#define XIDE_SEMANTIC_DEFINITION_H
#include "services/toolchain.h"
#include "protocol/lsp_wire.h"
typedef struct xide_semantic_definitions xide_semantic_definitions;
/* Verified original UTF-8 after BOM, before newline normalization. Borrowed
   only during decoding; utf16Prefix accounts for an overlay BOM on line 0.
   Returned table owns paths and converted positions. */
typedef struct xide_semantic_source { const char *path,*text; int kind,utf16Prefix; } xide_semantic_source;
/* Batch conversion validates exact byte endpoints and compiler scalar positions
   in one sorted pass. Borrowed ranges; output positions use UTF-16/LF lines. */
typedef struct xide_semantic_range_query {
 int sourceId; const xvalue* range; xide_lsp_position start,end;
} xide_semantic_range_query;
int xideSemanticRangesToLsp(const xide_semantic_source* sources,size_t count,xide_semantic_range_query* ranges,size_t rangeCount);
xide_semantic_definitions* xideSemanticDefinitionsCreate(const xvalue* index,const xide_semantic_source* sources,size_t count,xide_model_error* error);
#ifdef XLS_COMPILER_XRT
#include "analysis/index.h"
/* Borrows the checked native facts only during construction. The returned
 * navigation table owns all paths, text and UTF-16 ranges independently. */
xide_semantic_definitions* xideSemanticDefinitionsCreateTyped(const xls_semantic_index* index,const xide_semantic_source* sources,size_t count,xide_model_error* error);
#endif
void xideSemanticDefinitionsDestroy(xide_semantic_definitions* definitions);
/* Returns a Location or JSON null. Explicit type facts can resolve a unique
   same-compilation declaration, including bases, constraints and generic
   parameters without a runtime ABI; value references keep SymbolId.
   Never searches by spelling or reads disk. */
xvalue* xideSemanticDefinition(xide_semantic_definitions* definitions,const char* path,int line,int character);
xvalue* xideSemanticHover(xide_semantic_definitions* definitions,const char* path,int line,int character);
xvalue* xideSemanticDocumentSymbols(xide_semantic_definitions* definitions,const char* path);
#endif
