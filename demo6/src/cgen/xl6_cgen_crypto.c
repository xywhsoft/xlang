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
    xl6OutputPuts("typedef struct { unsigned int state[8]; unsigned char buffer[64]; unsigned int len; unsigned long long bits; } xsha256_ctx;\n", out);
    xl6OutputPuts("typedef struct { unsigned long long state[8]; unsigned char buffer[128]; unsigned int len; unsigned long long bits; } xsha512_ctx;\n", out);
    xl6OutputPuts("void xrtSHA256Init(xsha256_ctx* pCtx);\n", out);
    xl6OutputPuts("void xrtSHA256Update(xsha256_ctx* pCtx, const void* pData, size_t iLen);\n", out);
    xl6OutputPuts("void xrtSHA256Final(xsha256_ctx* pCtx, unsigned char* pOut);\n", out);
    xl6OutputPuts("void xrtSHA384Init(xsha512_ctx* pCtx);\n", out);
    xl6OutputPuts("void xrtSHA384Final(xsha512_ctx* pCtx, unsigned char* pOut);\n", out);
    xl6OutputPuts("void xrtSHA512Init(xsha512_ctx* pCtx);\n", out);
    xl6OutputPuts("void xrtSHA512Update(xsha512_ctx* pCtx, const void* pData, size_t iLen);\n", out);
    xl6OutputPuts("void xrtSHA512Final(xsha512_ctx* pCtx, unsigned char* pOut);\n", out);
    xl6OutputPuts("void xrtHMAC_SHA256(const unsigned char* pKey, size_t iKeyLen, const unsigned char* pMsg, size_t iMsgLen, unsigned char* pOut);\n", out);
    xl6OutputPuts("void xrtHMAC_SHA384(const unsigned char* pKey, size_t iKeyLen, const unsigned char* pMsg, size_t iMsgLen, unsigned char* pOut);\n", out);
    xl6OutputPuts("void xrtHMAC_SHA512(const unsigned char* pKey, size_t iKeyLen, const unsigned char* pMsg, size_t iMsgLen, unsigned char* pOut);\n", out);
    xl6OutputPuts("void xrtChaCha20(unsigned char* pOut, const unsigned char* pKey, const unsigned char* pNonce, unsigned int iCounter, const unsigned char* pIn, size_t iLen);\n", out);
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
    xl6OutputPuts("void xrtX25519Keypair(unsigned char* pPrivKey, unsigned char* pPubKey);\n", out);
    xl6OutputPuts("void xrtX25519SharedSecret(unsigned char* pOut, const unsigned char* pPrivKey, const unsigned char* pPubKey);\n", out);
    xl6OutputPuts("void xrtX448Keypair(unsigned char* pPrivKey, unsigned char* pPubKey);\n", out);
    xl6OutputPuts("void xrtX448SharedSecret(unsigned char* pOut, const unsigned char* pPrivKey, const unsigned char* pPubKey);\n", out);
    xl6OutputPuts("void xrtEd25519Keypair(unsigned char* pSeed, unsigned char* pPubKey);\n", out);
    xl6OutputPuts("void xrtEd25519PublicKey(unsigned char* pPubKey, const unsigned char* pSeed);\n", out);
    xl6OutputPuts("bool xrtEd25519Sign(unsigned char* pSig, const unsigned char* pMsg, size_t iMsgLen, const unsigned char* pSeed);\n", out);
    xl6OutputPuts("bool xrtEd25519Verify(const unsigned char* pMsg, size_t iMsgLen, const unsigned char* pSig, const unsigned char* pPubKey);\n", out);
    xl6OutputPuts("bool xrtRSAPSSVerify(const unsigned char* pHash, size_t iHashLen, const unsigned char* pSig, size_t iSigLen, const unsigned char* pMod, size_t iModSz, const unsigned char* pExp, size_t iExpSz);\n", out);
    xl6OutputPuts("bool xrtRSAPKCS1Verify(const unsigned char* pHash, size_t iHashLen, const unsigned char* pSig, size_t iSigLen, const unsigned char* pMod, size_t iModSz, const unsigned char* pExp, size_t iExpSz);\n", out);
    xl6OutputPuts("void xrtRandomBytes(unsigned char* pBuf, size_t iLen);\n", out);
    xl6OutputPuts("void* xrtMalloc(size_t iSize);\n", out);
    xl6OutputPuts("void xrtFree(void* p);\n", out);
    xl6OutputPuts("void* xrtHexEncode(void* pMem, size_t iSize);\n", out);
    xl6OutputPuts("void* xrtHexDecode(const char* sText, size_t iSize);\n", out);
    xl6OutputPuts("typedef struct xvalue_struct* xvalue;\n", out);
    xl6OutputPuts("xvalue xvoCreateTable(void);\n", out);
    xl6OutputPuts("xvalue xvoCreateText(void* sVal, unsigned int iSize, bool bColloc);\n", out);
    xl6OutputPuts("bool xvoTableSetValue(xvalue pTbl, const void* key, unsigned int kl, xvalue pVal, bool bColloc);\n\n", out);

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

    xl6OutputPuts("static bool xl6_crypto_decode_hex_exact(const char* text, size_t size, size_t expected, unsigned char* out)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char* decoded;\n", out);
    xl6OutputPuts("    if ( out == 0 || size != expected * 2u || !xl6_crypto_hex_text(text, size) ) { return false; }\n", out);
    xl6OutputPuts("    decoded = (unsigned char*)xrtHexDecode(text, size);\n", out);
    xl6OutputPuts("    if ( decoded == 0 ) { return false; }\n", out);
    xl6OutputPuts("    memcpy(out, decoded, expected);\n", out);
    xl6OutputPuts("    xrtFree(decoded);\n", out);
    xl6OutputPuts("    return true;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_crypto_hex_from_bytes(const unsigned char* data, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    void* hex = xrtHexEncode((void*)data, size);\n", out);
    xl6OutputPuts("    return hex != 0 ? hex : xl6_crypto_empty_result();\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_crypto_table_set_hex(xvalue table, const char* name, const unsigned char* data, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    char* hex;\n", out);
    xl6OutputPuts("    xvalue value;\n", out);
    xl6OutputPuts("    if ( table == 0 || name == 0 ) { return; }\n", out);
    xl6OutputPuts("    hex = (char*)xrtHexEncode((void*)data, size);\n", out);
    xl6OutputPuts("    value = xvoCreateText(hex != 0 ? hex : \"\", 0, false);\n", out);
    xl6OutputPuts("    if ( hex != 0 ) { xrtFree(hex); }\n", out);
    xl6OutputPuts("    if ( value != 0 ) { xvoTableSetValue(table, name, 0, value, true); }\n", out);
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

    xl6OutputPuts("void* xl6_xrt_sha256_stream_create(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xsha256_ctx* ctx = (xsha256_ctx*)xrtMalloc(sizeof(xsha256_ctx));\n", out);
    xl6OutputPuts("    if ( ctx != 0 ) { xrtSHA256Init(ctx); }\n", out);
    xl6OutputPuts("    return ctx;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_sha256_stream_destroy(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self != 0 ) { xrtFree(self); }\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_sha256_stream_update(void* self, const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    xrtSHA256Update((xsha256_ctx*)self, (const void*)xl6_crypto_text_or_empty(text), size);\n", out);
    xl6OutputPuts("    return 1;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_sha256_stream_hex(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char digest[32];\n", out);
    xl6OutputPuts("    xsha256_ctx copy;\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    copy = *(xsha256_ctx*)self;\n", out);
    xl6OutputPuts("    xrtSHA256Final(&copy, digest);\n", out);
    xl6OutputPuts("    return xl6_crypto_hex_from_bytes(digest, sizeof(digest));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_sha384_stream_create(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xsha512_ctx* ctx = (xsha512_ctx*)xrtMalloc(sizeof(xsha512_ctx));\n", out);
    xl6OutputPuts("    if ( ctx != 0 ) { xrtSHA384Init(ctx); }\n", out);
    xl6OutputPuts("    return ctx;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_sha384_stream_destroy(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self != 0 ) { xrtFree(self); }\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_sha384_stream_update(void* self, const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    xrtSHA512Update((xsha512_ctx*)self, (const void*)xl6_crypto_text_or_empty(text), size);\n", out);
    xl6OutputPuts("    return 1;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_sha384_stream_hex(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char digest[48];\n", out);
    xl6OutputPuts("    xsha512_ctx copy;\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    copy = *(xsha512_ctx*)self;\n", out);
    xl6OutputPuts("    xrtSHA384Final(&copy, digest);\n", out);
    xl6OutputPuts("    return xl6_crypto_hex_from_bytes(digest, sizeof(digest));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_sha512_stream_create(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xsha512_ctx* ctx = (xsha512_ctx*)xrtMalloc(sizeof(xsha512_ctx));\n", out);
    xl6OutputPuts("    if ( ctx != 0 ) { xrtSHA512Init(ctx); }\n", out);
    xl6OutputPuts("    return ctx;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_sha512_stream_destroy(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self != 0 ) { xrtFree(self); }\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("int xl6_xrt_sha512_stream_update(void* self, const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    xrtSHA512Update((xsha512_ctx*)self, (const void*)xl6_crypto_text_or_empty(text), size);\n", out);
    xl6OutputPuts("    return 1;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_sha512_stream_hex(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char digest[64];\n", out);
    xl6OutputPuts("    xsha512_ctx copy;\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    copy = *(xsha512_ctx*)self;\n", out);
    xl6OutputPuts("    xrtSHA512Final(&copy, digest);\n", out);
    xl6OutputPuts("    return xl6_crypto_hex_from_bytes(digest, sizeof(digest));\n", out);
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

    xl6OutputPuts("void* xl6_xrt_chacha20_encrypt_hex(const char* key, size_t keySize, const char* nonce, size_t nonceSize, const char* text, size_t textSize, unsigned int counter)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char* out;\n", out);
    xl6OutputPuts("    void* hex;\n", out);
    xl6OutputPuts("    if ( keySize != 32u || nonceSize != 12u ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    if ( textSize == 0 ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    out = (unsigned char*)xrtMalloc(textSize);\n", out);
    xl6OutputPuts("    if ( out == 0 ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    xrtChaCha20(out, (const unsigned char*)key, (const unsigned char*)nonce, counter, (const unsigned char*)xl6_crypto_text_or_empty(text), textSize);\n", out);
    xl6OutputPuts("    hex = xl6_crypto_hex_from_bytes(out, textSize);\n", out);
    xl6OutputPuts("    xrtFree(out);\n", out);
    xl6OutputPuts("    return hex;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_chacha20_decrypt_text(const char* key, size_t keySize, const char* nonce, size_t nonceSize, const char* cipherHex, size_t cipherHexSize, unsigned int counter)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t cipherSize;\n", out);
    xl6OutputPuts("    unsigned char* cipher;\n", out);
    xl6OutputPuts("    unsigned char* out;\n", out);
    xl6OutputPuts("    if ( keySize != 32u || nonceSize != 12u ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    if ( !xl6_crypto_hex_text(cipherHex, cipherHexSize) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    cipherSize = cipherHexSize / 2u;\n", out);
    xl6OutputPuts("    cipher = (unsigned char*)xrtHexDecode(cipherHex, cipherHexSize);\n", out);
    xl6OutputPuts("    if ( cipher == 0 ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    out = (unsigned char*)xrtMalloc(cipherSize + 1u);\n", out);
    xl6OutputPuts("    if ( out == 0 ) { xrtFree(cipher); return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    xrtChaCha20(out, (const unsigned char*)key, (const unsigned char*)nonce, counter, cipher, cipherSize);\n", out);
    xl6OutputPuts("    xrtFree(cipher);\n", out);
    xl6OutputPuts("    out[cipherSize] = '\\0';\n", out);
    xl6OutputPuts("    return out;\n", out);
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

    xl6OutputPuts("xvalue xl6_xrt_ed25519_keypair(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char seed[32];\n", out);
    xl6OutputPuts("    unsigned char pub[32];\n", out);
    xl6OutputPuts("    xvalue table = xvoCreateTable();\n", out);
    xl6OutputPuts("    if ( table == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    xrtEd25519Keypair(seed, pub);\n", out);
    xl6OutputPuts("    xl6_crypto_table_set_hex(table, \"seed\", seed, sizeof(seed));\n", out);
    xl6OutputPuts("    xl6_crypto_table_set_hex(table, \"publicKey\", pub, sizeof(pub));\n", out);
    xl6OutputPuts("    return table;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_ed25519_public_key_hex(const char* seedHex, size_t seedHexSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char seed[32];\n", out);
    xl6OutputPuts("    unsigned char pub[32];\n", out);
    xl6OutputPuts("    if ( !xl6_crypto_decode_hex_exact(seedHex, seedHexSize, sizeof(seed), seed) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    xrtEd25519PublicKey(pub, seed);\n", out);
    xl6OutputPuts("    return xl6_crypto_hex_from_bytes(pub, sizeof(pub));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_ed25519_sign_hex(const char* seedHex, size_t seedHexSize, const char* text, size_t textSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char seed[32];\n", out);
    xl6OutputPuts("    unsigned char sig[64];\n", out);
    xl6OutputPuts("    if ( !xl6_crypto_decode_hex_exact(seedHex, seedHexSize, sizeof(seed), seed) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    if ( !xrtEd25519Sign(sig, (const unsigned char*)xl6_crypto_text_or_empty(text), textSize, seed) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    return xl6_crypto_hex_from_bytes(sig, sizeof(sig));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_ed25519_verify(const char* publicKeyHex, size_t publicKeyHexSize, const char* text, size_t textSize, const char* signatureHex, size_t signatureHexSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char pub[32];\n", out);
    xl6OutputPuts("    unsigned char sig[64];\n", out);
    xl6OutputPuts("    if ( !xl6_crypto_decode_hex_exact(publicKeyHex, publicKeyHexSize, sizeof(pub), pub) ) { return false; }\n", out);
    xl6OutputPuts("    if ( !xl6_crypto_decode_hex_exact(signatureHex, signatureHexSize, sizeof(sig), sig) ) { return false; }\n", out);
    xl6OutputPuts("    return xrtEd25519Verify((const unsigned char*)xl6_crypto_text_or_empty(text), textSize, sig, pub);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("xvalue xl6_xrt_x25519_keypair(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char priv[32];\n", out);
    xl6OutputPuts("    unsigned char pub[32];\n", out);
    xl6OutputPuts("    xvalue table = xvoCreateTable();\n", out);
    xl6OutputPuts("    if ( table == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    xrtX25519Keypair(priv, pub);\n", out);
    xl6OutputPuts("    xl6_crypto_table_set_hex(table, \"privateKey\", priv, sizeof(priv));\n", out);
    xl6OutputPuts("    xl6_crypto_table_set_hex(table, \"publicKey\", pub, sizeof(pub));\n", out);
    xl6OutputPuts("    return table;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_x25519_shared_secret_hex(const char* privateKeyHex, size_t privateKeyHexSize, const char* publicKeyHex, size_t publicKeyHexSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char priv[32];\n", out);
    xl6OutputPuts("    unsigned char pub[32];\n", out);
    xl6OutputPuts("    unsigned char secret[32];\n", out);
    xl6OutputPuts("    if ( !xl6_crypto_decode_hex_exact(privateKeyHex, privateKeyHexSize, sizeof(priv), priv) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    if ( !xl6_crypto_decode_hex_exact(publicKeyHex, publicKeyHexSize, sizeof(pub), pub) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    xrtX25519SharedSecret(secret, priv, pub);\n", out);
    xl6OutputPuts("    return xl6_crypto_hex_from_bytes(secret, sizeof(secret));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("xvalue xl6_xrt_x448_keypair(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char priv[56];\n", out);
    xl6OutputPuts("    unsigned char pub[56];\n", out);
    xl6OutputPuts("    xvalue table = xvoCreateTable();\n", out);
    xl6OutputPuts("    if ( table == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    xrtX448Keypair(priv, pub);\n", out);
    xl6OutputPuts("    xl6_crypto_table_set_hex(table, \"privateKey\", priv, sizeof(priv));\n", out);
    xl6OutputPuts("    xl6_crypto_table_set_hex(table, \"publicKey\", pub, sizeof(pub));\n", out);
    xl6OutputPuts("    return table;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_x448_shared_secret_hex(const char* privateKeyHex, size_t privateKeyHexSize, const char* publicKeyHex, size_t publicKeyHexSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char priv[56];\n", out);
    xl6OutputPuts("    unsigned char pub[56];\n", out);
    xl6OutputPuts("    unsigned char secret[56];\n", out);
    xl6OutputPuts("    if ( !xl6_crypto_decode_hex_exact(privateKeyHex, privateKeyHexSize, sizeof(priv), priv) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    if ( !xl6_crypto_decode_hex_exact(publicKeyHex, publicKeyHexSize, sizeof(pub), pub) ) { return xl6_crypto_empty_result(); }\n", out);
    xl6OutputPuts("    xrtX448SharedSecret(secret, priv, pub);\n", out);
    xl6OutputPuts("    return xl6_crypto_hex_from_bytes(secret, sizeof(secret));\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_crypto_rsa_verify(int alg, const char* hashHex, size_t hashHexSize, const char* sigHex, size_t sigHexSize, const char* modHex, size_t modHexSize, const char* expHex, size_t expHexSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned char* hash;\n", out);
    xl6OutputPuts("    unsigned char* sig;\n", out);
    xl6OutputPuts("    unsigned char* mod;\n", out);
    xl6OutputPuts("    unsigned char* exp;\n", out);
    xl6OutputPuts("    bool ok;\n", out);
    xl6OutputPuts("    if ( !xl6_crypto_hex_text(hashHex, hashHexSize) || !xl6_crypto_hex_text(sigHex, sigHexSize) || !xl6_crypto_hex_text(modHex, modHexSize) || !xl6_crypto_hex_text(expHex, expHexSize) ) { return false; }\n", out);
    xl6OutputPuts("    hash = (unsigned char*)xrtHexDecode(hashHex, hashHexSize);\n", out);
    xl6OutputPuts("    sig = (unsigned char*)xrtHexDecode(sigHex, sigHexSize);\n", out);
    xl6OutputPuts("    mod = (unsigned char*)xrtHexDecode(modHex, modHexSize);\n", out);
    xl6OutputPuts("    exp = (unsigned char*)xrtHexDecode(expHex, expHexSize);\n", out);
    xl6OutputPuts("    if ( hash == 0 || sig == 0 || mod == 0 || exp == 0 ) { xrtFree(hash); xrtFree(sig); xrtFree(mod); xrtFree(exp); return false; }\n", out);
    xl6OutputPuts("    if ( alg == 0 ) { ok = xrtRSAPSSVerify(hash, hashHexSize / 2u, sig, sigHexSize / 2u, mod, modHexSize / 2u, exp, expHexSize / 2u); }\n", out);
    xl6OutputPuts("    else { ok = xrtRSAPKCS1Verify(hash, hashHexSize / 2u, sig, sigHexSize / 2u, mod, modHexSize / 2u, exp, expHexSize / 2u); }\n", out);
    xl6OutputPuts("    xrtFree(hash);\n", out);
    xl6OutputPuts("    xrtFree(sig);\n", out);
    xl6OutputPuts("    xrtFree(mod);\n", out);
    xl6OutputPuts("    xrtFree(exp);\n", out);
    xl6OutputPuts("    return ok;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_rsa_pss_verify(const char* hashHex, size_t hashHexSize, const char* sigHex, size_t sigHexSize, const char* modHex, size_t modHexSize, const char* expHex, size_t expHexSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_crypto_rsa_verify(0, hashHex, hashHexSize, sigHex, sigHexSize, modHex, modHexSize, expHex, expHexSize);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("bool xl6_xrt_rsa_pkcs1_verify(const char* hashHex, size_t hashHexSize, const char* sigHex, size_t sigHexSize, const char* modHex, size_t modHexSize, const char* expHex, size_t expHexSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_crypto_rsa_verify(1, hashHex, hashHexSize, sigHex, sigHexSize, modHex, modHexSize, expHex, expHexSize);\n", out);
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
