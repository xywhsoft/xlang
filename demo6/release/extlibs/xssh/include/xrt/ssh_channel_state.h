#ifndef XRT_SSH_CHANNEL_STATE_H
#define XRT_SSH_CHANNEL_STATE_H

#include <xrt/ssh_wire.h>



#if defined(XSSH_FEATURE_CHANNEL_STATE) && !defined(XSSH_FEATURE_WIRE)
	#error "XSSH_FEATURE_CHANNEL_STATE requires XSSH_FEATURE_WIRE"
#endif



#if defined(XSSH_FEATURE_CHANNEL_STATE)

/* Channel 生命周期只记录双向 EOF/CLOSE，不拥有网络和缓冲。 */
typedef struct xsshchannelstate {
	bool LocalEof;
	bool RemoteEof;
	bool LocalClose;
	bool RemoteClose;
	bool Initialized;
} xsshchannelstate;



XRT_EXTERN_C_BEGIN



/* 初始化已经完成 open confirmation 的 channel 生命周期。 */
XRT_API bool xrtSshChannelStateInit(xsshchannelstate* pState);



/* 判断本端是否仍可发送 data 或 extended-data。 */
XRT_API bool xrtSshChannelCanSendData(const xsshchannelstate* pState);



/* 判断远端 data 或 extended-data 是否仍可被接受。 */
XRT_API bool xrtSshChannelCanReceiveData(const xsshchannelstate* pState);



/* 判断本端是否仍可发送 channel request。 */
XRT_API bool xrtSshChannelCanSendRequest(const xsshchannelstate* pState);



/* 判断收到远端 close 后是否仍需排队本端 close。 */
XRT_API bool xrtSshChannelCloseReplyNeeded(const xsshchannelstate* pState);



/* 判断双向 close 握手是否完成，可以回收 channel slot。 */
XRT_API bool xrtSshChannelClosed(const xsshchannelstate* pState);



/* 在本端 EOF 已可靠排队后提交单向发送结束。 */
XRT_API xsshcode xrtSshChannelLocalEofCommit(xsshchannelstate* pState);



/* 接收远端 EOF；重复或 close 后 EOF 是协议错误。 */
XRT_API xsshcode xrtSshChannelRemoteEofCommit(xsshchannelstate* pState);



/* 在本端 close 已可靠排队后提交本端关闭。 */
XRT_API xsshcode xrtSshChannelLocalCloseCommit(xsshchannelstate* pState);



/* 接收远端 close；调用方随后按需回复 close。 */
XRT_API xsshcode xrtSshChannelRemoteCloseCommit(xsshchannelstate* pState);



XRT_EXTERN_C_END

#endif

#endif
