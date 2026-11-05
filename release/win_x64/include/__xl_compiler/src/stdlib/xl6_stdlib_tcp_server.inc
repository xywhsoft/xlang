const char* xl6StdlibTcpServerStateSource=
    "enum TcpServerState as int\n    starting = 0\n    open = 1\n    closing = 2\n    closed = 3\nend\n";
const char* xl6StdlibTcpServerOptionsSource=
    "enum TcpServerMode as int\n    shared = 0\n    reusePort = 1\nend\n"
    "class TcpServerOptions\n    private native point handle\n"
    "    extern \"xl6TcpServerOptionsCreateV1\" @nonblocking create()\n"
    "    extern \"xl6TcpServerOptionsCreateListenV1\" @nonblocking create(TcpListenOptions endpoint)\n"
    "    extern \"xl6TcpServerOptionsDestroyV1\" @no_raise destroy()\n"
    "    extern \"xl6TcpServerOptionsEndpointCountV1\" @pure @no_raise @nonblocking get endpointCount() as uint64\n"
    "    extern \"xl6TcpServerOptionsModeV1\" @pure @no_raise @nonblocking get mode() as TcpServerMode\n"
    "    extern \"xl6TcpServerOptionsSharedPortV1\" @pure @no_raise @nonblocking get sharedPort() as bool\n"
    "    extern \"xl6TcpServerOptionsAcceptQueueLimitV1\" @pure @no_raise @nonblocking get acceptQueueLimit() as int\n"
    "    extern \"xl6TcpServerOptionsEndpointV1\" @nonblocking func endpoint(int index) as TcpListenOptions\n"
    "    extern \"xl6TcpServerOptionsWithEndpointV1\" @nonblocking func withEndpoint(int index, TcpListenOptions endpoint) as TcpServerOptions\n"
    "    extern \"xl6TcpServerOptionsWithAddedEndpointV1\" @nonblocking func withAddedEndpoint(TcpListenOptions endpoint) as TcpServerOptions\n"
    "    extern \"xl6TcpServerOptionsWithoutEndpointV1\" @nonblocking func withoutEndpoint(int index) as TcpServerOptions\n"
    "    extern \"xl6TcpServerOptionsWithModeV1\" @nonblocking func withMode(TcpServerMode mode) as TcpServerOptions\n"
    "    extern \"xl6TcpServerOptionsWithSharedPortV1\" @nonblocking func withSharedPort(bool enabled) as TcpServerOptions\n"
    "    extern \"xl6TcpServerOptionsWithAcceptQueueLimitV1\" @nonblocking func withAcceptQueueLimit(int limit) as TcpServerOptions\nend\n";
const char* xl6StdlibTcpServerStatsSource=
    "class TcpServerStats\n    private native point handle\n"
    "    extern \"xl6TcpServerStatsDestroyV1\" @no_raise destroy()\n"
    "    extern \"xl6TcpServerStatsStateV1\" @pure @no_raise @nonblocking get state() as TcpServerState\n"
#define XL6_TCP_SERVER_UINT(Field,Name) "    extern \"xl6TcpServerStats" #Field "V1\" @pure @no_raise @nonblocking get " #Name "() as uint64\n"
#include "src/runtime/xl6_tcp_server_stats_fields.inc"
#undef XL6_TCP_SERVER_UINT
    "end\n";
const char* xl6StdlibTcpServerClassSource=
    "class TcpServer\n    private native point handle\n"
    "    extern \"xl6TcpServerCreateV1\" create(NetEngine engine, TcpServerOptions options)\n"
    "    extern \"xl6TcpServerDestroyV1\" @no_raise destroy()\n"
    "    extern \"xl6TcpServerCloseV1\" @nonblocking func close()\n"
    "    extern \"xl6TcpServerStateV1\" @nonblocking get state() as TcpServerState\n"
    "    extern \"xl6TcpServerEndpointCountV1\" @nonblocking get endpointCount() as uint64\n"
    "    extern \"xl6TcpServerListenerCountV1\" @nonblocking get listenerCount() as uint64\n"
    "    extern \"xl6TcpServerLocalV1\" @nonblocking func localAddress(int endpoint = 0) as NetAddress\n"
    "    extern \"xl6TcpServerSnapshotV1\" @nonblocking func snapshot() as TcpServerStats\n"
    "    extern \"xl6TcpServerTryAcceptV1\" @nonblocking func tryAccept() as TcpStream?\n"
    "    extern \"xl6TcpServerAcceptV1\" func accept(int timeoutMillis = -1) as TcpStream\n"
    "    extern \"xl6TcpServerAcceptCancelV1\" func acceptCancel(CancellationToken cancel, int timeoutMillis = -1) as TcpStream\n"
    "    extern \"xl6TcpServerAcceptUntilV1\" func acceptUntil(uint64 deadline) as TcpStream\n"
    "    extern \"xl6TcpServerAcceptUntilCancelV1\" func acceptUntilCancel(uint64 deadline, CancellationToken cancel) as TcpStream\n"
    "    private extern \"xl6TcpServerPrepareAcceptV1\" @nonblocking func prepareAcceptNative() as TcpStream\n"
    "    private extern \"xl6TcpServerAcceptStartV1\" @nonblocking func startAcceptNative(TcpStream value) as future<TcpStream>\n"
    "    private extern \"xl6TcpServerAcceptStartCancelV1\" @nonblocking func startAcceptCancelNative(TcpStream value, CancellationToken cancel) as future<TcpStream>\n"
    "    private extern \"xl6TcpServerAcceptStartUntilV1\" @nonblocking func startAcceptUntilNative(TcpStream value, uint64 deadline) as future<TcpStream>\n"
    "    private extern \"xl6TcpServerAcceptStartUntilCancelV1\" @nonblocking func startAcceptUntilCancelNative(TcpStream value, uint64 deadline, CancellationToken cancel) as future<TcpStream>\n"
    "    func acceptAsync() as future<TcpStream>\n"
    "        TcpStream pending = self.prepareAcceptNative()\n        return self.startAcceptNative(pending)\n    end\n"
    "    func acceptAsyncCancel(CancellationToken cancel) as future<TcpStream>\n"
    "        TcpStream pending = self.prepareAcceptNative()\n        return self.startAcceptCancelNative(pending, cancel)\n    end\n"
    "    func acceptAsyncUntil(uint64 deadline) as future<TcpStream>\n"
    "        TcpStream pending = self.prepareAcceptNative()\n        return self.startAcceptUntilNative(pending, deadline)\n    end\n"
    "    func acceptAsyncUntilCancel(uint64 deadline, CancellationToken cancel) as future<TcpStream>\n"
    "        TcpStream pending = self.prepareAcceptNative()\n        return self.startAcceptUntilCancelNative(pending, deadline, cancel)\n    end\n"
    "end\n";
static const char xl6StdTcpServerBody[]=
    "func __xl_std_tcp_server(NetEngine engine, TcpServerOptions options) as TcpServer\n"
    "    return TcpServer(engine, options)\nend\n";
