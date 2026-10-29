#include "handler_runtime.h"

#include "../include/xs_handler.h"
#define XRT_MODULE_STRING
#define XRT_MODULE_BUFFER
#define XRT_MODULE_COND
#define XRT_MODULE_THREAD
#define XRT_MODULE_TIME
#define XRT_MODULE_UNICODE
#define XRT_MODULE_NET_TCP_SYNC
#define XRT_MODULE_NET_UDP_SYNC
#include "xrt.h"

#include <stdint.h>
#include <string.h>

/* 服务端 endpoint 与连接收尾统一使用同一有界等待上限。 */
#define XS2_SOCKET_ENDPOINT_CLOSE_TIMEOUT_MS 5000u

/* ------------------------------------ 服务状态 ------------------------------------ */

typedef enum xs2_socket_type {
    XS2_SOCKET_TCP = 1,
    XS2_SOCKET_UDP = 2
} xs2_socket_type;

typedef enum xs2_tcp_framing {
    XS2_TCP_FRAMING_RAW = 1,
    XS2_TCP_FRAMING_TEXT = 2,
    XS2_TCP_FRAMING_DELIMITER = 3,
    XS2_TCP_FRAMING_LENGTH_PREFIX = 4
} xs2_tcp_framing;

typedef struct xs2_socket_service {
    xs2_socket_type type;
    char* name;
    char* host;
    char* delimiter;
    char* last_error;

    int port;
    int bind_port;
    int backlog;
    int worker_count;
    int connection_worker_count;
    int recv_limit;
    int frame_limit;
    int idle_timeout_ms;
    int write_timeout_ms;
    xs2_tcp_framing framing;
    size_t delimiter_size;

    void* handler;
    xnetengine* engine;
    xnetlistener* listener;
    xnetudp* udp;

    xmutex* mutex;
    xcond* changed;
    void** queue;
    int queue_capacity;
    int queue_head;
    int queue_tail;
    int queue_count;

    xthread* receiver_thread;
    xthread** worker_threads;
    int started_workers;
    int ready;
    int started;
    int stopping;

    uint64 accepted;
    uint64 received;
    uint64 sent;
    uint64 rejected;
    uint64 errors;
    uint64 active;
} xs2_socket_service;

/* ------------------------------------ 字符串与状态工具 ------------------------------------ */

static char* xs2SocketCopyText(const char* text)
{
    if ( text == NULL ) {
        text = "";
    }
    return xrtStrDup(text);
}

static void xs2SocketSetErrorUnlocked(xs2_socket_service* service, const char* text)
{
    if ( service->last_error != NULL ) {
        xrtFree(service->last_error);
    }
    service->last_error = xs2SocketCopyText(text);
}

static void xs2SocketSetError(xs2_socket_service* service, const char* text)
{
    if ( service == NULL || service->mutex == NULL ) {
        return;
    }
    xrtMutexLock(service->mutex);
    xs2SocketSetErrorUnlocked(service, text);
    xrtMutexUnlock(service->mutex);
}

static uint64 xs2SocketNowMs(void)
{
    double now = xrtTimer();
    return now > 0.0 ? (uint64)(now * 1000.0) : 0u;
}

static xdeadline xs2SocketDeadline(int timeout_ms)
{
    return timeout_ms < 0
        ? XRT_DEADLINE_NEVER
        : xrtDeadlineAfter((uint64)timeout_ms * 1000u);
}

static xbuffer* xs2SocketBufferCreate(size_t capacity)
{
    xbuffer* buffer = xrtBufferCreate();

    if ( buffer != NULL && capacity != 0u && !xrtBufferReserve(buffer, capacity) ) {
        xrtBufferDestroy(buffer);
        buffer = NULL;
    }
    return buffer;
}

static int xs2SocketIsStopping(xs2_socket_service* service)
{
    int stopping;
    xrtMutexLock(service->mutex);
    stopping = service->stopping;
    xrtMutexUnlock(service->mutex);
    return stopping;
}

static void xs2SocketAddMetric(xs2_socket_service* service, uint64* metric, int64 delta)
{
    xrtMutexLock(service->mutex);
    if ( delta >= 0 ) {
        *metric += (uint64)delta;
    } else {
        uint64 amount = (uint64)(-delta);
        *metric = *metric > amount ? *metric - amount : 0u;
    }
    xrtMutexUnlock(service->mutex);
}

/* ------------------------------------ 有界工作队列 ------------------------------------ */

static int xs2SocketQueuePush(xs2_socket_service* service, void* item)
{
    int pushed = 0;

    xrtMutexLock(service->mutex);
    if ( !service->stopping && service->queue_count < service->queue_capacity ) {
        service->queue[service->queue_tail] = item;
        service->queue_tail = (service->queue_tail + 1) % service->queue_capacity;
        service->queue_count += 1;
        xrtCondSignal(service->changed);
        pushed = 1;
    } else {
        service->rejected += 1u;
    }
    xrtMutexUnlock(service->mutex);
    return pushed;
}

static void* xs2SocketQueuePop(xs2_socket_service* service)
{
    void* item;

    xrtMutexLock(service->mutex);
    while ( service->queue_count == 0 && !service->stopping ) {
        xrtCondWait(service->changed, service->mutex);
    }
    if ( service->stopping ) {
        xrtMutexUnlock(service->mutex);
        return NULL;
    }
    item = service->queue[service->queue_head];
    service->queue[service->queue_head] = NULL;
    service->queue_head = (service->queue_head + 1) % service->queue_capacity;
    service->queue_count -= 1;
    xrtMutexUnlock(service->mutex);
    return item;
}

