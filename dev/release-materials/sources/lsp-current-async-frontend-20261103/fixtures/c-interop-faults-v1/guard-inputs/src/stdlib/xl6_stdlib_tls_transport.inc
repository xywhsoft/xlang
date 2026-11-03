/* Public transport declarations only. Every method names an already verified
 * resident ABI; no raw importer, native pointer getter or generated adapter.
 * Runtime provider selection remains a property of retained XLIR calls. */
#define XL6_TLS_RECEIVE(Prefix,Name,Type,Argument,Default) \
    "    extern \"" Prefix "V1\" func " Name "(int " Argument " = " Default ", int timeoutMillis = -1) as " Type "\n" \
    "    extern \"" Prefix "CancelV1\" func " Name "Cancel(int " Argument ", CancellationToken cancel, int timeoutMillis = -1) as " Type "\n" \
    "    extern \"" Prefix "UntilV1\" func " Name "Until(int " Argument ", uint64 deadline) as " Type "\n" \
    "    extern \"" Prefix "UntilCancelV1\" func " Name "UntilCancel(int " Argument ", uint64 deadline, CancellationToken cancel) as " Type "\n" \
    "    extern \"" Prefix "AsyncV1\" @nonblocking func " Name "Async(int " Argument " = " Default ") as future<" Type ">\n" \
    "    extern \"" Prefix "AsyncCancelV1\" @nonblocking func " Name "AsyncCancel(int " Argument ", CancellationToken cancel) as future<" Type ">\n" \
    "    extern \"" Prefix "AsyncUntilV1\" @nonblocking func " Name "AsyncUntil(int " Argument ", uint64 deadline) as future<" Type ">\n" \
    "    extern \"" Prefix "AsyncUntilCancelV1\" @nonblocking func " Name "AsyncUntilCancel(int " Argument ", uint64 deadline, CancellationToken cancel) as future<" Type ">\n" \
    "    extern \"" Prefix "AsyncTimeoutV1\" @nonblocking func " Name "AsyncTimeout(int " Argument ", int timeoutMillis) as future<" Type ">\n" \
    "    extern \"" Prefix "AsyncTimeoutCancelV1\" @nonblocking func " Name "AsyncTimeoutCancel(int " Argument ", CancellationToken cancel, int timeoutMillis) as future<" Type ">\n"
#define XL6_TLS_SEND(Prefix,Name,Type) \
    "    extern \"" Prefix "V1\" func " Name "(" Type " input, int size = -1, int timeoutMillis = -1) as int\n" \
    "    extern \"" Prefix "CancelV1\" func " Name "Cancel(" Type " input, int size, CancellationToken cancel, int timeoutMillis = -1) as int\n" \
    "    extern \"" Prefix "UntilV1\" func " Name "Until(" Type " input, int size, uint64 deadline) as int\n" \
    "    extern \"" Prefix "UntilCancelV1\" func " Name "UntilCancel(" Type " input, int size, uint64 deadline, CancellationToken cancel) as int\n" \
    "    extern \"" Prefix "AsyncV1\" @nonblocking func " Name "Async(" Type " input, int size = -1) as future<int>\n" \
    "    extern \"" Prefix "AsyncCancelV1\" @nonblocking func " Name "AsyncCancel(" Type " input, int size, CancellationToken cancel) as future<int>\n" \
    "    extern \"" Prefix "AsyncUntilV1\" @nonblocking func " Name "AsyncUntil(" Type " input, int size, uint64 deadline) as future<int>\n" \
    "    extern \"" Prefix "AsyncUntilCancelV1\" @nonblocking func " Name "AsyncUntilCancel(" Type " input, int size, uint64 deadline, CancellationToken cancel) as future<int>\n" \
    "    extern \"" Prefix "AsyncTimeoutV1\" @nonblocking func " Name "AsyncTimeout(" Type " input, int size, int timeoutMillis) as future<int>\n" \
    "    extern \"" Prefix "AsyncTimeoutCancelV1\" @nonblocking func " Name "AsyncTimeoutCancel(" Type " input, int size, CancellationToken cancel, int timeoutMillis) as future<int>\n"

