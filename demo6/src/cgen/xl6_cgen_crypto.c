static void xl6CgenEmitCryptoRuntime(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_CRYPTO_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_CRYPTO_RUNTIME\n", out);
    xl6OutputPuts("void xrtSHA1(const void* pData, size_t iLen, unsigned char* pOut);\n", out);
    xl6OutputPuts("void xrtSHA256(const void* pData, size_t iLen, unsigned char* pOut);\n", out);
    xl6OutputPuts("void xrtSHA384(const void* pData, size_t iLen, unsigned char* pOut);\n", out);
    xl6OutputPuts("void xrtSHA512(const void* pData, size_t iLen, unsigned char* pOut);\n", out);
    xl6OutputPuts("void xrtHMAC_SHA256(const unsigned char* pKey, size_t iKeyLen, const unsigned char* pMsg, size_t iMsgLen, unsigned char* pOut);\n", out);
    xl6OutputPuts("void xrtHMAC_SHA384(const unsigned char* pKey, size_t iKeyLen, const unsigned char* pMsg, size_t iMsgLen, unsigned char* pOut);\n", out);
    xl6OutputPuts("void xrtHMAC_SHA512(const unsigned char* pKey, size_t iKeyLen, const unsigned char* pMsg, size_t iMsgLen, unsigned char* pOut);\n", out);
    xl6OutputPuts("bool xrtChaCha20Poly1305Encrypt(unsigned char* pOut, const unsigned char* pKey, const unsigned char* pNonce, const unsigned char* pAAD, size_t iAADLen, const unsigned char* pIn, size_t iLen);\n", out);
    xl6OutputPuts("bool xrtChaCha20Poly1305Decrypt(unsigned char* pOut, const unsigned char* pKey, const unsigned char* pNonce, const unsigned char* pAAD, size_t iAADLen, const unsigned char* pIn, size_t iLen);\n", out);
    xl6OutputPuts("bool xrtAES128GCMEncrypt(unsigned char* pOut, const unsigned char* pKey, const unsigned char* pNonce, size_t iNonceLen, const unsigned char* pAAD, size_t iAADLen, const unsigned char* pIn, size_t iLen);\n", out);
    xl6OutputPuts("bool xrtAES128GCMDecrypt(unsigned char* pOut, const unsigned char* pKey, const unsigned char* pNonce, size_t iNonceLen, const unsigned char* pAAD, size_t iAADLen, const unsigned char* pIn, size_t iLen);\n", out);
    xl6OutputPuts("bool xrtAES256GCMEncrypt(unsigned char* pOut, const unsigned char* pKey, const unsigned char* pNonce, size_t iNonceLen, const unsigned char* pAAD, size_t iAADLen, const unsigned char* pIn, size_t iLen);\n", out);
    xl6OutputPuts("bool xrtAES256GCMDecrypt(unsigned char* pOut, const unsigned char* pKey, const unsigned char* pNonce, size_t iNonceLen, const unsigned char* pAAD, size_t iAADLen, const unsigned char* pIn, size_t iLen);\n", out);
    xl6OutputPuts("void xrtHKDFExtract(unsigned char* pPRK, const unsigned char* pSalt, size_t iSaltLen, const unsigned char* pIKM, size_t iIKMLen);\n", out);
    xl6OutputPuts("void xrtHKDFExpand(unsigned char* pOKM, size_t iOKMLen, const unsigned char* pPRK, size_t iPRKLen, const unsigned char* pInfo, size_t iInfoLen);\n", out);
    xl6OutputPuts("void xrtHKDFExtract_SHA384(unsigned char* pPRK, const unsigned char* pSalt, size_t iSaltLen, const unsigned char* pIKM, size_t iIKMLen);\n", out);
    xl6OutputPuts("void xrtHKDFExpand_SHA384(unsigned char* pOKM, size_t iOKMLen, const unsigned char* pPRK, size_t iPRKLen, const unsigned char* pInfo, size_t iInfoLen);\n", out);
    xl6OutputPuts("void xrtRandomBytes(unsigned char* pBuf, size_t iLen);\n", out);
    xl6OutputPuts("void* xrtMalloc(size_t iSize);\n", out);
    xl6OutputPuts("void xrtFree(void* p);\n", out);
    xl6OutputPuts("void* xrtHexEncode(void* pMem, size_t iSize);\n", out);
    xl6OutputPuts("void* xrtHexDecode(const char* sText, size_t iSize);\n\n", out);

    xl6OutputPuts("static const char* xl6_crypto_text_or_empty(const char* text)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return text != 0 ? text : \"\";\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_crypto_empty_result(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    char* text = (char*)xrtMalloc(1);\n", out);
    xl6OutputPuts("    if ( text != 0 ) { text[0] = '\\0'; }\n", out);
    xl6OutputPuts("    return text;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_crypto_hex_char(char ch)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_crypto_hex_text(const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t i;\n", out);
    xl6OutputPuts("    if ( text == 0 || size == 0 || (size & 1u) != 0 ) { return false; }\n", out);
    xl6OutputPuts("    for ( i = 0; i < size; ++i ) {\n", out);
    xl6OutputPuts("        if ( !xl6_crypto_hex_char(text[i]) ) { return false; }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return true;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_sha1_hex(const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char digest[20];\n", out);
    xl6OutputPuts("    xrtSHA1((const void*)xl6_crypto_text_or_empty(text), size, digest);\n", out);
    xl6OutputPuts("    return xrtHexEncode((void*)digest, sizeof(digest));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_sha256_hex(const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char digest[32];\n", out);
    xl6OutputPuts("    xrtSHA256((const void*)xl6_crypto_text_or_empty(text), size, digest);\n", out);
    xl6OutputPuts("    return xrtHexEncode((void*)digest, sizeof(digest));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_sha384_hex(const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char digest[48];\n", out);
    xl6OutputPuts("    xrtSHA384((const void*)xl6_crypto_text_or_empty(text), size, digest);\n", out);
    xl6OutputPuts("    return xrtHexEncode((void*)digest, sizeof(digest));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_sha512_hex(const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char digest[64];\n", out);
    xl6OutputPuts("    xrtSHA512((const void*)xl6_crypto_text_or_empty(text), size, digest);\n", out);
    xl6OutputPuts("    return xrtHexEncode((void*)digest, sizeof(digest));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_hmac_sha256_hex(const char* key, size_t keySize, const char* text, size_t textSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char digest[32];\n", out);
    xl6OutputPuts("    xrtHMAC_SHA256((const unsigned char*)xl6_crypto_text_or_empty(key), keySize, (const unsigned char*)xl6_crypto_text_or_empty(text), textSize, digest);\n", out);
    xl6OutputPuts("    return xrtHexEncode((void*)digest, sizeof(digest));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_hmac_sha384_hex(const char* key, size_t keySize, const char* text, size_t textSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char digest[48];\n", out);
    xl6OutputPuts("    xrtHMAC_SHA384((const unsigned char*)xl6_crypto_text_or_empty(key), keySize, (const unsigned char*)xl6_crypto_text_or_empty(text), textSize, digest);\n", out);
    xl6OutputPuts("    return xrtHexEncode((void*)digest, sizeof(digest));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_hmac_sha512_hex(const char* key, size_t keySize, const char* text, size_t textSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char digest[64];\n", out);
    xl6OutputPuts("    xrtHMAC_SHA512((const unsigned char*)xl6_crypto_text_or_empty(key), keySize, (const unsigned char*)xl6_crypto_text_or_empty(text), textSize, digest);\n", out);
    xl6OutputPuts("    return xrtHexEncode((void*)digest, sizeof(digest));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_crypto_aead_encrypt_hex(int alg, const char* key, size_t keySize, const char* nonce, size_t nonceSize, const char* text, size_t textSize, const char* aad, size_t aadSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char* out;\n", out);
    xl6OutputPuts("    void* hex;\n", out);
    xl6OutputPuts("    bool ok = false;\n", out);
    xl6OutputPuts("    size_t outSize = textSize + 16u;\n", out);
    xl6OutputPuts("    if ( nonceSize != 12u ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    if ( (alg == 1 && keySize != 16u) || (alg != 1 && keySize != 32u) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    out = (unsigned char*)xrtMalloc(outSize);\n", out);
    xl6OutputPuts("    if ( out == 0 ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    if ( alg == 0 ) {\n", out);
    xl6OutputPuts("        ok = xrtChaCha20Poly1305Encrypt(out, (const unsigned char*)key, (const unsigned char*)nonce, (const unsigned char*)xl6_crypto_text_or_empty(aad), aadSize, (const unsigned char*)xl6_crypto_text_or_empty(text), textSize);\n", out);
    xl6OutputPuts("    } else if ( alg == 1 ) {\n", out);
    xl6OutputPuts("        ok = xrtAES128GCMEncrypt(out, (const unsigned char*)key, (const unsigned char*)nonce, nonceSize, (const unsigned char*)xl6_crypto_text_or_empty(aad), aadSize, (const unsigned char*)xl6_crypto_text_or_empty(text), textSize);\n", out);
    xl6OutputPuts("    } else {\n", out);
    xl6OutputPuts("        ok = xrtAES256GCMEncrypt(out, (const unsigned char*)key, (const unsigned char*)nonce, nonceSize, (const unsigned char*)xl6_crypto_text_or_empty(aad), aadSize, (const unsigned char*)xl6_crypto_text_or_empty(text), textSize);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( !ok ) { xrtFree(out); return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    hex = xrtHexEncode((void*)out, outSize);\n", out);
    xl6OutputPuts("    xrtFree(out);\n", out);
    xl6OutputPuts("    return hex != 0 ? hex : xl6_crypto_empty_result();\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_crypto_aead_decrypt_text(int alg, const char* key, size_t keySize, const char* nonce, size_t nonceSize, const char* cipherHex, size_t cipherHexSize, const char* aad, size_t aadSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t cipherSize;\n", out);
    xl6OutputPuts("    size_t plainSize;\n", out);
    xl6OutputPuts("    unsigned char* cipher;\n", out);
    xl6OutputPuts("    unsigned char* plain;\n", out);
    xl6OutputPuts("    bool ok = false;\n", out);
    xl6OutputPuts("    if ( nonceSize != 12u ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    if ( (alg == 1 && keySize != 16u) || (alg != 1 && keySize != 32u) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    if ( !xl6_crypto_hex_text(cipherHex, cipherHexSize) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    cipherSize = cipherHexSize / 2u;\n", out);
    xl6OutputPuts("    if ( cipherSize < 16u ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    cipher = (unsigned char*)xrtHexDecode(cipherHex, cipherHexSize);\n", out);
    xl6OutputPuts("    if ( cipher == 0 ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    plainSize = cipherSize - 16u;\n", out);
    xl6OutputPuts("    plain = (unsigned char*)xrtMalloc(plainSize + 1u);\n", out);
    xl6OutputPuts("    if ( plain == 0 ) { xrtFree(cipher); return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    if ( alg == 0 ) {\n", out);
    xl6OutputPuts("        ok = xrtChaCha20Poly1305Decrypt(plain, (const unsigned char*)key, (const unsigned char*)nonce, (const unsigned char*)xl6_crypto_text_or_empty(aad), aadSize, cipher, cipherSize);\n", out);
    xl6OutputPuts("    } else if ( alg == 1 ) {\n", out);
    xl6OutputPuts("        ok = xrtAES128GCMDecrypt(plain, (const unsigned char*)key, (const unsigned char*)nonce, nonceSize, (const unsigned char*)xl6_crypto_text_or_empty(aad), aadSize, cipher, cipherSize);\n", out);
    xl6OutputPuts("    } else {\n", out);
    xl6OutputPuts("        ok = xrtAES256GCMDecrypt(plain, (const unsigned char*)key, (const unsigned char*)nonce, nonceSize, (const unsigned char*)xl6_crypto_text_or_empty(aad), aadSize, cipher, cipherSize);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    xrtFree(cipher);\n", out);
    xl6OutputPuts("    if ( !ok ) { xrtFree(plain); return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    plain[plainSize] = '\\0';\n", out);
    xl6OutputPuts("    return plain;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_chacha20_poly1305_encrypt_hex(const char* key, size_t keySize, const char* nonce, size_t nonceSize, const char* text, size_t textSize, const char* aad, size_t aadSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_crypto_aead_encrypt_hex(0, key, keySize, nonce, nonceSize, text, textSize, aad, aadSize);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_chacha20_poly1305_decrypt_text(const char* key, size_t keySize, const char* nonce, size_t nonceSize, const char* cipherHex, size_t cipherHexSize, const char* aad, size_t aadSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_crypto_aead_decrypt_text(0, key, keySize, nonce, nonceSize, cipherHex, cipherHexSize, aad, aadSize);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_aes128_gcm_encrypt_hex(const char* key, size_t keySize, const char* nonce, size_t nonceSize, const char* text, size_t textSize, const char* aad, size_t aadSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_crypto_aead_encrypt_hex(1, key, keySize, nonce, nonceSize, text, textSize, aad, aadSize);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_aes128_gcm_decrypt_text(const char* key, size_t keySize, const char* nonce, size_t nonceSize, const char* cipherHex, size_t cipherHexSize, const char* aad, size_t aadSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_crypto_aead_decrypt_text(1, key, keySize, nonce, nonceSize, cipherHex, cipherHexSize, aad, aadSize);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_aes256_gcm_encrypt_hex(const char* key, size_t keySize, const char* nonce, size_t nonceSize, const char* text, size_t textSize, const char* aad, size_t aadSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_crypto_aead_encrypt_hex(2, key, keySize, nonce, nonceSize, text, textSize, aad, aadSize);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_aes256_gcm_decrypt_text(const char* key, size_t keySize, const char* nonce, size_t nonceSize, const char* cipherHex, size_t cipherHexSize, const char* aad, size_t aadSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_crypto_aead_decrypt_text(2, key, keySize, nonce, nonceSize, cipherHex, cipherHexSize, aad, aadSize);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_hkdf_sha256_hex(const char* ikm, size_t ikmSize, const char* salt, size_t saltSize, const char* info, size_t infoSize, size_t outSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char prk[32];\n", out);
    xl6OutputPuts("    unsigned char* okm;\n", out);
    xl6OutputPuts("    void* hex;\n", out);
    xl6OutputPuts("    if ( outSize == 0 || outSize > 8160u ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    okm = (unsigned char*)xrtMalloc(outSize);\n", out);
    xl6OutputPuts("    if ( okm == 0 ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    xrtHKDFExtract(prk, (const unsigned char*)xl6_crypto_text_or_empty(salt), saltSize, (const unsigned char*)xl6_crypto_text_or_empty(ikm), ikmSize);\n", out);
    xl6OutputPuts("    xrtHKDFExpand(okm, outSize, prk, sizeof(prk), (const unsigned char*)xl6_crypto_text_or_empty(info), infoSize);\n", out);
    xl6OutputPuts("    hex = xrtHexEncode((void*)okm, outSize);\n", out);
    xl6OutputPuts("    xrtFree(okm);\n", out);
    xl6OutputPuts("    return hex != 0 ? hex : xl6_crypto_empty_result();\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_hkdf_sha384_hex(const char* ikm, size_t ikmSize, const char* salt, size_t saltSize, const char* info, size_t infoSize, size_t outSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char prk[48];\n", out);
    xl6OutputPuts("    unsigned char* okm;\n", out);
    xl6OutputPuts("    void* hex;\n", out);
    xl6OutputPuts("    if ( outSize == 0 || outSize > 12240u ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    okm = (unsigned char*)xrtMalloc(outSize);\n", out);
    xl6OutputPuts("    if ( okm == 0 ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    xrtHKDFExtract_SHA384(prk, (const unsigned char*)xl6_crypto_text_or_empty(salt), saltSize, (const unsigned char*)xl6_crypto_text_or_empty(ikm), ikmSize);\n", out);
    xl6OutputPuts("    xrtHKDFExpand_SHA384(okm, outSize, prk, sizeof(prk), (const unsigned char*)xl6_crypto_text_or_empty(info), infoSize);\n", out);
    xl6OutputPuts("    hex = xrtHexEncode((void*)okm, outSize);\n", out);
    xl6OutputPuts("    xrtFree(okm);\n", out);
    xl6OutputPuts("    return hex != 0 ? hex : xl6_crypto_empty_result();\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_random_hex(size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char* data;\n", out);
    xl6OutputPuts("    void* hex;\n", out);
    xl6OutputPuts("    if ( size == 0 ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    data = (unsigned char*)xrtMalloc(size);\n", out);
    xl6OutputPuts("    if ( data == 0 ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    xrtRandomBytes(data, size);\n", out);
    xl6OutputPuts("    hex = xrtHexEncode((void*)data, size);\n", out);
    xl6OutputPuts("    xrtFree(data);\n", out);
    xl6OutputPuts("    return hex != 0 ? hex : xl6_crypto_empty_result();\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("#endif\n\n", out);
}
