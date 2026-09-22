#define XRT_MODULE_CODEC_HEX
#define XRT_MODULE_CRYPTO_SHA1
#define XRT_MODULE_CRYPTO_SHA256
#define XRT_MODULE_CRYPTO_SHA512
#define XRT_MODULE_CRYPTO_HMAC_SHA256
#define XRT_MODULE_CRYPTO_HMAC_SHA512
#define XRT_MODULE_CRYPTO_CHACHA20_POLY1305
#define XRT_MODULE_CRYPTO_AES_GCM
#define XRT_MODULE_CRYPTO_HKDF_SHA256
#define XRT_MODULE_CRYPTO_HKDF_SHA512
#define XRT_MODULE_CRYPTO_ED25519_KEYPAIR
#define XRT_MODULE_CRYPTO_ED25519_SIGN
#define XRT_MODULE_CRYPTO_ED25519_VERIFY
#define XRT_MODULE_CRYPTO_X25519_KEYPAIR
#define XRT_MODULE_CRYPTO_X448_KEYPAIR
#define XRT_MODULE_CRYPTO_P256_KEYPAIR
#define XRT_MODULE_CRYPTO_P384_KEYPAIR
#define XRT_MODULE_CRYPTO_ECDSA_P256_DER
#define XRT_MODULE_RANDOM_SECURE
#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



/* 检查示例步骤并输出与 xlang 示例一致的结果。 */
static void require_line(const char* name, int ok)
{
    if ( !ok ) {
        fprintf(stderr, "%s=fail\n", name);
        exit(1);
    }
    printf("%s=ok\n", name);
}



/* 把二进制结果编码为由调用方释放的大写 HEX 文本。 */
static str hex_of(const void* data, size_t size)
{
    return xrtHexEncodeNew(data, size, XHEX_UPPER);
}



/* 使用 AES-GCM 的组合输出形式执行一次往返。 */
static int aes_gcm_round_trip(
    const void* key,
    size_t keySize,
    const void* nonce,
    size_t nonceSize,
    const void* aad,
    size_t aadSize,
    const void* plain,
    size_t plainSize
)
{
    xaesgcm state;
    unsigned char sealed[160];
    unsigned char restored[144];
    int ok;

    if ( !xrtAesGcmInit(&state, key, keySize, 16u) ) {
        return 0;
    }
    ok = xrtAesGcmSeal(
        &state,
        nonce,
        nonceSize,
        aad,
        aadSize,
        plain,
        plainSize,
        sealed,
        plainSize + 16u
    ) && xrtAesGcmOpen(
        &state,
        nonce,
        nonceSize,
        aad,
        aadSize,
        sealed,
        plainSize + 16u,
        restored,
        plainSize
    ) && (memcmp(restored, plain, plainSize) == 0);
    xrtAesGcmClear(&state);
    return ok;
}



