#include "mqtt_internal.h"



xbuffer* xrtMqttConnect(xstrview ClientId, xstrview Username,
    xbytesview Password, uint16 iKeepalive, bool bCleanSession)
{
    xbuffer* result;
    size_t pos;
    size_t size;
    uint8 flags = bCleanSession ? 2u : 0u;
    if (!xmqtt_text(ClientId, bCleanSession) || !xmqtt_text(Username, true)) return NULL;
    if (Password.Size > UINT16_MAX || (Password.Size != 0 && Password.Data == NULL) ||
        (Password.Size != 0 && Username.Size == 0)) {
        xmqtt_error(XERR_VALUE, "mqtt: password requires a username and at most 65535 bytes");
        return NULL;
    }
    size = 12 + ClientId.Size;
    if (Username.Size != 0) { flags |= 128u; size += 2 + Username.Size; }
    if (Password.Size != 0) { flags |= 64u; size += 2 + Password.Size; }
    result = xmqtt_frame(0x10, size, &pos);
    if (result == NULL) return NULL;
    xmqtt_put_text(result->Data, &pos, (xstrview){"MQTT", 4});
    result->Data[pos++] = 4;
    result->Data[pos++] = flags;
    xmqtt_put16(result->Data, &pos, iKeepalive);
    xmqtt_put_text(result->Data, &pos, ClientId);
    if (Username.Size != 0) xmqtt_put_text(result->Data, &pos, Username);
    if (Password.Size != 0) {
        xmqtt_put16(result->Data, &pos, (uint16)Password.Size);
        xmqtt_put_bytes(result->Data, &pos, Password);
    }
    return result;
}



xbuffer* xrtMqttPublish(uint8 iQoS, xstrview Topic, xbytesview Payload,
    uint16 iPacketId, bool bRetain, bool bDuplicate)
{
    size_t pos;
    size_t variable;
    xbuffer* result;
    uint8 header;
    if (iQoS > 1 || (iQoS == 0 && (iPacketId != 0 || bDuplicate)) ||
        (iQoS == 1 && iPacketId == 0)) {
        xmqtt_error(XERR_VALUE, "mqtt: invalid QoS, packet id or DUP flag");
        return NULL;
    }
    if (!xmqtt_topic(Topic, false)) return NULL;
    variable = 2 + Topic.Size + (iQoS == 1 ? 2 : 0);
    if (Payload.Size > XMQTT_MAX_REMAINING - variable ||
        (Payload.Size != 0 && Payload.Data == NULL)) {
        xmqtt_error(XERR_RANGE, "mqtt: invalid publish payload length");
        return NULL;
    }
    header = 0x30u | (iQoS << 1) | (bRetain ? 1u : 0u) | (bDuplicate ? 8u : 0u);
    result = xmqtt_frame(header, variable + Payload.Size, &pos);
    if (result == NULL) return NULL;
    xmqtt_put_text(result->Data, &pos, Topic);
    if (iQoS == 1) xmqtt_put16(result->Data, &pos, iPacketId);
    xmqtt_put_bytes(result->Data, &pos, Payload);
    return result;
}



xbuffer* xrtMqttPuback(uint16 iPacketId)
{
    size_t pos;
    xbuffer* result;
    if (iPacketId == 0) {
        xmqtt_error(XERR_VALUE, "mqtt: packet id must be nonzero");
        return NULL;
    }
    result = xmqtt_frame(0x40, 2, &pos);
    if (result != NULL) xmqtt_put16(result->Data, &pos, iPacketId);
    return result;
}



xbuffer* xrtMqttSubscribe(uint16 iPacketId, xstrview Filter, uint8 iQoS)
{
    xbuffer* result;
    size_t pos;
    if (iPacketId == 0 || iQoS > 1) {
        xmqtt_error(XERR_VALUE, "mqtt: subscription requires packet id and QoS 0/1");
        return NULL;
    }
    if (!xmqtt_topic(Filter, true)) return NULL;
    result = xmqtt_frame(0x82, 5 + Filter.Size, &pos);
    if (result != NULL) {
        xmqtt_put16(result->Data, &pos, iPacketId);
        xmqtt_put_text(result->Data, &pos, Filter);
        result->Data[pos] = iQoS;
    }
    return result;
}



xbuffer* xrtMqttPingreq(void)
{
    size_t pos;
    return xmqtt_frame(0xc0, 0, &pos);
}



xbuffer* xrtMqttDisconnect(void)
{
    size_t pos;
    return xmqtt_frame(0xe0, 0, &pos);
}
