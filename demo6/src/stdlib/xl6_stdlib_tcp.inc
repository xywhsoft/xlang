const char* xl6StdlibTcpStreamStateSource=
    "enum TcpStreamState as int\n    connecting = 0\n    open = 1\n    closing = 2\n    closed = 3\nend\n";
const char* xl6StdlibTcpListenerStateSource=
    "enum TcpListenerState as int\n    open = 0\n    closing = 1\n    closed = 2\nend\n";
const char* xl6StdlibTcpWaitSource=
    "enum TcpWait as int\n    open = 0\n    readable = 1\n    writable = 2\n    drain = 3\n    close = 4\nend\n";
const char* xl6StdlibTcpStreamStatsClassSource=
    "class TcpStreamStats\n    private native point handle\n"
    "    extern \"xl6TcpStreamStatsDestroyV1\" @no_raise destroy()\n"
    "    extern \"xl6TcpStreamStatsStateV1\" @pure @no_raise @nonblocking get state() as TcpStreamState\n"
#define XL6_TCP_STREAM_UINT(Field,Name) "    extern \"xl6TcpStreamStats" #Field "V1\" @pure @no_raise @nonblocking get " #Name "() as uint64\n"
#define XL6_TCP_STREAM_BOOL(Field,Name) "    extern \"xl6TcpStreamStats" #Field "V1\" @pure @no_raise @nonblocking get " #Name "() as bool\n"
#include "src/runtime/xl6_tcp_stats_fields.inc"
#undef XL6_TCP_STREAM_UINT
#undef XL6_TCP_STREAM_BOOL
    "end\n";
const char* xl6StdlibTcpListenerStatsClassSource=
    "class TcpListenerStats\n    private native point handle\n"
    "    extern \"xl6TcpListenerStatsDestroyV1\" @no_raise destroy()\n"
    "    extern \"xl6TcpListenerStatsStateV1\" @pure @no_raise @nonblocking get state() as TcpListenerState\n"
#define XL6_TCP_LISTENER_UINT(Field,Name) "    extern \"xl6TcpListenerStats" #Field "V1\" @pure @no_raise @nonblocking get " #Name "() as uint64\n"
#include "src/runtime/xl6_tcp_stats_fields.inc"
#undef XL6_TCP_LISTENER_UINT
    "end\n";
const char* xl6StdlibTcpStreamClassSource=
    "class TcpStream\n    private native point handle\n"
    "    extern \"xl6TcpStreamCreateDefaultV1\" create(NetEngine engine, NetAddress remote)\n"
    "    extern \"xl6TcpStreamCreateV1\" create(NetEngine engine, NetAddress remote, TcpStreamOptions options, int worker = 0)\n"
    "    extern \"xl6TcpStreamDestroyV1\" @no_raise destroy()\n"
    "    extern \"xl6TcpStreamCloseV1\" @nonblocking func close()\n"
    "    extern \"xl6TcpStreamAbortV1\" @nonblocking func abort()\n"
    "    extern \"xl6TcpStreamStateV1\" @nonblocking get state() as TcpStreamState\n"
    "    extern \"xl6TcpStreamAvailableV1\" @nonblocking get available() as uint64\n"
    "    extern \"xl6TcpStreamPendingSendV1\" @nonblocking get pendingSend() as uint64\n"
    "    extern \"xl6TcpStreamWriteLimitV1\" @nonblocking get writeLimit() as uint64\n"
    "    extern \"xl6TcpStreamWritableV1\" @nonblocking get writable() as uint64\n"
    "    extern \"xl6TcpStreamSendTextV1\" @nonblocking func send(string data, int size = -1) as NetStatus\n"
    "    extern \"xl6TcpStreamSendBytesV1\" @nonblocking func send(bytes data, int size = -1) as NetStatus\n"
    "    extern \"xl6TcpStreamSendBufferV1\" @nonblocking func send(Buffer data, int size = -1) as NetStatus\n"
    "    extern \"xl6TcpStreamShutdownWriteV1\" @nonblocking func shutdownWrite()\n"
    "    extern \"xl6TcpStreamPauseReadV1\" @nonblocking func pauseRead()\n"
    "    extern \"xl6TcpStreamResumeReadV1\" @nonblocking func resumeRead()\n"
    "    extern \"xl6TcpStreamLocalV1\" @nonblocking func localAddress() as NetAddress\n"
    "    extern \"xl6TcpStreamRemoteV1\" @nonblocking func remoteAddress() as NetAddress\n"
    "    extern \"xl6TcpStreamRecvV1\" func recv(int maxBytes = 65536, int timeoutMillis = -1) as bytes\n"
    "    extern \"xl6TcpStreamRecvAsyncV1\" @nonblocking func recvAsync(int maxBytes = 65536) as future<bytes>\n"
    "    extern \"xl6TcpStreamWaitV1\" func wait(TcpWait condition, int timeoutMillis = -1) as NetStatus\n"
    "    extern \"xl6TcpStreamWaitAsyncV1\" @nonblocking func waitAsync(TcpWait condition) as future<bool>\n"
    "    extern \"xl6TcpStreamWaitAvailableAsyncV1\" @nonblocking func waitAvailableAsync(int minimum = 1) as future<bool>\n"
    "    private extern \"xl6TcpConnectedStartV1\" @nonblocking func connectedNative(TcpStream value) as future<TcpStream>\n"
    "    func connectedAsync() as future<TcpStream>\n        return self.connectedNative(self)\n    end\n"
    "    extern \"xl6TcpStreamSnapshotV1\" @nonblocking func snapshot() as TcpStreamStats\n"
    "end\n";
const char* xl6StdlibTcpListenerClassSource=
    "class TcpListener\n    private native point handle\n"
    "    extern \"xl6TcpListenerCreateV1\" create(NetEngine engine, TcpListenOptions options)\n"
    "    extern \"xl6TcpListenerDestroyV1\" @no_raise destroy()\n"
    "    extern \"xl6TcpListenerCloseV1\" @nonblocking func close()\n"
    "    extern \"xl6TcpListenerStateV1\" @nonblocking get state() as TcpListenerState\n"
    "    extern \"xl6TcpListenerLocalV1\" @nonblocking func localAddress() as NetAddress\n"
    "    extern \"xl6TcpListenerSnapshotV1\" @nonblocking func snapshot() as TcpListenerStats\n"
    "    extern \"xl6TcpListenerAcceptV1\" func accept(int timeoutMillis = -1) as TcpStream\n"
    "    private extern \"xl6TcpPrepareAcceptV1\" @nonblocking func prepareAcceptNative() as TcpStream\n"
    "    private extern \"xl6TcpAcceptStartV1\" @nonblocking func startAcceptNative(TcpStream value) as future<TcpStream>\n"
    "    func acceptAsync() as future<TcpStream>\n"
    "        TcpStream pending = self.prepareAcceptNative()\n"
    "        return self.startAcceptNative(pending)\n    end\n"
    "end\n";
