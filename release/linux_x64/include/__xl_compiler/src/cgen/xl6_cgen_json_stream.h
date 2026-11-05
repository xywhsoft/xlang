#ifndef XL6_CGEN_JSON_STREAM_H
#define XL6_CGEN_JSON_STREAM_H

#include "src/common/xl6_output.h"

/* The caller supplies the public XRT declaration SDK and, for visitors, the
 * shared serialization callback runtime. */
void xl6CgenEmitJsonStreamRuntime(
    XL6_Output* out, int hasVisitor, int hasWriter);

#endif
