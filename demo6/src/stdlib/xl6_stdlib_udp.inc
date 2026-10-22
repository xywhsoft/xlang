const char* xl6StdlibUdpStateSource=
    "enum UdpState as int\n    opening = 0\n    open = 1\n    closing = 2\n    closed = 3\nend\n"
    "enum UdpWait as int\n    open = 0\n    receive = 1\n    error = 2\n    drain = 3\n    close = 4\nend\n";
const char* xl6StdlibUdpStatsClassSource=
    "class UdpStats\n    private native point handle\n"
    "    extern \"xl6UdpStatsDestroyV1\" @no_raise destroy()\n"
    "    extern \"xl6UdpStatsStateV1\" @pure @no_raise @nonblocking get state() as UdpState\n"
    "    extern \"xl6UdpStatsConnectedV1\" @pure @no_raise @nonblocking get connected() as bool\n"
#define XL6_UDP_STAT(Field,Name) "    extern \"xl6UdpStats" #Field "V1\" @pure @no_raise @nonblocking get " #Name "() as uint64\n"
#include "src/runtime/xl6_udp_stats_fields.h"
#undef XL6_UDP_STAT
    "end\n";
const char* xl6StdlibUdpClassSource=
    "class UdpPacket\n    private native point handle\n"
    "    extern \"xl6UdpPacketDestroyV1\" @no_raise destroy()\n"
    "    extern \"xl6UdpPacketLengthV1\" @pure @no_raise @nonblocking get length() as uint64\n"
    "    extern \"xl6UdpPacketTruncatedV1\" @pure @no_raise @nonblocking get truncated() as bool\n"
    "    extern \"xl6UdpPacketBytesV1\" @nonblocking func bytes(int size = -1, int offset = 0) as bytes\n"
    "    extern \"xl6UdpPacketRemoteV1\" @nonblocking get remote() as NetAddress\n"
    "    extern \"xl6UdpPacketInfoV1\" @nonblocking func info() as NetDatagramInfo\nend\n"
    "class UdpErrorPacket\n    private native point handle\n"
    "    extern \"xl6UdpErrorPacketDestroyV1\" @no_raise destroy()\n"
    "    extern \"xl6UdpErrorPacketLengthV1\" @pure @no_raise @nonblocking get length() as uint64\n"
    "    extern \"xl6UdpErrorPacketBytesV1\" @nonblocking func bytes(int size = -1, int offset = 0) as bytes\n"
    "    extern \"xl6UdpErrorPacketInfoV1\" @nonblocking func info() as NetDatagramErrorInfo\nend\n"
    "class UdpBatch\n    private native point handle\n"
    "    extern \"xl6UdpBatchDestroyV1\" @no_raise destroy()\n"
    "    extern \"xl6UdpBatchCountV1\" @no_raise @nonblocking get count() as uint64\n"
    "    extern \"xl6UdpBatchTakeV1\" @nonblocking func take(int index) as UdpPacket?\nend\n"
#include "xl6_stdlib_udp_message.inc"
    "class UdpSocket\n    private native point handle\n"
    "    extern \"xl6UdpBindDefaultV1\" create(NetEngine engine, NetAddress local)\n"
    "    extern \"xl6UdpBindV1\" create(NetEngine engine, NetAddress local, UdpConfig config, uint64 affinity = 0)\n"
    "    static extern \"xl6UdpConnectDefaultV1\" func connect(NetEngine engine, NetAddress peer) as UdpSocket\n"
    "    static extern \"xl6UdpConnectV1\" func connect(NetEngine engine, NetAddress peer, UdpConfig config, uint64 affinity = 0) as UdpSocket\n"
    "    static extern \"xl6UdpOpenV1\" func open(NetEngine engine, NetAddress local, NetAddress peer, UdpConfig config, uint64 affinity = 0) as UdpSocket\n"
    "    extern \"xl6UdpDestroyV1\" @no_raise destroy()\n"
    "    extern \"xl6UdpCloseV1\" @nonblocking func close()\n"
    "    extern \"xl6UdpAbortV1\" @nonblocking func abort()\n"
    "    extern \"xl6UdpStateV1\" @nonblocking get state() as UdpState\n"
    "    extern \"xl6UdpConnectedV1\" @nonblocking get connected() as bool\n"
    "    extern \"xl6UdpSendControlAvailableV1\" @nonblocking get sendControlAvailable() as NetDatagramControlFlags\n"
    "    extern \"xl6UdpLocalV1\" @nonblocking func localAddress() as NetAddress\n"
    "    extern \"xl6UdpPeerV1\" @nonblocking func peerAddress() as NetAddress\n"
    "    extern \"xl6UdpSnapshotV1\" @nonblocking func snapshot() as UdpStats\n"
    "    extern \"xl6UdpTryReceiveV1\" @nonblocking func tryRecv() as UdpPacket?\n"
    "    extern \"xl6UdpTryReceiveErrorV1\" @nonblocking func tryRecvError() as UdpErrorPacket?\n"
