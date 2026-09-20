/* 从公开类和实际 extern 收集最小网络运行时闭包。 */
static void xl6CgenNetCollectNeeds(
    XL6_CgenContext* ctx,
    XL6_CgenNetNeeds* needs,
    int hasTlsDialRuntime,
    int hasTlsServerRuntime
)
{
    XL6_AstNode* root;

    memset(needs, 0, sizeof(*needs));
    root = ctx->root;
    needs->address = xl6CgenFindClass(ctx, "NetAddress") != NULL ||
        xl6CgenFindClass(ctx, "NetAddressList") != NULL;
    needs->interfaces = xl6CgenFindClass(ctx, "NetInterfaceList") != NULL ||
        xl6CgenFindClass(ctx, "NetInterface") != NULL ||
        xl6CgenFindClass(ctx, "NetInterfaceAddress") != NULL ||
        xl6CgenNodeHasExternSymbol(root, "xl6_xrt_net_interface_index") ||
        xl6CgenNodeHasExternSymbol(root, "xl6_xrt_net_interface_name_by_index");
    needs->bytes = xl6CgenFindClass(ctx, "NetBytes") != NULL;
    needs->engineStats = xl6CgenFindClass(ctx, "NetEngineStats") != NULL;
    needs->resolverStats = xl6CgenFindClass(ctx, "NetResolverStats") != NULL;
    needs->proxy = xl6CgenFindClass(ctx, "NetProxy") != NULL;
    needs->engineObject = xl6CgenFindClass(ctx, "NetEngine") != NULL;
    needs->tcpStream = xl6CgenFindClass(ctx, "TcpStream") != NULL;
    needs->tcpFile = xl6CgenNodeHasExternSymbol(
        root,
        "xl6_xrt_tcp_stream_send_file_result"
    ) || xl6CgenNodeHasExternSymbol(
        root,
        "xl6_xrt_tcp_stream_send_file_handle_result"
    );
    needs->tcpStream = needs->tcpStream || needs->tcpFile;
    needs->tcpServer = xl6CgenFindClass(ctx, "TcpServer") != NULL;
    needs->tcpConnectConfig = xl6CgenFindClass(ctx, "TcpConnectConfig") != NULL;
    needs->tcpListenConfig = xl6CgenFindClass(ctx, "TcpListenConfig") != NULL;
    needs->udp = xl6CgenFindClass(ctx, "UdpSocket") != NULL;
    needs->udpConfig = xl6CgenFindClass(ctx, "UdpConfig") != NULL;
    needs->udpBatch = xl6CgenFindClass(ctx, "UdpBatch") != NULL;
    needs->tlsStream = xl6CgenFindClass(ctx, "TlsStream") != NULL;
    needs->tlsServer = hasTlsServerRuntime ||
        xl6CgenFindClass(ctx, "TlsServer") != NULL;
    needs->tlsDial = hasTlsDialRuntime;
    needs->resolverAsync = xl6CgenNodeHasExternSymbol(
        root,
        "xl6_xrt_net_resolve_async"
    );
    needs->tcpDial = xl6CgenNodeHasExternSymbol(root, "xl6_xrt_tcp_connect") ||
        xl6CgenNodeHasExternSymbol(root, "xl6_xrt_tcp_connect_async");
    needs->tcpListen = xl6CgenNodeHasExternSymbol(root, "xl6_xrt_tcp_listen");
    /*
        当前 TCP/UDP 输出段仍以一组桥接函数为单位。配置类型同样会
        带出这些函数，因此先闭包其返回类型与 future 映射器，避免
        生成半套运行时后引用未输出的 xl6_net_map_* 符号。
    */
    /*
        tcp.connect/tcp.listen 是标准库注入的封装调用，不会以用户 AST 的
        extern 节点出现。TcpStream/TcpServer 一旦进入编译单元，便必须同时
        保留对应桥接入口；否则类型已生成而调用实现被裁掉，直到 TCC 链接
        阶段才暴露 unresolved reference。
    */
    needs->tcpDial = needs->tcpDial || needs->tcpStream;
    needs->tcpListen = needs->tcpListen || needs->tcpServer;
    needs->tcpStream = needs->tcpStream || needs->tcpDial ||
        needs->tcpServer || needs->tcpListenConfig;
    needs->udp = needs->udp || needs->udpConfig;
    needs->tcpVectors = needs->tcpStream;
    needs->bytes = needs->bytes || needs->tcpStream || needs->tlsStream;
    needs->address = needs->address || needs->tcpStream || needs->udp;
    needs->engine = needs->engineObject || needs->engineStats ||
        needs->tcpStream || needs->tcpServer || needs->tcpConnectConfig ||
        needs->tcpListenConfig || needs->udp || needs->tlsDial || needs->tlsServer;
    needs->resolver = needs->resolverAsync || needs->resolverStats ||
        needs->tcpDial || needs->tcpConnectConfig || needs->tlsDial;
    needs->future = needs->resolverAsync || needs->tcpStream ||
        needs->tcpServer || needs->udp || needs->tlsStream ||
        needs->tlsDial || needs->tlsServer;
    needs->stats = needs->engineStats || needs->resolverStats ||
        needs->tcpStream || needs->tcpServer || needs->udp || needs->tlsServer;
    needs->addressHelper = needs->address || needs->tcpStream ||
        needs->tcpServer || needs->tcpListenConfig || needs->udp ||
        needs->tlsStream || needs->tlsServer;
    needs->deadlineHelper = needs->engineObject || needs->tcpStream ||
        needs->tcpServer || needs->tcpConnectConfig ||
        needs->tcpListenConfig || needs->udp || needs->tlsStream ||
        needs->tlsDial || needs->tlsServer;
}



