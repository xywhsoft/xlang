#ifndef XIDE_SIGNATURE_RESULT_H
#define XIDE_SIGNATURE_RESULT_H
#include "analysis/text.h"
#include <xrt.h>
typedef struct xide_signature_expected { int request,version,offset; } xide_signature_expected;
typedef struct xide_signature_parameter { size_t start,end; const char* documentation; } xide_signature_parameter;
typedef struct xide_signature_item {
 const char *label,*documentation; size_t count,activeParameter;
 xide_signature_parameter* parameters;
} xide_signature_item;
typedef struct xide_signature_result {
 uint64_t token; int valid,available; size_t count,activeSignature;
 xide_signature_item* items; xvalue* message;
} xide_signature_result;
typedef struct xide_signature_decoder xide_signature_decoder;
/* Immutable, bounded result backed by the owned response. Parameter spans are
 * UTF-8 byte boundaries derived from standard UTF-16 tuples or string labels.
 * Parsing and large response cleanup belong to a worker, never the UI. */
xide_signature_result* xideSignatureDecode(const xls_text*,const xvalue*,xide_signature_expected,
 int (*cancelled)(void*),void* user);
void xideSignatureResultRelease(xide_signature_result*);
xide_signature_decoder* xideSignatureDecoderCreate(void (*wake)(void*),void* user);
/* One running/reserved pending/completed/retired slot. Expect takes the exact
 * sent source only on success. Receiver takes only the prepared request ID;
 * oversized matching replies are rejected and destroyed on this same worker. */
int xideSignatureDecoderExpect(xide_signature_decoder*,uint64_t,xls_text*,xide_signature_expected);
int xideSignatureDecoderReceiveOwned(xvalue*,size_t,void*);
int xideSignatureDecoderRetire(xide_signature_decoder*,xide_signature_result*);
void xideSignatureDecoderCancel(xide_signature_decoder*);
xide_signature_result* xideSignatureDecoderTake(xide_signature_decoder*);
void xideSignatureDecoderDestroy(xide_signature_decoder*);
#endif