#define SEND(Name,Type) \
    "    extern \"xl6UdpSend" #Name "V1\" @nonblocking func send(" Type " data, int size = -1) as NetStatus\n" \
    "    extern \"xl6UdpSendTo" #Name "V1\" @nonblocking func sendTo(NetAddress remote, " Type " data, int size = -1) as NetStatus\n" \
    "    extern \"xl6UdpSendMessage" #Name "V1\" @nonblocking func sendMessage(NetAddress remote, NetDatagramControl control, " Type " data, int size = -1) as NetStatus\n" \
    "    extern \"xl6UdpSendConnectedMessage" #Name "V1\" @nonblocking func sendMessage(NetDatagramControl control, " Type " data, int size = -1) as NetStatus\n"
SEND(Text,"string")
SEND(Bytes,"bytes")
SEND(Buffer,"Buffer")
#undef SEND
#define REF(Name,Type) \
    "    extern \"xl6UdpSendRef" #Name "V1\" @nonblocking func sendRef(" Type " data, int size = -1, int offset = 0) as NetStatus\n" \
    "    extern \"xl6UdpSendRefTo" #Name "V1\" @nonblocking func sendRefTo(NetAddress remote, " Type " data, int size = -1, int offset = 0) as NetStatus\n" \
    "    extern \"xl6UdpSendRefMessage" #Name "V1\" @nonblocking func sendRefMessage(NetAddress remote, NetDatagramControl control, " Type " data, int size = -1, int offset = 0) as NetStatus\n" \
    "    extern \"xl6UdpSendRefConnectedMessage" #Name "V1\" @nonblocking func sendRefMessage(NetDatagramControl control, " Type " data, int size = -1, int offset = 0) as NetStatus\n"
REF(Bytes,"NetBytes")
REF(Packet,"UdpPacket")
#undef REF
    "    extern \"xl6UdpSendMoveV1\" @nonblocking func sendMove(Buffer data) as NetStatus\n"
    "    extern \"xl6UdpSendMoveToV1\" @nonblocking func sendMoveTo(NetAddress remote, Buffer data) as NetStatus\n"
    "    extern \"xl6UdpSendMoveMessageV1\" @nonblocking func sendMoveMessage(NetAddress remote, NetDatagramControl control, Buffer data) as NetStatus\n"
    "    extern \"xl6UdpSendMoveConnectedMessageV1\" @nonblocking func sendMoveMessage(NetDatagramControl control, Buffer data) as NetStatus\n"
#define WAIT(Name,Method,Argument) \
    "    extern \"xl6Udp" #Name "V1\" func " Method "(" Argument ", int timeoutMillis = -1) as NetStatus\n" \
    "    extern \"xl6Udp" #Name "CancelV1\" func " Method "Cancel(CancellationToken cancel, " Argument ", int timeoutMillis = -1) as NetStatus\n" \
    "    extern \"xl6Udp" #Name "UntilV1\" func " Method "Until(uint64 deadline, " Argument ") as NetStatus\n" \
    "    extern \"xl6Udp" #Name "UntilCancelV1\" func " Method "UntilCancel(uint64 deadline, CancellationToken cancel, " Argument ") as NetStatus\n"
WAIT(Wait,"wait","UdpWait condition")
WAIT(WaitWritable,"waitWritable","int size")
#undef WAIT
#define RECEIVE(Name,Method,Type) \
    "    extern \"xl6Udp" #Name "V1\" func " Method "(int timeoutMillis = -1) as " Type "\n" \
    "    extern \"xl6Udp" #Name "CancelV1\" func " Method "Cancel(CancellationToken cancel, int timeoutMillis = -1) as " Type "\n" \
    "    extern \"xl6Udp" #Name "UntilV1\" func " Method "Until(uint64 deadline) as " Type "\n" \
    "    extern \"xl6Udp" #Name "UntilCancelV1\" func " Method "UntilCancel(uint64 deadline, CancellationToken cancel) as " Type "\n"
RECEIVE(Receive,"recv","UdpPacket")
RECEIVE(ReceiveError,"recvError","UdpErrorPacket")
#undef RECEIVE
    "    extern \"xl6UdpReceiveBatchV1\" func recvBatch(int capacity = 16, int timeoutMillis = -1) as UdpBatch\n"
    "    extern \"xl6UdpReceiveBatchCancelV1\" func recvBatchCancel(CancellationToken cancel, int capacity = 16, int timeoutMillis = -1) as UdpBatch\n"
    "    extern \"xl6UdpReceiveBatchUntilV1\" func recvBatchUntil(uint64 deadline, int capacity = 16) as UdpBatch\n"
    "    extern \"xl6UdpReceiveBatchUntilCancelV1\" func recvBatchUntilCancel(uint64 deadline, CancellationToken cancel, int capacity = 16) as UdpBatch\n"
#include "xl6_stdlib_udp_future.inc"
#include "xl6_stdlib_udp_transfer.inc"
    "end\n";
