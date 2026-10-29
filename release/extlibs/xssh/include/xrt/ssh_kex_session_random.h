#ifndef XRT_SSH_KEX_SESSION_RANDOM_H
#define XRT_SSH_KEX_SESSION_RANDOM_H

#include <xrt/ssh_kex_curve25519_random.h>
#include <xrt/ssh_kex_session.h>



#if defined(XSSH_FEATURE_KEX_SESSION_RANDOM) && \
	(!defined(XSSH_FEATURE_KEX_SESSION) || \
	 !defined(XSSH_FEATURE_KEX_CURVE25519_RANDOM))
	#error "XSSH_FEATURE_KEX_SESSION_RANDOM requires KEX session and secure Curve25519 keypair"
#endif



#if defined(XSSH_FEATURE_KEX_SESSION_RANDOM)

XRT_EXTERN_C_BEGIN



/* 使用 XRT 系统安全随机源开始一代 Curve25519 KEX。 */
XRT_API xsshcode xrtSshKexSessionBegin(
	xsshkexsession* pSession,
	xsshtransportcore* pCore,
	const xsshkextranscript* pTranscript,
	xbytesview ServerHostKey
);



XRT_EXTERN_C_END

#endif

#endif
