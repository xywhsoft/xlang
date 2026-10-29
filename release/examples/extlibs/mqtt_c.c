#include "../../extlibs/mqtt/include/xrt_mqtt.h"
#include <assert.h>
#include <stdio.h>



static int64 mqtt_example_int(xvalue* object, const char* key)
{
    int64 result;
    assert(xrtValueGetInt(xrtValueObjectGet(object, xrtStrView(key)), &result));
    return result;
}

static void mqtt_example_hex(const char* label, xbuffer* frame)
{
    str hex;
    assert(frame != NULL);
    hex = xrtHexEncodeNew(frame->Data, frame->Size, 0);
    assert(hex != NULL);
    printf("%s: %s\n", label, hex);
    xrtFree(hex);
}

static void mqtt_example_take(const char* label, xbuffer* frame)
{
    mqtt_example_hex(label, frame);
    xrtBufferDestroy(frame);
}



int main(void)
{
    xbuffer* frame;
    xbuffer* binary;
    xbuffer* saved;
    xvalue* parsed;
    xmqttmessage view;
    xmqttheader header;
    int64 granted;

    /* C 与 xlang 采用同一协议核心；每个拥有型结果恰好释放一次。 */
    frame = xrtMqttConnect(xrtStrView("sensor-1"), xrtStrView(""), (xbytesview){NULL, 0}, 0, true);
    assert(frame != NULL);
    parsed = xrtMqttParse(xrtBufferView(frame));
    assert(parsed != NULL);
    mqtt_example_take("connect", frame);
    printf("keepalive: %lld\n", (long long)mqtt_example_int(parsed, "keepalive"));
    xrtValueRelease(parsed);

    frame = xrtMqttPublish(1, xrtStrView("home/temp"), (xbytesview){(cbytes)"22.5", 4}, 42, false, false);
    assert(frame != NULL && xrtMqttPublishView(xrtBufferView(frame), &view));
    mqtt_example_hex("publish", frame);
    printf("topic: %.*s\n", (int)view.Topic.Size, view.Topic.Data);
    printf("payload: %.*s\n", (int)view.Payload.Size, (const char*)view.Payload.Data);
    printf("packetId: %u\n", (unsigned int)view.PacketId);
    xrtBufferDestroy(frame);
    mqtt_example_take("subscribe", xrtMqttSubscribe(7, xrtStrView("home/+/temp"), 1));
    parsed = xrtMqttParse((xbytesview){(cbytes)"\x90\x03\x00\x07\x01", 5});
    assert(parsed != NULL);
    assert(xrtValueGetInt(xrtValueArrayGet(xrtValueObjectGet(parsed, xrtStrView("granted")), 0), &granted));
    printf("granted: %lld\n", (long long)granted);
    xrtValueRelease(parsed);
    mqtt_example_take("puback", xrtMqttPuback(42));
    mqtt_example_take("ping", xrtMqttPingreq());
    mqtt_example_take("disconnect", xrtMqttDisconnect());

    /* 借用视图不延长原帧生命周期，显式复制后才可销毁原帧。 */
    binary = xrtBufferFromHex(xrtStrView("00ff61"), 0);
    assert(binary != NULL);
    frame = xrtMqttPublish(0, xrtStrView("raw"), xrtBufferView(binary), 0, false, false);
    assert(frame != NULL && xrtMqttPublishView(xrtBufferView(frame), &view));
    saved = xrtBufferFrom(view.Payload);
    assert(saved != NULL);
    xrtBufferDestroy(binary);
    xrtBufferDestroy(frame);
    mqtt_example_take("binary", saved);
    assert(xrtMqttInspect((xbytesview){(cbytes)"\x30\x80\x01", 3}, &header) == XMQTT_MORE);
    printf("frameSize: %zu\n", header.PacketSize);
    parsed = xrtMqttParse((xbytesview){(cbytes)"\x20\x02\x00\x00", 4});
    assert(parsed != NULL);
    printf("connack: %lld\n", (long long)mqtt_example_int(parsed, "returnCode"));
    xrtValueRelease(parsed);
    puts("mqtt example: PASS");
    return 0;
}



/* 让单文件 xl run/build 可直接运行；独立 GCC 编译时另链接一份 XRT。 */
#include "../../extlibs/mqtt/mqtt_frame.c"
#include "../../extlibs/mqtt/mqtt_build.c"
#include "../../extlibs/mqtt/mqtt_parse.c"
