#ifndef XL6_CGEN_TEXT_OUTPUT_H
#define XL6_CGEN_TEXT_OUTPUT_H

#include "src/common/xl6_output.h"

enum {
    XL6_CGEN_TEXT_OUTPUT_WRITE = 1u,
    XL6_CGEN_TEXT_OUTPUT_WRITELN = 2u
};

/* Plain C stdio component: no XRT implementation or value-boxing dependency. */
void xl6CgenEmitTextOutputRuntime(XL6_Output* out, unsigned operations);

#endif