static void xs2SocketDestroyItem(xs2_socket_service* service, void* item)
{
    if ( item == NULL ) {
        return;
    }
    if ( service->type == XS2_SOCKET_TCP ) {
        (void)xrtNetStreamAbort((xnetstream*)item);
        xrtNetStreamDestroy((xnetstream*)item);
    } else {
        xrtNetUdpPacketDestroy((xnetudppacket*)item);
    }
}

static void xs2SocketQueueDrain(xs2_socket_service* service)
{
    void* item;

    for ( ;; ) {
        xrtMutexLock(service->mutex);
        if ( service->queue_count == 0 ) {
            xrtMutexUnlock(service->mutex);
            break;
        }
        item = service->queue[service->queue_head];
        service->queue[service->queue_head] = NULL;
        service->queue_head = (service->queue_head + 1) % service->queue_capacity;
        service->queue_count -= 1;
        xrtMutexUnlock(service->mutex);
        xs2SocketDestroyItem(service, item);
    }
}

/* ------------------------------------ 网络端点 ------------------------------------ */

/*
    XRT 的 listener/udp 关闭是异步状态机。服务 stop 的承诺是“端口已经可重绑”，
    所以不能只提交关闭请求后立即返回，也不能在终态前释放调用方持有的对象引用。
*/
static void xs2SocketRequestEndpointClose(xs2_socket_service* service);

static int xs2SocketWaitEndpointClosed(xs2_socket_service* service)
{
    xdeadline deadline = xrtDeadlineAfter(
        (uint64)XS2_SOCKET_ENDPOINT_CLOSE_TIMEOUT_MS * 1000u
    );

    for ( ;; ) {
        int listener_closed = service->listener == NULL ||
            xrtNetListenerState(service->listener) == XNET_LISTENER_CLOSED;
        int udp_closed = service->udp == NULL ||
            xrtNetUdpState(service->udp) == XNET_UDP_CLOSED;

        if ( listener_closed && udp_closed ) {
            return 1;
        }
        if ( xrtDeadlineExpired(deadline) ) {
            xs2SocketSetError(service, "TCP/UDP endpoint shutdown timed out");
            return 0;
        }
        xrtSleep(1u);
    }
}

static int xs2SocketStopEngine(xs2_socket_service* service)
{
    xdeadline deadline = xrtDeadlineAfter(
        (uint64)XS2_SOCKET_ENDPOINT_CLOSE_TIMEOUT_MS * 1000u
    );
    xnetenginestate state;

    if ( service->engine == NULL ) {
        return 1;
    }

    for ( ;; ) {
        state = xrtNetEngineState(service->engine);
        if ( state == XNET_ENGINE_STOPPED ) {
            return 1;
        }
        if ( state != XNET_ENGINE_RUNNING ) {
            xs2SocketSetError(service, "network engine is changing state");
            return 0;
        }
        if ( xrtNetEngineStop(service->engine) ) {
            return 1;
        }
        if ( xrtDeadlineExpired(deadline) ) {
            xerror* error = xrtTakeError();

            xs2SocketSetError(
                service,
                error != NULL ? xrtErrorMessage(error) : "network engine shutdown timed out"
            );
            xrtErrorFree(error);
            return 0;
        }

        /* 已请求终态的 Stream 仍在 XRT worker 上回收，下一轮再尝试停止。 */
        xrtClearError();
        xrtSleep(1u);
    }
}

static int xs2SocketCloseEndpoint(xs2_socket_service* service)
{
    xs2SocketRequestEndpointClose(service);
    if ( !xs2SocketWaitEndpointClosed(service) ) {
        return 0;
    }
    if ( service->listener != NULL ) {
        xrtNetListenerDestroy(service->listener);
        service->listener = NULL;
    }
    if ( service->udp != NULL ) {
        xrtNetUdpDestroy(service->udp);
        service->udp = NULL;
    }
    if ( !xs2SocketStopEngine(service) ) {
        return 0;
    }
    service->port = service->bind_port;
    return 1;
}

/*
    停止阶段先只请求端点关闭。接收线程仍可能阻塞在 Accept/ReceiveWait，
    此时销毁 listener/udp 会产生悬空访问；仅关闭会唤醒等待，而实际销毁
    统一留到线程全部退出后的 xs2SocketCloseEndpoint。
*/
static void xs2SocketRequestEndpointClose(xs2_socket_service* service)
{
    if ( service->listener != NULL ) {
        (void)xrtNetListenerClose(service->listener);
    }
    if ( service->udp != NULL ) {
        (void)xrtNetUdpClose(service->udp);
    }
}

