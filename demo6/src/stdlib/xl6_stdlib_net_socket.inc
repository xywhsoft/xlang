#include "xl6_stdlib_net_datagram.inc"
#include "xl6_stdlib_net_vector.inc"
#include "xl6_stdlib_net_batch.inc"
const char* xl6StdlibNetSocketSource=
    XL6_NET_DATAGRAM_SOURCE
    XL6_NET_BATCH_SOURCE
    "enum NetStatus as int\n    failed = -1\n    ok = 0\n    again = 1\n    closed = 2\n"
    "    truncated = 3\n    timeout = 4\n    cancelled = 5\nend\n"
    "enum NetSocketKind as int\n    stream = 1\n    datagram = 2\nend\n"
    "enum NetShutdown as int\n    read = 1\n    write = 2\n    both = 3\nend\n"
    "enum NetSocketOption as int\n"
#define XL6_NET_SOCKET_OPTION(Name,Value) "    " #Name " = " #Value "\n"
#include "src/runtime/xl6_net_socket_options.inc"
#undef XL6_NET_SOCKET_OPTION
    "end\n"
    "class NetSocket\n    private native point handle\n"
    "    extern \"xl6NetSocketEmptyV1\" @nonblocking create()\n"
    "    extern \"xl6NetSocketCreateV1\" create(NetSocketKind kind, int family = 4, bool nonblocking = true)\n"
    "    extern \"xl6NetSocketDestroyV1\" destroy()\n"
    "    extern \"xl6NetSocketCloseV1\" func close()\n"
    "    extern \"xl6NetSocketClosedV1\" @no_raise @nonblocking get isClosed() as bool\n"
    "    extern \"xl6NetSocketFamilyV1\" @nonblocking get family() as int\n"
    "    extern \"xl6NetSocketKindV1\" @nonblocking get kind() as NetSocketKind\n"
    "    extern \"xl6NetSocketAvailableV1\" func available() as uint64\n"
    "    extern \"xl6NetSocketSetV1\" func setOption(NetSocketOption option, int value)\n"
    "    extern \"xl6NetSocketGetV1\" func getOption(NetSocketOption option) as int\n"
    "    extern \"xl6NetSocketBindV1\" func bind(NetAddress address)\n"
    "    extern \"xl6NetSocketListenV1\" func listen(int backlog = 128)\n"
    "    extern \"xl6NetSocketConnectV1\" func connect(NetAddress address) as NetStatus\n"
    "    extern \"xl6NetSocketFinishConnectV1\" func finishConnect() as NetStatus\n"
    "    extern \"xl6NetSocketAcceptIntoV1\" func acceptInto(NetSocket destination) as NetStatus\n"
    "    extern \"xl6NetSocketShutdownV1\" func shutdown(NetShutdown direction)\n"
    "    extern \"xl6NetSocketLocalV1\" func localAddress() as NetAddress\n"
    "    extern \"xl6NetSocketRemoteV1\" func remoteAddress() as NetAddress\n"
    "    extern \"xl6NetSocketSendBytesV1\" func send(bytes data, int size = -1) as (NetStatus, uint64)\n"
    "    extern \"xl6NetSocketSendTextV1\" func send(string data, int size = -1) as (NetStatus, uint64)\n"
    "    extern \"xl6NetSocketSendBufferV1\" func send(Buffer data, int size = -1) as (NetStatus, uint64)\n"
    "    extern \"xl6NetSocketSendToBytesV1\" func sendTo(bytes data, NetAddress target, int size = -1) as (NetStatus, uint64)\n"
    "    extern \"xl6NetSocketSendToTextV1\" func sendTo(string data, NetAddress target, int size = -1) as (NetStatus, uint64)\n"
    "    extern \"xl6NetSocketSendToBufferV1\" func sendTo(Buffer data, NetAddress target, int size = -1) as (NetStatus, uint64)\n"
    "    extern \"xl6NetSocketReceiveIntoV1\" func receiveInto(Buffer output, int maxBytes = 65536) as (NetStatus, uint64)\n"
    XL6_NET_SOCKET_DATAGRAM_METHODS
    XL6_NET_SOCKET_VECTOR_METHODS
    XL6_NET_SOCKET_BATCH_METHODS
    "end\n";
#undef XL6_NET_DATAGRAM_SOURCE
#undef XL6_NET_SOCKET_DATAGRAM_METHODS
#undef XL6_NET_DATAGRAM_ERROR_SOURCE
#undef XL6_NET_SOCKET_DATAGRAM_ERROR_METHODS
#undef XL6_NET_SOCKET_VECTOR_METHODS
#undef XL6_NET_VECTOR_OVERLOADS
#undef XL6_NET_BATCH_SOURCE
#undef XL6_NET_SOCKET_BATCH_METHODS
