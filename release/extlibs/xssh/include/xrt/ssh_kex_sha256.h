#ifndef XRT_SSH_KEX_SHA256_H
#define XRT_SSH_KEX_SHA256_H

#include <xrt/ssh_wire.h>
#include <xrt/crypto.h>



#if defined(XSSH_FEATURE_KEX_SHA256) && \
	(!defined(XSSH_FEATURE_WIRE) || !defined(XRT_FEATURE_CRYPTO_SHA256))
	#error "XSSH_FEATURE_KEX_SHA256 requires wire and crypto_sha256"
#endif



#if defined(XSSH_FEATURE_KEX_SHA256)

#define XSSH_SHA256_SIZE 32u



/* Curve25519/ECDH exchange hash 输入均为未带外层 packet framing 的协议值。 */
typedef struct xsshkexhashsha256 {
	xbytesview ClientVersion;
	xbytesview ServerVersion;
	xbytesview ClientKexInit;
	xbytesview ServerKexInit;
	xbytesview ServerHostKey;
	xbytesview ClientEphemeral;
	xbytesview ServerEphemeral;
	xbytesview SharedSecret;
} xsshkexhashsha256;



XRT_EXTERN_C_BEGIN



/* 按 RFC 5656/8731 顺序计算 Curve25519 SHA-256 exchange hash。 */
XRT_API xsshcode xrtSshKexHashSha256(
	const xsshkexhashsha256* pInput,
	void* pHash
);



/* 按 RFC 4253 扩展 A-F 类密钥材料，输出可大于单个摘要。 */
XRT_API xsshcode xrtSshKexDeriveSha256(
	void* pOutput,
	size_t iOutputSize,
	xbytesview SharedSecret,
	const void* pExchangeHash,
	const void* pSessionId,
	uint8 iLetter
);



XRT_EXTERN_C_END

#endif

#endif
