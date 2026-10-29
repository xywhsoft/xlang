#include "include/xl_clock.h"
#define XRT_MODULE_THREAD
#include "../../extlibs/mqtt/include/xrt_mqtt_net.h"
#include <assert.h>
#include <stdio.h>



static void mqtt_example_send(xnetstream* stream, xbuffer* frame)
{
    assert(frame != NULL);
    assert(xrtMqttSendFrame(stream, xrtBufferView(frame), xl6DeadlineAfterMillisecondsV1(5000), NULL));
    xrtBufferDestroy(frame);
}

static xvalue* mqtt_example_recv(xnetstream* stream)
{
    xbuffer* frame = xrtMqttRecvFrame(stream, 4096, xl6DeadlineAfterMillisecondsV1(5000), NULL);
    xvalue* object;
    assert(frame != NULL);
    object = xrtMqttParse(xrtBufferView(frame));
    xrtBufferDestroy(frame);
    assert(object != NULL);
    return object;
}

static int64 mqtt_example_field(xvalue* object, const char* name)
{
    int64 result;
    assert(xrtValueGetInt(xrtValueObjectGet(object, xrtStrView(name)), &result));
    return result;
}

static xstrview mqtt_example_text(xvalue* object, const char* name)
{
    xstrview result;
    assert(xrtValueGetString(xrtValueObjectGet(object, xrtStrView(name)), &result));
    return result;
}



int main(void)
{
    xnetengineconfig config;
    xnetlistenconfig listen_config;
    xnetengine* engine;
    xnetlistener* listener;
    xnetstream* client;
    xnetstream* peer;
    xnetaddr address;
    xnetenginestats stats;
    XL6_DeadlineV1 deadline;
    xvalue* object;
    xstrview text, payload;
    uint16 id;

    /* C 侧显式创建/关闭网络驱动；语言侧由 TcpServer/TcpStream 管理。 */
    xrtNetEngineConfigInit(&config);
    config.Workers = 1;
    engine = xrtNetEngineCreate(&config);
    assert(engine != NULL && xrtNetEngineStart(engine));
    xrtNetListenConfigInit(&listen_config);
    assert(xrtNetAddrLoopback(&listen_config.Address, XNET_FAMILY_IPV4, 0));
    listener = xrtNetListen(engine, &listen_config, NULL, NULL, NULL);
    assert(listener != NULL && xrtNetListenerLocal(listener, &address));
    client = xrtNetStreamConnect(engine, &address, 0, &listen_config.Stream, NULL, NULL);
    assert(client != NULL);
    assert(xrtNetStreamWait(client, XNET_STREAM_WAIT_OPEN, xl6DeadlineAfterMillisecondsV1(5000), NULL));
    peer = xrtNetListenerAcceptWait(listener, xl6DeadlineAfterMillisecondsV1(5000), NULL);
    assert(peer != NULL);

    mqtt_example_send(client, xrtMqttConnect(xrtStrView("sensor-1"), xrtStrView(""),
        (xbytesview){NULL, 0}, 0, true));
    object = mqtt_example_recv(peer);
    text = mqtt_example_text(object, "clientId");
    printf("connect: %.*s\n", (int)text.Size, text.Data);
    xrtValueRelease(object);
    mqtt_example_send(peer, xrtBufferFromHex(xrtStrView("20020000"), 0));
    object = mqtt_example_recv(client);
    assert(mqtt_example_field(object, "returnCode") == 0);
    xrtValueRelease(object);

    mqtt_example_send(client, xrtMqttPublish(1, xrtStrView("home/temp"),
        (xbytesview){(cbytes)"22.5", 4}, 42, false, false));
    object = mqtt_example_recv(peer);
    text = mqtt_example_text(object, "topic");
    payload = mqtt_example_text(object, "payload");
    id = (uint16)mqtt_example_field(object, "packetId");
    printf("publish: %.*s=%.*s\n", (int)text.Size, text.Data, (int)payload.Size, payload.Data);
    xrtValueRelease(object);
    mqtt_example_send(peer, xrtMqttPuback(id));
    object = mqtt_example_recv(client);
    printf("puback: %lld\n", (long long)mqtt_example_field(object, "packetId"));
    xrtValueRelease(object);
    mqtt_example_send(client, xrtMqttDisconnect());
    object = mqtt_example_recv(peer);
    assert(mqtt_example_field(object, "type") == 14);
    xrtValueRelease(object);

    /* 关闭请求由驱动完成，不能在对象仍存活时停止 Engine。 */
    assert(xrtNetStreamClose(client) && xrtNetStreamClose(peer));
    assert(xrtNetListenerClose(listener));
    xrtNetStreamDestroy(client);
    xrtNetStreamDestroy(peer);
    xrtNetListenerDestroy(listener);
    deadline = xl6DeadlineAfterMillisecondsV1(5000);
    do {
        assert(xrtNetEngineStats(engine, &stats));
        if (stats.LiveObjects == 0) break;
        assert(!xl6DeadlineExpiredV1(deadline));
        xrtSleep(1);
    } while (true);
    assert(xrtNetEngineStop(engine) && xrtNetEngineDestroy(engine));
    puts("mqtt TCP example: PASS");
    return 0;
}



#include "../../extlibs/mqtt/mqtt_frame.c"
#include "../../extlibs/mqtt/mqtt_build.c"
#include "../../extlibs/mqtt/mqtt_parse.c"
#include "../../extlibs/mqtt/mqtt_net.c"
