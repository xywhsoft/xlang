/* Backend-independent transport mechanics. No AST, class names or type
 * recovery: language object/Future adapters are supplied by verified IR. */
void xl6CgenEmitNetDatagramComponent(XL6_Output* out, const XL6_CgenNetDatagramNeeds* needs)
{
    if (out == NULL || needs == NULL) return;
    if (needs->socket) xl6OutputPuts(
        "void* xl6_xrt_udp_bind(xlir_string host, int64_t port)\n"
        "{ xnetaddr address; xnetudpconfig config; const char* chost = xlir_string_cstr(host); if (xl6_net_engine == NULL || chost == NULL || !xl6_net_address_parse(&address, chost, (uint16_t)port, true)) return NULL; xrtNetUdpConfigInit(&config); return xrtNetUdpBind(xl6_net_engine, &address, 0u, &config, NULL, NULL); }\n"
        "void xl6_xrt_udp_destroy(void* raw) { if (raw != NULL) { (void)xrtNetUdpClose((xnetudp*)raw); xrtNetUdpDestroy((xnetudp*)raw); } }\n"
        "int64_t xl6_xrt_udp_local_port(void* raw) { xnetaddr address; return raw != NULL && xrtNetUdpLocal((xnetudp*)raw, &address) ? address.Port : 0; }\n"
        "int64_t xl6_xrt_udp_send_bytes_result(void* raw, xlir_string host, int64_t port, void* data, size_t size)\n"
        "{ xnetaddr address; const char* chost = xlir_string_cstr(host); return raw != NULL && chost != NULL && (data != NULL || size == 0) && xl6_net_address_parse(&address, chost, (uint16_t)port, false) ? (int64_t)xrtNetUdpSendTo((xnetudp*)raw, &address, data, size) : XNET_RESULT_ERROR; }\n"
        "bool xl6_xrt_udp_send_bytes(void* raw, xlir_string host, int64_t port, void* data, size_t size)\n"
        "{ return xl6_xrt_udp_send_bytes_result(raw, host, port, data, size) == XNET_RESULT_OK; }\n"
        "bool xl6_xrt_udp_send_text(void* raw, xlir_string host, int64_t port, xlir_string text)\n"
        "{ return text.Data != NULL && xl6_xrt_udp_send_bytes_result(raw, host, port, (void*)text.Data, text.Size) == XNET_RESULT_OK; }\n"
        "bool xl6_xrt_udp_send_move(void* raw, xlir_string host, int64_t port, void* bufferRaw)\n"
        "{ xbuffer* buffer = (xbuffer*)bufferRaw; xnetaddr address; xnetresult result; const char* chost = xlir_string_cstr(host); if (raw == NULL || buffer == NULL || chost == NULL || !xl6_net_address_parse(&address, chost, (uint16_t)port, false)) return false; if (buffer->Size == 0) return true; result = xrtNetUdpSendTakeTo((xnetudp*)raw, &address, buffer->Data, buffer->Size); if (result == XNET_RESULT_OK) { buffer->Data = NULL; buffer->Size = 0; buffer->Capacity = 0; } return result == XNET_RESULT_OK; }\n"
        "void* xl6_xrt_udp_recv(void* raw, int64_t timeout) { return raw != NULL ? xrtNetUdpReceiveWait((xnetudp*)raw, xl6_net_deadline(timeout), NULL) : NULL; }\n"
        "int64_t xl6_xrt_udp_pending(void* raw) { return raw != NULL ? (int64_t)xrtNetUdpPending((xnetudp*)raw) : 0; }\n"
        "int64_t xl6_xrt_udp_queued(void* raw) { return raw != NULL ? (int64_t)xrtNetUdpQueued((xnetudp*)raw) : 0; }\n"
        "int64_t xl6_xrt_udp_queued_bytes(void* raw) { return raw != NULL ? (int64_t)xrtNetUdpQueuedBytes((xnetudp*)raw) : 0; }\n"
        "bool xl6_xrt_udp_writable(void* raw, size_t size, int64_t timeout) { return raw != NULL && xrtNetUdpWritable((xnetudp*)raw, size, xl6_net_deadline(timeout), NULL); }\n", out);
    if (needs->buffers) xl6OutputPuts(
        "bool xl6_xrt_udp_send_vec(void* raw, xlir_string host, int64_t port, xvalue* buffers)\n"
        "{\n"
        "    xnetspan local[16], *spans; xnetaddr address; size_t i, count = buffers != NULL ? xrtValueCount(buffers) : 0; xnetresult result; const char* chost = xlir_string_cstr(host);\n"
        "    if (raw == NULL || chost == NULL || count == 0 || count > SIZE_MAX / sizeof(*spans) || !xl6_net_address_parse(&address, chost, (uint16_t)port, false)) return false;\n"
        "    spans = count <= 16 ? local : (xnetspan*)xrtCalloc(count, sizeof(*spans)); if (spans == NULL) return false;\n"
        "    for (i = 0; i < count; ++i) { xbuffer* buffer = xlir_datagram_buffer_borrow(xrtValueArrayGet(buffers, i)); if (buffer == NULL) { if (spans != local) xrtFree(spans); return false; } spans[i].Data = buffer->Data; spans[i].Size = buffer->Size; }\n"
        "    result = xrtNetUdpSendVecTo((xnetudp*)raw, &address, spans, count); if (spans != local) xrtFree(spans); return result == XNET_RESULT_OK;\n"
        "}\n"
        "int64_t xl6_xrt_udp_send_batch(void* raw, xlir_string host, int64_t port, xvalue* buffers)\n"
        "{\n"
        "    xnetdgramsend local[16], *items; xnetaddr address; size_t i, accepted = 0, count = buffers != NULL ? xrtValueCount(buffers) : 0; xnetresult result; const char* chost = xlir_string_cstr(host);\n"
        "    if (raw == NULL || chost == NULL || count == 0 || count > SIZE_MAX / sizeof(*items) || !xl6_net_address_parse(&address, chost, (uint16_t)port, false)) return 0;\n"
        "    items = count <= 16 ? local : (xnetdgramsend*)xrtCalloc(count, sizeof(*items)); if (items == NULL) return 0;\n"
        "    for (i = 0; i < count; ++i) { xbuffer* buffer = xlir_datagram_buffer_borrow(xrtValueArrayGet(buffers, i)); if (buffer == NULL) { if (items != local) xrtFree(items); return 0; } items[i].Remote = &address; items[i].Data = buffer->Data; items[i].Size = buffer->Size; }\n"
        "    result = xrtNetUdpSendBatch((xnetudp*)raw, items, count, &accepted); if (items != local) xrtFree(items); return result == XNET_RESULT_ERROR ? -1 : (int64_t)accepted;\n"
        "}\n", out);
    if (needs->recvAsync) xl6OutputPuts(
        "xfuture* xl6_xrt_udp_recv_async(void* raw) { return raw != NULL ? xlir_datagram_future_map(xrtNetUdpReceiveAsync((xnetudp*)raw), xlir_datagram_map_packet, NULL) : NULL; }\n", out);
    if (needs->packet) xl6OutputPuts(
        "void xl6_xrt_udp_packet_destroy(void* raw) { xrtNetUdpPacketDestroy((xnetudppacket*)raw); }\n"
        "void* xl6_xrt_udp_packet_text(void* raw) { size_t size = raw != NULL ? xrtNetUdpPacketSize((xnetudppacket*)raw) : 0; return xrtStrDupN(raw != NULL ? (const char*)xrtNetUdpPacketData((xnetudppacket*)raw) : \"\", size); }\n"
        "void* xl6_xrt_udp_packet_buffer(void* raw) { xbuffer* buffer; size_t size; const unsigned char* data; if (raw == NULL) return NULL; size = xrtNetUdpPacketSize((xnetudppacket*)raw); data = xrtNetUdpPacketData((xnetudppacket*)raw); buffer = xrtBufferCreate(); if (buffer == NULL) return NULL; if (size != 0 && !xrtBufferAppend(buffer, (xbytesview){ data, size })) { xrtBufferDestroy(buffer); return NULL; } return buffer; }\n"
        "void* xl6_xrt_udp_packet_addr(void* raw) { const xnetaddr* remote = raw != NULL ? xrtNetUdpPacketRemote((xnetudppacket*)raw) : NULL; return remote != NULL ? xrtNetAddrString(remote) : xrtStrDup(\"\"); }\n"
        "int64_t xl6_xrt_udp_packet_port(void* raw) { const xnetaddr* remote = raw != NULL ? xrtNetUdpPacketRemote((xnetudppacket*)raw) : NULL; return remote != NULL ? (int64_t)remote->Port : 0; }\n"
        "bool xl6_xrt_udp_packet_truncated(void* raw) { return raw != NULL && xrtNetUdpPacketTruncated((xnetudppacket*)raw); }\n", out);
    if (needs->batch) xl6OutputPuts(
        "void* xl6_xrt_udp_recv_batch(void* raw, size_t capacity, int64_t timeout) { return raw != NULL && capacity > 0 ? xrtNetUdpReceiveBatchWait((xnetudp*)raw, capacity, xl6_net_deadline(timeout), NULL) : NULL; }\n"
        "void xl6_xrt_udp_batch_destroy(void* raw) { xrtNetUdpBatchDestroy((xnetudpbatch*)raw); }\n"
        "int64_t xl6_xrt_udp_batch_count(void* raw) { return raw != NULL ? (int64_t)xrtNetUdpBatchCount((xnetudpbatch*)raw) : 0; }\n"
        "void* xl6_xrt_udp_batch_take(void* raw, int64_t index) { return raw != NULL && index >= 0 ? xrtNetUdpBatchTake((xnetudpbatch*)raw, (size_t)index) : NULL; }\n", out);
    if (needs->engineStats || needs->udpStats) {
        XL6_CgenNetNeeds common = {0}; common.stats = 1;
        xl6CgenEmitNetTcpStatsComponent(out, &common);
        xl6OutputPuts("static void* xlir_datagram_stats(const uint64_t* values, size_t count) { xl6_net_stats_snapshot* snapshot = xl6_net_stats_create(count); if (snapshot != NULL) memcpy(snapshot->Values, values, count * sizeof(*values)); return snapshot; }\n", out);
    }
    if (needs->engineStats) xl6OutputPuts(
        "void* xl6_xrt_net_runtime_stats(void) { xnetenginestats v; if (xl6_net_engine == NULL || !xrtNetEngineStats(xl6_net_engine, &v)) return NULL; { const uint64_t a[] = {v.State,v.Workers,v.PostsAccepted,v.PostsRejected,v.PostsExecuted,v.TimersAccepted,v.TimersRejected,v.TimersFired,v.TimersCancelled,v.TimersClosed,v.TimerErrors,v.Events,v.WaitErrors,v.NodeCacheHits,v.NodeCacheMisses,v.PendingCommands,v.ActiveTimers,v.NodeCachedBytes,v.LiveObjects}; return xlir_datagram_stats(a, sizeof(a)/sizeof(a[0])); } }\n", out);
    if (needs->udpStats) xl6OutputPuts(
        "void* xl6_xrt_udp_stats(void* raw) { xnetudpstats v; if (raw == NULL || !xrtNetUdpStats((xnetudp*)raw, &v)) return NULL; { const uint64_t a[] = {v.State,v.ReceivedPackets,v.ReceivedBytes,v.SentPackets,v.SentBytes,v.Truncated,v.TruncatedDropped,v.DroppedNewest,v.DroppedOldest,v.ReceiveErrors,v.SendErrors,v.SendRejected,v.DatagramErrors,v.DatagramErrorsDropped,v.PathMtuUpdates,v.PathMtu,v.QueuedBytes,v.PeakQueuedBytes,v.QueuedPackets,v.PeakQueuedPackets,v.ReceiveQueued,v.PeakReceiveQueued,v.ReceiveQueuedBytes,v.PeakReceiveQueuedBytes,v.ErrorQueued,v.PeakErrorQueued,v.ErrorQueuedBytes,v.PeakErrorQueuedBytes,v.ReceiveWaiters,v.ErrorWaiters,v.ActiveReceives,v.ActiveSends,v.PeakActiveSends,v.Connected}; return xlir_datagram_stats(a, sizeof(a)/sizeof(a[0])); } }\n", out);
}
