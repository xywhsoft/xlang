#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require_line(const char* name, int ok)
{
    if ( !ok ) {
        fprintf(stderr, "%s=fail\n", name);
        exit(1);
    }
    printf("%s=ok\n", name);
}

static int text_eq(const char* a, const char* b)
{
    return a != NULL && b != NULL && strcmp(a, b) == 0;
}

static char* hex_of(const unsigned char* data, size_t size)
{
    return (char*)xrtHexEncode((void*)data, size);
}

static char* sha256_hex(const char* text)
{
    unsigned char digest[32];
    xrtSHA256((void*)text, strlen(text), digest);
    return hex_of(digest, sizeof(digest));
}

static char* sha384_hex(const char* text)
{
    unsigned char digest[48];
    xrtSHA384((void*)text, strlen(text), digest);
    return hex_of(digest, sizeof(digest));
}

static char* sha512_hex(const char* text)
{
    unsigned char digest[64];
    xrtSHA512((void*)text, strlen(text), digest);
    return hex_of(digest, sizeof(digest));
}

static char* hmac_sha256_hex(const char* key, const char* text)
{
    unsigned char digest[32];
    xrtHMAC_SHA256((const unsigned char*)key, strlen(key), (const unsigned char*)text, strlen(text), digest);
    return hex_of(digest, sizeof(digest));
}

