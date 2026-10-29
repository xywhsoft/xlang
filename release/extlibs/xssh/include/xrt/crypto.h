#ifndef XRT_CRYPTO_H
#define XRT_CRYPTO_H

#include <xrt/core.h>
#include <xrt/memory.h>



#if defined(XRT_FEATURE_CRYPTO_SHA224) && \
	!defined(XRT_FEATURE_CRYPTO_SHA256)
	#error "XRT SHA-224 support requires XRT_FEATURE_CRYPTO_SHA256"
#endif

#if defined(XRT_FEATURE_CRYPTO_INT31) && !defined(XRT_FEATURE_CRYPTO_CORE)
	#error "XRT int31 support requires XRT_FEATURE_CRYPTO_CORE"
#endif

#if defined(XRT_FEATURE_CRYPTO_NIST) && !defined(XRT_FEATURE_CRYPTO_INT31)
	#error "XRT NIST curve support requires XRT_FEATURE_CRYPTO_INT31"
#endif

#if defined(XRT_FEATURE_CRYPTO_ECDSA_CORE) && !defined(XRT_FEATURE_CRYPTO_CORE)
	#error "XRT ECDSA core support requires XRT_FEATURE_CRYPTO_CORE"
#endif

#if defined(XRT_FEATURE_CRYPTO_ECDSA_DER) && \
	!defined(XRT_FEATURE_CRYPTO_ECDSA_CORE)
	#error "XRT ECDSA DER support requires XRT_FEATURE_CRYPTO_ECDSA_CORE"
#endif

#if defined(XRT_FEATURE_CRYPTO_ECDSA_MATH) && \
	(!defined(XRT_FEATURE_CRYPTO_ECDSA_CORE) || \
	 !defined(XRT_FEATURE_CRYPTO_NIST))
	#error "XRT ECDSA math requires ECDSA core and NIST curves"
#endif

#if defined(XRT_FEATURE_CRYPTO_ECDSA_VERIFY) && \
	!defined(XRT_FEATURE_CRYPTO_ECDSA_MATH)
	#error "XRT ECDSA verification requires XRT_FEATURE_CRYPTO_ECDSA_MATH"
#endif

#if defined(XRT_FEATURE_CRYPTO_ECDSA_P256) && \
	(!defined(XRT_FEATURE_CRYPTO_ECDSA_VERIFY) || \
	 !defined(XRT_FEATURE_CRYPTO_P256))
	#error "XRT P-256 ECDSA requires ECDSA verification and P-256"
#endif

#if defined(XRT_FEATURE_CRYPTO_ECDSA_P384) && \
	(!defined(XRT_FEATURE_CRYPTO_ECDSA_VERIFY) || \
	 !defined(XRT_FEATURE_CRYPTO_P384))
	#error "XRT P-384 ECDSA requires ECDSA verification and P-384"
#endif

#if defined(XRT_FEATURE_CRYPTO_ECDSA_SIGN) && \
	!defined(XRT_FEATURE_CRYPTO_ECDSA_MATH)
	#error "XRT ECDSA signing requires XRT_FEATURE_CRYPTO_ECDSA_MATH"
#endif

#if defined(XRT_FEATURE_CRYPTO_ECDSA_P256_SIGN) && \
	(!defined(XRT_FEATURE_CRYPTO_ECDSA_SIGN) || \
	 !defined(XRT_FEATURE_CRYPTO_P256) || \
	 !defined(XRT_FEATURE_CRYPTO_HMAC_SHA256))
	#error "XRT P-256 ECDSA signing requires ECDSA sign, P-256 and HMAC-SHA256"
#endif

#if defined(XRT_FEATURE_CRYPTO_ECDSA_P384_SIGN) && \
	(!defined(XRT_FEATURE_CRYPTO_ECDSA_SIGN) || \
	 !defined(XRT_FEATURE_CRYPTO_P384) || \
	 !defined(XRT_FEATURE_CRYPTO_HMAC_SHA512))
	#error "XRT P-384 ECDSA signing requires ECDSA sign, P-384 and HMAC-SHA384"
#endif

#if defined(XRT_FEATURE_CRYPTO_ECDSA_SIGN_DER) && \
	(!defined(XRT_FEATURE_CRYPTO_ECDSA_SIGN) || \
	 !defined(XRT_FEATURE_CRYPTO_ECDSA_DER))
	#error "XRT ECDSA DER signing requires raw signing and ECDSA DER"
#endif

#if defined(XRT_FEATURE_CRYPTO_ECDSA_P256_SIGN_DER) && \
	(!defined(XRT_FEATURE_CRYPTO_ECDSA_P256_SIGN) || \
	 !defined(XRT_FEATURE_CRYPTO_ECDSA_SIGN_DER))
	#error "XRT P-256 ECDSA DER signing requires P-256 signing and DER signing"
#endif

#if defined(XRT_FEATURE_CRYPTO_ECDSA_P384_SIGN_DER) && \
	(!defined(XRT_FEATURE_CRYPTO_ECDSA_P384_SIGN) || \
	 !defined(XRT_FEATURE_CRYPTO_ECDSA_SIGN_DER))
	#error "XRT P-384 ECDSA DER signing requires P-384 signing and DER signing"
#endif

#if defined(XRT_FEATURE_CRYPTO_ECDSA_VERIFY_DER) && \
	(!defined(XRT_FEATURE_CRYPTO_ECDSA_VERIFY) || \
	 !defined(XRT_FEATURE_CRYPTO_ECDSA_DER))
	#error "XRT ECDSA DER verification requires raw verification and ECDSA DER"
#endif

#if defined(XRT_FEATURE_CRYPTO_RSA) && \
	(!defined(XRT_FEATURE_CRYPTO_CORE) || \
	 !defined(XRT_FEATURE_CRYPTO_INT31))
	#error "XRT RSA support requires crypto core and int31"
#endif

#if defined(XRT_FEATURE_CRYPTO_RSA_PRIVATE) && \
	!defined(XRT_FEATURE_CRYPTO_RSA)
	#error "XRT RSA private operations require RSA"
#endif

#if defined(XRT_FEATURE_CRYPTO_RSA_PSS) && \
	(!defined(XRT_FEATURE_CRYPTO_RSA) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA1) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA224) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA256) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA512))
	#error "XRT RSA-PSS requires RSA and SHA-1/SHA-2 including SHA-224"
#endif

#if defined(XRT_FEATURE_CRYPTO_RSA_PSS_SIGN) && \
	(!defined(XRT_FEATURE_CRYPTO_RSA_PSS) || \
	 !defined(XRT_FEATURE_CRYPTO_RSA_PRIVATE) || \
	 !defined(XRT_FEATURE_RANDOM_SECURE))
	#error "XRT RSA-PSS signing requires PSS, RSA private operations and secure random"
#endif

#if defined(XRT_FEATURE_CRYPTO_RSA_PKCS1) && \
	!defined(XRT_FEATURE_CRYPTO_RSA)
	#error "XRT RSA PKCS#1 verification requires RSA"
#endif

#if defined(XRT_FEATURE_CRYPTO_RSA_PKCS1_SIGN) && \
	(!defined(XRT_FEATURE_CRYPTO_RSA_PKCS1) || \
	 !defined(XRT_FEATURE_CRYPTO_RSA_PRIVATE))
	#error "XRT RSA PKCS#1 signing requires PKCS#1 and RSA private operations"
#endif

#if defined(XRT_FEATURE_CRYPTO_ECDSA_P256_DER) && \
	(!defined(XRT_FEATURE_CRYPTO_ECDSA_P256) || \
	 !defined(XRT_FEATURE_CRYPTO_ECDSA_VERIFY_DER))
	#error "XRT P-256 ECDSA DER requires P-256 ECDSA DER verification"
