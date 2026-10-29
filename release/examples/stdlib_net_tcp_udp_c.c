#include "include/xl_clock.h"
#define XRT_MODULE_ALL
#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



/* Report an example failure and terminate the runtime. */
static void fail(const char* name)
{
	fprintf(stderr, "%s=fail\n", name);
	exit(1);
}



/* Print one result line shared with the xlang example. */
static void require_line(const char* name, bool condition)
{
	if ( !condition ) {
		fail(name);
	}
	printf("%s=ok\n", name);
}



/* Wait for a Future to resolve before its deadline. */
static void wait_future(const char* name, xfuture* future)
{
	if ( (future == NULL) ||
		 (xrtFutureWaitFor(future, UINT64_C(8000000)) != XWAIT_OK) ||
		 (xrtFutureState(future) != XFUTURE_RESOLVED) ) {
		xrtFutureDestroy(future);
		fail(name);
	}
}



/* Take an independent Stream reference from a resolved Future. */
static xnetstream* wait_stream(const char* name, xfuture* future)
{
	xnetstream* stream;

	wait_future(name, future);
	stream = xrtNetStreamRef((xnetstream*)xrtFutureValue(future));
	xrtFutureDestroy(future);
	if ( stream == NULL ) {
		fail(name);
	}
	return stream;
}



/* Take an independent UDP packet reference from a resolved Future. */
static xnetudppacket* wait_packet(const char* name, xfuture* future)
{
	xnetudppacket* packet;

	wait_future(name, future);
	packet = xrtNetUdpPacketRef(
		(xnetudppacket*)xrtFutureValue(future)
	);
	xrtFutureDestroy(future);
	if ( packet == NULL ) {
		fail(name);
	}
	return packet;
}



/* Compare an owned byte Future with an expected payload. */
static bool wait_bytes_equal(
	const char* name,
	xfuture* future,
	const void* expected,
	size_t size
)
{
	const xnetbytes* bytes;
	xbytesview view;
	bool equal;

	wait_future(name, future);
	bytes = (const xnetbytes*)xrtFutureValue(future);
	view = xrtNetBytesView(bytes);
	equal = (view.Size == size) &&
		((size == 0u) || (memcmp(view.Data, expected, size) == 0));
	xrtFutureDestroy(future);
	return equal;
}



/* Compare an owned synchronous receive result with an expected payload. */
static bool recv_bytes_equal(
	xnetstream* stream,
	const void* expected,
	size_t size
)
{
	xnetbytes* bytes = xrtNetStreamRecv(
		stream,
		0u,
		xl6DeadlineAfterMillisecondsV1(8000),
		NULL
	);
	xbytesview view = xrtNetBytesView(bytes);
	bool equal = (view.Size == size) &&
		((size == 0u) || (memcmp(view.Data, expected, size) == 0));

	xrtNetBytesDestroy(bytes);
	return equal;
}



/* Open a pull-mode TCP listener on a dynamic loopback port. */
static xnetlistener* open_listener(
	xnetengine* engine,
	xnetaddr* local
)
{
	xnetlistenconfig config;
	xnetlistener* listener;

	xrtNetListenConfigInit(&config);
	(void)xrtNetAddrLoopback(
		&config.Address,
		XNET_FAMILY_IPV4,
		0u
	);
	listener = xrtNetListen(
		engine,
		&config,
		NULL,
		NULL,
		NULL
	);
	if ( (listener == NULL) || !xrtNetListenerLocal(listener, local) ) {
		fail("tcp");
	}
	return listener;
}



/* Close a Stream and wait until all internal references leave the Engine. */
static bool close_stream(xnetstream* stream)
{
	return xrtNetStreamClose(stream) && xrtNetStreamWait(
		stream,
		XNET_STREAM_WAIT_CLOSE,
		xl6DeadlineAfterMillisecondsV1(8000),
		NULL
	);
}



