#ifndef XIDE_REFERENCE_RESULTS_H
#define XIDE_REFERENCE_RESULTS_H
#include "protocol/lsp_wire.h"
#include <xrt.h>
typedef struct xide_reference_item {
 char *path,*label;wchar_t* key;
 xide_lsp_position start,end;
 const xvalue *range,*document;
} xide_reference_item;
typedef struct xide_reference_result {
 uint64_t token;int valid;size_t count;
 xide_reference_item* items;xvalue* message;
} xide_reference_result;
typedef struct xide_reference_decoder xide_reference_decoder;
/* Worker/test API. Takes message,including failures;owns the original DOM.
 * Strict complete Location[]+negotiated source proofs,at most65536 rows and
 *32MiB known typed row/path/label/key storage. No source reads or GUI calls. */
xide_reference_result* xideReferenceDecode(xvalue* message,int (*cancelled)(void*),void* user);
void xideReferenceResultRelease(xide_reference_result* result);
/* Prepared native path key;binary lookup,without allocation or I/O. */
size_t xideReferenceResultFind(const xide_reference_result* result,const wchar_t* key,xide_lsp_position start,xide_lsp_position end);
/* One owner,one XRT worker,one pending message,one completed result and one
 * retired result. Large DOMs/rows are decoded AND disposed on the worker.
 * Wake only notifies the owner and outlives Destroy. */
xide_reference_decoder* xideReferenceDecoderCreate(void (*wake)(void*),void* user);
/* Success TAKES message. Nonzero monotonic token cancels older work;zero
 * retires a message without changing the current query. Failure takes none. */
int xideReferenceDecoderSubmit(xide_reference_decoder* decoder,uint64_t token,xvalue* message);
/* Arm before sending an IDE-owned positive request ID. Receive is installed
 * once on the stdio transport worker;it never reads GUI state. A full slot
 * returns false so the original bounded client queue remains the fallback. */
int xideReferenceDecoderArm(xide_reference_decoder* decoder,uint64_t token,int requestId);
int xideReferenceDecoderReceive(xvalue* message,size_t bytes,void* decoder);
int xideReferenceDecoderHasResult(xide_reference_decoder* decoder);
uint64_t xideReferenceDecoderReceived(xide_reference_decoder* decoder);
/* Success TAKES result for worker disposal;false leaves it with the owner. */
int xideReferenceDecoderRetire(xide_reference_decoder* decoder,xide_reference_result* result);
void xideReferenceDecoderCancel(xide_reference_decoder* decoder);
xide_reference_result* xideReferenceDecoderTake(xide_reference_decoder* decoder);
/* Only final shutdown joins;all queued/published/retired data is drained. */
void xideReferenceDecoderDestroy(xide_reference_decoder* decoder);
#endif