#endif

#if defined(XRT_FEATURE_CRYPTO_ECDSA_P384_DER) && \
	(!defined(XRT_FEATURE_CRYPTO_ECDSA_P384) || \
	 !defined(XRT_FEATURE_CRYPTO_ECDSA_VERIFY_DER))
	#error "XRT P-384 ECDSA DER requires P-384 ECDSA DER verification"
#endif

#if defined(XRT_FEATURE_CRYPTO_P256) && !defined(XRT_FEATURE_CRYPTO_NIST)
	#error "XRT P-256 support requires XRT_FEATURE_CRYPTO_NIST"
#endif

#if defined(XRT_FEATURE_CRYPTO_P384) && !defined(XRT_FEATURE_CRYPTO_NIST)
	#error "XRT P-384 support requires XRT_FEATURE_CRYPTO_NIST"
#endif

#if defined(XRT_FEATURE_CRYPTO_NIST_KEYPAIR) && \
	(!defined(XRT_FEATURE_CRYPTO_NIST) || !defined(XRT_FEATURE_RANDOM_SECURE))
	#error "XRT NIST key-pair support requires NIST curves and cryptographic random"
#endif

#if defined(XRT_FEATURE_CRYPTO_P256_KEYPAIR) && \
	(!defined(XRT_FEATURE_CRYPTO_P256) || !defined(XRT_FEATURE_CRYPTO_NIST_KEYPAIR))
	#error "XRT P-256 key-pair support requires P-256 and NIST key-pair support"
#endif

#if defined(XRT_FEATURE_CRYPTO_P384_KEYPAIR) && \
	(!defined(XRT_FEATURE_CRYPTO_P384) || !defined(XRT_FEATURE_CRYPTO_NIST_KEYPAIR))
	#error "XRT P-384 key-pair support requires P-384 and NIST key-pair support"
#endif

#if defined(XRT_FEATURE_CRYPTO_SHA1) && !defined(XRT_FEATURE_CRYPTO_CORE)
	#error "XRT SHA-1 support requires XRT_FEATURE_CRYPTO_CORE"
#endif

#if defined(XRT_FEATURE_CRYPTO_MD5) && !defined(XRT_FEATURE_CRYPTO_CORE)
	#error "XRT MD5 support requires XRT_FEATURE_CRYPTO_CORE"
#endif

#if defined(XRT_FEATURE_CRYPTO_SHA256) && !defined(XRT_FEATURE_CRYPTO_CORE)
	#error "XRT SHA-256 support requires XRT_FEATURE_CRYPTO_CORE"
#endif

#if defined(XRT_FEATURE_CRYPTO_SHA512) && !defined(XRT_FEATURE_CRYPTO_CORE)
	#error "XRT SHA-384/SHA-512 support requires XRT_FEATURE_CRYPTO_CORE"
#endif

#if defined(XRT_FEATURE_CRYPTO_SHA512_256) && \
	!defined(XRT_FEATURE_CRYPTO_SHA512)
	#error "XRT SHA-512/256 support requires XRT_FEATURE_CRYPTO_SHA512"
#endif

#if defined(XRT_FEATURE_CRYPTO_HMAC_SHA256) && !defined(XRT_FEATURE_CRYPTO_SHA256)
	#error "XRT HMAC-SHA256 support requires XRT_FEATURE_CRYPTO_SHA256"
#endif

#if defined(XRT_FEATURE_CRYPTO_HMAC_SHA512) && !defined(XRT_FEATURE_CRYPTO_SHA512)
	#error "XRT HMAC-SHA384/SHA512 support requires XRT_FEATURE_CRYPTO_SHA512"
#endif

#if defined(XRT_FEATURE_CRYPTO_PBKDF2_SHA256) && \
	!defined(XRT_FEATURE_CRYPTO_HMAC_SHA256)
	#error "XRT PBKDF2-SHA256 support requires XRT_FEATURE_CRYPTO_HMAC_SHA256"
#endif

#if defined(XRT_FEATURE_CRYPTO_PBKDF2_SHA512) && \
	!defined(XRT_FEATURE_CRYPTO_HMAC_SHA512)
	#error "XRT PBKDF2-SHA384/SHA512 support requires XRT_FEATURE_CRYPTO_HMAC_SHA512"
#endif

#if defined(XRT_FEATURE_CRYPTO_HKDF_SHA256) && !defined(XRT_FEATURE_CRYPTO_HMAC_SHA256)
	#error "XRT HKDF-SHA256 support requires XRT_FEATURE_CRYPTO_HMAC_SHA256"
#endif

#if defined(XRT_FEATURE_CRYPTO_HKDF_SHA512) && !defined(XRT_FEATURE_CRYPTO_HMAC_SHA512)
	#error "XRT HKDF-SHA384/SHA512 support requires XRT_FEATURE_CRYPTO_HMAC_SHA512"
#endif

#if defined(XRT_FEATURE_CRYPTO_CHACHA20) && !defined(XRT_FEATURE_CRYPTO_CORE)
	#error "XRT ChaCha20 support requires XRT_FEATURE_CRYPTO_CORE"
#endif

#if defined(XRT_FEATURE_CRYPTO_POLY1305) && !defined(XRT_FEATURE_CRYPTO_CORE)
	#error "XRT Poly1305 support requires XRT_FEATURE_CRYPTO_CORE"
#endif

#if defined(XRT_FEATURE_CRYPTO_CHACHA20_POLY1305) && \
	(!defined(XRT_FEATURE_CRYPTO_CHACHA20) || !defined(XRT_FEATURE_CRYPTO_POLY1305))
	#error "XRT ChaCha20-Poly1305 support requires ChaCha20 and Poly1305"
#endif

#if defined(XRT_FEATURE_CRYPTO_AES) && !defined(XRT_FEATURE_CRYPTO_CORE)
	#error "XRT AES support requires XRT_FEATURE_CRYPTO_CORE"
#endif

#if defined(XRT_FEATURE_CRYPTO_AES_GCM) && !defined(XRT_FEATURE_CRYPTO_AES)
	#error "XRT AES-GCM support requires XRT_FEATURE_CRYPTO_AES"
#endif

#if defined(XRT_FEATURE_CRYPTO_CURVE25519) && !defined(XRT_FEATURE_CRYPTO_CORE)
	#error "XRT Curve25519 arithmetic requires XRT_FEATURE_CRYPTO_CORE"
#endif

#if defined(XRT_FEATURE_CRYPTO_X25519) && \
	!defined(XRT_FEATURE_CRYPTO_CURVE25519)
	#error "XRT X25519 support requires XRT_FEATURE_CRYPTO_CURVE25519"
#endif

#if defined(XRT_FEATURE_CRYPTO_X25519_KEYPAIR) && \
	(!defined(XRT_FEATURE_CRYPTO_X25519) || !defined(XRT_FEATURE_RANDOM_SECURE))
	#error "XRT X25519 key-pair support requires X25519 and cryptographic random"
#endif

#if defined(XRT_FEATURE_CRYPTO_ED25519) && \
	(!defined(XRT_FEATURE_CRYPTO_CURVE25519) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA512))
	#error "XRT Ed25519 support requires Curve25519 arithmetic and SHA-512"
#endif

#if defined(XRT_FEATURE_CRYPTO_ED25519_SIGN) && \
	!defined(XRT_FEATURE_CRYPTO_ED25519)
	#error "XRT Ed25519 signing requires XRT_FEATURE_CRYPTO_ED25519"
#endif

#if defined(XRT_FEATURE_CRYPTO_ED25519_VERIFY) && \
	!defined(XRT_FEATURE_CRYPTO_ED25519)
	#error "XRT Ed25519 verification requires XRT_FEATURE_CRYPTO_ED25519"
