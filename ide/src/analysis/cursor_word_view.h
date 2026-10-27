#ifndef XLANG_ANALYSIS_CURSOR_WORD_VIEW_H
#define XLANG_ANALYSIS_CURSOR_WORD_VIEW_H
#include "analysis/session.h"
#include "src/sem/xl6_completion.h"
/* Original-parser proof, never a lexer/grammar of its own. Both immutable
 * snapshots must be successful ordinary completion projections. Every logical
 * AST value is compared in current placement coordinates, including type
 * forests. Only a single complete query token and its induced coordinates may
 * differ; source text outside it is exact. No AST is materialized or retained.
 * Output prefix bytes borrow current syntax. Zero declines to the full query.
 * At most262144 nodes,depth512; cancellation every64 nodes. No allocation/I/O. */
typedef struct xls_cursor_word_view {
    XL6_AstRange before,after;
    const char* prefix;size_t prefixSize,offset;
    uint64_t nodes;
} xls_cursor_word_view;
int xlsSyntaxCursorWordProof(const xls_syntax* previous,const xls_syntax* current,
    xls_cursor_word_view*,int (*cancelled)(void*),void* user);
/* Explicit admission measurement, including an already materialized read-only
 * syntax view. No published syntax/chunk accounting fields are modified. */
int xlsSyntaxCursorRetainedBytes(const xls_syntax*,size_t limit,size_t* bytes);
/* Original query-token coordinate mapping. Reject an edge through the token's
 * interior; selection facts overlapping the token must use fresh analysis. */
int xlsCursorWordMapRange(const xls_cursor_word_view*,const XL6_AstRange*,XL6_AstRange*);
#endif
