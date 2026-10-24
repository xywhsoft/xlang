#ifndef XLANG_ANALYSIS_CALL_CONTEXT_H
#define XLANG_ANALYSIS_CALL_CONTEXT_H
#include "analysis/text.h"
#include <stdint.h>
typedef struct xls_call_context xls_call_context;
typedef struct xls_call_site {
    size_t calleeStart,calleeEnd,nameStart,nameEnd;
    size_t argumentsStart,argumentsEnd,argument,namedStart,namedEnd;
    int closed,argumentKnown,inComment,inLiteral,embedded;
} xls_call_site;
typedef struct xls_call_context_work {
    size_t tokens,calls,chargedBytes,constructionBytes,lexedBytes;
    uint64_t genericProofs,genericDeclines;
    int lexicalError;
} xls_call_context_work;
/* Immutable structural call candidates, not name/type/overload resolution.
 * Reuses the shared lexer and its parser's virtual-token type matcher. Raw
 * tokens/scratch and completed storage are bounded together by bytes (64MiB
 * maximum). Retained text is charged; lexer/allocator internals are not RSS
 * bounded. No grammar or lexer copy; unchanged text can share this index.
 * Cancellation is checked between tokens/build steps; one lexer token is not
 * interruptible. Lexical errors retain only the proven prefix of the input. */
xls_call_context* xlsCallContextCreate(xls_text* text,size_t bytes,
    int (*cancelled)(void*),void* user,char* error,size_t capacity);
xls_call_context* xlsCallContextRetain(xls_call_context* context);
void xlsCallContextRelease(xls_call_context* context);
const xls_text* xlsCallContextText(const xls_call_context* context);
xls_call_context_work xlsCallContextWork(const xls_call_context* context);
/* UTF-8 byte offsets exclude optional BOM; scalar/CRLF interiors reject.
 * O(log tokens), no allocation/I/O/mutation. 1=candidate,0=none,-1=invalid.
 * namedStart/End identify an explicit name= argument when present. Unproven
 * generic syntax sets argumentKnown=0; never treat its commas as proven
 * argument numbers. embedded marks opaque interpolation/anonymous bodies needing
 * embedded-language projection. nameStart==nameEnd for expression callees.
 * Views borrow the retained text. Separate readers can query concurrently. */
int xlsCallContextLookup(const xls_call_context* context,size_t offset,xls_call_site* site);
#endif
