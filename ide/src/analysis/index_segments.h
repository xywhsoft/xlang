#ifndef XLS_INDEX_SEGMENTS_H
#define XLS_INDEX_SEGMENTS_H
#include "analysis/index.h"
/* Original preorder partition. A non-NULL identity borrows the immutable final
 * function owned by this index; BOTH source checks must stay retained while
 * comparing identities. Never retain this pointer alone or treat it as an
 * input-freshness proof. No AST,report or mutable producer type escapes. */
typedef struct xls_index_segment {
    size_t first,count,canonicalCharge,proofFirst,proofEnd;
    const void* identity;
    unsigned source;
    int reusable;
} xls_index_segment;
size_t xlsSemanticIndexSegmentCount(const xls_semantic_index*);
int xlsSemanticIndexSegment(const xls_semantic_index*,size_t slot,xls_index_segment*);
#endif