/* 输出当前 XRT 网络 ABI 所需的类描述和 Future 声明。 */
static void xl6CgenEmitNetDecls(XL6_Output* out, const XL6_CgenNetNeeds* needs)
{
    xl6OutputPuts(
        "#ifndef XL6_NET_RUNTIME\n"
        "#define XL6_NET_RUNTIME\n"
        "#define XL6_XNET_OK XNET_RESULT_OK\n"
        "#define XL6_XNET_AGAIN XNET_RESULT_AGAIN\n"
        "#define XL6_XNET_CLOSED XNET_RESULT_CLOSED\n"
        "#define XL6_XNET_WAIT_OPEN XNET_STREAM_WAIT_OPEN\n"
        "#define XL6_XNET_WAIT_READ XNET_STREAM_WAIT_READ\n"
        "#define XL6_XNET_WAIT_WRITE XNET_STREAM_WAIT_WRITE\n"
        "#define XL6_XNET_WAIT_DRAIN XNET_STREAM_WAIT_DRAIN\n"
        "#define XL6_XNET_WAIT_CLOSE XNET_STREAM_WAIT_CLOSE\n"
        "#define XL6_XNET_FAMILY_UNSPEC XNET_FAMILY_UNSPEC\n"
        "#define XL6_XNET_FAMILY_IPV4 XNET_FAMILY_IPV4\n"
        "#define XL6_XNET_FAMILY_IPV6 XNET_FAMILY_IPV6\n"
        "#define XL6_DEADLINE_NEVER XRT_DEADLINE_NEVER\n"
        "typedef xbytesview XL6_XrtBytesView;\n",
        out
    );
    if ( needs->address ) {
        xl6OutputPuts("static const xl6_type_desc* xl6_type_desc_XL_Class_NetAddressList(void);\n", out);
    }
    if ( needs->bytes ) {
        xl6OutputPuts("static const xl6_type_desc* xl6_type_desc_XL_Class_NetBytes(void);\n", out);
    }
    if ( needs->tcpStream ) {
        xl6OutputPuts("static const xl6_type_desc* xl6_type_desc_XL_Class_TcpStream(void);\n", out);
    }
    if ( needs->udp ) {
        xl6OutputPuts("static const xl6_type_desc* xl6_type_desc_XL_Class_UdpPacket(void);\n", out);
    }
    if ( needs->udpBatch ) {
        xl6OutputPuts("static const xl6_type_desc* xl6_type_desc_XL_Class_UdpBatch(void);\n", out);
    }
    xl6OutputPuts("\n", out);

    xl6OutputPuts("static inline char* xl6_xrt_string_dup(const char* text, size_t size);\n", out);
    if ( needs->future ) {
        xl6OutputPuts(
            "xfuture* xl6FutureThen(xfuture* future, int (*callback)(const xl6_future_result*, void*, xl6_future_result*), void* arg);\n"
            "bool xl6FutureLinkCancel(xfuture* future, xfuture* target);\n"
            "void xl6FutureDestroy(xfuture* future);\n",
            out
        );
    }
    xl6OutputPuts("\n", out);
}



