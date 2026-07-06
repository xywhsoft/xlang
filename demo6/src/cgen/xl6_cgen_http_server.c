static void xl6CgenEmitHttpServerRuntime(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

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
    xl6OutputPuts("typedef struct xrt_tls_session xtlssession;\n", out);
    xl6OutputPuts("typedef struct xrt_tls_resume xtlsresume;\n", out);
    xl6OutputPuts("typedef struct { uint16_t iFamily; uint16_t iPort; uint32_t iScopeId; uint8_t aAddr[16]; } xnetaddr;\n", out);
    xl6OutputPuts("typedef struct { const char* sCertFile; const char* sKeyFile; const char* sCaFile; const char* sCrlFile; const void* pCertData; size_t iCertDataLen; const void* pKeyData; size_t iKeyDataLen; const void* pCaData; size_t iCaDataLen; const void* pCrlData; size_t iCrlDataLen; const char* sHostName; bool bVerifyPeer; void (*OnSNI)(xtlssession* pSession, const char* sHostName, void* pUserData); void* pSNIUserData; bool bAllowTLS12Ed25519; uint16_t iMaxVersion; const xtlsresume* pResume; volatile long iDataLock; } xtlsconfig;\n", out);
    xl6OutputPuts("typedef struct { xnetaddr tBindAddr; uint32_t iFlags; uint32_t iBacklog; uint32_t iRecvLimit; uint32_t iBodyLimit; uint32_t iHeaderLimit; uint32_t iHeaderBytesLimit; uint32_t iHeaderTimeoutMs; uint32_t iBodyTimeoutMs; uint32_t iIdleTimeoutMs; uint32_t iWriteTimeoutMs; uint32_t iMultipartPartLimit; uint32_t iMultipartPartSizeLimit; uint32_t iWorkerCount; const xtlsconfig* pTlsConfig; } xwebconfig;\n", out);

    xl6OutputPuts("#define XL6_XWEB_DONE 1\n", out);
    xl6OutputPuts("#define XL6_XWEB_ERROR -1\n", out);
    xl6OutputPuts("#define XL6_XWEB_METHOD_GET (1u << 1)\n", out);
    xl6OutputPuts("#define XL6_XWEB_METHOD_HEAD (1u << 2)\n", out);
    xl6OutputPuts("#define XL6_XWEB_METHOD_POST (1u << 3)\n", out);
    xl6OutputPuts("#define XL6_XWEB_METHOD_ANY 0xffffffffu\n\n", out);

    xl6OutputPuts("typedef struct XL6_Http_Route {\n", out);
    xl6OutputPuts("    XL6_Function handler;\n", out);
    xl6OutputPuts("    XL6_Function beginHandler;\n", out);
    xl6OutputPuts("    XL6_Function chunkHandler;\n", out);
    xl6OutputPuts("    XL6_Function endHandler;\n", out);
    xl6OutputPuts("    int objectHandler;\n", out);
    xl6OutputPuts("    struct XL6_Http_Route* next;\n", out);
    xl6OutputPuts("} XL6_Http_Route;\n\n", out);

    xl6OutputPuts("typedef struct XL6_Http_Server {\n", out);
    xl6OutputPuts("    xwebserver* server;\n", out);
    xl6OutputPuts("    XL6_Http_Route* routes;\n", out);
    xl6OutputPuts("    int running;\n", out);
    xl6OutputPuts("} XL6_Http_Server;\n\n", out);

    xl6OutputPuts("typedef struct XL6_Http_App {\n", out);
    xl6OutputPuts("    xwebapp* app;\n", out);
    xl6OutputPuts("} XL6_Http_App;\n\n", out);

    xl6OutputPuts("void* xrtMalloc(size_t iSize);\n", out);
    xl6OutputPuts("void xrtFree(void* p);\n", out);
    xl6OutputPuts("char* xrtCopyStr(const char* sText, size_t iSize);\n", out);
    xl6OutputPuts("xvalue xvoCreateInt(long long iVal);\n", out);
    xl6OutputPuts("xwebapp* xrtWebAppCreate(void);\n", out);
    xl6OutputPuts("void xrtWebAppDestroy(xwebapp* pApp);\n", out);
    xl6OutputPuts("uint32_t xrtWebMethodMask(const char* sMethods);\n", out);
    xl6OutputPuts("bool xrtWebAppError(xwebapp* pApp, int (*pHandler)(xwebrequest* pReq, xwebresponse* pResp, uint32_t iStatusCode, const char* sMessage, void* pUserData), void* pUserData, void (*pFreeUserData)(void* pUserData));\n", out);
    xl6OutputPuts("bool xrtWebAppRouteEx(xwebapp* pApp, uint32_t iMethodMask, const char* sPattern, int (*pHandler)(xwebrequest* pReq, xwebresponse* pResp, void* pUserData), void* pUserData, void (*pFreeUserData)(void* pUserData));\n", out);
    xl6OutputPuts("bool xrtWebAppStreamRouteEx(xwebapp* pApp, uint32_t iMethodMask, const char* sPattern, bool (*pBegin)(xwebrequest* pReq, void* pUserData), bool (*pBody)(xwebrequest* pReq, const void* pData, size_t iLen, void* pUserData), int (*pEnd)(xwebrequest* pReq, xwebresponse* pResp, void* pUserData), void* pUserData, void (*pFreeUserData)(void* pUserData));\n", out);
    xl6OutputPuts("bool xrtWebAppStaticDefault(xwebapp* pApp, const char* sMount, const char* sRoot);\n", out);
    xl6OutputPuts("bool xrtWebAppStaticEx(xwebapp* pApp, const char* sMount, const char* sRoot, const char* sIndexNames, const char* sCacheControl, size_t iChunkSize, uint32_t iFlags);\n", out);
    xl6OutputPuts("int xrtNetAddrParse(xnetaddr* pAddr, const char* sHost, uint16_t iPort);\n", out);
    xl6OutputPuts("void xrtWebConfigInit(xwebconfig* pCfg);\n", out);
    xl6OutputPuts("xwebserver* xrtWebServerCreate(xnetengine* pEngine, const xwebconfig* pCfg);\n", out);
    xl6OutputPuts("xwebserver* xrtWebServerCreateHostEx(xnetengine* pEngine, const char* sHost, uint16_t iPort, uint32_t iBacklog, uint32_t iRecvLimit, uint32_t iBodyLimit, uint32_t iWorkerCount);\n", out);
    xl6OutputPuts("xwebserver* xrtWebServerCreateHostTlsEx(xnetengine* pEngine, const char* sCertFile, const char* sKeyFile, const char* sHost, uint16_t iPort, uint32_t iBacklog, uint32_t iRecvLimit, uint32_t iBodyLimit, uint32_t iWorkerCount);\n", out);
    xl6OutputPuts("void xrtWebServerDestroy(xwebserver* pServer);\n", out);
    xl6OutputPuts("bool xrtWebServerStartOk(xwebserver* pServer);\n", out);
    xl6OutputPuts("void xrtWebServerStop(xwebserver* pServer);\n", out);
    xl6OutputPuts("bool xrtWebServerDrain(xwebserver* pServer, uint32_t iTimeoutMs);\n", out);
    xl6OutputPuts("uint16_t xrtWebServerPort(const xwebserver* pServer);\n", out);
    xl6OutputPuts("uint32_t xrtWebServerRequestCount(const xwebserver* pServer);\n", out);
    xl6OutputPuts("uint32_t xrtWebServerConnectionCount(xwebserver* pServer);\n", out);
    xl6OutputPuts("bool xrtWebServerSetApp(xwebserver* pServer, xwebapp* pApp);\n", out);
    xl6OutputPuts("bool xrtWebServerReloadApp(xwebserver* pServer, xwebapp* pApp);\n", out);
    xl6OutputPuts("bool xrtWebServerHost(xwebserver* pServer, const char* sHost, xwebapp* pApp);\n", out);
    xl6OutputPuts("bool xrtWebServerRemoveHost(xwebserver* pServer, const char* sHost);\n", out);
    xl6OutputPuts("bool xrtWebServerTlsHost(xwebserver* pServer, const char* sHost, const char* sCertFile, const char* sKeyFile);\n", out);
    xl6OutputPuts("bool xrtWebServerRemoveTlsHost(xwebserver* pServer, const char* sHost);\n", out);
    xl6OutputPuts("bool xrtWebServerError(xwebserver* pServer, int (*pHandler)(xwebrequest* pReq, xwebresponse* pResp, uint32_t iStatusCode, const char* sMessage, void* pUserData), void* pUserData, void (*pFreeUserData)(void* pUserData));\n", out);
    xl6OutputPuts("bool xrtWebServerRouteEx(xwebserver* pServer, uint32_t iMethodMask, const char* sPattern, int (*pHandler)(xwebrequest* pReq, xwebresponse* pResp, void* pUserData), void* pUserData, void (*pFreeUserData)(void* pUserData));\n", out);
    xl6OutputPuts("bool xrtWebServerRoute(xwebserver* pServer, uint32_t iMethodMask, const char* sPattern, int (*pHandler)(xwebrequest* pReq, xwebresponse* pResp, void* pUserData), void* pUserData);\n", out);
    xl6OutputPuts("bool xrtWebServerStreamRouteEx(xwebserver* pServer, uint32_t iMethodMask, const char* sPattern, bool (*pBegin)(xwebrequest* pReq, void* pUserData), bool (*pBody)(xwebrequest* pReq, const void* pData, size_t iLen, void* pUserData), int (*pEnd)(xwebrequest* pReq, xwebresponse* pResp, void* pUserData), void* pUserData, void (*pFreeUserData)(void* pUserData));\n", out);
    xl6OutputPuts("bool xrtWebServerStaticDefault(xwebserver* pServer, const char* sMount, const char* sRoot);\n", out);
    xl6OutputPuts("bool xrtWebServerStaticEx(xwebserver* pServer, const char* sMount, const char* sRoot, const char* sIndexNames, const char* sCacheControl, size_t iChunkSize, uint32_t iFlags);\n", out);
    xl6OutputPuts("const char* xrtWebRequestMethod(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("char* xrtWebRequestMethodCopy(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("uint32_t xrtWebRequestMethodId(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("char* xrtWebRequestTargetCopy(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("const char* xrtWebRequestPath(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("char* xrtWebRequestPathCopy(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("const char* xrtWebRequestQuery(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("char* xrtWebRequestQueryCopy(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("size_t xrtWebRequestQueryCount(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("bool xrtWebRequestQueryHasValueAt(const xwebrequest* pReq, size_t iIndex);\n", out);
    xl6OutputPuts("char* xrtWebRequestQueryNameCopyAt(const xwebrequest* pReq, size_t iIndex);\n", out);
    xl6OutputPuts("char* xrtWebRequestQueryValueCopyAt(const xwebrequest* pReq, size_t iIndex);\n", out);
    xl6OutputPuts("const char* xrtWebRequestHeader(const xwebrequest* pReq, const char* sName);\n", out);
    xl6OutputPuts("char* xrtWebRequestHeaderCopy(const xwebrequest* pReq, const char* sName);\n", out);
    xl6OutputPuts("size_t xrtWebRequestHeaderCount(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("char* xrtWebRequestHeaderNameCopyAt(const xwebrequest* pReq, size_t iIndex);\n", out);
    xl6OutputPuts("char* xrtWebRequestHeaderValueCopyAt(const xwebrequest* pReq, size_t iIndex);\n", out);
    xl6OutputPuts("const void* xrtWebRequestBody(const xwebrequest* pReq, size_t* pLen);\n", out);
    xl6OutputPuts("char* xrtWebRequestBodyTextCopy(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("int64_t xrtWebRequestContentLength(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("const char* xrtWebRequestContentType(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("bool xrtWebRequestIsFormUrlEncoded(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("size_t xrtWebRequestFormCount(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("bool xrtWebRequestFormHasValueAt(const xwebrequest* pReq, size_t iIndex);\n", out);
    xl6OutputPuts("char* xrtWebRequestFormNameCopyAt(const xwebrequest* pReq, size_t iIndex);\n", out);
    xl6OutputPuts("char* xrtWebRequestFormValueCopyAt(const xwebrequest* pReq, size_t iIndex);\n", out);
    xl6OutputPuts("char* xrtWebRequestFormValueCopy(const xwebrequest* pReq, const char* sName);\n", out);
    xl6OutputPuts("bool xrtWebRequestIsMultipartForm(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("size_t xrtWebRequestMultipartCount(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("bool xrtWebRequestMultipartIsFileAt(const xwebrequest* pReq, size_t iIndex);\n", out);
    xl6OutputPuts("char* xrtWebRequestMultipartNameCopyAt(const xwebrequest* pReq, size_t iIndex);\n", out);
    xl6OutputPuts("char* xrtWebRequestMultipartFileNameCopyAt(const xwebrequest* pReq, size_t iIndex);\n", out);
    xl6OutputPuts("char* xrtWebRequestMultipartContentTypeCopyAt(const xwebrequest* pReq, size_t iIndex);\n", out);
    xl6OutputPuts("bool xrtWebRequestMultipartBodyViewAt(const xwebrequest* pReq, size_t iIndex, const void** ppData, size_t* pLen);\n", out);
    xl6OutputPuts("size_t xrtWebRequestMultipartBodySizeAt(const xwebrequest* pReq, size_t iIndex);\n", out);
    xl6OutputPuts("char* xrtWebRequestMultipartBodyTextCopyAt(const xwebrequest* pReq, size_t iIndex);\n", out);
    xl6OutputPuts("char* xrtWebRequestMultipartValueCopy(const xwebrequest* pReq, const char* sName);\n", out);
    xl6OutputPuts("char* xrtWebRequestLocalAddrCopy(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("char* xrtWebRequestRemoteAddrCopy(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("char* xrtWebRequestTlsSNICopy(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("int xrtWebRequestLocalPort(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("int xrtWebRequestRemotePort(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("char* xrtWebRequestParamCopy(const xwebrequest* pReq, const char* sName);\n", out);
    xl6OutputPuts("size_t xrtWebRequestParamCount(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("char* xrtWebRequestParamNameCopyAt(const xwebrequest* pReq, size_t iIndex);\n", out);
    xl6OutputPuts("char* xrtWebRequestParamValueCopyAt(const xwebrequest* pReq, size_t iIndex);\n", out);
    xl6OutputPuts("char* xrtWebRequestQueryValueCopy(const xwebrequest* pReq, const char* sName);\n", out);
    xl6OutputPuts("char* xrtWebRequestCookieCopy(const xwebrequest* pReq, const char* sName);\n", out);
    xl6OutputPuts("size_t xrtWebRequestCookieCount(const xwebrequest* pReq);\n", out);
    xl6OutputPuts("char* xrtWebRequestCookieNameCopyAt(const xwebrequest* pReq, size_t iIndex);\n", out);
    xl6OutputPuts("char* xrtWebRequestCookieValueCopyAt(const xwebrequest* pReq, size_t iIndex);\n", out);
    xl6OutputPuts("void xrtWebResponseStatus(xwebresponse* pResp, uint32_t iStatusCode, const char* sReason);\n", out);
    xl6OutputPuts("bool xrtWebResponseSetHeader(xwebresponse* pResp, const char* sName, const char* sValue);\n", out);
    xl6OutputPuts("bool xrtWebResponseAddHeader(xwebresponse* pResp, const char* sName, const char* sValue);\n", out);
    xl6OutputPuts("bool xrtWebResponseCookie(xwebresponse* pResp, const char* sName, const char* sValue, const char* sPath, int32_t iMaxAge, uint32_t iSameSite, uint32_t iFlags);\n", out);
    xl6OutputPuts("bool xrtWebResponseDeleteCookie(xwebresponse* pResp, const char* sName, const char* sPath);\n", out);
    xl6OutputPuts("bool xrtWebResponseText(xwebresponse* pResp, const char* sText, const char* sContentType);\n\n", out);
    xl6OutputPuts("bool xrtWebResponseJsonText(xwebresponse* pResp, const char* sText);\n", out);
    xl6OutputPuts("bool xrtWebResponseBody(xwebresponse* pResp, const void* pData, size_t iLen, const char* sContentType);\n", out);
    xl6OutputPuts("bool xrtWebResponseStart(xwebresponse* pResp);\n", out);
    xl6OutputPuts("bool xrtWebResponseSend(xwebresponse* pResp, const void* pData, size_t iLen);\n", out);
    xl6OutputPuts("bool xrtWebResponseWriteText(xwebresponse* pResp, const char* sText);\n", out);
    xl6OutputPuts("bool xrtWebResponseEnd(xwebresponse* pResp);\n", out);
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

    xl6OutputPuts("static void xl6_http_server_free_route(void* userData)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Route* route = (XL6_Http_Route*)userData;\n", out);
    xl6OutputPuts("    if ( route == 0 ) { return; }\n", out);
    xl6OutputPuts("    xl6_http_server_release_function(&route->handler);\n", out);
    xl6OutputPuts("    xl6_http_server_release_function(&route->beginHandler);\n", out);
    xl6OutputPuts("    xl6_http_server_release_function(&route->chunkHandler);\n", out);
    xl6OutputPuts("    xl6_http_server_release_function(&route->endHandler);\n", out);
    xl6OutputPuts("    xrtFree(route);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static xvalue xl6_http_server_text_value(const char* text)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xvoCreateText((void*)(text != 0 ? text : \"\"), 0, false);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_http_server_on_route(xwebrequest* req, xwebresponse* resp, void* userData)\n", out);
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

    xl6OutputPuts("static int xl6_http_server_on_route_object(xwebrequest* req, xwebresponse* resp, void* userData)\n", out);
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

    xl6OutputPuts("static int xl6_http_server_on_error(xwebrequest* req, xwebresponse* resp, uint32_t statusCode, const char* message, void* userData)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Route* route = (XL6_Http_Route*)userData;\n", out);
    xl6OutputPuts("    xvalue result;\n", out);
    xl6OutputPuts("    int ok;\n", out);
    xl6OutputPuts("    if ( route == 0 || req == 0 || resp == 0 || (route->handler.invoke_var == 0 && route->handler.invoke_xcall == 0) ) { return XL6_XWEB_ERROR; }\n", out);
    xl6OutputPuts("    result = xl6_function_call_var(route->handler, 4,\n", out);
    xl6OutputPuts("        xvoCreateInt((long long)statusCode),\n", out);
    xl6OutputPuts("        xl6_http_server_text_value(message != 0 ? message : \"\"),\n", out);
    xl6OutputPuts("        xvoCreatePoint(req),\n", out);
    xl6OutputPuts("        xvoCreatePoint(resp));\n", out);
    xl6OutputPuts("    ok = result != 0 && xvoGetBool(result) ? 1 : 0;\n", out);
    xl6OutputPuts("    if ( result != 0 ) { xvoUnref(result); }\n", out);
    xl6OutputPuts("    return ok ? XL6_XWEB_DONE : XL6_XWEB_ERROR;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_http_server_on_body_begin(xwebrequest* req, void* userData)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Route* route = (XL6_Http_Route*)userData;\n", out);
    xl6OutputPuts("    xvalue result;\n", out);
    xl6OutputPuts("    int ok;\n", out);
    xl6OutputPuts("    if ( route == 0 || req == 0 || (route->beginHandler.invoke_var == 0 && route->beginHandler.invoke_xcall == 0) ) { return false; }\n", out);
    xl6OutputPuts("    result = xl6_function_call_var(route->beginHandler, 1, xvoCreatePoint(req));\n", out);
    xl6OutputPuts("    ok = result != 0 && xvoGetBool(result) ? 1 : 0;\n", out);
    xl6OutputPuts("    if ( result != 0 ) { xvoUnref(result); }\n", out);
    xl6OutputPuts("    return ok ? true : false;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_http_server_on_body_chunk(xwebrequest* req, const void* data, size_t size, void* userData)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Route* route = (XL6_Http_Route*)userData;\n", out);
    xl6OutputPuts("    xvalue result;\n", out);
    xl6OutputPuts("    int ok;\n", out);
    xl6OutputPuts("    if ( route == 0 || req == 0 || (size > 0u && data == 0) || (route->chunkHandler.invoke_var == 0 && route->chunkHandler.invoke_xcall == 0) ) { return false; }\n", out);
    xl6OutputPuts("    result = xl6_function_call_var(route->chunkHandler, 3, xvoCreatePoint(req), xvoCreatePoint((void*)data), xvoCreateInt((long long)size));\n", out);
    xl6OutputPuts("    ok = result != 0 && xvoGetBool(result) ? 1 : 0;\n", out);
    xl6OutputPuts("    if ( result != 0 ) { xvoUnref(result); }\n", out);
    xl6OutputPuts("    return ok ? true : false;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_http_server_on_body_end(xwebrequest* req, xwebresponse* resp, void* userData)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Route* route = (XL6_Http_Route*)userData;\n", out);
    xl6OutputPuts("    xvalue result;\n", out);
    xl6OutputPuts("    int ok;\n", out);
    xl6OutputPuts("    if ( route == 0 || req == 0 || resp == 0 || (route->endHandler.invoke_var == 0 && route->endHandler.invoke_xcall == 0) ) { return XL6_XWEB_ERROR; }\n", out);
    xl6OutputPuts("    result = xl6_function_call_var(route->endHandler, 2, xvoCreatePoint(req), xvoCreatePoint(resp));\n", out);
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

    xl6OutputPuts("long long xl6_xrt_http_request_method_id(void* req)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return (long long)xrtWebRequestMethodId((const xwebrequest*)req);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_target(void* req)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebRequestTargetCopy((const xwebrequest*)req);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_path(void* req)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebRequestPathCopy((const xwebrequest*)req);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_query(void* req)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebRequestQueryCopy((const xwebrequest*)req);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_http_request_query_count(void* req)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return (long long)xrtWebRequestQueryCount((const xwebrequest*)req);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_http_request_query_has_value(void* req, long long index)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( index < 0 ) { return 0; }\n", out);
    xl6OutputPuts("    return xrtWebRequestQueryHasValueAt((const xwebrequest*)req, (size_t)index) ? 1 : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_query_name(void* req, long long index)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( index < 0 ) { return xrtCopyStr((str)\"\", 0); }\n", out);
    xl6OutputPuts("    return xrtWebRequestQueryNameCopyAt((const xwebrequest*)req, (size_t)index);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_query_value_at(void* req, long long index)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( index < 0 ) { return xrtCopyStr((str)\"\", 0); }\n", out);
    xl6OutputPuts("    return xrtWebRequestQueryValueCopyAt((const xwebrequest*)req, (size_t)index);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_header(void* req, const char* name)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebRequestHeaderCopy((const xwebrequest*)req, name);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_http_request_header_count(void* req)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return (long long)xrtWebRequestHeaderCount((const xwebrequest*)req);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_header_name(void* req, long long index)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( index < 0 ) { return xrtCopyStr((str)\"\", 0); }\n", out);
    xl6OutputPuts("    return xrtWebRequestHeaderNameCopyAt((const xwebrequest*)req, (size_t)index);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_header_value(void* req, long long index)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( index < 0 ) { return xrtCopyStr((str)\"\", 0); }\n", out);
    xl6OutputPuts("    return xrtWebRequestHeaderValueCopyAt((const xwebrequest*)req, (size_t)index);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_param(void* req, const char* name)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebRequestParamCopy((const xwebrequest*)req, name);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_http_request_param_count(void* req)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return (long long)xrtWebRequestParamCount((const xwebrequest*)req);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_param_name(void* req, long long index)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( index < 0 ) { return xrtCopyStr((str)\"\", 0); }\n", out);
    xl6OutputPuts("    return xrtWebRequestParamNameCopyAt((const xwebrequest*)req, (size_t)index);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_param_value(void* req, long long index)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( index < 0 ) { return xrtCopyStr((str)\"\", 0); }\n", out);
    xl6OutputPuts("    return xrtWebRequestParamValueCopyAt((const xwebrequest*)req, (size_t)index);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_query_value(void* req, const char* name)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebRequestQueryValueCopy((const xwebrequest*)req, name);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_cookie(void* req, const char* name)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebRequestCookieCopy((const xwebrequest*)req, name);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_http_request_cookie_count(void* req)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return (long long)xrtWebRequestCookieCount((const xwebrequest*)req);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_cookie_name(void* req, long long index)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( index < 0 ) { return xrtCopyStr((str)\"\", 0); }\n", out);
    xl6OutputPuts("    return xrtWebRequestCookieNameCopyAt((const xwebrequest*)req, (size_t)index);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_cookie_value_at(void* req, long long index)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( index < 0 ) { return xrtCopyStr((str)\"\", 0); }\n", out);
    xl6OutputPuts("    return xrtWebRequestCookieValueCopyAt((const xwebrequest*)req, (size_t)index);\n", out);
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

    xl6OutputPuts("void* xl6_xrt_http_request_body_buffer(void* req)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t len = 0u;\n", out);
    xl6OutputPuts("    const void* data = xrtWebRequestBody((const xwebrequest*)req, &len);\n", out);
    xl6OutputPuts("    return xl6_xrt_buffer_from_view_bytes(data, len);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_http_request_content_length(void* req)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return (long long)xrtWebRequestContentLength((const xwebrequest*)req);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_content_type(void* req)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtCopyStr((str)xrtWebRequestContentType((const xwebrequest*)req), 0);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_http_request_is_form_urlencoded(void* req)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebRequestIsFormUrlEncoded((const xwebrequest*)req) ? 1 : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_http_request_form_count(void* req)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return (long long)xrtWebRequestFormCount((const xwebrequest*)req);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_http_request_form_has_value(void* req, long long index)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( index < 0 ) { return 0; }\n", out);
    xl6OutputPuts("    return xrtWebRequestFormHasValueAt((const xwebrequest*)req, (size_t)index) ? 1 : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_form_name(void* req, long long index)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( index < 0 ) { return xrtCopyStr((str)\"\", 0); }\n", out);
    xl6OutputPuts("    return xrtWebRequestFormNameCopyAt((const xwebrequest*)req, (size_t)index);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_form_value_at(void* req, long long index)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( index < 0 ) { return xrtCopyStr((str)\"\", 0); }\n", out);
    xl6OutputPuts("    return xrtWebRequestFormValueCopyAt((const xwebrequest*)req, (size_t)index);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_form_value(void* req, const char* name)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebRequestFormValueCopy((const xwebrequest*)req, name);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_http_request_is_multipart_form(void* req)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebRequestIsMultipartForm((const xwebrequest*)req) ? 1 : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_http_request_multipart_count(void* req)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return (long long)xrtWebRequestMultipartCount((const xwebrequest*)req);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_http_request_multipart_is_file(void* req, long long index)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( index < 0 ) { return 0; }\n", out);
    xl6OutputPuts("    return xrtWebRequestMultipartIsFileAt((const xwebrequest*)req, (size_t)index) ? 1 : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_multipart_name(void* req, long long index)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( index < 0 ) { return xrtCopyStr((str)\"\", 0); }\n", out);
    xl6OutputPuts("    return xrtWebRequestMultipartNameCopyAt((const xwebrequest*)req, (size_t)index);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_multipart_file_name(void* req, long long index)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( index < 0 ) { return xrtCopyStr((str)\"\", 0); }\n", out);
    xl6OutputPuts("    return xrtWebRequestMultipartFileNameCopyAt((const xwebrequest*)req, (size_t)index);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_multipart_content_type(void* req, long long index)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( index < 0 ) { return xrtCopyStr((str)\"\", 0); }\n", out);
    xl6OutputPuts("    return xrtWebRequestMultipartContentTypeCopyAt((const xwebrequest*)req, (size_t)index);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_http_request_multipart_size(void* req, long long index)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( index < 0 ) { return 0; }\n", out);
    xl6OutputPuts("    return (long long)xrtWebRequestMultipartBodySizeAt((const xwebrequest*)req, (size_t)index);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_multipart_text(void* req, long long index)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( index < 0 ) { return xrtCopyStr((str)\"\", 0); }\n", out);
    xl6OutputPuts("    return xrtWebRequestMultipartBodyTextCopyAt((const xwebrequest*)req, (size_t)index);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_multipart_bytes(void* req, long long index)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const void* data = 0;\n", out);
    xl6OutputPuts("    size_t len = 0u;\n", out);
    xl6OutputPuts("    if ( index < 0 || !xrtWebRequestMultipartBodyViewAt((const xwebrequest*)req, (size_t)index, &data, &len) ) { return xl6_xrt_buffer_from_view_bytes(0, 0u); }\n", out);
    xl6OutputPuts("    return xl6_xrt_buffer_from_view_bytes(data, len);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_multipart_value(void* req, const char* name)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebRequestMultipartValueCopy((const xwebrequest*)req, name);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_local_addr(void* req)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebRequestLocalAddrCopy((const xwebrequest*)req);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_remote_addr(void* req)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebRequestRemoteAddrCopy((const xwebrequest*)req);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_request_tls_sni(void* req)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebRequestTlsSNICopy((const xwebrequest*)req);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_http_request_local_port(void* req)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return (long long)xrtWebRequestLocalPort((const xwebrequest*)req);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_http_request_remote_port(void* req)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return (long long)xrtWebRequestRemotePort((const xwebrequest*)req);\n", out);
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

    xl6OutputPuts("int xl6_xrt_http_response_cookie(void* resp, const char* name, const char* value, const char* path, long long maxAge, long long sameSite, unsigned int flags)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebResponseCookie((xwebresponse*)resp, name, value, path, (int32_t)maxAge, (uint32_t)sameSite, (uint32_t)flags);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_http_response_delete_cookie(void* resp, const char* name, const char* path)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebResponseDeleteCookie((xwebresponse*)resp, name, path);\n", out);
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

    xl6OutputPuts("int xl6_xrt_http_response_start(void* resp)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebResponseStart((xwebresponse*)resp);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_http_response_send(void* resp, void* data, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebResponseSend((xwebresponse*)resp, data, size);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_http_response_write(void* resp, const char* text)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebResponseWriteText((xwebresponse*)resp, text);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_http_response_end(void* resp)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtWebResponseEnd((xwebresponse*)resp);\n", out);
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

    xl6OutputPuts("void* xl6_xrt_http_body_chunk_text(void* data, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( data == 0 && size > 0u ) { return xrtCopyStr((char*)\"\", 0); }\n", out);
    xl6OutputPuts("    return xrtCopyStr((const char*)(data != 0 ? data : \"\"), size);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_body_chunk_bytes(void* data, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( data == 0 && size > 0u ) { return xl6_xrt_buffer_from_view_bytes(0, 0u); }\n", out);
    xl6OutputPuts("    return xl6_xrt_buffer_from_view_bytes(data, size);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_server_create(const char* host, int port, uint32_t backlog, uint32_t recvLimit, uint32_t bodyLimit, uint32_t headerLimit, uint32_t headerBytesLimit, uint32_t workerCount, uint32_t headerTimeoutMs, uint32_t bodyTimeoutMs, uint32_t idleTimeoutMs, uint32_t writeTimeoutMs, uint32_t multipartPartLimit, uint32_t multipartPartSizeLimit)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* self;\n", out);
    xl6OutputPuts("    xwebconfig cfg;\n", out);
    xl6OutputPuts("    const char* bindHost;\n", out);
    xl6OutputPuts("    if ( port < 0 || port > 65535 ) { return 0; }\n", out);
    xl6OutputPuts("    xrtWebConfigInit(&cfg);\n", out);
    xl6OutputPuts("    bindHost = (host != 0 && host[0] != 0) ? host : \"127.0.0.1\";\n", out);
    xl6OutputPuts("    if ( xrtNetAddrParse(&cfg.tBindAddr, bindHost, (uint16_t)port) != 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( backlog > 0 ) { cfg.iBacklog = backlog; }\n", out);
    xl6OutputPuts("    if ( recvLimit > 0 ) { cfg.iRecvLimit = recvLimit; }\n", out);
    xl6OutputPuts("    if ( bodyLimit > 0 ) { cfg.iBodyLimit = bodyLimit; }\n", out);
    xl6OutputPuts("    if ( headerLimit > 0 ) { cfg.iHeaderLimit = headerLimit; }\n", out);
    xl6OutputPuts("    if ( headerBytesLimit > 0 ) { cfg.iHeaderBytesLimit = headerBytesLimit; }\n", out);
    xl6OutputPuts("    if ( workerCount > 0 ) { cfg.iWorkerCount = workerCount; }\n", out);
    xl6OutputPuts("    if ( headerTimeoutMs > 0 ) { cfg.iHeaderTimeoutMs = headerTimeoutMs; }\n", out);
    xl6OutputPuts("    if ( bodyTimeoutMs > 0 ) { cfg.iBodyTimeoutMs = bodyTimeoutMs; }\n", out);
    xl6OutputPuts("    if ( idleTimeoutMs > 0 ) { cfg.iIdleTimeoutMs = idleTimeoutMs; }\n", out);
    xl6OutputPuts("    if ( writeTimeoutMs > 0 ) { cfg.iWriteTimeoutMs = writeTimeoutMs; }\n", out);
    xl6OutputPuts("    cfg.iMultipartPartLimit = multipartPartLimit;\n", out);
    xl6OutputPuts("    cfg.iMultipartPartSizeLimit = multipartPartSizeLimit;\n", out);
    xl6OutputPuts("    self = (XL6_Http_Server*)xrtMalloc(sizeof(XL6_Http_Server));\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    memset(self, 0, sizeof(*self));\n", out);
    xl6OutputPuts("    self->server = xrtWebServerCreate(0, &cfg);\n", out);
    xl6OutputPuts("    if ( self->server == 0 ) { xrtFree(self); return 0; }\n", out);
    xl6OutputPuts("    return self;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_server_create_tls(const char* certFile, const char* keyFile, const char* host, int port, uint32_t backlog, uint32_t recvLimit, uint32_t bodyLimit, uint32_t headerLimit, uint32_t headerBytesLimit, uint32_t workerCount, uint32_t headerTimeoutMs, uint32_t bodyTimeoutMs, uint32_t idleTimeoutMs, uint32_t writeTimeoutMs, uint32_t multipartPartLimit, uint32_t multipartPartSizeLimit)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* self;\n", out);
    xl6OutputPuts("    xwebconfig cfg;\n", out);
    xl6OutputPuts("    xtlsconfig tlsCfg;\n", out);
    xl6OutputPuts("    const char* bindHost;\n", out);
    xl6OutputPuts("    if ( certFile == 0 || certFile[0] == 0 || keyFile == 0 || keyFile[0] == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( port < 0 || port > 65535 ) { return 0; }\n", out);
    xl6OutputPuts("    xrtWebConfigInit(&cfg);\n", out);
    xl6OutputPuts("    memset(&tlsCfg, 0, sizeof(tlsCfg));\n", out);
    xl6OutputPuts("    tlsCfg.sCertFile = certFile;\n", out);
    xl6OutputPuts("    tlsCfg.sKeyFile = keyFile;\n", out);
    xl6OutputPuts("    cfg.pTlsConfig = &tlsCfg;\n", out);
    xl6OutputPuts("    bindHost = (host != 0 && host[0] != 0) ? host : \"127.0.0.1\";\n", out);
    xl6OutputPuts("    if ( xrtNetAddrParse(&cfg.tBindAddr, bindHost, (uint16_t)port) != 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( backlog > 0 ) { cfg.iBacklog = backlog; }\n", out);
    xl6OutputPuts("    if ( recvLimit > 0 ) { cfg.iRecvLimit = recvLimit; }\n", out);
    xl6OutputPuts("    if ( bodyLimit > 0 ) { cfg.iBodyLimit = bodyLimit; }\n", out);
    xl6OutputPuts("    if ( headerLimit > 0 ) { cfg.iHeaderLimit = headerLimit; }\n", out);
    xl6OutputPuts("    if ( headerBytesLimit > 0 ) { cfg.iHeaderBytesLimit = headerBytesLimit; }\n", out);
    xl6OutputPuts("    if ( workerCount > 0 ) { cfg.iWorkerCount = workerCount; }\n", out);
    xl6OutputPuts("    if ( headerTimeoutMs > 0 ) { cfg.iHeaderTimeoutMs = headerTimeoutMs; }\n", out);
    xl6OutputPuts("    if ( bodyTimeoutMs > 0 ) { cfg.iBodyTimeoutMs = bodyTimeoutMs; }\n", out);
    xl6OutputPuts("    if ( idleTimeoutMs > 0 ) { cfg.iIdleTimeoutMs = idleTimeoutMs; }\n", out);
    xl6OutputPuts("    if ( writeTimeoutMs > 0 ) { cfg.iWriteTimeoutMs = writeTimeoutMs; }\n", out);
    xl6OutputPuts("    cfg.iMultipartPartLimit = multipartPartLimit;\n", out);
    xl6OutputPuts("    cfg.iMultipartPartSizeLimit = multipartPartSizeLimit;\n", out);
    xl6OutputPuts("    self = (XL6_Http_Server*)xrtMalloc(sizeof(XL6_Http_Server));\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    memset(self, 0, sizeof(*self));\n", out);
    xl6OutputPuts("    self->server = xrtWebServerCreate(0, &cfg);\n", out);
    xl6OutputPuts("    if ( self->server == 0 ) { xrtFree(self); return 0; }\n", out);
    xl6OutputPuts("    return self;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_app_create(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_App* self = (XL6_Http_App*)xrtMalloc(sizeof(XL6_Http_App));\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    memset(self, 0, sizeof(*self));\n", out);
    xl6OutputPuts("    self->app = xrtWebAppCreate();\n", out);
    xl6OutputPuts("    if ( self->app == 0 ) { xrtFree(self); return 0; }\n", out);
    xl6OutputPuts("    return self;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void xl6_xrt_http_app_destroy(void* handle)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_App* self = (XL6_Http_App*)handle;\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return; }\n", out);
    xl6OutputPuts("    if ( self->app != 0 ) { xrtWebAppDestroy(self->app); self->app = 0; }\n", out);
    xl6OutputPuts("    xrtFree(self);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_xrt_http_app_add_route(void* handle, uint32_t methodMask, const char* pattern, XL6_Function handler, int objectHandler)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_App* self = (XL6_Http_App*)handle;\n", out);
    xl6OutputPuts("    XL6_Http_Route* route;\n", out);
    xl6OutputPuts("    if ( self == 0 || self->app == 0 || pattern == 0 || (handler.invoke_var == 0 && handler.invoke_xcall == 0) ) { return false; }\n", out);
    xl6OutputPuts("    route = (XL6_Http_Route*)xrtMalloc(sizeof(XL6_Http_Route));\n", out);
    xl6OutputPuts("    if ( route == 0 ) { return false; }\n", out);
    xl6OutputPuts("    memset(route, 0, sizeof(*route));\n", out);
    xl6OutputPuts("    route->handler = handler;\n", out);
    xl6OutputPuts("    route->objectHandler = objectHandler;\n", out);
    xl6OutputPuts("    xl6_http_server_retain_function(&route->handler);\n", out);
    xl6OutputPuts("    if ( !xrtWebAppRouteEx(self->app, methodMask, pattern, objectHandler ? xl6_http_server_on_route_object : xl6_http_server_on_route, route, xl6_http_server_free_route) ) {\n", out);
    xl6OutputPuts("        xl6_http_server_release_function(&route->handler);\n", out);
    xl6OutputPuts("        xrtFree(route);\n", out);
    xl6OutputPuts("        return false;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return true;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_app_route(void* handle, const char* method, const char* pattern, XL6_Function handler)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    uint32_t methodMask = xrtWebMethodMask(method);\n", out);
    xl6OutputPuts("    if ( methodMask == 0 ) { return false; }\n", out);
    xl6OutputPuts("    return xl6_xrt_http_app_add_route(handle, methodMask, pattern, handler, 0);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_app_route_request(void* handle, const char* method, const char* pattern, XL6_Function handler)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    uint32_t methodMask = xrtWebMethodMask(method);\n", out);
    xl6OutputPuts("    if ( methodMask == 0 ) { return false; }\n", out);
    xl6OutputPuts("    return xl6_xrt_http_app_add_route(handle, methodMask, pattern, handler, 1);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_app_get(void* handle, const char* pattern, XL6_Function handler)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_xrt_http_app_add_route(handle, XL6_XWEB_METHOD_GET, pattern, handler, 0);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_app_post(void* handle, const char* pattern, XL6_Function handler)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_xrt_http_app_add_route(handle, XL6_XWEB_METHOD_POST, pattern, handler, 0);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_app_any(void* handle, const char* pattern, XL6_Function handler)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_xrt_http_app_add_route(handle, XL6_XWEB_METHOD_ANY, pattern, handler, 0);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_app_get_request(void* handle, const char* pattern, XL6_Function handler)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_xrt_http_app_add_route(handle, XL6_XWEB_METHOD_GET, pattern, handler, 1);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_app_post_request(void* handle, const char* pattern, XL6_Function handler)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_xrt_http_app_add_route(handle, XL6_XWEB_METHOD_POST, pattern, handler, 1);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_app_any_request(void* handle, const char* pattern, XL6_Function handler)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_xrt_http_app_add_route(handle, XL6_XWEB_METHOD_ANY, pattern, handler, 1);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_xrt_http_app_add_stream_route(void* handle, uint32_t methodMask, const char* pattern, XL6_Function beginHandler, XL6_Function chunkHandler, XL6_Function endHandler)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_App* self = (XL6_Http_App*)handle;\n", out);
    xl6OutputPuts("    XL6_Http_Route* route;\n", out);
    xl6OutputPuts("    if ( self == 0 || self->app == 0 || pattern == 0 ) { return false; }\n", out);
    xl6OutputPuts("    if ( (beginHandler.invoke_var == 0 && beginHandler.invoke_xcall == 0) || (chunkHandler.invoke_var == 0 && chunkHandler.invoke_xcall == 0) || (endHandler.invoke_var == 0 && endHandler.invoke_xcall == 0) ) { return false; }\n", out);
    xl6OutputPuts("    route = (XL6_Http_Route*)xrtMalloc(sizeof(XL6_Http_Route));\n", out);
    xl6OutputPuts("    if ( route == 0 ) { return false; }\n", out);
    xl6OutputPuts("    memset(route, 0, sizeof(*route));\n", out);
    xl6OutputPuts("    route->beginHandler = beginHandler;\n", out);
    xl6OutputPuts("    route->chunkHandler = chunkHandler;\n", out);
    xl6OutputPuts("    route->endHandler = endHandler;\n", out);
    xl6OutputPuts("    xl6_http_server_retain_function(&route->beginHandler);\n", out);
    xl6OutputPuts("    xl6_http_server_retain_function(&route->chunkHandler);\n", out);
    xl6OutputPuts("    xl6_http_server_retain_function(&route->endHandler);\n", out);
    xl6OutputPuts("    if ( !xrtWebAppStreamRouteEx(self->app, methodMask, pattern, xl6_http_server_on_body_begin, xl6_http_server_on_body_chunk, xl6_http_server_on_body_end, route, xl6_http_server_free_route) ) {\n", out);
    xl6OutputPuts("        xl6_http_server_free_route(route);\n", out);
    xl6OutputPuts("        return false;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return true;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_app_route_stream_request(void* handle, const char* method, const char* pattern, XL6_Function beginHandler, XL6_Function chunkHandler, XL6_Function endHandler)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    uint32_t methodMask = xrtWebMethodMask(method);\n", out);
    xl6OutputPuts("    if ( methodMask == 0 ) { return false; }\n", out);
    xl6OutputPuts("    return xl6_xrt_http_app_add_stream_route(handle, methodMask, pattern, beginHandler, chunkHandler, endHandler);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_app_post_stream_request(void* handle, const char* pattern, XL6_Function beginHandler, XL6_Function chunkHandler, XL6_Function endHandler)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_xrt_http_app_add_stream_route(handle, XL6_XWEB_METHOD_POST, pattern, beginHandler, chunkHandler, endHandler);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_app_any_stream_request(void* handle, const char* pattern, XL6_Function beginHandler, XL6_Function chunkHandler, XL6_Function endHandler)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_xrt_http_app_add_stream_route(handle, XL6_XWEB_METHOD_ANY, pattern, beginHandler, chunkHandler, endHandler);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_app_static(void* handle, const char* mount, const char* root)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_App* self = (XL6_Http_App*)handle;\n", out);
    xl6OutputPuts("    if ( self == 0 || self->app == 0 || mount == 0 || root == 0 ) { return false; }\n", out);
    xl6OutputPuts("    return xrtWebAppStaticDefault(self->app, mount, root);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_app_static_ex(void* handle, const char* mount, const char* root, const char* indexNames, const char* cacheControl, size_t chunkSize, uint32_t flags)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_App* self = (XL6_Http_App*)handle;\n", out);
    xl6OutputPuts("    if ( self == 0 || self->app == 0 || mount == 0 || root == 0 ) { return false; }\n", out);
    xl6OutputPuts("    return xrtWebAppStaticEx(self->app, mount, root, indexNames, cacheControl, chunkSize, flags);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_app_error(void* handle, XL6_Function handler)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_App* self = (XL6_Http_App*)handle;\n", out);
    xl6OutputPuts("    XL6_Http_Route* route;\n", out);
    xl6OutputPuts("    if ( self == 0 || self->app == 0 || (handler.invoke_var == 0 && handler.invoke_xcall == 0) ) { return false; }\n", out);
    xl6OutputPuts("    route = (XL6_Http_Route*)xrtMalloc(sizeof(XL6_Http_Route));\n", out);
    xl6OutputPuts("    if ( route == 0 ) { return false; }\n", out);
    xl6OutputPuts("    memset(route, 0, sizeof(*route));\n", out);
    xl6OutputPuts("    route->handler = handler;\n", out);
    xl6OutputPuts("    xl6_http_server_retain_function(&route->handler);\n", out);
    xl6OutputPuts("    if ( !xrtWebAppError(self->app, xl6_http_server_on_error, route, xl6_http_server_free_route) ) {\n", out);
    xl6OutputPuts("        xl6_http_server_release_function(&route->handler);\n", out);
    xl6OutputPuts("        xrtFree(route);\n", out);
    xl6OutputPuts("        return false;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return true;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_server_set_app(void* serverHandle, void* appHandle)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* server = (XL6_Http_Server*)serverHandle;\n", out);
    xl6OutputPuts("    XL6_Http_App* app = (XL6_Http_App*)appHandle;\n", out);
    xl6OutputPuts("    if ( server == 0 || server->server == 0 || app == 0 || app->app == 0 ) { return false; }\n", out);
    xl6OutputPuts("    return xrtWebServerSetApp(server->server, app->app);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_server_reload_app(void* serverHandle, void* appHandle)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* server = (XL6_Http_Server*)serverHandle;\n", out);
    xl6OutputPuts("    XL6_Http_App* app = (XL6_Http_App*)appHandle;\n", out);
    xl6OutputPuts("    if ( server == 0 || server->server == 0 || app == 0 || app->app == 0 ) { return false; }\n", out);
    xl6OutputPuts("    return xrtWebServerReloadApp(server->server, app->app);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_server_host(void* serverHandle, const char* host, void* appHandle)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* server = (XL6_Http_Server*)serverHandle;\n", out);
    xl6OutputPuts("    XL6_Http_App* app = (XL6_Http_App*)appHandle;\n", out);
    xl6OutputPuts("    if ( server == 0 || server->server == 0 || app == 0 || app->app == 0 || host == 0 ) { return false; }\n", out);
    xl6OutputPuts("    return xrtWebServerHost(server->server, host, app->app);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_server_remove_host(void* serverHandle, const char* host)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* server = (XL6_Http_Server*)serverHandle;\n", out);
    xl6OutputPuts("    if ( server == 0 || server->server == 0 || host == 0 ) { return false; }\n", out);
    xl6OutputPuts("    return xrtWebServerRemoveHost(server->server, host);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_server_tls_host(void* serverHandle, const char* host, const char* certFile, const char* keyFile)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* server = (XL6_Http_Server*)serverHandle;\n", out);
    xl6OutputPuts("    if ( server == 0 || server->server == 0 || host == 0 || certFile == 0 || keyFile == 0 ) { return false; }\n", out);
    xl6OutputPuts("    return xrtWebServerTlsHost(server->server, host, certFile, keyFile);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_server_remove_tls_host(void* serverHandle, const char* host)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* server = (XL6_Http_Server*)serverHandle;\n", out);
    xl6OutputPuts("    if ( server == 0 || server->server == 0 || host == 0 ) { return false; }\n", out);
    xl6OutputPuts("    return xrtWebServerRemoveTlsHost(server->server, host);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_server_error(void* handle, XL6_Function handler)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* self = (XL6_Http_Server*)handle;\n", out);
    xl6OutputPuts("    XL6_Http_Route* route;\n", out);
    xl6OutputPuts("    if ( self == 0 || self->server == 0 || (handler.invoke_var == 0 && handler.invoke_xcall == 0) ) { return false; }\n", out);
    xl6OutputPuts("    route = (XL6_Http_Route*)xrtMalloc(sizeof(XL6_Http_Route));\n", out);
    xl6OutputPuts("    if ( route == 0 ) { return false; }\n", out);
    xl6OutputPuts("    memset(route, 0, sizeof(*route));\n", out);
    xl6OutputPuts("    route->handler = handler;\n", out);
    xl6OutputPuts("    xl6_http_server_retain_function(&route->handler);\n", out);
    xl6OutputPuts("    if ( !xrtWebServerError(self->server, xl6_http_server_on_error, route, xl6_http_server_free_route) ) {\n", out);
    xl6OutputPuts("        xl6_http_server_release_function(&route->handler);\n", out);
    xl6OutputPuts("        xrtFree(route);\n", out);
    xl6OutputPuts("        return false;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return true;\n", out);
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
    xl6OutputPuts("    if ( !xrtWebServerRouteEx(self->server, methodMask, pattern, objectHandler ? xl6_http_server_on_route_object : xl6_http_server_on_route, route, xl6_http_server_free_route) ) {\n", out);
    xl6OutputPuts("        xl6_http_server_release_function(&route->handler);\n", out);
    xl6OutputPuts("        xrtFree(route);\n", out);
    xl6OutputPuts("        return false;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return true;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_server_route(void* handle, const char* method, const char* pattern, XL6_Function handler)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    uint32_t methodMask = xrtWebMethodMask(method);\n", out);
    xl6OutputPuts("    if ( methodMask == 0 ) { return false; }\n", out);
    xl6OutputPuts("    return xl6_xrt_http_server_add_route(handle, methodMask, pattern, handler, 0);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_server_route_request(void* handle, const char* method, const char* pattern, XL6_Function handler)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    uint32_t methodMask = xrtWebMethodMask(method);\n", out);
    xl6OutputPuts("    if ( methodMask == 0 ) { return false; }\n", out);
    xl6OutputPuts("    return xl6_xrt_http_server_add_route(handle, methodMask, pattern, handler, 1);\n", out);
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

    xl6OutputPuts("static bool xl6_xrt_http_server_add_stream_route(void* handle, uint32_t methodMask, const char* pattern, XL6_Function beginHandler, XL6_Function chunkHandler, XL6_Function endHandler)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* self = (XL6_Http_Server*)handle;\n", out);
    xl6OutputPuts("    XL6_Http_Route* route;\n", out);
    xl6OutputPuts("    if ( self == 0 || self->server == 0 || pattern == 0 ) { return false; }\n", out);
    xl6OutputPuts("    if ( (beginHandler.invoke_var == 0 && beginHandler.invoke_xcall == 0) || (chunkHandler.invoke_var == 0 && chunkHandler.invoke_xcall == 0) || (endHandler.invoke_var == 0 && endHandler.invoke_xcall == 0) ) { return false; }\n", out);
    xl6OutputPuts("    route = (XL6_Http_Route*)xrtMalloc(sizeof(XL6_Http_Route));\n", out);
    xl6OutputPuts("    if ( route == 0 ) { return false; }\n", out);
    xl6OutputPuts("    memset(route, 0, sizeof(*route));\n", out);
    xl6OutputPuts("    route->beginHandler = beginHandler;\n", out);
    xl6OutputPuts("    route->chunkHandler = chunkHandler;\n", out);
    xl6OutputPuts("    route->endHandler = endHandler;\n", out);
    xl6OutputPuts("    xl6_http_server_retain_function(&route->beginHandler);\n", out);
    xl6OutputPuts("    xl6_http_server_retain_function(&route->chunkHandler);\n", out);
    xl6OutputPuts("    xl6_http_server_retain_function(&route->endHandler);\n", out);
    xl6OutputPuts("    if ( !xrtWebServerStreamRouteEx(self->server, methodMask, pattern, xl6_http_server_on_body_begin, xl6_http_server_on_body_chunk, xl6_http_server_on_body_end, route, xl6_http_server_free_route) ) {\n", out);
    xl6OutputPuts("        xl6_http_server_free_route(route);\n", out);
    xl6OutputPuts("        return false;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return true;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_server_route_stream_request(void* handle, const char* method, const char* pattern, XL6_Function beginHandler, XL6_Function chunkHandler, XL6_Function endHandler)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    uint32_t methodMask = xrtWebMethodMask(method);\n", out);
    xl6OutputPuts("    if ( methodMask == 0 ) { return false; }\n", out);
    xl6OutputPuts("    return xl6_xrt_http_server_add_stream_route(handle, methodMask, pattern, beginHandler, chunkHandler, endHandler);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_server_post_stream_request(void* handle, const char* pattern, XL6_Function beginHandler, XL6_Function chunkHandler, XL6_Function endHandler)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_xrt_http_server_add_stream_route(handle, XL6_XWEB_METHOD_POST, pattern, beginHandler, chunkHandler, endHandler);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_server_any_stream_request(void* handle, const char* pattern, XL6_Function beginHandler, XL6_Function chunkHandler, XL6_Function endHandler)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_xrt_http_server_add_stream_route(handle, XL6_XWEB_METHOD_ANY, pattern, beginHandler, chunkHandler, endHandler);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_server_static(void* handle, const char* mount, const char* root)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* self = (XL6_Http_Server*)handle;\n", out);
    xl6OutputPuts("    if ( self == 0 || self->server == 0 || mount == 0 || root == 0 ) { return false; }\n", out);
    xl6OutputPuts("    return xrtWebServerStaticDefault(self->server, mount, root);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_server_static_ex(void* handle, const char* mount, const char* root, const char* indexNames, const char* cacheControl, size_t chunkSize, uint32_t flags)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* self = (XL6_Http_Server*)handle;\n", out);
    xl6OutputPuts("    if ( self == 0 || self->server == 0 || mount == 0 || root == 0 ) { return false; }\n", out);
    xl6OutputPuts("    return xrtWebServerStaticEx(self->server, mount, root, indexNames, cacheControl, chunkSize, flags);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_server_start(void* handle)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* self = (XL6_Http_Server*)handle;\n", out);
    xl6OutputPuts("    if ( self == 0 || self->server == 0 ) { return false; }\n", out);
    xl6OutputPuts("    if ( !xrtWebServerStartOk(self->server) ) { return false; }\n", out);
    xl6OutputPuts("    self->running = 1;\n", out);
    xl6OutputPuts("    return true;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_http_server_port(void* handle)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* self = (XL6_Http_Server*)handle;\n", out);
    xl6OutputPuts("    if ( self == 0 || self->server == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    return (int)xrtWebServerPort(self->server);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_http_server_request_count(void* handle)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* self = (XL6_Http_Server*)handle;\n", out);
    xl6OutputPuts("    if ( self == 0 || self->server == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    return (int)xrtWebServerRequestCount(self->server);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_http_server_connection_count(void* handle)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* self = (XL6_Http_Server*)handle;\n", out);
    xl6OutputPuts("    if ( self == 0 || self->server == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    return (int)xrtWebServerConnectionCount(self->server);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void xl6_xrt_http_server_stop(void* handle)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* self = (XL6_Http_Server*)handle;\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return; }\n", out);
    xl6OutputPuts("    if ( self->server != 0 ) { xrtWebServerStop(self->server); }\n", out);
    xl6OutputPuts("    self->running = 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_http_server_drain(void* handle, long long timeoutMs)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Http_Server* self = (XL6_Http_Server*)handle;\n", out);
    xl6OutputPuts("    uint32_t waitMs;\n", out);
    xl6OutputPuts("    if ( self == 0 || self->server == 0 ) { return true; }\n", out);
    xl6OutputPuts("    waitMs = timeoutMs < 0 ? 0xffffffffu : (uint32_t)timeoutMs;\n", out);
    xl6OutputPuts("    if ( xrtWebServerDrain(self->server, waitMs) ) {\n", out);
    xl6OutputPuts("        self->running = 0;\n", out);
    xl6OutputPuts("        return true;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return false;\n", out);
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
