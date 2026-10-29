#ifndef XRT_SSH_HOSTKEY_ED25519_H
#define XRT_SSH_HOSTKEY_ED25519_H

#include <xrt/ssh_hostkey.h>
#include <xrt/crypto.h>



#if defined(XSSH_FEATURE_HOSTKEY_ED25519) && \
	(!defined(XSSH_FEATURE_HOSTKEY) || \
	 !defined(XRT_FEATURE_CRYPTO_ED25519_VERIFY))
	#error "XSSH_FEATURE_HOSTKEY_ED25519 requires hostkey and crypto_ed25519_verify"
#endif



#if defined(XSSH_FEATURE_HOSTKEY_ED25519)

XRT_EXTERN_C_BEGIN



/* 验证 ssh-ed25519 主机密钥对 Message 的签名。 */
XRT_API xsshcode xrtSshEd25519HostKeyVerify(
	xbytesview PublicKeyBlob,
	xbytesview SignatureBlob,
	xbytesview Message
);



XRT_EXTERN_C_END

#endif

#endif