#endif

#if defined(XRT_FEATURE_CRYPTO_ED25519_KEYPAIR) && \
	(!defined(XRT_FEATURE_CRYPTO_ED25519) || \
	 !defined(XRT_FEATURE_RANDOM_SECURE))
	#error "XRT Ed25519 key-pair support requires Ed25519 and cryptographic random"
#endif

#if defined(XRT_FEATURE_CRYPTO_X448) && !defined(XRT_FEATURE_CRYPTO_CORE)
	#error "XRT X448 support requires XRT_FEATURE_CRYPTO_CORE"
#endif

#if defined(XRT_FEATURE_CRYPTO_X448_KEYPAIR) && \
	(!defined(XRT_FEATURE_CRYPTO_X448) || !defined(XRT_FEATURE_RANDOM_SECURE))
	#error "XRT X448 key-pair support requires X448 and cryptographic random"
#endif



#if defined(XRT_FEATURE_CRYPTO_CHACHA20_POLY1305) || \
	defined(XRT_FEATURE_CRYPTO_AES_GCM)

/* AEAD 认证标签与密文不匹配。 */
#define XCRYPTO_ERROR_AUTHENTICATION 2

#endif

#if defined(XRT_FEATURE_CRYPTO_X25519) || defined(XRT_FEATURE_CRYPTO_X448) || \
	defined(XRT_FEATURE_CRYPTO_P256) || defined(XRT_FEATURE_CRYPTO_P384)

/* 对端 Montgomery 曲线公钥不能形成有效的非零共享秘密。 */
#define XCRYPTO_ERROR_KEY_AGREEMENT 3

#endif

#if defined(XRT_FEATURE_CRYPTO_NIST) || defined(XRT_FEATURE_CRYPTO_RSA) || \
	defined(XRT_FEATURE_CRYPTO_ED25519)

/* 私钥标量、公钥编码或曲线点不合法。 */
#define XCRYPTO_ERROR_KEY 4

#endif

#if defined(XRT_FEATURE_CRYPTO_ECDSA_CORE) || \
	defined(XRT_FEATURE_CRYPTO_RSA_PSS) || \
	defined(XRT_FEATURE_CRYPTO_RSA_PKCS1) || \
	defined(XRT_FEATURE_CRYPTO_ED25519_SIGN) || \
	defined(XRT_FEATURE_CRYPTO_ED25519_VERIFY)

/* ECDSA 签名、签名编码或签名验证失败。 */
#define XCRYPTO_ERROR_SIGNATURE 5

#endif



#if defined(XRT_FEATURE_CRYPTO_CORE)

/* 密码协议中允许公开选择的摘要算法。 */
typedef enum xcrypto_hash {
	XCRYPTO_HASH_SHA1 = 1,
	XCRYPTO_HASH_SHA224,
	XCRYPTO_HASH_SHA256,
	XCRYPTO_HASH_SHA384,
	XCRYPTO_HASH_SHA512,
	XCRYPTO_HASH_SHA512_256,
	XCRYPTO_HASH_MD5
} xcryptohash;

#define XRT_MD5_SIZE 16u
#define XRT_SHA1_SIZE 20u
#define XRT_SHA224_SIZE 28u
#define XRT_SHA256_SIZE 32u
#define XRT_SHA384_SIZE 48u
#define XRT_SHA512_SIZE 64u
#define XRT_SHA512_256_SIZE 32u



/* 返回标准摘要算法的固定输出长度，未知算法返回零且不设置错误。 */
XRT_API size_t xrtCryptoHashSize(xcryptohash Hash);

#endif



#if defined(XRT_FEATURE_CRYPTO_RSA)

#define XRT_RSA_MODULUS_MIN_SIZE 128u
#define XRT_RSA_MODULUS_MAX_SIZE 1024u

/* RSA 公钥是对调用方持有的定宽大端模数和指数的只读视图。 */
typedef struct xrsa_public_key {
	const void* Modulus;
	size_t ModulusSize;
	const void* Exponent;
	size_t ExponentSize;
} xrsapublickey;

#endif



#if defined(XRT_FEATURE_CRYPTO_RSA_PRIVATE)

/*
	RSA 私钥是调用方持有字节的只读视图。
	完整 CRT 五参数存在时优先使用 CRT；否则必须提供完整私有指数。
*/
typedef struct xrsa_private_key {
	xrsapublickey Public;
	const void* PrivateExponent;
	size_t PrivateExponentSize;
	const void* Prime1;
	size_t Prime1Size;
	const void* Prime2;
	size_t Prime2Size;
	const void* Exponent1;
	size_t Exponent1Size;
	const void* Exponent2;
	size_t Exponent2Size;
	const void* Coefficient;
	size_t CoefficientSize;
} xrsaprivatekey;

#endif



#if defined(XRT_FEATURE_CRYPTO_RSA_PSS)

/* 接受编码中实际携带的任意非负 PSS 盐长度。 */
#define XRT_RSA_PSS_SALT_ANY SIZE_MAX

#endif



#if defined(XRT_FEATURE_CRYPTO_AES)

#define XRT_AES_BLOCK_SIZE 16u
#define XRT_AES128_KEY_SIZE 16u
#define XRT_AES192_KEY_SIZE 24u
#define XRT_AES256_KEY_SIZE 32u
#define XRT_AES_MAX_ROUND_KEY_SIZE 240u

/* AES 状态由调用方持有；RoundKey 保存标准正向轮密钥，Backend 仅供实现选择后端。 */
typedef struct xaes {
	uint8 RoundKey[XRT_AES_MAX_ROUND_KEY_SIZE];
	uint32 Guard;
	uint32 Rounds;
	uint32 Backend;
} xaes;

#endif



#if defined(XRT_FEATURE_CRYPTO_AES_GCM)

#define XRT_AES_GCM_TAG_MIN_SIZE 4u
#define XRT_AES_GCM_TAG_MAX_SIZE 16u
#define XRT_AES_GCM_TAG_DEFAULT_SIZE 16u
#define XRT_AES_GCM_NONCE_DEFAULT_SIZE 12u
#define XRT_AES_GCM_MAX_SIZE UINT64_C(68719476704)

/* AES-GCM 状态固定绑定一个 AES 密钥和标签长度，可供多个线程只读并发使用。 */
typedef struct xaesgcm {
	xaes Cipher;
	uint8 Hash[XRT_AES_BLOCK_SIZE];
	uint32 Guard;
	uint32 TagSize;
} xaesgcm;

#endif



#if defined(XRT_FEATURE_CRYPTO_MD5)

#define XRT_MD5_BLOCK_SIZE 64u

/* MD5 流状态由调用方持有；仅用于必须兼容 MD5 的历史协议。 */
typedef struct xmd5 {
	uint32 State[4];
	uint64 Size;
	uint8 Buffer[XRT_MD5_BLOCK_SIZE];
	uint32 Guard;
	uint32 BufferSize;
} xmd5;

#endif



#if defined(XRT_FEATURE_CRYPTO_SHA1)

#define XRT_SHA1_BLOCK_SIZE 64u

/* SHA-1 流状态由调用方持有；字段公开只用于无分配存储。 */
typedef struct xsha1 {
	uint32 State[5];
	uint64 Size;
	uint8 Buffer[XRT_SHA1_BLOCK_SIZE];
	uint32 Guard;
	uint32 BufferSize;
} xsha1;

#endif



#if defined(XRT_FEATURE_CRYPTO_SHA256)

#define XRT_SHA256_BLOCK_SIZE 64u