static int xs2SocketOpenEndpoint(xs2_socket_service* service)
{
    xnetaddr address;

    if ( service->engine == NULL || !xrtNetEngineStart(service->engine) ) {
        xs2SocketSetError(service, "cannot start network engine");
        return 0;
    }
    if ( strcmp(service->host, "*") == 0 || service->host[0] == '\0' ) {
        if ( !xrtNetAddrAny(&address, XNET_FAMILY_IPV4, (uint16)service->bind_port) ) {
            xs2SocketSetError(service, "cannot create wildcard bind address");
            (void)xrtNetEngineStop(service->engine);
            return 0;
        }
    } else if ( !xrtNetAddrParse(&address, service->host, (uint16)service->bind_port) ) {
        xs2SocketSetError(service, "TCP/UDP bind host must be a numeric IP address");
        (void)xrtNetEngineStop(service->engine);
        return 0;
    }

    if ( service->type == XS2_SOCKET_TCP ) {
        xnetlistenconfig config;
        xnetaddr local;
        size_t wire_limit = (size_t)service->frame_limit + service->delimiter_size + 4u;

        xrtNetListenConfigInit(&config);
        config.Address = address;
        config.Backlog = service->backlog;
        config.AcceptQueueLimit = (uint32)service->queue_capacity;

        /*
            Listener 默认把新连接留在创建它的网络 worker；服务端会有
            大量长连接或等待中的连接，必须轮转分配，避免一个 worker
            承担全部收发而其余 worker 空闲。
        */
        config.Distribution = XNET_ACCEPT_ROUND_ROBIN;
        config.Stream.ReadLimit = (size_t)service->recv_limit;
        if ( config.Stream.ReadSize > config.Stream.ReadLimit ) {
            config.Stream.ReadSize = config.Stream.ReadLimit;
        }
        if ( config.Stream.WriteLimit < wire_limit ) {
            config.Stream.WriteLimit = wire_limit;
        }
        service->listener = xrtNetListen(service->engine, &config, NULL, NULL, NULL);
        if ( service->listener == NULL ) {
            xs2SocketSetError(service, "cannot bind TCP listener");
            (void)xrtNetEngineStop(service->engine);
            return 0;
        }
        if ( !xrtNetListenerLocal(service->listener, &local) ) {
            xs2SocketSetError(service, "cannot query TCP listener address");
            xs2SocketCloseEndpoint(service);
            return 0;
        }
        service->port = (int)local.Port;
    } else {
        xnetudpconfig config;
        xnetaddr local;
        size_t receive_size = (size_t)service->recv_limit;

        if ( receive_size > XNET_UDP_PAYLOAD_MAX ) {
            receive_size = XNET_UDP_PAYLOAD_MAX;
        }
        xrtNetUdpConfigInit(&config);
        config.ReceiveSize = receive_size;
        config.ReceiveQueueLimit = (size_t)service->queue_capacity;
        config.ReceiveQueueByteLimit = receive_size * (size_t)service->queue_capacity;
        if ( config.SendLimit < (size_t)service->frame_limit ) {
            config.SendLimit = (size_t)service->frame_limit;
        }
        service->udp = xrtNetUdpBind(service->engine, &address, 0u, &config, NULL, NULL);
        if ( service->udp == NULL ) {
            xs2SocketSetError(service, "cannot bind UDP socket");
            (void)xrtNetEngineStop(service->engine);
            return 0;
        }
        if ( !xrtNetUdpLocal(service->udp, &local) ) {
            xs2SocketSetError(service, "cannot query UDP bind address");
            xs2SocketCloseEndpoint(service);
            return 0;
        }
        service->port = (int)local.Port;
    }
    return 1;
}

/* ------------------------------------ Handler 调用 ------------------------------------ */

static int xs2SocketInvoke(
    xs2_socket_service* service,
    int kind,
    void* request,
    void* response,
    const void* data,
    size_t data_size,
    const char* remote_address,
    unsigned int remote_port,
    xbuffer* output)
{
    int result;

    xrtBufferClear(output);
    result = xs2_handler_slot_call_buffer(
        service->handler,
        kind,
        service->name,
        "",
        request,
        response,
        data,
        data_size,
        remote_address,
        remote_port,
        output);
    if ( result != XS_HANDLER_OK ) {
        void* error_text = xs2_handler_slot_last_error_copy(service->handler);
        xs2SocketAddMetric(service, &service->errors, 1);
        if ( error_text != NULL ) {
            xs2SocketSetError(service, (const char*)error_text);
            xrtFree(error_text);
        }
    }
    return result;
}

static int xs2SocketSendTcp(xs2_socket_service* service, xnetstream* stream, xbuffer* output)
{
    const void* suffix = NULL;
    size_t suffix_size = 0u;
    unsigned char prefix[4];
    size_t prefix_size = 0u;
    xnetspan spans[3];
    size_t span_count = 0u;
    size_t wire_size;
    xdeadline deadline;
    xnetresult send_result;

    if ( output->Size == 0u ) {
        return 1;
    }
    if ( output->Size > (size_t)service->frame_limit ) {
        xs2SocketSetError(service, "TCP handler output exceeds frameLimit");
        xs2SocketAddMetric(service, &service->errors, 1);
        return 0;
    }
    if ( service->framing == XS2_TCP_FRAMING_TEXT ) {
        if ( !xrtUtf8Valid((xstrview){ (cstr)output->Data, output->Size }, NULL) ||
             memchr(output->Data, '\r', output->Size) != NULL ||
             memchr(output->Data, '\n', output->Size) != NULL ) {
            xs2SocketSetError(service, "TCP text handler output is not one valid UTF-8 line");
            xs2SocketAddMetric(service, &service->errors, 1);
            return 0;
        }
        suffix = "\n";
        suffix_size = 1u;
    } else if ( service->framing == XS2_TCP_FRAMING_DELIMITER ) {
        size_t index;
        for ( index = 0u; index + service->delimiter_size <= output->Size; ++index ) {
            if ( memcmp(output->Data + index, service->delimiter, service->delimiter_size) == 0 ) {
                xs2SocketSetError(service, "TCP handler output contains the configured delimiter");
                xs2SocketAddMetric(service, &service->errors, 1);
                return 0;
            }
        }
        suffix = service->delimiter;
        suffix_size = service->delimiter_size;
    } else if ( service->framing == XS2_TCP_FRAMING_LENGTH_PREFIX ) {
        uint32 length = (uint32)output->Size;
        prefix[0] = (unsigned char)(length >> 24);
        prefix[1] = (unsigned char)(length >> 16);
        prefix[2] = (unsigned char)(length >> 8);
        prefix[3] = (unsigned char)length;
        prefix_size = sizeof(prefix);
    }

    if ( prefix_size != 0u ) {
        spans[span_count].Data = prefix;
        spans[span_count].Size = prefix_size;
        span_count += 1u;
    }
    spans[span_count].Data = output->Data;
    spans[span_count].Size = output->Size;
    span_count += 1u;
    if ( suffix_size != 0u ) {
        spans[span_count].Data = (cbytes)suffix;
        spans[span_count].Size = suffix_size;
        span_count += 1u;
    }

    deadline = xs2SocketDeadline(service->write_timeout_ms);
    for ( ;; ) {
        send_result = xrtNetStreamSendVec(stream, spans, span_count);
        if ( send_result == XNET_RESULT_OK ) {
            break;
        }
        if ( send_result != XNET_RESULT_AGAIN ||
             !xrtNetStreamWait(stream, XNET_STREAM_WAIT_WRITE, deadline, NULL) ) {
            xs2SocketAddMetric(service, &service->errors, 1);
            return 0;
        }
    }
    if ( !xrtNetStreamWait(stream, XNET_STREAM_WAIT_DRAIN, deadline, NULL) ) {
        xs2SocketAddMetric(service, &service->errors, 1);
        return 0;
    }
    wire_size = prefix_size + output->Size + suffix_size;
    xs2SocketAddMetric(service, &service->sent, (int64)wire_size);
    return 1;
}

