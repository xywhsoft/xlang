#ifndef XL6_CGEN_TEXT_CODEC_H
#define XL6_CGEN_TEXT_CODEC_H

#include "src/common/xl6_output.h"

/* A backend-neutral component with borrowed text/size/alphabet inputs and an
 * owned raw text result. No AST, symbol lookup, or IR inference is permitted. */
void xl6CgenEmitBase64TextCodecRuntime(XL6_Output* out, int hasXrtDeclarations);
/* Binary input length is exact (including zero). Decode returns an owned
 * Buffer handle rather than a C string, preserving embedded NUL bytes. */
void xl6CgenEmitHexBytesCodecRuntime(XL6_Output* out, int hasXrtDeclarations);
void xl6CgenEmitBase64BytesCodecRuntime(XL6_Output* out, int hasXrtDeclarations);

#endif
