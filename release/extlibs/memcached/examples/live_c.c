#define XRT_MODULE_NET_SOCKET
#define XRT_MODULE_BUFFER
#define XRT_MODULE_VALUE_CONTAINER
#include "../include/memcached_native.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned live_port(void)
{
    const char *text = getenv("MEMCACHED_LIVE_PORT");
    char *end;
    long port;
    if (text == NULL) return 11211u;
    port = strtol(text, &end, 10);
    return end != text && *end == '\0' && port >= 1 && port <= 65535 ?
        (unsigned)port : 0u;
}

static bool send_all(xnetsocket socket, const void *data, size_t size)
{
    size_t sent = 0;
    while (sent < size) {
        size_t done = 0;
        if (xrtNetSocketSend(socket, (const uint8 *)data + sent,
            size - sent, &done) != XNET_RESULT_OK || done == 0) return false;
        sent += done;
    }
    return true;
}

static xvalue *exchange(xnetsocket socket, const char *command,
    const void *body, size_t bodySize, bool hasBody)
{
    xbuffer received;
    xvalue *result = NULL;
    if (command == NULL || !xrtBufferInit(&received)) return NULL;
    if (!send_all(socket, command, strlen(command)) ||
        (hasBody && (!send_all(socket, body, bodySize) ||
            !send_all(socket, "\r\n", 2)))) goto done;
    while (received.Size <= 67108864u) {
        uint8 chunk[4096];
        size_t count = 0;
        int64 frame;
        if (xrtNetSocketRecv(socket, chunk, sizeof(chunk), &count) !=
            XNET_RESULT_OK || count == 0 ||
            !xrtBufferAppend(&received, (xbytesview){chunk, count})) goto done;
        frame = xmc_frame_size(xrtBufferView(&received));
        if (frame < 0 || (frame > 0 && (size_t)frame != received.Size)) goto done;
        if (frame > 0) {
            result = xmc_parse_response_span(xrtBufferView(&received));
            break;
        }
    }
done:
    xrtBufferUnit(&received);
    return result;
}

static xvalue *get_many(xnetsocket socket,
    const char *const *keys, size_t count)
{
    xvalue *items = xrtValueArray();
    xvalue *reply;
    char *command;
    size_t i;
    if (items == NULL) return NULL;
    for (i = 0; i < count; ++i) {
        if (!xrtValueArrayAppendNew(items,
            xrtValueString((xstrview){keys[i], strlen(keys[i])}))) {
            xrtValueRelease(items);
            return NULL;
        }
    }
    command = xmc_build_get(items);
    xrtValueRelease(items);
    reply = exchange(socket, command, NULL, 0, false);
    xrtFree(command);
    return reply;
}

static xvalue *get(xnetsocket socket, const char *key)
{
    return get_many(socket, &key, 1);
}

static xvalue *set(xnetsocket socket, const char *key,
    const void *data, size_t size)
{
    char *command = xmc_build_set((xstrview){key, strlen(key)},
        0, 0, (int64)size);
    xvalue *reply = exchange(socket, command, data, size, true);
    xrtFree(command);
    return reply;
}

static xvalue *add(xnetsocket socket, const char *key,
    const void *data, size_t size)
{
    char *command = xmc_build_add((xstrview){key, strlen(key)},
        0, 0, (int64)size);
    xvalue *reply = exchange(socket, command, data, size, true);
    xrtFree(command);
    return reply;
}

static xvalue *touch(xnetsocket socket, const char *key, int64 exptime)
{
    char *command = xmc_build_touch((xstrview){key, strlen(key)}, exptime);
    xvalue *reply = exchange(socket, command, NULL, 0, false);
    xrtFree(command);
    return reply;
}

static xvalue *del(xnetsocket socket, const char *key)
{
    char *command = xmc_build_delete((xstrview){key, strlen(key)});
    xvalue *reply = exchange(socket, command, NULL, 0, false);
    xrtFree(command);
    return reply;
}

static bool status_is(xvalue *reply, const char *word)
{
    xstrview text;
    xvalue *status = reply != NULL ?
        xrtValueObjectGet(reply, XRT_STR_LITERAL("status")) : NULL;
    return status != NULL && xrtValueGetString(status, &text) &&
        text.Size == strlen(word) && memcmp(text.Data, word, text.Size) == 0;
}

static xvalue *first_data(xvalue *reply)
{
    xvalue *items = reply != NULL ?
        xrtValueObjectGet(reply, XRT_STR_LITERAL("items")) : NULL;
    xvalue *item = items != NULL && xrtValueCount(items) == 1 ?
        xrtValueArrayAt(items, 0) : NULL;
    return item != NULL ? xrtValueObjectGet(item, XRT_STR_LITERAL("data")) : NULL;
}

