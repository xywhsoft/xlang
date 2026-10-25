/* 输出 TCP 便捷层和 Stream/Listener 绑定。 */
void xl6CgenEmitNetTcpComponent(
    XL6_Output* out,
    const XL6_CgenNetNeeds* needs
)
{
    if ( needs->tcpDial ) {
    xl6OutputPuts(
        "void* xl6_xrt_tcp_connect(xlir_string host, uint16_t port, int64_t timeoutMs)\n"
        "{\n"
        "    xnetdialconfig config; const char* chost = xlir_string_cstr(host);\n"
        "    if ( (xl6_net_engine == 0) || (xl6_net_resolver == 0) || (chost == 0) || (chost[0] == '\\0') || (port == 0u) ) { return 0; }\n"
        "    xrtNetDialConfigInit(&config);\n"
        "    config.Timeout = xl6_net_timeout_us(timeoutMs);\n"
        "    config.Stream.ConnectTimeout = config.Timeout;\n"
        "    config.Affinity = xl6_net_next_dial_worker();\n"
        "    return xrtNetConnect(xl6_net_engine, xl6_net_resolver, chost, port, &config, 0, 0, xl6_net_deadline(timeoutMs), 0);\n"
        "}\n\n"
        "xfuture* xl6_xrt_tcp_connect_async(xlir_string host, uint16_t port, int64_t timeoutMs)\n"
        "{\n"
        "    xnetdialconfig config; xfuture* source; const char* chost = xlir_string_cstr(host);\n"
        "    if ( (xl6_net_engine == 0) || (xl6_net_resolver == 0) || (chost == 0) || (chost[0] == '\\0') || (port == 0u) ) { return 0; }\n"
        "    xrtNetDialConfigInit(&config); config.Timeout = xl6_net_timeout_us(timeoutMs); config.Stream.ConnectTimeout = config.Timeout; config.Affinity = xl6_net_next_dial_worker();\n"
        "    source = xrtNetDialAsync(xl6_net_engine, xl6_net_resolver, chost, port, &config, 0, 0);\n"
        "    return xl6_net_future_map(source, xl6_net_map_stream);\n"
        "}\n\n",
        out
    );
    }

    if ( needs->tcpListen ) {
    xl6OutputPuts(
        "void* xl6_xrt_tcp_listen(xlir_string host, uint16_t port, uint32_t backlog)\n"
        "{\n"
        "    xnetlistenconfig config; const char* chost = xlir_string_cstr(host);\n"
        "    if ( xl6_net_engine == 0 ) { return 0; }\n"
        "    xrtNetListenConfigInit(&config);\n"
        "    if ( chost == NULL || !xl6_net_address_parse(&config.Address, chost, port, true) ) { return 0; }\n"
        "    if ( backlog != 0u ) { config.Backlog = backlog > INT_MAX ? INT_MAX : (int)backlog; }\n"
        "    return xrtNetListen(xl6_net_engine, &config, 0, 0, 0);\n"
        "}\n\n",
        out
    );
    }

    if ( needs->tcpStream ) {
    xl6OutputPuts(
		"void* xl6_xrt_tcp_stream_native(void* raw) { return raw; }\n"
        "void xl6_xrt_tcp_stream_destroy(void* raw) { xnetstream* stream = (xnetstream*)raw; if ( stream != 0 ) { (void)xrtNetStreamClose(stream); xrtNetStreamDestroy(stream); } }\n"
        "void xl6_xrt_tcp_stream_close(void* raw) { if ( raw != 0 ) { (void)xrtNetStreamClose((xnetstream*)raw); } }\n"
        "int xl6_xrt_tcp_stream_shutdown_write(void* raw) { return (raw != 0) && xrtNetStreamShutdownWrite((xnetstream*)raw); }\n"
        "int xl6_xrt_tcp_stream_abort(void* raw) { return (raw != 0) && xrtNetStreamAbort((xnetstream*)raw); }\n"
        "long long xl6_xrt_tcp_stream_send_text_result(void* raw, xlir_string text) { return (raw != 0) && (text.Data != 0) ? xrtNetStreamSend((xnetstream*)raw, text.Data, text.Size) : XNET_RESULT_ERROR; }\n"
        "long long xl6_xrt_tcp_stream_send_bytes_result(void* raw, void* data, size_t size) { return (raw != 0) && ((data != 0) || (size == 0u)) ? xrtNetStreamSend((xnetstream*)raw, data, size) : XNET_RESULT_ERROR; }\n"
        "int xl6_xrt_tcp_stream_send_text(void* raw, xlir_string text) { return (raw != 0) && (text.Data != 0) && (xrtNetStreamSend((xnetstream*)raw, text.Data, text.Size) == XL6_XNET_OK); }\n"
		"int xl6_xrt_tcp_stream_send_bytes(void* raw, void* data, size_t size) { return (raw != 0) && ((data != 0) || (size == 0u)) && (xrtNetStreamSend((xnetstream*)raw, data, size) == XL6_XNET_OK); }\n"
        "size_t xl6_xrt_tcp_stream_available(void* raw) { return raw != 0 ? xrtNetStreamAvailable((xnetstream*)raw) : 0u; }\n"
        "long long xl6_xrt_net_stream_pending_send(void* raw) { return raw != 0 ? (long long)xrtNetStreamPending((xnetstream*)raw) : 0; }\n"
        "long long xl6_xrt_net_stream_write_limit(void* raw) { return raw != 0 ? (long long)xrtNetStreamWriteLimit((xnetstream*)raw) : 0; }\n"
        "long long xl6_xrt_net_stream_writable(void* raw) { return raw != 0 ? (long long)xrtNetStreamWritable((xnetstream*)raw) : 0; }\n"
        "void xl6_xrt_net_stream_pause(void* raw) { if ( raw != 0 ) { xrtNetStreamPause((xnetstream*)raw); } }\n"
        "void xl6_xrt_net_stream_resume(void* raw) { if ( raw != 0 ) { (void)xrtNetStreamResume((xnetstream*)raw); } }\n\n",
        out
    );

    if ( needs->tcpFile ) {
    xl6OutputPuts(
        "/* 校验完整文件范围；size 为 -1 时取 offset 到文件尾。 */\n"
        "static bool xl6_net_stream_file_range(xfile file, long long offset, long long size, uint64_t* amount)\n"
        "{\n"
        "    uint64_t fileSize; uint64_t range;\n"
        "    if ( (file == 0) || (amount == 0) || (offset < 0) || (size < -1) ) { xrtSetErrorInfo(XERR_ARGUMENT, \"xlang.net\", 0, \"invalid stream file range\"); return false; }\n"
        "    if ( !xrtFileSize(file, &fileSize) ) { return false; }\n"
        "    if ( (uint64_t)offset > fileSize ) { xrtSetErrorInfo(XERR_RANGE, \"xlang.net\", 0, \"file offset exceeds file size\"); return false; }\n"
        "    range = size < 0 ? fileSize - (uint64_t)offset : (uint64_t)size;\n"
        "    if ( range > (fileSize - (uint64_t)offset) ) { xrtSetErrorInfo(XERR_RANGE, \"xlang.net\", 0, \"file range exceeds file size\"); return false; }\n"
        "    *amount = range;\n"
        "    return true;\n"
        "}\n\n"
        "/* 单次提交保留 XRT 的 OK、AGAIN、CLOSED 和 ERROR 结果。 */\n"
        "static xnetresult xl6_net_stream_send_file_range(void* raw, xfile file, long long offset, long long size)\n"
        "{\n"
        "    uint64_t amount;\n"
        "    if ( (raw == 0) || !xl6_net_stream_file_range(file, offset, size, &amount) ) { return XNET_RESULT_ERROR; }\n"
        "    if ( amount > (uint64_t)SIZE_MAX ) { xrtSetErrorInfo(XERR_RANGE, \"xlang.net\", 0, \"file range exceeds platform size limit\"); return XNET_RESULT_ERROR; }\n"
        "    return xrtNetStreamSendFile((xnetstream*)raw, file, (uint64_t)offset, (size_t)amount);\n"
        "}\n\n"
        "/* 完整发送按当前可写预算分段，不要求文件区间小于 WriteLimit。 */\n"
        "static int xl6_net_stream_send_file_all(void* raw, xfile file, long long offset, long long size, long long timeoutMs)\n"
        "{\n"
        "    xnetstream* stream = (xnetstream*)raw; uint64_t amount; uint64_t current; xdeadline deadline;\n"
        "    if ( (stream == 0) || !xl6_net_stream_file_range(file, offset, size, &amount) ) { return 0; }\n"
        "    current = (uint64_t)offset; deadline = xl6_net_deadline(timeoutMs);\n"
        "    while ( amount != 0u ) {\n"
        "        size_t writable = xrtNetStreamWritable(stream); size_t chunk; xnetresult resultCode;\n"
        "        if ( writable == 0u ) { if ( !xrtNetStreamWait(stream, XL6_XNET_WAIT_WRITE, deadline, 0) ) { return 0; } continue; }\n"
        "        chunk = amount < (uint64_t)writable ? (size_t)amount : writable;\n"
        "        resultCode = xrtNetStreamSendFile(stream, file, current, chunk);\n"
        "        if ( resultCode == XNET_RESULT_OK ) { current += (uint64_t)chunk; amount -= (uint64_t)chunk; continue; }\n"
        "        if ( resultCode != XNET_RESULT_AGAIN ) { return 0; }\n"
        "        if ( !xrtNetStreamWait(stream, XL6_XNET_WAIT_WRITE, deadline, 0) ) { return 0; }\n"
        "    }\n"
        "    return 1;\n"
        "}\n\n"
        "long long xl6_xrt_tcp_stream_send_file_handle_result(void* raw, void* fileRaw, long long offset, long long size)\n"
        "{\n"
        "    return (long long)xl6_net_stream_send_file_range(raw, (xfile)fileRaw, offset, size);\n"
        "}\n\n"
        "long long xl6_xrt_tcp_stream_send_file_result(void* raw, xlir_string pathValue, long long offset, long long size)\n"
        "{\n"
        "    xfile file; xnetresult resultCode; const char* path = xlir_string_cstr(pathValue);\n"
        "    if ( (path == 0) || (path[0] == '\\0') ) { xrtSetErrorInfo(XERR_ARGUMENT, \"xlang.net\", 0, \"file path is empty\"); return XNET_RESULT_ERROR; }\n"
        "    file = xrtOpen(path, XFILE_READ);\n"
        "    if ( file == 0 ) { return XNET_RESULT_ERROR; }\n"
        "    resultCode = xl6_net_stream_send_file_range(raw, file, offset, size);\n"
        "    (void)xrtClose(file);\n"
        "    return (long long)resultCode;\n"
        "}\n\n"
        "int xl6_xrt_tcp_stream_send_file_handle(void* raw, void* fileRaw, long long offset, long long size, long long timeoutMs)\n"
        "{\n"
        "    return xl6_net_stream_send_file_all(raw, (xfile)fileRaw, offset, size, timeoutMs);\n"
        "}\n\n"
        "int xl6_xrt_tcp_stream_send_file(void* raw, xlir_string pathValue, long long offset, long long size, long long timeoutMs)\n"
        "{\n"
        "    xfile file; int sent; const char* path = xlir_string_cstr(pathValue);\n"
        "    if ( (path == 0) || (path[0] == '\\0') ) { xrtSetErrorInfo(XERR_ARGUMENT, \"xlang.net\", 0, \"file path is empty\"); return 0; }\n"
        "    file = xrtOpen(path, XFILE_READ);\n"
        "    if ( file == 0 ) { return 0; }\n"
        "    sent = xl6_net_stream_send_file_all(raw, file, offset, size, timeoutMs);\n"
        "    (void)xrtClose(file);\n"
        "    return sent;\n"
        "}\n\n",
        out
    );
    }

    xl6OutputPuts(
        "static char* xl6_net_stream_recv_text(xnetstream* stream, size_t maxBytes, xdeadline deadline)\n"
        "{\n"
        "    xnetbytes* bytes; XL6_XrtBytesView view; char* text;\n"
        "    if ( stream == 0 ) { return 0; }\n"
        "    bytes = xrtNetStreamRecv(stream, maxBytes, deadline, 0);\n"
        "    if ( bytes == 0 ) { return 0; }\n"
        "    view = xrtNetBytesView(bytes); text = (char*)xrtMalloc(view.Size + 1u);\n"
        "    if ( text != 0 ) { if ( view.Size != 0u ) { memcpy(text, view.Data, view.Size); } text[view.Size] = '\\0'; }\n"
        "    xrtNetBytesDestroy(bytes); return text;\n"
        "}\n"
        "void* xl6_xrt_tcp_stream_read_text(void* raw, size_t maxBytes) { return xl6_net_stream_recv_text((xnetstream*)raw, maxBytes, xrtDeadlineAfter(0u)); }\n"
        "void* xl6_xrt_tcp_stream_recv_text(void* raw, size_t maxBytes, long long timeoutMs) { return xl6_net_stream_recv_text((xnetstream*)raw, maxBytes, xl6_net_deadline(timeoutMs)); }\n"
        "xfuture* xl6_xrt_tcp_stream_recv_text_async(void* raw, size_t maxBytes) { return raw != 0 ? xl6_net_future_map(xrtNetStreamRecvAsync((xnetstream*)raw, maxBytes), xl6_net_map_text) : 0; }\n"
        "void* xl6_xrt_tcp_stream_recv_owned(void* raw, size_t maxBytes, long long timeoutMs) { return raw != 0 ? xrtNetStreamRecv((xnetstream*)raw, maxBytes, xl6_net_deadline(timeoutMs), 0) : 0; }\n"
        "xfuture* xl6_xrt_tcp_stream_recv_owned_async(void* raw, size_t maxBytes) { return raw != 0 ? xl6_net_future_map(xrtNetStreamRecvAsync((xnetstream*)raw, maxBytes), xl6_net_map_bytes) : 0; }\n"
        "int xl6_xrt_tcp_stream_wait_available(void* raw, long long minimum, long long timeoutMs) { return (raw != 0) && (minimum >= 0) && ((uint64_t)minimum <= (uint64_t)SIZE_MAX) && xrtNetStreamWaitAvailable((xnetstream*)raw, (size_t)minimum, xl6_net_deadline(timeoutMs), 0); }\n"
        "xfuture* xl6_xrt_tcp_stream_wait_available_async(void* raw, long long minimum) { return (raw != 0) && (minimum >= 0) && ((uint64_t)minimum <= (uint64_t)SIZE_MAX) ? xl6_net_future_map(xrtNetStreamWaitAvailableAsync((xnetstream*)raw, (size_t)minimum), xl6_net_map_bool) : 0; }\n"
        "int xl6_xrt_tcp_stream_drain(void* raw, long long timeoutMs) { return (raw != 0) && xrtNetStreamWait((xnetstream*)raw, XL6_XNET_WAIT_DRAIN, xl6_net_deadline(timeoutMs), 0); }\n"
        "xfuture* xl6_xrt_tcp_stream_drain_async(void* raw) { return raw != 0 ? xl6_net_future_map(xrtNetStreamWaitAsync((xnetstream*)raw, XL6_XNET_WAIT_DRAIN), xl6_net_map_bool) : 0; }\n"
        "xfuture* xl6_xrt_tcp_stream_writable_async(void* raw) { return raw != 0 ? xl6_net_future_map(xrtNetStreamWaitAsync((xnetstream*)raw, XL6_XNET_WAIT_WRITE), xl6_net_map_bool) : 0; }\n\n",
        out
    );

    xl6OutputPuts(
        "static char* xl6_net_stream_address(void* raw, bool remote) { xnetaddr address; bool ok = raw != 0 && (remote ? xrtNetStreamRemote((xnetstream*)raw, &address) : xrtNetStreamLocal((xnetstream*)raw, &address)); return ok ? xl6_net_address_string(&address) : xl6_xrt_string_dup(\"\", 0u); }\n"
        "void* xl6_xrt_tcp_stream_local_addr(void* raw) { return xl6_net_stream_address(raw, false); }\n"
        "void* xl6_xrt_tcp_stream_remote_addr(void* raw) { return xl6_net_stream_address(raw, true); }\n"
        "long long xl6_xrt_tcp_stream_local_port(void* raw) { xnetaddr address; return (raw != 0) && xrtNetStreamLocal((xnetstream*)raw, &address) ? address.Port : 0; }\n"
        "long long xl6_xrt_tcp_stream_remote_port(void* raw) { xnetaddr address; return (raw != 0) && xrtNetStreamRemote((xnetstream*)raw, &address) ? address.Port : 0; }\n"
        "int xl6_xrt_net_stream_wait(void* raw, long long kind, long long timeoutMs) { static const int waits[4] = { XL6_XNET_WAIT_READ, XL6_XNET_WAIT_WRITE, XL6_XNET_WAIT_DRAIN, XL6_XNET_WAIT_CLOSE }; return (raw != 0) && (kind >= 0) && (kind < 4) && xrtNetStreamWait((xnetstream*)raw, waits[kind], xl6_net_deadline(timeoutMs), 0); }\n"
        , out);
    if (needs->tcpVectors) xl6OutputPuts(
        "int xl6_xrt_net_stream_send_vec(void* raw, xvalue* buffers)\n"
        "{\n"
        "    xnetspan local[16]; size_t count = buffers != 0 ? xrtValueCount(buffers) : 0u; size_t i; xnetspan* spans; xnetresult resultCode;\n"
        "    if ( (raw == 0) || (count == 0u) ) { return 0; }\n"
        "    spans = count <= 16u ? local : (xnetspan*)xrtCalloc(count, sizeof(*spans)); if ( spans == 0 ) { return 0; }\n"
        "    for ( i = 0u; i < count; ++i ) { xbuffer* buffer = (xbuffer*)xl6ValueHandleData(xrtValueArrayGet(buffers, i)); if ( buffer == 0 ) { if ( spans != local ) { xrtFree(spans); } return 0; } spans[i].Data = buffer->Data; spans[i].Size = buffer->Size; }\n"
        "    resultCode = xrtNetStreamSendVec((xnetstream*)raw, spans, count); if ( spans != local ) { xrtFree(spans); } return resultCode == XL6_XNET_OK;\n"
        "}\n", out);
    xl6OutputPuts(
        "int xl6_xrt_net_stream_send_move(void* raw, void* bufferRaw) { xbuffer* buffer = (xbuffer*)bufferRaw; xnetresult resultCode; if ( (raw == 0) || (buffer == 0) ) { return 0; } if ( buffer->Size == 0u ) { return 1; } resultCode = xrtNetStreamSendTake((xnetstream*)raw, buffer->Data, buffer->Size); if ( resultCode == XL6_XNET_OK ) { buffer->Data = 0; buffer->Size = 0u; buffer->Capacity = 0u; } return resultCode == XL6_XNET_OK; }\n\n",
        out
    );
    }

    if ( needs->tcpServer ) {
    xl6OutputPuts(
        "void xl6_xrt_tcp_listener_destroy(void* raw) { xnetlistener* listener = (xnetlistener*)raw; if ( listener != 0 ) { (void)xrtNetListenerClose(listener); xrtNetListenerDestroy(listener); } }\n"
		"uint16_t xl6_xrt_tcp_listener_port(void* raw) { xnetaddr address; return (raw != 0) && xrtNetListenerLocal((xnetlistener*)raw, &address) ? address.Port : 0; }\n"
		"int xl6_xrt_tcp_listener_close(void* raw) { return (raw != 0) && xrtNetListenerClose((xnetlistener*)raw); }\n"
		"long long xl6_xrt_tcp_listener_state(void* raw) { return raw != 0 ? (long long)xrtNetListenerState((xnetlistener*)raw) : (long long)XNET_LISTENER_CLOSED; }\n"
        "void* xl6_xrt_tcp_accept(void* raw, long long timeoutMs) { return raw != 0 ? xrtNetListenerAcceptWait((xnetlistener*)raw, xl6_net_deadline(timeoutMs), 0) : 0; }\n"
        "xfuture* xl6_xrt_tcp_accept_async(void* raw) { return raw != 0 ? xl6_net_future_map(xrtNetListenerAcceptAsync((xnetlistener*)raw), xl6_net_map_stream) : 0; }\n\n",
        out
    );
    }
}

