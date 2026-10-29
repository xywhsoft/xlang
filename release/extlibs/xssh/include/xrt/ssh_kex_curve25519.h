#ifndef XRT_SSH_KEX_CURVE25519_H
#define XRT_SSH_KEX_CURVE25519_H

#include <xrt/ssh_wire.h>
#include <xrt/crypto.h>



#if defined(XSSH_FEATURE_KEX_CURVE25519) && \
	(!defined(XSSH_FEATURE_WIRE) || !defined(XRT_FEATURE_CRYPTO_X25519))
	#error "XSSH_FEATURE_KEX_CURVE25519 requires wire and crypto_x25519"
#endif



#if defined(XSSH_FEATURE_KEX_CURVE25519)

#define XSSH_CURVE25519_PRIVATE_SIZE 32u
#define XSSH_CURVE25519_PUBLIC_SIZE 32u
#define XSSH_CURVE25519_SHARED_SIZE 32u

XRT_EXTERN_C_BEGIN



/* 从固定长度私钥导出 Curve25519 SSH 临时公钥。 */
XRT_API xsshcode xrtSshCurve25519Public(
	const void* pPrivate,
	void* pPublic
);



/* 计算共享秘密并拒绝低阶公钥产生的全零结果。 */
XRT_API xsshcode xrtSshCurve25519Shared(
	const void* pPrivate,
	const void* pPeerPublic,
	void* pShared
);



XRT_EXTERN_C_END

#endif

#endif
