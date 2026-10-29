#include "mqtt_internal.h"



typedef struct mqttreader {
    xbytesview Frame;
    size_t Position;
} mqttreader;



static bool malformed(void)
{
    return xmqtt_error(XERR_PROTOCOL, "mqtt: malformed packet fields");
}

static bool read16(mqttreader* pReader, uint16* pValue)
{
    const uint8* data;
    if (pReader->Frame.Size - pReader->Position < 2) return malformed();
    data = pReader->Frame.Data + pReader->Position;
    *pValue = (uint16)(((uint16)data[0] << 8) | data[1]);
    pReader->Position += 2;
    return true;
}

static bool read_blob(mqttreader* pReader, xbytesview* pValue)
{
    uint16 size;
    if (!read16(pReader, &size)) return false;
    if (size > pReader->Frame.Size - pReader->Position) return malformed();
    *pValue = (xbytesview){pReader->Frame.Data + pReader->Position, size};
    pReader->Position += size;
    return true;
}

static xstrview as_text(xbytesview Value)
{
    return (xstrview){(const char*)Value.Data, Value.Size};
}

/* SetNew 无论成功与否都会接管 Value；分配失败的 NULL 不能当作合法空值插入。 */
static bool put(xvalue* pObject, const char* Key, xvalue* pValue)
{
    return xrtValueObjectSetNew(pObject, xrtStrView(Key), pValue);
}

static bool exact(xbytesview Frame, xmqttheader* pHeader)
{
    xmqttstatus status = xrtMqttInspect(Frame, pHeader);
    if (status == XMQTT_ERROR) return false;
    if (status != XMQTT_READY || pHeader->PacketSize != Frame.Size)
        return xmqtt_error(XERR_PROTOCOL, "mqtt: expected exactly one complete packet");
    return true;
}



bool xrtMqttPublishView(xbytesview Frame, xmqttmessage* pMessage)
{
    xmqttheader header;
    mqttreader reader;
    xbytesview topic;
    xmqttmessage message = {0};
    if (pMessage == NULL) return xmqtt_error(XERR_ARGUMENT, "mqtt: message output is null");
    memset(pMessage, 0, sizeof(*pMessage));
    if (!exact(Frame, &header)) return false;
    if (header.Type != 3) return xmqtt_error(XERR_PROTOCOL, "mqtt: expected PUBLISH");
    reader = (mqttreader){Frame, header.HeaderSize};
    if (!read_blob(&reader, &topic) || !xmqtt_topic(as_text(topic), false)) return false;
    message.QoS = (header.Flags >> 1) & 3u;
    if (message.QoS != 0) {
        if (!read16(&reader, &message.PacketId)) return false;
        if (message.PacketId == 0) return malformed();
    }
    message.Topic = as_text(topic);
    message.Payload = (xbytesview){Frame.Data + reader.Position, Frame.Size - reader.Position};
    message.Retain = (header.Flags & 1u) != 0;
    message.Duplicate = (header.Flags & 8u) != 0;
    *pMessage = message;
    return true;
}



static bool parse_connect(mqttreader* pReader, xvalue* pObject)
{
    xbytesview protocol, client, username = {0}, password = {0};
    uint16 keepalive;
    uint8 flags;
    if (!read_blob(pReader, &protocol)) return false;
    if (protocol.Size != 4 || memcmp(protocol.Data, "MQTT", 4) != 0 ||
        pReader->Frame.Size - pReader->Position < 4) return malformed();
    if (pReader->Frame.Data[pReader->Position++] != 4)
        return xmqtt_error(XERR_UNSUPPORTED, "mqtt: only protocol level 4 is supported");
    flags = pReader->Frame.Data[pReader->Position++];
    if ((flags & 1u) != 0 || ((flags & 4u) == 0 && (flags & 0x38u) != 0) ||
        ((flags & 64u) != 0 && (flags & 128u) == 0)) return malformed();
    if ((flags & 4u) != 0)
        return xmqtt_error(XERR_UNSUPPORTED, "mqtt: Will messages are not supported");
    if (!read16(pReader, &keepalive) || !read_blob(pReader, &client) ||
        !xmqtt_text(as_text(client), (flags & 2u) != 0)) return false;
    if ((flags & 128u) != 0 &&
        (!read_blob(pReader, &username) || !xmqtt_text(as_text(username), true))) return false;
    if ((flags & 64u) != 0 && !read_blob(pReader, &password)) return false;
    if (pReader->Position != pReader->Frame.Size) return malformed();
    return put(pObject, "clientId", xrtValueString(as_text(client))) &&
        put(pObject, "cleanSession", xrtValueBool((flags & 2u) != 0)) &&
        put(pObject, "keepalive", xrtValueInt(keepalive)) &&
        put(pObject, "username", xrtValueString(as_text(username))) &&
        put(pObject, "password", xrtValueString(as_text(password)));
}