int main(void)
{
    static const uint8 binary[] = {0xff, 0x00};
    static const char *const manyKeys[] = {
        "xlang:live:text", "xlang:live:binary", "xlang:live:absent"
    };
    xnetaddr address;
    xnetsocket socket = NULL;
    xvalue *reply = NULL;
    xstrview text;
    xbytesview bytes;
    int result = 1;
    unsigned port = live_port();
    if (port == 0 || !xrtNetAddrLoopback(&address, XNET_FAMILY_IPV4, port))
        goto done;
    socket = xrtNetSocketOpen(XNET_FAMILY_IPV4, XNET_SOCKET_STREAM, 0u);
    if (socket == NULL || xrtNetSocketConnect(socket, &address) !=
        XNET_RESULT_OK) goto done;

    reply = set(socket, "xlang:live:text", "hello", 5);
    if (!status_is(reply, "stored")) goto done;
    xrtValueRelease(reply); reply = get(socket, "xlang:live:text");
    if (!status_is(reply, "value") ||
        !xrtValueGetString(first_data(reply), &text) || text.Size != 5 ||
        memcmp(text.Data, "hello", 5) != 0) goto done;
    xrtValueRelease(reply); reply = set(socket, "xlang:live:binary", binary, 2);
    if (!status_is(reply, "stored")) goto done;
    xrtValueRelease(reply); reply = get(socket, "xlang:live:binary");
    if (!status_is(reply, "value") ||
        !xrtValueGetBytes(first_data(reply), &bytes) || bytes.Size != 2 ||
        memcmp(bytes.Data, binary, 2) != 0) goto done;
    xrtValueRelease(reply); reply = get_many(socket, manyKeys, 3);
    if (!status_is(reply, "value")) goto done;
    {
        xvalue *items = xrtValueObjectGet(reply, XRT_STR_LITERAL("items"));
        xvalue *first = items != NULL ? xrtValueArrayAt(items, 0) : NULL;
        xvalue *second = items != NULL ? xrtValueArrayAt(items, 1) : NULL;
        if (items == NULL || xrtValueCount(items) != 2 ||
            first == NULL || second == NULL ||
            !xrtValueGetString(xrtValueObjectGet(first,
                XRT_STR_LITERAL("data")), &text) ||
            text.Size != 5 || memcmp(text.Data, "hello", 5) != 0 ||
            !xrtValueGetBytes(xrtValueObjectGet(second,
                XRT_STR_LITERAL("data")), &bytes) ||
            bytes.Size != 2 || memcmp(bytes.Data, binary, 2) != 0) goto done;
    }
    xrtValueRelease(reply); reply = add(socket, "xlang:live:add", "first", 5);
    if (!status_is(reply, "stored")) goto done;
    xrtValueRelease(reply); reply = add(socket, "xlang:live:add", "second", 6);
    if (!status_is(reply, "not_stored")) goto done;
    xrtValueRelease(reply); reply = touch(socket, "xlang:live:text", 30);
    if (!status_is(reply, "touched")) goto done;
    xrtValueRelease(reply); reply = touch(socket, "xlang:live:absent", 30);
    if (!status_is(reply, "not_found")) goto done;
    xrtValueRelease(reply); reply = exchange(socket,
        xmc_build_version(), NULL, 0, false);
    if (!status_is(reply, "version") ||
        !xrtValueGetString(xrtValueObjectGet(reply,
            XRT_STR_LITERAL("version")), &text) || text.Size == 0) goto done;
    xrtValueRelease(reply); reply = exchange(socket,
        xmc_build_stats(), NULL, 0, false);
    if (!status_is(reply, "stats") ||
        xrtValueObjectGet(xrtValueObjectGet(reply,
            XRT_STR_LITERAL("stats")), XRT_STR_LITERAL("pid")) == NULL)
        goto done;
    xrtValueRelease(reply); reply = set(socket, "xlang:live:empty", "", 0);
    if (!status_is(reply, "stored")) goto done;
    xrtValueRelease(reply); reply = get(socket, "xlang:live:empty");
    if (!status_is(reply, "value") ||
        !xrtValueGetString(first_data(reply), &text) || text.Size != 0) goto done;
    xrtValueRelease(reply); reply = get(socket, "xlang:live:absent");
    if (!status_is(reply, "end")) goto done;
    xrtValueRelease(reply); reply = del(socket, "xlang:live:text");
    if (!status_is(reply, "deleted")) goto done;
    xrtValueRelease(reply); reply = del(socket, "xlang:live:text");
    if (!status_is(reply, "not_found")) goto done;
    xrtValueRelease(reply); reply = get(socket, "xlang:live:text");
    if (!status_is(reply, "end")) goto done;

    xrtValueRelease(reply); reply = exchange(socket,
        xmc_build_flush_all(), NULL, 0, false);
    if (!status_is(reply, "ok")) goto done;
    xrtValueRelease(reply); reply = get(socket, "xlang:live:binary");
    if (!status_is(reply, "end")) goto done;

    puts("text: hello");
    puts("binary bytes: 2");
    puts("empty vs missing: distinct");
    puts("delete: ok");
    result = 0;
done:
    xrtValueRelease(reply);
    if (socket != NULL) (void)xrtNetSocketClose(socket);
    if (result != 0) fputs("memcached live C example failed\n", stderr);
    return result;
}
