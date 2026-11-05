#ifndef XL6_CGEN_FILE_H
#define XL6_CGEN_FILE_H

#include "src/common/xl6_output.h"

/* File is a generated-program runtime component selected by verified XLIR
 * bridge identities.  It emits only the declarations absent from the shared
 * public runtime declaration SDK. */
void xl6CgenEmitFileRuntime(
    XL6_Output* out, int hasXrtDeclarations);

#endif
