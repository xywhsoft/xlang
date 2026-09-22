#define XL6_WS_SOURCE(...) #__VA_ARGS__

#include "src/cgen/xl6_cgen_ws_contract.c"
#include "src/cgen/xl6_cgen_ws_core.c"
#include "src/cgen/xl6_cgen_ws_options.c"
#include "src/cgen/xl6_cgen_ws_connection.c"
#include "src/cgen/xl6_cgen_ws_events.c"
#include "src/cgen/xl6_cgen_ws_client.c"
#include "src/cgen/xl6_cgen_ws_server.c"
#include "src/cgen/xl6_cgen_ws_http_bridge.c"

static void xl6CgenEmitWebSocketRuntime(XL6_Output* out)
{
    if ( out == NULL ) { return; }
    xl6OutputPuts(xl6WebSocketTypesSource, out);
    xl6OutputPuts(xl6WebSocketApiSource, out);
    xl6OutputPuts(xl6WebSocketCoreSource, out);
    xl6OutputPuts(xl6WebSocketOptionsSource, out);
    xl6OutputPuts(xl6WebSocketConnectionSource, out);
    xl6OutputPuts(xl6WebSocketEventsSource, out);
    xl6OutputPuts(xl6WebSocketClientSource, out);
    xl6OutputPuts(xl6WebSocketServerSource, out);
    xl6OutputPuts("#endif\n\n", out);
}



static void xl6CgenEmitWebSocketHttpBridge(XL6_Output* out)
{
    if ( out != NULL ) { xl6OutputPuts(xl6WebSocketHttpBridgeSource, out); }
}



#undef XL6_WS_SOURCE
