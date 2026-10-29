#ifndef XRT_SSH_AUTH_HOSTBASED_H
#define XRT_SSH_AUTH_HOSTBASED_H

#include <xrt/ssh_auth_message.h>
#include <xrt/ssh_hostkey.h>



#if defined(XSSH_FEATURE_AUTH_HOSTBASED) && \
	(!defined(XSSH_FEATURE_AUTH_MESSAGE) || !defined(XSSH_FEATURE_HOSTKEY))
	#error "XSSH_FEATURE_AUTH_HOSTBASED requires auth message and hostkey"
#endif



#if defined(XSSH_FEATURE_AUTH_HOSTBASED)

#define XSSH_AUTH_HOST_NAME_MAX 254u



/* Hostbased 请求借用完整 payload。 */
typedef struct xsshauthhostbased {
	xstrview User;
	xstrview Algorithm;
	xbytesview PublicKey;
	xstrview HostName;
	xstrview ClientUser;
	xbytesview Signature;
} xsshauthhostbased;



XRT_EXTERN_C_BEGIN



/* 校验 US-ASCII DNS 主机名；允许末尾根标签点。 */
XRT_API bool xrtSshAuthHostNameValid(xstrview HostName);



/* 使用 ssh-connection 服务写入完整 hostbased 请求。 */
XRT_API xsshcode xrtSshAuthHostBasedWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Algorithm,
	xbytesview PublicKey,
	xstrview HostName,
	xstrview ClientUser,
	xbytesview Signature
);



/* 严格读取 hostbased 请求。 */
XRT_API xsshcode xrtSshAuthHostBasedRead(
	xbytesview Payload,
	xsshauthhostbased* pHostBased
);



/* 写入 RFC 4252 hostbased 签名原文，不执行签名。 */
XRT_API xsshcode xrtSshAuthHostBasedSignDataWrite(
	xsshwriter* pWriter,
	xbytesview SessionId,
	xstrview User,
	xstrview Algorithm,
	xbytesview PublicKey,
	xstrview HostName,
	xstrview ClientUser
);



XRT_EXTERN_C_END

#endif

#endif
