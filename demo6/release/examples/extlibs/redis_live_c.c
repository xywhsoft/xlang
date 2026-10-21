#define XRT_MODULE_BUFFER
#define XRT_MODULE_NET_SOCKET
#define XRT_MODULE_UNICODE
#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"
#include "../../extlibs/redis/include/redis_native.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* 一条命令对应一条 RESP 回复；连接由调用方持有。 */
static xvalue* command(xnetsocket socket, const char* const* words, size_t count)
{
    xvalue* args = xrtValueArray();
    xvalue* reply = NULL;
    xbuffer* request = NULL;
    xbuffer received;
    xbytesview bytes;
    size_t sent = 0u;
    size_t i;

    if (args == NULL) return NULL;
    for (i = 0u; i < count; ++i) {
        xvalue* word = xrtValueString((xstrview){words[i], strlen(words[i])});
        if (word == NULL || !xrtValueArrayAppendTake(args, &word)) {
            xrtValueRelease(word);
            goto done;
        }
    }
    request = (xbuffer*)xr_encode_bytes(args);
    if (request == NULL || !xrtBufferInit(&received)) goto done;
    bytes = xrtBufferView(request);
    while (sent < bytes.Size) {
        size_t n = 0u;
        if (xrtNetSocketSend(socket, bytes.Data + sent, bytes.Size - sent, &n) !=
            XNET_RESULT_OK || n == 0u) goto done_received;
        sent += n;
    }
    for (;;) {
        char chunk[4096];
        size_t n = 0u;
        bool complete;
        if (xrtNetSocketRecv(socket, chunk, sizeof(chunk), &n) != XNET_RESULT_OK ||
            n == 0u || received.Size > 65536u - n ||
            !xrtBufferAppend(&received, (xbytesview){(cbytes)chunk, n})) {
            goto done_received;
        }
        complete = xr_is_complete_raw(received.Data, received.Size);
        if (complete) break;
        if (xrtGetError() != NULL) goto done_received;
    }
    reply = xr_decode_raw(received.Data, received.Size);
    if (reply != NULL) {
        xvalue* rest = xrtValueObjectGet(reply, XRT_STR_LITERAL("rest"));
        xstrview text;
        if (rest == NULL || !xrtValueGetString(rest, &text) || text.Size != 0u) {
            xrtValueRelease(reply);
            reply = NULL;
        }
    }
done_received:
    xrtBufferUnit(&received);
done:
    xrtBufferDestroy(request);
    xrtValueRelease(args);
    return reply;
}

static bool response_text(xvalue* reply, const char* type, const char* expected)
{
    xvalue* tagged = reply != NULL ?
        xrtValueObjectGet(reply, XRT_STR_LITERAL("value")) : NULL;
    xstrview actual_type;
    xstrview actual_text;
    if (tagged == NULL ||
        !xrtValueGetString(xrtValueObjectGet(tagged, XRT_STR_LITERAL("type")),
            &actual_type) ||
        !xrtValueGetString(xrtValueObjectGet(tagged, XRT_STR_LITERAL("value")),
            &actual_text)) return false;
    return actual_type.Size == strlen(type) &&
        memcmp(actual_type.Data, type, actual_type.Size) == 0 &&
        actual_text.Size == strlen(expected) &&
        memcmp(actual_text.Data, expected, actual_text.Size) == 0;
}

int main(void)
{
    const char* ping[] = {"PING"};
    const char* set[] = {"SET", "xlang:example:live", "hello"};
    const char* get[] = {"GET", "xlang:example:live"};
    const char* del[] = {"DEL", "xlang:example:live"};
    xnetaddr address;
    xnetsocket socket = NULL;
    xvalue* reply = NULL;
    int64 removed = 0;
    int result = 1;

    unsigned port = redis_port();
    if (port == 0u || !xrtNetAddrLoopback(&address, XNET_FAMILY_IPV4, port)) goto done;
    socket = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_STREAM, 0u);
    if (socket == NULL || xrtNetSocketConnect(socket, &address) != XNET_RESULT_OK)
        goto done;
    reply = command(socket, ping, 1u);
    if (!response_text(reply, "simple", "PONG")) goto done;
    xrtValueRelease(reply);
    reply = command(socket, set, 3u);
    if (!response_text(reply, "simple", "OK")) goto done;
    xrtValueRelease(reply);
    reply = command(socket, get, 2u);
    if (!response_text(reply, "bulk", "hello")) goto done;
    xrtValueRelease(reply);
    reply = command(socket, del, 2u);
    if (reply == NULL || !xrtValueGetInt(
        xrtValueObjectGet(xrtValueObjectGet(reply, XRT_STR_LITERAL("value")),
            XRT_STR_LITERAL("value")), &removed) || removed != 1) goto done;
    puts("PING: PONG");
    puts("GET: hello");
    result = 0;
done:
    xrtValueRelease(reply);
    if (socket != NULL) (void)xrtNetSocketClose(socket);
    if (result != 0) fputs("redis live C example: failed\n", stderr);
    return result;
}

/* 可由 xl run 单文件运行；正式包仍按清单单独编译原生实现。 */
#include "../../extlibs/redis/redis_native.c"
