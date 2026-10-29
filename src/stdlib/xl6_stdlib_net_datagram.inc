/* Source declarations only. Native POD values never expose point constructors. */
#include "xl6_stdlib_net_datagram_error.inc"
#define XL6_NET_DATAGRAM_SOURCE \
    XL6_NET_DATAGRAM_ERROR_SOURCE \
    "enum NetDatagramMetadata as int\n    none = 0\n    destination = 1\n    interfaceIndex = 2\n" \
    "    hopLimit = 4\n    trafficClass = 8\n    segmentSize = 16\n    truncated = 1073741824\nend\n" \
    "enum NetDatagramControlFlags as int\n    none = 0\n    source = 1\n    interfaceIndex = 2\n" \
    "    hopLimit = 4\n    trafficClass = 8\n    segmentSize = 16\nend\n" \
    "enum NetDatagramCapabilities as int\n    none = 0\n    pathMtuMode = 1\n    pathMtuQuery = 2\n" \
    "    errorQueue = 4\n    segmentSend = 8\n    segmentReceive = 16\nend\n" \
    "class NetDatagramInfo\n    private native point handle\n" \
    "    extern \"xl6NetDatagramInfoCreateV1\" @nonblocking create()\n" \
    "    extern \"xl6NetDatagramInfoDestroyV1\" @no_raise destroy()\n" \
    "    extern \"xl6NetDatagramInfoClearV1\" @no_raise @nonblocking func clear()\n" \
    "    extern \"xl6NetDatagramInfoValidV1\" @no_raise @nonblocking get hasPacket() as bool\n" \
    "    extern \"xl6NetDatagramInfoFlagsV1\" @no_raise @nonblocking get metadata() as NetDatagramMetadata\n" \
    "    extern \"xl6NetDatagramInfoRemoteV1\" @nonblocking get remote() as NetAddress\n" \
    "    extern \"xl6NetDatagramInfoDestinationV1\" @nonblocking get destination() as NetAddress\n" \
    "    extern \"xl6NetDatagramInfoInterfaceV1\" @nonblocking get interfaceIndex() as int\n" \
    "    extern \"xl6NetDatagramInfoHopLimitV1\" @nonblocking get hopLimit() as int\n" \
    "    extern \"xl6NetDatagramInfoTrafficClassV1\" @nonblocking get trafficClass() as int\n" \
    "    extern \"xl6NetDatagramInfoSegmentSizeV1\" @nonblocking get segmentSize() as int\nend\n" \
    "class NetDatagramControl\n    private native point handle\n" \
    "    extern \"xl6NetDatagramControlCreateV1\" @nonblocking create()\n" \
    "    extern \"xl6NetDatagramControlDestroyV1\" @no_raise destroy()\n" \
    "    extern \"xl6NetDatagramControlFlagsV1\" @pure @no_raise @nonblocking get flags() as NetDatagramControlFlags\n" \
    "    extern \"xl6NetDatagramControlSourceV1\" @nonblocking func withSource(NetAddress source) as NetDatagramControl\n" \
    "    extern \"xl6NetDatagramControlInterfaceV1\" @nonblocking func withInterface(int index) as NetDatagramControl\n" \
    "    extern \"xl6NetDatagramControlHopLimitV1\" @nonblocking func withHopLimit(int limit) as NetDatagramControl\n" \
    "    extern \"xl6NetDatagramControlTrafficClassV1\" @nonblocking func withTrafficClass(int value) as NetDatagramControl\n" \
    "    extern \"xl6NetDatagramControlSegmentSizeV1\" @nonblocking func withSegmentSize(int size) as NetDatagramControl\n" \
    "    extern \"xl6NetDatagramControlWithoutV1\" @nonblocking func without(NetDatagramControlFlags flags) as NetDatagramControl\nend\n"
#define XL6_NET_SOCKET_DATAGRAM_METHODS \
    XL6_NET_SOCKET_DATAGRAM_ERROR_METHODS \
    "    extern \"xl6NetSocketMetadataAvailableV1\" func metadataAvailable() as NetDatagramMetadata\n" \
    "    extern \"xl6NetSocketMetadataEnabledV1\" func metadataEnabled() as NetDatagramMetadata\n" \
    "    extern \"xl6NetSocketControlAvailableV1\" func controlAvailable() as NetDatagramControlFlags\n" \
    "    extern \"xl6NetSocketDatagramCapabilitiesV1\" func datagramCapabilities() as NetDatagramCapabilities\n" \
    "    extern \"xl6NetSocketMetadataSetV1\" func enableMetadata(NetDatagramMetadata flags)\n" \
    "    extern \"xl6NetSocketMulticastJoinV1\" func joinMulticast(NetAddress group)\n" \
    "    extern \"xl6NetSocketMulticastJoinOnV1\" func joinMulticast(NetAddress group, NetAddress interfaceAddress)\n" \
    "    extern \"xl6NetSocketMulticastLeaveV1\" func leaveMulticast(NetAddress group)\n" \
    "    extern \"xl6NetSocketMulticastLeaveOnV1\" func leaveMulticast(NetAddress group, NetAddress interfaceAddress)\n" \
    "    extern \"xl6NetSocketMulticastLoopV1\" func setMulticastLoop(bool enabled)\n" \
    "    extern \"xl6NetSocketMulticastHopLimitV1\" func setMulticastHopLimit(int limit)\n" \
    "    extern \"xl6NetSocketMulticastInterfaceV1\" func setMulticastInterface(NetAddress interfaceAddress)\n" \
    "    extern \"xl6NetSocketMulticastInterfaceDefaultV1\" func setMulticastInterface()\n" \
    "    extern \"xl6NetSocketReceiveFromIntoV1\" func receiveFromInto(Buffer output, NetDatagramInfo info, int maxBytes = 65536) as (NetStatus, uint64)\n" \
    "    extern \"xl6NetSocketSendMessageTextV1\" func sendMessage(string data, NetAddress target, NetDatagramControl control, int size = -1) as (NetStatus, uint64)\n" \
    "    extern \"xl6NetSocketSendMessageBytesV1\" func sendMessage(bytes data, NetAddress target, NetDatagramControl control, int size = -1) as (NetStatus, uint64)\n" \
    "    extern \"xl6NetSocketSendMessageBufferV1\" func sendMessage(Buffer data, NetAddress target, NetDatagramControl control, int size = -1) as (NetStatus, uint64)\n" \
    "    extern \"xl6NetSocketSendConnectedMessageTextV1\" func sendMessage(string data, NetDatagramControl control, int size = -1) as (NetStatus, uint64)\n" \
    "    extern \"xl6NetSocketSendConnectedMessageBytesV1\" func sendMessage(bytes data, NetDatagramControl control, int size = -1) as (NetStatus, uint64)\n" \
    "    extern \"xl6NetSocketSendConnectedMessageBufferV1\" func sendMessage(Buffer data, NetDatagramControl control, int size = -1) as (NetStatus, uint64)\n"