/* 输出生成模块独占的默认网络运行时。 */
static void xl6CgenEmitNetLifecycle(XL6_Output* out)
{
    xl6OutputPuts(
        "static xnetengine* xl6_net_engine = 0;\n"
        "static xnetresolver* xl6_net_resolver = 0;\n\n"
        "static bool xl6_net_runtime_init(void)\n"
        "{\n"
        "    xnetengine* engine;\n"
        "    xnetresolver* resolver;\n\n"
        "    if ( (xl6_net_engine != 0) && (xl6_net_resolver != 0) ) { return true; }\n"
        "    engine = xrtNetEngineCreate(0);\n"
        "    if ( engine == 0 ) { return false; }\n"
        "    if ( !xrtNetEngineStart(engine) ) {\n"
        "        (void)xrtNetEngineDestroy(engine);\n"
        "        return false;\n"
        "    }\n"
        "    resolver = xrtNetResolverCreate(0);\n"
        "    if ( resolver == 0 ) {\n"
        "        (void)xrtNetEngineStop(engine);\n"
        "        (void)xrtNetEngineDestroy(engine);\n"
        "        return false;\n"
        "    }\n"
        "    xl6_net_engine = engine;\n"
        "    xl6_net_resolver = resolver;\n"
        "    return true;\n"
        "}\n\n"
		"static bool xl6_net_runtime_wait_objects(xnetengine* engine)\n"
		"{\n"
		"    xdeadline deadline = xrtDeadlineAfter(5000000u);\n"
		"    xnetenginestats stats;\n\n"
		"    for ( ;; ) {\n"
		"        if ( !xrtNetEngineStats(engine, &stats) ) { return false; }\n"
		"        if ( stats.LiveObjects == 0u ) { return true; }\n"
		"        if ( xrtDeadlineExpired(deadline) ) { return false; }\n"
		"        xrtSleep(1u);\n"
		"    }\n"
		"}\n\n"
        "static void xl6_net_runtime_unit(void)\n"
        "{\n"
        "    xnetresolver* resolver = xl6_net_resolver;\n"
        "    xnetengine* engine = xl6_net_engine;\n\n"
        "    xl6_net_resolver = 0;\n"
        "    xl6_net_engine = 0;\n"
        "    if ( (resolver != 0) && !xrtNetResolverDestroy(resolver) ) {\n"
        "        xl6_runtime_fatal_message(3, \"network resolver shutdown failed\");\n"
        "    }\n"
        "    if ( engine != 0 ) {\n"
		"        if ( !xl6_net_runtime_wait_objects(engine) ) {\n"
		"            xl6_runtime_fatal_message(3, \"network objects did not close before shutdown\");\n"
		"        }\n"
        "        if ( !xrtNetEngineStop(engine) ) {\n"
        "            xl6_runtime_fatal_message(3, \"network engine shutdown failed\");\n"
        "        }\n"
        "        if ( !xrtNetEngineDestroy(engine) ) {\n"
        "            xl6_runtime_fatal_message(3, \"network engine destruction failed\");\n"
        "        }\n"
        "    }\n"
        "}\n\n",
        out
    );

}



