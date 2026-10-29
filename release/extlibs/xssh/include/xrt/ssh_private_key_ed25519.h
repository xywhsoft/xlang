#ifndef XRT_SSH_PRIVATE_KEY_ED25519_H
#define XRT_SSH_PRIVATE_KEY_ED25519_H

#include <xrt/ssh_private_key.h>



#if defined(XSSH_FEATURE_PRIVATE_KEY_ED25519) && \
	(!defined(XSSH_FEATURE_PRIVATE_KEY) || \
	 !defined(XRT_FEATURE_CRYPTO_ED25519_SIGN))
	#error "XSSH_FEATURE_PRIVATE_KEY_ED25519 requires private-key and Ed25519 signing"
#endif



#if defined(XSSH_FEATURE_PRIVATE_KEY_ED25519)

/* 身份借用解码后的秘密缓冲；释放前由调用方不可消除地清零该缓冲。 */
typedef struct xsshed25519identity {
	xbytesview PublicKeyBlob;
	xbytesview Seed;
	xbytesview PublicKey;
	xbytesview Comment;
} xsshed25519identity;



XRT_EXTERN_C_BEGIN



/* 严格读取未加密、单密钥 openssh-key-v1 Ed25519 身份。 */
XRT_API xsshcode xrtSshPrivateKeyEd25519Read(
	const xsshopensshprivatekey* pPrivateKey,
	xsshed25519identity* pIdentity
);



/* 使用借用 seed 签署任意二进制消息，输出固定 64 字节原始签名。 */
XRT_API xsshcode xrtSshPrivateKeyEd25519Sign(
	const xsshed25519identity* pIdentity,
	xbytesview Message,
	void* pSignature
);



/* 签名并直接向最终 writer 写入 ssh-ed25519 signature blob。 */
XRT_API xsshcode xrtSshPrivateKeyEd25519SignatureWrite(
	xsshwriter* pWriter,
	const xsshed25519identity* pIdentity,
	xbytesview Message
);



XRT_EXTERN_C_END

#endif

#endif