static const char* xs2SocketFindBytes(
    const char* data,
    size_t data_size,
    const char* needle,
    size_t needle_size)
{
    size_t index;
    if ( data == NULL || needle == NULL || needle_size == 0u || data_size < needle_size ) {
        return NULL;
    }
    for ( index = 0u; index + needle_size <= data_size; ++index ) {
        if ( memcmp(data + index, needle, needle_size) == 0 ) {
            return data + index;
        }
    }
    return NULL;
}

static void xs2SocketConsume(xbuffer* pending, size_t size)
{
    if ( pending == NULL || size == 0u ) {
        return;
    }
    if ( size >= pending->Size ) {
        xrtBufferClear(pending);
        return;
    }
    (void)xrtBufferRemove(pending, 0u, size);
}

static int xs2SocketProtocolError(xs2_socket_service* service, const char* message)
{
    xs2SocketSetError(service, message);
    xs2SocketAddMetric(service, &service->rejected, 1);
    xs2SocketAddMetric(service, &service->errors, 1);
    return 0;
}

static int xs2SocketDispatchTcpFrame(
    xs2_socket_service* service,
    xnetstream* stream,
    const void* data,
    size_t data_size,
    const char* remote_address,
    unsigned int remote_port,
    xbuffer* output)
{
    int result = xs2SocketInvoke(
        service,
        XS_HANDLER_KIND_TCP_DATA,
        stream,
        stream,
        data,
        data_size,
        remote_address,
        remote_port,
        output);
    if ( output->Size != 0u && !xs2SocketSendTcp(service, stream, output) ) {
        return 0;
    }
    return result == XS_HANDLER_OK;
}

static int xs2SocketDispatchPending(
    xs2_socket_service* service,
    xnetstream* stream,
    xbuffer* pending,
    const char* remote_address,
    unsigned int remote_port,
    xbuffer* output)
{
    for ( ;; ) {
        size_t payload_size;
        size_t consumed;

        if ( service->framing == XS2_TCP_FRAMING_LENGTH_PREFIX ) {
            const unsigned char* bytes;
            uint32 declared;
            if ( pending->Size < 4u ) {
                return 1;
            }
            bytes = (const unsigned char*)pending->Data;
            declared = ((uint32)bytes[0] << 24) | ((uint32)bytes[1] << 16) |
                       ((uint32)bytes[2] << 8) | (uint32)bytes[3];
            if ( declared > (uint32)service->frame_limit ) {
                return xs2SocketProtocolError(service, "TCP length-prefix frame exceeds frameLimit");
            }
            if ( pending->Size < declared + 4u ) {
                return 1;
            }
            payload_size = declared;
            consumed = payload_size + 4u;
            if ( !xs2SocketDispatchTcpFrame(service, stream, pending->Data + 4u, payload_size,
                    remote_address, remote_port, output) ) {
                return 0;
            }
            xs2SocketConsume(pending, consumed);
            continue;
        }

        {
            const char* delimiter = service->framing == XS2_TCP_FRAMING_TEXT
                ? "\n" : service->delimiter;
            size_t delimiter_size = service->framing == XS2_TCP_FRAMING_TEXT
                ? 1u : service->delimiter_size;
            const char* found = xs2SocketFindBytes(
                (const char*)pending->Data, pending->Size, delimiter, delimiter_size);
            if ( found == NULL ) {
                size_t maximum_pending = (size_t)service->frame_limit + delimiter_size - 1u;
                if ( pending->Size > maximum_pending ) {
                    return xs2SocketProtocolError(service, "TCP delimited frame exceeds frameLimit");
                }
                return 1;
            }
            payload_size = (size_t)(found - (const char*)pending->Data);
            consumed = payload_size + delimiter_size;
            if ( payload_size > (size_t)service->frame_limit ) {
                return xs2SocketProtocolError(service, "TCP delimited frame exceeds frameLimit");
            }
            if ( service->framing == XS2_TCP_FRAMING_TEXT && payload_size > 0u &&
                  pending->Data[payload_size - 1u] == '\r' ) {
                payload_size -= 1u;
            }
            if ( service->framing == XS2_TCP_FRAMING_TEXT &&
                  !xrtUtf8Valid((xstrview){ (cstr)pending->Data, payload_size }, NULL) ) {
                return xs2SocketProtocolError(service, "TCP text frame is not valid UTF-8");
            }
            if ( !xs2SocketDispatchTcpFrame(service, stream, pending->Data, payload_size,
                    remote_address, remote_port, output) ) {
                return 0;
            }
            xs2SocketConsume(pending, consumed);
        }
    }
}