/* SHA-256 流状态由调用方持有；字段公开只用于无分配存储。 */
typedef struct xsha256 {
	uint32 State[8];
	uint64 Size;
	uint8 Buffer[XRT_SHA256_BLOCK_SIZE];
	uint32 Guard;
	uint32 BufferSize;
} xsha256;

#endif



#if defined(XRT_FEATURE_CRYPTO_SHA224)

#define XRT_SHA224_BLOCK_SIZE XRT_SHA256_BLOCK_SIZE

/* SHA-224 与 SHA-256 共享状态布局，但初始化标记严格区分算法。 */
typedef xsha256 xsha224;

#endif



#if defined(XRT_FEATURE_CRYPTO_SHA512)

#define XRT_SHA384_BLOCK_SIZE 128u
#define XRT_SHA512_BLOCK_SIZE 128u

/* SHA-384/512 共享压缩状态布局；Guard 区分具体算法。 */
typedef struct xsha512 {
	uint64 State[8];
	uint64 SizeLow;
	uint64 SizeHigh;
	uint8 Buffer[XRT_SHA512_BLOCK_SIZE];
	uint32 Guard;
	uint32 BufferSize;
} xsha512;

typedef xsha512 xsha384;

#endif



#if defined(XRT_FEATURE_CRYPTO_SHA512_256)

#define XRT_SHA512_256_BLOCK_SIZE XRT_SHA512_BLOCK_SIZE

/* SHA-512/256 复用 SHA-512 状态布局，但使用独立初始向量和状态标记。 */
typedef xsha512 xsha512_256;

#endif



#if defined(XRT_FEATURE_CRYPTO_HMAC_SHA256)

/* HMAC-SHA256 保存预计算的 inner/outer 摘要状态。 */
typedef struct xhmacsha256 {
	xsha256 Inner;
	xsha256 Outer;
	uint32 Guard;
} xhmacsha256;

#endif



#if defined(XRT_FEATURE_CRYPTO_HMAC_SHA512)

/* HMAC-SHA384/512 共享状态布局；Guard 区分具体算法。 */
typedef struct xhmacsha512 {
	xsha512 Inner;
	xsha512 Outer;
	uint32 Guard;
} xhmacsha512;

typedef xhmacsha512 xhmacsha384;

#endif



#if defined(XRT_FEATURE_CRYPTO_AES)

/* 使用 16、24 或 32 字节密钥初始化 AES-128、AES-192 或 AES-256。 */
XRT_API bool xrtAesInit(xaes* pState, const void* pKey, size_t iKeySize);



/* 清除 AES 轮密钥；空指针视为空操作。 */
XRT_API void xrtAesClear(xaes* pState);



/* 加密一个 16 字节块；输入输出可完全相同，不允许部分重叠。 */
XRT_API bool xrtAesEncrypt(
	const xaes* pState,
	const void* pInput,
	void* pOutput
);



/* 解密一个 16 字节块；输入输出可完全相同，不允许部分重叠。 */
XRT_API bool xrtAesDecrypt(
	const xaes* pState,
	const void* pInput,
	void* pOutput
);

#endif



#if defined(XRT_FEATURE_CRYPTO_AES_GCM)

/* 初始化 AES-GCM，并把 NIST 支持的固定标签长度绑定到该密钥状态。 */
XRT_API bool xrtAesGcmInit(
	xaesgcm* pState,
	const void* pKey,
	size_t iKeySize,
	size_t iTagSize
);



/* 清除 AES-GCM 密钥、哈希子密钥及状态；空指针视为空操作。 */
XRT_API void xrtAesGcmClear(xaesgcm* pState);



/* 返回状态绑定的标签长度；无效状态返回 0 并设置错误。 */
XRT_API size_t xrtAesGcmTagSize(const xaesgcm* pState);



/* 加密并把密文和固定长度认证标签写入分离输出。 */
XRT_API bool xrtAesGcmEncrypt(
	const xaesgcm* pState,
	const void* pNonce,
	size_t iNonceSize,
	const void* pAad,
	size_t iAadSize,
	const void* pPlain,
	size_t iPlainSize,
	void* pCipher,
	void* pTag
);



/* 验证分离标签后解密；认证失败时不修改明文输出。 */
XRT_API bool xrtAesGcmDecrypt(
	const xaesgcm* pState,
	const void* pNonce,
	size_t iNonceSize,
	const void* pAad,
	size_t iAadSize,
	const void* pCipher,
	size_t iCipherSize,
	const void* pTag,
	void* pPlain
);



/* 加密为 cipher || tag；输出容量至少为明文长度加状态标签长度。 */
XRT_API bool xrtAesGcmSeal(
	const xaesgcm* pState,
	const void* pNonce,
	size_t iNonceSize,
	const void* pAad,
	size_t iAadSize,
	const void* pPlain,
	size_t iPlainSize,
	void* pOutput,
	size_t iOutputSize
);



/* 打开 cipher || tag；认证失败时不修改明文输出。 */
XRT_API bool xrtAesGcmOpen(
	const xaesgcm* pState,
	const void* pNonce,
	size_t iNonceSize,
	const void* pAad,
	size_t iAadSize,
	const void* pInput,
	size_t iInputSize,
	void* pPlain,
	size_t iPlainSize
);



/* 以 GMAC 模式认证一段不加密的数据。 */
XRT_API bool xrtAesGmac(
	const xaesgcm* pState,
	const void* pNonce,
	size_t iNonceSize,
	const void* pData,
	size_t iSize,
	void* pTag
);



/* 以常量时间比较验证 GMAC 标签。 */
XRT_API bool xrtAesGmacVerify(
	const xaesgcm* pState,
	const void* pNonce,
	size_t iNonceSize,
	const void* pData,
	size_t iSize,
	const void* pTag
);

#endif



#if defined(XRT_FEATURE_CRYPTO_X25519)

#define XRT_X25519_PRIVATE_SIZE 32u
#define XRT_X25519_PUBLIC_SIZE 32u
#define XRT_X25519_SHARED_SIZE 32u

#endif



#if defined(XRT_FEATURE_CRYPTO_ED25519)

#define XRT_ED25519_SEED_SIZE 32u
#define XRT_ED25519_PUBLIC_SIZE 32u
#define XRT_ED25519_SIGNATURE_SIZE 64u
#define XRT_ED25519_PREHASH_SIZE 64u
#define XRT_ED25519_CONTEXT_MAX_SIZE 255u

/* RFC 8032 的纯消息、带上下文消息和预哈希消息三种互不兼容的域。 */
typedef enum xed25519_mode {
	XED25519_PURE = 0,
	XED25519_CONTEXT,
	XED25519_PREHASH
} xed25519mode;

/* 展开的 Ed25519 签名密钥由调用方持有，避免重复派生公钥和私有前缀。 */
typedef struct xed25519_key {
	uint8 Scalar[XRT_ED25519_SEED_SIZE];
	uint8 Prefix[XRT_ED25519_SEED_SIZE];
	uint8 Public[XRT_ED25519_PUBLIC_SIZE];
	uint32 Guard;
} xed25519key;

#endif



#if defined(XRT_FEATURE_CRYPTO_X448)

#define XRT_X448_PRIVATE_SIZE 56u
#define XRT_X448_PUBLIC_SIZE 56u
#define XRT_X448_SHARED_SIZE 56u

#endif



#if defined(XRT_FEATURE_CRYPTO_P256)

#define XRT_P256_PRIVATE_SIZE 32u
#define XRT_P256_PUBLIC_SIZE 65u
#define XRT_P256_SHARED_SIZE 32u

#endif



#if defined(XRT_FEATURE_CRYPTO_P384)

#define XRT_P384_PRIVATE_SIZE 48u
#define XRT_P384_PUBLIC_SIZE 97u
#define XRT_P384_SHARED_SIZE 48u

#endif



