#ifndef XRT_SSH_CHANNEL_IO_H
#define XRT_SSH_CHANNEL_IO_H

#include <xrt/net.h>
#include <xrt/ssh_channel_core.h>



#if defined(XSSH_FEATURE_CHANNEL_IO) && \
	(!defined(XSSH_FEATURE_CHANNEL_CORE) || \
	 !defined(XRT_FEATURE_NET_BUFFER))
	#error "XSSH_FEATURE_CHANNEL_IO requires channel core and XRT network buffer"
#endif



#if defined(XSSH_FEATURE_CHANNEL_IO)

#define XSSH_CHANNEL_IO_LIMIT_DEFAULT 2097152u



/* DATA 表示普通 channel data，STDERR 表示 RFC 4254 标准错误扩展流。 */
typedef enum xsshchanneliostream {
	XSSH_CHANNEL_IO_DATA = 0,
	XSSH_CHANNEL_IO_STDERR = 1
} xsshchanneliostream;



/* 待提交类型只允许一个收发事务，避免同一 channel 的窗口与队首交错变化。 */
typedef enum xsshchanneliopending {
	XSSH_CHANNEL_IO_PENDING_NONE = 0,
	XSSH_CHANNEL_IO_PENDING_RECEIVE = 1,
	XSSH_CHANNEL_IO_PENDING_SEND = 2
} xsshchanneliopending;



/* 收发限制分别约束两条流的总量；默认值均为 2 MiB。 */
typedef struct xsshchannelioconfig {
	size_t ReceiveLimit;
	size_t SendLimit;
} xsshchannelioconfig;



/*
	对象只拥有动态缓冲链，并借用一个 channel core；不拥有网络、packet 或请求队列。
	字段公开用于诊断，缓冲内容只能通过本模块和 xnetbuf 的只读视图访问。
*/
typedef struct xsshchannelio {
	xnetbuf ReceiveData;
	xnetbuf ReceiveError;
	xnetbuf SendData;
	xnetbuf SendError;
	xnetbuf ReceiveStaging;
	xsshchannelcore ChannelBefore;
	xsshchannelcore ChannelAfter;
	xsshchannelcore* Channel;
	cbytes SendHead;
	size_t ReceiveLimit;
	size_t SendLimit;
	size_t PendingBytes;
	xsshchanneliostream PendingStream;
	xsshchanneliopending Pending;
	bool Initialized;
	uint32 Guard;
} xsshchannelio;



XRT_EXTERN_C_BEGIN



/* 写入 2 MiB 收发硬上限。 */
XRT_API void xrtSshChannelIoConfigInit(xsshchannelioconfig* pConfig);



/*
	绑定一个尚无已接收数据的 channel，并用同一缓冲池初始化五条动态链。
	ReceiveLimit 必须覆盖当前已通告接收窗口；空配置使用默认值。
*/
XRT_API bool xrtSshChannelIoInit(
	xsshchannelio* pIo,
	xnetbufpool* pPool,
	xsshchannelcore* pChannel,
	const xsshchannelioconfig* pConfig
);



/* 释放全部动态块；未读取数据按应用丢弃处理并转入 channel 待返还额度。 */
XRT_API void xrtSshChannelIoClear(xsshchannelio* pIo);



/* 返回指定接收流当前已经可靠提交的可读字节数。 */
XRT_API size_t xrtSshChannelIoReadable(
	const xsshchannelio* pIo,
	xsshchanneliostream Stream
);



/* 返回指定接收流的借用只读缓冲；对象失效或流类型错误时返回空。 */
XRT_API const xnetbuf* xrtSshChannelIoReadBuffer(
	const xsshchannelio* pIo,
	xsshchanneliostream Stream
);



/* 复制并消费最多 Capacity 字节，同时把实际消费量转入接收窗口待返还额度。 */
XRT_API xsshcode xrtSshChannelIoRead(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	void* pOutput,
	size_t iCapacity,
	size_t* pRead
);



/* 零复制消费指定接收流的前缀，并更新 channel 接收窗口计数。 */
XRT_API xsshcode xrtSshChannelIoConsume(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	size_t iSize
);



/* 返回指定发送流当前排队字节数。 */
XRT_API size_t xrtSshChannelIoQueued(
	const xsshchannelio* pIo,
	xsshchanneliostream Stream
);



/* 返回两条发送流共享硬上限中尚可受理的字节数。 */
XRT_API size_t xrtSshChannelIoWritable(const xsshchannelio* pIo);



/* 返回指定流下一条消息可发送的连续队首字节数，零表示无数据或远端窗口阻塞。 */
XRT_API size_t xrtSshChannelIoSendLimit(
	const xsshchannelio* pIo,
	xsshchanneliostream Stream
);



/* 在共享发送预算内追加一份数据副本。 */
XRT_API xsshcode xrtSshChannelIoWrite(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	const void* pData,
	size_t iSize
);



/* 追加借用数据；调用方保证其存活到消费或清理。 */
XRT_API xsshcode xrtSshChannelIoWriteBorrow(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	const void* pData,
	size_t iSize
);



/* 接管由 xrtMalloc 家族分配的数据；失败或零长度不转移所有权。 */
XRT_API xsshcode xrtSshChannelIoWriteTake(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	void* pData,
	size_t iSize
);



/* 接管带释放过程的外部数据；失败或零长度不转移所有权。 */
XRT_API xsshcode xrtSshChannelIoWriteRef(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	const void* pData,
	size_t iSize,
	xnetreleaseproc pRelease,
	ptr pContext
);



/* 把调用方缓冲链移动到指定发送流；超出共享预算时源缓冲保持不变。 */
XRT_API xsshcode xrtSshChannelIoWriteBuffer(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	xnetbuf* pBuffer
);



/*
	为一条已解析 data 预分配接收存储，但不改变 channel 或可读缓冲。
	STDERR 只应用于 extended-data type 1；未知扩展类型应走 connection 的借用快路径。
*/
XRT_API xsshcode xrtSshChannelIoReceivePrepare(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	uint32 iRecipient,
	xbytesview Data
);



/* 外层已提交 channel 接收状态后，把预分配块无分配移动到可读缓冲。 */
XRT_API xsshcode xrtSshChannelIoReceiveCommit(xsshchannelio* pIo);



/* 在 channel 状态尚未提交时放弃接收预分配。 */
XRT_API xsshcode xrtSshChannelIoReceiveAbort(xsshchannelio* pIo);



/*
	把发送队首按远端窗口、最大包和 writer 空间切成一条最终 channel payload。
	成功后外层应依次提交 transport、connection/channel，再调用 SendCommit。
*/
XRT_API xsshcode xrtSshChannelIoSendPrepare(
	xsshchannelio* pIo,
	xsshchanneliostream Stream,
	xsshwriter* pWriter,
	xbytesview* pPayload
);



/* 外层已提交 channel 发送状态后消费本次发送队首。 */
XRT_API xsshcode xrtSshChannelIoSendCommit(xsshchannelio* pIo);



/* 在 channel 状态尚未提交时放弃发送事务，排队数据保持不变。 */
XRT_API xsshcode xrtSshChannelIoSendAbort(xsshchannelio* pIo);



XRT_EXTERN_C_END

#endif

#endif
