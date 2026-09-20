#ifndef XL6_CGEN_JSON_STREAM_H
#define XL6_CGEN_JSON_STREAM_H

#include "src/common/xl6_output.h"

/* The caller supplies the public XRT declaration SDK and, for visitors, the
 * shared serialization callback runtime. No implementation header or legacy
 * Cgen needed. */
void xl6CgenEmitJsonStreamRuntime(
    XL6_Output* out, int hasVisitor, int hasWriter, int xlirAbi);

#endif
