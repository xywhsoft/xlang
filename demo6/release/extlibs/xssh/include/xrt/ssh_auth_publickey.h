#ifndef XRT_SSH_AUTH_PUBLICKEY_H
#define XRT_SSH_AUTH_PUBLICKEY_H

#include <xrt/ssh_auth_message.h>
#include <xrt/ssh_hostkey.h>



#if defined(XSSH_FEATURE_AUTH_PUBLICKEY) && \
	(!defined(XSSH_FEATURE_AUTH_MESSAGE) || !defined(XSSH_FEATURE_HOSTKEY))
	#error "XSSH_FEATURE_AUTH_PUBLICKEY requires auth message and hostkey"
#endif



#if defined(XSSH_FEATURE_AUTH_PUBLICKEY)

#define XSSH_MSG_USERAUTH_PK_OK 60u



/* Publickey 请求借用完整 payload。 */
typedef struct xsshauthpublickey {
	xstrview User;
	bool HasSignature;
	xstrview Algorithm;
	xbytesview PublicKey;
	xbytesview Signature;
} xsshauthpublickey;



/* Publickey 探测成功响应借用完整 payload。 */
typedef struct xsshauthpublickeyok {
	xstrview Algorithm;
	xbytesview PublicKey;
} xsshauthpublickeyok;



XRT_EXTERN_C_BEGIN



/* 使用 ssh-connection 服务写入无签名 publickey 探测。 */
XRT_API xsshcode xrtSshAuthPublicKeyWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Algorithm,
	xbytesview PublicKey
);



/* 使用 ssh-connection 服务写入带签名 publickey 请求。 */
XRT_API xsshcode xrtSshAuthPublicKeySignedWrite(
	xsshwriter* pWriter,
	xstrview User,
	xstrview Algorithm,
	xbytesview PublicKey,
	xbytesview Signature
);



/* 严格读取 publickey 探测或带签名请求。 */
XRT_API xsshcode xrtSshAuthPublicKeyRead(
	xbytesview Payload,
	xsshauthpublickey* pPublicKey
);



/* 写入 RFC 4252 publickey 签名原文，不执行签名。 */
XRT_API xsshcode xrtSshAuthPublicKeySignDataWrite(
	xsshwriter* pWriter,
	xbytesview SessionId,
	xstrview User,
	xstrview Algorithm,
	xbytesview PublicKey
);



/* 写入或严格读取服务端 publickey 探测成功响应。 */
XRT_API xsshcode xrtSshAuthPublicKeyOkWrite(
	xsshwriter* pWriter,
	xstrview Algorithm,
	xbytesview PublicKey
);
XRT_API xsshcode xrtSshAuthPublicKeyOkRead(
	xbytesview Payload,
	xsshauthpublickeyok* pPublicKey
);



XRT_EXTERN_C_END

#endif

#endif
