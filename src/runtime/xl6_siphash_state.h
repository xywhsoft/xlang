#ifndef XL6_SIPHASH_STATE_H
#define XL6_SIPHASH_STATE_H
/* Shared physical state layout, using the pinned XRT declaration ABI. */
typedef struct xlir_siphasher64 { xsiphash state; xsipkey key; } xlir_siphasher64;
#endif
