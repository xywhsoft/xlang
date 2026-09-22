#ifndef XL6_CGEN_SERIALIZATION_CALLBACK_H
#define XL6_CGEN_SERIALIZATION_CALLBACK_H

#include "src/common/xl6_output.h"

/* Requires the public XRT declarations, callable ABI and xlir_result_c_int.
 * Emit once before JSON/XSON visitor components. */
void xl6CgenEmitSerializationCallbackRuntime(XL6_Output* out);

#endif
