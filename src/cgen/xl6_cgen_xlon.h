#ifndef XL6_CGEN_XLON_H
#define XL6_CGEN_XLON_H

#include "src/common/xl6_output.h"

enum {
    XL6_CGEN_XLON_PARSE = 1u,
    XL6_CGEN_XLON_VALID = 2u,
    XL6_CGEN_XLON_READ = 4u,
    XL6_CGEN_XLON_STRINGIFY = 8u,
    XL6_CGEN_XLON_WRITE = 16u,
    XL6_CGEN_XLON_VISIT = 32u,
    XL6_CGEN_XLON_WRITER = 64u,
    XL6_CGEN_XLON_ALL = 127u
};
/* Requires the public declaration SDK and, for visitors, the shared
 * serialization callback runtime; never XRT implementation headers. */
void xl6CgenEmitXlonRuntime(XL6_Output* out, unsigned operations);

#endif