#if defined(XRT_FEATURE_CRYPTO_ECDSA_P256) || \
	defined(XRT_FEATURE_CRYPTO_ECDSA_P256_SIGN)

#define XRT_ECDSA_P256_SIGNATURE_SIZE 64u

#endif



#if defined(XRT_FEATURE_CRYPTO_ECDSA_P384) || \
	defined(XRT_FEATURE_CRYPTO_ECDSA_P384_SIGN)

#define XRT_ECDSA_P384_SIGNATURE_SIZE 96u

#endif



#if defined(XRT_FEATURE_CRYPTO_ECDSA_P256_DER) || \
	defined(XRT_FEATURE_CRYPTO_ECDSA_P256_SIGN_DER)

#define XRT_ECDSA_P256_DER_MAX_SIZE 72u

#endif



#if defined(XRT_FEATURE_CRYPTO_ECDSA_P384_DER) || \
	defined(XRT_FEATURE_CRYPTO_ECDSA_P384_SIGN_DER)

#define XRT_ECDSA_P384_DER_MAX_SIZE 104u

#endif



#if defined(XRT_FEATURE_CRYPTO_CHACHA20)

#define XRT_CHACHA20_KEY_SIZE 32u
#define XRT_CHACHA20_NONCE_SIZE 12u
#define XRT_CHACHA20_BLOCK_SIZE 64u

#endif



#if defined(XRT_FEATURE_CRYPTO_POLY1305)

#define XRT_POLY1305_KEY_SIZE 32u
#define XRT_POLY1305_TAG_SIZE 16u
#define XRT_POLY1305_BLOCK_SIZE 16u

/* Poly1305 流状态由调用方持有；同一密钥不得用于不同消息。 */
typedef struct xpoly1305 {
	uint32 R[5];
	uint32 H[5];
	uint32 Pad[4];
	uint8 Buffer[XRT_POLY1305_BLOCK_SIZE];
	uint32 Guard;
	uint32 BufferSize;
} xpoly1305;

#endif



#if defined(XRT_FEATURE_CRYPTO_CHACHA20_POLY1305)

#define XRT_CHACHA20_POLY1305_KEY_SIZE 32u
#define XRT_CHACHA20_POLY1305_NONCE_SIZE 12u
#define XRT_CHACHA20_POLY1305_TAG_SIZE 16u
#define XRT_CHACHA20_POLY1305_OVERHEAD 16u
#define XRT_CHACHA20_POLY1305_MAX_SIZE UINT64_C(274877906880)

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_CRYPTO_CORE)

/* 按固定数据长度比较两段内存；空区间允许空指针。 */
XRT_API bool xrtConstTimeEqual(const void* pLeft, const void* pRight, size_t iSize);

#endif



#if defined(XRT_FEATURE_CRYPTO_RSA)

/* 执行原始 RSA 公钥运算，输入和输出长度必须等于公钥模数长度。 */
XRT_API bool xrtRsaPublic(
	const xrsapublickey* pKey,
	const void* pInput,
	size_t iInputSize,
	void* pOutput
);

#endif



#if defined(XRT_FEATURE_CRYPTO_RSA_PRIVATE)

/* 执行原始 RSA 私钥运算；优先使用 CRT，并用公钥重新验证结果。 */
XRT_API bool xrtRsaPrivate(
	const xrsaprivatekey* pKey,
	const void* pInput,
	size_t iInputSize,
	void* pOutput
);

#endif



#if defined(XRT_FEATURE_CRYPTO_RSA_PSS)

/* 严格验证 EMSA-PSS 签名，可分别指定消息摘要与 MGF1 摘要。 */
XRT_API bool xrtRsaPssVerify(
	const xrsapublickey* pKey,
	xcryptohash iHash,
	xcryptohash iMaskHash,
	size_t iSaltSize,
	const void* pHash,
	const void* pSignature,
	size_t iSignatureSize
);

#endif



#if defined(XRT_FEATURE_CRYPTO_RSA_PSS_SIGN)

/* 使用调用方提供的盐生成 EMSA-PSS 签名，零长度盐允许传入空指针。 */
XRT_API bool xrtRsaPssSignSalt(
	const xrsaprivatekey* pKey,
	xcryptohash iHash,
	xcryptohash iMaskHash,
	const void* pSalt,
	size_t iSaltSize,
	const void* pHash,
	void* pSignature
);



/* 使用与消息摘要等长的密码安全随机盐生成 EMSA-PSS 签名。 */
XRT_API bool xrtRsaPssSign(
	const xrsaprivatekey* pKey,
	xcryptohash iHash,
	xcryptohash iMaskHash,
	const void* pHash,
	void* pSignature
);

#endif



#if defined(XRT_FEATURE_CRYPTO_RSA_PKCS1)

/* 严格验证带规范 DigestInfo 的 EMSA-PKCS1-v1_5 签名。 */
XRT_API bool xrtRsaPkcs1Verify(
	const xrsapublickey* pKey,
	xcryptohash iHash,
	const void* pHash,
	const void* pSignature,
	size_t iSignatureSize
);

#endif



#if defined(XRT_FEATURE_CRYPTO_RSA_PKCS1_SIGN)

/* 使用规范 DigestInfo 生成 EMSA-PKCS1-v1_5 签名。 */
XRT_API bool xrtRsaPkcs1Sign(
	const xrsaprivatekey* pKey,
	xcryptohash iHash,
	const void* pHash,
	void* pSignature
);

#endif



#if defined(XRT_FEATURE_CRYPTO_X25519)

/* 执行 RFC 7748 X25519 标量乘法；三段固定长度缓冲可以任意重叠。 */
XRT_API bool xrtX25519(
	const void* pScalar,
	const void* pPoint,
	void* pOutput
);



/* 从 32 字节私钥导出 X25519 公钥，允许原位覆盖私钥。 */
XRT_API bool xrtX25519Public(const void* pPrivate, void* pPublic);



/* 计算共享秘密并以常量时间拒绝低阶公钥产生的全零结果。 */
XRT_API bool xrtX25519Shared(
	const void* pPrivate,
	const void* pPeerPublic,
	void* pShared
);

#endif



#if defined(XRT_FEATURE_CRYPTO_X25519_KEYPAIR)

/* 使用操作系统安全随机源生成私钥和对应公钥；两个输出不得重叠。 */
XRT_API bool xrtX25519KeyPair(void* pPrivate, void* pPublic);

#endif



#if defined(XRT_FEATURE_CRYPTO_ED25519)

/* 从 32 字节种子展开可重复使用的签名密钥；成功前不修改目标状态。 */
XRT_API bool xrtEd25519KeyInit(
	xed25519key* pKey,
	const void* pSeed
);



/* 不可消除地清除展开后的私有标量、前缀和公钥。 */
XRT_API void xrtEd25519KeyClear(xed25519key* pKey);



/* 从 32 字节种子导出规范 Ed25519 公钥，允许输出覆盖种子。 */
XRT_API bool xrtEd25519Public(
	const void* pSeed,
	void* pPublic
);

#endif



#if defined(XRT_FEATURE_CRYPTO_ED25519_KEYPAIR)

/* 生成随机种子和对应公钥；两个输出区域不得重叠。 */
XRT_API bool xrtEd25519KeyPair(void* pSeed, void* pPublic);

#endif



#if defined(XRT_FEATURE_CRYPTO_ED25519_SIGN)

/* 使用展开密钥签署纯 Ed25519 消息。 */
XRT_API bool xrtEd25519SignKey(
	const xed25519key* pKey,
	const void* pMessage,
	size_t iMessageSize,
	void* pSignature
);



/* 使用种子签署纯 Ed25519 消息。 */
XRT_API bool xrtEd25519Sign(
	const void* pSeed,
	const void* pMessage,
	size_t iMessageSize,
	void* pSignature
);