const char* xl6StdlibTlsStreamClassSource=
    "class TlsStream\n    private native point handle\n"
    "    extern \"xl6TlsStreamCreateDefaultV1\" create(NetEngine engine, NetAddress remote, TlsClientContext context, string serverName, int size = -1)\n"
    "    extern \"xl6TlsStreamCreateV1\" create(NetEngine engine, NetAddress remote, TlsClientContext context, string serverName, int size, string verifyName, int verifySize, TlsStreamOptions stream, TcpStreamOptions tcp, int worker = 0)\n"
    "    extern \"xl6TlsStreamDestroyV1\" @no_raise @nonblocking destroy()\n"
    "    extern \"xl6TlsStreamCloseV1\" @nonblocking func close()\n"
    "    extern \"xl6TlsStreamAbortV1\" @nonblocking func abort()\n"
    "    extern \"xl6TlsStreamStateV1\" @nonblocking get state() as int\n"
    "    extern \"xl6TlsStreamAvailableV1\" @nonblocking get available() as uint64\n"
    "    extern \"xl6TlsStreamPendingV1\" @nonblocking get pending() as uint64\n"
    "    extern \"xl6TlsStreamLocalV1\" @nonblocking func local() as NetAddress\n"
    "    extern \"xl6TlsStreamRemoteV1\" @nonblocking func remote() as NetAddress\n"
    "    extern \"xl6TlsStreamSnapshotV1\" func info(int timeoutMillis = -1) as TlsConnectionInfo\n"
    "    extern \"xl6TlsStreamSnapshotAsyncV1\" @nonblocking func infoAsync() as future<TlsConnectionInfo>\n"
    XL6_TLS_RECEIVE("xl6TlsStreamRecv","recv","bytes","maximum","65536")
    XL6_TLS_RECEIVE("xl6TlsStreamRecvOwned","recvOwned","NetBytes","maximum","65536")
    XL6_TLS_RECEIVE("xl6TlsStreamWait","wait","bool","condition","0")
    XL6_TLS_SEND("xl6TlsStreamSendText","send","string")
    XL6_TLS_SEND("xl6TlsStreamSendBytes","send","bytes")
    XL6_TLS_SEND("xl6TlsStreamSendBuffer","send","Buffer")
    XL6_TLS_SEND("xl6TlsStreamSendVectorText","sendVector","array<string>")
    XL6_TLS_SEND("xl6TlsStreamSendVectorBytes","sendVector","array<bytes>")
    XL6_TLS_SEND("xl6TlsStreamSendVectorBuffer","sendVector","array<Buffer>")
    "    extern \"xl6TlsStreamPeekV1\" func peek(int maximum = -1, int timeoutMillis = -1) as bytes\n"
    "    extern \"xl6TlsStreamPeekAsyncV1\" @nonblocking func peekAsync(int maximum = -1) as future<bytes>\n"
    "    extern \"xl6TlsStreamConsumeV1\" func consume(int count, int timeoutMillis = -1) as int\n"
    "    extern \"xl6TlsStreamConsumeAsyncV1\" @nonblocking func consumeAsync(int count) as future<int>\n"
    "    extern \"xl6TlsStreamSendBoundV1\" func sendBound(int size, int timeoutMillis = -1) as int\n"
    "    extern \"xl6TlsStreamSendBoundAsyncV1\" @nonblocking func sendBoundAsync(int size) as future<int>\n"
    "    extern \"xl6TlsStreamProtocolV1\" func protocol(int timeoutMillis = -1) as bytes\n"
    "    extern \"xl6TlsStreamProtocolAsyncV1\" @nonblocking func protocolAsync() as future<bytes>\n"
    "    extern \"xl6TlsStreamKeyUpdateV1\" func keyUpdate(bool requestPeer = false, int timeoutMillis = -1) as TlsResult\n"
    "    extern \"xl6TlsStreamKeyUpdateAsyncV1\" @nonblocking func keyUpdateAsync(bool requestPeer = false) as future<TlsResult>\n"
    "    extern \"xl6TlsStreamTicketCountV1\" func ticketCount(int timeoutMillis = -1) as int\n"
    "    extern \"xl6TlsStreamTicketCountAsyncV1\" @nonblocking func ticketCountAsync() as future<int>\n"
    "    extern \"xl6TlsStreamDroppedTicketsV1\" func droppedTickets(int timeoutMillis = -1) as uint64\n"
    "    extern \"xl6TlsStreamDroppedTicketsAsyncV1\" @nonblocking func droppedTicketsAsync() as future<uint64>\n"
    "    extern \"xl6TlsStreamTryTakeTicketV1\" func tryTakeTicket() as TlsTicketResult\n"
    "    extern \"xl6TlsStreamTryTakeTicketAsyncV1\" @nonblocking func tryTakeTicketAsync() as future<TlsTicketResult>\n"
    "    extern \"xl6TlsStreamIssueNewTicketV1\" func issueTicket() as TlsTicketResult\n"
    "    extern \"xl6TlsStreamIssueTicketV1\" func issueTicket(bytes ticket, int lifetimeSeconds, int size = -1) as TlsTicketResult\n"
    "    extern \"xl6TlsStreamIssueNewTicketAsyncV1\" @nonblocking func issueTicketAsync() as future<TlsTicketResult>\n"
    "    extern \"xl6TlsStreamIssueTicketAsyncV1\" @nonblocking func issueTicketAsync(bytes ticket, int lifetimeSeconds, int size = -1) as future<TlsTicketResult>\n"
    "    func waitOpen(int timeoutMillis = -1) as bool\n        return self.wait(0, timeoutMillis)\n    end\n"
    "    func waitReadable(int timeoutMillis = -1) as bool\n        return self.wait(1, timeoutMillis)\n    end\n"
    "    func waitWritable(int timeoutMillis = -1) as bool\n        return self.wait(2, timeoutMillis)\n    end\n"
    "    func drain(int timeoutMillis = -1) as bool\n        return self.wait(3, timeoutMillis)\n    end\n"
    "    func waitEnd(int timeoutMillis = -1) as bool\n        return self.wait(4, timeoutMillis)\n    end\n"
    "    func waitClose(int timeoutMillis = -1) as bool\n        return self.wait(5, timeoutMillis)\n    end\n"
    "    func openAsync() as future<bool>\n        return self.waitAsync(0)\n    end\n"
    "    func readableAsync() as future<bool>\n        return self.waitAsync(1)\n    end\n"
    "    func writableAsync() as future<bool>\n        return self.waitAsync(2)\n    end\n"
    "    func drainAsync() as future<bool>\n        return self.waitAsync(3)\n    end\n"
    "    func endAsync() as future<bool>\n        return self.waitAsync(4)\n    end\n"
    "    func closeAsync() as future<bool>\n        return self.waitAsync(5)\n    end\n";

