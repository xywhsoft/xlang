#ifndef XL6_CGEN_BUFFER_H
#define XL6_CGEN_BUFFER_H

#include "src/common/xl6_output.h"

/* Legacy file/network conversion helpers only. Public Buffer operations are
 * sealed resident components selected through the direct ABI registry; this
 * emitter must not implement their factories, byte operations or queries. */
void xl6CgenEmitBufferRuntime(
    XL6_Output* out, int hasFileRuntime, int hasXrtDeclarations);

#endif
