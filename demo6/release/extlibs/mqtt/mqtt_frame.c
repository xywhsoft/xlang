#include "mqtt_internal.h"



bool xmqtt_error(xerrkind Kind, const char* Message)
{
    xrtSetErrorInfo(Kind, "xrt.mqtt", 0, Message);
    return false;
}



bool xmqtt_text(xstrview Text, bool bEmpty)
{
    if ((!bEmpty && Text.Size == 0) || Text.Size > UINT16_MAX ||
        (Text.Size != 0 && Text.Data == NULL))
        return xmqtt_error(XERR_VALUE, "mqtt: invalid UTF-8 field length");
    if (Text.Size != 0 &&
        (memchr(Text.Data, 0, Text.Size) != NULL || !xrtUtf8Valid(Text, NULL)))
        return xmqtt_error(XERR_VALUE, "mqtt: malformed UTF-8 or null character");
    return true;
}



bool xmqtt_topic(xstrview Text, bool bFilter)
{
    size_t i;
    if (!xmqtt_text(Text, false)) return false;
    for (i = 0; i < Text.Size; ++i) {
        char c = Text.Data[i];
        if (c != '#' && c != '+') continue;
        if (!bFilter || (i != 0 && Text.Data[i - 1] != '/') ||
            (c == '#' && i + 1 != Text.Size) ||
            (c == '+' && i + 1 != Text.Size && Text.Data[i + 1] != '/'))
            return xmqtt_error(XERR_VALUE, "mqtt: invalid topic wildcard");
    }
    return true;
}



/* 保留位与报文的最短/固定长度在分配正文之前验证。 */
static bool header_valid(uint8 type, uint8 flags, size_t remaining)
{
    if (type == 3) {
        unsigned int qos = (flags >> 1) & 3u;
        if (qos > 1) return xmqtt_error(XERR_UNSUPPORTED, "mqtt: only QoS 0/1 is supported");
        if (qos == 0 && (flags & 8u) != 0)
            return xmqtt_error(XERR_PROTOCOL, "mqtt: QoS 0 cannot set DUP");
        if (remaining < (qos == 0 ? 3u : 5u))
            return xmqtt_error(XERR_PROTOCOL, "mqtt: publish variable header is truncated");
        return true;
    }
    if (flags != (type == 8 ? 2 : 0))
        return xmqtt_error(XERR_PROTOCOL, "mqtt: invalid fixed header flags");
    switch (type) {
    case 1: if (remaining >= 12) return true; break;
    case 2: case 4: if (remaining == 2) return true; break;
    case 8: if (remaining >= 6) return true; break;
    case 9: if (remaining >= 3) return true; break;
    case 12: case 13: case 14: if (remaining == 0) return true; break;
    default: return xmqtt_error(XERR_UNSUPPORTED, "mqtt: unsupported packet type");
    }
    return xmqtt_error(XERR_PROTOCOL, "mqtt: invalid control packet length");
}



xmqttstatus xrtMqttInspect(xbytesview Data, xmqttheader* pHeader)
{
    size_t remaining = 0;
    size_t i;
    if (pHeader == NULL || (Data.Size != 0 && Data.Data == NULL)) {
        xmqtt_error(XERR_ARGUMENT, "mqtt: invalid frame view");
        return XMQTT_ERROR;
    }
    memset(pHeader, 0, sizeof(*pHeader));
    if (Data.Size == 0) return XMQTT_MORE;
    pHeader->Type = Data.Data[0] >> 4;
    pHeader->Flags = Data.Data[0] & 15u;
    for (i = 1; i <= 4; ++i) {
        uint8 digit;
        if (i >= Data.Size) return XMQTT_MORE;
        digit = Data.Data[i];
        remaining |= (size_t)(digit & 127u) << ((i - 1) * 7);
        if ((digit & 128u) == 0) {
            if (!header_valid(pHeader->Type, pHeader->Flags, remaining)) return XMQTT_ERROR;
            pHeader->HeaderSize = i + 1;
            pHeader->Remaining = remaining;
            pHeader->PacketSize = i + 1 + remaining;
            return Data.Size >= pHeader->PacketSize ? XMQTT_READY : XMQTT_MORE;
        }
    }
    xmqtt_error(XERR_PROTOCOL, "mqtt: remaining length exceeds four bytes");
    return XMQTT_ERROR;
}



xbuffer* xmqtt_frame(uint8 iHeader, size_t iRemaining, size_t* pPosition)
{
    uint8 header[5];
    size_t count = 1;
    size_t remaining = iRemaining;
    xbuffer* result;
    if (iRemaining > XMQTT_MAX_REMAINING) {
        xmqtt_error(XERR_RANGE, "mqtt: packet exceeds protocol length limit");
        return NULL;
    }
    header[0] = iHeader;
    do {
        uint8 digit = (uint8)(remaining & 127u);
        remaining >>= 7;
        header[count++] = digit | (remaining != 0 ? 128u : 0u);
    } while (remaining != 0);
    result = xrtBufferCreate();
    if (result == NULL) return NULL;
    if (!xrtBufferResize(result, count + iRemaining)) {
        xrtBufferDestroy(result);
        return NULL;
    }
    memcpy(result->Data, header, count);
    *pPosition = count;
    return result;
}