/* 输出超时、地址和 Future 结果适配。 */
/* 输出网络统计快照桥；各统计类共用一份紧凑的只读存储。 */
/* 只创建当前模块实际使用的网络运行时对象。 */
static void xl6CgenEmitNetLifecycleSelective(
    XL6_Output* out,
    const XL6_CgenNetNeeds* needs
)
{
    if ( !needs->engine && !needs->resolver ) {
        xl6OutputPuts(
            "static bool xl6_net_runtime_init(void)\n"
            "{\n"
            "    return true;\n"
            "}\n\n"
            "static void xl6_net_runtime_unit(void)\n"
            "{\n"
            "}\n\n",
            out
        );
        return;
    }

    if ( needs->engine ) {
        xl6OutputPuts("static xnetengine* xl6_net_engine = 0;\n", out);
    }
    if ( needs->resolver ) {
        xl6OutputPuts("static xnetresolver* xl6_net_resolver = 0;\n", out);
    }
    xl6OutputPuts("\n", out);

    if ( needs->tcpDial || needs->tcpConnectConfig ) {
        xl6OutputPuts(
            "/* xlang 便捷连接不暴露 worker 亲和性；默认轮转，不能把所有 dial 固定到 0 号 worker。 */\n"
            "static xatomic64 xl6_net_next_dial_affinity;\n\n"
            "static uint64_t xl6_net_next_dial_worker(void)\n"
            "{\n"
            "    uint32_t workers;\n"
            "    if ( xl6_net_engine == 0 ) { return 0u; }\n"
            "    workers = xrtNetEngineWorkerCount(xl6_net_engine);\n"
            "    if ( workers == 0u ) { return 0u; }\n"
            "    return xrtAtomic64FetchAdd(&xl6_net_next_dial_affinity, 1u, XMEMORY_RELAXED) % workers;\n"
            "}\n\n",
            out
        );
    }

    xl6OutputPuts("static bool xl6_net_runtime_init(void)\n{\n", out);
    if ( needs->engine ) {
        xl6OutputPuts("    xnetengine* engine = 0;\n", out);
    }
    if ( needs->resolver ) {
        xl6OutputPuts("    xnetresolver* resolver = 0;\n", out);
    }
    xl6OutputPuts("\n", out);

    if ( needs->engine && needs->resolver ) {
        xl6OutputPuts("    if ( (xl6_net_engine != 0) && (xl6_net_resolver != 0) ) { return true; }\n", out);
    } else if ( needs->engine ) {
        xl6OutputPuts("    if ( xl6_net_engine != 0 ) { return true; }\n", out);
    } else if ( needs->resolver ) {
        xl6OutputPuts("    if ( xl6_net_resolver != 0 ) { return true; }\n", out);
    }

    if ( needs->engine ) {
        xl6OutputPuts(
            "    engine = xrtNetEngineCreate(0);\n"
            "    if ( engine == 0 ) { return false; }\n"
            "    if ( !xrtNetEngineStart(engine) ) {\n"
            "        (void)xrtNetEngineDestroy(engine);\n"
            "        return false;\n"
            "    }\n",
            out
        );
    }
    if ( needs->resolver ) {
        xl6OutputPuts(
            "    resolver = xrtNetResolverCreate(0);\n"
            "    if ( resolver == 0 ) {\n",
            out
        );
        if ( needs->engine ) {
            xl6OutputPuts(
                "        (void)xrtNetEngineStop(engine);\n"
                "        (void)xrtNetEngineDestroy(engine);\n",
                out
            );
        }
        xl6OutputPuts("        return false;\n    }\n", out);
    }
    if ( needs->engine ) {
        xl6OutputPuts("    xl6_net_engine = engine;\n", out);
        if ( needs->tcpDial || needs->tcpConnectConfig ) {
            xl6OutputPuts("    xrtAtomic64Init(&xl6_net_next_dial_affinity, 0u);\n", out);
        }
    }
    if ( needs->resolver ) {
        xl6OutputPuts("    xl6_net_resolver = resolver;\n", out);
    }
    xl6OutputPuts("    return true;\n}\n\n", out);

    if ( needs->engine ) {
        xl6OutputPuts(
            "static bool xl6_net_runtime_wait_objects(xnetengine* engine)\n"
            "{\n"
            "    xdeadline deadline = xrtDeadlineAfter(5000000u);\n"
            "    xnetenginestats stats;\n\n"
            "    for ( ;; ) {\n"
            "        if ( !xrtNetEngineStats(engine, &stats) ) { return false; }\n"
            "        if ( stats.LiveObjects == 0u ) { return true; }\n"
            "        if ( xrtDeadlineExpired(deadline) ) { return false; }\n"
            "        xrtSleep(1u);\n"
            "    }\n"
            "}\n\n",
            out
        );
    }

    xl6OutputPuts("static void xl6_net_runtime_unit(void)\n{\n", out);
    if ( needs->resolver ) {
        xl6OutputPuts(
            "    xnetresolver* resolver = xl6_net_resolver;\n"
            "    xl6_net_resolver = 0;\n"
            "    if ( (resolver != 0) && !xrtNetResolverDestroy(resolver) ) {\n"
            "        xl6_runtime_fatal_message(3, \"network resolver shutdown failed\");\n"
            "    }\n",
            out
        );
    }
    if ( needs->engine ) {
        xl6OutputPuts(
            "    xnetengine* engine = xl6_net_engine;\n"
            "    xl6_net_engine = 0;\n"
            "    if ( engine != 0 ) {\n"
            "        if ( !xl6_net_runtime_wait_objects(engine) ) {\n"
            "            xl6_runtime_fatal_message(3, \"network objects did not close before shutdown\");\n"
            "        }\n"
            "        if ( !xrtNetEngineStop(engine) ) {\n"
            "            xl6_runtime_fatal_message(3, \"network engine shutdown failed\");\n"
            "        }\n"
            "        if ( !xrtNetEngineDestroy(engine) ) {\n"
            "            xl6_runtime_fatal_message(3, \"network engine destruction failed\");\n"
            "        }\n"
            "    }\n",
            out
        );
    }
    xl6OutputPuts("}\n\n", out);
}



