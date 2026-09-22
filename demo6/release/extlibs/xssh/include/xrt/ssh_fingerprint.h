#ifndef XRT_SSH_FINGERPRINT_H
#define XRT_SSH_FINGERPRINT_H

#include <xrt/ssh_wire.h>



#if defined(XSSH_FEATURE_FINGERPRINT) && \
	(!defined(XSSH_FEATURE_WIRE) || \
	 !defined(XRT_FEATURE_CODEC_BASE64) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA256))
	#error "XSSH_FEATURE_FINGERPRINT requires wire, Base64 and SHA-256"
#endif



#if defined(XSSH_FEATURE_FINGERPRINT)

#define XSSH_FINGERPRINT_SHA256_SIZE 32u



XRT_EXTERN_C_BEGIN



/* 计算完整 host-key blob 的 32 字节 SHA-256 摘要。 */
XRT_API xsshcode xrtSshHostKeyDigestSha256(
	xbytesview HostKey,
	void* pDigest
);



/*
	输出 OpenSSH 风格 SHA256:<base64-no-padding> 指纹。
	查询模式允许 sOutput 为 NULL、容量为零；实际容量必须包含末尾零字节。
*/
XRT_API xsshcode xrtSshHostKeyFingerprintSha256(
	xbytesview HostKey,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



XRT_EXTERN_C_END

#endif

#endif
