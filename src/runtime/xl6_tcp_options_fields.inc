/* Shared spelling/type inventory; setters validate coherent groups atomically. */
#ifdef XL6_TCP_STREAM_SIZE
XL6_TCP_STREAM_SIZE(ReadSize,readSize)
XL6_TCP_STREAM_SIZE(ReadLimit,readLimit)
XL6_TCP_STREAM_SIZE(WriteHighWater,writeHighWater)
XL6_TCP_STREAM_SIZE(WriteLowWater,writeLowWater)
XL6_TCP_STREAM_SIZE(WriteLimit,writeLimit)
#endif
#ifdef XL6_TCP_STREAM_BOOL
XL6_TCP_STREAM_BOOL(NoDelay,noDelay)
XL6_TCP_STREAM_BOOL(KeepAlive,keepAlive)
#endif
#ifdef XL6_TCP_LISTEN_INT
XL6_TCP_LISTEN_INT(AcceptConcurrency,acceptConcurrency)
XL6_TCP_LISTEN_INT(AcceptQueueLimit,acceptQueueLimit)
XL6_TCP_LISTEN_INT(Backlog,backlog)
#endif
#ifdef XL6_TCP_LISTEN_BOOL
XL6_TCP_LISTEN_BOOL(ReuseAddress,reuseAddress)
XL6_TCP_LISTEN_BOOL(ReusePort,reusePort)
XL6_TCP_LISTEN_BOOL(ExclusiveAddress,exclusiveAddress)
XL6_TCP_LISTEN_BOOL(IPv6Only,ipv6Only)
#endif
