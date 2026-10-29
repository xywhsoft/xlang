#include "mqtt_fixture.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>



void mqtt_test_open(mqtt_test_pair* pair)
{
    xnetengineconfig engine_config;
    xnetlistenconfig listen_config;
    xnetaddr local;
    memset(pair, 0, sizeof(*pair));
    xrtNetEngineConfigInit(&engine_config);
    engine_config.Workers = 1;
    pair->Engine = xrtNetEngineCreate(&engine_config);
    assert(pair->Engine != NULL && xrtNetEngineStart(pair->Engine));
    xrtNetListenConfigInit(&listen_config);
    assert(xrtNetAddrLoopback(&listen_config.Address, XNET_FAMILY_IPV4, 0));
    listen_config.Stream.ReadSize = 64;
    listen_config.Stream.ReadLimit = 1024;
    listen_config.Stream.WriteLimit = 64;
    listen_config.Stream.WriteHighWater = 32;
    listen_config.Stream.WriteLowWater = 16;
    listen_config.Stream.NoDelay = true;
    pair->Listener = xrtNetListen(pair->Engine, &listen_config, NULL, NULL, NULL);
    assert(pair->Listener != NULL && xrtNetListenerLocal(pair->Listener, &local));
    pair->Client = xrtNetStreamConnect(pair->Engine, &local, 0, &listen_config.Stream, NULL, NULL);
    assert(pair->Client != NULL);
    assert(xrtNetStreamWait(pair->Client, XNET_STREAM_WAIT_OPEN, 5000, NULL));
    pair->Peer = xrtNetListenerAcceptWait(pair->Listener, 5000, NULL);
    assert(pair->Peer != NULL);
}

void mqtt_test_close(mqtt_test_pair* pair)
{
    xnetenginestats stats;
    XL6_DeadlineV1 deadline = xl6DeadlineAfterMillisecondsV1(5000);
    assert(xrtNetStreamAbort(pair->Client));
    assert(xrtNetStreamAbort(pair->Peer));
    assert(xrtNetListenerClose(pair->Listener));
    xrtNetStreamDestroy(pair->Client);
    xrtNetStreamDestroy(pair->Peer);
    xrtNetListenerDestroy(pair->Listener);
    /* Abort/Close 是异步请求；等待对象的真实终态，再停止其驱动线程。 */
    do {
        assert(xrtNetEngineStats(pair->Engine, &stats));
        if (stats.LiveObjects == 0) break;
        assert(!xl6DeadlineExpiredV1(deadline));
        xrtSleep(1);
    } while (true);
    if (!xrtNetEngineStop(pair->Engine)) {
        fprintf(stderr, "MQTT fixture stop: %s\n", xrtErrorMessage(xrtGetError()));
        assert(false);
    }
    assert(xrtNetEngineDestroy(pair->Engine));
    memset(pair, 0, sizeof(*pair));
    xrtClearError();
}

void mqtt_test_send(xnetstream* stream, xbytesview data)
{
    assert(xrtNetStreamSend(stream, data.Data, data.Size) == XNET_RESULT_OK);
    assert(xrtNetStreamWait(stream, XNET_STREAM_WAIT_DRAIN, 5000, NULL));
}



typedef struct mqtt_fixture {
    mqtt_test_pair Pair;
    xthread* Thread;
} mqtt_fixture;

/* ACK 逐字节发送，最后一个字节与 PUBLISH/PINGRESP 一起发送；随后接收大报文。 */
static int32 broker(void* context)
{
    mqtt_fixture* fixture = context;
    xnetstream* peer = fixture->Pair.Peer;
    xbuffer* frame = xrtMqttRecvFrame(peer, 65536, xl6DeadlineAfterMillisecondsV1(5000), NULL);
    xvalue* request;
    xmqttmessage publish;
    static const uint8 ack_prefix[] = {0x20, 2, 0};
    static const uint8 ack_tail[] = {0, 0x30, 7, 0, 1, 't', 0, 255, 'a', 'b', 0xd0, 0};
    size_t i;
    if (frame == NULL) return 1;
    request = xrtMqttParse(xrtBufferView(frame));
    xrtBufferDestroy(frame);
    if (request == NULL) return 2;
    xrtValueRelease(request);
    for (i = 0; i < sizeof(ack_prefix); ++i) {
        mqtt_test_send(peer, (xbytesview){ack_prefix + i, 1});
        xrtSleep(5);
    }
    mqtt_test_send(peer, (xbytesview){ack_tail, sizeof(ack_tail)});
    frame = xrtMqttRecvFrame(peer, 65536, xl6DeadlineAfterMillisecondsV1(10000), NULL);
    if (frame == NULL) return 3;
    if (!xrtMqttPublishView(xrtBufferView(frame), &publish) ||
        publish.PacketId != 7 || publish.Payload.Size != 16384) {
        xrtBufferDestroy(frame);
        return 4;
    }
    for (i = 0; i < publish.Payload.Size; ++i) {
        if (publish.Payload.Data[i] != (uint8)i) { xrtBufferDestroy(frame); return 5; }
    }
    xrtBufferDestroy(frame);
    frame = xrtMqttPuback(7);
    if (frame == NULL) return 6;
    if (!xrtMqttSendFrame(peer, xrtBufferView(frame), xl6DeadlineAfterMillisecondsV1(5000), NULL)) {
        xrtBufferDestroy(frame);
        return 7;
    }
    xrtBufferDestroy(frame);
    return 0;
}

void* mqtt_fixture_open(void)
{
    mqtt_fixture* fixture = xrtCalloc(1, sizeof(*fixture));
    if (fixture == NULL) return NULL;
    mqtt_test_open(&fixture->Pair);
    fixture->Thread = xrtThreadCreate(broker, fixture, 0);
    assert(fixture->Thread != NULL);
    return fixture;
}

void* mqtt_fixture_client(void* handle)
{
    mqtt_fixture* fixture = handle;
    return xrtNetStreamRef(fixture->Pair.Client);
}

bool mqtt_fixture_close(void* handle)
{
    mqtt_fixture* fixture = handle;
    bool ok = xrtThreadWaitFor(fixture->Thread, 10000) == XWAIT_OK;
    if (ok) ok = xrtThreadExitCode(fixture->Thread) == 0;
    assert(ok);
    xrtThreadDestroy(fixture->Thread);
    mqtt_test_close(&fixture->Pair);
    xrtFree(fixture);
    return ok;
}