/* Statistics are transport mechanics; the language-facing NetStats shape
 * remains owned by verified XLIR. */
void xl6CgenEmitNetTcpStatsComponent(
    XL6_Output* out,
    const XL6_CgenNetNeeds* needs
)
{
    if ( out == NULL || needs == NULL || !needs->stats ) { return; }
    xl6OutputPuts(
        "#ifndef XL6_NET_STATS_RUNTIME\n"
        "#define XL6_NET_STATS_RUNTIME\n"
        "typedef struct xl6_net_stats_snapshot { size_t Count; uint64_t Values[34]; } xl6_net_stats_snapshot;\n"
        "static xl6_net_stats_snapshot* xl6_net_stats_create(size_t count)\n"
        "{ xl6_net_stats_snapshot* value = (xl6_net_stats_snapshot*)xrtCalloc(1u, sizeof(*value)); if (value != NULL) value->Count = count; return value; }\n"
        "void xl6_xrt_net_stats_destroy(void* raw) { xrtFree(raw); }\n"
        "uint64_t xl6_xrt_net_stats_value(void* raw, int64_t index)\n"
        "{ xl6_net_stats_snapshot* value = (xl6_net_stats_snapshot*)raw; return value != NULL && index >= 0 && (size_t)index < value->Count ? value->Values[index] : UINT64_C(0); }\n"
        "#endif\n\n",
        out);
    if ( needs->tcpStream )
        xl6OutputPuts(
            "void* xl6_xrt_tcp_stream_stats(void* raw)\n"
            "{\n"
            "    xnetstreamstats value; xl6_net_stats_snapshot* snapshot;\n"
            "    if (raw == NULL || !xrtNetStreamStats((xnetstream*)raw, &value)) return NULL;\n"
            "    snapshot = xl6_net_stats_create(14u); if (snapshot == NULL) return NULL;\n"
            "    snapshot->Values[0] = (uint64_t)value.State; snapshot->Values[1] = (uint64_t)value.ReceivedBytes; snapshot->Values[2] = (uint64_t)value.SentBytes;\n"
            "    snapshot->Values[3] = (uint64_t)value.ReadEvents; snapshot->Values[4] = (uint64_t)value.WriteEvents; snapshot->Values[5] = (uint64_t)value.SendRejected;\n"
            "    snapshot->Values[6] = (uint64_t)value.BufferedBytes; snapshot->Values[7] = (uint64_t)value.QueuedBytes; snapshot->Values[8] = (uint64_t)value.PeakQueuedBytes;\n"
            "    snapshot->Values[9] = (uint64_t)value.ReadPaused; snapshot->Values[10] = (uint64_t)value.ReadBlocked; snapshot->Values[11] = (uint64_t)value.ReadEnded;\n"
            "    snapshot->Values[12] = (uint64_t)value.WriteEnded; snapshot->Values[13] = (uint64_t)value.WriteBackpressured; return snapshot;\n"
            "}\n\n",
            out);
    if ( needs->tcpServer )
        xl6OutputPuts(
            "void* xl6_xrt_tcp_listener_stats(void* raw)\n"
            "{\n"
            "    xnetlistenerstats value; xl6_net_stats_snapshot* snapshot;\n"
            "    if (raw == NULL || !xrtNetListenerStats((xnetlistener*)raw, &value)) return NULL;\n"
            "    snapshot = xl6_net_stats_create(9u); if (snapshot == NULL) return NULL;\n"
            "    snapshot->Values[0] = (uint64_t)value.State; snapshot->Values[1] = (uint64_t)value.Accepted; snapshot->Values[2] = (uint64_t)value.Rejected;\n"
            "    snapshot->Values[3] = (uint64_t)value.Errors; snapshot->Values[4] = (uint64_t)value.ActiveAccepts; snapshot->Values[5] = (uint64_t)value.ActiveDispatches;\n"
            "    snapshot->Values[6] = (uint64_t)value.QueuedAccepts; snapshot->Values[7] = (uint64_t)value.PeakQueuedAccepts; snapshot->Values[8] = (uint64_t)value.AcceptWaiters; return snapshot;\n"
            "}\n\n",
            out);
}






