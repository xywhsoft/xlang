static void xl6CgenEmitHttpServerRuntime(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_HTTP_SERVER_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_HTTP_SERVER_RUNTIME\n", out);
    xl6OutputPuts("#include <stdlib.h>\n", out);
    xl6OutputPuts("#include <string.h>\n\n", out);

    xl6OutputPuts("typedef struct xrt_net_engine xnetengine;\n", out);
    xl6OutputPuts("typedef struct xrt_tls_config xtlsconfig;\n", out);
    xl6OutputPuts("typedef struct xrt_web_server xwebserver;\n", out);
    xl6OutputPuts("typedef struct xrt_web_request xwebrequest;\n", out);
    xl6OutputPuts("typedef struct xrt_web_response xwebresponse;\n", out);
    xl6OutputPuts("typedef int xnet_result;\n", out);
    xl6OutputPuts("typedef int xwebaction;\n", out);
    xl6OutputPuts("typedef xwebaction (*xwebhandler)(xwebrequest* pReq, xwebresponse* pResp, void* pUserData);\n\n", out);

    xl6OutputPuts("typedef struct {\n", out);
    xl6OutputPuts("    uint16_t iFamily;\n", out);
    xl6OutputPuts("    uint16_t iPort;\n", out);
    xl6OutputPuts("    uint32_t iScopeId;\n", out);
    xl6OutputPuts("    uint8_t aAddr[16];\n", out);
    xl6OutputPuts("} xnetaddr;\n\n", out);

    xl6OutputPuts("typedef struct {\n", out);
    xl6OutputPuts("    xnetaddr tBindAddr;\n", out);
    xl6OutputPuts("    uint32_t iFlags;\n", out);
    xl6OutputPuts("    uint32_t iBacklog;\n", out);
    xl6OutputPuts("    uint32_t iRecvLimit;\n", out);
    xl6OutputPuts("    uint32_t iBodyLimit;\n", out);
    xl6OutputPuts("    uint32_t iWorkerCount;\n", out);
    xl6OutputPuts("    const xtlsconfig* pTlsConfig;\n", out);
    xl6OutputPuts("} xwebconfig;\n\n", out);

    xl6OutputPuts("typedef struct {\n", out);
    xl6OutputPuts("    uint32_t iFlags;\n", out);
    xl6OutputPuts("    const char* sIndexNames;\n", out);
    xl6OutputPuts("    const char* sCacheControl;\n", out);
    xl6OutputPuts("    size_t iChunkSize;\n", out);
    xl6OutputPuts("} xwebstaticconfig;\n\n", out);

    xl6OutputPuts("#define XL6_XWEB_DONE 1\n", out);
    xl6OutputPuts("#define XL6_XWEB_ERROR -1\n", out);
    xl6OutputPuts("#define XL6_XWEB_METHOD_GET (1u << 1)\n", out);
    xl6OutputPuts("#define XL6_XWEB_METHOD_HEAD (1u << 2)\n", out);
    xl6OutputPuts("#define XL6_XWEB_METHOD_POST (1u << 3)\n", out);
    xl6OutputPuts("#define XL6_XWEB_METHOD_ANY 0xffffffffu\n\n", out);

    xl6OutputPuts("typedef struct XL6_Http_Route {\n", out);
    xl6OutputPuts("    XL6_Function handler;\n", out);
    xl6OutputPuts("    int objectHandler;\n", out);
    xl6OutputPuts("    struct XL6_Http_Route* next;\n", out);
    xl6OutputPuts("} XL6_Http_Route;\n\n", out);

    xl6OutputPuts("typedef struct XL6_Http_Server {\n", out);
    xl6OutputPuts("    xwebserver* server;\n", out);
    xl6OutputPuts("    XL6_Http_Route* routes;\n", out);
    xl6OutputPuts("    int running;\n", out);
    xl6OutputPuts("} XL6_Http_Server;\n\n", out);

    xl6OutputPuts("void* xrtMalloc(size_t iSize);\n", out);
    xl6OutputPuts("void xrtFree(void* p);\n", out);
    xl6OutputPuts("char* xrtCopyStr(const char* sText, size_t iSize);\n", out);
    xl6OutputPuts("xnet_result xrtNetAddrParse(xnetaddr* pAddr, const char* sIP, uint16_t iPort);\n", out);
    xl6OutputPuts("void xrtWebConfigInit(xwebconfig* pCfg);\n", out);
    xl6OutputPuts("void xrtWebStaticConfigInit(xwebstaticconfig* pCfg);\n", out);
    xl6OutputPuts("xwebserver* xrtWebServerCreate(xnetengine* pEngine, const xwebconfig* pCfg);\n", out);
    xl6OutputPuts("void xrtWebServerDestroy(xwebserver* pServer);\n", out);
    xl6OutputPuts("xnet_result xrtWebServerStart(xwebserver* pServer);\n", out);
    xl6OutputPuts("void xrtWebServerStop(xwebserver* pServer);\n", out);
    xl6OutputPuts("uint16_t xrtWebServerPort(const xwebserver* pServer);\n", out);
    xl6OutputPuts("bool xrtWebServerRoute(xwebserver* pServer, uint32_t iMethodMask, const char* sPattern, xwebhandler pHandler, void* pUserData);\n", out);
    xl6OutputPuts("bool xrtWebServerStatic(xwebserver* pServer, const char* sMount, const char* sRoot, const xwebstaticconfig* pCfg);\n", out);
    xl6OutputPuts("const char* xrtWebRequestMethod(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("char* xrtWebRequestMethodCopy(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("const char* xrtWebRequestPath(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("char* xrtWebRequestPathCopy(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("const char* xrtWebRequestQuery(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("char* xrtWebRequestQueryCopy(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("const char* xrtWebRequestHeader(const xwebrequest* pReq, const char* sName);\n", out);
    xl6OutputPuts("char* xrtWebRequestHeaderCopy(const xwebrequest* pReq, const char* sName);\n", out);
    xl6OutputPuts("const void* xrtWebRequestBody(const xwebrequest* pReq, size_t* pLen);\n", out);
    xl6OutputPuts("char* xrtWebRequestBodyTextCopy(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("char* xrtWebRequestParamCopy(const xwebrequest* pReq, const char* sName);\n", out);
    xl6OutputPuts("char* xrtWebRequestQueryValueCopy(const xwebrequest* pReq, const char* sName);\n", out);
    xl6OutputPuts("char* xrtWebRequestCookieCopy(const xwebrequest* pReq, const char* sName);\n", out);
    xl6OutputPuts("void xrtWebResponseStatus(xwebresponse* pResp, uint32_t iStatusCode, const char* sReason);\n", out);
    xl6OutputPuts("bool xrtWebResponseSetHeader(xwebresponse* pResp, const char* sName, const char* sValue);\n", out);
    xl6OutputPuts("bool xrtWebResponseAddHeader(xwebresponse* pResp, const char* sName, const char* sValue);\n", out);
    xl6OutputPuts("bool xrtWebResponseText(xwebresponse* pResp, const char* sText, const char* sContentType);\n\n", out);
    xl6OutputPuts("bool xrtWebResponseJsonText(xwebresponse* pResp, const char* sText);\n", out);
    xl6OutputPuts("bool xrtWebResponseBody(xwebresponse* pResp, const void* pData, size_t iLen, const char* sContentType);\n", out);
    xl6OutputPuts("bool xrtWebResponseFile(xwebresponse* pResp, const char* sFilePath, size_t iChunkSize);\n", out);
    xl6OutputPuts("bool xrtWebResponseRedirect(xwebresponse* pResp, const char* sURL, uint32_t iStatusCode);\n", out);
    xl6OutputPuts("const char* xrtWebMimeByPath(const char* sPath);\n\n", out);

    xl6OutputPuts("static void xl6_http_server_retain_function(XL6_Function* fn)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( fn != 0 && fn->retain != 0 && fn->env != 0 ) { fn->retain(fn->env); }\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_http_server_release_function(XL6_Function* fn)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( fn != 0 && fn->release != 0 && fn->env != 0 ) { fn->release(fn->env); }\n", out);
    xl6OutputPuts("    if ( fn != 0 ) { memset(fn, 0, sizeof(*fn)); }\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xvalue xl6_http_server_text_value(const char* text)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xvoCreateText((void*)(text != 0 ? text : \"\"), 0, false);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xwebaction xl6_http_server_on_route(xwebrequest* req, xwebresponse* resp, void* userData)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Route* route = (XL6_Http_Route*)userData;\n", out);
    xl6OutputPuts("    xvalue bodyArg;\n", out);
    xl6OutputPuts("    xvalue result;\n", out);
    xl6OutputPuts("    const char* text;\n", out);
    xl6OutputPuts("    const void* body;\n", out);
    xl6OutputPuts("    size_t bodyLen = 0u;\n", out);
    xl6OutputPuts("    if ( route == 0 || req == 0 || resp == 0 || (route->handler.invoke_var == 0 && route->handler.invoke_xcall == 0) ) {\n", out);
    xl6OutputPuts("        if ( resp != 0 ) { xrtWebResponseStatus(resp, 500u, 0); (void)xrtWebResponseText(resp, \"\", \"text/plain; charset=utf-8\"); }\n", out);
    xl6OutputPuts("        return XL6_XWEB_DONE;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    body = xrtWebRequestBody(req, &bodyLen);\n", out);
    xl6OutputPuts("    bodyArg = xvoCreateText((void*)(body != 0 ? body : \"\"), (uint32_t)bodyLen, false);\n", out);
    xl6OutputPuts("    result = xl6_function_call_var(route->handler, 4,\n", out);
    xl6OutputPuts("        xl6_http_server_text_value(xrtWebRequestMethod(req)),\n", out);
    xl6OutputPuts("        xl6_http_server_text_value(xrtWebRequestPath(req)),\n", out);
    xl6OutputPuts("        xl6_http_server_text_value(xrtWebRequestQuery(req)),\n", out);
    xl6OutputPuts("        bodyArg != 0 ? bodyArg : xl6_http_server_text_value(\"\"));\n", out);
    xl6OutputPuts("    text = result != 0 ? xvoGetText(result) : \"\";\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; }\n", out);
    xl6OutputPuts("    xrtWebResponseStatus(resp, 200u, 0);\n", out);
    xl6OutputPuts("    (void)xrtWebResponseSetHeader(resp, \"X-Powered-By\", \"xlang\");\n", out);
    xl6OutputPuts("    (void)xrtWebResponseText(resp, text, \"text/plain; charset=utf-8\");\n", out);
    xl6OutputPuts("    if ( result != 0 ) { xvoUnref(result); }\n", out);
    xl6OutputPuts("    return XL6_XWEB_DONE;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xwebaction xl6_http_server_on_route_object(xwebrequest* req, xwebresponse* resp, void* userData)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Route* route = (XL6_Http_Route*)userData;\n", out);
    xl6OutputPuts("    xvalue result;\n", out);
    xl6OutputPuts("    int ok;\n", out);
    xl6OutputPuts("    if ( route == 0 || req == 0 || resp == 0 || (route->handler.invoke_var == 0 && route->handler.invoke_xcall == 0) ) {\n", out);
    xl6OutputPuts("        if ( resp != 0 ) { xrtWebResponseStatus(resp, 500u, 0); (void)xrtWebResponseText(resp, \"\", \"text/plain; charset=utf-8\"); }\n", out);
    xl6OutputPuts("        return XL6_XWEB_DONE;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    result = xl6_function_call_var(route->handler, 2, xvoCreatePoint(req), xvoCreatePoint(resp));\n", out);
    xl6OutputPuts("    ok = result != 0 && xvoGetBool(result) ? 1 : 0;\n", out);
    xl6OutputPuts("    if ( result != 0 ) { xvoUnref(result); }\n", out);
    xl6OutputPuts("    return ok ? XL6_XWEB_DONE : XL6_XWEB_ERROR;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_wrap(void* handle)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return handle;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_response_wrap(void* handle)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return handle;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_method(void* req)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebRequestMethodCopy((const xwebrequest*)req);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_path(void* req)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebRequestPathCopy((const xwebrequest*)req);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_query(void* req)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebRequestQueryCopy((const xwebrequest*)req);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_header(void* req, const char* name)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebRequestHeaderCopy((const xwebrequest*)req, name);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_param(void* req, const char* name)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebRequestParamCopy((const xwebrequest*)req, name);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_query_value(void* req, const char* name)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebRequestQueryValueCopy((const xwebrequest*)req, name);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_cookie(void* req, const char* name)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebRequestCookieCopy((const xwebrequest*)req, name);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_body_text(void* req)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebRequestBodyTextCopy((const xwebrequest*)req);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_body_data(void* req)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t len = 0u;\n", out);
    xl6OutputPuts("    return (void*)xrtWebRequestBody((const xwebrequest*)req, &len);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_http_request_body_size(void* req)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t len = 0u;\n", out);
    xl6OutputPuts("    (void)xrtWebRequestBody((const xwebrequest*)req, &len);\n", out);
    xl6OutputPuts("    return (long long)len;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void xl6_xrt_http_response_status(void* resp, long long code, const char* reason)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( code < 100 || code > 999 ) { code = 500; }\n", out);
    xl6OutputPuts("    xrtWebResponseStatus((xwebresponse*)resp, (uint32_t)code, reason);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_http_response_header(void* resp, const char* name, const char* value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebResponseSetHeader((xwebresponse*)resp, name, value);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_http_response_add_header(void* resp, const char* name, const char* value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebResponseAddHeader((xwebresponse*)resp, name, value);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_http_response_text(void* resp, const char* text, const char* contentType)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebResponseText((xwebresponse*)resp, text, contentType);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_http_response_json_text(void* resp, const char* text)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebResponseJsonText((xwebresponse*)resp, text);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_http_response_body(void* resp, void* data, size_t size, const char* contentType)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebResponseBody((xwebresponse*)resp, data, size, contentType);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_http_response_file(void* resp, const char* path, size_t chunkSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebResponseFile((xwebresponse*)resp, path, chunkSize);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_http_response_redirect(void* resp, const char* url, long long code)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebResponseRedirect((xwebresponse*)resp, url, (uint32_t)code);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_mime_by_path(const char* path)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtCopyStr((char*)xrtWebMimeByPath(path), 0);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_server_create(int port)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* self;\n", out);
    xl6OutputPuts("    xwebconfig cfg;\n", out);
    xl6OutputPuts("    if ( port < 0 || port > 65535 ) { return 0; }\n", out);
    xl6OutputPuts("    self = (XL6_Http_Server*)xrtMalloc(sizeof(XL6_Http_Server));\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    memset(self, 0, sizeof(*self));\n", out);
    xl6OutputPuts("    xrtWebConfigInit(&cfg);\n", out);
    xl6OutputPuts("    if ( xrtNetAddrParse(&cfg.tBindAddr, \"127.0.0.1\", (uint16_t)port) != 0 ) { xrtFree(self); return 0; }\n", out);
    xl6OutputPuts("    self->server = xrtWebServerCreate(0, &cfg);\n", out);
    xl6OutputPuts("    if ( self->server == 0 ) { xrtFree(self); return 0; }\n", out);
    xl6OutputPuts("    return self;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_xrt_http_server_add_route(void* handle, uint32_t methodMask, const char* pattern, XL6_Function handler, int objectHandler)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* self = (XL6_Http_Server*)handle;\n", out);
    xl6OutputPuts("    XL6_Http_Route* route;\n", out);
    xl6OutputPuts("    if ( self == 0 || self->server == 0 || pattern == 0 || (handler.invoke_var == 0 && handler.invoke_xcall == 0) ) { return false; }\n", out);
    xl6OutputPuts("    route = (XL6_Http_Route*)xrtMalloc(sizeof(XL6_Http_Route));\n", out);
    xl6OutputPuts("    if ( route == 0 ) { return false; }\n", out);
    xl6OutputPuts("    memset(route, 0, sizeof(*route));\n", out);
    xl6OutputPuts("    route->handler = handler;\n", out);
    xl6OutputPuts("    route->objectHandler = objectHandler;\n", out);
    xl6OutputPuts("    xl6_http_server_retain_function(&route->handler);\n", out);
    xl6OutputPuts("    if ( !xrtWebServerRoute(self->server, methodMask, pattern, objectHandler ? xl6_http_server_on_route_object : xl6_http_server_on_route, route) ) {\n", out);
    xl6OutputPuts("        xl6_http_server_release_function(&route->handler);\n", out);
    xl6OutputPuts("        xrtFree(route);\n", out);
    xl6OutputPuts("        return false;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    route->next = self->routes;\n", out);
    xl6OutputPuts("    self->routes = route;\n", out);
    xl6OutputPuts("    return true;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_server_get(void* handle, const char* pattern, XL6_Function handler)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_xrt_http_server_add_route(handle, XL6_XWEB_METHOD_GET, pattern, handler, 0);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_server_post(void* handle, const char* pattern, XL6_Function handler)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_xrt_http_server_add_route(handle, XL6_XWEB_METHOD_POST, pattern, handler, 0);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_server_any(void* handle, const char* pattern, XL6_Function handler)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_xrt_http_server_add_route(handle, XL6_XWEB_METHOD_ANY, pattern, handler, 0);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_server_get_request(void* handle, const char* pattern, XL6_Function handler)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_xrt_http_server_add_route(handle, XL6_XWEB_METHOD_GET, pattern, handler, 1);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_server_post_request(void* handle, const char* pattern, XL6_Function handler)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_xrt_http_server_add_route(handle, XL6_XWEB_METHOD_POST, pattern, handler, 1);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_server_any_request(void* handle, const char* pattern, XL6_Function handler)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_xrt_http_server_add_route(handle, XL6_XWEB_METHOD_ANY, pattern, handler, 1);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_server_static(void* handle, const char* mount, const char* root)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* self = (XL6_Http_Server*)handle;\n", out);
    xl6OutputPuts("    xwebstaticconfig cfg;\n", out);
    xl6OutputPuts("    if ( self == 0 || self->server == 0 || mount == 0 || root == 0 ) { return false; }\n", out);
    xl6OutputPuts("    xrtWebStaticConfigInit(&cfg);\n", out);
    xl6OutputPuts("    return xrtWebServerStatic(self->server, mount, root, &cfg);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_server_start(void* handle)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* self = (XL6_Http_Server*)handle;\n", out);
    xl6OutputPuts("    if ( self == 0 || self->server == 0 ) { return false; }\n", out);
    xl6OutputPuts("    if ( xrtWebServerStart(self->server) != 0 ) { return false; }\n", out);
    xl6OutputPuts("    self->running = 1;\n", out);
    xl6OutputPuts("    return true;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_http_server_port(void* handle)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* self = (XL6_Http_Server*)handle;\n", out);
    xl6OutputPuts("    if ( self == 0 || self->server == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    return (int)xrtWebServerPort(self->server);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void xl6_xrt_http_server_stop(void* handle)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* self = (XL6_Http_Server*)handle;\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return; }\n", out);
    xl6OutputPuts("    if ( self->server != 0 ) { xrtWebServerStop(self->server); }\n", out);
    xl6OutputPuts("    self->running = 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void xl6_xrt_http_server_destroy(void* handle)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* self = (XL6_Http_Server*)handle;\n", out);
    xl6OutputPuts("    XL6_Http_Route* route;\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return; }\n", out);
    xl6OutputPuts("    if ( self->server != 0 ) { xrtWebServerDestroy(self->server); self->server = 0; }\n", out);
    xl6OutputPuts("    route = self->routes;\n", out);
    xl6OutputPuts("    while ( route != 0 ) {\n", out);
    xl6OutputPuts("        XL6_Http_Route* next = route->next;\n", out);
    xl6OutputPuts("        xl6_http_server_release_function(&route->handler);\n", out);
    xl6OutputPuts("        xrtFree(route);\n", out);
    xl6OutputPuts("        route = next;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    xrtFree(self);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("#endif\n\n", out);
}
