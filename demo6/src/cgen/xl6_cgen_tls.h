#ifndef XL6_CGEN_TLS_H
#define XL6_CGEN_TLS_H

#include "src/common/xl6_output.h"

/* Emits shared transport mechanics for XLIR's resident-exclusive Future ABI.
 * Result projections borrow native handles through the supplied functions. */
void xl6CgenEmitTlsRuntime(
    XL6_Output* out,
    int hasTlsStreamRuntime,
    int hasTlsDialRuntime,
    int hasTlsServerRuntime,
    const char* streamResultHandle,
    const char* bytesResultHandle
);

#endif