/* 输出 Buffer 所需的当前 TCP 字节桥。 */
void xl6CgenEmitNetBufferComponent(
    XL6_Output* out,
    const XL6_CgenNetNeeds* needs
)
{
    if ( needs->bytes ) {
    xl6OutputPuts(
        "void xl6_xrt_net_bytes_destroy(void* raw) { xrtNetBytesDestroy((xnetbytes*)raw); }\n"
        "long long xl6_xrt_net_bytes_len(void* raw) { return raw != 0 ? (long long)xrtNetBytesView((xnetbytes*)raw).Size : 0; }\n"
        "void* xl6_xrt_net_bytes_data(void* raw) { return raw != 0 ? (void*)xrtNetBytesView((xnetbytes*)raw).Data : 0; }\n"
        "void* xl6_xrt_net_bytes_buffer(void* raw) { XL6_XrtBytesView view = raw != 0 ? xrtNetBytesView((xnetbytes*)raw) : (XL6_XrtBytesView){ 0, 0u }; return xl6_xrt_buffer_from_view_bytes(view.Data, view.Size); }\n"
        "void* xl6_xrt_net_bytes_text(void* raw) { XL6_XrtBytesView view = raw != 0 ? xrtNetBytesView((xnetbytes*)raw) : (XL6_XrtBytesView){ 0, 0u }; return xl6_xrt_string_dup((const char*)view.Data, view.Size); }\n\n",
        out
    );
    }

    if ( needs->tcpStream ) {
    xl6OutputPuts(
        "typedef const xnetbuf* (*xl6_net_buffer_view_proc)(void* transport);\n"
        "typedef size_t (*xl6_net_buffer_consume_proc)(void* transport, size_t size);\n\n"
        "typedef enum xl6_net_buffer_command_kind {\n"
        "    XL6_NET_BUFFER_PEEK = 1,\n"
        "    XL6_NET_BUFFER_CONSUME\n"
        "} xl6_net_buffer_command_kind;\n\n"
        "typedef struct xl6_net_buffer_command {\n"
        "    xevent Done;\n"
        "    xnetpost Post;\n"
        "    void* Transport;\n"
        "    xl6_net_buffer_view_proc View;\n"
        "    xl6_net_buffer_consume_proc Consume;\n"
        "    xl6_net_buffer_command_kind Kind;\n"
        "    size_t Size;\n"
        "    void* Buffer;\n"
        "    bool Boolean;\n"
        "    bool Signal;\n"
        "} xl6_net_buffer_command;\n\n"
        "/* 在传输所属 Worker 内复制快照或精确消费明文前缀。 */\n"
        "static void xl6_net_buffer_command_run(xnetworker* worker, void* data)\n"
        "{\n"
        "    xl6_net_buffer_command* command = (xl6_net_buffer_command*)data;\n"
        "    const xnetbuf* buffer;\n"
        "    size_t available;\n"
        "    size_t size;\n"
        "    void* bytes;\n\n"
        "    (void)worker;\n"
        "    buffer = command->View != 0 ? command->View(command->Transport) : 0;\n"
        "    available = buffer != 0 ? xrtNetBufSize(buffer) : 0u;\n"
        "    if ( command->Kind == XL6_NET_BUFFER_PEEK ) {\n"
        "        size = (command->Size == 0u) || (command->Size > available) ? available : command->Size;\n"
        "        if ( size == 0u ) {\n"
        "            command->Buffer = xrtBufferCreate();\n"
        "        } else {\n"
        "            bytes = xrtMalloc(size);\n"
        "            if ( (bytes != 0) && (xrtNetBufPeek(buffer, 0u, bytes, size) == size) ) {\n"
        "                command->Buffer = xl6_xrt_buffer_from_owned_bytes(bytes, size);\n"
        "            } else {\n"
        "                xrtFree(bytes);\n"
        "            }\n"
        "        }\n"
        "    } else if ( command->Kind == XL6_NET_BUFFER_CONSUME ) {\n"
        "        command->Boolean = (command->Size == 0u) ||\n"
        "            ((command->Consume != 0) && (command->Size <= available) &&\n"
        "             (command->Consume(command->Transport, command->Size) == command->Size));\n"
        "    }\n"
        "    if ( command->Signal ) { (void)xrtEventSet(&command->Done); }\n"
        "}\n\n"
        "/* 跨线程同步编组只覆盖内存操作，不在 Worker 内等待网络事件。 */\n"
        "static bool xl6_net_buffer_command_execute(xnetworker* worker, xl6_net_buffer_command* command)\n"
        "{\n"
        "    bool posted;\n"
        "    bool waited;\n\n"
        "    if ( (worker == 0) || (command == 0) ) { return false; }\n"
        "    if ( xrtNetWorkerIsCurrent(worker) ) {\n"
        "        command->Signal = false;\n"
        "        xl6_net_buffer_command_run(worker, command);\n"
        "        return true;\n"
        "    }\n"
        "    if ( !xrtEventInit(&command->Done, false, false) ) { return false; }\n"
        "    if ( !xrtNetPostInit(&command->Post) ) {\n"
        "        (void)xrtEventUnit(&command->Done);\n"
        "        return false;\n"
        "    }\n"
        "    command->Signal = true;\n"
        "    posted = xrtNetPost(worker, &command->Post, xl6_net_buffer_command_run, command);\n"
        "    waited = posted && (xrtEventWait(&command->Done) == XWAIT_OK);\n"
        "    /* Done is caller-owned embedded storage, not an EventCreate allocation. */\n"
        "    (void)xrtEventUnit(&command->Done);\n"
        "    return waited;\n"
        "}\n\n",
        out
    );
    }

    if ( needs->tcpStream ) {
    xl6OutputPuts(
        "static void* xl6_net_stream_recv_buffer(xnetstream* stream, size_t maxBytes, xdeadline deadline)\n"
        "{\n"
        "    xnetbytes* bytes; XL6_XrtBytesView view; void* buffer;\n"
        "    if ( stream == 0 ) { return 0; }\n"
        "    bytes = xrtNetStreamRecv(stream, maxBytes, deadline, 0); if ( bytes == 0 ) { return 0; }\n"
        "    view = xrtNetBytesView(bytes); buffer = xl6_xrt_buffer_from_view_bytes(view.Data, view.Size); xrtNetBytesDestroy(bytes); return buffer;\n"
        "}\n"
        "void* xl6_xrt_tcp_stream_read_buffer(void* raw, size_t maxBytes) { return xl6_net_stream_recv_buffer((xnetstream*)raw, maxBytes, xrtDeadlineAfter(0u)); }\n"
        "void* xl6_xrt_tcp_stream_recv_buffer(void* raw, size_t maxBytes, long long timeoutMs) { return xl6_net_stream_recv_buffer((xnetstream*)raw, maxBytes, xl6_net_deadline(timeoutMs)); }\n\n"
        "static const xnetbuf* xl6_net_tcp_buffer_view(void* transport) { return xrtNetStreamBuffer((xnetstream*)transport); }\n"
        "static size_t xl6_net_tcp_buffer_consume(void* transport, size_t size) { return xrtNetStreamConsume((xnetstream*)transport, size); }\n\n"
        "void* xl6_xrt_tcp_stream_peek_buffer(void* raw, long long maxBytes)\n"
        "{\n"
        "    xnetstream* stream = raw != 0 && maxBytes >= 0 ? xrtNetStreamRef((xnetstream*)raw) : 0;\n"
        "    xnetworker* worker = stream != 0 ? xrtNetStreamWorker(stream) : 0;\n"
        "    xl6_net_buffer_command command = { 0 };\n\n"
        "    if ( (stream == 0) || ((uint64_t)maxBytes > (uint64_t)SIZE_MAX) ) { xrtNetStreamDestroy(stream); return 0; }\n"
        "    command.Transport = stream; command.View = xl6_net_tcp_buffer_view;\n"
        "    command.Kind = XL6_NET_BUFFER_PEEK; command.Size = (size_t)maxBytes;\n"
        "    if ( !xl6_net_buffer_command_execute(worker, &command) ) { command.Buffer = 0; }\n"
        "    xrtNetStreamDestroy(stream);\n"
        "    return command.Buffer;\n"
        "}\n\n"
        "int xl6_xrt_tcp_stream_consume_exact(void* raw, long long size)\n"
        "{\n"
        "    xnetstream* stream = raw != 0 && size >= 0 ? xrtNetStreamRef((xnetstream*)raw) : 0;\n"
        "    xnetworker* worker = stream != 0 ? xrtNetStreamWorker(stream) : 0;\n"
        "    xl6_net_buffer_command command = { 0 };\n\n"
        "    if ( (stream == 0) || ((uint64_t)size > (uint64_t)SIZE_MAX) ) { xrtNetStreamDestroy(stream); return 0; }\n"
        "    command.Transport = stream; command.View = xl6_net_tcp_buffer_view;\n"
        "    command.Consume = xl6_net_tcp_buffer_consume;\n"
        "    command.Kind = XL6_NET_BUFFER_CONSUME; command.Size = (size_t)size;\n"
        "    if ( !xl6_net_buffer_command_execute(worker, &command) ) { command.Boolean = false; }\n"
        "    xrtNetStreamDestroy(stream);\n"
        "    return command.Boolean;\n"
        "}\n\n",
        out
    );
    }

}
