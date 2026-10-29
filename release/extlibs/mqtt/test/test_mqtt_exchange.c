#define XRT_MODULE_MEMORY_DEBUG
#include "mqtt_fixture.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>



static void expect_closed(xnetstream* stream)
{
    (void)xrtNetStreamWait(stream, XNET_STREAM_WAIT_CLOSE, 5000, NULL);
    assert(xrtNetStreamState(stream) == XNET_STREAM_CLOSED);
    xrtClearError();
}

/* 对端预放应答，覆盖握手的状态验证；TCP 分包由另一组真实对话测试负责。 */
static void check_ack(const uint8* data, size_t size, bool valid)
{
    mqtt_test_pair pair;
    xvalue* result;
    mqtt_test_open(&pair);
    mqtt_test_send(pair.Peer, (xbytesview){data, size});
    result = xrtMqttHandshake(pair.Client, xrtStrView("client"), xrtStrView(""),
        (xbytesview){NULL, 0}, 0, xl6DeadlineAfterMillisecondsV1(5000), NULL);
    if (valid) {
        xbuffer* next;
        assert(result != NULL);
        xrtValueRelease(result);
        next = xrtMqttRecvFrame(pair.Client, 2, xl6DeadlineAfterMillisecondsV1(5000), NULL);
        assert(next != NULL && next->Size == 2 && next->Data[0] == 0xd0);
        xrtBufferDestroy(next);
    } else {
        assert(result == NULL && xrtErrorKind(xrtGetError()) == XERR_PROTOCOL);
        xrtClearError();
        expect_closed(pair.Client);
    }
    mqtt_test_close(&pair);
}

static void check_handshakes(void)
{
    uint8 refused[] = {0x20, 2, 0, 1};
    static const uint8 valid[] = {0x20, 2, 0, 0, 0xd0, 0};
    static const uint8 extended[] = {0x20, 0x82, 0x80, 0x80, 0, 0, 0, 0xd0, 0};
    static const uint8 session[] = {0x20, 2, 1, 0};
    static const uint8 reserved[] = {0x20, 2, 2, 0};
    static const uint8 wrong[] = {0xd0, 0};
    unsigned int code;
    check_ack(valid, sizeof(valid), true);
    check_ack(extended, sizeof(extended), true);
    for (code = 1; code <= 5; ++code) {
        refused[3] = (uint8)code;
        check_ack(refused, sizeof(refused), false);
    }
    check_ack(session, sizeof(session), false);
    check_ack(reserved, sizeof(reserved), false);
    check_ack(wrong, sizeof(wrong), false);
}



static void check_send_preflight(void)
{
    mqtt_test_pair pair;
    xbuffer* ping = xrtMqttPingreq();
    xbuffer* received;
    xcancel* cancel = xrtCancelCreate();
    assert(ping != NULL && cancel != NULL && xrtCancelRequest(cancel));
    mqtt_test_open(&pair);
    assert(!xrtMqttSendFrame(pair.Client, xrtBufferView(ping), XRT_DEADLINE_NEVER, cancel));
    assert(xrtErrorKind(xrtGetError()) == XERR_CANCELLED);
    xrtClearError();
    assert(!xrtMqttSendFrame(pair.Client, xrtBufferView(ping), xl6DeadlineAfterMillisecondsV1(0), NULL));
    assert(xrtErrorKind(xrtGetError()) == XERR_TIMEOUT);
    xrtClearError();
    assert(!xrtMqttSendFrame(pair.Client, (xbytesview){(cbytes)"\xc0\0\xd0\0", 4},
        XRT_DEADLINE_NEVER, NULL));
    assert(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL);
    xrtClearError();
    /* 所有预检查失败均未发送字节，原连接仍能收发。 */
    assert(xrtMqttSendFrame(pair.Client, xrtBufferView(ping), xl6DeadlineAfterMillisecondsV1(5000), NULL));
    received = xrtMqttRecvFrame(pair.Peer, 2, xl6DeadlineAfterMillisecondsV1(5000), NULL);
    assert(received != NULL && received->Size == 2 && received->Data[0] == 0xc0);
    xrtBufferDestroy(received);
    xrtBufferDestroy(ping);
    xrtCancelDestroy(cancel);
    mqtt_test_close(&pair);
}

typedef struct cancel_send {
    xnetstream* Peer;
    xcancel* Cancel;
} cancel_send;

static int32 cancel_after_send(void* data)
{
    cancel_send* state = data;
    assert(xrtNetStreamWaitAvailable(state->Peer, 1, 5000, NULL));
    assert(xrtCancelRequest(state->Cancel));
    return 0;
}

