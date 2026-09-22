/* bbre_xrt.h - execution context extension used by XRT */

#ifndef XRT_BBRE_XRT_H
#define XRT_BBRE_XRT_H

#include "bbre.h"

/*
 * BBRE stores its execution cache in bbre and bbre_set by default. XRT keeps
 * compiled expressions immutable and moves that mutable state into a matcher.
 * These functions expose only the execution context required for that split.
 */
typedef struct bbre_exec bbre_xrt_context;

int bbre_xrt_context_init_regex(
    bbre_xrt_context **pcontext, const bbre *reg, const bbre_alloc *alloc);
int bbre_xrt_context_init_set(
    bbre_xrt_context **pcontext, const bbre_set *set, const bbre_alloc *alloc);
void bbre_xrt_context_destroy(bbre_xrt_context *context);

int bbre_xrt_context_match(
    bbre_xrt_context *context, const char *text, size_t text_size, size_t pos,
    bbre_span *out_captures, unsigned int *out_captures_did_match,
    unsigned int out_captures_size);
int bbre_xrt_context_full_match(
    bbre_xrt_context *context, const char *text, size_t text_size,
    bbre_span *out_captures, unsigned int *out_captures_did_match,
    unsigned int out_captures_size);
int bbre_xrt_context_set_match(
    bbre_xrt_context *context, const char *text, size_t text_size, size_t pos,
    unsigned int *out_idxs, unsigned int out_idxs_size,
    unsigned int *out_num_idxs);

#endif /* XRT_BBRE_XRT_H */