/* ------------------------------------ TCP 工作线程 ------------------------------------ */

static void xs2SocketHandleTcp(xs2_socket_service* service, xnetstream* stream)
{
    xbuffer* output;
    xbuffer* pending = NULL;
    xnetaddr remote;
    char* remote_address;
    unsigned int remote_port;
    uint64 last_activity;
    int result;

    output = xs2SocketBufferCreate(4096u);
    if ( service->framing != XS2_TCP_FRAMING_RAW ) {
        pending = xs2SocketBufferCreate(service->frame_limit < 65536 ? (size_t)service->frame_limit : 65536u);
    }
    remote_address = xrtNetStreamRemote(stream, &remote)
        ? xrtNetAddrString(&remote)
        : NULL;
    remote_port = remote_address != NULL ? (unsigned int)remote.Port : 0u;
    if ( output == NULL || (service->framing != XS2_TCP_FRAMING_RAW && pending == NULL) ) {
        xs2SocketAddMetric(service, &service->errors, 1);
        xrtFree(remote_address);
        if ( pending != NULL ) {
            xrtBufferDestroy(pending);
        }
        if ( output != NULL ) {
            xrtBufferDestroy(output);
        }
        (void)xrtNetStreamAbort(stream);
        xrtNetStreamDestroy(stream);
        return;
    }

    xs2SocketAddMetric(service, &service->active, 1);
    result = xs2SocketInvoke(
        service,
        XS_HANDLER_KIND_TCP_OPEN,
        stream,
        stream,
        NULL,
        0u,
        remote_address,
        remote_port,
        output);
    if ( output->Size != 0u ) {
        (void)xs2SocketSendTcp(service, stream, output);
    }

    last_activity = xs2SocketNowMs();
    while ( result == XS_HANDLER_OK && !xs2SocketIsStopping(service) ) {
        xnetbytes* received;
        xbytesview data;
        size_t read_limit = service->framing == XS2_TCP_FRAMING_RAW
            ? (size_t)service->frame_limit
            : (size_t)service->recv_limit - pending->Size;

        if ( read_limit == 0u ) {
            (void)xs2SocketProtocolError(service, "TCP receive buffer exceeds recvLimit");
            break;
        }
        if ( read_limit > 65536u ) {
            read_limit = 65536u;
        }
        received = xrtNetStreamRecv(
            stream,
            read_limit,
            xs2SocketDeadline(100u),
            NULL
        );

        if ( received != NULL ) {
            data = xrtNetBytesView(received);
            last_activity = xs2SocketNowMs();
            xs2SocketAddMetric(service, &service->received, data.Size);
            if ( service->framing == XS2_TCP_FRAMING_RAW ) {
                result = xs2SocketDispatchTcpFrame(
                    service, stream, data.Data, data.Size, remote_address, remote_port, output)
                    ? XS_HANDLER_OK : XS_HANDLER_ERROR;
            } else if ( !xrtBufferAppend(pending, data) ) {
                xs2SocketSetError(service, "cannot grow TCP framing buffer");
                xs2SocketAddMetric(service, &service->errors, 1);
                result = XS_HANDLER_ERROR;
            } else {
                result = xs2SocketDispatchPending(
                    service, stream, pending, remote_address, remote_port, output)
                    ? XS_HANDLER_OK : XS_HANDLER_ERROR;
            }
            xrtNetBytesDestroy(received);
            continue;
        }
        if ( xrtNetStreamState(stream) != XNET_STREAM_OPEN ) {
            break;
        }
        if ( service->idle_timeout_ms > 0 &&
             xs2SocketNowMs() - last_activity >= (uint64)service->idle_timeout_ms ) {
            break;
        }
    }

    if ( result == XS_HANDLER_OK && pending != NULL && pending->Size != 0u &&
         !xs2SocketIsStopping(service) ) {
        (void)xs2SocketProtocolError(service, "TCP connection closed with an incomplete frame");
    }

    (void)xs2SocketInvoke(
        service,
        XS_HANDLER_KIND_TCP_CLOSE,
        stream,
        stream,
        NULL,
        0u,
        remote_address,
        remote_port,
        output);
    xs2SocketAddMetric(service, &service->active, -1);
    xrtFree(remote_address);
    xrtBufferDestroy(pending);
    xrtBufferDestroy(output);
    (void)xrtNetStreamClose(stream);
    xrtNetStreamDestroy(stream);
}

/* ------------------------------------ UDP 工作线程 ------------------------------------ */

