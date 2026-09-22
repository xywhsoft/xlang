#include "mqtt_fixture.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>



static void check_broker(void)
{
    void* fixture = mqtt_fixture_open();
    xnetstream* client = mqtt_fixture_client(fixture);
    xvalue* ack = xrtMqttHandshake(client, xrtStrView("c-client"), xrtStrView(""),
        (xbytesview){NULL, 0}, 0, xrtDeadlineAfter(5000000), NULL);
    xbuffer* received;
    xbuffer* payload;
    xbuffer* published;
    xmqttmessage view;
    size_t i;
    assert(ack != NULL);
    xrtValueRelease(ack);
    received = xrtMqttRecvFrame(client, 64, xrtDeadlineAfter(5000000), NULL);
    assert(received != NULL && xrtMqttPublishView(xrtBufferView(received), &view));
    assert(view.Payload.Size == 4 && view.Payload.Data[0] == 0 && view.Payload.Data[1] == 255);
    xrtBufferDestroy(received);
    received = xrtMqttRecvFrame(client, 64, xrtDeadlineAfter(5000000), NULL);
    assert(received != NULL && received->Size == 2 && received->Data[0] == 0xd0);
    xrtBufferDestroy(received);
    payload = xrtBufferCreate();
    assert(payload != NULL && xrtBufferResize(payload, 16384));
    for (i = 0; i < payload->Size; ++i) payload->Data[i] = (uint8)i;
    published = xrtMqttPublish(1, xrtStrView("upload"), xrtBufferView(payload), 7, false, false);
    assert(published != NULL && published->Size > xrtNetStreamWriteLimit(client));
    assert(xrtMqttSendFrame(client, xrtBufferView(published), xrtDeadlineAfter(10000000), NULL));
    xrtBufferDestroy(payload);
    xrtBufferDestroy(published);
    received = xrtMqttRecvFrame(client, 64, xrtDeadlineAfter(5000000), NULL);
    assert(received != NULL && received->Size == 4 && received->Data[0] == 0x40 && received->Data[3] == 7);
    xrtBufferDestroy(received);
    xrtNetStreamDestroy(client);
    assert(mqtt_fixture_close(fixture));
}



static void check_empty_wait(void)
{
    mqtt_test_pair pair;
    xbuffer* received;
    xcancel* cancel;
    mqtt_test_open(&pair);
    assert(xrtMqttRecvFrame(pair.Client, 64, xrtDeadlineAfter(20000), NULL) == NULL);
    assert(xrtErrorKind(xrtGetError()) == XERR_TIMEOUT);
    xrtClearError();
    cancel = xrtCancelCreate();
    assert(cancel != NULL && xrtCancelRequest(cancel));
    assert(xrtMqttRecvFrame(pair.Client, 64, XRT_DEADLINE_NEVER, cancel) == NULL);
    assert(xrtErrorKind(xrtGetError()) == XERR_CANCELLED);
    xrtClearError();
    xrtCancelDestroy(cancel);
    mqtt_test_send(pair.Peer, (xbytesview){(cbytes)"\xd0\x00", 2});
    received = xrtMqttRecvFrame(pair.Client, 64, xrtDeadlineAfter(5000000), NULL);
    assert(received != NULL && received->Size == 2);
    xrtBufferDestroy(received);
    mqtt_test_close(&pair);
}

static int32 cancel_later(void* data)
{
    xrtSleep(50);
    assert(xrtCancelRequest(data));
    return 0;
}

static void check_broken_frame(unsigned int mode)
{
    mqtt_test_pair pair;
    xbuffer* received;
    xcancel* cancel = NULL;
    xthread* thread = NULL;
    static const uint8 oversized[] = {0x30, 127};
    static const uint8 invalid[] = {0x21, 2};
    static const uint8 partial[] = {0x20, 2};
    xbytesview data = {mode == 0 ? oversized : mode == 1 ? invalid : partial, 2};
    mqtt_test_open(&pair);
    mqtt_test_send(pair.Peer, data);
    if (mode == 3) assert(xrtNetStreamClose(pair.Peer));
    if (mode == 4) {
        cancel = xrtCancelCreate();
        assert(cancel != NULL);
        thread = xrtThreadCreate(cancel_later, cancel, 0);
        assert(thread != NULL);
    }
    received = xrtMqttRecvFrame(pair.Client, 64, xrtDeadlineAfter(mode == 2 ? 20000 : 5000000), cancel);
    assert(received == NULL && xrtGetError() != NULL);
    if (mode == 0) assert(xrtErrorKind(xrtGetError()) == XERR_RANGE);
    if (mode == 2) assert(xrtErrorKind(xrtGetError()) == XERR_TIMEOUT);
    if (mode == 4) assert(xrtErrorKind(xrtGetError()) == XERR_CANCELLED);
    xrtClearError();
    if (thread != NULL) {
        assert(xrtThreadWaitFor(thread, 5000000) == XWAIT_OK);
        xrtThreadDestroy(thread);
        xrtCancelDestroy(cancel);
    }
    /* 异常关闭的 wait 会返回原始失败；这里验收的是连接已经终止而非正常 EOF。 */
    (void)xrtNetStreamWait(pair.Client, XNET_STREAM_WAIT_CLOSE, xrtDeadlineAfter(5000000), NULL);
    assert(xrtNetStreamState(pair.Client) == XNET_STREAM_CLOSED);
    xrtClearError();
    mqtt_test_close(&pair);
}



int main(void)
{
    unsigned int mode;
    check_broker();
    check_empty_wait();
    for (mode = 0; mode < 5; ++mode) check_broken_frame(mode);
    puts("mqtt C TCP boundaries: PASS");
    return 0;
}
