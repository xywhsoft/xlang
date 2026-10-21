#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/redis_native.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct argpart {
    const char* data;
    size_t size;
    bool binary;
} argpart;

#define PART(text) { (text), sizeof(text) - 1u, false }
#define BPART(text) { (text), sizeof(text) - 1u, true }
#define COUNT(parts) (sizeof(parts) / sizeof((parts)[0]))

static unsigned redis_port(void)
{
    const char* text = getenv("REDIS_LIVE_PORT");
    char* end;
    long port;
    if (text == NULL) return 6379u;
    port = strtol(text, &end, 10);
    return end != text && *end == '\0' && port >= 1 && port <= 65535 ?
        (unsigned)port : 0u;
}

static xvalue* send_command(xnetsocket socket, size_t count, const argpart* parts)
{
    xvalue* args = xrtValueArray();
    xbuffer* request = NULL;
    xvalue* reply = NULL;
    xbuffer received;
    xbytesview bytes;
    size_t i;
    size_t sent = 0u;
    bool ready = false;
    if (args == NULL || !xrtBufferInit(&received)) goto done;
    for (i = 0u; i < count; ++i) {
        xvalue* part = parts[i].binary ?
            xrtValueBytes((xbytesview){ (cbytes)parts[i].data, parts[i].size }) :
            xrtValueString((xstrview){ parts[i].data, parts[i].size });
        if (part == NULL || !xrtValueArrayAppendTake(args, &part)) {
            xrtValueRelease(part);
            goto done_buffer;
        }
    }
    request = (xbuffer*)xr_encode_bytes(args);
    if (request == NULL) goto done_buffer;
    bytes = xrtBufferView(request);
    while (sent < bytes.Size) {
        size_t n = 0u;
        if (xrtNetSocketSend(socket, bytes.Data + sent, bytes.Size - sent, &n) != XNET_RESULT_OK ||
            n == 0u) goto done_buffer;
        sent += n;
    }
    for (i = 0u; i < 64u; ++i) {
        char chunk[4096];
        size_t n = 0u;
        if (xrtNetSocketRecv(socket, chunk, sizeof(chunk), &n) != XNET_RESULT_OK ||
            n == 0u || !xrtBufferAppend(&received, (xbytesview){ (cbytes)chunk, n }))
            goto done_buffer;
        if (received.Size > 65536u) goto done_buffer;
        if (xr_is_complete_raw(received.Data, received.Size)) {
            ready = true;
            break;
        }
    }
    if (ready) reply = xr_decode_raw(received.Data, received.Size);
done_buffer:
    xrtBufferUnit(&received);
done:
    xrtBufferDestroy(request);
    xrtValueRelease(args);
    return reply;
}

static xvalue* payload(xvalue* reply)
{
    xvalue* tagged;
    if (reply == NULL) return NULL;
    tagged = xrtValueObjectGet(reply, XRT_STR_LITERAL("value"));
    return tagged != NULL ? xrtValueObjectGet(tagged, XRT_STR_LITERAL("value")) : NULL;
}

static bool is_text(xvalue* reply, const char* text, size_t length)
{
    xstrview actual;
    return xrtValueGetString(payload(reply), &actual) && actual.Size == length &&
        memcmp(actual.Data, text, length) == 0;
}

int main(void)
{
    xnetsocket socket = NULL;
    xnetaddr address;
    xvalue* reply = NULL;
    xvalue* rows;
    int64 count;
    int result = 1;
    const argpart ping[] = { PART("PING") };
    const argpart set[] = { PART("SET"), PART("xlang:test:redis:c-live"), PART("a\0b") };
    const argpart get[] = { PART("GET"), PART("xlang:test:redis:c-live") };
    const argpart binarySet[] = { PART("SET"), BPART("k\0\xff"), BPART("\x00\xff" "A") };
    const argpart binaryGet[] = { PART("GET"), BPART("k\0\xff") };
    const argpart binaryDel[] = { PART("DEL"), BPART("k\0\xff") };
    const argpart push[] = { PART("RPUSH"), PART("xlang:test:redis:c-list"), PART("one"), PART("two") };
    const argpart range[] = { PART("LRANGE"), PART("xlang:test:redis:c-list"), PART("0"), PART("-1") };
    const argpart missing[] = { PART("__XL_UNSUPPORTED_COMMAND__") };
    const argpart del[] = { PART("DEL"), PART("xlang:test:redis:c-live"), PART("xlang:test:redis:c-list") };

    unsigned port = redis_port();
    if (port == 0u || !xrtNetAddrLoopback(&address, XNET_FAMILY_IPV4, port)) goto done;
    socket = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_STREAM, 0u);
    if (socket == NULL || xrtNetSocketConnect(socket, &address) != XNET_RESULT_OK) goto done;

    reply = send_command(socket, COUNT(del), del);
    if (reply == NULL) goto done;
    xrtValueRelease(reply);

    reply = send_command(socket, COUNT(ping), ping);
    if (!is_text(reply, "PONG", 4u)) goto done;
    xrtValueRelease(reply);
    reply = send_command(socket, COUNT(set), set);
    if (!is_text(reply, "OK", 2u)) goto done;
    xrtValueRelease(reply);
    reply = send_command(socket, COUNT(get), get);
    if (!is_text(reply, "a\0b", 3u)) goto done;
    xrtValueRelease(reply);
    reply = send_command(socket, COUNT(binarySet), binarySet);
    if (!is_text(reply, "OK", 2u)) goto done;
    xrtValueRelease(reply);
    reply = send_command(socket, COUNT(binaryGet), binaryGet);
    {
        xbytesview actual;
        if (!xrtValueGetBytes(payload(reply), &actual) || actual.Size != 3u ||
            memcmp(actual.Data, "\x00\xff" "A", 3u) != 0) goto done;
    }
    xrtValueRelease(reply);
    reply = send_command(socket, COUNT(binaryDel), binaryDel);
    if (!xrtValueGetInt(payload(reply), &count) || count != 1) goto done;
    xrtValueRelease(reply);
    reply = send_command(socket, COUNT(push), push);
    if (!xrtValueGetInt(payload(reply), &count) || count != 2) goto done;
    xrtValueRelease(reply);
    reply = send_command(socket, COUNT(range), range);
    rows = payload(reply);
    if (xrtValueType(rows) != XVALUE_ARRAY || xrtValueCount(rows) != 2u) goto done;
    xrtValueRelease(reply);
    reply = send_command(socket, COUNT(missing), missing);
    rows = xrtValueObjectGet(reply, XRT_STR_LITERAL("value"));
    {
        /* 错误正文因 Redis 版本而变化；只校验稳定的 RESP error 类型。 */
        xstrview tag;
        if (rows == NULL ||
            !xrtValueGetString(xrtValueObjectGet(rows, XRT_STR_LITERAL("type")), &tag) ||
            tag.Size != 5u || memcmp(tag.Data, "error", 5u) != 0) goto done;
    }
    xrtValueRelease(reply);
    reply = send_command(socket, COUNT(del), del);
    if (!xrtValueGetInt(payload(reply), &count) || count != 2) goto done;
    puts("redis real server C: ok");
    result = 0;
done:
    xrtValueRelease(reply);
    if (socket != NULL) (void)xrtNetSocketClose(socket);
    if (result != 0) fputs("redis real server C: failed\n", stderr);
    return result;
}
