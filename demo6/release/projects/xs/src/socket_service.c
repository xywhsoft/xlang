#include "handler_runtime.h"

#include "../include/xs_handler.h"
#include "xrt.h"

#include <stdint.h>
#include <string.h>

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
    int recv_limit;
    int frame_limit;
    int idle_timeout_ms;
    int write_timeout_ms;
    xs2_tcp_framing framing;
    size_t delimiter_size;

    void* handler;
    xnetlistener* listener;
    xdgramsock* udp;

    xmutex mutex;
    xcond changed;
    void** queue;
    int queue_capacity;
    int queue_head;
    int queue_tail;
    int queue_count;

    xthread receiver_thread;
    xthread* worker_threads;
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
    return (char*)xrtCopyStr((str)(void*)text, 0u);
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
        xrtNetTcpStreamDestroy((xnetstream*)item);
    } else {
        xrtNetDgramPacketDestroy((xnetdgrampkt*)item);
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

static void xs2SocketCloseEndpoint(xs2_socket_service* service)
{
    if ( service->listener != NULL ) {
        xrtNetTcpListenerDestroy(service->listener);
        service->listener = NULL;
    }
    if ( service->udp != NULL ) {
        xrtNetUdpDestroy(service->udp);
        service->udp = NULL;
    }
    service->port = service->bind_port;
}

static int xs2SocketOpenEndpoint(xs2_socket_service* service)
{
    if ( service->type == XS2_SOCKET_TCP ) {
        service->listener = xrtNetTcpListen(
            service->host, (uint16)service->bind_port, (uint32)service->backlog);
        if ( service->listener == NULL ) {
            xs2SocketSetError(service, "cannot bind TCP listener");
            return 0;
        }
        service->port = (int)xrtNetTcpListenerPort(service->listener);
    } else {
        service->udp = xrtNetUdpBind(service->host, (uint16)service->bind_port);
        if ( service->udp == NULL ) {
            xs2SocketSetError(service, "cannot bind UDP socket");
            return 0;
        }
        service->port = xrtNetUdpLocalPort(service->udp);
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
    xbuffer output)
{
    int result;

    output->Length = 0u;
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

static int xs2SocketSendTcp(xs2_socket_service* service, xnetstream* stream, xbuffer output)
{
    const void* suffix = NULL;
    size_t suffix_size = 0u;
    unsigned char prefix[4];
    size_t prefix_size = 0u;
    size_t wire_size;

    if ( output->Length == 0u ) {
        return 1;
    }
    if ( output->Length > (uint32)service->frame_limit ) {
        xs2SocketSetError(service, "TCP handler output exceeds frameLimit");
        xs2SocketAddMetric(service, &service->errors, 1);
        return 0;
    }
    if ( service->framing == XS2_TCP_FRAMING_TEXT ) {
        if ( !xrtIsUTF8((str)output->Buffer, output->Length) ||
             memchr(output->Buffer, '\r', output->Length) != NULL ||
             memchr(output->Buffer, '\n', output->Length) != NULL ) {
            xs2SocketSetError(service, "TCP text handler output is not one valid UTF-8 line");
            xs2SocketAddMetric(service, &service->errors, 1);
            return 0;
        }
        suffix = "\n";
        suffix_size = 1u;
    } else if ( service->framing == XS2_TCP_FRAMING_DELIMITER ) {
        size_t index;
        for ( index = 0u; index + service->delimiter_size <= output->Length; ++index ) {
            if ( memcmp(output->Buffer + index, service->delimiter, service->delimiter_size) == 0 ) {
                xs2SocketSetError(service, "TCP handler output contains the configured delimiter");
                xs2SocketAddMetric(service, &service->errors, 1);
                return 0;
            }
        }
        suffix = service->delimiter;
        suffix_size = service->delimiter_size;
    } else if ( service->framing == XS2_TCP_FRAMING_LENGTH_PREFIX ) {
        uint32 length = output->Length;
        prefix[0] = (unsigned char)(length >> 24);
        prefix[1] = (unsigned char)(length >> 16);
        prefix[2] = (unsigned char)(length >> 8);
        prefix[3] = (unsigned char)length;
        prefix_size = sizeof(prefix);
    }

    if ( (prefix_size != 0u && !xrtNetStreamSendBytes(stream, prefix, prefix_size)) ||
         !xrtNetStreamSendBytes(stream, output->Buffer, output->Length) ||
         (suffix_size != 0u && !xrtNetStreamSendBytes(stream, suffix, suffix_size)) ||
         !xrtNetStreamDrain(stream, service->write_timeout_ms) ) {
        xs2SocketAddMetric(service, &service->errors, 1);
        return 0;
    }
    wire_size = prefix_size + output->Length + suffix_size;
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

static void xs2SocketConsume(xbuffer pending, size_t size)
{
    size_t remaining;
    if ( pending == NULL || size == 0u ) {
        return;
    }
    if ( size >= pending->Length ) {
        pending->Length = 0u;
        return;
    }
    remaining = pending->Length - size;
    memmove(pending->Buffer, pending->Buffer + size, remaining);
    pending->Length = (uint32)remaining;
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
    xbuffer output)
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
    if ( output->Length != 0u && !xs2SocketSendTcp(service, stream, output) ) {
        return 0;
    }
    return result == XS_HANDLER_OK;
}

static int xs2SocketDispatchPending(
    xs2_socket_service* service,
    xnetstream* stream,
    xbuffer pending,
    const char* remote_address,
    unsigned int remote_port,
    xbuffer output)
{
    for ( ;; ) {
        size_t payload_size;
        size_t consumed;

        if ( service->framing == XS2_TCP_FRAMING_LENGTH_PREFIX ) {
            const unsigned char* bytes;
            uint32 declared;
            if ( pending->Length < 4u ) {
                return 1;
            }
            bytes = (const unsigned char*)pending->Buffer;
            declared = ((uint32)bytes[0] << 24) | ((uint32)bytes[1] << 16) |
                       ((uint32)bytes[2] << 8) | (uint32)bytes[3];
            if ( declared > (uint32)service->frame_limit ) {
                return xs2SocketProtocolError(service, "TCP length-prefix frame exceeds frameLimit");
            }
            if ( pending->Length < declared + 4u ) {
                return 1;
            }
            payload_size = declared;
            consumed = payload_size + 4u;
            if ( !xs2SocketDispatchTcpFrame(service, stream, pending->Buffer + 4u, payload_size,
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
                pending->Buffer, pending->Length, delimiter, delimiter_size);
            if ( found == NULL ) {
                size_t maximum_pending = (size_t)service->frame_limit + delimiter_size - 1u;
                if ( pending->Length > maximum_pending ) {
                    return xs2SocketProtocolError(service, "TCP delimited frame exceeds frameLimit");
                }
                return 1;
            }
            payload_size = (size_t)(found - pending->Buffer);
            consumed = payload_size + delimiter_size;
            if ( payload_size > (size_t)service->frame_limit ) {
                return xs2SocketProtocolError(service, "TCP delimited frame exceeds frameLimit");
            }
            if ( service->framing == XS2_TCP_FRAMING_TEXT && payload_size > 0u &&
                 pending->Buffer[payload_size - 1u] == '\r' ) {
                payload_size -= 1u;
            }
            if ( service->framing == XS2_TCP_FRAMING_TEXT &&
                 !xrtIsUTF8((str)pending->Buffer, payload_size) ) {
                return xs2SocketProtocolError(service, "TCP text frame is not valid UTF-8");
            }
            if ( !xs2SocketDispatchTcpFrame(service, stream, pending->Buffer, payload_size,
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
    xbuffer output;
    xbuffer pending = NULL;
    char* remote_address;
    unsigned int remote_port;
    uint64 last_activity;
    int result;

    output = xrtBufferCreate(4096u);
    if ( service->framing != XS2_TCP_FRAMING_RAW ) {
        pending = xrtBufferCreate(service->frame_limit < 65536 ? (uint32)service->frame_limit : 65536u);
    }
    remote_address = (char*)xrtNetStreamRemoteAddrText(stream);
    remote_port = (unsigned int)xrtNetStreamRemotePort(stream);
    if ( output == NULL || (service->framing != XS2_TCP_FRAMING_RAW && pending == NULL) ) {
        xs2SocketAddMetric(service, &service->errors, 1);
        xrtFree(remote_address);
        if ( pending != NULL ) {
            xrtBufferDestroy(pending);
        }
        if ( output != NULL ) {
            xrtBufferDestroy(output);
        }
        xrtNetTcpStreamDestroy(stream);
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
    if ( output->Length != 0u ) {
        (void)xs2SocketSendTcp(service, stream, output);
    }

    last_activity = xs2SocketNowMs();
    while ( result == XS_HANDLER_OK && !xs2SocketIsStopping(service) ) {
        size_t data_size = 0u;
        size_t read_limit = service->framing == XS2_TCP_FRAMING_RAW
            ? (size_t)service->frame_limit
            : (size_t)service->recv_limit - pending->Length;
        void* data;

        if ( read_limit == 0u ) {
            (void)xs2SocketProtocolError(service, "TCP receive buffer exceeds recvLimit");
            break;
        }
        if ( read_limit > 65536u ) {
            read_limit = 65536u;
        }
        data = xrtNetStreamRecvBytes(stream, read_limit, 100, &data_size);

        if ( data != NULL && data_size != 0u ) {
            last_activity = xs2SocketNowMs();
            xs2SocketAddMetric(service, &service->received, data_size);
            if ( service->framing == XS2_TCP_FRAMING_RAW ) {
                result = xs2SocketDispatchTcpFrame(
                    service, stream, data, data_size, remote_address, remote_port, output)
                    ? XS_HANDLER_OK : XS_HANDLER_ERROR;
            } else if ( !xrtBufferAppend(pending, data, (uint32)data_size, 0u) ) {
                xs2SocketSetError(service, "cannot grow TCP framing buffer");
                xs2SocketAddMetric(service, &service->errors, 1);
                result = XS_HANDLER_ERROR;
            } else {
                result = xs2SocketDispatchPending(
                    service, stream, pending, remote_address, remote_port, output)
                    ? XS_HANDLER_OK : XS_HANDLER_ERROR;
            }
            xrtFree(data);
            continue;
        }
        xrtFree(data);

        {
            xnet_result wait_result = xrtNetStreamWaitTimeoutEx(stream, XNET_STREAM_WAIT_READABLE, 0u);
            if ( wait_result == XRT_NET_CLOSED || wait_result == XRT_NET_CANCELLED || wait_result == XRT_NET_ERROR ) {
                break;
            }
        }
        if ( service->idle_timeout_ms > 0 &&
             xs2SocketNowMs() - last_activity >= (uint64)service->idle_timeout_ms ) {
            break;
        }
    }

    if ( result == XS_HANDLER_OK && pending != NULL && pending->Length != 0u &&
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
    xrtNetTcpStreamDestroy(stream);
}

/* ------------------------------------ UDP 工作线程 ------------------------------------ */

static void xs2SocketHandleUdp(xs2_socket_service* service, xnetdgrampkt* packet)
{
    xbuffer output;
    char* remote_address;
    int remote_port;
    size_t data_size;
    void* data;
    int result;

    data_size = xrtNetDgramPacketBytes(packet);
    if ( data_size > (size_t)service->recv_limit ) {
        xs2SocketAddMetric(service, &service->rejected, 1);
        xrtNetDgramPacketDestroy(packet);
        return;
    }
    data = xrtNetDgramPacketBytesCopy(packet, &data_size);
    if ( data_size != 0u && data == NULL ) {
        xs2SocketAddMetric(service, &service->errors, 1);
        xrtNetDgramPacketDestroy(packet);
        return;
    }
    output = xrtBufferCreate(4096u);
    remote_address = (char*)xrtNetDgramPacketFromText(packet);
    remote_port = xrtNetDgramPacketFromPort(packet);
    if ( output == NULL || remote_address == NULL ) {
        xs2SocketAddMetric(service, &service->errors, 1);
        xrtFree(data);
        xrtFree(remote_address);
        if ( output != NULL ) {
            xrtBufferDestroy(output);
        }
        xrtNetDgramPacketDestroy(packet);
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
    if ( result == XS_HANDLER_OK && output->Length != 0u ) {
        if ( xrtNetUdpSendBytes(service->udp, remote_address, (uint16)remote_port,
                                output->Buffer, output->Length) ) {
            xs2SocketAddMetric(service, &service->sent, output->Length);
        } else {
            xs2SocketAddMetric(service, &service->errors, 1);
        }
    }
    xs2SocketAddMetric(service, &service->active, -1);
    xrtFree(data);
    xrtFree(remote_address);
    xrtBufferDestroy(output);
    xrtNetDgramPacketDestroy(packet);
}

/* ------------------------------------ 线程入口 ------------------------------------ */

static uint32 xs2SocketReceiverThread(void* argument)
{
    xs2_socket_service* service = (xs2_socket_service*)argument;

    while ( !xs2SocketIsStopping(service) ) {
        void* item;
        if ( service->type == XS2_SOCKET_TCP ) {
            item = xrtNetTcpAccept(service->listener, 100);
            if ( item != NULL ) {
                xs2SocketAddMetric(service, &service->accepted, 1);
            }
        } else {
            item = xrtNetUdpRecv(service->udp, 100);
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
            xs2SocketHandleUdp(service, (xnetdgrampkt*)item);
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
         queue_capacity <= 0 || recv_limit <= 0 || recv_limit > 67108864 ||
         frame_limit <= 0 || frame_limit > recv_limit || service->framing == 0 ||
         (service->type == XS2_SOCKET_UDP && service->framing != XS2_TCP_FRAMING_RAW) ||
         (service->framing == XS2_TCP_FRAMING_DELIMITER &&
          (service->delimiter_size == 0u || service->delimiter_size > 256u)) ||
         idle_timeout_ms < 0 || write_timeout_ms < 0 ) {
        xs2SocketSetError(service, "invalid TCP/UDP service configuration");
        return service;
    }

    service->queue = (void**)xrtCalloc((size_t)queue_capacity, sizeof(void*));
    service->worker_threads = (xthread*)xrtCalloc((size_t)worker_count, sizeof(xthread));
    if ( service->queue == NULL || service->worker_threads == NULL ) {
        xs2SocketSetError(service, "cannot allocate TCP/UDP service queue or workers");
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

    for ( i = 0; i < service->worker_count; ++i ) {
        service->worker_threads[i] = xrtThreadCreate((void*)xs2SocketWorkerThread, service, 0u);
        if ( service->worker_threads[i] == NULL ) {
            break;
        }
        service->started_workers += 1;
    }
    if ( service->started_workers == service->worker_count ) {
        service->receiver_thread = xrtThreadCreate((void*)xs2SocketReceiverThread, service, 0u);
    }
    if ( service->started_workers != service->worker_count || service->receiver_thread == NULL ) {
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
    xs2SocketCloseEndpoint(service);
    return 1;
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
