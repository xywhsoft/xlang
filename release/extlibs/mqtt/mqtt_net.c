#include "include/xrt_mqtt_net.h"
#include "mqtt_internal.h"



static void abort_preserving_error(xnetstream* pStream)
{
    xerror* error = xrtTakeError();
    (void)xrtNetStreamAbort(pStream);
    xrtSetErrorTake(error);
}

static bool budget(xdeadline Deadline, xcancel* pCancel)
{
    if (xrtCancelRequested(pCancel))
        return xmqtt_error(XERR_CANCELLED, "mqtt: operation cancelled");
    if (xrtDeadlineExpired(Deadline))
        return xmqtt_error(XERR_TIMEOUT, "mqtt: deadline expired");
    return true;
}



bool xrtMqttSendFrame(xnetstream* pStream, xbytesview Frame,
    xdeadline Deadline, xcancel* pCancel)
{
    xmqttheader header;
    xmqttstatus status;
    size_t position = 0, limit;
    if (pStream == NULL) return xmqtt_error(XERR_ARGUMENT, "mqtt: stream is null");
    status = xrtMqttInspect(Frame, &header);
    if (status == XMQTT_ERROR) return false;
    if (status != XMQTT_READY || header.PacketSize != Frame.Size)
        return xmqtt_error(XERR_PROTOCOL, "mqtt: send requires exactly one complete packet");
    limit = xrtNetStreamWriteLimit(pStream);
    if (limit == 0) return xmqtt_error(XERR_STATE, "mqtt: stream has no send capacity");

    /* XRT 队列每次完整受理一个片段，系统短写由 XRT 驱动；大报文不受单次队列容量限制。 */
    while (position < Frame.Size) {
        size_t count = Frame.Size - position;
        xnetresult result;
        if (!budget(Deadline, pCancel)) goto fail;
        if (count > limit) count = limit;
        result = xrtNetStreamSend(pStream, Frame.Data + position, count);
        if (result == XNET_RESULT_OK) position += count;
        else if (result != XNET_RESULT_AGAIN) {
            if (result != XNET_RESULT_ERROR)
                xmqtt_error(XERR_IO, "mqtt: stream closed during send");
            goto fail;
        }
        if (!xrtNetStreamWait(pStream, XNET_STREAM_WAIT_DRAIN, Deadline, pCancel)) goto fail;
    }
    return true;
fail:
    if (position != 0) abort_preserving_error(pStream);
    return false;
}



xbuffer* xrtMqttRecvFrame(xnetstream* pStream, size_t iPacketLimit,
    xdeadline Deadline, xcancel* pCancel)
{
    xbuffer* frame;
    xmqttheader header = {0};
    bool started = false;
    if (pStream == NULL || iPacketLimit < 2 ||
        iPacketLimit > (size_t)XMQTT_MAX_REMAINING + 5) {
        xmqtt_error(XERR_ARGUMENT, "mqtt: invalid stream or packet limit");
        return NULL;
    }
    if (!budget(Deadline, pCancel)) return NULL;
    frame = xrtBufferCreate();
    if (frame == NULL) return NULL;
    if (!xrtBufferReserve(frame, 5)) goto fail;

    /* 等首字节只观察、不消费；空连接等待超时/取消后仍可重试。 */
    if (!xrtNetStreamWaitAvailable(pStream, 1, Deadline, pCancel)) goto fail;
    for (;;) {
        xnetbytes* bytes;
        xbytesview data;
        xmqttstatus status;
        size_t needed = header.HeaderSize == 0 ? 1 : header.PacketSize - frame->Size;
        if (!budget(Deadline, pCancel)) goto fail;
        started = true;
        bytes = xrtNetStreamRecv(pStream, needed, Deadline, pCancel);
        if (bytes == NULL) goto fail;
        data = xrtNetBytesView(bytes);
        if (data.Size == 0 || data.Size > needed) {
            xrtNetBytesDestroy(bytes);
            xmqtt_error(XERR_PROTOCOL, "mqtt: stream ended inside a packet");
            goto fail;
        }
        memcpy(frame->Data + frame->Size, data.Data, data.Size);
        frame->Size += data.Size;
        xrtNetBytesDestroy(bytes);
        status = xrtMqttInspect(xrtBufferView(frame), &header);
        if (status == XMQTT_ERROR) goto fail;
        if (header.PacketSize > iPacketLimit) {
            xmqtt_error(XERR_RANGE, "mqtt: packet exceeds receive limit");
            goto fail;
        }
        if (status == XMQTT_READY) return frame;
        if (header.HeaderSize != 0 && !xrtBufferReserve(frame, header.PacketSize)) goto fail;
    }
fail:
    /* 读取一旦受理，超时与完成可能竞争；不能把不确定是否已消费的连接交还调用方。 */
    if (started) abort_preserving_error(pStream);
    xrtBufferDestroy(frame);
    return NULL;
}



xvalue* xrtMqttHandshake(xnetstream* pStream, xstrview ClientId, xstrview Username,
    xbytesview Password, uint16 iKeepalive, xdeadline Deadline, xcancel* pCancel)
{
    xbuffer* frame = xrtMqttConnect(ClientId, Username, Password, iKeepalive, true);
    xmqttheader header;
    xvalue* ack;
    bool sent;
    if (frame == NULL) return NULL;
    sent = xrtMqttSendFrame(pStream, xrtBufferView(frame), Deadline, pCancel);
    xrtBufferDestroy(frame);
    if (!sent) return NULL;
    frame = xrtMqttRecvFrame(pStream, 7, Deadline, pCancel);
    if (frame == NULL) { abort_preserving_error(pStream); return NULL; }
    /* CONNACK 正文固定两字节；尊重固定头的实际编码长度，不读取后续 PUBLISH。 */
    if (xrtMqttInspect(xrtBufferView(frame), &header) != XMQTT_READY || header.Type != 2 ||
        frame->Data[header.HeaderSize] != 0 || frame->Data[header.HeaderSize + 1] != 0) {
        xrtBufferDestroy(frame);
        xmqtt_error(XERR_PROTOCOL, "mqtt: connection rejected or invalid CONNACK");
        abort_preserving_error(pStream);
        return NULL;
    }
    ack = xrtMqttParse(xrtBufferView(frame));
    xrtBufferDestroy(frame);
    if (ack == NULL) abort_preserving_error(pStream);
    return ack;
}
