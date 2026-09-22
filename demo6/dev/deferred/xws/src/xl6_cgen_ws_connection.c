static const char xl6WebSocketConnectionSource[] = XL6_WS_SOURCE(
void* xl6_xrt_ws_close_info_wrap(void* raw) { return raw; }

long long xl6_xrt_ws_close_info_destroy(void* raw)
{
    XL6_WsCloseSnapshot* snapshot = (XL6_WsCloseSnapshot*)raw;
    if ( snapshot != 0 ) { xl6_ws_close_clear(snapshot); xrtFree(snapshot); }
    return 0;
}

long long xl6_xrt_ws_close_info_flags(void* raw) { return raw != 0 ? (long long)((XL6_WsCloseSnapshot*)raw)->Close.Flags : 0; }
long long xl6_xrt_ws_close_info_transport(void* raw) { return raw != 0 ? (long long)((XL6_WsCloseSnapshot*)raw)->Close.Transport : -1; }
long long xl6_xrt_ws_close_info_local_code(void* raw) { return raw != 0 ? (long long)((XL6_WsCloseSnapshot*)raw)->Close.LocalCode : 0; }
long long xl6_xrt_ws_close_info_remote_code(void* raw) { return raw != 0 ? (long long)((XL6_WsCloseSnapshot*)raw)->Close.RemoteCode : 0; }
void* xl6_xrt_ws_close_info_reason(void* raw) { XL6_WsCloseSnapshot* snapshot = (XL6_WsCloseSnapshot*)raw; return xl6_xrt_string_dup(snapshot != 0 && snapshot->Reason != 0 ? snapshot->Reason : "", 0u); }

void* xl6_xrt_ws_error_wrap(void* raw) { return raw; }

long long xl6_xrt_ws_error_destroy(void* raw)
{
    XL6_WsErrorSnapshot* snapshot = (XL6_WsErrorSnapshot*)raw;
    if ( snapshot != 0 ) { xl6_ws_error_clear(snapshot); xrtFree(snapshot); }
    return 0;
}

long long xl6_xrt_ws_error_result(void* raw) { return raw != 0 ? (long long)((XL6_WsErrorSnapshot*)raw)->Result : -1; }
long long xl6_xrt_ws_error_category(void* raw) { XL6_WsErrorSnapshot* snapshot = (XL6_WsErrorSnapshot*)raw; return snapshot != 0 && snapshot->Error != 0 ? (long long)xrtErrorKind(snapshot->Error) : 0; }
long long xl6_xrt_ws_error_operation(void* raw) { (void)raw; return 0; }
long long xl6_xrt_ws_error_phase(void* raw) { (void)raw; return 0; }
long long xl6_xrt_ws_error_system(void* raw) { XL6_WsErrorSnapshot* snapshot = (XL6_WsErrorSnapshot*)raw; return snapshot != 0 && snapshot->Error != 0 ? (long long)xrtErrorSystemCode(snapshot->Error) : 0; }
long long xl6_xrt_ws_error_http_status(void* raw) { return raw != 0 ? (long long)((XL6_WsErrorSnapshot*)raw)->HttpStatus : 0; }
long long xl6_xrt_ws_error_protocol(void* raw) { return raw != 0 ? (long long)((XL6_WsErrorSnapshot*)raw)->ProtocolError : 0; }
long long xl6_xrt_ws_error_close_code(void* raw) { return raw != 0 ? (long long)((XL6_WsErrorSnapshot*)raw)->CloseCode : 0; }
void* xl6_xrt_ws_error_message(void* raw) { XL6_WsErrorSnapshot* snapshot = (XL6_WsErrorSnapshot*)raw; const char* message = snapshot != 0 && snapshot->Error != 0 ? xrtErrorMessage(snapshot->Error) : ""; return xl6_xrt_string_dup(message != 0 ? message : "", 0u); }

void* xl6_xrt_ws_handshake_message_wrap(void* raw) { return raw; }

static const xhttpfield* xl6_ws_handshake_header(const XL6_WsHandshakeMessage* message, const char* name)
{
    if ( message == 0 || name == 0 ) { return 0; }
    if ( message->Kind == 1 ) { return xrtHttpServerRequestHeader(message->Request, xl6_ws_view(name)); }
    if ( message->Kind == 2 ) { return xrtHttpResponseHeader(message->Response, xl6_ws_view(name)); }
    return 0;
}

static size_t xl6_ws_handshake_header_count(const XL6_WsHandshakeMessage* message)
{
    if ( message == 0 ) { return 0u; }
    if ( message->Kind == 1 ) { return xrtHttpServerRequestHeaderCount(message->Request); }
    if ( message->Kind == 2 ) { return xrtHttpResponseHeaderCount(message->Response); }
    return 0u;
}

static const xhttpfield* xl6_ws_handshake_header_at(const XL6_WsHandshakeMessage* message, size_t index)
{
    if ( message == 0 ) { return 0; }
    if ( message->Kind == 1 ) { return xrtHttpServerRequestHeaderAt(message->Request, index); }
    if ( message->Kind == 2 ) { return xrtHttpResponseHeaderAt(message->Response, index); }
    return 0;
}

void* xl6_xrt_ws_handshake_message_target(void* raw) { XL6_WsHandshakeMessage* message = (XL6_WsHandshakeMessage*)raw; return xl6_ws_copy_view(message != 0 && message->Kind == 1 ? xrtHttpServerRequestTarget(message->Request) : xl6_ws_view("")); }
long long xl6_xrt_ws_handshake_message_status(void* raw) { XL6_WsHandshakeMessage* message = (XL6_WsHandshakeMessage*)raw; return message != 0 && message->Kind == 2 ? (long long)xrtHttpResponseStatus(message->Response) : 0; }
void* xl6_xrt_ws_handshake_message_reason(void* raw) { XL6_WsHandshakeMessage* message = (XL6_WsHandshakeMessage*)raw; return xl6_ws_copy_view(message != 0 && message->Kind == 2 ? xrtHttpResponseReason(message->Response) : xl6_ws_view("")); }
void* xl6_xrt_ws_handshake_message_header(void* raw, const char* name) { const xhttpfield* field = xl6_ws_handshake_header((XL6_WsHandshakeMessage*)raw, name); return xl6_ws_copy_view(field != 0 ? field->Value : xl6_ws_view("")); }
long long xl6_xrt_ws_handshake_message_header_count(void* raw) { return (long long)xl6_ws_handshake_header_count((XL6_WsHandshakeMessage*)raw); }
void* xl6_xrt_ws_handshake_message_header_name(void* raw, long long index) { const xhttpfield* field = index >= 0 ? xl6_ws_handshake_header_at((XL6_WsHandshakeMessage*)raw, (size_t)index) : 0; return xl6_ws_copy_view(field != 0 ? field->Name : xl6_ws_view("")); }
void* xl6_xrt_ws_handshake_message_header_value(void* raw, long long index) { const xhttpfield* field = index >= 0 ? xl6_ws_handshake_header_at((XL6_WsHandshakeMessage*)raw, (size_t)index) : 0; return xl6_ws_copy_view(field != 0 ? field->Value : xl6_ws_view("")); }

void* xl6_xrt_ws_handshake_response_wrap(void* raw) { return raw; }
void xl6_xrt_ws_handshake_response_accept_default(void* raw) { XL6_WsHandshakeResponse* response = (XL6_WsHandshakeResponse*)raw; if ( response != 0 ) { response->Action = 0; } }

void xl6_xrt_ws_handshake_response_accept(void* raw, const char* protocol, int compression)
{
    XL6_WsHandshakeResponse* response = (XL6_WsHandshakeResponse*)raw;
    if ( response == 0 || !xl6_ws_replace(&response->Protocol, protocol) ) { return; }
    response->Action = 0;
    response->Compression = compression != 0;
}

void xl6_xrt_ws_handshake_response_reject(void* raw, long long status, const char* reason)
{
    XL6_WsHandshakeResponse* response = (XL6_WsHandshakeResponse*)raw;
    if ( response == 0 || status < 400 || status > 599 || !xl6_ws_replace(&response->Reason, reason) ) { return; }
    response->Action = 1;
    response->Status = (int)status;
}

int xl6_xrt_ws_handshake_response_header(void* raw, const char* name, const char* value)
{
    XL6_WsHandshakeResponse* response = (XL6_WsHandshakeResponse*)raw;
    return response != 0 && name != 0 && value != 0 && xrtHttpHeadersAdd(response->Headers, xl6_ws_view(name), xl6_ws_view(value));
}

void xl6_xrt_ws_handshake_response_data(void* raw, void* data) { XL6_WsHandshakeResponse* response = (XL6_WsHandshakeResponse*)raw; if ( response != 0 ) { response->ConnectionData = data; } }

static int xl6_ws_submit(xfuture* future)
{
    if ( future == 0 ) { return -1; }
    xrtFutureDestroy(future);
    return 0;
}

static bool xl6_ws_deflate_enabled(const xwsconn* connection)
{
    xwsdeflate deflate;
    return connection != 0 && xrtWsConnDeflate(connection, &deflate);
}

static int xl6_ws_send_text(xwsconn* connection, const char* data, size_t compressMin, bool compression)
{
    xstrview text = xl6_ws_view(data != 0 ? data : "");
    if ( connection == 0 ) { return -1; }
    return xl6_ws_submit(compression && text.Size >= compressMin ? xrtWsConnTextCompressedAsync(connection, text) : xrtWsConnTextAsync(connection, text));
}

static int xl6_ws_send_binary(xwsconn* connection, const void* data, size_t size, size_t compressMin, bool compression)
{
    xbytesview bytes = xl6_ws_bytes(data, size);
    if ( connection == 0 || (size != 0u && data == 0) ) { return -1; }
    return xl6_ws_submit(compression && size >= compressMin ? xrtWsConnBinaryCompressedAsync(connection, bytes) : xrtWsConnBinaryAsync(connection, bytes));
}

static int xl6_ws_ping(xwsconn* connection, const void* data, size_t size)
{
    if ( connection == 0 || (size != 0u && data == 0) || size > 125u ) { return -1; }
    return xl6_ws_submit(xrtWsConnPingAsync(connection, xl6_ws_bytes(data, size)));
}

static int xl6_ws_close(xwsconn* connection, long long code, const char* reason)
{
    if ( connection == 0 || code < 0 || code > 65535 ) { return -1; }
    return xl6_ws_submit(xrtWsConnCloseAsync(connection, (uint16_t)code, xl6_ws_view(reason != 0 ? reason : "")));
}

static long long xl6_ws_wait(xwsconn* connection, long long kind, long long timeout)
{
    xfuture* future;
    bool ready;
    int status;
    if ( connection == 0 || kind < 1 || kind > 3 ) { return -1; }
    future = xrtWsConnWaitAsync(connection, (int)kind);
    if ( future == 0 ) { return -1; }
    ready = xl6FutureWaitFor(future, timeout);
    if ( !ready ) { (void)xl6FutureCancel(future); xl6FutureDestroy(future); return -3; }
    status = xl6FutureStatus(future);
    xl6FutureDestroy(future);
    if ( status != 0 ) { return -1; }
    return kind == 3 ? -4 : 0;
}

static void xl6_ws_writer_free(XL6_WsWriter* writer)
{
    if ( writer == 0 ) { return; }
    xrtWsConnDestroy(writer->Connection);
    xrtBufferDestroy(writer->Buffer);
    xrtFree(writer);
}

static XL6_WsWriter* xl6_ws_writer_create(xwsconn* connection, uint8_t opcode, size_t compressMin, bool compression)
{
    XL6_WsWriter* writer;
    if ( connection == 0 ) { return 0; }
    writer = (XL6_WsWriter*)xrtCalloc(1u, sizeof(*writer));
    if ( writer == 0 ) { return 0; }
    writer->Connection = xrtWsConnRef(connection);
    writer->Buffer = xrtBufferCreate();
    writer->Opcode = opcode;
    writer->CompressMin = compressMin;
    writer->Compression = compression;
    if ( writer->Connection == 0 || writer->Buffer == 0 ) { xl6_ws_writer_free(writer); return 0; }
    return writer;
}

static int xl6_ws_writer_append(XL6_WsWriter* writer, const void* data, size_t size)
{
    if ( writer == 0 || writer->Finished || (size != 0u && data == 0) ) { return -1; }
    return xl6_ws_buffer_append(writer->Buffer, xl6_ws_bytes(data, size)) ? 0 : -1;
}

static int xl6_ws_writer_finish(XL6_WsWriter* writer, const void* data, size_t size)
{
    int result;
    if ( xl6_ws_writer_append(writer, data, size) != 0 ) { return -1; }
    writer->Finished = true;
    if ( writer->Opcode == 1u ) {
        char* text = xl6_xrt_string_dup((const char*)writer->Buffer->Data, writer->Buffer->Size);
        if ( text == 0 ) { return -1; }
        result = xl6_ws_send_text(writer->Connection, text, writer->CompressMin, writer->Compression);
        xrtFree(text);
        return result;
    }
    return xl6_ws_send_binary(writer->Connection, writer->Buffer->Data, writer->Buffer->Size, writer->CompressMin, writer->Compression);
}

void* xl6_xrt_ws_writer_wrap(void* raw) { return raw; }
long long xl6_xrt_ws_writer_destroy(void* raw) { xl6_ws_writer_free((XL6_WsWriter*)raw); return 0; }
long long xl6_xrt_ws_writer_write_text(void* raw, const char* data) { return xl6_ws_writer_append((XL6_WsWriter*)raw, data != 0 ? data : "", data != 0 ? strlen(data) : 0u); }
long long xl6_xrt_ws_writer_write_bytes(void* raw, void* data, size_t size) { return xl6_ws_writer_append((XL6_WsWriter*)raw, data, size); }
long long xl6_xrt_ws_writer_finish_text(void* raw, const char* data) { return xl6_ws_writer_finish((XL6_WsWriter*)raw, data != 0 ? data : "", data != 0 ? strlen(data) : 0u); }
long long xl6_xrt_ws_writer_finish_bytes(void* raw, void* data, size_t size) { return xl6_ws_writer_finish((XL6_WsWriter*)raw, data, size); }
int xl6_xrt_ws_writer_finished(void* raw) { return raw != 0 && ((XL6_WsWriter*)raw)->Finished; }
) "\n";



/* XRT 流式消息事件在这里聚合为 xlang 的完整 Text/Binary 回调。 */
