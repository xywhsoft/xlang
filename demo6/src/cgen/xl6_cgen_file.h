#ifndef XL6_CGEN_FILE_H
#define XL6_CGEN_FILE_H

#include "src/common/xl6_output.h"

/* File is a generated-program runtime component shared by both C backends.
 * xlirAbi selects narrow public-XRT declarations and XLIR error propagation;
 * the implementation body remains single-sourced. */
void xl6CgenEmitFileRuntime(
    XL6_Output* out, int xlirAbi, int hasXrtDeclarations);

#endif