static bool parse_subscription(mqttreader* pReader, xvalue* pObject, bool bAck)
{
    uint16 id;
    xvalue* items;
    if (!read16(pReader, &id)) return false;
    if (id == 0) return malformed();
    if (!put(pObject, "packetId", xrtValueInt(id))) return false;
    items = xrtValueArray();
    if (items == NULL) return false;
    while (pReader->Position < pReader->Frame.Size) {
        xvalue* item;
        if (bAck) {
            uint8 code = pReader->Frame.Data[pReader->Position++];
            if (code != 0 && code != 1 && code != 2 && code != 128) {
                malformed();
                goto fail;
            }
            item = xrtValueInt(code);
        } else {
            xbytesview topic;
            uint8 qos;
            if (!read_blob(pReader, &topic) || !xmqtt_topic(as_text(topic), true)) goto fail;
            if (pReader->Position == pReader->Frame.Size) { malformed(); goto fail; }
            qos = pReader->Frame.Data[pReader->Position++];
            if (qos > 1) {
                xmqtt_error(XERR_UNSUPPORTED, "mqtt: only QoS 0/1 subscriptions are supported");
                goto fail;
            }
            item = xrtValueObject();
            if (item == NULL) goto fail;
            if (!put(item, "topic", xrtValueString(as_text(topic))) ||
                !put(item, "qos", xrtValueInt(qos))) {
                xrtValueRelease(item);
                goto fail;
            }
        }
        if (!xrtValueArrayAppendNew(items, item)) goto fail;
    }
    return put(pObject, bAck ? "granted" : "subscriptions", items);
fail:
    xrtValueRelease(items);
    return false;
}



xvalue* xrtMqttParse(xbytesview Frame)
{
    xmqttheader header;
    mqttreader reader;
    xvalue* result;
    bool ok = false;
    if (!exact(Frame, &header)) return NULL;
    reader = (mqttreader){Frame, header.HeaderSize};
    result = xrtValueObject();
    if (result == NULL) return NULL;
    if (!put(result, "type", xrtValueInt(header.Type)) ||
        !put(result, "remaining", xrtValueInt((int64)header.Remaining))) goto fail;
    switch (header.Type) {
    case 1: ok = parse_connect(&reader, result); break;
    case 2: {
        uint8 session = Frame.Data[reader.Position];
        uint8 code = Frame.Data[reader.Position + 1];
        if (session > 1 || code > 5 || (code != 0 && session != 0)) { malformed(); break; }
        ok = put(result, "returnCode", xrtValueInt(code)) &&
            put(result, "sessionPresent", xrtValueInt(session));
        break;
    }
    case 3: {
        xmqttmessage message;
        if (!xrtMqttPublishView(Frame, &message)) break;
        ok = put(result, "topic", xrtValueString(message.Topic)) &&
            put(result, "payload", xrtValueString(as_text(message.Payload))) &&
            put(result, "qos", xrtValueInt(message.QoS)) &&
            put(result, "retain", xrtValueBool(message.Retain)) &&
            put(result, "dup", xrtValueBool(message.Duplicate));
        if (ok && message.QoS != 0) ok = put(result, "packetId", xrtValueInt(message.PacketId));
        break;
    }
    case 4: {
        uint16 id;
        if (!read16(&reader, &id)) break;
        if (id == 0) { malformed(); break; }
        ok = put(result, "packetId", xrtValueInt(id));
        break;
    }
    case 8: case 9: ok = parse_subscription(&reader, result, header.Type == 9); break;
    case 12: case 13: case 14: ok = true; break;
    default: malformed(); break;
    }
    if (ok) return result;
fail:
    xrtValueRelease(result);
    return NULL;
}
