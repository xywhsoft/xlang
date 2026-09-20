#ifndef XL6_CGEN_TLS_H
#define XL6_CGEN_TLS_H

#include "src/common/xl6_output.h"

/* Emits shared transport mechanics. Result projections borrow the native
 * handle from each backend's exact Future payload representation. */
typedef enum XL6_CgenTlsFutureAbi {
    XL6_CGEN_TLS_FUTURE_LEGACY = 0,
    XL6_CGEN_TLS_FUTURE_RESIDENT_EXCLUSIVE_V1
} XL6_CgenTlsFutureAbi;
void xl6CgenEmitTlsRuntime(
    XL6_Output* out,
    int hasTlsStreamRuntime,
    int hasTlsDialRuntime,
    int hasTlsServerRuntime,
    const char* streamResultHandle,
    const char* bytesResultHandle,
    XL6_CgenTlsFutureAbi futureAbi
);

#endif
