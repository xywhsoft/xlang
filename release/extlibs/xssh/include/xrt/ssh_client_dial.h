#ifndef XRT_SSH_CLIENT_DIAL_H
#define XRT_SSH_CLIENT_DIAL_H

#include <xrt/ssh_client.h>
#include <xrt/tcp.h>



#if defined(XSSH_FEATURE_CLIENT_DIAL) && \
	(!defined(XSSH_FEATURE_CLIENT) || \
	 !defined(XRT_FEATURE_NET_TCP_DIAL))
	#error "XSSH_FEATURE_CLIENT_DIAL requires SSH client and XRT TCP Dial"
#endif



#if defined(XSSH_FEATURE_CLIENT_DIAL)

XRT_EXTERN_C_BEGIN



/*
	通过 XRT Resolver 和 TCP Dial 建链，并在公开 TCP Open 前安装 SSH 驱动。
	完成回调只表示 TCP Dial 终态；SSH 握手完成由 xsshclientevents.Ready 发布。
*/
XRT_API xnetdial* xrtSshClientDial(
	xsshclient* pClient,
	xnetengine* pEngine,
	xnetresolver* pResolver,
	cstr sHost,
	uint16 iPort,
	const xnetdialconfig* pConfig,
	xnetdialproc pDone,
	ptr pData
);



XRT_EXTERN_C_END

#endif

#endif
