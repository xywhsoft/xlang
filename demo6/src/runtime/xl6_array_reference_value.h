#ifndef XL6_ARRAY_REFERENCE_VALUE_H
#define XL6_ARRAY_REFERENCE_VALUE_H
#include "src/runtime/xl6_sequence.h"
/* Canonical resident retain policy; never dynamic Value graph cloning. */
XL6_TEXT_API const xvalueconverter* xl6ArrayReferenceConverterV1(void);
#endif