/* 验证摘要、认证加密、派生、密钥交换和签名公开契约。 */
int main(void)
{
    static const char hmacText[] =
        "The quick brown fox jumps over the lazy dog";
    static const unsigned char key16[] = "0123456789abcdef";
    static const unsigned char key32[] =
        "0123456789abcdef0123456789abcdef";
    static const unsigned char nonce[] = "123456789012";
    static const unsigned char aad[] = "meta";
    static const unsigned char plain[] = "hello crypto";
    static const unsigned char binary[] = {0u, 1u, 2u, 255u};
    unsigned char digest1[20];
    unsigned char digest256[32];
    unsigned char digest384[48];
    unsigned char digest512[64];
    unsigned char changed[32];
    unsigned char mac256[32];
    unsigned char mac384[48];
    unsigned char mac512[64];
    unsigned char sealed[160];
    unsigned char restored[144];
    unsigned char okm256[32];
    unsigned char okm384[32];
    unsigned char randomBytes[16];
    unsigned char streamCipher[144];
    unsigned char streamPlain[144];
    unsigned char seed[32];
    unsigned char publicKey[32];
    unsigned char signature[64];
    unsigned char x25519PrivA[32], x25519PubA[32];
    unsigned char x25519PrivB[32], x25519PubB[32];
    unsigned char x25519SecretA[32], x25519SecretB[32];
    unsigned char x448PrivA[56], x448PubA[56];
    unsigned char x448PrivB[56], x448PubB[56];
    unsigned char x448SecretA[56], x448SecretB[56];
    unsigned char p256PrivA[32], p256PubA[65];
    unsigned char p256PrivB[32], p256PubB[65];
    unsigned char p256SecretA[32], p256SecretB[32];
    unsigned char p384PrivA[48], p384PubA[97];
    unsigned char p384PrivB[48], p384PubB[97];
    unsigned char p384SecretA[48], p384SecretB[48];
    xsha1 sha1State;
    xsha256 sha256State;
    xsha384 sha384State;
    xsha512 sha512State;
    str text256;
    str textChanged;
    str text384;
    str text512;
    str textMac;
    int ok;

    ok = xrtSha1("xlang", 5u, digest1) &&
        xrtSha256("xlang", 5u, digest256) &&
        xrtSha384("xlang", 5u, digest384) &&
        xrtSha512("xlang", 5u, digest512) &&
        xrtSha256("xlang!", 6u, changed);
    text256 = hex_of(digest256, sizeof(digest256));
    textChanged = hex_of(changed, sizeof(changed));
    text384 = hex_of(digest384, sizeof(digest384));
    text512 = hex_of(digest512, sizeof(digest512));
    require_line(
        "hash",
        ok &&
        (text256 != NULL) && (strlen(text256) == 64u) &&
        (text384 != NULL) && (strlen(text384) == 96u) &&
        (text512 != NULL) && (strlen(text512) == 128u) &&
        (textChanged != NULL) && (strcmp(text256, textChanged) != 0));
    xrtFree(text512);
    xrtFree(text384);
    xrtFree(textChanged);
    xrtFree(text256);

    ok = xrtHmacSha256("key", 3u, hmacText, strlen(hmacText), mac256) &&
        xrtHmacSha384("key", 3u, hmacText, strlen(hmacText), mac384) &&
        xrtHmacSha512("key", 3u, hmacText, strlen(hmacText), mac512);
    textMac = hex_of(mac256, sizeof(mac256));
    require_line(
        "hmac",
        ok && (textMac != NULL) &&
        (strcmp(textMac,
            "F7BC83F430538424B13298E6AA6FB143EF4D59A14946175997479DBC2D1A3CD8") == 0));
    xrtFree(textMac);

    ok = xrtChaCha20Poly1305Seal(
        key32, nonce, aad, sizeof(aad) - 1u,
        plain, sizeof(plain) - 1u, sealed, sizeof(plain) - 1u + 16u
    ) && xrtChaCha20Poly1305Open(
        key32, nonce, aad, sizeof(aad) - 1u,
        sealed, sizeof(plain) - 1u + 16u,
        restored, sizeof(plain) - 1u
    ) && (memcmp(restored, plain, sizeof(plain) - 1u) == 0) &&
        aes_gcm_round_trip(
            key16, 16u, nonce, 12u, aad, 4u, plain, sizeof(plain) - 1u
        ) && aes_gcm_round_trip(
            key32, 32u, nonce, 12u, aad, 4u, plain, sizeof(plain) - 1u
        );
    require_line("aead", ok);

    ok = xrtHkdfSha256(
        "salt", 4u, "key", 3u, "info", 4u, okm256, sizeof(okm256)
    ) && xrtHkdfSha384(
        "salt", 4u, "key", 3u, "info", 4u, okm384, sizeof(okm384)
    ) && xrtSecureRandom(randomBytes, sizeof(randomBytes));
    require_line("hkdf_random", ok);

    xrtSha1Init(&sha1State);
    xrtSha256Init(&sha256State);
    xrtSha384Init(&sha384State);
    xrtSha512Init(&sha512State);
    ok = xrtSha1Update(&sha1State, "a", 1u) &&
        xrtSha1Update(&sha1State, "bc", 2u) &&
        xrtSha1Final(&sha1State, digest1) &&
        xrtSha256Update(&sha256State, "a", 1u) &&
        xrtSha256Update(&sha256State, "bc", 2u) &&
        xrtSha256Final(&sha256State, digest256) &&
        xrtSha384Update(&sha384State, "abc", 3u) &&
        xrtSha384Final(&sha384State, digest384) &&
        xrtSha512Update(&sha512State, "ab", 2u) &&
        xrtSha512Update(&sha512State, "c", 1u) &&
        xrtSha512Final(&sha512State, digest512);
    text256 = hex_of(digest256, sizeof(digest256));
    require_line(
        "stream",
        ok && (text256 != NULL) &&
        (strcmp(text256,
            "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD") == 0));
    xrtFree(text256);

    ok = xrtSha256(binary, sizeof(binary), digest256) &&
        xrtHmacSha256("key", 3u, binary, sizeof(binary), mac256) &&
        xrtChaCha20(key32, nonce, 3u, binary, streamCipher, sizeof(binary)) &&
        xrtChaCha20(
            key32, nonce, 3u, streamCipher, streamPlain, sizeof(streamCipher)
        ) && (memcmp(streamPlain, binary, sizeof(binary)) == 0);
    text256 = hex_of(digest256, sizeof(digest256));
    require_line(
        "binary",
        ok && (text256 != NULL) &&
        (strcmp(text256,
            "3D1F57C984978EF98A18378C8166C1CB8EDE02C03EEB6AEE7E2F121DFEEE3E56") == 0));
    xrtFree(text256);

    ok = xrtChaCha20(
        key32, nonce, 1u, plain, streamCipher, sizeof(plain) - 1u
    ) && xrtChaCha20(
        key32, nonce, 1u, streamCipher, streamPlain, sizeof(plain) - 1u
    ) && (memcmp(streamPlain, plain, sizeof(plain) - 1u) == 0);
    require_line("chacha20", ok);

    ok = xrtEd25519KeyPair(seed, publicKey) &&
        xrtEd25519Sign(seed, "message", 7u, signature) &&
        xrtEd25519Verify(publicKey, "message", 7u, signature) &&
        !xrtEd25519Verify(publicKey, "message!", 8u, signature);
    require_line("ed25519", ok);

    ok = xrtX25519KeyPair(x25519PrivA, x25519PubA) &&
        xrtX25519KeyPair(x25519PrivB, x25519PubB) &&
        xrtX25519Shared(x25519PrivA, x25519PubB, x25519SecretA) &&
        xrtX25519Shared(x25519PrivB, x25519PubA, x25519SecretB) &&
        (memcmp(x25519SecretA, x25519SecretB, sizeof(x25519SecretA)) == 0);
    require_line("x25519", ok);

    ok = xrtX448KeyPair(x448PrivA, x448PubA) &&
        xrtX448KeyPair(x448PrivB, x448PubB) &&
        xrtX448Shared(x448PrivA, x448PubB, x448SecretA) &&
        xrtX448Shared(x448PrivB, x448PubA, x448SecretB) &&
        (memcmp(x448SecretA, x448SecretB, sizeof(x448SecretA)) == 0);
    require_line("x448", ok);

    ok = xrtP256KeyPair(p256PrivA, p256PubA) &&
        xrtP256KeyPair(p256PrivB, p256PubB) &&
        xrtP256Shared(p256PrivA, p256PubB, p256SecretA) &&
        xrtP256Shared(p256PrivB, p256PubA, p256SecretB) &&
        (memcmp(p256SecretA, p256SecretB, sizeof(p256SecretA)) == 0) &&
        xrtP384KeyPair(p384PrivA, p384PubA) &&
        xrtP384KeyPair(p384PrivB, p384PubB) &&
        xrtP384Shared(p384PrivA, p384PubB, p384SecretA) &&
        xrtP384Shared(p384PrivB, p384PubA, p384SecretB) &&
        (memcmp(p384SecretA, p384SecretB, sizeof(p384SecretA)) == 0);
    require_line("ecdh", ok);

    require_line(
        "signature_guard",
        !xrtEcdsaP256VerifyDer(NULL, 0u, NULL, 0u, NULL));
    return 0;
}
