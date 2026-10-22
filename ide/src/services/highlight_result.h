#ifndef XIDE_HIGHLIGHT_RESULT_H
#define XIDE_HIGHLIGHT_RESULT_H
#include "analysis/text.h"
#include <stdint.h>
#include <xrt.h>

typedef struct xide_highlight_range { int start,end,kind; } xide_highlight_range;
typedef struct xide_highlight_result {
    uint64_t token;
    int valid;
    size_t count;
    xide_highlight_range* ranges;
} xide_highlight_result;
typedef struct xide_highlight_decoder xide_highlight_decoder;
/* Owned sorted, deduplicated ranges, or valid==0 with no ranges. null/[] is a
 * valid empty answer. Strict UTF-16 endpoints; at most 65536 input ranges.
 * Synchronous decoding is for the worker/tests, never the UI response handler. */
xide_highlight_result* xideHighlightDecode(const xls_text* text,const xvalue* result,
                                         int (*cancelled)(void*),void* user);
void xideHighlightResultRelease(xide_highlight_result* result);
/* XRT worker: one running job plus one queued job and one completed answer.
 * Wake only notifies the owner; it must not touch UI and outlives Destroy.
 * All calls except the worker callback are made by a single owner. */
xide_highlight_decoder* xideHighlightDecoderCreate(void (*wake)(void*),void* user);
/* Successful submission TAKES both owned message and text, without copying.
 * message is a complete JSON-RPC response; result is borrowed by the worker.
 * Failure takes neither input. Nonzero tokens must be unique in this decoder.
 * A full pending slot rejects submission instead of growing a queue. */
int xideHighlightDecoderSubmit(xide_highlight_decoder* decoder,uint64_t token,
                               xvalue* message,xls_text* text);
/* Cancel does not join or release pending JSON on the calling/UI thread. */
void xideHighlightDecoderCancel(xide_highlight_decoder* decoder);
xide_highlight_result* xideHighlightDecoderTake(xide_highlight_decoder* decoder);
/* Only final shutdown joins the worker. */
void xideHighlightDecoderDestroy(xide_highlight_decoder* decoder);
#endif
