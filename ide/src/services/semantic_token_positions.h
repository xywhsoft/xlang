#ifndef XIDE_SEMANTIC_TOKEN_POSITIONS_H
#define XIDE_SEMANTIC_TOKEN_POSITIONS_H
#include "analysis/semantic_tokens.h"
typedef struct xide_token_positions xide_token_positions;
typedef struct xide_token_position {int line,character,length;unsigned kind,modifiers;} xide_token_position;
typedef struct xide_token_position_work {
 xls_semantic_tokens_work classification;size_t bytes,peakBytes,scannedBytes;
} xide_token_position_work;
/* Worker-only original checked text -> verified UTF16 positions. No per-token
 * line rescans: reuse the complete ordered coordinate converter. prefix is the
 * captured editor overlay's BOM width (0 or1), not the disk BOM byte count.
 * All known producer/temporary array storage fits budget (0=16MiB). Returned
 * immutable storage owns no check/text. The host still proves publication. */
xide_token_positions* xideTokenPositionsCreate(const xls_semantic_check*,size_t source,
 int prefix,size_t budget,int (*cancelled)(void*),void*,enum xls_semantic_tokens_error*);
xide_token_positions* xideTokenPositionsRetain(xide_token_positions*);
void xideTokenPositionsRelease(xide_token_positions*);
size_t xideTokenPositionsCount(const xide_token_positions*);
const xide_token_position* xideTokenPositionsData(const xide_token_positions*);
xide_token_position_work xideTokenPositionsWork(const xide_token_positions*);
#endif
