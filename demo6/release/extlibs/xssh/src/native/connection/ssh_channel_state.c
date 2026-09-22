#include <xrt/ssh_channel_state.h>



#if defined(XSSH_FEATURE_CHANNEL_STATE)

/* 判断生命周期对象已经初始化。 */
static bool xsshChannelStateValid(const xsshchannelstate* pState)
{
	return (pState != NULL) && pState->Initialized;
}



/* 初始化打开状态。 */
bool xrtSshChannelStateInit(xsshchannelstate* pState)
{
	xsshchannelstate State;

	if ( pState == NULL ) {
		return false;
	}
	State.LocalEof = false;
	State.RemoteEof = false;
	State.LocalClose = false;
	State.RemoteClose = false;
	State.Initialized = true;
	*pState = State;
	return true;
}



/* EOF 只停止本方向 data；任一 close 都停止新增发送。 */
bool xrtSshChannelCanSendData(const xsshchannelstate* pState)
{
	return xsshChannelStateValid(pState) && !pState->LocalEof &&
		!pState->LocalClose && !pState->RemoteClose;
}



/* 本端 close 后仍允许处理此前在途数据，直到远端 EOF/CLOSE。 */
bool xrtSshChannelCanReceiveData(const xsshchannelstate* pState)
{
	return xsshChannelStateValid(pState) && !pState->RemoteEof &&
		!pState->RemoteClose;
}



/* EOF 不禁止控制 request，close 才终止新的 request。 */
bool xrtSshChannelCanSendRequest(const xsshchannelstate* pState)
{
	return xsshChannelStateValid(pState) && !pState->LocalClose &&
		!pState->RemoteClose;
}



/* 远端先 close 时必须补发一次本端 close。 */
bool xrtSshChannelCloseReplyNeeded(const xsshchannelstate* pState)
{
	return xsshChannelStateValid(pState) && pState->RemoteClose &&
		!pState->LocalClose;
}



/* 只有双向 close 都已提交才能回收 channel。 */
bool xrtSshChannelClosed(const xsshchannelstate* pState)
{
	return xsshChannelStateValid(pState) && pState->LocalClose &&
		pState->RemoteClose;
}



/* 提交本端 EOF。 */
xsshcode xrtSshChannelLocalEofCommit(xsshchannelstate* pState)
{
	if ( !xsshChannelStateValid(pState) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pState->LocalEof || pState->LocalClose || pState->RemoteClose ) {
		return XSSH_ERROR_STATE;
	}
	pState->LocalEof = true;
	return XSSH_OK;
}



/* 提交远端 EOF。 */
xsshcode xrtSshChannelRemoteEofCommit(xsshchannelstate* pState)
{
	if ( !xsshChannelStateValid(pState) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pState->RemoteEof || pState->RemoteClose ) {
		return XSSH_ERROR_PROTOCOL;
	}
	pState->RemoteEof = true;
	return XSSH_OK;
}



/* 提交本端 close；close 隐含本方向不再发送任何数据。 */
xsshcode xrtSshChannelLocalCloseCommit(xsshchannelstate* pState)
{
	if ( !xsshChannelStateValid(pState) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pState->LocalClose ) {
		return XSSH_ERROR_STATE;
	}
	pState->LocalClose = true;
	return XSSH_OK;
}



/* 提交远端 close；重复 close 是线路协议错误。 */
xsshcode xrtSshChannelRemoteCloseCommit(xsshchannelstate* pState)
{
	if ( !xsshChannelStateValid(pState) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pState->RemoteClose ) {
		return XSSH_ERROR_PROTOCOL;
	}
	pState->RemoteClose = true;
	return XSSH_OK;
}

#endif