/*
	签署 RFC 8032 指定模式的数据；PREHASH 模式要求消息恰为 64 字节
	SHA-512 预哈希，CONTEXT 与 PREHASH 的上下文长度上限为 255 字节。
*/
XRT_API bool xrtEd25519SignMode(
	const xed25519key* pKey,
	xed25519mode iMode,
	const void* pContext,
	size_t iContextSize,
	const void* pMessage,
	size_t iMessageSize,
	void* pSignature
);

#endif



#if defined(XRT_FEATURE_CRYPTO_ED25519_VERIFY)

/* 严格验证纯 Ed25519 签名、规范编码和主子群公钥。 */
XRT_API bool xrtEd25519Verify(
	const void* pPublic,
	const void* pMessage,
	size_t iMessageSize,
	const void* pSignature
);



/* 严格验证 RFC 8032 指定模式的签名。 */
XRT_API bool xrtEd25519VerifyMode(
	const void* pPublic,
	xed25519mode iMode,
	const void* pContext,
	size_t iContextSize,
	const void* pMessage,
	size_t iMessageSize,
	const void* pSignature
);

#endif



#if defined(XRT_FEATURE_CRYPTO_X448)

/* 执行 RFC 7748 X448 标量乘法；三段固定长度缓冲可以任意重叠。 */
XRT_API bool xrtX448(
	const void* pScalar,
	const void* pPoint,
	void* pOutput
);



/* 从 56 字节私钥导出 X448 公钥，允许原位覆盖私钥。 */
XRT_API bool xrtX448Public(const void* pPrivate, void* pPublic);



/* 计算共享秘密并以常量时间拒绝低阶公钥产生的全零结果。 */
XRT_API bool xrtX448Shared(
	const void* pPrivate,
	const void* pPeerPublic,
	void* pShared
);

#endif



#if defined(XRT_FEATURE_CRYPTO_X448_KEYPAIR)

/* 使用操作系统安全随机源生成私钥和对应公钥；两个输出不得重叠。 */
XRT_API bool xrtX448KeyPair(void* pPrivate, void* pPublic);

#endif



#if defined(XRT_FEATURE_CRYPTO_P256)

/* 验证 65 字节未压缩 SEC 1 公钥是否为有效 P-256 曲线点。 */
XRT_API bool xrtP256Valid(const void* pPublic);



/* 计算 scalar * point；三个固定长度缓冲可任意重叠。 */
XRT_API bool xrtP256Multiply(
	const void* pScalar,
	const void* pPoint,
	void* pOutput
);



/* 计算两个未压缩 P-256 公共点之和；输入输出可任意重叠。 */
XRT_API bool xrtP256Add(
	const void* pLeft,
	const void* pRight,
	void* pOutput
);



/* 从 32 字节私钥派生未压缩 P-256 公钥。 */
XRT_API bool xrtP256Public(const void* pPrivate, void* pPublic);



/* 计算经过完整私钥和对端公钥验证的 P-256 ECDH 横坐标。 */
XRT_API bool xrtP256Shared(
	const void* pPrivate,
	const void* pPeerPublic,
	void* pShared
);

#endif



#if defined(XRT_FEATURE_CRYPTO_P384)

/* 验证 97 字节未压缩 SEC 1 公钥是否为有效 P-384 曲线点。 */
XRT_API bool xrtP384Valid(const void* pPublic);



/* 计算 scalar * point；三个固定长度缓冲可任意重叠。 */
XRT_API bool xrtP384Multiply(
	const void* pScalar,
	const void* pPoint,
	void* pOutput
);



/* 计算两个未压缩 P-384 公共点之和；输入输出可任意重叠。 */
XRT_API bool xrtP384Add(
	const void* pLeft,
	const void* pRight,
	void* pOutput
);



/* 从 48 字节私钥派生未压缩 P-384 公钥。 */
XRT_API bool xrtP384Public(const void* pPrivate, void* pPublic);



/* 计算经过完整私钥和对端公钥验证的 P-384 ECDH 横坐标。 */
XRT_API bool xrtP384Shared(
	const void* pPrivate,
	const void* pPeerPublic,
	void* pShared
);

#endif



#if defined(XRT_FEATURE_CRYPTO_P256_KEYPAIR)

/* 使用操作系统安全随机源生成 P-256 私钥和未压缩公钥。 */
XRT_API bool xrtP256KeyPair(void* pPrivate, void* pPublic);

#endif



#if defined(XRT_FEATURE_CRYPTO_P384_KEYPAIR)

/* 使用操作系统安全随机源生成 P-384 私钥和未压缩公钥。 */
XRT_API bool xrtP384KeyPair(void* pPrivate, void* pPublic);

#endif



#if defined(XRT_FEATURE_CRYPTO_ECDSA_DER)

/* 把定宽 raw r||s 签名编码为规范 DER；空输出可查询所需长度。 */
XRT_API bool xrtEcdsaDerEncode(
	const void* pRaw,
	size_t iScalarSize,
	void* pDer,
	size_t iCapacity,
	size_t* pSize
);



/* 严格解码规范 DER ECDSA 签名为定宽 raw r||s。 */
XRT_API bool xrtEcdsaDerDecode(
	const void* pDer,
	size_t iDerSize,
	void* pRaw,
	size_t iScalarSize
);

#endif



#if defined(XRT_FEATURE_CRYPTO_ECDSA_P256)

/* 验证任意非空摘要上的定宽 P-256 ECDSA raw r||s 签名。 */
XRT_API bool xrtEcdsaP256Verify(
	const void* pHash,
	size_t iHashSize,
	const void* pSignature,
	const void* pPublic
);

#endif



#if defined(XRT_FEATURE_CRYPTO_ECDSA_P384)

/* 验证任意非空摘要上的定宽 P-384 ECDSA raw r||s 签名。 */
XRT_API bool xrtEcdsaP384Verify(
	const void* pHash,
	size_t iHashSize,
	const void* pSignature,
	const void* pPublic
);

#endif



#if defined(XRT_FEATURE_CRYPTO_ECDSA_P256_DER)

/* 严格解码 DER 后验证任意非空摘要上的 P-256 ECDSA 签名。 */
XRT_API bool xrtEcdsaP256VerifyDer(
	const void* pHash,
	size_t iHashSize,
	const void* pDer,
	size_t iDerSize,
	const void* pPublic
);

#endif



#if defined(XRT_FEATURE_CRYPTO_ECDSA_P384_DER)

/* 严格解码 DER 后验证任意非空摘要上的 P-384 ECDSA 签名。 */
XRT_API bool xrtEcdsaP384VerifyDer(
	const void* pHash,
	size_t iHashSize,
	const void* pDer,
	size_t iDerSize,
	const void* pPublic
);

#endif



#if defined(XRT_FEATURE_CRYPTO_ECDSA_P256_SIGN)

/* 使用指定摘要算法的 RFC 6979 路径生成定宽 low-S P-256 ECDSA 签名。 */
XRT_API bool xrtEcdsaP256Sign(
	xcryptohash Hash,
	const void* pHash,
	const void* pPrivate,
	void* pSignature
);

#endif



#if defined(XRT_FEATURE_CRYPTO_ECDSA_P384_SIGN)

/* 使用指定摘要算法的 RFC 6979 路径生成定宽 low-S P-384 ECDSA 签名。 */
XRT_API bool xrtEcdsaP384Sign(
	xcryptohash Hash,
	const void* pHash,
	const void* pPrivate,
	void* pSignature
);

#endif



#if defined(XRT_FEATURE_CRYPTO_ECDSA_P256_SIGN_DER)

