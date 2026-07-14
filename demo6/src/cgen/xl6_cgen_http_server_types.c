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
    xl6OutputPuts("typedef struct xrt_tls_session xtlssession;\n", out);
    xl6OutputPuts("typedef struct xrt_tls_resume xtlsresume;\n", out);
    xl6OutputPuts("#ifndef XL6_XRT_NETADDR_TYPE\n", out);
    xl6OutputPuts("#define XL6_XRT_NETADDR_TYPE\n", out);
    xl6OutputPuts("typedef struct { uint16_t iFamily; uint16_t iPort; uint32_t iScopeId; uint8_t aAddr[16]; } xnetaddr;\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("#ifndef XL6_XRT_TLS_CONFIG_TYPE\n", out);
    xl6OutputPuts("#define XL6_XRT_TLS_CONFIG_TYPE\n", out);
    xl6OutputPuts("typedef struct xtlsconfig { const char* sCertFile; const char* sKeyFile; const char* sCaFile; const char* sCrlFile; const void* pCertData; size_t iCertDataLen; const void* pKeyData; size_t iKeyDataLen; const void* pCaData; size_t iCaDataLen; const void* pCrlData; size_t iCrlDataLen; const char* sHostName; bool bVerifyPeer; void (*OnSNI)(xtlssession* pSession, const char* sHostName, void* pUserData); void* pSNIUserData; bool bAllowTLS12Ed25519; uint16_t iMaxVersion; const xtlsresume* pResume; volatile long iDataLock; } xtlsconfig;\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("typedef struct { xnetaddr tBindAddr; uint32_t iFlags; uint32_t iBacklog; uint32_t iRecvLimit; uint32_t iBodyLimit; uint32_t iHeaderLimit; uint32_t iHeaderBytesLimit; uint32_t iHeaderTimeoutMs; uint32_t iBodyTimeoutMs; uint32_t iIdleTimeoutMs; uint32_t iWriteTimeoutMs; uint32_t iMultipartPartLimit; uint32_t iMultipartPartSizeLimit; uint32_t iWorkerCount; const xtlsconfig* pTlsConfig; } xwebconfig;\n", out);
}
