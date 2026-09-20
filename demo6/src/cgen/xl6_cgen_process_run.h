#ifndef XL6_CGEN_PROCESS_RUN_H
#define XL6_CGEN_PROCESS_RUN_H

#include "src/common/xl6_output.h"

enum {
    XL6_CGEN_PROCESS_EXEC = 1u,
    XL6_CGEN_PROCESS_SUCCESS = 2u,
    XL6_CGEN_PROCESS_EXIT_CODE = 4u,
    XL6_CGEN_PROCESS_RUN_ALL = 7u
};
/* Synchronous shell capture, independent of live process/callback machinery. */
void xl6CgenEmitProcessRunRuntime(XL6_Output* out, unsigned operations);

#endif
