const char* xl6StdlibNetAddressListClassSource=
    "class NetAddressList\n    private native point handle\n"
    "    extern \"xl6NetAddressListFromArrayV1\" create(array<NetAddress> addresses)\n"
    "    extern \"xl6NetAddressListResolveV1\" create(string host, int port = 0, int family = 0)\n"
    "    extern \"xl6NetAddressListDestroyV1\" @no_raise destroy()\n"
    "    extern \"xl6NetAddressListCountV1\" @pure @no_raise @nonblocking func count() as int\n"
    "    get length() as int\n        return self.count()\n    end\n"
    "    extern \"xl6NetAddressListAtV1\" @nonblocking func at(int index) as NetAddress\n"
    "    func __item__(int index) as NetAddress\n        return self.at(index)\n    end\n"
    "    extern \"xl6NetAddressListTextV1\" @nonblocking func text(int index) as string\n"
    "    extern \"xl6NetAddressListFamilyV1\" @pure @nonblocking func family(int index) as int\n"
    "    extern \"xl6NetAddressListPortV1\" @pure @nonblocking func port(int index) as int\n"
    "    extern \"xl6NetAddressListCloneV1\" @nonblocking func clone() as NetAddressList\n"
    "    extern \"xl6NetAddressListWithPortV1\" @nonblocking func withPort(int port) as NetAddressList\nend\n";
const char* xl6StdlibNetResolverOptionsSource=
    "class NetResolverOptions\n    private native point handle\n"
    "    extern \"xl6NetResolverConfigCreateV1\" @nonblocking create()\n"
    "    extern \"xl6NetResolverConfigDestroyV1\" @no_raise destroy()\n"
    "    extern \"xl6NetResolverConfigWorkersV1\" @pure @no_raise @nonblocking get workers() as int\n"
    "    extern \"xl6NetResolverConfigWithWorkersV1\" @nonblocking func withWorkers(int count) as NetResolverOptions\n"
#define XL6_NET_RESOLVER_CONFIG_SIZE(Field,Name,Minimum) \
    "    extern \"xl6NetResolverConfig" #Field "V1\" @pure @no_raise @nonblocking get " #Name "() as uint64\n" \
    "    extern \"xl6NetResolverConfigWith" #Field "V1\" @nonblocking func with" #Field "(uint64 value) as NetResolverOptions\n"
#include "src/runtime/xl6_net_resolver_config_fields.inc"
#undef XL6_NET_RESOLVER_CONFIG_SIZE
    "    extern \"xl6NetResolverConfigSuccessTTLMillisV1\" @pure @no_raise @nonblocking get successTtlMillis() as uint64\n"
    "    extern \"xl6NetResolverConfigFailureTTLMillisV1\" @pure @no_raise @nonblocking get failureTtlMillis() as uint64\n"
    "    extern \"xl6NetResolverConfigWithSuccessTTLMillisV1\" @nonblocking func withSuccessTtlMillis(uint64 value) as NetResolverOptions\n"
    "    extern \"xl6NetResolverConfigWithFailureTTLMillisV1\" @nonblocking func withFailureTtlMillis(uint64 value) as NetResolverOptions\nend\n";
const char* xl6StdlibNetResolverStatsClassSource=
    "class NetResolverStats\n    private native point handle\n"
    "    extern \"xl6NetResolverSnapshotDestroyV1\" @no_raise destroy()\n"
#define XL6_NET_RESOLVER_STAT(Field,Name) \
    "    extern \"xl6NetResolverSnapshot" #Field "V1\" @pure @no_raise @nonblocking get " #Name "() as uint64\n"
#include "src/runtime/xl6_net_resolver_stats_fields.inc"
#undef XL6_NET_RESOLVER_STAT
    "end\n";
const char* xl6StdlibNetResolveOperationSource=
    "class NetResolveOperation\n    private native point handle\n"
    "    extern \"xl6NetResolveOperationDestroyV1\" @no_raise destroy()\n"
    "    extern \"xl6NetResolveOperationDoneV1\" @no_raise @nonblocking get done() as bool\n"
    "    extern \"xl6NetResolveOperationCancelV1\" @nonblocking func cancel() as bool\n"
    "    extern \"xl6NetResolveOperationWaitV1\" @nonblocking func waitAsync() as future<bool>\n"
    "    extern \"xl6NetResolveOperationResultV1\" @nonblocking func result(int port = 0) as NetAddressList\nend\n";
const char* xl6StdlibNetResolverSource=
    "class NetResolver\n    private native point handle\n"
    "    extern \"xl6NetResolverCreateDefaultV1\" create()\n"
    "    extern \"xl6NetResolverCreateV1\" create(NetResolverOptions options)\n"
    "    extern \"xl6NetResolverDestroyV1\" @no_raise destroy()\n"
    "    extern \"xl6NetResolverCloseV1\" @nonblocking func close()\n"
    "    extern \"xl6NetResolverWaitV1\" func wait(int timeoutMillis = -1) as bool\n"
    "    extern \"xl6NetResolverRetryV1\" @nonblocking func retryCleanup()\n"
    "    extern \"xl6NetResolverClosedV1\" @nonblocking get closed() as bool\n"
    "    extern \"xl6NetResolverClearV1\" @nonblocking func clearCache()\n"
    "    extern \"xl6NetResolverConfigurationV1\" @nonblocking func configuration() as NetResolverOptions\n"
    "    extern \"xl6NetResolverSnapshotV1\" @nonblocking func snapshot() as NetResolverStats\n"
    "    extern \"xl6NetResolverQueryV1\" @nonblocking func query(string host, int family = 0) as NetResolveOperation\n"
    "    async func resolveAsync(string host, int port = 0, int family = 0) as NetAddressList\n"
    "        if port < 0 or port > 65535 then throw RangeError \"invalid resolver port\" end\n"
    "        auto operation = self.query(host, family)\n"
    "        await operation.waitAsync()\n        return operation.result(port)\n    end\n"
    "    async func resolveAsyncCancel(string host, CancellationToken token, int port = 0, int family = 0) as NetAddressList\n"
    "        if port < 0 or port > 65535 then throw RangeError \"invalid resolver port\" end\n"
    "        token.throwIfCancelled()\n        auto operation = self.query(host, family)\n"
    "        auto registration = token.onCancel(func()\n            operation.cancel()\n        end)\n"
    "        try\n            await operation.waitAsync()\n            return operation.result(port)\n"
    "        finally\n            registration.close()\n        end\n    end\nend\n";
/* Selected once, only when a namespace convenience entry actually needs it.
 * This is a real module global with ordinary ownership, not an untraced TLS
 * singleton or a separate worker pool for each call. */
const char* xl6StdlibNetDefaultResolverSource=
    "NetResolver __xl_std_net_default_resolver = NetResolver()\n";
