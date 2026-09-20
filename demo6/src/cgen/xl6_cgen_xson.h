#ifndef XL6_CGEN_XSON_H
#define XL6_CGEN_XSON_H

#include "src/common/xl6_output.h"

enum {
    XL6_CGEN_XSON_PARSE = 1u,
    XL6_CGEN_XSON_VALID = 2u,
    XL6_CGEN_XSON_READ = 4u,
    XL6_CGEN_XSON_STRINGIFY = 8u,
    XL6_CGEN_XSON_WRITE = 16u,
    XL6_CGEN_XSON_VISIT = 32u,
    XL6_CGEN_XSON_WRITER = 64u,
    XL6_CGEN_XSON_ALL = 127u
};
/* Requires the public declaration SDK and, for visitors, the shared
 * serialization callback runtime; never XRT implementation headers. */
void xl6CgenEmitXsonRuntime(XL6_Output* out, unsigned operations, int xlirAbi);

#endif
