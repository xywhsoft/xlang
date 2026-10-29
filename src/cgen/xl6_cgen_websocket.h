#ifndef XL6_CGEN_WEBSOCKET_H
#define XL6_CGEN_WEBSOCKET_H

#include "src/common/xl6_output.h"

/* The caller supplies an XLIR-verified WebSocket component closure; this
 * emitter never discovers features from source names or generated C text. */
void xl6CgenEmitWebSocketProtocolRuntime(
    XL6_Output* out,
    int hasFrame,
    int hasHandshake,
    int hasUpgrade,
    int hasStreamConfig,
    int hasStream,
    int hasStreamTls);

#endif
