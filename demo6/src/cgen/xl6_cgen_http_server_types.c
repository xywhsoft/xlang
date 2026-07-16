static void xl6CgenEmitHttpServerRuntimeTypes(XL6_Output* out)
{
    xl6OutputPuts("#ifndef XL6_HTTP_SERVER_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_HTTP_SERVER_RUNTIME\n", out);
    xl6OutputPuts("#include <stdlib.h>\n", out);
    xl6OutputPuts("#include <stdint.h>\n", out);
    xl6OutputPuts("#include <string.h>\n\n", out);

    xl6OutputPuts("typedef struct xrt_net_engine xnetengine;\n", out);
    xl6OutputPuts("typedef struct xrt_web_server xwebserver;\n", out);
    xl6OutputPuts("typedef struct xrt_web_app xwebapp;\n", out);
    xl6OutputPuts("typedef struct xrt_web_request xwebrequest;\n", out);
    xl6OutputPuts("typedef struct xrt_web_response xwebresponse;\n", out);
    xl6OutputPuts("typedef struct xrt_net_future xfuture;\n", out);
}