/* 按实际使用的网络对象输出统计桥接，避免统计模块互相带出。 */
static void xl6CgenEmitNetStats(
	XL6_Output* out,
	const XL6_CgenNetNeeds* needs
)
{
	xl6OutputPuts(
		"#ifndef XL6_NET_STATS_RUNTIME\n"
		"#define XL6_NET_STATS_RUNTIME\n"
		"typedef struct xl6_net_stats_snapshot { size_t Count; uint64_t Values[34]; } xl6_net_stats_snapshot;\n"
		"#define XL6_NET_STATS_SET(snapshot, index, value) ((snapshot)->Values[(index)] = (uint64_t)(value))\n\n"
		"/* 创建指定字段数量的零初始化统计快照。 */\n"
		"static xl6_net_stats_snapshot* xl6_net_stats_create(size_t count)\n"
		"{\n"
		"    xl6_net_stats_snapshot* snapshot = (xl6_net_stats_snapshot*)xrtCalloc(1u, sizeof(*snapshot));\n"
		"    if ( snapshot != 0 ) { snapshot->Count = count; }\n"
		"    return snapshot;\n"
		"}\n\n"
		"/* 释放任意网络统计快照。 */\n"
		"void xl6_xrt_net_stats_destroy(void* raw) { xrtFree(raw); }\n\n"
		"/* 按稳定字段编号读取网络统计值。 */\n"
		"uint64_t xl6_xrt_net_stats_value(void* raw, int64_t index)\n"
		"{\n"
		"    xl6_net_stats_snapshot* snapshot = (xl6_net_stats_snapshot*)raw;\n"
		"    return (snapshot != 0) && (index >= 0) && ((size_t)index < snapshot->Count) ? snapshot->Values[index] : UINT64_C(0);\n"
		"}\n"
		"#endif\n\n",
		out
	);

	if ( needs->engineStats ) {
		xl6OutputPuts(
		"/* 取得显式网络引擎的一致统计快照。 */\n"
		"void* xl6_xrt_net_engine_stats(void* raw)\n"
		"{\n"
		"    xnetenginestats value; xl6_net_stats_snapshot* snapshot;\n"
		"    if ( (raw == 0) || !xrtNetEngineStats((xnetengine*)raw, &value) ) { return 0; }\n"
		"    snapshot = xl6_net_stats_create(19u); if ( snapshot == 0 ) { return 0; }\n"
		"    XL6_NET_STATS_SET(snapshot, 0, value.State); XL6_NET_STATS_SET(snapshot, 1, value.Workers);\n"
		"    XL6_NET_STATS_SET(snapshot, 2, value.PostsAccepted); XL6_NET_STATS_SET(snapshot, 3, value.PostsRejected); XL6_NET_STATS_SET(snapshot, 4, value.PostsExecuted);\n"
		"    XL6_NET_STATS_SET(snapshot, 5, value.TimersAccepted); XL6_NET_STATS_SET(snapshot, 6, value.TimersRejected); XL6_NET_STATS_SET(snapshot, 7, value.TimersFired);\n"
		"    XL6_NET_STATS_SET(snapshot, 8, value.TimersCancelled); XL6_NET_STATS_SET(snapshot, 9, value.TimersClosed); XL6_NET_STATS_SET(snapshot, 10, value.TimerErrors);\n"
		"    XL6_NET_STATS_SET(snapshot, 11, value.Events); XL6_NET_STATS_SET(snapshot, 12, value.WaitErrors); XL6_NET_STATS_SET(snapshot, 13, value.NodeCacheHits);\n"
		"    XL6_NET_STATS_SET(snapshot, 14, value.NodeCacheMisses); XL6_NET_STATS_SET(snapshot, 15, value.PendingCommands); XL6_NET_STATS_SET(snapshot, 16, value.ActiveTimers);\n"
		"    XL6_NET_STATS_SET(snapshot, 17, value.NodeCachedBytes); XL6_NET_STATS_SET(snapshot, 18, value.LiveObjects);\n"
		"    return snapshot;\n"
		"}\n\n"
		"/* 取得当前模块默认网络引擎的统计快照。 */\n"
		"void* xl6_xrt_net_runtime_stats(void) { return xl6_xrt_net_engine_stats(xl6_net_engine); }\n\n",
			out
		);
	}

	if ( needs->resolverStats ) {
		xl6OutputPuts(
		"/* 取得当前模块默认解析器的一致统计快照。 */\n"
		"void* xl6_xrt_net_resolver_stats(void)\n"
		"{\n"
		"    xnetresolverstats value; xl6_net_stats_snapshot* snapshot;\n"
		"    if ( (xl6_net_resolver == 0) || !xrtNetResolverStats(xl6_net_resolver, &value) ) { return 0; }\n"
		"    snapshot = xl6_net_stats_create(16u); if ( snapshot == 0 ) { return 0; }\n"
		"    XL6_NET_STATS_SET(snapshot, 0, value.Workers); XL6_NET_STATS_SET(snapshot, 1, value.Submitted); XL6_NET_STATS_SET(snapshot, 2, value.Rejected);\n"
		"    XL6_NET_STATS_SET(snapshot, 3, value.CacheHits); XL6_NET_STATS_SET(snapshot, 4, value.CacheMisses); XL6_NET_STATS_SET(snapshot, 5, value.Coalesced);\n"
		"    XL6_NET_STATS_SET(snapshot, 6, value.QueriesStarted); XL6_NET_STATS_SET(snapshot, 7, value.Resolved); XL6_NET_STATS_SET(snapshot, 8, value.Failed);\n"
		"    XL6_NET_STATS_SET(snapshot, 9, value.Cancelled); XL6_NET_STATS_SET(snapshot, 10, value.Outstanding); XL6_NET_STATS_SET(snapshot, 11, value.ActiveQueries);\n"
		"    XL6_NET_STATS_SET(snapshot, 12, value.QueuedQueries); XL6_NET_STATS_SET(snapshot, 13, value.RunningQueries); XL6_NET_STATS_SET(snapshot, 14, value.ReadyCallbacks);\n"
		"    XL6_NET_STATS_SET(snapshot, 15, value.CachedResults); return snapshot;\n"
		"}\n\n",
			out
		);
	}


	xl6CgenEmitNetTcpStatsComponent(out, needs);

	if ( needs->udp ) {
		xl6OutputPuts(
		"/* 取得 UDP 套接字的无锁统计快照。 */\n"
		"void* xl6_xrt_udp_stats(void* raw)\n"
		"{\n"
		"    xnetudpstats value; xl6_net_stats_snapshot* snapshot;\n"
		"    if ( (raw == 0) || !xrtNetUdpStats((xnetudp*)raw, &value) ) { return 0; }\n"
		"    snapshot = xl6_net_stats_create(34u); if ( snapshot == 0 ) { return 0; }\n"
		"    XL6_NET_STATS_SET(snapshot, 0, value.State); XL6_NET_STATS_SET(snapshot, 1, value.ReceivedPackets); XL6_NET_STATS_SET(snapshot, 2, value.ReceivedBytes);\n"
		"    XL6_NET_STATS_SET(snapshot, 3, value.SentPackets); XL6_NET_STATS_SET(snapshot, 4, value.SentBytes); XL6_NET_STATS_SET(snapshot, 5, value.Truncated);\n"
		"    XL6_NET_STATS_SET(snapshot, 6, value.TruncatedDropped); XL6_NET_STATS_SET(snapshot, 7, value.DroppedNewest); XL6_NET_STATS_SET(snapshot, 8, value.DroppedOldest);\n"
		"    XL6_NET_STATS_SET(snapshot, 9, value.ReceiveErrors); XL6_NET_STATS_SET(snapshot, 10, value.SendErrors); XL6_NET_STATS_SET(snapshot, 11, value.SendRejected);\n"
		"    XL6_NET_STATS_SET(snapshot, 12, value.DatagramErrors); XL6_NET_STATS_SET(snapshot, 13, value.DatagramErrorsDropped); XL6_NET_STATS_SET(snapshot, 14, value.PathMtuUpdates);\n"
		"    XL6_NET_STATS_SET(snapshot, 15, value.PathMtu); XL6_NET_STATS_SET(snapshot, 16, value.QueuedBytes); XL6_NET_STATS_SET(snapshot, 17, value.PeakQueuedBytes);\n"
		"    XL6_NET_STATS_SET(snapshot, 18, value.QueuedPackets); XL6_NET_STATS_SET(snapshot, 19, value.PeakQueuedPackets); XL6_NET_STATS_SET(snapshot, 20, value.ReceiveQueued);\n"
		"    XL6_NET_STATS_SET(snapshot, 21, value.PeakReceiveQueued); XL6_NET_STATS_SET(snapshot, 22, value.ReceiveQueuedBytes); XL6_NET_STATS_SET(snapshot, 23, value.PeakReceiveQueuedBytes);\n"
		"    XL6_NET_STATS_SET(snapshot, 24, value.ErrorQueued); XL6_NET_STATS_SET(snapshot, 25, value.PeakErrorQueued); XL6_NET_STATS_SET(snapshot, 26, value.ErrorQueuedBytes);\n"
		"    XL6_NET_STATS_SET(snapshot, 27, value.PeakErrorQueuedBytes); XL6_NET_STATS_SET(snapshot, 28, value.ReceiveWaiters); XL6_NET_STATS_SET(snapshot, 29, value.ErrorWaiters);\n"
		"    XL6_NET_STATS_SET(snapshot, 30, value.ActiveReceives); XL6_NET_STATS_SET(snapshot, 31, value.ActiveSends); XL6_NET_STATS_SET(snapshot, 32, value.PeakActiveSends);\n"
		"    XL6_NET_STATS_SET(snapshot, 33, value.Connected); return snapshot;\n"
		"}\n\n",
			out
		);
	}

	xl6OutputPuts("#undef XL6_NET_STATS_SET\n\n", out);
}



