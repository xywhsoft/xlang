const char* xl6StdlibUdpConfigClassSource=
    "enum UdpOverflow as int\n    dropNewest = 0\n    dropOldest = 1\n    error = 2\nend\n"
    "enum UdpTruncation as int\n    deliver = 0\n    drop = 1\n    error = 2\nend\n"
    "class UdpConfig\n    private native point handle\n"
    "    extern \"xl6UdpConfigCreateV1\" @nonblocking create()\n"
    "    extern \"xl6UdpConfigDestroyV1\" @no_raise destroy()\n"
#define XL6_UDP_CONFIG_SIZE(Field,Name) "    extern \"xl6UdpConfig" #Field "V1\" @pure @no_raise @nonblocking get " #Name "() as uint64\n"
#define XL6_UDP_CONFIG_UINT(Field,Name) XL6_UDP_CONFIG_SIZE(Field,Name)
#define XL6_UDP_CONFIG_INT(Field,Name) "    extern \"xl6UdpConfig" #Field "V1\" @pure @no_raise @nonblocking get " #Name "() as int\n"
#define XL6_UDP_CONFIG_BOOL(Field,Name) "    extern \"xl6UdpConfig" #Field "V1\" @pure @no_raise @nonblocking get " #Name "() as bool\n"
#include "src/runtime/xl6_udp_config_fields.h"
#undef XL6_UDP_CONFIG_SIZE
#undef XL6_UDP_CONFIG_UINT
#undef XL6_UDP_CONFIG_INT
#undef XL6_UDP_CONFIG_BOOL
    "    extern \"xl6UdpConfigReceiveMetadataV1\" @pure @no_raise @nonblocking get receiveMetadata() as NetDatagramMetadata\n"
    "    extern \"xl6UdpConfigOverflowV1\" @pure @no_raise @nonblocking get overflow() as UdpOverflow\n"
    "    extern \"xl6UdpConfigTruncationV1\" @pure @no_raise @nonblocking get truncation() as UdpTruncation\n"
    "    extern \"xl6UdpConfigErrorOverflowV1\" @pure @no_raise @nonblocking get errorOverflow() as UdpOverflow\n"
    "    extern \"xl6UdpConfigPathMtuV1\" @pure @no_raise @nonblocking get pathMtuMode() as NetPathMtuMode\n"
    "    extern \"xl6UdpConfigWithReceiveV1\" @nonblocking func withReceive(uint64 size, uint64 concurrency, uint64 batch) as UdpConfig\n"
    "    extern \"xl6UdpConfigWithReceiveQueueV1\" @nonblocking func withReceiveQueue(uint64 packets, uint64 bytes, UdpOverflow overflow) as UdpConfig\n"
    "    extern \"xl6UdpConfigWithReceiveMetadataV1\" @nonblocking func withReceiveMetadata(NetDatagramMetadata flags) as UdpConfig\n"
    "    extern \"xl6UdpConfigWithTruncationV1\" @nonblocking func withTruncation(UdpTruncation mode) as UdpConfig\n"
    "    extern \"xl6UdpConfigWithSendQueueV1\" @nonblocking func withSendQueue(uint64 high, uint64 low, uint64 limit, uint64 packets) as UdpConfig\n"
    "    extern \"xl6UdpConfigWithSendConcurrencyV1\" @nonblocking func withSendConcurrency(uint64 count) as UdpConfig\n"
    "    extern \"xl6UdpConfigWithSocketBuffersV1\" @nonblocking func withSocketBuffers(int receive, int send) as UdpConfig\n"
    "    extern \"xl6UdpConfigWithTrafficV1\" @nonblocking func withTraffic(int hopLimit, int trafficClass) as UdpConfig\n"
    "    extern \"xl6UdpConfigWithPathMtuV1\" @nonblocking func withPathMtuMode(NetPathMtuMode mode) as UdpConfig\n"
    "    extern \"xl6UdpConfigWithBindingV1\" @nonblocking func withBinding(bool reuseAddress, bool reusePort, bool exclusiveAddress, bool ipv6Only) as UdpConfig\n"
    "    extern \"xl6UdpConfigWithBroadcastV1\" @nonblocking func withBroadcast(bool enabled) as UdpConfig\n"
    "    extern \"xl6UdpConfigWithErrorQueueV1\" @nonblocking func withErrorQueue(bool enabled, uint64 size, uint64 packets, uint64 bytes, UdpOverflow overflow) as UdpConfig\n"
    "end\n";
