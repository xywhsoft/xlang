#ifndef XL6_ABI_JSON_H
#define XL6_ABI_JSON_H

#include <stddef.h>

#include "src/common/xl6_output.h"

void xl6AbiJsonEscapedRange(XL6_Output* out, const char* text, size_t len);
void xl6AbiJsonEscaped(XL6_Output* out, const char* text);
void xl6AbiJsonString(XL6_Output* out, const char* text);
void xl6AbiJsonCStringSymbol(XL6_Output* out, const char* text);

#endif
