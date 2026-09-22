#ifndef XRT_SSH_CLIENT_FUTURE_H
#define XRT_SSH_CLIENT_FUTURE_H

#include <xrt/future.h>
#include <xrt/ssh_client.h>



#if defined(XSSH_FEATURE_CLIENT_FUTURE) && \
	(!defined(XSSH_FEATURE_CLIENT) || \
	 !defined(XRT_FEATURE_FUTURE) || \
	 !defined(XRT_FEATURE_SPIN))
	#error "XSSH_FEATURE_CLIENT_FUTURE requires SSH client, XRT Future and spin"
#endif



#if defined(XSSH_FEATURE_CLIENT_FUTURE)

/* 客户端等待是水平条件；Drain 只表示底层 TCP 发送队列已经排空。 */
typedef enum xsshclientwait {
	XSSH_CLIENT_WAIT_READY = 0,
	XSSH_CLIENT_WAIT_DRAIN = 1,
	XSSH_CLIENT_WAIT_CLOSE = 2
} xsshclientwait;



/* Channel 等待不消费数据，也不改变 EOF/CLOSE 或请求回复状态。 */
typedef enum xsshclientchannelwait {
	XSSH_CLIENT_CHANNEL_WAIT_OPEN = 0,
	XSSH_CLIENT_CHANNEL_WAIT_WRITE = 1,
	XSSH_CLIENT_CHANNEL_WAIT_EOF = 2,
	XSSH_CLIENT_CHANNEL_WAIT_CLOSE = 3
} xsshclientchannelwait;



XRT_EXTERN_C_BEGIN



/* 在客户端 Worker 上创建 Ready、TCP Drain 或 Close 的单次等待。 */
XRT_API xfuture* xrtSshClientWaitAsync(
	xsshclient* pClient,
	xsshclientwait Wait
);



/* 在客户端 Worker 上创建 channel open、可写、EOF 或 close 等待。 */
XRT_API xfuture* xrtSshClientChannelWaitAsync(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xsshclientchannelwait Wait
);



/* 等待指定接收流出现至少一个可读字节；成功不会消费缓冲。 */
XRT_API xfuture* xrtSshClientChannelReadAsync(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xsshchanneliostream Stream
);



/* 等待指定 channel 请求 token 的 success/failure 回复。 */
XRT_API xfuture* xrtSshClientChannelReplyAsync(
	xsshclient* pClient,
	xsshchannel* pChannel,
	uint64 iReplyToken
);



/* 等待指定全局请求 token 的 success/failure 回复。 */
XRT_API xfuture* xrtSshClientGlobalReplyAsync(
	xsshclient* pClient,
	uint64 iReplyToken
);



XRT_EXTERN_C_END

#endif

#endif