/* Static factories use the same exact class-result recipe as accept. No
 * language-visible pending object or native-handle escape is necessary. */
/* Default is a suffix of the physical name, not of the public method. Keep
 * complete spelling explicit to avoid inventing alternate native symbols. */
#define XL6_TLS_DIAL_FAMILY(Prefix,Suffix,Name,Extra,Options) \
    "    static extern \"" Prefix Suffix "V1\" func " Name "(NetEngine engine, NetResolver resolver, " Extra "string host, int port, TlsClientContext context, " Options "int size = -1, int timeoutMillis = -1) as TlsStream\n" \
    "    static extern \"" Prefix "Cancel" Suffix "V1\" func " Name "Cancel(NetEngine engine, NetResolver resolver, " Extra "string host, int port, TlsClientContext context, " Options "int size, CancellationToken cancel, int timeoutMillis = -1) as TlsStream\n" \
    "    static extern \"" Prefix "Until" Suffix "V1\" func " Name "Until(NetEngine engine, NetResolver resolver, " Extra "string host, int port, TlsClientContext context, " Options "int size, uint64 deadline) as TlsStream\n" \
    "    static extern \"" Prefix "UntilCancel" Suffix "V1\" func " Name "UntilCancel(NetEngine engine, NetResolver resolver, " Extra "string host, int port, TlsClientContext context, " Options "int size, uint64 deadline, CancellationToken cancel) as TlsStream\n" \
    "    static extern \"" Prefix "Async" Suffix "V1\" @nonblocking func " Name "Async(NetEngine engine, NetResolver resolver, " Extra "string host, int port, TlsClientContext context, " Options "int size = -1) as future<TlsStream>\n" \
    "    static extern \"" Prefix "AsyncCancel" Suffix "V1\" @nonblocking func " Name "AsyncCancel(NetEngine engine, NetResolver resolver, " Extra "string host, int port, TlsClientContext context, " Options "int size, CancellationToken cancel) as future<TlsStream>\n" \
    "    static extern \"" Prefix "AsyncUntil" Suffix "V1\" @nonblocking func " Name "AsyncUntil(NetEngine engine, NetResolver resolver, " Extra "string host, int port, TlsClientContext context, " Options "int size, uint64 deadline) as future<TlsStream>\n" \
    "    static extern \"" Prefix "AsyncUntilCancel" Suffix "V1\" @nonblocking func " Name "AsyncUntilCancel(NetEngine engine, NetResolver resolver, " Extra "string host, int port, TlsClientContext context, " Options "int size, uint64 deadline, CancellationToken cancel) as future<TlsStream>\n" \
    "    static extern \"" Prefix "AsyncTimeout" Suffix "V1\" @nonblocking func " Name "AsyncTimeout(NetEngine engine, NetResolver resolver, " Extra "string host, int port, TlsClientContext context, " Options "int size, int timeoutMillis) as future<TlsStream>\n" \
    "    static extern \"" Prefix "AsyncTimeoutCancel" Suffix "V1\" @nonblocking func " Name "AsyncTimeoutCancel(NetEngine engine, NetResolver resolver, " Extra "string host, int port, TlsClientContext context, " Options "int size, CancellationToken cancel, int timeoutMillis) as future<TlsStream>\n"