static void xl6CgenEmitNetHelpers(
    XL6_Output* out,
    const XL6_CgenNetNeeds* needs
)
{
    if ( needs->deadlineHelper ) {
        xl6OutputPuts(
        "static uint64_t xl6_net_timeout_us(long long timeoutMs)\n"
        "{\n"
        "    uint64_t value;\n\n"
        "    if ( timeoutMs < 0 ) { return UINT64_MAX; }\n"
        "    value = (uint64_t)timeoutMs;\n"
        "    return value > (UINT64_MAX / 1000u) ? UINT64_MAX : (value * 1000u);\n"
        "}\n\n"
        "static xdeadline xl6_net_deadline(long long timeoutMs)\n"
        "{\n"
        "    uint64_t timeout = xl6_net_timeout_us(timeoutMs);\n"
        "    return timeout == UINT64_MAX ? XL6_DEADLINE_NEVER : xrtDeadlineAfter(timeout);\n"
        "}\n\n",
            out
        );
    }

    if ( needs->addressHelper ) {
        xl6OutputPuts(
		"static bool xl6_net_address_parse(xnetaddr* address, const char* host, uint16_t port, bool passive)\n"
        "{\n"
        "    if ( address == 0 ) { return false; }\n"
        "    if ( (host == 0) || (host[0] == '\\0') ) {\n"
        "        return xrtNetAddrAny(address, XL6_XNET_FAMILY_IPV4, port);\n"
        "    }\n"
        "    if ( xrtNetAddrParse(address, host, port) ) { return true; }\n"
        "    (void)passive;\n"
        "    return xrtNetResolveOne(address, host, port, XL6_XNET_FAMILY_UNSPEC);\n"
        "}\n\n"
        "static char* xl6_net_address_string(const xnetaddr* address)\n"
        "{\n"
        "    char* text = address != 0 ? xrtNetAddrString(address) : 0;\n"
        "    return text != 0 ? text : xl6_xrt_string_dup(\"\", 0u);\n"
        "}\n\n",
            out
        );
    }

    if ( needs->future ) {
        xl6OutputPuts(
        "static void xl6_net_free_string_value(void* value)\n"
        "{\n"
        "    const char** box = (const char**)value;\n"
        "    if ( box == 0 ) { return; }\n"
        "    xrtFree((void*)*box);\n"
        "    xrtFree(box);\n"
        "}\n\n"
        "static void xl6_net_free_class_value(void* value)\n"
        "{\n"
        "    xvalue** box = (xvalue**)value;\n"
        "    if ( box == 0 ) { return; }\n"
        "    if ( *box != 0 ) { xrtValueRelease(*box); }\n"
        "    xrtFree(box);\n"
        "}\n\n"
        "static int xl6_net_result_class(xl6_future_result* output, void* handle, const xl6_type_desc* type, void (*drop)(void*))\n"
        "{\n"
        "    xvalue** box;\n"
        "    xvalue* value;\n\n"
        "    if ( (output == 0) || (handle == 0) || (type == 0) ) { if ( (handle != 0) && (drop != 0) ) { drop(handle); } return -1; }\n"
        "    value = xl6ValueHandle(type, handle, 0x00000001u | 0x00000004u);\n"
        "    if ( value == 0 ) { if ( drop != 0 ) { drop(handle); } return -1; }\n"
        "    box = (xvalue**)xrtMalloc(sizeof(*box));\n"
        "    if ( box == 0 ) { xrtValueRelease(value); return -1; }\n"
        "    *box = value;\n"
        "    memset(output, 0, sizeof(*output));\n"
        "    output->pValue = box;\n"
        "    output->iFlags = XL6_FUTURE_OWN_VALUE;\n"
        "    output->pfnFreeValue = xl6_net_free_class_value;\n"
        "    return 0;\n"
        "}\n\n"
        "static int xl6_net_result_bool(xl6_future_result* output, bool value)\n"
        "{\n"
        "    int* box;\n\n"
        "    if ( output == 0 ) { return -1; }\n"
        "    box = (int*)xrtMalloc(sizeof(int));\n"
        "    if ( box == 0 ) { return -1; }\n"
        "    *box = value ? 1 : 0;\n"
        "    memset(output, 0, sizeof(*output));\n"
        "    output->pValue = box;\n"
        "    output->iFlags = XL6_FUTURE_OWN_VALUE;\n"
        "    output->pfnFreeValue = xrtFree;\n"
        "    return 0;\n"
        "}\n\n",
            out
        );
    }

    if ( needs->tcpStream ) {
        xl6OutputPuts(
        "static void xl6_net_drop_stream(void* value) { xrtNetStreamDestroy((xnetstream*)value); }\n\n"
        "static int xl6_net_map_stream(const xl6_future_result* input, void* arg, xl6_future_result* output)\n"
        "{\n"
        "    xnetstream* stream = input != 0 ? (xnetstream*)input->pValue : 0;\n"
        "    (void)arg;\n"
        "    stream = stream != 0 ? xrtNetStreamRef(stream) : 0;\n"
        "    return xl6_net_result_class(output, stream, xl6_type_desc_XL_Class_TcpStream(), xl6_net_drop_stream);\n"
        "}\n\n",
            out
        );
    }
    if ( needs->udp ) {
        xl6OutputPuts(
        "static void xl6_net_drop_packet(void* value) { xrtNetUdpPacketDestroy((xnetudppacket*)value); }\n\n"
        "static int xl6_net_map_packet(const xl6_future_result* input, void* arg, xl6_future_result* output)\n"
        "{\n"
        "    xnetudppacket* packet = input != 0 ? (xnetudppacket*)input->pValue : 0;\n"
        "    (void)arg;\n"
        "    packet = packet != 0 ? xrtNetUdpPacketRef(packet) : 0;\n"
        "    return xl6_net_result_class(output, packet, xl6_type_desc_XL_Class_UdpPacket(), xl6_net_drop_packet);\n"
        "}\n\n",
            out
        );
    }
    if ( needs->resolverAsync ) {
        xl6OutputPuts(
        "static void xl6_net_drop_address_list(void* value) { xrtNetAddrListDestroy((xnetaddrlist*)value); }\n\n"
        "static int xl6_net_map_address_list(const xl6_future_result* input, void* arg, xl6_future_result* output)\n"
        "{\n"
        "    xnetaddrlist* list = input != 0 ? (xnetaddrlist*)input->pValue : 0;\n"
        "    uint16_t port = (uint16_t)(uintptr_t)arg;\n"
        "    list = list != 0 ? xrtNetAddrListWithPort(list, port) : 0;\n"
        "    return xl6_net_result_class(output, list, xl6_type_desc_XL_Class_NetAddressList(), xl6_net_drop_address_list);\n"
        "}\n\n",
            out
        );
    }
    if ( needs->bytes && needs->future ) {
        xl6OutputPuts(
        "static void xl6_net_drop_bytes(void* value) { xrtNetBytesDestroy((xnetbytes*)value); }\n\n"
        "static int xl6_net_map_bytes(const xl6_future_result* input, void* arg, xl6_future_result* output)\n"
        "{\n"
        "    xnetbytes* bytes = input != 0 ? (xnetbytes*)input->pValue : 0;\n"
        "    (void)arg;\n"
        "    bytes = bytes != 0 ? xrtNetBytesRef(bytes) : 0;\n"
        "    return xl6_net_result_class(output, bytes, xl6_type_desc_XL_Class_NetBytes(), xl6_net_drop_bytes);\n"
        "}\n\n",
            out
        );
    }
    if ( needs->future ) {
        xl6OutputPuts(
        "static int xl6_net_map_bool(const xl6_future_result* input, void* arg, xl6_future_result* output)\n"
        "{\n"
        "    (void)input;\n"
        "    (void)arg;\n"
        "    return xl6_net_result_bool(output, true);\n"
        "}\n\n",
        out
    );
    }
    if ( needs->udpBatch ) {
        xl6OutputPuts(
            "static void xl6_net_drop_udp_batch(void* value) { xrtNetUdpBatchDestroy((xnetudpbatch*)value); }\n\n"
            "static int xl6_net_map_udp_batch(const xl6_future_result* input, void* arg, xl6_future_result* output)\n"
            "{\n"
            "    xnetudpbatch* batch = input != 0 ? (xnetudpbatch*)input->pValue : 0;\n"
            "    (void)arg;\n"
            "    batch = batch != 0 ? xrtNetUdpBatchRef(batch) : 0;\n"
            "    return xl6_net_result_class(output, batch, xl6_type_desc_XL_Class_UdpBatch(), xl6_net_drop_udp_batch);\n"
            "}\n\n",
            out
        );
    }

    if ( needs->tcpStream || needs->tlsStream ) {
    xl6OutputPuts(
        "static int xl6_net_map_text(const xl6_future_result* input, void* arg, xl6_future_result* output)\n"
        "{\n"
        "    const xnetbytes* bytes = input != 0 ? (const xnetbytes*)input->pValue : 0;\n"
        "    XL6_XrtBytesView view;\n"
        "    const char** box;\n"
        "    char* text;\n"
        "    (void)arg;\n\n"
        "    if ( bytes == 0 ) { return -1; }\n"
        "    view = xrtNetBytesView(bytes);\n"
        "    text = (char*)xrtMalloc(view.Size + 1u);\n"
        "    if ( text == 0 ) { return -1; }\n"
        "    if ( view.Size != 0u ) { memcpy(text, view.Data, view.Size); }\n"
        "    text[view.Size] = '\\0';\n"
        "    box = (const char**)xrtMalloc(sizeof(const char*));\n"
        "    if ( box == 0 ) { xrtFree(text); return -1; }\n"
        "    *box = text;\n"
        "    memset(output, 0, sizeof(*output));\n"
        "    output->pValue = box;\n"
        "    output->iFlags = XL6_FUTURE_OWN_VALUE;\n"
        "    output->pfnFreeValue = xl6_net_free_string_value;\n"
        "    return 0;\n"
        "}\n\n",
        out
    );
    }
    if ( needs->future ) {
    xl6OutputPuts(
        "static xfuture* xl6_net_future_map(xfuture* source, int (*mapper)(const xl6_future_result*, void*, xl6_future_result*))\n"
        "{\n"
        "    xfuture* mapped;\n\n"
        "    if ( source == 0 ) { return 0; }\n"
        "    mapped = xl6FutureThen(source, mapper, 0);\n"
        "    if ( mapped != 0 ) { (void)xl6FutureLinkCancel(mapped, source); }\n"
        "    xl6FutureDestroy(source);\n"
        "    return mapped;\n"
        "}\n\n"
        "static xfuture* xl6_net_future_map_arg(xfuture* source, int (*mapper)(const xl6_future_result*, void*, xl6_future_result*), void* arg)\n"
        "{\n"
        "    xfuture* mapped;\n\n"
        "    if ( source == 0 ) { return 0; }\n"
        "    mapped = xl6FutureThen(source, mapper, arg);\n"
        "    if ( mapped != 0 ) { (void)xl6FutureLinkCancel(mapped, source); }\n"
        "    xl6FutureDestroy(source);\n"
        "    return mapped;\n"
        "}\n\n",
        out
    );
    }
}



#include "xl6_cgen_net_transport.c"
