#ifndef XL6_CGEN_XLANG_H
#define XL6_CGEN_XLANG_H

#include "src/common/xl6_output.h"

/* Compiler-embedding component shared by both C backends.  Selection belongs
 * to each backend's verified feature model; the emitted implementation stays
 * single-sourced here. */
void xl6EmitXlangRuntimeComponent(XL6_Output* out, int xlirAbi);

#endif
