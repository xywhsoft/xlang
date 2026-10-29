#include "xl_clock.h"
#include "include/mqtt_native.h"
#include "mqtt_internal.h"



/* 窄化仅发生在范围验证之后，不把语言 int64 静默截断为协议字段。 */
static bool range(int64_t value, int64_t minimum, int64_t maximum)
{
    return (value >= minimum && value <= maximum) ||
        xmqtt_error(XERR_RANGE, "mqtt: argument is outside the protocol field range");
}

static bool deadline(int64_t timeout, XL6_DeadlineV1* result)
{
    if (timeout < -1)
        return xmqtt_error(XERR_RANGE, "mqtt: timeout must be -1 or nonnegative milliseconds");
    *result = timeout == -1 ? XL6_DEADLINE_NEVER : xl6DeadlineAfterMillisecondsV1((uint64_t)timeout);
    return true;
}



void* xmq_connect_frame(const char* client, const char* username, void* password,
    size_t size, int64_t keepalive, bool clean)
{
    if (!range(keepalive, 0, UINT16_MAX)) return NULL;
    return xrtMqttConnect(xrtStrView(client), xrtStrView(username),
        (xbytesview){password, size}, (uint16)keepalive, clean);
}

void* xmq_publish_frame(int64_t qos, const char* topic, void* payload,
    size_t size, int64_t packetId, bool retain, bool duplicate)
{
    if (!range(qos, 0, 1) || !range(packetId, 0, UINT16_MAX)) return NULL;
    return xrtMqttPublish((uint8)qos, xrtStrView(topic), (xbytesview){payload, size},
        (uint16)packetId, retain, duplicate);
}

void* xmq_connect_text(const char* client, const char* username, const char* password,
    int64_t keepalive, bool clean)
{
    xstrview text = xrtStrView(password);
    return xmq_connect_frame(client, username, (void*)text.Data, text.Size, keepalive, clean);
}

void* xmq_publish_text(int64_t qos, const char* topic, const char* payload,
    int64_t packetId, bool retain, bool duplicate)
{
    xstrview text = xrtStrView(payload);
    return xmq_publish_frame(qos, topic, (void*)text.Data, text.Size, packetId, retain, duplicate);
}

void* xmq_puback_frame(int64_t packetId)
{
    return range(packetId, 1, UINT16_MAX) ? xrtMqttPuback((uint16)packetId) : NULL;
}

void* xmq_subscribe_frame(int64_t packetId, const char* filter, int64_t qos)
{
    if (!range(packetId, 1, UINT16_MAX) || !range(qos, 0, 1)) return NULL;
    return xrtMqttSubscribe((uint16)packetId, xrtStrView(filter), (uint8)qos);
}

void* xmq_pingreq_frame(void) { return xrtMqttPingreq(); }
void* xmq_disconnect_frame(void) { return xrtMqttDisconnect(); }



/* Hex 只是展示/调试入口；数据转换与内存分配统一由 XRT 负责。 */
void* xmq_hex_to_frame(const char* hex)
{
    return xrtBufferFromHex(xrtStrView(hex), 0);
}

void* xmq_frame_to_hex(void* data, size_t size)
{
    return xrtHexEncodeNew(data, size, 0);
}

xvalue* xmq_parse_frame(void* data, size_t size)
{
    xvalue* result = xrtMqttParse((xbytesview){data, size});
    /* C 的 NULL 失败不是语言 var 的 null 值；桥接层保留原始错误并提供合法空值。 */
    return result != NULL ? result : xrtValueNull();
}

void* xmq_payload_bytes(void* data, size_t size)
{
    xmqttmessage message;
    return xrtMqttPublishView((xbytesview){data, size}, &message) ?
        xrtBufferFrom(message.Payload) : NULL;
}

int64_t xmq_frame_size(void* data, size_t size)
{
    xmqttheader header;
    if (xrtMqttInspect((xbytesview){data, size}, &header) == XMQTT_ERROR) return -1;
    return (int64_t)header.PacketSize;
}



bool xmq_send_frame(void* stream, void* data, size_t size, int64_t timeout, void* cancel)
{
    XL6_DeadlineV1 until;
    return deadline(timeout, &until) &&
        xrtMqttSendFrame(stream, (xbytesview){data, size}, until, cancel);
}

void* xmq_recv_frame(void* stream, int64_t limit, int64_t timeout, void* cancel)
{
    XL6_DeadlineV1 until;
    if (!range(limit, 2, (int64_t)XMQTT_MAX_REMAINING + 5) || !deadline(timeout, &until)) return NULL;
    return xrtMqttRecvFrame(stream, (size_t)limit, until, cancel);
}

xvalue* xmq_handshake(void* stream, const char* client, const char* username,
    const char* password, int64_t keepalive, int64_t timeout, void* cancel)
{
    XL6_DeadlineV1 until;
    xvalue* result;
    xstrview secret = xrtStrView(password);
    if (!range(keepalive, 0, UINT16_MAX) || !deadline(timeout, &until)) return xrtValueNull();
    result = xrtMqttHandshake(stream, xrtStrView(client), xrtStrView(username),
        (xbytesview){(cbytes)secret.Data, secret.Size}, (uint16)keepalive, until, cancel);
    return result != NULL ? result : xrtValueNull();
}
