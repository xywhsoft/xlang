#ifndef XL6_CGEN_DIR_H
#define XL6_CGEN_DIR_H

#include "src/common/xl6_output.h"

enum {
    XL6_CGEN_DIR_LIST = 1u,
    XL6_CGEN_DIR_GLOB = 2u,
    XL6_CGEN_DIR_ENTRIES = 4u,
    XL6_CGEN_DIR_IS_EMPTY = 8u,
    XL6_CGEN_DIR_COUNT = 16u,
    XL6_CGEN_DIR_SIZE = 32u,
    XL6_CGEN_DIR_ALL = 63u
};
void xl6CgenEmitDirRuntime(XL6_Output* out, unsigned operations);

#endif