const char* xl6StdlibTlsStreamDialSource=
    XL6_TLS_DIAL_FAMILY("xl6TlsDial","Default","dial","","")
    XL6_TLS_DIAL_FAMILY("xl6TlsDial","","dial","","TlsDialOptions options, ")
    XL6_TLS_DIAL_FAMILY("xl6TlsProxyDial","Default","dialProxy","NetProxy proxy, ","")
    XL6_TLS_DIAL_FAMILY("xl6TlsProxyDial","","dialProxy","NetProxy proxy, ","TlsDialOptions options, ");

const char* xl6StdlibTlsServerClassSource=
    "class TlsServer\n    private native point handle\n"
    "    extern \"xl6TlsListenerCreateDefaultV1\" create(NetEngine engine, TcpListenOptions listen, TlsServerContext context)\n"
    "    extern \"xl6TlsListenerCreateV1\" create(NetEngine engine, TcpListenOptions listen, TlsServerContext context, TlsStreamOptions stream, int accepts = 128, int handshakes = 64)\n"
    "    extern \"xl6TlsListenerDestroyV1\" @no_raise @nonblocking destroy()\n"
    "    extern \"xl6TlsListenerCloseV1\" @nonblocking func close()\n"
    "    extern \"xl6TlsListenerStateV1\" @nonblocking get state() as int\n"
    "    extern \"xl6TlsListenerLocalV1\" @nonblocking func local() as NetAddress\n"
    "    extern \"xl6TlsListenerSnapshotV1\" @nonblocking func stats() as TlsServerStats\n"
    "    extern \"xl6TlsListenerAcceptV1\" func accept(int timeoutMillis = -1) as TlsStream\n"
    "    extern \"xl6TlsListenerAcceptCancelV1\" func acceptCancel(CancellationToken cancel, int timeoutMillis = -1) as TlsStream\n"
    "    extern \"xl6TlsListenerAcceptUntilV1\" func acceptUntil(uint64 deadline) as TlsStream\n"
    "    extern \"xl6TlsListenerAcceptUntilCancelV1\" func acceptUntilCancel(uint64 deadline, CancellationToken cancel) as TlsStream\n"
    "    extern \"xl6TlsListenerAcceptAsyncV1\" @nonblocking func acceptAsync() as future<TlsStream>\n"
    "    extern \"xl6TlsListenerAcceptAsyncCancelV1\" @nonblocking func acceptAsyncCancel(CancellationToken cancel) as future<TlsStream>\n"
    "    extern \"xl6TlsListenerAcceptAsyncUntilV1\" @nonblocking func acceptAsyncUntil(uint64 deadline) as future<TlsStream>\n"
    "    extern \"xl6TlsListenerAcceptAsyncUntilCancelV1\" @nonblocking func acceptAsyncUntilCancel(uint64 deadline, CancellationToken cancel) as future<TlsStream>\n"
    "    extern \"xl6TlsListenerAcceptAsyncTimeoutV1\" @nonblocking func acceptAsyncTimeout(int timeoutMillis) as future<TlsStream>\n"
    "    extern \"xl6TlsListenerAcceptAsyncTimeoutCancelV1\" @nonblocking func acceptAsyncTimeoutCancel(CancellationToken cancel, int timeoutMillis) as future<TlsStream>\n"
    "    get port() as int\n        return self.local().port\n    end\n"
    "end\n";
#undef XL6_TLS_RECEIVE
#undef XL6_TLS_SEND
#undef XL6_TLS_DIAL_FAMILY
