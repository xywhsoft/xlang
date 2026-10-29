/* Explicit typed boundaries: direct calls, no erased operation dispatch. */
#include <xl_foreign_graph.h>
#include <xl_foreign_net.h>

#define XMAIL_MANAGED_METHOD(Type,Name,Parameters,Arguments) \
Type Name Parameters { \
    XL6_ForeignGraphScopeV1 scope={0}; \
    if(!xl6ForeignGraphEnterV1(pHandle,&scope))return 0; \
    Type result=__raw_##Name Arguments; \
    xl6ForeignGraphLeaveV1(&scope); \
    return result; \
}

XMAIL_MANAGED_METHOD(long long,xmail_smtp_state,(void* pHandle),(scope.raw))
XMAIL_MANAGED_METHOD(unsigned long long,xmail_smtp_capabilities,(void* pHandle),(scope.raw))
XMAIL_MANAGED_METHOD(unsigned long long,xmail_smtp_size_limit,(void* pHandle),(scope.raw))
XMAIL_MANAGED_METHOD(long long,xmail_smtp_security,(void* pHandle),(scope.raw))
XMAIL_MANAGED_METHOD(bool,xmail_smtp_authenticated,(void* pHandle),(scope.raw))
XMAIL_MANAGED_METHOD(long long,xmail_smtp_reply_code,(void* pHandle),(scope.raw))
XMAIL_MANAGED_METHOD(void*,xmail_smtp_reply_text,(void* pHandle),(scope.raw))
XMAIL_MANAGED_METHOD(bool,xmail_smtp_send,(void* pHandle, cstr sLine, size_t iLineSize, long long iTimeoutMilliseconds),(scope.raw, sLine, iLineSize, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_smtp_receive,(void* pHandle, long long iTimeoutMilliseconds),(scope.raw, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_smtp_command,(void* pHandle, cstr sVerb, size_t iVerbSize, cstr sArguments, size_t iArgumentsSize, long long iTimeoutMilliseconds),(scope.raw, sVerb, iVerbSize, sArguments, iArgumentsSize, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_smtp_mail,(void* pHandle, cstr sPath, size_t iPathSize, cstr sParameters, size_t iParametersSize, long long iTimeoutMilliseconds),(scope.raw, sPath, iPathSize, sParameters, iParametersSize, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_smtp_rcpt,(void* pHandle, cstr sPath, size_t iPathSize, cstr sParameters, size_t iParametersSize, long long iTimeoutMilliseconds),(scope.raw, sPath, iPathSize, sParameters, iParametersSize, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_smtp_data_begin,(void* pHandle, long long iTimeoutMilliseconds),(scope.raw, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_smtp_data_write,(void* pHandle, void* pData, size_t iSize, long long iTimeoutMilliseconds),(scope.raw, pData, iSize, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_smtp_data_write_text,(void* pHandle, cstr sData, size_t iSize, long long iTimeoutMilliseconds),(scope.raw, sData, iSize, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_smtp_data_end,(void* pHandle, long long iTimeoutMilliseconds),(scope.raw, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_smtp_data,(void* pHandle, cstr sMessage, size_t iMessageSize, long long iTimeoutMilliseconds),(scope.raw, sMessage, iMessageSize, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_smtp_bdat_begin,(void* pHandle, unsigned long long iSize, bool Last, long long iTimeoutMilliseconds),(scope.raw, iSize, Last, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_smtp_bdat_write,(void* pHandle, void* pData, size_t iSize, long long iTimeoutMilliseconds),(scope.raw, pData, iSize, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_smtp_bdat_end,(void* pHandle, long long iTimeoutMilliseconds),(scope.raw, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_smtp_bdat,(void* pHandle, void* pData, size_t iSize, bool Last, long long iTimeoutMilliseconds),(scope.raw, pData, iSize, Last, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_smtp_auth,(void* pHandle, unsigned long long iMethod, cstr sUsername, size_t iUsernameSize, cstr sSecret, size_t iSecretSize, cstr sAuthorizationId, size_t iAuthorizationIdSize, bool InitialResponse, bool AllowPlaintext, long long iTimeoutMilliseconds),(scope.raw, iMethod, sUsername, iUsernameSize, sSecret, iSecretSize, sAuthorizationId, iAuthorizationIdSize, InitialResponse, AllowPlaintext, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_smtp_submit,(void* pHandle, xvalue* pValue, long long iTimeoutMilliseconds),(scope.raw, pValue, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_smtp_reset,(void* pHandle, long long iTimeoutMilliseconds),(scope.raw, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_smtp_noop,(void* pHandle, long long iTimeoutMilliseconds),(scope.raw, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_smtp_quit,(void* pHandle, long long iTimeoutMilliseconds),(scope.raw, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_smtp_close,(void* pHandle, long long iTimeoutMilliseconds),(scope.raw, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_smtp_abort,(void* pHandle),(scope.raw))
XMAIL_MANAGED_METHOD(long long,xmail_pop3_state,(void* pHandle),(scope.raw))
XMAIL_MANAGED_METHOD(unsigned long long,xmail_pop3_capabilities,(void* pHandle),(scope.raw))
XMAIL_MANAGED_METHOD(unsigned long long,xmail_pop3_sasl,(void* pHandle),(scope.raw))
XMAIL_MANAGED_METHOD(long long,xmail_pop3_security,(void* pHandle),(scope.raw))
XMAIL_MANAGED_METHOD(bool,xmail_pop3_reply_ok,(void* pHandle),(scope.raw))
XMAIL_MANAGED_METHOD(void*,xmail_pop3_reply_text,(void* pHandle),(scope.raw))
XMAIL_MANAGED_METHOD(void*,xmail_pop3_reply_source,(void* pHandle),(scope.raw))
XMAIL_MANAGED_METHOD(bool,xmail_pop3_send,(void* pHandle, cstr sLine, size_t iLineSize, long long iTimeoutMilliseconds),(scope.raw, sLine, iLineSize, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_pop3_line,(void* pHandle, long long iTimeoutMilliseconds),(scope.raw, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_pop3_receive,(void* pHandle, long long iTimeoutMilliseconds),(scope.raw, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_pop3_command,(void* pHandle, cstr sVerb, size_t iVerbSize, cstr sArguments, size_t iArgumentsSize, long long iTimeoutMilliseconds),(scope.raw, sVerb, iVerbSize, sArguments, iArgumentsSize, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_pop3_begin,(void* pHandle, cstr sVerb, size_t iVerbSize, cstr sArguments, size_t iArgumentsSize, long long iTimeoutMilliseconds),(scope.raw, sVerb, iVerbSize, sArguments, iArgumentsSize, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(long long,xmail_pop3_next,(void* pHandle, long long iTimeoutMilliseconds),(scope.raw, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(void*,xmail_pop3_last_line,(void* pHandle),(scope.raw))
XMAIL_MANAGED_METHOD(bool,xmail_pop3_stat,(void* pHandle, long long iTimeoutMilliseconds),(scope.raw, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_pop3_list,(void* pHandle, unsigned long long iMessage, long long iTimeoutMilliseconds),(scope.raw, iMessage, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_pop3_uidl,(void* pHandle, unsigned long long iMessage, long long iTimeoutMilliseconds),(scope.raw, iMessage, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(unsigned long long,xmail_pop3_first,(void* pHandle),(scope.raw))
XMAIL_MANAGED_METHOD(unsigned long long,xmail_pop3_second,(void* pHandle),(scope.raw))
XMAIL_MANAGED_METHOD(bool,xmail_pop3_list_all,(void* pHandle, long long iTimeoutMilliseconds),(scope.raw, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_pop3_uidl_all,(void* pHandle, long long iTimeoutMilliseconds),(scope.raw, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_pop3_retr,(void* pHandle, unsigned long long iMessage, long long iTimeoutMilliseconds),(scope.raw, iMessage, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_pop3_top,(void* pHandle, unsigned long long iMessage, unsigned long long iLines, long long iTimeoutMilliseconds),(scope.raw, iMessage, iLines, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_pop3_auth,(void* pHandle, unsigned long long iMethod, cstr sUsername, size_t iUsernameSize, cstr sSecret, size_t iSecretSize, cstr sAuthorizationId, size_t iAuthorizationIdSize, bool InitialResponse, bool AllowPlaintext, long long iTimeoutMilliseconds),(scope.raw, iMethod, sUsername, iUsernameSize, sSecret, iSecretSize, sAuthorizationId, iAuthorizationIdSize, InitialResponse, AllowPlaintext, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_pop3_delete,(void* pHandle, unsigned long long iMessage, long long iTimeoutMilliseconds),(scope.raw, iMessage, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_pop3_reset,(void* pHandle, long long iTimeoutMilliseconds),(scope.raw, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_pop3_noop,(void* pHandle, long long iTimeoutMilliseconds),(scope.raw, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_pop3_quit,(void* pHandle, long long iTimeoutMilliseconds),(scope.raw, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_pop3_close,(void* pHandle, long long iTimeoutMilliseconds),(scope.raw, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_pop3_abort,(void* pHandle),(scope.raw))
XMAIL_MANAGED_METHOD(long long,xmail_imap_state,(void* pHandle),(scope.raw))
XMAIL_MANAGED_METHOD(unsigned long long,xmail_imap_capabilities,(void* pHandle),(scope.raw))
XMAIL_MANAGED_METHOD(long long,xmail_imap_security,(void* pHandle),(scope.raw))
XMAIL_MANAGED_METHOD(unsigned long long,xmail_imap_literal_remaining,(void* pHandle),(scope.raw))
XMAIL_MANAGED_METHOD(unsigned long long,xmail_imap_command_limit,(void* pHandle),(scope.raw))
XMAIL_MANAGED_METHOD(unsigned long long,xmail_imap_append_limit,(void* pHandle),(scope.raw))
XMAIL_MANAGED_METHOD(void*,xmail_imap_tag,(void* pHandle),(scope.raw))
XMAIL_MANAGED_METHOD(void*,xmail_imap_last_response_text,(void* pHandle, long long iField),(scope.raw, iField))
XMAIL_MANAGED_METHOD(long long,xmail_imap_last_response_value,(void* pHandle, long long iField),(scope.raw, iField))
XMAIL_MANAGED_METHOD(bool,xmail_imap_send,(void* pHandle, cstr sTag, size_t iTagSize, cstr sCommand, size_t iCommandSize, cstr sArguments, size_t iArgumentsSize, long long iTimeoutMilliseconds),(scope.raw, sTag, iTagSize, sCommand, iCommandSize, sArguments, iArgumentsSize, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_imap_write,(void* pHandle, void* pData, size_t iSize, long long iTimeoutMilliseconds),(scope.raw, pData, iSize, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_imap_continue,(void* pHandle, cstr sData, size_t iDataSize, long long iTimeoutMilliseconds),(scope.raw, sData, iDataSize, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_imap_receive,(void* pHandle, long long iTimeoutMilliseconds),(scope.raw, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_imap_begin,(void* pHandle, cstr sCommand, size_t iCommandSize, cstr sArguments, size_t iArgumentsSize, long long iTimeoutMilliseconds),(scope.raw, sCommand, iCommandSize, sArguments, iArgumentsSize, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(long long,xmail_imap_next,(void* pHandle, long long iTimeoutMilliseconds),(scope.raw, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(long long,xmail_imap_event_value,(void* pHandle, long long iField),(scope.raw, iField))
XMAIL_MANAGED_METHOD(unsigned long long,xmail_imap_event_literal_size,(void* pHandle),(scope.raw))
XMAIL_MANAGED_METHOD(void*,xmail_imap_event_text,(void* pHandle, long long iField),(scope.raw, iField))
XMAIL_MANAGED_METHOD(void*,xmail_imap_read_literal,(void* pHandle, unsigned long long iMaximum, long long iTimeoutMilliseconds),(scope.raw, iMaximum, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_imap_auth,(void* pHandle, unsigned long long iMethod, cstr sUsername, size_t iUsernameSize, cstr sSecret, size_t iSecretSize, cstr sAuthorizationId, size_t iAuthorizationIdSize, bool InitialResponse, bool AllowPlaintext, long long iTimeoutMilliseconds),(scope.raw, iMethod, sUsername, iUsernameSize, sSecret, iSecretSize, sAuthorizationId, iAuthorizationIdSize, InitialResponse, AllowPlaintext, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_imap_simple,(void* pHandle, long long iOperation, long long iTimeoutMilliseconds),(scope.raw, iOperation, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_imap_select,(void* pHandle, cstr sMailbox, size_t iMailboxSize, bool ReadOnly, long long iTimeoutMilliseconds),(scope.raw, sMailbox, iMailboxSize, ReadOnly, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(unsigned long long,xmail_imap_mailbox_value,(void* pHandle, long long iField),(scope.raw, iField))
XMAIL_MANAGED_METHOD(bool,xmail_imap_logout,(void* pHandle, long long iTimeoutMilliseconds),(scope.raw, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_imap_close,(void* pHandle, long long iTimeoutMilliseconds),(scope.raw, iTimeoutMilliseconds))
XMAIL_MANAGED_METHOD(bool,xmail_imap_abort,(void* pHandle),(scope.raw))

#undef XMAIL_MANAGED_METHOD

typedef struct xmail_smtp_open_input {
    const xvalue* engine;
    xvalue* postRelease;
    cstr sHost;
    size_t iHostSize;
    unsigned long long iPort;
    unsigned long long iSecurity;
    cstr sHello;
    size_t iHelloSize;
    unsigned long long iLineLimit;
    long long iTimeoutMilliseconds;
    void* result;
} xmail_smtp_open_input;

static bool xmail_smtp_open_visit(xnetengine* engine,const xtlsclientconfig* tls,void* data)
{
    xmail_smtp_open_input* input=data;
    input->result=__raw_xmail_smtp_open(engine, input->sHost, input->iHostSize, input->iPort, input->iSecurity, input->sHello, input->iHelloSize, input->iLineLimit, input->iTimeoutMilliseconds, (void*)tls, input->postRelease);
    xmail_native_smtp* client=input->result;
    if(!client)return false;
    client->Protocol.EngineOwner=xrtValueRetain((xvalue*)input->engine);
    if(!client->Protocol.EngineOwner){xmail_smtp_raw_destroy(client);input->result=NULL;return false;}
    return true;
}

void* xmail_smtp_open(xvalue* pEngine, cstr sHost, size_t iHostSize, unsigned long long iPort, unsigned long long iSecurity, cstr sHello, size_t iHelloSize, unsigned long long iLineLimit, long long iTimeoutMilliseconds, xvalue* pTlsConfig,xvalue* pPostRelease)
{
    xmail_smtp_open_input input={.engine=pEngine,.postRelease=pPostRelease,.sHost=sHost, .iHostSize=iHostSize, .iPort=iPort, .iSecurity=iSecurity, .sHello=sHello, .iHelloSize=iHelloSize, .iLineLimit=iLineLimit, .iTimeoutMilliseconds=iTimeoutMilliseconds};
    bool ok=xl6ForeignNetClientVisitV1(pEngine,pTlsConfig,xmail_smtp_open_visit,&input);
    if(!ok&&input.result){
        xerror* error=xrtTakeError();xmail_smtp_raw_destroy(input.result);
        xrtClearError();xrtSetErrorTake(error);input.result=NULL;
    }
    return ok?input.result:NULL;
}

static bool xmail_smtp_trace(const void* data,xrtownershipvisitor visit,void* context)
{
    const xmail_native_smtp* client=data;
    return xmail_native_protocol_trace(&client->Protocol,visit,context)&&
        (!client->Client||xmail_native_transport_trace(&client->Client->Transport,visit,context));
}
void* xmail_smtp_manage(void* raw,xvalue* releaser)
{
    static const XL6_ForeignGraphDescriptorV1 descriptor={sizeof(descriptor),XL6_FOREIGN_GRAPH_V1_TAG,xmail_smtp_trace,NULL,NULL};
    return xl6ForeignGraphCreateV1(raw,&descriptor,releaser);
}

typedef struct xmail_pop3_open_input {
    const xvalue* engine;
    xvalue* postRelease;
    cstr sHost;
    size_t iHostSize;
    unsigned long long iPort;
    unsigned long long iSecurity;
    bool ReadCapabilities;
    unsigned long long iLineLimit;
    long long iTimeoutMilliseconds;
    void* result;
} xmail_pop3_open_input;

static bool xmail_pop3_open_visit(xnetengine* engine,const xtlsclientconfig* tls,void* data)
{
    xmail_pop3_open_input* input=data;
    input->result=__raw_xmail_pop3_open(engine, input->sHost, input->iHostSize, input->iPort, input->iSecurity, input->ReadCapabilities, input->iLineLimit, input->iTimeoutMilliseconds, (void*)tls, input->postRelease);
    xmail_native_pop3* client=input->result;
    if(!client)return false;
    client->Protocol.EngineOwner=xrtValueRetain((xvalue*)input->engine);
    if(!client->Protocol.EngineOwner){xmail_pop3_raw_destroy(client);input->result=NULL;return false;}
    return true;
}

void* xmail_pop3_open(xvalue* pEngine, cstr sHost, size_t iHostSize, unsigned long long iPort, unsigned long long iSecurity, bool ReadCapabilities, unsigned long long iLineLimit, long long iTimeoutMilliseconds, xvalue* pTlsConfig,xvalue* pPostRelease)
{
    xmail_pop3_open_input input={.engine=pEngine,.postRelease=pPostRelease,.sHost=sHost, .iHostSize=iHostSize, .iPort=iPort, .iSecurity=iSecurity, .ReadCapabilities=ReadCapabilities, .iLineLimit=iLineLimit, .iTimeoutMilliseconds=iTimeoutMilliseconds};
    bool ok=xl6ForeignNetClientVisitV1(pEngine,pTlsConfig,xmail_pop3_open_visit,&input);
    if(!ok&&input.result){
        xerror* error=xrtTakeError();xmail_pop3_raw_destroy(input.result);
        xrtClearError();xrtSetErrorTake(error);input.result=NULL;
    }
    return ok?input.result:NULL;
}

static bool xmail_pop3_trace(const void* data,xrtownershipvisitor visit,void* context)
{
    const xmail_native_pop3* client=data;
    return xmail_native_protocol_trace(&client->Protocol,visit,context)&&
        (!client->Client||xmail_native_transport_trace(&client->Client->Transport,visit,context));
}
void* xmail_pop3_manage(void* raw,xvalue* releaser)
{
    static const XL6_ForeignGraphDescriptorV1 descriptor={sizeof(descriptor),XL6_FOREIGN_GRAPH_V1_TAG,xmail_pop3_trace,NULL,NULL};
    return xl6ForeignGraphCreateV1(raw,&descriptor,releaser);
}

typedef struct xmail_imap_open_input {
    const xvalue* engine;
    xvalue* postRelease;
    cstr sHost;
    size_t iHostSize;
    unsigned long long iPort;
    unsigned long long iSecurity;
    unsigned long long iLineLimit;
    unsigned long long iCommandLineLimit;
    long long iTimeoutMilliseconds;
    void* result;
} xmail_imap_open_input;

static bool xmail_imap_open_visit(xnetengine* engine,const xtlsclientconfig* tls,void* data)
{
    xmail_imap_open_input* input=data;
    input->result=__raw_xmail_imap_open(engine, input->sHost, input->iHostSize, input->iPort, input->iSecurity, input->iLineLimit, input->iCommandLineLimit, input->iTimeoutMilliseconds, (void*)tls, input->postRelease);
    xmail_native_imap* client=input->result;
    if(!client)return false;
    client->Protocol.EngineOwner=xrtValueRetain((xvalue*)input->engine);
    if(!client->Protocol.EngineOwner){xmail_imap_raw_destroy(client);input->result=NULL;return false;}
    return true;
}

void* xmail_imap_open(xvalue* pEngine, cstr sHost, size_t iHostSize, unsigned long long iPort, unsigned long long iSecurity, unsigned long long iLineLimit, unsigned long long iCommandLineLimit, long long iTimeoutMilliseconds, xvalue* pTlsConfig,xvalue* pPostRelease)
{
    xmail_imap_open_input input={.engine=pEngine,.postRelease=pPostRelease,.sHost=sHost, .iHostSize=iHostSize, .iPort=iPort, .iSecurity=iSecurity, .iLineLimit=iLineLimit, .iCommandLineLimit=iCommandLineLimit, .iTimeoutMilliseconds=iTimeoutMilliseconds};
    bool ok=xl6ForeignNetClientVisitV1(pEngine,pTlsConfig,xmail_imap_open_visit,&input);
    if(!ok&&input.result){
        xerror* error=xrtTakeError();xmail_imap_raw_destroy(input.result);
        xrtClearError();xrtSetErrorTake(error);input.result=NULL;
    }
    return ok?input.result:NULL;
}

static bool xmail_imap_trace(const void* data,xrtownershipvisitor visit,void* context)
{
    const xmail_native_imap* client=data;
    return xmail_native_protocol_trace(&client->Protocol,visit,context)&&
        (!client->Client||xmail_native_transport_trace(&client->Client->Transport,visit,context));
}
void* xmail_imap_manage(void* raw,xvalue* releaser)
{
    static const XL6_ForeignGraphDescriptorV1 descriptor={sizeof(descriptor),XL6_FOREIGN_GRAPH_V1_TAG,xmail_imap_trace,NULL,NULL};
    return xl6ForeignGraphCreateV1(raw,&descriptor,releaser);
}
