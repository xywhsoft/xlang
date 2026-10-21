#ifndef XLANG_ANALYSIS_INDEX_H
#define XLANG_ANALYSIS_INDEX_H
#include <stddef.h>
#include <stdint.h>

/* Read-only checked-compilation facts. No frontend AST, transport, JSON or
 * GUI runtime types escape this boundary. All pointers borrow the owning
 * semantic check; obtain a fresh view after validating that check. */
typedef struct xls_semantic_index xls_semantic_index;
typedef struct xls_index_range {
    size_t startOffset,endOffset;
    int startLine,startColumn,endLine,endColumn,known;
} xls_index_range;
typedef struct xls_index_fact {
    const char *kind,*canonical;
    unsigned id,parent,source,selectionSource;
    unsigned declaredSymbol,symbol,targetSymbol,compoundOperatorSymbol;
    unsigned declaredType,referencedType,typeId,typeObjectId;
    uint64_t typeAbi,typeObjectAbi;
    /* 0=no type object, 1=declaration, 2=inferred, 3=syntax. */
    int typeOrigin;
    xls_index_range range,tokenRange,selectionRange;
} xls_index_fact;
size_t xlsSemanticIndexCount(const xls_semantic_index* index);
int xlsSemanticIndexFact(const xls_semantic_index* index,size_t at,xls_index_fact* fact);
#endif
