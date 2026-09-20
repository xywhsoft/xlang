#ifndef XL6_CGEN_HTTP1_H
#define XL6_CGEN_HTTP1_H

#include "src/common/xl6_output.h"

/* HTTP/1 protocol wrappers are generated-program runtime components shared
 * by both C backends.  XLIR uses the public declaration SDK and a local
 * managed-string compatibility adapter; component selection is resolved by
 * verified IR before this emitter is called. */
void xl6CgenEmitHttp1Runtime(XL6_Output* out, int xlirAbi);
void xl6CgenEmitHttpDecodeRuntime(XL6_Output* out);

#endif
