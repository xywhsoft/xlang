static const char xl6WebSocketEventsSource[] = XL6_WS_SOURCE(
static XL6_WsClient* xl6_ws_client_retain(XL6_WsClient* client)
{
    if ( client != 0 ) { (void)xrtRefRetain(&client->References); }
    return client;
}

static void xl6_ws_client_release(XL6_WsClient* client);

static XL6_WsServer* xl6_ws_server_retain(XL6_WsServer* server)
{
    if ( server != 0 ) { (void)xrtRefRetain(&server->References); }
    return server;
}

static void xl6_ws_server_release(XL6_WsServer* server);

static XL6_WsPeer* xl6_ws_peer_retain(XL6_WsPeer* peer)
{
    if ( peer != 0 ) { (void)xrtRefRetain(&peer->References); }
    return peer;
}

static void xl6_ws_peer_release(XL6_WsPeer* peer)
{
    if ( peer == 0 || xrtRefRelease(&peer->References) != 0 ) { return; }
    xrtWsConnDestroy(peer->Connection);
    xrtBufferDestroy(peer->Message);
    xl6_ws_close_clear(&peer->LastClose);
    xl6_ws_error_clear(&peer->LastError);
    xl6_ws_server_release(peer->Server);
    xrtFree(peer);
}

static XL6_WsPeer* xl6_ws_peer_create(XL6_WsServer* server)
{
    XL6_WsPeer* peer;
    if ( server == 0 ) { return 0; }
    peer = (XL6_WsPeer*)xrtCalloc(1u, sizeof(*peer));
    if ( peer == 0 ) { return 0; }
    peer->References = 1;
    peer->Server = xl6_ws_server_retain(server);
    peer->Message = xrtBufferCreate();
    if ( peer->Server == 0 || peer->Message == 0 ) { xl6_ws_peer_release(peer); return 0; }
    return peer;
}

static void xl6_ws_call_client_error(XL6_WsClient* client)
{
    XL6_WsErrorSnapshot* error;
    xvalue* result;
    const char* message;
    if ( client == 0 || !xl6_ws_function_valid(&client->Callbacks.Error) ) { return; }
    error = &client->LastError;
    message = error->Error != 0 && xrtErrorMessage(error->Error) != 0 ? xrtErrorMessage(error->Error) : "";
    result = xl6_function_call_var(client->Callbacks.Error, 9,
        xrtValueInt(error->Result),
        xrtValueInt(error->Error != 0 ? xrtErrorKind(error->Error) : 0),
        xrtValueInt(0),
        xrtValueInt(0),
        xrtValueInt(error->Error != 0 ? xrtErrorSystemCode(error->Error) : 0),
        xrtValueInt(error->HttpStatus),
        xrtValueInt(error->ProtocolError),
        xrtValueInt(error->CloseCode),
        xl6_ws_text_value(message, strlen(message)));
    xl6_ws_result_release(result);
}

static void xl6_ws_call_peer_error(XL6_WsPeer* peer)
{
    XL6_WsErrorSnapshot* error;
    xvalue* result;
    const char* message;
    if ( peer == 0 || peer->Server == 0 || !xl6_ws_function_valid(&peer->Server->Callbacks.Error) ) { return; }
    error = &peer->LastError;
    message = error->Error != 0 && xrtErrorMessage(error->Error) != 0 ? xrtErrorMessage(error->Error) : "";
    result = xl6_function_call_var(peer->Server->Callbacks.Error, 10,
        xrtValuePointer(peer),
        xrtValueInt(error->Result),
        xrtValueInt(error->Error != 0 ? xrtErrorKind(error->Error) : 0),
        xrtValueInt(0),
        xrtValueInt(0),
        xrtValueInt(error->Error != 0 ? xrtErrorSystemCode(error->Error) : 0),
        xrtValueInt(error->HttpStatus),
        xrtValueInt(error->ProtocolError),
        xrtValueInt(error->CloseCode),
        xl6_ws_text_value(message, strlen(message)));
    xl6_ws_result_release(result);
}

static void xl6_ws_client_message_begin(xwsconn* connection, const xwsmessageinfo* info, void* data)
{
    XL6_WsClient* client = (XL6_WsClient*)data;
    (void)connection;
    if ( client == 0 || client->Message == 0 || info == 0 ) { return; }
    xrtBufferClear(client->Message);
    client->MessageOpcode = info->Opcode;
}

static void xl6_ws_client_message_data(xwsconn* connection, xbytesview bytes, void* data)
{
    XL6_WsClient* client = (XL6_WsClient*)data;
    if ( client == 0 || client->Message == 0 || !xl6_ws_buffer_append(client->Message, bytes) ) { (void)xrtWsConnAbort(connection); }
}

static void xl6_ws_client_message_end(xwsconn* connection, void* data)
{
    XL6_WsClient* client = (XL6_WsClient*)data;
    xvalue* result = 0;
    (void)connection;
    if ( client == 0 || client->Message == 0 ) { return; }
    if ( client->MessageOpcode == 1u && xl6_ws_function_valid(&client->Callbacks.Text) ) {
        result = xl6_function_call_var(client->Callbacks.Text, 1, xl6_ws_text_value(client->Message->Data, client->Message->Size));
    } else if ( client->MessageOpcode == 2u && xl6_ws_function_valid(&client->Callbacks.Binary) ) {
        result = xl6_function_call_var(client->Callbacks.Binary, 2, xrtValuePointer(client->Message->Data), xrtValueInt((long long)client->Message->Size));
    }
    xl6_ws_result_release(result);
    xrtBufferClear(client->Message);
}

static void xl6_ws_peer_message_begin(xwsconn* connection, const xwsmessageinfo* info, void* data)
{
    XL6_WsPeer* peer = (XL6_WsPeer*)data;
    (void)connection;
    if ( peer == 0 || peer->Message == 0 || info == 0 ) { return; }
    xrtBufferClear(peer->Message);
    peer->MessageOpcode = info->Opcode;
}

static void xl6_ws_peer_message_data(xwsconn* connection, xbytesview bytes, void* data)
{
    XL6_WsPeer* peer = (XL6_WsPeer*)data;
    if ( peer == 0 || peer->Message == 0 || !xl6_ws_buffer_append(peer->Message, bytes) ) { (void)xrtWsConnAbort(connection); }
}

static void xl6_ws_peer_message_end(xwsconn* connection, void* data)
{
    XL6_WsPeer* peer = (XL6_WsPeer*)data;
    xvalue* result = 0;
    (void)connection;
    if ( peer == 0 || peer->Server == 0 || peer->Message == 0 ) { return; }
    if ( peer->MessageOpcode == 1u && xl6_ws_function_valid(&peer->Server->Callbacks.Text) ) {
        result = xl6_function_call_var(peer->Server->Callbacks.Text, 2, xrtValuePointer(peer), xl6_ws_text_value(peer->Message->Data, peer->Message->Size));
    } else if ( peer->MessageOpcode == 2u && xl6_ws_function_valid(&peer->Server->Callbacks.Binary) ) {
        result = xl6_function_call_var(peer->Server->Callbacks.Binary, 3, xrtValuePointer(peer), xrtValuePointer(peer->Message->Data), xrtValueInt((long long)peer->Message->Size));
    }
    xl6_ws_result_release(result);
    xrtBufferClear(peer->Message);
}

static void xl6_ws_client_ping(xwsconn* connection, xbytesview payload, void* data)
{
    XL6_WsClient* client = (XL6_WsClient*)data;
    xvalue* result;
    (void)connection;
    if ( client == 0 || !xl6_ws_function_valid(&client->Callbacks.Ping) ) { return; }
    result = xl6_function_call_var(client->Callbacks.Ping, 2, xrtValuePointer((void*)payload.Data), xrtValueInt((long long)payload.Size));
    xl6_ws_result_release(result);
}

static void xl6_ws_client_pong(xwsconn* connection, xbytesview payload, void* data)
{
    XL6_WsClient* client = (XL6_WsClient*)data;
    xvalue* result;
    (void)connection;
    if ( client == 0 || !xl6_ws_function_valid(&client->Callbacks.Pong) ) { return; }
    result = xl6_function_call_var(client->Callbacks.Pong, 2, xrtValuePointer((void*)payload.Data), xrtValueInt((long long)payload.Size));
    xl6_ws_result_release(result);
}

static void xl6_ws_peer_ping(xwsconn* connection, xbytesview payload, void* data)
{
    XL6_WsPeer* peer = (XL6_WsPeer*)data;
    xvalue* result;
    (void)connection;
    if ( peer == 0 || peer->Server == 0 || !xl6_ws_function_valid(&peer->Server->Callbacks.Ping) ) { return; }
    result = xl6_function_call_var(peer->Server->Callbacks.Ping, 3, xrtValuePointer(peer), xrtValuePointer((void*)payload.Data), xrtValueInt((long long)payload.Size));
    xl6_ws_result_release(result);
}

static void xl6_ws_peer_pong(xwsconn* connection, xbytesview payload, void* data)
{
    XL6_WsPeer* peer = (XL6_WsPeer*)data;
    xvalue* result;
    (void)connection;
    if ( peer == 0 || peer->Server == 0 || !xl6_ws_function_valid(&peer->Server->Callbacks.Pong) ) { return; }
    result = xl6_function_call_var(peer->Server->Callbacks.Pong, 3, xrtValuePointer(peer), xrtValuePointer((void*)payload.Data), xrtValueInt((long long)payload.Size));
    xl6_ws_result_release(result);
}

static void xl6_ws_client_backpressure(xwsconn* connection, size_t pending, void* data)
{
    XL6_WsClient* client = (XL6_WsClient*)data;
    xvalue* result;
    (void)connection;
    if ( client == 0 || !xl6_ws_function_valid(&client->Callbacks.Backpressure) ) { return; }
    result = xl6_function_call_var(client->Callbacks.Backpressure, 1, xrtValueInt((long long)pending));
    xl6_ws_result_release(result);
}

static void xl6_ws_client_writable(xwsconn* connection, size_t pending, void* data)
{
    XL6_WsClient* client = (XL6_WsClient*)data;
    xvalue* result;
    (void)connection;
    if ( client == 0 || !xl6_ws_function_valid(&client->Callbacks.Writable) ) { return; }
    result = xl6_function_call_var(client->Callbacks.Writable, 1, xrtValueInt((long long)pending));
    xl6_ws_result_release(result);
}

static void xl6_ws_client_drain(xwsconn* connection, void* data)
{
    XL6_WsClient* client = (XL6_WsClient*)data;
    xvalue* result;
    (void)connection;
    if ( client == 0 || !xl6_ws_function_valid(&client->Callbacks.Drain) ) { return; }
    result = xl6_function_call_var(client->Callbacks.Drain, 0);
    xl6_ws_result_release(result);
}

static void xl6_ws_peer_backpressure(xwsconn* connection, size_t pending, void* data)
{
    XL6_WsPeer* peer = (XL6_WsPeer*)data;
    xvalue* result;
    (void)connection;
    if ( peer == 0 || peer->Server == 0 || !xl6_ws_function_valid(&peer->Server->Callbacks.Backpressure) ) { return; }
    result = xl6_function_call_var(peer->Server->Callbacks.Backpressure, 2, xrtValuePointer(peer), xrtValueInt((long long)pending));
    xl6_ws_result_release(result);
}

static void xl6_ws_peer_writable(xwsconn* connection, size_t pending, void* data)
{
    XL6_WsPeer* peer = (XL6_WsPeer*)data;
    xvalue* result;
    (void)connection;
    if ( peer == 0 || peer->Server == 0 || !xl6_ws_function_valid(&peer->Server->Callbacks.Writable) ) { return; }
    result = xl6_function_call_var(peer->Server->Callbacks.Writable, 2, xrtValuePointer(peer), xrtValueInt((long long)pending));
    xl6_ws_result_release(result);
}

static void xl6_ws_peer_drain(xwsconn* connection, void* data)
{
    XL6_WsPeer* peer = (XL6_WsPeer*)data;
    xvalue* result;
    (void)connection;
    if ( peer == 0 || peer->Server == 0 || !xl6_ws_function_valid(&peer->Server->Callbacks.Drain) ) { return; }
    result = xl6_function_call_var(peer->Server->Callbacks.Drain, 1, xrtValuePointer(peer));
    xl6_ws_result_release(result);
}

static void xl6_ws_client_error_event(xwsconn* connection, const xerror* error, void* data)
{
    XL6_WsClient* client = (XL6_WsClient*)data;
    (void)connection;
    if ( client == 0 ) { return; }
    xl6_ws_error_set(&client->LastError, -1, 0, 0u, error);
    xl6_ws_call_client_error(client);
}

static void xl6_ws_peer_error_event(xwsconn* connection, const xerror* error, void* data)
{
    XL6_WsPeer* peer = (XL6_WsPeer*)data;
    (void)connection;
    if ( peer == 0 ) { return; }
    xl6_ws_error_set(&peer->LastError, -1, 0, 0u, error);
    xl6_ws_call_peer_error(peer);
}

static void xl6_ws_client_close_event(xwsconn* connection, const xwsconnclose* closeInfo, void* data)
{
    XL6_WsClient* client = (XL6_WsClient*)data;
    xvalue* result;
    (void)connection;
    if ( client == 0 ) { return; }
    (void)xl6_ws_close_set(&client->LastClose, closeInfo);
    if ( xl6_ws_function_valid(&client->Callbacks.Close) ) {
        result = xl6_function_call_var(client->Callbacks.Close, 5,
            xrtValueInt(closeInfo != 0 ? closeInfo->Flags : 0),
            xrtValueInt(closeInfo != 0 ? closeInfo->Transport : -1),
            xl6_ws_text_value(closeInfo != 0 ? closeInfo->Reason.Data : "", closeInfo != 0 ? closeInfo->Reason.Size : 0u),
            xrtValueInt(closeInfo != 0 ? closeInfo->LocalCode : 0),
            xrtValueInt(closeInfo != 0 ? closeInfo->RemoteCode : 0));
        xl6_ws_result_release(result);
    }
    if ( client->EventReference ) { client->EventReference = false; xl6_ws_client_release(client); }
}

static void xl6_ws_peer_close_event(xwsconn* connection, const xwsconnclose* closeInfo, void* data)
{
    XL6_WsPeer* peer = (XL6_WsPeer*)data;
    xvalue* result;
    (void)connection;
    if ( peer == 0 ) { return; }
    (void)xl6_ws_close_set(&peer->LastClose, closeInfo);
    if ( peer->Server != 0 && xl6_ws_function_valid(&peer->Server->Callbacks.Close) ) {
        result = xl6_function_call_var(peer->Server->Callbacks.Close, 6,
            xrtValuePointer(peer),
            xrtValueInt(closeInfo != 0 ? closeInfo->Flags : 0),
            xrtValueInt(closeInfo != 0 ? closeInfo->Transport : -1),
            xl6_ws_text_value(closeInfo != 0 ? closeInfo->Reason.Data : "", closeInfo != 0 ? closeInfo->Reason.Size : 0u),
            xrtValueInt(closeInfo != 0 ? closeInfo->LocalCode : 0),
            xrtValueInt(closeInfo != 0 ? closeInfo->RemoteCode : 0));
        xl6_ws_result_release(result);
    }
    xl6_ws_peer_release(peer);
}

static void xl6_ws_client_events(xwsconnevents* events)
{
    memset(events, 0, sizeof(*events));
    events->MessageBegin = xl6_ws_client_message_begin;
    events->MessageData = xl6_ws_client_message_data;
    events->MessageEnd = xl6_ws_client_message_end;
    events->Ping = xl6_ws_client_ping;
    events->Pong = xl6_ws_client_pong;
    events->Backpressure = xl6_ws_client_backpressure;
    events->Writable = xl6_ws_client_writable;
    events->Drain = xl6_ws_client_drain;
    events->Error = xl6_ws_client_error_event;
    events->Close = xl6_ws_client_close_event;
}

static void xl6_ws_peer_events(xwsconnevents* events)
{
    memset(events, 0, sizeof(*events));
    events->MessageBegin = xl6_ws_peer_message_begin;
    events->MessageData = xl6_ws_peer_message_data;
    events->MessageEnd = xl6_ws_peer_message_end;
    events->Ping = xl6_ws_peer_ping;
    events->Pong = xl6_ws_peer_pong;
    events->Backpressure = xl6_ws_peer_backpressure;
    events->Writable = xl6_ws_peer_writable;
    events->Drain = xl6_ws_peer_drain;
    events->Error = xl6_ws_peer_error_event;
    events->Close = xl6_ws_peer_close_event;
}
) "\n";



/* 客户端是一次启动对象；HTTP Call、升级 Connection 和事件引用各自拥有明确生命周期。 */
