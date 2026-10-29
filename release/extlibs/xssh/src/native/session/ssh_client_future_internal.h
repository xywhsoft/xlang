#ifndef XRT_SSH_CLIENT_FUTURE_INTERNAL_H
#define XRT_SSH_CLIENT_FUTURE_INTERNAL_H

#include <xrt/ssh_client_future.h>



#if defined(XSSH_FEATURE_CLIENT_FUTURE)

/* 内部信号只在对应 SSH/TCP 事务提交后发布。 */
typedef enum xsshclientfuturesignal {
	XSSH_CLIENT_FUTURE_READY = 0,
	XSSH_CLIENT_FUTURE_DRAIN = 1,
	XSSH_CLIENT_FUTURE_CLOSE = 2,
	XSSH_CLIENT_FUTURE_CHANNEL = 3,
	XSSH_CLIENT_FUTURE_DATA = 4,
	XSSH_CLIENT_FUTURE_WRITABLE = 5,
	XSSH_CLIENT_FUTURE_GLOBAL = 6
} xsshclientfuturesignal;



/* 通知只借用现有稳定对象和回调期错误，不借用输入 packet。 */
typedef struct xsshclientfuturenotice {
	xsshclientfuturesignal Signal;
	xsshchannel* Channel;
	const xsshclientchannelnotice* ChannelNotice;
	const xsshclientglobalnotice* GlobalNotice;
	const xerror* Error;
	xsshchanneliostream Stream;
	uint32 ChannelLocal;
	bool HasChannelLocal;
} xsshclientfuturenotice;



/* 发布一个已经提交的客户端条件变化。 */
void __xrtSshClientFutureNotify(
	xsshclient* pClient,
	const xsshclientfuturenotice* pNotice
);



/* 终结并分离客户端拥有的等待管理器。 */
void __xrtSshClientFutureClear(xsshclient* pClient);

#endif

#endif
