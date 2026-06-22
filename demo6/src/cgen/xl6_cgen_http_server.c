static void xl6CgenEmitHttpServerRuntime(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_HTTP_SERVER_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_HTTP_SERVER_RUNTIME\n", out);
    xl6OutputPuts("#include <stdlib.h>\n", out);
    xl6OutputPuts("#include <string.h>\n", out);
    xl6OutputPuts("#define XL6_HTTPD_METHOD_CAP 16u\n", out);
    xl6OutputPuts("#define XL6_HTTPD_TARGET_CAP 256u\n", out);
    xl6OutputPuts("#define XL6_HTTPD_PATH_CAP 256u\n", out);
    xl6OutputPuts("#define XL6_HTTPD_QUERY_CAP 256u\n", out);
    xl6OutputPuts("#define XL6_HTTPD_VERSION_CAP 32u\n", out);
    xl6OutputPuts("#define XL6_HTTPD_REASON_CAP 128u\n", out);
    xl6OutputPuts("#define XL6_HTTPD_HEADER_NAME_CAP 64u\n", out);
    xl6OutputPuts("#define XL6_HTTPD_HEADER_VALUE_CAP 256u\n", out);
    xl6OutputPuts("#define XL6_HTTPD_MAX_HEADERS 32u\n\n", out);

    xl6OutputPuts("typedef struct xrt_net_engine xnetengine;\n", out);
    xl6OutputPuts("typedef struct xrt_httpd_server xhttpdserver;\n", out);
    xl6OutputPuts("typedef struct xrt_httpd_conn xhttpdconn;\n", out);
    xl6OutputPuts("typedef struct xrt_tls_config xtlsconfig;\n", out);
    xl6OutputPuts("typedef struct xrt_net_worker xnetworker;\n", out);
    xl6OutputPuts("typedef struct xrt_net_future xfuture;\n", out);
    xl6OutputPuts("typedef int xnet_result;\n\n", out);

    xl6OutputPuts("typedef struct {\n", out);
    xl6OutputPuts("    uint16_t iFamily;\n", out);
    xl6OutputPuts("    uint16_t iPort;\n", out);
    xl6OutputPuts("    uint32_t iScopeId;\n", out);
    xl6OutputPuts("    uint8_t aAddr[16];\n", out);
    xl6OutputPuts("} xnetaddr;\n\n", out);

    xl6OutputPuts("typedef struct {\n", out);
    xl6OutputPuts("    uint32_t iWorkerCount;\n", out);
    xl6OutputPuts("    uint32_t iFlags;\n", out);
    xl6OutputPuts("    uint32_t iSqEntries;\n", out);
    xl6OutputPuts("    uint32_t iCqEntries;\n", out);
    xl6OutputPuts("    uint32_t iAcceptBatch;\n", out);
    xl6OutputPuts("    uint32_t iCmdQueueSize;\n", out);
    xl6OutputPuts("    uint32_t iTimerTickMs;\n", out);
    xl6OutputPuts("    uint32_t iTimerWheelSlots;\n", out);
    xl6OutputPuts("    uint32_t iDefaultHighWater;\n", out);
    xl6OutputPuts("    uint32_t iDefaultLowWater;\n", out);
    xl6OutputPuts("    uint32_t iSmallBlockSize;\n", out);
    xl6OutputPuts("    uint32_t iMediumBlockSize;\n", out);
    xl6OutputPuts("    uint32_t iLargeBlockSize;\n", out);
    xl6OutputPuts("    uint32_t iBlockCachePerWorker;\n", out);
    xl6OutputPuts("    uint32_t iMaxConnsPerWorker;\n", out);
    xl6OutputPuts("} xnetengineconfig;\n\n", out);

    xl6OutputPuts("typedef struct {\n", out);
    xl6OutputPuts("    char sName[XL6_HTTPD_HEADER_NAME_CAP];\n", out);
    xl6OutputPuts("    char sValue[XL6_HTTPD_HEADER_VALUE_CAP];\n", out);
    xl6OutputPuts("} xhttpdheader;\n\n", out);

    xl6OutputPuts("typedef struct {\n", out);
    xl6OutputPuts("    uint32_t iFlags;\n", out);
    xl6OutputPuts("    uint32_t iHeaderCount;\n", out);
    xl6OutputPuts("    uint32_t iMethod;\n", out);
    xl6OutputPuts("    int64_t iContentLength;\n", out);
    xl6OutputPuts("    char sMethod[XL6_HTTPD_METHOD_CAP];\n", out);
    xl6OutputPuts("    char sTarget[XL6_HTTPD_TARGET_CAP];\n", out);
    xl6OutputPuts("    char sPath[XL6_HTTPD_PATH_CAP];\n", out);
    xl6OutputPuts("    char sQuery[XL6_HTTPD_QUERY_CAP];\n", out);
    xl6OutputPuts("    char sVersion[XL6_HTTPD_VERSION_CAP];\n", out);
    xl6OutputPuts("    xhttpdheader arrHeaders[XL6_HTTPD_MAX_HEADERS];\n", out);
    xl6OutputPuts("    char* pBody;\n", out);
    xl6OutputPuts("    size_t iBodyLen;\n", out);
    xl6OutputPuts("} xhttpdrequest;\n\n", out);

    xl6OutputPuts("typedef struct {\n", out);
    xl6OutputPuts("    uint32_t iStatusCode;\n", out);
    xl6OutputPuts("    uint32_t iFlags;\n", out);
    xl6OutputPuts("    uint32_t iHeaderCount;\n", out);
    xl6OutputPuts("    char sReason[XL6_HTTPD_REASON_CAP];\n", out);
    xl6OutputPuts("    xhttpdheader arrHeaders[XL6_HTTPD_MAX_HEADERS];\n", out);
    xl6OutputPuts("    char* pBody;\n", out);
    xl6OutputPuts("    size_t iBodyLen;\n", out);
    xl6OutputPuts("} xhttpdresponse;\n\n", out);

    xl6OutputPuts("typedef struct {\n", out);
    xl6OutputPuts("    xnetaddr tBindAddr;\n", out);
    xl6OutputPuts("    uint32_t iFlags;\n", out);
    xl6OutputPuts("    uint32_t iBacklog;\n", out);
    xl6OutputPuts("    uint32_t iRecvLimit;\n", out);
    xl6OutputPuts("    uint32_t iBodyLimit;\n", out);
    xl6OutputPuts("    const xtlsconfig* pTlsConfig;\n", out);
    xl6OutputPuts("} xhttpdconfig;\n\n", out);

    xl6OutputPuts("typedef struct {\n", out);
    xl6OutputPuts("    void (*OnOpen)(void* pOwner, xhttpdserver* pServer, xhttpdconn* pConn);\n", out);
    xl6OutputPuts("    bool (*OnRequest)(void* pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq, xhttpdresponse* pResp);\n", out);
    xl6OutputPuts("    xfuture* (*OnRequestAsync)(void* pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq);\n", out);
    xl6OutputPuts("    void (*OnClose)(void* pOwner, xhttpdserver* pServer, xhttpdconn* pConn, xnet_result iReason);\n", out);
    xl6OutputPuts("    void (*OnError)(void* pOwner, xhttpdserver* pServer, xhttpdconn* pConn, int iSysErr);\n", out);
    xl6OutputPuts("} xhttpdevents;\n\n", out);

    xl6OutputPuts("typedef struct XL6_Http_Server {\n", out);
    xl6OutputPuts("    xnetengine* engine;\n", out);
    xl6OutputPuts("    xhttpdserver* server;\n", out);
    xl6OutputPuts("    XL6_Function handler;\n", out);
    xl6OutputPuts("    int running;\n", out);
    xl6OutputPuts("} XL6_Http_Server;\n\n", out);

    xl6OutputPuts("void* xrtMalloc(size_t iSize);\n", out);
    xl6OutputPuts("void xrtFree(void* p);\n", out);
    xl6OutputPuts("void xrtNetEngineConfigInit(xnetengineconfig* pCfg);\n", out);
    xl6OutputPuts("xnetengine* xrtNetEngineCreate(const xnetengineconfig* pCfg);\n", out);
    xl6OutputPuts("void xrtNetEngineDestroy(xnetengine* pEngine);\n", out);
    xl6OutputPuts("xnet_result xrtNetEngineStart(xnetengine* pEngine);\n", out);
    xl6OutputPuts("void xrtNetEngineStop(xnetengine* pEngine);\n", out);
    xl6OutputPuts("xnet_result xrtNetAddrParse(xnetaddr* pAddr, const char* sIP, uint16_t iPort);\n", out);
    xl6OutputPuts("void xrtHttpdConfigInit(xhttpdconfig* pCfg);\n", out);
    xl6OutputPuts("xhttpdserver* xrtHttpdCreate(xnetengine* pEngine, const xhttpdconfig* pCfg, const xhttpdevents* pEvents, void* pUserData);\n", out);
    xl6OutputPuts("uint16_t xrtHttpdBoundPort(const xhttpdserver* pServer);\n", out);
    xl6OutputPuts("xnet_result xrtHttpdStart(xhttpdserver* pServer);\n", out);
    xl6OutputPuts("void xrtHttpdStop(xhttpdserver* pServer);\n", out);
    xl6OutputPuts("void xrtHttpdDestroy(xhttpdserver* pServer);\n", out);
    xl6OutputPuts("void xrtHttpdResponseSetStatus(xhttpdresponse* pResp, uint32_t iStatusCode, const char* sReason);\n", out);
    xl6OutputPuts("bool xrtHttpdResponseSetHeader(xhttpdresponse* pResp, const char* sName, const char* sValue);\n", out);
    xl6OutputPuts("bool xrtHttpdResponseSetBodyCopy(xhttpdresponse* pResp, const void* pData, size_t iLen, const char* sContentType);\n\n", out);

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

    xl6OutputPuts("static bool xl6_http_server_on_request(void* owner, xhttpdserver* server, xhttpdconn* conn, const xhttpdrequest* req, xhttpdresponse* resp)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* self = (XL6_Http_Server*)owner;\n", out);
    xl6OutputPuts("    xvalue bodyArg;\n", out);
    xl6OutputPuts("    xvalue result;\n", out);
    xl6OutputPuts("    const char* text;\n", out);
    xl6OutputPuts("    (void)server;\n", out);
    xl6OutputPuts("    (void)conn;\n", out);
    xl6OutputPuts("    if ( self == 0 || req == 0 || resp == 0 || (self->handler.invoke_var == 0 && self->handler.invoke_xcall == 0) ) {\n", out);
    xl6OutputPuts("        if ( resp != 0 ) {\n", out);
    xl6OutputPuts("            xrtHttpdResponseSetStatus(resp, 500u, 0);\n", out);
    xl6OutputPuts("            (void)xrtHttpdResponseSetBodyCopy(resp, \"\", 0u, \"text/plain; charset=utf-8\");\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("        return true;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    bodyArg = xvoCreateText((void*)(req->pBody != 0 ? req->pBody : \"\"), (uint32_t)req->iBodyLen, false);\n", out);
    xl6OutputPuts("    result = xl6_function_call_var(self->handler, 4,\n", out);
    xl6OutputPuts("        xl6_http_server_text_value(req->sMethod),\n", out);
    xl6OutputPuts("        xl6_http_server_text_value(req->sPath),\n", out);
    xl6OutputPuts("        xl6_http_server_text_value(req->sQuery),\n", out);
    xl6OutputPuts("        bodyArg != 0 ? bodyArg : xl6_http_server_text_value(\"\"));\n", out);
    xl6OutputPuts("    text = result != 0 ? xvoGetText(result) : \"\";\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; }\n", out);
    xl6OutputPuts("    xrtHttpdResponseSetStatus(resp, 200u, 0);\n", out);
    xl6OutputPuts("    (void)xrtHttpdResponseSetHeader(resp, \"X-Powered-By\", \"xlang\");\n", out);
    xl6OutputPuts("    (void)xrtHttpdResponseSetBodyCopy(resp, text, strlen(text), \"text/plain; charset=utf-8\");\n", out);
    xl6OutputPuts("    if ( result != 0 ) { xvoUnref(result); }\n", out);
    xl6OutputPuts("    return true;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_server_create(int port, XL6_Function handler)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* self;\n", out);
    xl6OutputPuts("    xnetengineconfig engineCfg;\n", out);
    xl6OutputPuts("    xhttpdconfig httpCfg;\n", out);
    xl6OutputPuts("    xhttpdevents events;\n", out);
    xl6OutputPuts("    if ( port < 0 || port > 65535 || (handler.invoke_var == 0 && handler.invoke_xcall == 0) ) { return 0; }\n", out);
    xl6OutputPuts("    self = (XL6_Http_Server*)xrtMalloc(sizeof(XL6_Http_Server));\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    memset(self, 0, sizeof(*self));\n", out);
    xl6OutputPuts("    self->handler = handler;\n", out);
    xl6OutputPuts("    xl6_http_server_retain_function(&self->handler);\n", out);
    xl6OutputPuts("    xrtNetEngineConfigInit(&engineCfg);\n", out);
    xl6OutputPuts("    engineCfg.iWorkerCount = 1u;\n", out);
    xl6OutputPuts("    self->engine = xrtNetEngineCreate(&engineCfg);\n", out);
    xl6OutputPuts("    if ( self->engine == 0 || xrtNetEngineStart(self->engine) != 0 ) { goto fail; }\n", out);
    xl6OutputPuts("    xrtHttpdConfigInit(&httpCfg);\n", out);
    xl6OutputPuts("    if ( xrtNetAddrParse(&httpCfg.tBindAddr, \"127.0.0.1\", (uint16_t)port) != 0 ) { goto fail; }\n", out);
    xl6OutputPuts("    memset(&events, 0, sizeof(events));\n", out);
    xl6OutputPuts("    events.OnRequest = xl6_http_server_on_request;\n", out);
    xl6OutputPuts("    self->server = xrtHttpdCreate(self->engine, &httpCfg, &events, self);\n", out);
    xl6OutputPuts("    if ( self->server == 0 ) { goto fail; }\n", out);
    xl6OutputPuts("    return self;\n", out);
    xl6OutputPuts("fail:\n", out);
    xl6OutputPuts("    if ( self->server != 0 ) { xrtHttpdDestroy(self->server); }\n", out);
    xl6OutputPuts("    if ( self->engine != 0 ) { xrtNetEngineStop(self->engine); xrtNetEngineDestroy(self->engine); }\n", out);
    xl6OutputPuts("    xl6_http_server_release_function(&self->handler);\n", out);
    xl6OutputPuts("    xrtFree(self);\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_server_start(void* handle)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* self = (XL6_Http_Server*)handle;\n", out);
    xl6OutputPuts("    if ( self == 0 || self->server == 0 ) { return false; }\n", out);
    xl6OutputPuts("    if ( xrtHttpdStart(self->server) != 0 ) { return false; }\n", out);
    xl6OutputPuts("    self->running = 1;\n", out);
    xl6OutputPuts("    return true;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_http_server_port(void* handle)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* self = (XL6_Http_Server*)handle;\n", out);
    xl6OutputPuts("    if ( self == 0 || self->server == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    return (int)xrtHttpdBoundPort(self->server);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void xl6_xrt_http_server_stop(void* handle)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* self = (XL6_Http_Server*)handle;\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return; }\n", out);
    xl6OutputPuts("    if ( self->server != 0 ) { xrtHttpdStop(self->server); }\n", out);
    xl6OutputPuts("    if ( self->engine != 0 ) { xrtNetEngineStop(self->engine); }\n", out);
    xl6OutputPuts("    self->running = 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void xl6_xrt_http_server_destroy(void* handle)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* self = (XL6_Http_Server*)handle;\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return; }\n", out);
    xl6OutputPuts("    if ( self->server != 0 ) { xrtHttpdDestroy(self->server); self->server = 0; }\n", out);
    xl6OutputPuts("    if ( self->engine != 0 ) { xrtNetEngineStop(self->engine); xrtNetEngineDestroy(self->engine); self->engine = 0; }\n", out);
    xl6OutputPuts("    xl6_http_server_release_function(&self->handler);\n", out);
    xl6OutputPuts("    xrtFree(self);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("#endif\n\n", out);
}
