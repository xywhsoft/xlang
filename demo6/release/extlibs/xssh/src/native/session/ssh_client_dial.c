#include <xrt/ssh_client_dial.h>



#if defined(XSSH_FEATURE_CLIENT_DIAL)

/* 设置当前执行上下文的客户端 Dial 参数或状态错误。 */
static xnetdial* xsshClientDialError(xerrkind Kind, xsshcode Code)
{
	xrtSetErrorInfo(
		Kind,
		"xrt.ssh.client",
		(int32)Code,
		Kind == XERR_ARGUMENT ?
			"invalid SSH client dial arguments" :
			"SSH client dial requires a created client"
	);
	return NULL;
}



/* 直接复用 XRT Dial 的解析、地址竞速、截止时间、取消和所有权契约。 */
xnetdial* xrtSshClientDial(
	xsshclient* pClient,
	xnetengine* pEngine,
	xnetresolver* pResolver,
	cstr sHost,
	uint16 iPort,
	const xnetdialconfig* pConfig,
	xnetdialproc pDone,
	ptr pData
)
{
	xsshclientstate State = xrtSshClientState(pClient);
	ptr pStreamData;

	if ( State == XSSH_CLIENT_INVALID ) {
		return xsshClientDialError(
			XERR_ARGUMENT,
			XSSH_ERROR_ARGUMENT
		);
	}
	if ( State != XSSH_CLIENT_CREATED ) {
		return xsshClientDialError(XERR_STATE, XSSH_ERROR_STATE);
	}
	pStreamData = xrtSshClientNetData(pClient);
	if ( pStreamData == NULL ) {
		return xsshClientDialError(XERR_STATE, XSSH_ERROR_STATE);
	}
	return xrtNetDial(
		pEngine,
		pResolver,
		sHost,
		iPort,
		pConfig,
		xrtSshClientNetEvents(),
		pStreamData,
		pDone,
		pData
	);
}

#endif