/* 生成确定性 low-S P-256 ECDSA 签名并编码为规范 DER。 */
XRT_API bool xrtEcdsaP256SignDer(
	xcryptohash Hash,
	const void* pHash,
	const void* pPrivate,
	void* pDer,
	size_t iCapacity,
	size_t* pSize
);

#endif



#if defined(XRT_FEATURE_CRYPTO_ECDSA_P384_SIGN_DER)

/* 生成确定性 low-S P-384 ECDSA 签名并编码为规范 DER。 */
XRT_API bool xrtEcdsaP384SignDer(
	xcryptohash Hash,
	const void* pHash,
	const void* pPrivate,
	void* pDer,
	size_t iCapacity,
	size_t* pSize
);

#endif



#if defined(XRT_FEATURE_CRYPTO_MD5)

/* 初始化或重置 MD5 流状态。 */
XRT_API void xrtMd5Init(xmd5* pState);



/* 向 MD5 状态追加任意分块；失败时状态保持不变。 */
XRT_API bool xrtMd5Update(xmd5* pState, const void* pData, size_t iSize);



/* 从状态快照输出 16 字节摘要，不结束或修改原状态。 */
XRT_API bool xrtMd5Final(const xmd5* pState, void* pDigest);



/* 一次计算一段连续数据的 16 字节 MD5 摘要。 */
XRT_API bool xrtMd5(const void* pData, size_t iSize, void* pDigest);

#endif



#if defined(XRT_FEATURE_CRYPTO_SHA1)

/* 初始化或重置 SHA-1 流状态。 */
XRT_API void xrtSha1Init(xsha1* pState);



/* 向 SHA-1 状态追加任意分块；失败时状态保持不变。 */
XRT_API bool xrtSha1Update(xsha1* pState, const void* pData, size_t iSize);



/* 从状态快照输出 20 字节摘要，不结束或修改原状态。 */
XRT_API bool xrtSha1Final(const xsha1* pState, void* pDigest);



/* 一次计算一段连续数据的 20 字节 SHA-1 摘要。 */
XRT_API bool xrtSha1(const void* pData, size_t iSize, void* pDigest);

#endif



#if defined(XRT_FEATURE_CRYPTO_SHA224)

/* 初始化或重置 SHA-224 流状态。 */
XRT_API void xrtSha224Init(xsha224* pState);



/* 向 SHA-224 状态追加任意分块；失败时状态保持不变。 */
XRT_API bool xrtSha224Update(xsha224* pState, const void* pData, size_t iSize);



/* 从状态快照输出 28 字节摘要，不结束或修改原状态。 */
XRT_API bool xrtSha224Final(const xsha224* pState, void* pDigest);



/* 一次计算一段连续数据的 28 字节 SHA-224 摘要。 */
XRT_API bool xrtSha224(const void* pData, size_t iSize, void* pDigest);

#endif



#if defined(XRT_FEATURE_CRYPTO_SHA256)

/* 初始化或重置 SHA-256 流状态。 */
XRT_API void xrtSha256Init(xsha256* pState);



/* 向 SHA-256 状态追加任意分块；失败时状态保持不变。 */
XRT_API bool xrtSha256Update(xsha256* pState, const void* pData, size_t iSize);



/* 从状态快照输出 32 字节摘要，不结束或修改原状态。 */
XRT_API bool xrtSha256Final(const xsha256* pState, void* pDigest);



/* 一次计算一段连续数据的 32 字节 SHA-256 摘要。 */
XRT_API bool xrtSha256(const void* pData, size_t iSize, void* pDigest);

#endif



#if defined(XRT_FEATURE_CRYPTO_SHA512)

/* 初始化或重置 SHA-384 流状态。 */
XRT_API void xrtSha384Init(xsha384* pState);



/* 向 SHA-384 状态追加任意分块；失败时状态保持不变。 */
XRT_API bool xrtSha384Update(xsha384* pState, const void* pData, size_t iSize);



/* 从状态快照输出 48 字节摘要，不结束或修改原状态。 */
XRT_API bool xrtSha384Final(const xsha384* pState, void* pDigest);



/* 一次计算一段连续数据的 48 字节 SHA-384 摘要。 */
XRT_API bool xrtSha384(const void* pData, size_t iSize, void* pDigest);



/* 初始化或重置 SHA-512 流状态。 */
XRT_API void xrtSha512Init(xsha512* pState);



/* 向 SHA-512 状态追加任意分块；失败时状态保持不变。 */
XRT_API bool xrtSha512Update(xsha512* pState, const void* pData, size_t iSize);



/* 从状态快照输出 64 字节摘要，不结束或修改原状态。 */
XRT_API bool xrtSha512Final(const xsha512* pState, void* pDigest);



/* 一次计算一段连续数据的 64 字节 SHA-512 摘要。 */
XRT_API bool xrtSha512(const void* pData, size_t iSize, void* pDigest);

#endif



#if defined(XRT_FEATURE_CRYPTO_SHA512_256)

/* 初始化或重置 SHA-512/256 流状态。 */
XRT_API void xrtSha512_256Init(xsha512_256* pState);



/* 向 SHA-512/256 状态追加任意分块；失败时状态保持不变。 */
XRT_API bool xrtSha512_256Update(
	xsha512_256* pState,
	const void* pData,
	size_t iSize
);



/* 从状态快照输出 32 字节摘要，不结束或修改原状态。 */
XRT_API bool xrtSha512_256Final(
	const xsha512_256* pState,
	void* pDigest
);



/* 一次计算一段连续数据的 32 字节 SHA-512/256 摘要。 */
XRT_API bool xrtSha512_256(
	const void* pData,
	size_t iSize,
	void* pDigest
);

#endif



#if defined(XRT_FEATURE_CRYPTO_HMAC_SHA256)

/* 使用任意长度密钥初始化或重置 HMAC-SHA256 状态。 */
XRT_API bool xrtHmacSha256Init(
	xhmacsha256* pState,
	const void* pKey,
	size_t iKeySize
);



/* 向 HMAC-SHA256 状态追加任意分块；失败时状态保持不变。 */
XRT_API bool xrtHmacSha256Update(
	xhmacsha256* pState,
	const void* pData,
	size_t iSize
);



/* 从状态快照输出 32 字节 HMAC，不结束或修改原状态。 */
XRT_API bool xrtHmacSha256Final(const xhmacsha256* pState, void* pMac);



/* 一次计算一段连续数据的 HMAC-SHA256。 */
XRT_API bool xrtHmacSha256(
	const void* pKey,
	size_t iKeySize,
	const void* pData,
	size_t iSize,
	void* pMac
);

#endif



#if defined(XRT_FEATURE_CRYPTO_HMAC_SHA512)

/* 使用任意长度密钥初始化或重置 HMAC-SHA384 状态。 */
XRT_API bool xrtHmacSha384Init(
	xhmacsha384* pState,
	const void* pKey,
	size_t iKeySize
);



/* 向 HMAC-SHA384 状态追加任意分块；失败时状态保持不变。 */
XRT_API bool xrtHmacSha384Update(
	xhmacsha384* pState,
	const void* pData,
	size_t iSize
);



/* 从状态快照输出 48 字节 HMAC，不结束或修改原状态。 */
XRT_API bool xrtHmacSha384Final(const xhmacsha384* pState, void* pMac);



/* 一次计算一段连续数据的 HMAC-SHA384。 */
XRT_API bool xrtHmacSha384(
	const void* pKey,
	size_t iKeySize,
	const void* pData,
	size_t iSize,
	void* pMac
);



/* 使用任意长度密钥初始化或重置 HMAC-SHA512 状态。 */
XRT_API bool xrtHmacSha512Init(
	xhmacsha512* pState,
	const void* pKey,
	size_t iKeySize
);



