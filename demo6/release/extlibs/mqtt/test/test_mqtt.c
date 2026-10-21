#include "../include/xrt_mqtt.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



static xbytesview text_bytes(const char* text)
{
    return (xbytesview){(cbytes)text, strlen(text)};
}

static xvalue* field(xvalue* object, const char* key)
{
    return xrtValueObjectGet(object, xrtStrView(key));
}

static int64 integer(xvalue* value)
{
    int64 result;
    assert(xrtValueGetInt(value, &result));
    return result;
}

/* 每一种构帧结果都检查预期字节、完整解析与所有截断位置。 */
static void check_frame(xbuffer* frame, const char* expected, int type)
{
    size_t i;
    xmqttheader header;
    xvalue* parsed;
    str hex;
    assert(frame != NULL);
    hex = xrtHexEncodeNew(frame->Data, frame->Size, 0);
    assert(hex != NULL && strcmp(hex, expected) == 0);
    xrtFree(hex);
    parsed = xrtMqttParse(xrtBufferView(frame));
    assert(parsed != NULL && integer(field(parsed, "type")) == type);
    xrtValueRelease(parsed);
    for (i = 0; i < frame->Size; ++i) {
        xbytesview prefix = {frame->Data, i};
        xrtClearError();
        assert(xrtMqttInspect(prefix, &header) == XMQTT_MORE);
        assert(xrtGetError() == NULL);
        assert(xrtMqttParse(prefix) == NULL && xrtGetError() != NULL);
    }
    xrtClearError();
    xrtBufferDestroy(frame);
}

static void check_invalid(const char* hex)
{
    xbuffer* frame = xrtBufferFromHex(xrtStrView(hex), 0);
    assert(frame != NULL);
    xrtClearError();
    assert(xrtMqttParse(xrtBufferView(frame)) == NULL && xrtGetError() != NULL);
    xrtBufferDestroy(frame);
    xrtClearError();
}



static void check_payload(void)
{
    xbuffer* payload = xrtBufferCreate();
    xbuffer* frame;
    xvalue* parsed;
    xmqttmessage view;
    xstrview stored;
    size_t i;
    assert(payload != NULL && xrtBufferResize(payload, 65536));
    for (i = 0; i < payload->Size; ++i) payload->Data[i] = (uint8)i;
    frame = xrtMqttPublish(1, xrtStrView("设备/数据"), xrtBufferView(payload), 65535, true, true);
    assert(frame != NULL && xrtMqttPublishView(xrtBufferView(frame), &view));
    assert(view.QoS == 1 && view.PacketId == 65535 && view.Retain && view.Duplicate);
    assert(view.Payload.Size == payload->Size && memcmp(view.Payload.Data, payload->Data, payload->Size) == 0);
    parsed = xrtMqttParse(xrtBufferView(frame));
    assert(parsed != NULL && xrtValueGetString(field(parsed, "payload"), &stored));
    xrtBufferDestroy(frame);
    assert(stored.Size == payload->Size && memcmp(stored.Data, payload->Data, payload->Size) == 0);
    xrtValueRelease(parsed);
    xrtBufferDestroy(payload);
}



static void check_limits(void)
{
    xbuffer* text = xrtBufferCreate();
    xbuffer* frame;
    xvalue* parsed;
    xmqttheader header;
    xmqttmessage message;
    xstrview maximum, too_long;
    static const uint8 largest_header[] = {0x30, 0xff, 0xff, 0xff, 0x7f};
    assert(text != NULL && xrtBufferResize(text, 65536));
    memset(text->Data, 'x', text->Size);
    maximum = (xstrview){(const char*)text->Data, 65535};
    too_long = (xstrview){(const char*)text->Data, 65536};
    frame = xrtMqttConnect(maximum, maximum, (xbytesview){text->Data, 65535}, 65535, false);
    assert(frame != NULL && xrtMqttInspect(xrtBufferView(frame), &header) == XMQTT_READY);
    assert(header.Remaining == 196621 && header.HeaderSize == 4);
    parsed = xrtMqttParse(xrtBufferView(frame));
    assert(parsed != NULL && integer(field(parsed, "keepalive")) == 65535);
    xrtValueRelease(parsed);
    xrtBufferDestroy(frame);
    frame = xrtMqttPublish(1, maximum, (xbytesview){NULL, 0}, 65535, true, true);
    assert(frame != NULL && xrtMqttPublishView(xrtBufferView(frame), &message));
    assert(message.Topic.Size == 65535 && message.Payload.Size == 0 && message.PacketId == 65535);
    xrtBufferDestroy(frame);

    /* 大小检查先于字段读取；只检查超大长度，不分配 256MiB 正文。 */
    assert(xrtMqttConnect(too_long, maximum, (xbytesview){NULL, 0}, 0, true) == NULL);
    assert(xrtMqttConnect(maximum, too_long, (xbytesview){NULL, 0}, 0, true) == NULL);
    assert(xrtMqttConnect(maximum, maximum, xrtBufferView(text), 0, true) == NULL);
    assert(xrtMqttPublish(0, too_long, (xbytesview){NULL, 0}, 0, false, false) == NULL);
    assert(xrtMqttPublish(0, maximum, (xbytesview){text->Data, SIZE_MAX}, 0, false, false) == NULL);
    xrtClearError();
    assert(xrtMqttInspect((xbytesview){largest_header, sizeof(largest_header)}, &header) == XMQTT_MORE);
    assert(header.PacketSize == (size_t)XMQTT_MAX_REMAINING + 5 && xrtGetError() == NULL);
    xrtBufferDestroy(text);
}



