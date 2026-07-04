#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(const char* name)
{
    fprintf(stderr, "%s=fail\n", name);
    xrtUnit();
    exit(1);
}

static void require_line(const char* name, int ok)
{
    if ( !ok ) {
        fail(name);
    }
    printf("%s=ok\n", name);
}

static void* wait_point(const char* name, xfuture* future)
{
    void** box;
    void* value;

    if ( future == NULL || !xFutureWait(future) ) {
        if ( future != NULL ) {
            xFutureRelease(future);
        }
        fail(name);
    }
    box = (void**)xFutureValue(future);
    value = box != NULL ? *box : NULL;
    xFutureRelease(future);
    return value;
}

static int wait_bool(const char* name, xfuture* future)
{
    int* value;
    int ok;

    if ( future == NULL || !xFutureWait(future) ) {
        if ( future != NULL ) {
            xFutureRelease(future);
        }
        fail(name);
    }
    value = (int*)xFutureValue(future);
    ok = value != NULL && *value != 0;
    xFutureRelease(future);
    return ok;
}

static int wait_text_equals(const char* name, xfuture* future, const char* expected)
{
    char** box;
    const char* text;
    int ok;

    if ( future == NULL || !xFutureWait(future) ) {
        if ( future != NULL ) {
            xFutureRelease(future);
        }
        fail(name);
    }
    box = (char**)xFutureValue(future);
    text = (box != NULL && *box != NULL) ? *box : "";
    ok = strcmp(text, expected) == 0;
    xFutureRelease(future);
    return ok;
}

int main(void)
{
    xnetlistener* server;
    xnetstream* client;
    xnetstream* peer;
    xfuture* accepted;
    xfuture* received;
    char* reply;
    int tcpOk;

    xnetlistener* asyncServer;
    xnetstream* asyncClient;
    xnetstream* asyncPeer;
    xfuture* asyncAccepted;
    xfuture* asyncConnected;
    xfuture* asyncReceived;
    int tcpAsyncOk;

    xdgramsock* left;
    xdgramsock* right;
    xnetdgrampkt* packet;
    xnetdgrampkt* replyPacket;
    char* packetText;
    char* replyText;
    int udpOk;

    xdgramsock* asyncLeft;
    xdgramsock* asyncRight;
    xfuture* packetFuture;
    xnetdgrampkt* asyncPacket;
    char* asyncPacketText;
    int udpAsyncOk;

    xrtInit();

    server = xrtNetTcpListen("127.0.0.1", 0, 128);
    accepted = xrtNetTcpAcceptAsync(server, 3000);
    client = xrtNetTcpConnect("127.0.0.1", xrtNetTcpListenerPort(server), 3000);
    peer = (xnetstream*)wait_point("tcp", accepted);
    received = xrtNetStreamRecvTextAsync(peer, 0, 3000);
    tcpOk = xrtNetTcpListenerPort(server) > 0 &&
            xrtNetStreamSendText(client, "ping") &&
            xrtNetStreamDrain(client, 3000) &&
            wait_text_equals("tcp", received, "ping") &&
            xrtNetStreamSendText(peer, "pong") &&
            xrtNetStreamDrain(peer, 3000);
    reply = xrtNetStreamRecvText(client, 0, 3000);
    tcpOk = tcpOk && reply != NULL && strcmp(reply, "pong") == 0;
    xrtFree(reply);
    xrtNetTcpStreamClose(client);
    xrtNetTcpStreamClose(peer);
    require_line("tcp", tcpOk);

    asyncServer = xrtNetTcpListen("127.0.0.1", 0, 128);
    asyncAccepted = xrtNetTcpAcceptAsync(asyncServer, 3000);
    asyncConnected = xrtNetTcpConnectAsync("127.0.0.1", xrtNetTcpListenerPort(asyncServer), 3000);
    asyncClient = (xnetstream*)wait_point("tcp_async", asyncConnected);
    asyncPeer = (xnetstream*)wait_point("tcp_async", asyncAccepted);
    asyncReceived = xrtNetStreamRecvTextAsync(asyncPeer, 0, 3000);
    tcpAsyncOk = xrtNetTcpListenerPort(asyncServer) > 0 &&
                 wait_bool("tcp_async", xrtNetStreamSendTextAsync(asyncClient, "async")) &&
                 wait_bool("tcp_async", xrtNetStreamDrainAsync(asyncClient, 3000)) &&
                 wait_text_equals("tcp_async", asyncReceived, "async");
    xrtNetTcpStreamClose(asyncClient);
    xrtNetTcpStreamClose(asyncPeer);
    require_line("tcp_async", tcpAsyncOk);

    left = xrtNetUdpBind("127.0.0.1", 0);
    right = xrtNetUdpBind("127.0.0.1", 0);
    packet = NULL;
    replyPacket = NULL;
    packetText = NULL;
    replyText = NULL;
    udpOk = left != NULL && right != NULL &&
            xrtNetUdpLocalPort(left) > 0 &&
            xrtNetUdpLocalPort(right) > 0 &&
            xrtNetUdpSendText(left, "127.0.0.1", (uint16)xrtNetUdpLocalPort(right), "hello");
    packet = xrtNetUdpRecv(right, 3000);
    packetText = xrtNetDgramPacketText(packet);
    udpOk = udpOk && packetText != NULL && strcmp(packetText, "hello") == 0 &&
            xrtNetUdpSendText(right, "127.0.0.1", (uint16)xrtNetUdpLocalPort(left), "reply");
    replyPacket = xrtNetUdpRecv(left, 3000);
    replyText = xrtNetDgramPacketText(replyPacket);
    udpOk = udpOk && replyText != NULL && strcmp(replyText, "reply") == 0;
    xrtFree(packetText);
    xrtFree(replyText);
    xrtNetDgramPacketDestroy(packet);
    xrtNetDgramPacketDestroy(replyPacket);
    xrtNetUdpDestroy(left);
    xrtNetUdpDestroy(right);
    require_line("udp", udpOk);

    asyncLeft = xrtNetUdpBind("127.0.0.1", 0);
    asyncRight = xrtNetUdpBind("127.0.0.1", 0);
    packetFuture = xrtNetUdpRecvAsync(asyncRight, 3000);
    udpAsyncOk = asyncLeft != NULL && asyncRight != NULL &&
                 xrtNetUdpLocalPort(asyncLeft) > 0 &&
                 xrtNetUdpLocalPort(asyncRight) > 0 &&
                 wait_bool("udp_async", xrtNetUdpSendTextAsync(asyncLeft, "127.0.0.1", (uint16)xrtNetUdpLocalPort(asyncRight), "async-udp"));
    asyncPacket = (xnetdgrampkt*)wait_point("udp_async", packetFuture);
    asyncPacketText = xrtNetDgramPacketText(asyncPacket);
    udpAsyncOk = udpAsyncOk && asyncPacketText != NULL && strcmp(asyncPacketText, "async-udp") == 0;
    xrtFree(asyncPacketText);
    xrtNetDgramPacketDestroy(asyncPacket);
    xrtNetUdpDestroy(asyncLeft);
    xrtNetUdpDestroy(asyncRight);
    require_line("udp_async", udpAsyncOk);

    xrtNetTcpStreamDestroy(client);
    xrtNetTcpStreamDestroy(peer);
    xrtNetTcpListenerDestroy(server);
    xrtNetTcpStreamDestroy(asyncClient);
    xrtNetTcpStreamDestroy(asyncPeer);
    xrtNetTcpListenerDestroy(asyncServer);

    xrtUnit();
    return 0;
}