/* 向 HMAC-SHA512 状态追加任意分块；失败时状态保持不变。 */
XRT_API bool xrtHmacSha512Update(
	xhmacsha512* pState,
	const void* pData,
	size_t iSize
);



/* 从状态快照输出 64 字节 HMAC，不结束或修改原状态。 */
XRT_API bool xrtHmacSha512Final(const xhmacsha512* pState, void* pMac);



/* 一次计算一段连续数据的 HMAC-SHA512。 */
XRT_API bool xrtHmacSha512(
	const void* pKey,
	size_t iKeySize,
	const void* pData,
	size_t iSize,
	void* pMac
);

#endif



#if defined(XRT_FEATURE_CRYPTO_PBKDF2_SHA256)

/* 使用 PBKDF2-HMAC-SHA256 从密码和 salt 派生任意合规长度的密钥。 */
XRT_API bool xrtPbkdf2Sha256(
	const void* pPassword,
	size_t iPasswordSize,
	const void* pSalt,
	size_t iSaltSize,
	uint32 iIterations,
	void* pOutput,
	size_t iOutputSize
);

#endif



#if defined(XRT_FEATURE_CRYPTO_PBKDF2_SHA512)

/* 使用 PBKDF2-HMAC-SHA384 从密码和 salt 派生任意合规长度的密钥。 */
XRT_API bool xrtPbkdf2Sha384(
	const void* pPassword,
	size_t iPasswordSize,
	const void* pSalt,
	size_t iSaltSize,
	uint32 iIterations,
	void* pOutput,
	size_t iOutputSize
);



/* 使用 PBKDF2-HMAC-SHA512 从密码和 salt 派生任意合规长度的密钥。 */
XRT_API bool xrtPbkdf2Sha512(
	const void* pPassword,
	size_t iPasswordSize,
	const void* pSalt,
	size_t iSaltSize,
	uint32 iIterations,
	void* pOutput,
	size_t iOutputSize
);

#endif



#if defined(XRT_FEATURE_CRYPTO_HKDF_SHA256)

/* 从 salt 和输入密钥材料提取 32 字节 SHA-256 PRK。 */
XRT_API bool xrtHkdfSha256Extract(
	const void* pSalt,
	size_t iSaltSize,
	const void* pIkm,
	size_t iIkmSize,
	void* pPrk
);



/* 从 PRK 和可选 info 展开最多 255 * 32 字节输出。 */
XRT_API bool xrtHkdfSha256Expand(
	const void* pPrk,
	size_t iPrkSize,
	const void* pInfo,
	size_t iInfoSize,
	void* pOkm,
	size_t iOkmSize
);



/* 组合 Extract 与 Expand 完成一次 HKDF-SHA256 派生。 */
XRT_API bool xrtHkdfSha256(
	const void* pSalt,
	size_t iSaltSize,
	const void* pIkm,
	size_t iIkmSize,
	const void* pInfo,
	size_t iInfoSize,
	void* pOkm,
	size_t iOkmSize
);

#endif



#if defined(XRT_FEATURE_CRYPTO_HKDF_SHA512)

/* 从 salt 和输入密钥材料提取 48 字节 SHA-384 PRK。 */
XRT_API bool xrtHkdfSha384Extract(
	const void* pSalt,
	size_t iSaltSize,
	const void* pIkm,
	size_t iIkmSize,
	void* pPrk
);



/* 从 PRK 和可选 info 展开最多 255 * 48 字节输出。 */
XRT_API bool xrtHkdfSha384Expand(
	const void* pPrk,
	size_t iPrkSize,
	const void* pInfo,
	size_t iInfoSize,
	void* pOkm,
	size_t iOkmSize
);



/* 组合 Extract 与 Expand 完成一次 HKDF-SHA384 派生。 */
XRT_API bool xrtHkdfSha384(
	const void* pSalt,
	size_t iSaltSize,
	const void* pIkm,
	size_t iIkmSize,
	const void* pInfo,
	size_t iInfoSize,
	void* pOkm,
	size_t iOkmSize
);



/* 从 salt 和输入密钥材料提取 64 字节 SHA-512 PRK。 */
XRT_API bool xrtHkdfSha512Extract(
	const void* pSalt,
	size_t iSaltSize,
	const void* pIkm,
	size_t iIkmSize,
	void* pPrk
);



/* 从 PRK 和可选 info 展开最多 255 * 64 字节输出。 */
XRT_API bool xrtHkdfSha512Expand(
	const void* pPrk,
	size_t iPrkSize,
	const void* pInfo,
	size_t iInfoSize,
	void* pOkm,
	size_t iOkmSize
);



/* 组合 Extract 与 Expand 完成一次 HKDF-SHA512 派生。 */
XRT_API bool xrtHkdfSha512(
	const void* pSalt,
	size_t iSaltSize,
	const void* pIkm,
	size_t iIkmSize,
	const void* pInfo,
	size_t iInfoSize,
	void* pOkm,
	size_t iOkmSize
);

#endif



#if defined(XRT_FEATURE_CRYPTO_CHACHA20)

/* 使用 IETF 96 位 nonce 从指定块计数器开始异或 ChaCha20 密钥流。 */
XRT_API bool xrtChaCha20(
	const void* pKey,
	const void* pNonce,
	uint32 iCounter,
	const void* pInput,
	void* pOutput,
	size_t iSize
);

#endif



#if defined(XRT_FEATURE_CRYPTO_POLY1305)

/* 使用一个 32 字节一次性密钥初始化或重置 Poly1305 状态。 */
XRT_API bool xrtPoly1305Init(xpoly1305* pState, const void* pKey);



/* 向 Poly1305 状态追加任意分块；失败时状态保持不变。 */
XRT_API bool xrtPoly1305Update(
	xpoly1305* pState,
	const void* pData,
	size_t iSize
);



/* 从状态快照输出 16 字节标签，不结束或修改原状态。 */
XRT_API bool xrtPoly1305Final(const xpoly1305* pState, void* pTag);



/* 一次计算一段连续数据的 16 字节 Poly1305 标签。 */
XRT_API bool xrtPoly1305(
	const void* pKey,
	const void* pData,
	size_t iSize,
	void* pTag
);

#endif



#if defined(XRT_FEATURE_CRYPTO_CHACHA20_POLY1305)

/* 加密并把密文与 16 字节认证标签写入分离输出。 */
XRT_API bool xrtChaCha20Poly1305Encrypt(
	const void* pKey,
	const void* pNonce,
	const void* pAad,
	size_t iAadSize,
	const void* pPlain,
	size_t iPlainSize,
	void* pCipher,
	void* pTag
);



/* 验证分离标签后解密；认证失败时不修改明文输出。 */
XRT_API bool xrtChaCha20Poly1305Decrypt(
	const void* pKey,
	const void* pNonce,
	const void* pAad,
	size_t iAadSize,
	const void* pCipher,
	size_t iCipherSize,
	const void* pTag,
	void* pPlain
);



/* 加密为 cipher || tag；输出容量至少为明文长度加 16。 */
XRT_API bool xrtChaCha20Poly1305Seal(
	const void* pKey,
	const void* pNonce,
	const void* pAad,
	size_t iAadSize,
	const void* pPlain,
	size_t iPlainSize,
	void* pOutput,
	size_t iOutputSize
);



/* 打开 cipher || tag；输出容量至少为输入长度减 16。 */
XRT_API bool xrtChaCha20Poly1305Open(
	const void* pKey,
	const void* pNonce,
	const void* pAad,
	size_t iAadSize,
	const void* pInput,
	size_t iInputSize,
	void* pPlain,
	size_t iPlainSize
);

#endif



XRT_EXTERN_C_END

#endif