static void check_partial_send(bool cancelled)
{
    mqtt_test_pair pair;
    xbuffer* payload = xrtBufferCreate();
    xbuffer* frame;
    xcancel* cancel = cancelled ? xrtCancelCreate() : NULL;
    xthread* thread = NULL;
    cancel_send state;
    xnetstreamstats stats;
    assert(payload != NULL && xrtBufferResize(payload, 8u * 1024u * 1024u));
    memset(payload->Data, 'x', payload->Size);
    frame = xrtMqttPublish(0, xrtStrView("t"), xrtBufferView(payload), 0, false, false);
    assert(frame != NULL);
    mqtt_test_open(&pair);
    if (cancelled) {
        assert(cancel != NULL);
        state = (cancel_send){pair.Peer, cancel};
        thread = xrtThreadCreate(cancel_after_send, &state, 0);
        assert(thread != NULL);
    }
    /* 对端不读正文，有限接收队列形成背压；超时/取消发生于已经开始的发送。 */
    assert(!xrtMqttSendFrame(pair.Client, xrtBufferView(frame),
        xl6DeadlineAfterMillisecondsV1(cancelled ? 5000 : 250), cancel));
    assert(xrtErrorKind(xrtGetError()) == (cancelled ? XERR_CANCELLED : XERR_TIMEOUT));
    xrtClearError();
    assert(xrtNetStreamStats(pair.Client, &stats) && stats.SentBytes > 0 && stats.SentBytes < frame->Size);
    if (thread != NULL) {
        assert(xrtThreadWaitFor(thread, 5000) == XWAIT_OK);
        xrtThreadDestroy(thread);
    }
    expect_closed(pair.Client);
    mqtt_test_close(&pair);
    xrtCancelDestroy(cancel);
    xrtBufferDestroy(frame);
    xrtBufferDestroy(payload);
}



static void check_recv_allocation(void)
{
    unsigned int index, failures = 0, successes = 0, preserved = 0, aborted = 0;
    uint8 payload[512] = {0};
    xbuffer* frame = xrtMqttPublish(0, xrtStrView("t"),
        (xbytesview){payload, sizeof(payload)}, 0, false, false);
    xmemdebugsnapshot before, after;
    assert(frame != NULL);
    /* 先初始化当前线程的注入状态，常驻调试元数据不计作测试对象泄漏。 */
    assert(xrtMemDebugFailAfter(UINT64_MAX));
    xrtMemDebugFailClear();
    xrtMemDebugSnapshot(&before);
    for (index = 0; index < 48; ++index) {
        mqtt_test_pair pair;
        xbuffer* received;
        bool injected;
        mqtt_test_open(&pair);
        assert(xrtMqttSendFrame(pair.Peer, xrtBufferView(frame), xl6DeadlineAfterMillisecondsV1(5000), NULL));
        assert(xrtNetStreamWaitAvailable(pair.Client, frame->Size, 5000, NULL));
        /* XRT 的故障注入仅影响本线程，不干扰网络驱动的异步内存申请。 */
        assert(xrtMemDebugFailAfter(index));
        received = xrtMqttRecvFrame(pair.Client, 1024, xl6DeadlineAfterMillisecondsV1(5000), NULL);
        injected = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (received != NULL) {
            assert(!injected && received->Size == frame->Size);
            assert(memcmp(received->Data, frame->Data, frame->Size) == 0);
            ++successes;
        } else {
            assert(injected && xrtGetError() != NULL);
            ++failures;
            xrtClearError();
            if (xrtNetStreamAvailable(pair.Client) == frame->Size) {
                /* 缓冲或首字节等待的分配失败未读数据；已受理的读取也可能被保守终止。 */
                received = xrtMqttRecvFrame(pair.Client, 1024, xl6DeadlineAfterMillisecondsV1(5000), NULL);
                if (index < 2) assert(received != NULL);
            }
            if (received != NULL) {
                assert(received->Size == frame->Size);
                assert(memcmp(received->Data, frame->Data, frame->Size) == 0);
                ++preserved;
            } else {
                expect_closed(pair.Client);
                ++aborted;
            }
        }
        xrtBufferDestroy(received);
        mqtt_test_close(&pair);
    }
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);
    assert(after.InvalidFreeCount == before.InvalidFreeCount && after.DoubleFreeCount == before.DoubleFreeCount);
    assert(after.OverflowCount == before.OverflowCount && after.UnderflowCount == before.UnderflowCount);
    assert(after.UseAfterFreeCount == before.UseAfterFreeCount);
    xrtBufferDestroy(frame);
    assert(failures != 0 && successes != 0 && preserved != 0 && aborted != 0);
    printf("mqtt receive allocation checks: rejected=%u completed=%u\n", failures, successes);
}



int main(void)
{
    assert(xrtMemDebugEnable(true));
    check_handshakes();
    check_send_preflight();
    check_partial_send(true);
    check_partial_send(false);
    check_recv_allocation();
    puts("mqtt C exchange boundaries: PASS");
    return 0;
}