static void xs2SocketHandleUdp(xs2_socket_service* service, xnetudppacket* packet)
{
    xbuffer* output;
    const xnetaddr* remote;
    char* remote_address;
    int remote_port;
    size_t data_size;
    cbytes data;
    int result;

    data_size = xrtNetUdpPacketSize(packet);
    if ( data_size > (size_t)service->recv_limit ) {
        xs2SocketAddMetric(service, &service->rejected, 1);
        xrtNetUdpPacketDestroy(packet);
        return;
    }
    data = xrtNetUdpPacketData(packet);
    output = xs2SocketBufferCreate(4096u);
    remote = xrtNetUdpPacketRemote(packet);
    remote_address = remote != NULL ? xrtNetAddrString(remote) : NULL;
    remote_port = remote != NULL ? (int)remote->Port : 0;
    if ( output == NULL || remote == NULL || remote_address == NULL ) {
        xs2SocketAddMetric(service, &service->errors, 1);
        xrtFree(remote_address);
        if ( output != NULL ) {
            xrtBufferDestroy(output);
        }
        xrtNetUdpPacketDestroy(packet);
        return;
    }

    xs2SocketAddMetric(service, &service->active, 1);
    xs2SocketAddMetric(service, &service->received, data_size);
    result = xs2SocketInvoke(
        service,
        XS_HANDLER_KIND_UDP,
        packet,
        service->udp,
        data,
        data_size,
        remote_address,
        (unsigned int)remote_port,
        output);
    if ( result == XS_HANDLER_OK && output->Size != 0u ) {
        xnetresult send_result = xrtNetUdpSendTo(service->udp, remote, output->Data, output->Size);
        if ( send_result == XNET_RESULT_AGAIN &&
             xrtNetUdpWritable(service->udp, output->Size,
                 xs2SocketDeadline(service->write_timeout_ms), NULL) ) {
            send_result = xrtNetUdpSendTo(service->udp, remote, output->Data, output->Size);
        }
        if ( send_result == XNET_RESULT_OK ) {
            xs2SocketAddMetric(service, &service->sent, output->Size);
        } else {
            xs2SocketAddMetric(service, &service->errors, 1);
        }
    }
    xs2SocketAddMetric(service, &service->active, -1);
    xrtFree(remote_address);
    xrtBufferDestroy(output);
    xrtNetUdpPacketDestroy(packet);
}

/* ------------------------------------ 线程入口 ------------------------------------ */

static uint32 xs2SocketReceiverThread(void* argument)
{
    xs2_socket_service* service = (xs2_socket_service*)argument;

    while ( !xs2SocketIsStopping(service) ) {
        void* item;
        if ( service->type == XS2_SOCKET_TCP ) {
            item = xrtNetListenerAcceptWait(
                service->listener, xs2SocketDeadline(100), NULL);
            if ( item != NULL ) {
                xs2SocketAddMetric(service, &service->accepted, 1);
            }
        } else {
            item = xrtNetUdpReceiveWait(
                service->udp, xs2SocketDeadline(100), NULL);
        }
        if ( item != NULL && !xs2SocketQueuePush(service, item) ) {
            xs2SocketDestroyItem(service, item);
        }
    }
    return 0u;
}

static uint32 xs2SocketWorkerThread(void* argument)
{
    xs2_socket_service* service = (xs2_socket_service*)argument;

    for ( ;; ) {
        void* item = xs2SocketQueuePop(service);
        if ( item == NULL ) {
            break;
        }
        if ( service->type == XS2_SOCKET_TCP ) {
            xs2SocketHandleTcp(service, (xnetstream*)item);
        } else {
            xs2SocketHandleUdp(service, (xnetudppacket*)item);
        }
    }
    return 0u;
}

/* ------------------------------------ 创建与销毁 ------------------------------------ */

void xs2_socket_service_destroy(void* handle);