/* Close a listener after all preposted accepts have terminated. */
static bool close_listener(xnetlistener* listener)
{
	XL6_DeadlineV1 deadline = xl6DeadlineAfterMillisecondsV1(8000);

	if ( !xrtNetListenerClose(listener) ) {
		return false;
	}
	while ( xrtNetListenerState(listener) != XNET_LISTENER_CLOSED ) {
		if ( xl6DeadlineExpiredV1(deadline) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}



/* Open a pull-mode UDP socket on a dynamic loopback port. */
static xnetudp* open_udp(xnetengine* engine, xnetaddr* local)
{
	xnetudpconfig config;
	xnetaddr address;
	xnetudp* udp;

	xrtNetUdpConfigInit(&config);
	(void)xrtNetAddrLoopback(&address, XNET_FAMILY_IPV4, 0u);
	udp = xrtNetUdpBind(
		engine,
		&address,
		0u,
		&config,
		NULL,
		NULL
	);
	if ( (udp == NULL) || !xrtNetUdpWait(
		udp,
		XNET_UDP_WAIT_OPEN,
		xl6DeadlineAfterMillisecondsV1(8000),
		NULL
	) || !xrtNetUdpLocal(udp, local) ) {
		fail("udp");
	}
	return udp;
}



/* Close a UDP socket and wait for all accepted sends and receives. */
static bool close_udp(xnetudp* udp)
{
	return xrtNetUdpClose(udp) && xrtNetUdpWait(
		udp,
		XNET_UDP_WAIT_CLOSE,
		xl6DeadlineAfterMillisecondsV1(8000),
		NULL
	);
}



/* Exercise current synchronous and Future TCP/UDP contracts. */
int main(void)
{
	xnetengineconfig engineConfig;
	xnetdialconfig dialConfig;
	xnetengine* engine;
	xnetresolver* resolver;
	xnetlistener* server;
	xnetlistener* asyncServer;
	xnetstream* client;
	xnetstream* peer;
	xnetstream* asyncClient;
	xnetstream* asyncPeer;
	xfuture* received;
	xfuture* asyncReceived;
	xfuture* asyncDrain;
	xnetaddr serverAddress;
	xnetaddr asyncServerAddress;
	static const unsigned char tcpBytes[] = {0u, 1u, 128u, 255u};
	bool tcpOk;
	bool tcpAsyncOk;

	xnetudp* left;
	xnetudp* right;
	xnetudp* asyncLeft;
	xnetudp* asyncRight;
	xnetaddr leftAddress;
	xnetaddr rightAddress;
	xnetaddr asyncLeftAddress;
	xnetaddr asyncRightAddress;
	xnetudppacket* packet;
	xnetudppacket* replyPacket;
	xnetudppacket* bytesPacket;
	xnetudppacket* asyncPacket;
	static const unsigned char udpBytes[] = {0u, 2u, 127u, 255u};
	bool udpOk;
	bool udpAsyncOk;

	xrtNetEngineConfigInit(&engineConfig);
	engineConfig.Workers = 2u;
	engine = xrtNetEngineCreate(&engineConfig);
	resolver = xrtNetResolverCreate(NULL);
	if ( (engine == NULL) || (resolver == NULL) ||
		 !xrtNetEngineStart(engine) ) {
		fail("network_init");
	}
	xrtNetDialConfigInit(&dialConfig);
	dialConfig.Timeout = UINT64_C(8000000);

	server = open_listener(engine, &serverAddress);
	received = xrtNetListenerAcceptAsync(server);
	client = xrtNetConnect(
		engine,
		resolver,
		"127.0.0.1",
		serverAddress.Port,
		&dialConfig,
		NULL,
		NULL,
		xl6DeadlineAfterMillisecondsV1(8000),
		NULL
	);
	peer = wait_stream("tcp", received);
	received = xrtNetStreamRecvAsync(peer, 0u);
	tcpOk = (serverAddress.Port != 0u) &&
		(xrtNetStreamSend(client, "ping", 4u) == XNET_RESULT_OK) &&
		xrtNetStreamWait(
			client,
			XNET_STREAM_WAIT_DRAIN,
			xl6DeadlineAfterMillisecondsV1(8000),
			NULL
		) &&
		wait_bytes_equal("tcp", received, "ping", 4u) &&
		(xrtNetStreamSend(peer, "pong", 4u) == XNET_RESULT_OK) &&
		xrtNetStreamWait(
			peer,
			XNET_STREAM_WAIT_DRAIN,
			xl6DeadlineAfterMillisecondsV1(8000),
			NULL
		) &&
		recv_bytes_equal(client, "pong", 4u) &&
		(xrtNetStreamSend(client, tcpBytes, sizeof(tcpBytes)) ==
			XNET_RESULT_OK) &&
		xrtNetStreamWait(
			client,
			XNET_STREAM_WAIT_DRAIN,
			xl6DeadlineAfterMillisecondsV1(8000),
			NULL
		) &&
		recv_bytes_equal(peer, tcpBytes, sizeof(tcpBytes)) &&
		close_stream(client) && close_stream(peer) &&
		close_listener(server);
	xrtNetStreamDestroy(client);
	xrtNetStreamDestroy(peer);
	xrtNetListenerDestroy(server);
	require_line("tcp", tcpOk);

	asyncServer = open_listener(engine, &asyncServerAddress);
	received = xrtNetListenerAcceptAsync(asyncServer);
	asyncClient = wait_stream(
		"tcp_async",
		xrtNetDialAsync(
			engine,
			resolver,
			"127.0.0.1",
			asyncServerAddress.Port,
			&dialConfig,
			NULL,
			NULL
		)
	);
	asyncPeer = wait_stream("tcp_async", received);
	asyncReceived = xrtNetStreamRecvAsync(asyncPeer, 0u);
	asyncDrain = xrtNetStreamWaitAsync(
		asyncClient,
		XNET_STREAM_WAIT_DRAIN
	);
	tcpAsyncOk = (asyncServerAddress.Port != 0u) &&
		(xrtNetStreamSend(asyncClient, "async", 5u) == XNET_RESULT_OK);
	wait_future("tcp_async", asyncDrain);
	xrtFutureDestroy(asyncDrain);
	tcpAsyncOk = tcpAsyncOk && wait_bytes_equal(
		"tcp_async",
		asyncReceived,
		"async",
		5u
	) && close_stream(asyncClient) && close_stream(asyncPeer) &&
		close_listener(asyncServer);
	xrtNetStreamDestroy(asyncClient);
	xrtNetStreamDestroy(asyncPeer);
	xrtNetListenerDestroy(asyncServer);
	require_line("tcp_async", tcpAsyncOk);

	left = open_udp(engine, &leftAddress);
	right = open_udp(engine, &rightAddress);
	udpOk = (xrtNetUdpSendTo(
		left,
		&rightAddress,
		"hello",
		5u
	) == XNET_RESULT_OK);
	packet = xrtNetUdpReceiveWait(
		right,
		xl6DeadlineAfterMillisecondsV1(8000),
		NULL
	);
	udpOk = udpOk && (packet != NULL) &&
		(xrtNetUdpPacketSize(packet) == 5u) &&
		(memcmp(xrtNetUdpPacketData(packet), "hello", 5u) == 0) &&
		(xrtNetUdpSendTo(
			right,
			&leftAddress,
			"reply",
			5u
		) == XNET_RESULT_OK);
	replyPacket = xrtNetUdpReceiveWait(
		left,
		xl6DeadlineAfterMillisecondsV1(8000),
		NULL
	);
	udpOk = udpOk && (replyPacket != NULL) &&
		(xrtNetUdpPacketSize(replyPacket) == 5u) &&
		(memcmp(xrtNetUdpPacketData(replyPacket), "reply", 5u) == 0) &&
		(xrtNetUdpSendTo(
			left,
			&rightAddress,
			udpBytes,
			sizeof(udpBytes)
		) == XNET_RESULT_OK);
	bytesPacket = xrtNetUdpReceiveWait(
		right,
		xl6DeadlineAfterMillisecondsV1(8000),
		NULL
	);
	udpOk = udpOk && (bytesPacket != NULL) &&
		(xrtNetUdpPacketSize(bytesPacket) == sizeof(udpBytes)) &&
		(memcmp(
			xrtNetUdpPacketData(bytesPacket),
			udpBytes,
			sizeof(udpBytes)
		) == 0);
	xrtNetUdpPacketDestroy(packet);
	xrtNetUdpPacketDestroy(replyPacket);
	xrtNetUdpPacketDestroy(bytesPacket);
	udpOk = udpOk && close_udp(left) && close_udp(right);
	xrtNetUdpDestroy(left);
	xrtNetUdpDestroy(right);
	require_line("udp", udpOk);

	asyncLeft = open_udp(engine, &asyncLeftAddress);
	asyncRight = open_udp(engine, &asyncRightAddress);
	received = xrtNetUdpReceiveAsync(asyncRight);
	udpAsyncOk = (xrtNetUdpSendTo(
		asyncLeft,
		&asyncRightAddress,
		"async-udp",
		9u
	) == XNET_RESULT_OK);
	asyncPacket = wait_packet("udp_async", received);
	udpAsyncOk = udpAsyncOk &&
		(xrtNetUdpPacketSize(asyncPacket) == 9u) &&
		(memcmp(
			xrtNetUdpPacketData(asyncPacket),
			"async-udp",
			9u
		) == 0);
	xrtNetUdpPacketDestroy(asyncPacket);
	udpAsyncOk = udpAsyncOk && close_udp(asyncLeft) &&
		close_udp(asyncRight);
	xrtNetUdpDestroy(asyncLeft);
	xrtNetUdpDestroy(asyncRight);
	require_line("udp_async", udpAsyncOk);

	if ( !xrtNetResolverDestroy(resolver) ||
		 !xrtNetEngineStop(engine) ||
		 !xrtNetEngineDestroy(engine) ) {
		fail("network_unit");
	}
	return 0;
}
