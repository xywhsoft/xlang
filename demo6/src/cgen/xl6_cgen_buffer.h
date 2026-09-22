#ifndef XL6_CGEN_BUFFER_H
#define XL6_CGEN_BUFFER_H

#include "src/common/xl6_output.h"

/* Buffer is a generated-program runtime component selected by verified XLIR
 * bridge identities.  It uses either the shared declaration SDK or the exact
 * narrow public-XRT declaration surface emitted by this component. */
void xl6CgenEmitBufferRuntime(
    XL6_Output* out, int hasFileRuntime, int hasXrtDeclarations);

#endif
