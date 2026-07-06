static void xl6CgenEmitHttpClientRuntime(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_HTTP_CLIENT_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_HTTP_CLIENT_RUNTIME\n", out);
    xl6OutputPuts("#include <stdint.h>\n", out);
    xl6OutputPuts("#include <string.h>\n\n", out);

    xl6OutputPuts("typedef struct xrt_http_request xhttprequest;\n", out);
    xl6OutputPuts("typedef struct xrt_http_response xhttpresponse;\n", out);
    xl6OutputPuts("void xrtFree(void* pMem);\n", out);
    xl6OutputPuts("char* xrtCopyStr(const char* sText, size_t iSize);\n", out);
    xl6OutputPuts("xhttprequest* xrtHttpRequestCreate(void);\n", out);
    xl6OutputPuts("void xrtHttpRequestDestroy(xhttprequest* pReq);\n", out);
    xl6OutputPuts("bool xrtHttpRequestSetMethod(xhttprequest* pReq, const char* sMethod);\n", out);
    xl6OutputPuts("bool xrtHttpRequestSetURL(xhttprequest* pReq, const char* sURL);\n", out);
    xl6OutputPuts("bool xrtHttpRequestSetHeader(xhttprequest* pReq, const char* sName, const char* sValue);\n", out);
    xl6OutputPuts("bool xrtHttpRequestSetBodyCopy(xhttprequest* pReq, const void* pData, size_t iLen, const char* sContentType);\n", out);
    xl6OutputPuts("void xrtHttpRequestSetTimeout(xhttprequest* pReq, uint32_t iTimeoutMs);\n", out);
    xl6OutputPuts("void xrtHttpRequestSetIdleTimeout(xhttprequest* pReq, uint32_t iTimeoutMs);\n", out);
    xl6OutputPuts("void xrtHttpRequestSetVerifyPeer(xhttprequest* pReq, bool bVerifyPeer);\n", out);
    xl6OutputPuts("xhttpresponse* xrtHttpExecuteSyncDefault(const xhttprequest* pReq);\n", out);
    xl6OutputPuts("void xrtHttpResponseDestroy(xhttpresponse* pResp);\n", out);
    xl6OutputPuts("uint32_t xrtHttpResponseStatusCode(const xhttpresponse* pResp);\n", out);
    xl6OutputPuts("const char* xrtHttpResponseReason(const xhttpresponse* pResp);\n", out);
    xl6OutputPuts("const char* xrtHttpResponseHeader(const xhttpresponse* pResp, const char* sName);\n", out);
    xl6OutputPuts("int64_t xrtHttpResponseContentLength(const xhttpresponse* pResp);\n", out);
    xl6OutputPuts("const void* xrtHttpResponseBody(const xhttpresponse* pResp, size_t* pLen);\n", out);
    xl6OutputPuts("char* xrtHttpResponseBodyTextCopy(const xhttpresponse* pResp);\n\n", out);

    xl6OutputPuts("static uint32_t xl6_http_client_timeout(long long value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( value <= 0 ) { return 0u; }\n", out);
    xl6OutputPuts("    if ( value > 4294967295LL ) { return 4294967295u; }\n", out);
    xl6OutputPuts("    return (uint32_t)value;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_http_client_copy_text(const char* text)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const char* src = text != 0 ? text : \"\";\n", out);
    xl6OutputPuts("    return xrtCopyStr(src, strlen(src));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_client_request_create(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xrtHttpRequestCreate();\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_client_request_create_url(const char* url)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    void* req = xrtHttpRequestCreate();\n", out);
    xl6OutputPuts("    if ( req == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( !xrtHttpRequestSetURL(req, url != 0 ? url : \"\") ) {\n", out);
    xl6OutputPuts("        xrtHttpRequestDestroy(req);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return req;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_http_client_request_destroy(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xrtHttpRequestDestroy(self);\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_http_client_request_method(void* self, const char* method)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    return xrtHttpRequestSetMethod(self, method != 0 ? method : \"\") ? 1 : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_http_client_request_url(void* self, const char* url)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    return xrtHttpRequestSetURL(self, url != 0 ? url : \"\") ? 1 : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_http_client_request_header(void* self, const char* name, const char* value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    return xrtHttpRequestSetHeader(self, name != 0 ? name : \"\", value != 0 ? value : \"\") ? 1 : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_http_client_request_body(void* self, const char* data, size_t size, const char* contentType)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    if ( data == 0 ) { data = \"\"; size = 0u; }\n", out);
    xl6OutputPuts("    return xrtHttpRequestSetBodyCopy(self, data, size, contentType != 0 ? contentType : \"\") ? 1 : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void xl6_xrt_http_client_request_timeout(void* self, long long timeoutMs)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xrtHttpRequestSetTimeout(self, xl6_http_client_timeout(timeoutMs));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void xl6_xrt_http_client_request_idle_timeout(void* self, long long timeoutMs)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xrtHttpRequestSetIdleTimeout(self, xl6_http_client_timeout(timeoutMs));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void xl6_xrt_http_client_request_verify_peer(void* self, int value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xrtHttpRequestSetVerifyPeer(self, value ? true : false);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_client_request_execute(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    return xrtHttpExecuteSyncDefault(self);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_client_response_wrap(void* handle)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return handle;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_http_client_response_destroy(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xrtHttpResponseDestroy(self);\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_http_client_response_status(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return self != 0 ? (long long)xrtHttpResponseStatusCode(self) : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_client_response_reason(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_http_client_copy_text(self != 0 ? xrtHttpResponseReason(self) : \"\");\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_client_response_header(void* self, const char* name)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_http_client_copy_text(self != 0 ? xrtHttpResponseHeader(self, name != 0 ? name : \"\") : \"\");\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_http_client_response_content_length(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return self != 0 ? (long long)xrtHttpResponseContentLength(self) : -1LL;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_client_response_body_text(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    char* text = self != 0 ? xrtHttpResponseBodyTextCopy(self) : 0;\n", out);
    xl6OutputPuts("    return text != 0 ? text : xl6_http_client_copy_text(\"\");\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_client_response_body_data(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t len = 0u;\n", out);
    xl6OutputPuts("    return self != 0 ? (void*)xrtHttpResponseBody(self, &len) : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_http_client_response_body_size(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t len = 0u;\n", out);
    xl6OutputPuts("    (void)(self != 0 ? xrtHttpResponseBody(self, &len) : 0);\n", out);
    xl6OutputPuts("    return (long long)len;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_http_client_response_body_buffer(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t len = 0u;\n", out);
    xl6OutputPuts("    const void* data = self != 0 ? xrtHttpResponseBody(self, &len) : 0;\n", out);
    xl6OutputPuts("    return xl6_xrt_buffer_from_view_bytes(data, len);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("#endif\n\n", out);
}
