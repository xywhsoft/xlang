#ifndef XRT_SSH_KEX_CURVE25519_RANDOM_H
#define XRT_SSH_KEX_CURVE25519_RANDOM_H

#include <xrt/ssh_kex_curve25519.h>



#if defined(XSSH_FEATURE_KEX_CURVE25519_RANDOM) && \
	(!defined(XSSH_FEATURE_KEX_CURVE25519) || \
	 !defined(XRT_FEATURE_CRYPTO_X25519_KEYPAIR))
	#error "XSSH_FEATURE_KEX_CURVE25519_RANDOM requires curve25519 and X25519 keypair"
#endif



#if defined(XSSH_FEATURE_KEX_CURVE25519_RANDOM)

XRT_EXTERN_C_BEGIN



/* 使用操作系统安全随机源生成 Curve25519 SSH 临时密钥对。 */
XRT_API xsshcode xrtSshCurve25519KeyPair(
	void* pPrivate,
	void* pPublic
);



XRT_EXTERN_C_END

#endif

#endif