void* xs2_socket_service_create(
    const char* service_type,
    const char* name,
    const char* host,
    int port,
    int backlog,
    int worker_count,
    int connection_worker_count,
    int queue_capacity,
    int recv_limit,
    const char* framing,
    const char* delimiter,
    int frame_limit,
    int idle_timeout_ms,
    int write_timeout_ms,
    const char* source_path,
    const char* work_dir,
    const char* include_dir,
    int handler_pool_size,
    int handler_queue_capacity,
    int handler_queue_timeout_ms,
    int handler_timeout_ms)
{
    xs2_socket_service* service;

    service = (xs2_socket_service*)xrtCalloc(1u, sizeof(*service));
    if ( service == NULL ) {
        return NULL;
    }
    service->mutex = xrtMutexCreate();
    service->changed = xrtCondCreate();
    service->name = xs2SocketCopyText(name);
    service->host = xs2SocketCopyText(host);
    service->delimiter = xs2SocketCopyText(delimiter);
    service->last_error = xs2SocketCopyText("");
    service->port = port;
    service->bind_port = port;
    service->backlog = backlog;
    service->worker_count = worker_count;
    service->connection_worker_count = connection_worker_count;
    service->queue_capacity = queue_capacity;
    service->recv_limit = recv_limit;
    service->frame_limit = frame_limit;
    service->idle_timeout_ms = idle_timeout_ms;
    service->write_timeout_ms = write_timeout_ms;

    if ( framing != NULL && strcmp(framing, "raw") == 0 ) {
        service->framing = XS2_TCP_FRAMING_RAW;
    } else if ( framing != NULL && strcmp(framing, "text") == 0 ) {
        service->framing = XS2_TCP_FRAMING_TEXT;
    } else if ( framing != NULL && strcmp(framing, "delimiter") == 0 ) {
        service->framing = XS2_TCP_FRAMING_DELIMITER;
    } else if ( framing != NULL && strcmp(framing, "length-prefix") == 0 ) {
        service->framing = XS2_TCP_FRAMING_LENGTH_PREFIX;
    }
    service->delimiter_size = service->delimiter != NULL ? strlen(service->delimiter) : 0u;

    if ( service_type != NULL && strcmp(service_type, "tcp") == 0 ) {
        service->type = XS2_SOCKET_TCP;
    } else if ( service_type != NULL && strcmp(service_type, "udp") == 0 ) {
        service->type = XS2_SOCKET_UDP;
    }
    if ( service->mutex == NULL || service->changed == NULL || service->name == NULL ||
         service->host == NULL || service->delimiter == NULL || service->last_error == NULL ) {
        xs2_socket_service_destroy(service);
        return NULL;
    }
    if ( service->type == 0 || service->name[0] == '\0' || port < 0 || port > 65535 ||
         backlog <= 0 || worker_count <= 0 || worker_count > 256 ||
         connection_worker_count <= 0 || connection_worker_count > 256 ||
         queue_capacity <= 0 || recv_limit <= 0 || recv_limit > 67108864 ||
         frame_limit <= 0 || frame_limit > recv_limit || service->framing == 0 ||
         (service->type == XS2_SOCKET_UDP && service->framing != XS2_TCP_FRAMING_RAW) ||
         (service->framing == XS2_TCP_FRAMING_DELIMITER &&
          (service->delimiter_size == 0u || service->delimiter_size > 256u)) ||
         idle_timeout_ms < 0 || write_timeout_ms < 0 ) {
        xs2SocketSetError(service, "invalid TCP/UDP service configuration");
        return service;
    }

    {
        xnetengineconfig config;

        xrtNetEngineConfigInit(&config);
        config.Workers = (uint32)worker_count;
        service->engine = xrtNetEngineCreate(&config);
    }
    service->queue = (void**)xrtCalloc((size_t)queue_capacity, sizeof(void*));
    service->worker_threads = (xthread**)xrtCalloc((size_t)connection_worker_count, sizeof(xthread*));
    if ( service->engine == NULL || service->queue == NULL || service->worker_threads == NULL ) {
        xs2SocketSetError(service, "cannot allocate TCP/UDP network engine, queue or workers");
        return service;
    }

    service->handler = xs2_handler_slot_create(
        source_path,
        work_dir,
        include_dir,
        handler_pool_size,
        handler_queue_capacity,
        handler_queue_timeout_ms,
        handler_timeout_ms);
    if ( service->handler == NULL || !xs2_handler_slot_ready(service->handler) ) {
        void* error_text = service->handler != NULL
            ? xs2_handler_slot_last_error_copy(service->handler) : NULL;
        xs2SocketSetError(service, error_text != NULL
            ? (const char*)error_text : "cannot create handler generation");
        xrtFree(error_text);
        return service;
    }

    service->ready = 1;
    return service;
}

int xs2_socket_service_stop(void* handle);

void xs2_socket_service_destroy(void* handle)
{
    xs2_socket_service* service = (xs2_socket_service*)handle;
    if ( service == NULL ) {
        return;
    }
    (void)xs2_socket_service_stop(service);
    xs2SocketQueueDrain(service);
    xs2SocketCloseEndpoint(service);
    if ( service->engine != NULL ) {
        (void)xrtNetEngineDestroy(service->engine);
    }
    if ( service->handler != NULL ) {
        xs2_handler_slot_destroy(service->handler);
    }
    if ( service->changed != NULL ) {
        xrtCondDestroy(service->changed);
    }
    if ( service->mutex != NULL ) {
        xrtMutexDestroy(service->mutex);
    }
    xrtFree(service->worker_threads);
    xrtFree(service->queue);
    xrtFree(service->name);
    xrtFree(service->host);
    xrtFree(service->delimiter);
    xrtFree(service->last_error);
    xrtFree(service);
}

/* ------------------------------------ 生命周期 ------------------------------------ */

int xs2_socket_service_ready(void* handle)
{
    xs2_socket_service* service = (xs2_socket_service*)handle;
    return service != NULL && service->ready;
}

int xs2_socket_service_started(void* handle)
{
    xs2_socket_service* service = (xs2_socket_service*)handle;
    int started;
    if ( service == NULL || service->mutex == NULL ) {
        return 0;
    }
    xrtMutexLock(service->mutex);
    started = service->started;
    xrtMutexUnlock(service->mutex);
    return started;
}

int xs2_socket_service_start(void* handle)
{
    xs2_socket_service* service = (xs2_socket_service*)handle;
    int i;

    if ( service == NULL || !service->ready ) {
        return 0;
    }
    xrtMutexLock(service->mutex);
    if ( service->started ) {
        xrtMutexUnlock(service->mutex);
        return 1;
    }
    service->stopping = 0;
    service->started_workers = 0;
    xrtMutexUnlock(service->mutex);

    if ( !xs2SocketOpenEndpoint(service) ) {
        return 0;
    }

    for ( i = 0; i < service->connection_worker_count; ++i ) {
        service->worker_threads[i] = xrtThreadCreate((void*)xs2SocketWorkerThread, service, 0u);
        if ( service->worker_threads[i] == NULL ) {
            break;
        }
        service->started_workers += 1;
    }
    if ( service->started_workers == service->connection_worker_count ) {
        service->receiver_thread = xrtThreadCreate((void*)xs2SocketReceiverThread, service, 0u);
    }
    if ( service->started_workers != service->connection_worker_count || service->receiver_thread == NULL ) {
        xs2SocketSetError(service, "cannot create TCP/UDP service threads");
        xrtMutexLock(service->mutex);
        service->stopping = 1;
        xrtCondBroadcast(service->changed);
        xrtMutexUnlock(service->mutex);
        for ( i = 0; i < service->started_workers; ++i ) {
            xrtThreadWait(service->worker_threads[i]);
            xrtThreadDestroy(service->worker_threads[i]);
            service->worker_threads[i] = NULL;
        }
        service->started_workers = 0;
        xs2SocketCloseEndpoint(service);
        return 0;
    }

    xrtMutexLock(service->mutex);
    service->started = 1;
    xs2SocketSetErrorUnlocked(service, "");
    xrtMutexUnlock(service->mutex);
    return 1;
}

