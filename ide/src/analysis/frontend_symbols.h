#ifndef XLANG_ANALYSIS_FRONTEND_SYMBOLS_H
#define XLANG_ANALYSIS_FRONTEND_SYMBOLS_H
#include "src/ast/xl6_ast.h"
#include "src/type/xl6_type_id.h"
/* Private function-local identity. Original compiler IDs stay unchanged.
 * Slots name the first lexical declaration occurrence,not a type/global ID.
 * Tables borrow no AST after destruction and own only bounded XRT vectors.
 * Zero-initialize before first Build/Clear; rebuild clears the old vectors. */
#define XLS_LEXICAL_SYMBOL_BYTES (1u*1024u*1024u)
typedef struct xls_lexical_symbol {XL6_SymbolId symbol;uint32 slot;} xls_lexical_symbol;
typedef struct xls_lexical_symbols {
    const XL6_AstNode* root;
    xarray order,sorted;
    size_t bytes;
    int ready;
} xls_lexical_symbols;
int xlsLexicalSymbolsBuild(xls_lexical_symbols* symbols,const XL6_AstNode* root,size_t bytes);
void xlsLexicalSymbolsClear(xls_lexical_symbols* symbols);
XL6_SymbolId xlsLexicalSymbolKey(const xls_lexical_symbols* symbols,XL6_SymbolId symbol,int* local);
int xlsLexicalSymbolResolve(const xls_lexical_symbols* symbols,XL6_SymbolId key,int local,XL6_SymbolId* symbol);
#endif
