#ifndef XL6_CGEN_BUFFER_H
#define XL6_CGEN_BUFFER_H

#include "src/common/xl6_output.h"

/* Buffer is a generated-program runtime component shared by both C backends.
 * xlirAbi selects the narrow declaration/error surface used by XLIR output;
 * the implementation body and public XRT behavior remain single-sourced. */
void xl6CgenEmitBufferRuntime(
    XL6_Output* out, int hasFileRuntime, int xlirAbi,
    int hasXrtDeclarations);

#endif
