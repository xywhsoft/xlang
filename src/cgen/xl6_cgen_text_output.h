#ifndef XL6_CGEN_TEXT_OUTPUT_H
#define XL6_CGEN_TEXT_OUTPUT_H

#include "src/common/xl6_output.h"

enum {
    XL6_CGEN_TEXT_OUTPUT_WRITE = 1u,
    XL6_CGEN_TEXT_OUTPUT_WRITELN = 2u,
    XL6_CGEN_TEXT_OUTPUT_ERROR = 4u,
    XL6_CGEN_TEXT_OUTPUT_ERRORLN = 8u,
    XL6_CGEN_TEXT_OUTPUT_FLUSH = 16u
};

/* Selected XRT console adapters: no value-boxing or forced flushing. */
void xl6CgenEmitTextOutputRuntime(XL6_Output* out, unsigned operations);

#endif