int main(void)
{
    static const char* invalid[] = {
        "", "20", "200100", "2003000000", "20020105", "20020200", "20020006",
        "21020000", "c00100", "c100", "0000", "f000", "c080808080", "c000d000",
        "300b0005746f70696368656c6c6f", "320d0005746f706963000b776f726c64",
        "3003000000", "3003000123", "30030001ff", "3003000100", "30040002c080",
        "3803000174", "34050001740001", "32050001740000", "40020000",
        "9000", "900100", "90020001", "9003000000", "9003000103",
        "8006000100016100", "8206000000016100", "8206000100016102",
        "82080001000361236200", "820800010003612b6200",
        "100c00044d515454040000000000", "100c00044d515454040300000000",
        "100c00044d515454044200000000", "100c00044d515454040a00000000",
        "100c00044d515454050200000000"
    };
    xvalue* parsed;
    xbuffer* frame;
    xmqttheader header;
    size_t i;
    uint8 random_bytes[96];
    check_frame(xrtMqttConnect(xrtStrView("demo"), xrtStrView(""), text_bytes(""), 60, true),
        "101000044d5154540402003c000464656d6f", 1);
    check_frame(xrtMqttConnect(xrtStrView(""), xrtStrView(""), text_bytes(""), 0, true),
        "100c00044d515454040200000000", 1);
    check_frame(xrtMqttPublish(0, xrtStrView("a/b"), text_bytes("hi"), 0, false, false),
        "30070003612f626869", 3);
    check_frame(xrtMqttPublish(1, xrtStrView("t"), text_bytes("x"), 7, false, false),
        "3206000174000778", 3);
    check_frame(xrtMqttPuback(7), "40020007", 4);
    check_frame(xrtMqttSubscribe(1, xrtStrView("news/#"), 0), "820b000100066e6577732f2300", 8);
    check_frame(xrtMqttPingreq(), "c000", 12);
    check_frame(xrtMqttDisconnect(), "e000", 14);
    check_frame(xrtBufferFromHex(xrtStrView("20020100"), 0), "20020100", 2);
    check_frame(xrtBufferFromHex(xrtStrView("d000"), 0), "d000", 13);
    check_payload();
    check_limits();

    frame = xrtBufferFromHex(xrtStrView("9006000900010280"), 0);
    parsed = xrtMqttParse(xrtBufferView(frame));
    assert(parsed != NULL && integer(field(parsed, "packetId")) == 9);
    assert(xrtValueCount(field(parsed, "granted")) == 4);
    xrtValueRelease(parsed);
    xrtBufferDestroy(frame);
    frame = xrtBufferFromHex(xrtStrView("820c00010003612f2b0000016201"), 0);
    parsed = xrtMqttParse(xrtBufferView(frame));
    assert(parsed != NULL && xrtValueCount(field(parsed, "subscriptions")) == 2);
    xrtValueRelease(parsed);
    xrtBufferDestroy(frame);
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) check_invalid(invalid[i]);

    assert(xrtMqttPuback(0) == NULL);
    assert(xrtMqttPublish(0, xrtStrView("t"), text_bytes("x"), 1, false, false) == NULL);
    assert(xrtMqttPublish(1, xrtStrView("t"), text_bytes("x"), 0, false, false) == NULL);
    assert(xrtMqttPublish(0, xrtStrView("a/+"), text_bytes("x"), 0, false, false) == NULL);
    assert(xrtMqttConnect(xrtStrView("x"), xrtStrView(""), text_bytes("secret"), 0, true) == NULL);
    xrtClearError();
    assert(xrtMqttInspect((xbytesview){NULL, 1}, &header) == XMQTT_ERROR);
    xrtClearError();

    /* 固定种子的无效报文扫测配合 sanitizer；不依赖外部 broker 或随机环境。 */
    srand(1);
    for (i = 0; i < 8192; ++i) {
        size_t j, size = (size_t)rand() % sizeof(random_bytes);
        for (j = 0; j < size; ++j) random_bytes[j] = (uint8)rand();
        parsed = xrtMqttParse((xbytesview){random_bytes, size});
        xrtValueRelease(parsed);
        xrtClearError();
    }
    puts("mqtt C codec boundaries: PASS");
    return 0;
}
