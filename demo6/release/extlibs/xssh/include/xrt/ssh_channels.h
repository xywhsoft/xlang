#ifndef XRT_SSH_CHANNELS_H
#define XRT_SSH_CHANNELS_H

#include <xrt/map.h>
#include <xrt/ssh_channel_io.h>
#include <xrt/ssh_connection_session.h>



#if defined(XSSH_FEATURE_CHANNELS) && \
	(!defined(XSSH_FEATURE_CHANNEL_IO) || \
	 !defined(XSSH_FEATURE_CONNECTION_SESSION) || \
	 !defined(XRT_FEATURE_INT_MAP))
	#error "XSSH_FEATURE_CHANNELS requires channel I/O, connection session and XRT int map"
#endif



#if defined(XSSH_FEATURE_CHANNELS)

#define XSSH_CHANNELS_MAX_DEFAULT 1024u
#define XSSH_CHANNELS_REPLY_LIMIT_DEFAULT 64u
#define XSSH_CHANNELS_WINDOW_DEFAULT 2097152u
#define XSSH_CHANNELS_PACKET_DEFAULT 32768u
#define XSSH_CHANNELS_ADJUST_DEFAULT 1048576u



/* Channel 集合配置同时约束对象数量、窗口、动态数据和请求回复内存。 */
typedef struct xsshchannelsconfig {
	size_t MaxChannels;
	size_t ReplyLimit;
	uint32 ReceiveWindow;
	uint32 ReceiveMaxPacket;
	uint32 AdjustThreshold;
	xsshchannelioconfig Io;
} xsshchannelsconfig;



/* 单个动态 channel 组合协议核心、数据缓冲和按需回复队列。 */
typedef struct xsshchannel {
	xsshchannelcore Core;
	xsshchannelio Io;
	xsshreplyqueue Replies;
	uint64* ReplyTokens;
	size_t ReplyCapacity;
	size_t ReplyLimit;
	ptr UserData;
	bool Incoming;
	bool Initialized;
	uint32 Guard;
} xsshchannel;



/* Channel 集合借用网络缓冲池，拥有全部 channel 对象和回复 token。 */
typedef struct xsshchannels {
	xintmap Map;
	xsshchannelsconfig Config;
	xnetbufpool* Pool;
	uint32 NextLocal;
	bool Initialized;
	uint32 Guard;
} xsshchannels;



/* 外置迭代器允许调用方遍历活动 channel，结构修改会使其失效。 */
typedef struct xsshchannelsiter {
	xintmapiter Base;
	bool Active;
} xsshchannelsiter;



XRT_EXTERN_C_BEGIN



/* 写入适合交互会话和并发客户端的有界默认配置。 */
XRT_API void xrtSshChannelsConfigInit(xsshchannelsconfig* pConfig);



/* 初始化空集合；缓冲池只借用，空指针使用 XRT 默认网络缓冲池。 */
XRT_API bool xrtSshChannelsInit(
	xsshchannels* pChannels,
	xnetbufpool* pPool,
	const xsshchannelsconfig* pConfig
);



/* 释放全部 channel、动态数据和回复 token，不释放借用缓冲池。 */
XRT_API void xrtSshChannelsClear(xsshchannels* pChannels);



/* 返回当前活动 channel 数量；无效集合返回零。 */
XRT_API size_t xrtSshChannelsCount(const xsshchannels* pChannels);



/* 创建等待 peer confirmation 的本端 channel，并返回稳定借用地址。 */
XRT_API xsshcode xrtSshChannelsOpen(
	xsshchannels* pChannels,
	xsshchannel** ppChannel
);



/* 为一条 peer CHANNEL_OPEN 创建等待本端决定的动态 channel。 */
XRT_API xsshcode xrtSshChannelsAccept(
	xsshchannels* pChannels,
	const xsshchannelopen* pOpen,
	xsshchannel** ppChannel
);



/* 按本端 channel id 返回稳定借用地址；未找到是正常结果。 */
XRT_API xsshchannel* xrtSshChannelsGet(
	xsshchannels* pChannels,
	uint32 iLocal
);



/* 返回只读 channel；未找到是正常结果。 */
XRT_API const xsshchannel* xrtSshChannelsConstGet(
	const xsshchannels* pChannels,
	uint32 iLocal
);



/* 为 want-reply 请求按需扩展 token 存储，已有 token 顺序保持不变。 */
XRT_API xsshcode xrtSshChannelReplyReserve(
	xsshchannel* pChannel,
	size_t iCapacity
);



/* 删除已经结束且没有未消费数据或回复的 channel。 */
XRT_API bool xrtSshChannelsRemove(
	xsshchannels* pChannels,
	uint32 iLocal
);



/* 强制丢弃指定 channel 及其全部排队数据，供连接关闭和策略拒绝使用。 */
XRT_API bool xrtSshChannelsDiscard(
	xsshchannels* pChannels,
	uint32 iLocal
);



/* 直接适配 xsshchannelresolveproc，UserData 必须指向活动集合。 */
XRT_API bool xrtSshChannelsResolve(
	ptr pUserData,
	uint32 iLocal,
	xsshchannelcore** ppChannel,
	xsshreplyqueue** ppReplies
);



/* 启动按本端 channel id 递增的外置迭代。 */
XRT_API bool xrtSshChannelsIterBegin(
	xsshchannels* pChannels,
	xsshchannelsiter* pIterator
);



/* 返回下一活动 channel，并可选返回本端 id。 */
XRT_API xsshchannel* xrtSshChannelsIterNext(
	xsshchannelsiter* pIterator,
	uint32* pLocal
);



/* 提前结束迭代并释放结构修改保护。 */
XRT_API void xrtSshChannelsIterEnd(xsshchannelsiter* pIterator);



XRT_EXTERN_C_END

#endif

#endif
