#ifndef XRT_SSH_CLIENT_AUTH_ED25519_H
#define XRT_SSH_CLIENT_AUTH_ED25519_H

#include <xrt/ssh_auth_publickey.h>
#include <xrt/ssh_client_core.h>
#include <xrt/ssh_private_key_ed25519.h>



#if defined(XSSH_FEATURE_CLIENT_AUTH_ED25519) && \
	(!defined(XSSH_FEATURE_CLIENT_CORE) || \
	 !defined(XSSH_FEATURE_AUTH_PUBLICKEY) || \
	 !defined(XSSH_FEATURE_PRIVATE_KEY_ED25519))
	#error "XSSH_FEATURE_CLIENT_AUTH_ED25519 requires client core, publickey auth and Ed25519 private-key support"
#endif



#if defined(XSSH_FEATURE_CLIENT_AUTH_ED25519)

XRT_EXTERN_C_BEGIN



/* 使用借用的 Ed25519 身份直接构建带签名 publickey 请求，不执行多余的 probe 往返。 */
XRT_API xsshcode xrtSshClientEd25519Auth(
	xsshclientcore* pClient,
	xsshwriter* pWriter,
	const xsshclientauth* pAuth,
	ptr pUserData
);



XRT_EXTERN_C_END

#endif

#endif
