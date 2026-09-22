#define XL6_HTTP_SERVER_SOURCE(...) #__VA_ARGS__

#include "src/cgen/xl6_cgen_http_server_contract.c"
#include "src/cgen/xl6_cgen_http_server_router.c"
#include "src/cgen/xl6_cgen_http_server_request.c"
#include "src/cgen/xl6_cgen_http_server_response.c"
#include "src/cgen/xl6_cgen_http_server_extensions.c"
#include "src/cgen/xl6_cgen_http_server_lifecycle.c"

static void xl6CgenEmitHttpServerRuntime(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts(xl6HttpServerTypesSource, out);
    xl6OutputPuts(xl6HttpServerApiSource, out);
    xl6OutputPuts(xl6HttpServerCoreSource, out);
    xl6OutputPuts(xl6HttpServerRequestSource, out);
    xl6OutputPuts(xl6HttpServerResponseSource, out);
    xl6OutputPuts(xl6HttpServerDispatchSource, out);
    xl6OutputPuts(xl6HttpServerStaticSource, out);
    xl6OutputPuts(xl6HttpServerTlsSource, out);
    xl6OutputPuts(xl6HttpServerPublicSource, out);
    xl6OutputPuts("#endif\n\n", out);
}



#undef XL6_HTTP_SERVER_SOURCE
