#ifndef XL6_ARRAY_VALUE_H
#define XL6_ARRAY_VALUE_H
#include "src/runtime/xl6_array.h"

/* Explicit snapshots, NOT ordinary language assignment or implicit var
 * conversion. The resulting container is independent; managed elements keep
 * their actual references. The verifier supplies the element's explicit
 * converter (NULL for canonical builtins). A reference-transfer layout uses
 * the resident reference converter and rejects a caller-supplied converter. */
#define XL6_ARRAY_VALUE_LAYOUT uint64_t, uint64_t, unsigned int, size_t, const xrttype*
XL6_TEXT_API xvalue* xl6ArrayFromValueV1(const xvalue*, XL6_ARRAY_VALUE_LAYOUT, const xvalueconverter*);
XL6_TEXT_API xvalue* xl6ArrayToValueV1(const xvalue*, XL6_ARRAY_VALUE_LAYOUT, const xvalueconverter*);
#undef XL6_ARRAY_VALUE_LAYOUT
#endif