int xs2_socket_service_stop(void* handle)
{
    xs2_socket_service* service = (xs2_socket_service*)handle;
    int i;

    if ( service == NULL || service->mutex == NULL ) {
        return 0;
    }
    xrtMutexLock(service->mutex);
    if ( !service->started && service->started_workers == 0 && service->receiver_thread == NULL ) {
        xrtMutexUnlock(service->mutex);
        xs2SocketCloseEndpoint(service);
        return 1;
    }
    service->stopping = 1;
    service->started = 0;
    xrtCondBroadcast(service->changed);
    xrtMutexUnlock(service->mutex);

    /* 先释放内核端口并唤醒接收线程，再等待其退出。 */
    xs2SocketRequestEndpointClose(service);

    if ( service->receiver_thread != NULL ) {
        xrtThreadWait(service->receiver_thread);
        xrtThreadDestroy(service->receiver_thread);
        service->receiver_thread = NULL;
    }
    for ( i = 0; i < service->started_workers; ++i ) {
        if ( service->worker_threads[i] != NULL ) {
            xrtThreadWait(service->worker_threads[i]);
            xrtThreadDestroy(service->worker_threads[i]);
            service->worker_threads[i] = NULL;
        }
    }
    service->started_workers = 0;
    xs2SocketQueueDrain(service);
    return xs2SocketCloseEndpoint(service);
}

int xs2_socket_service_reload(void* handle)
{
    xs2_socket_service* service = (xs2_socket_service*)handle;
    void* error_text;
    if ( service == NULL || service->handler == NULL ) {
        return 0;
    }
    if ( xs2_handler_slot_reload(service->handler) ) {
        xs2SocketSetError(service, "");
        return 1;
    }
    error_text = xs2_handler_slot_last_error_copy(service->handler);
    xs2SocketSetError(service, error_text != NULL ? (const char*)error_text : "handler reload failed");
    xrtFree(error_text);
    return 0;
}

void* xs2_socket_service_prepare_reload(void* handle)
{
    xs2_socket_service* service = (xs2_socket_service*)handle;
    void* prepared;
    void* error_text;

    if ( service == NULL || service->handler == NULL ) {
        return NULL;
    }
    prepared = xs2_handler_slot_prepare_reload(service->handler);
    if ( prepared != NULL ) {
        xs2SocketSetError(service, "");
        return prepared;
    }
    error_text = xs2_handler_slot_last_error_copy(service->handler);
    xs2SocketSetError(service, error_text != NULL ? (const char*)error_text : "handler reload preparation failed");
    xrtFree(error_text);
    return NULL;
}

int xs2_socket_service_commit_reload(void* handle, void* prepared)
{
    xs2_socket_service* service = (xs2_socket_service*)handle;
    if ( service == NULL || service->handler == NULL || prepared == NULL ) {
        return 0;
    }
    if ( !xs2_handler_slot_commit_reload(service->handler, prepared) ) {
        xs2SocketSetError(service, "handler reload commit failed");
        return 0;
    }
    xs2SocketSetError(service, "");
    return 1;
}

void xs2_socket_service_discard_reload(void* handle, void* prepared)
{
    xs2_socket_service* service = (xs2_socket_service*)handle;
    if ( service != NULL && service->handler != NULL ) {
        xs2_handler_slot_discard_reload(service->handler, prepared);
    }
}

/* ------------------------------------ 状态与指标 ------------------------------------ */

int xs2_socket_service_port(void* handle)
{
    xs2_socket_service* service = (xs2_socket_service*)handle;
    return service != NULL ? service->port : 0;
}

void* xs2_socket_service_last_error_copy(void* handle)
{
    xs2_socket_service* service = (xs2_socket_service*)handle;
    char* text;
    if ( service == NULL || service->mutex == NULL ) {
        return xs2SocketCopyText("socket service is null");
    }
    xrtMutexLock(service->mutex);
    text = xs2SocketCopyText(service->last_error);
    xrtMutexUnlock(service->mutex);
    return text;
}

static long long xs2SocketMetric(void* handle, int kind)
{
    xs2_socket_service* service = (xs2_socket_service*)handle;
    uint64 value = 0u;
    if ( service == NULL || service->mutex == NULL ) {
        return 0;
    }
    xrtMutexLock(service->mutex);
    switch ( kind ) {
        case 1: value = service->accepted; break;
        case 2: value = service->received; break;
        case 3: value = service->sent; break;
        case 4: value = service->rejected; break;
        case 5: value = service->errors; break;
        case 6: value = service->active; break;
        case 7: value = (uint64)service->queue_count; break;
    }
    xrtMutexUnlock(service->mutex);
    return (long long)value;
}

long long xs2_socket_service_accepted(void* handle) { return xs2SocketMetric(handle, 1); }
long long xs2_socket_service_received(void* handle) { return xs2SocketMetric(handle, 2); }
long long xs2_socket_service_sent(void* handle) { return xs2SocketMetric(handle, 3); }
long long xs2_socket_service_rejected(void* handle) { return xs2SocketMetric(handle, 4); }
long long xs2_socket_service_errors(void* handle) { return xs2SocketMetric(handle, 5); }
long long xs2_socket_service_active(void* handle) { return xs2SocketMetric(handle, 6); }
long long xs2_socket_service_queued(void* handle) { return xs2SocketMetric(handle, 7); }
