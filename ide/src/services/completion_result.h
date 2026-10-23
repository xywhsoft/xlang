#ifndef XIDE_COMPLETION_RESULT_H
#define XIDE_COMPLETION_RESULT_H
#include "analysis/text.h"
#include <stdint.h>
#include <xrt.h>

typedef struct xide_completion_expected { int request,version,offset; } xide_completion_expected;
typedef struct xide_completion_item {
    const char *label,*detail,*sort;
    uint32_t symbol;
} xide_completion_item;
typedef struct xide_completion_result {
    uint64_t token;
    int valid,available,start,end;
    size_t count;
    xide_completion_item* items;
    const char** names;
    /* Read-only payload backing item strings; UI borrows, never mutates it. */
    xvalue* message;
} xide_completion_result;
typedef struct xide_completion_decoder xide_completion_decoder;
/* Worker/test synchronous parser. Borrows source/message, retains the message
 * only for a completely valid available result. No GUI state or body copy.
 * Up to 4096 candidates; strict UTF-16 and whole-token replacement validation. */
xide_completion_result* xideCompletionDecode(const xls_text* text,const xvalue* message,
    xide_completion_expected expected,int (*cancelled)(void*),void* user);
/* Heavy cleanup: worker/tests/final shutdown only, never a UI dismissal. */
void xideCompletionResultRelease(xide_completion_result* result);
/* One owner and one XRT worker, one running + one pending + one completed job,
 * and one separate retired-result slot. Wake only notifies and outlives Destroy. */
xide_completion_decoder* xideCompletionDecoderCreate(void (*wake)(void*),void* user);
/* Success TAKES owned message and exact synced source lease; false takes none.
 * Nonzero tokens are unique for the decoder lifetime. No source/JSON copying. */
int xideCompletionDecoderSubmit(xide_completion_decoder* decoder,uint64_t token,
    xvalue* message,xls_text* text,xide_completion_expected expected);
/* Shares the pending slot, changes no active token/result; success TAKES. */
int xideCompletionDecoderDiscard(xide_completion_decoder* decoder,xvalue* message);
/* Separate bounded retirement slot. Success TAKES; false keeps caller ownership. */
int xideCompletionDecoderRetire(xide_completion_decoder* decoder,xide_completion_result* result);
/* Cancellation only invalidates publication; no UI-thread JSON release/join. */
void xideCompletionDecoderCancel(xide_completion_decoder* decoder);
xide_completion_result* xideCompletionDecoderTake(xide_completion_decoder* decoder);
void xideCompletionDecoderDestroy(xide_completion_decoder* decoder);
#endif