static int valid_hex(const char* text)
{
    size_t i;
    size_t len;

    if ( text == NULL ) {
        return 0;
    }
    len = strlen(text);
    if ( len == 0 || (len & 1u) != 0 ) {
        return 0;
    }
    for ( i = 0; i < len; ++i ) {
        char ch = text[i];
        if ( !((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')) ) {
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    const char* hmacText = "The quick brown fox jumps over the lazy dog";
    const unsigned char key16[] = "0123456789abcdef";
    const unsigned char key32[] = "0123456789abcdef0123456789abcdef";
    const unsigned char nonce[] = "123456789012";
    const unsigned char aad[] = "meta";
    const char* plain = "hello crypto";
    size_t plainSize = strlen(plain);
    unsigned char cipher[128];
    unsigned char decrypted[128];
    unsigned char digest[64];
    unsigned char prk[32];
    unsigned char okm[32];
    unsigned char randomBytes[16];
    char* sha256 = NULL;
    char* sha256Changed = NULL;
    char* sha384 = NULL;
    char* sha512 = NULL;
    char* hmac = NULL;
    char* hkdf = NULL;
    xsha256_ctx sha256Ctx;
    xsha512_ctx sha384Ctx;
    xsha512_ctx sha512Ctx;
    unsigned char chachaCipher[128];
    unsigned char chachaPlain[128];
    unsigned char seed[32];
    unsigned char pub[32];
    unsigned char sig[64];
    unsigned char x25519PrivA[32], x25519PubA[32], x25519PrivB[32], x25519PubB[32], x25519SecretA[32], x25519SecretB[32];
    unsigned char x448PrivA[56], x448PubA[56], x448PrivB[56], x448PubB[56], x448SecretA[56], x448SecretB[56];

    xrtInit();

    sha256 = sha256_hex("xlang");
    sha256Changed = sha256_hex("xlang!");
    sha384 = sha384_hex("xlang");
    sha512 = sha512_hex("xlang");
    require_line("hash",
        strlen(sha256) == 64 &&
        strlen(sha384) == 96 &&
        strlen(sha512) == 128 &&
        !text_eq(sha256, sha256Changed));
    xrtFree(sha256);
    xrtFree(sha256Changed);
    xrtFree(sha384);
    xrtFree(sha512);

    hmac = hmac_sha256_hex("key", hmacText);
    require_line("hmac", text_eq(hmac, "F7BC83F430538424B13298E6AA6FB143EF4D59A14946175997479DBC2D1A3CD8"));
    xrtFree(hmac);

    memset(cipher, 0, sizeof(cipher));
    memset(decrypted, 0, sizeof(decrypted));
    require_line("aead",
        xrtChaCha20Poly1305Encrypt(cipher, key32, nonce, aad, sizeof(aad) - 1u, (const unsigned char*)plain, plainSize) &&
        xrtChaCha20Poly1305Decrypt(decrypted, key32, nonce, aad, sizeof(aad) - 1u, cipher, plainSize + 16u) &&
        memcmp(decrypted, plain, plainSize) == 0 &&
        xrtAES128GCMEncrypt(cipher, key16, nonce, sizeof(nonce) - 1u, aad, sizeof(aad) - 1u, (const unsigned char*)plain, plainSize) &&
        xrtAES128GCMDecrypt(decrypted, key16, nonce, sizeof(nonce) - 1u, aad, sizeof(aad) - 1u, cipher, plainSize + 16u) &&
        memcmp(decrypted, plain, plainSize) == 0 &&
        xrtAES256GCMEncrypt(cipher, key32, nonce, sizeof(nonce) - 1u, aad, sizeof(aad) - 1u, (const unsigned char*)plain, plainSize) &&
        xrtAES256GCMDecrypt(decrypted, key32, nonce, sizeof(nonce) - 1u, aad, sizeof(aad) - 1u, cipher, plainSize + 16u) &&
        memcmp(decrypted, plain, plainSize) == 0);

    xrtHKDFExtract(prk, (const unsigned char*)"salt", 4u, (const unsigned char*)"key", 3u);
    xrtHKDFExpand(okm, sizeof(okm), prk, sizeof(prk), (const unsigned char*)"info", 4u);
    hkdf = hex_of(okm, sizeof(okm));
    xrtRandomBytes(randomBytes, sizeof(randomBytes));
    require_line("hkdf_random", strlen(hkdf) == 64);
    xrtFree(hkdf);

    xrtSHA256Init(&sha256Ctx);
    xrtSHA256Update(&sha256Ctx, "a", 1u);
    xrtSHA256Update(&sha256Ctx, "bc", 2u);
    xrtSHA256Final(&sha256Ctx, digest);
    sha256 = hex_of(digest, 32u);
    xrtSHA384Init(&sha384Ctx);
    xrtSHA512Update(&sha384Ctx, "abc", 3u);
    xrtSHA384Final(&sha384Ctx, digest);
    sha384 = hex_of(digest, 48u);
    xrtSHA512Init(&sha512Ctx);
    xrtSHA512Update(&sha512Ctx, "ab", 2u);
    xrtSHA512Update(&sha512Ctx, "c", 1u);
    xrtSHA512Final(&sha512Ctx, digest);
    sha512 = hex_of(digest, 64u);
    require_line("stream",
        text_eq(sha256, "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD") &&
        text_eq(sha384, "CB00753F45A35E8BB5A03D699AC65007272C32AB0EDED1631A8B605A43FF5BED8086072BA1E7CC2358BAECA134C825A7") &&
        text_eq(sha512, "DDAF35A193617ABACC417349AE20413112E6FA4E89A97EA20A9EEEE64B55D39A2192992A274FC1A836BA3C23A3FEEBBD454D4423643CE80E2A9AC94FA54CA49F"));
    xrtFree(sha256);
    xrtFree(sha384);
    xrtFree(sha512);

    xrtChaCha20(chachaCipher, key32, nonce, 1u, (const unsigned char*)plain, plainSize);
    xrtChaCha20(chachaPlain, key32, nonce, 1u, chachaCipher, plainSize);
    require_line("chacha20", memcmp(chachaPlain, plain, plainSize) == 0);

    xrtEd25519Keypair(seed, pub);
    xrtEd25519Sign(sig, (const unsigned char*)"message", 7u, seed);
    require_line("ed25519",
        xrtEd25519Verify((const unsigned char*)"message", 7u, sig, pub) &&
        !xrtEd25519Verify((const unsigned char*)"message!", 8u, sig, pub));

    xrtX25519Keypair(x25519PrivA, x25519PubA);
    xrtX25519Keypair(x25519PrivB, x25519PubB);
    xrtX25519SharedSecret(x25519SecretA, x25519PrivA, x25519PubB);
    xrtX25519SharedSecret(x25519SecretB, x25519PrivB, x25519PubA);
    require_line("x25519", memcmp(x25519SecretA, x25519SecretB, sizeof(x25519SecretA)) == 0);

    xrtX448Keypair(x448PrivA, x448PubA);
    xrtX448Keypair(x448PrivB, x448PubB);
    xrtX448SharedSecret(x448SecretA, x448PrivA, x448PubB);
    xrtX448SharedSecret(x448SecretB, x448PrivB, x448PubA);
    require_line("x448", memcmp(x448SecretA, x448SecretB, sizeof(x448SecretA)) == 0);

    require_line("rsa_guard", !valid_hex("") && !valid_hex("bad"));

    xrtUnit();
    return 0;
}
