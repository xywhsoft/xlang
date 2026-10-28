#ifndef XL6_CGEN_HTTP1_H
#define XL6_CGEN_HTTP1_H

#include "src/common/xl6_output.h"

/* HTTP/1 wrappers use the public declaration SDK and an explicit managed
 * string adapter. Component selection is resolved by verified XLIR. */
void xl6CgenEmitHttp1Runtime(XL6_Output* out);

#endif
