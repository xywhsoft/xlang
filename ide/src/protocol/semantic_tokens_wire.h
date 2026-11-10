#ifndef XIDE_SEMANTIC_TOKENS_WIRE_H
#define XIDE_SEMANTIC_TOKENS_WIRE_H
#include "services/semantic_token_positions.h"
#include <xrt.h>
typedef struct xide_token_legend {
 int enabled,full,range,delta,typeCount,modifierCount;
 int types[XLS_TOKEN_KIND_COUNT];unsigned typeKinds[XLS_TOKEN_KIND_COUNT],modifierKinds[3],modifierBits[3];
} xide_token_legend;
typedef struct xide_token_range {int enabled,startLine,startCharacter,endLine,endCharacter;} xide_token_range;
typedef struct xide_token_data xide_token_data;
/* Validate supported standard capability shapes. Absent/unsupported relative
 * format or augmentation mode disables the provider without invalidating init.
 * Types/modifiers are an ordered subset of supported native roles. */
int xideTokenLegendParse(const xvalue* capabilities,xide_token_legend*);
xvalue* xideTokenLegendProvider(const xide_token_legend*);
xide_token_data* xideTokenDataEncode(const xide_token_positions*,const xide_token_legend*,
 const xide_token_range*,const char* resultId,int (*cancelled)(void*),void*);
xide_token_data* xideTokenDataRetain(xide_token_data*);
void xideTokenDataRelease(xide_token_data*);
size_t xideTokenDataCount(const xide_token_data*);
const uint32* xideTokenDataWords(const xide_token_data*);
const char* xideTokenDataId(const xide_token_data*);
size_t xideTokenDataBytes(const xide_token_data*);
/* Worker-only: full or one numeric-array edit; unknown/inefficient bases use
 * full data. IDs and primitive history are storage, never freshness proofs.
 * Maximal common prefix/suffix scanning and JSON preparation are cancellable.
 * Conservative integer text bounds leave1MiB of the wire body for its envelope. */
xvalue* xideTokenWireResult(const xide_token_data*,const xide_token_data* previous,
 int delta,int (*cancelled)(void*),void*);
#endif
