#ifndef XL6_CGEN_RUNTIME_H
#define XL6_CGEN_RUNTIME_H

#include "src/common/xl6_output.h"

void xl6CgenEmitApproxProtos(XL6_Output* out);
void xl6CgenEmitTempScopeRuntime(XL6_Output* out);
void xl6CgenEmitTimeRuntime(XL6_Output* out);
void xl6CgenEmitStringRuntime(XL6_Output* out);
void xl6CgenEmitClassHandleRuntime(XL6_Output* out);
void xl6CgenEmitIntPowHelper(XL6_Output* out);
void xl6CgenEmitRangeContainsHelper(XL6_Output* out);

#endif
